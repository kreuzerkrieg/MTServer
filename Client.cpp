#include <zmq.hpp>
#include <string>
#include <thread>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include "Client.h"

using namespace std;

static std::atomic<uint64_t> counter {0};

void MClient::SendMessages(zmq::socket_t& soc, std::vector<zmq::message_t>& messages)
{
	for (auto i = 0ul; i < messages.size(); ++i) {
		if (i < messages.size() - 1) {
			soc.send(messages[i], ZMQ_SNDMORE);
		}
		else {
			soc.send(messages[i]);
		}
	}
}

MClient::MClient(MQConfig&& config) :
		gen(rd()), clientConfiguration(std::move(config)), socket(clientConfiguration.ctx, ZMQ_DEALER),
		clientSocket(clientConfiguration.ctx, ZMQ_ROUTER)
{
	clientSocket.bind("inproc://clients" + to_string(reinterpret_cast<std::size_t>(this)));
	configureSocket();
	ioThread = std::thread([this]() {
		while (!shouldStop) {
			zmq::pollitem_t items[] = {{socket,       0, ZMQ_POLLIN, 0},
									   {clientSocket, 0, ZMQ_POLLIN, 0}};
			zmq::poll(&items[0], 2, 10s);
			if (items[1].revents & ZMQ_POLLIN) {
				std::vector<zmq::message_t> messages;
				bool messageReceived = false;
				while (1) {

					zmq::message_t reply;
					clientSocket.recv(&reply);
					messages.emplace_back(std::move(reply));
					int more = 0;
					size_t more_size = sizeof(more);
					clientSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
					if (!more) {
						messageReceived = true;
						break;
					}
				}
				if (messageReceived) {
					//messages.erase(messages.begin());
					SendMessages(socket, messages);
				}
			}

			if (items[0].revents & ZMQ_POLLIN) {
				std::vector<zmq::message_t> messages;
				bool messageReceived = false;
				while (1) {

					zmq::message_t reply;
					socket.recv(&reply);
					messages.emplace_back(std::move(reply));
					int more = 0;
					size_t more_size = sizeof(more);
					socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
					if (!more) {
						messageReceived = true;
						break;
					}
				}
				if (messageReceived) {
					uint64_t messageId = 0;
					memcpy(&messageId, messages[1].data(), messages[1].size());

					{
						std::lock_guard<std::mutex> guard(queueMutex);
						auto found = promises.find(messageId);
						if (found == promises.end()) {
							throw std::runtime_error("Message id " + std::to_string(messageId) + " not found.");
						}
						//std::cout << "message id " << messageId << " processed." << std::endl;
						found->second.set_value(std::vector<uint8_t>(static_cast<char*>(messages[2].data()),
																	 static_cast<char*>(messages[2].data()) + messages[2].size()));
						promises.erase(found);
					}
				}
			}
		}
	});
	timeoutThread = std::thread([&]() {
		while (!shouldStop || !timeouts.empty()) {
			{
				std::unique_lock<std::mutex> lck(timeoutCVMutex);
				while (timeouts.empty()) {
					timeoutCV.wait(lck);
				}
			}
			chrono::steady_clock::duration timeDelta;
			{
				std::lock_guard<std::mutex> guard(timeoutMutex);
				if (!timeouts.empty()) {
					if (timeouts.begin()->first <= chrono::steady_clock::now()) {
						std::lock_guard<std::mutex> guard(queueMutex);
						auto found = promises.find(timeouts.begin()->second);
						if (found != promises.end()) {
							found->second.set_exception(std::make_exception_ptr(std::runtime_error("Request timeout!")));
							promises.erase(found);
						}
						timeouts.erase(timeouts.begin());
					}
					else { timeDelta = chrono::steady_clock::now() - timeouts.begin()->first; }
				}
			}
			{
				std::unique_lock<std::mutex> lck(timeoutCVMutex);
				timeoutCV.wait_for(lck, timeDelta);
			}
		}
	});
}

MClient::~MClient()
{
	shouldStop = true;
	ioThread.join();
	timeoutThread.join();
}

std::pair<std::future<void>, Message> MClient::sendRequest(const void* data, uint64_t size)
{
	try {
		auto msgId = counter.fetch_add(1);
		Message fut;
		{
			std::lock_guard<std::mutex> guard(queueMutex);
			auto res = promises.emplace(msgId, std::promise<std::vector<uint8_t>>());
			if (!res.second) {
				throw std::runtime_error("Duplicate message ID");
			}
			fut = res.first->second.get_future();

		}
		{
			std::lock_guard<std::mutex> guard(timeoutMutex);
			timeouts.emplace(std::chrono::steady_clock::now() + std::chrono::milliseconds(1000), msgId);
		}
		{
			std::unique_lock<std::mutex> lck(timeoutCVMutex);
			timeoutCV.notify_all();
		}
		auto async = std::async([msgId, data, size, this]() mutable {
			zmq::message_t msg(const_cast<void*>(data), size, [](void*, void*) {}, nullptr);
			SendMessage(msgId, msg);
		});
		return std::make_pair(std::move(async), std::move(fut));
	}
	catch (const std::exception& ex) {
		throw;
	}
}

void MClient::SendMessage(uint64_t msgId, zmq::message_t& msg)
{
	thread_local bool isInitialized = false;
	thread_local zmq::socket_t tmpSocket(clientConfiguration.ctx, ZMQ_DEALER);
	if (!isInitialized) {
		tmpSocket.connect("inproc://clients" + to_string(reinterpret_cast<std::size_t>(this)));
		isInitialized = true;
	}
	std::vector<zmq::message_t> messages;
	messages.emplace_back(zmq::message_t(&msgId, sizeof(msgId)));
	messages.emplace_back(std::move(msg));
	SendMessages(tmpSocket, messages);
}

std::pair<std::future<void>, Message> MClient::sendRequest(const std::string& str)
{
	return sendRequest(static_cast<const void*>(str.data()), str.size());
}

void MClient::configureSocket()
{
	int linger = 0;
	socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

	identity = gen();
	socket.setsockopt(ZMQ_IDENTITY, &identity, sizeof(identity));
	int connectTimeout = 10;
	socket.setsockopt(ZMQ_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
	socket.connect(clientConfiguration.uri);
}
