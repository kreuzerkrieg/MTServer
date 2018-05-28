#include <zmq.hpp>
#include <string>
#include <thread>
#include <iostream>
#include <atomic>
#include <thread>
#include "Client.h"

MClient::MClient(MQConfig&& config) : gen(rd()), clientConfiguration(std::move(config)), socket(clientConfiguration.ctx, ZMQ_DEALER)
{
	configureSocket();
	for (auto i:{1, 2, 3}) {
		executors.emplace_back(std::thread([this]() {
			while (!shouldStop) {
				if (!responses.empty()) {
					std::lock_guard<std::mutex> guard(queueMutex);
					if (!responses.empty()) {
						auto& task = responses.back();
						task.func();
						responses.pop_back();
					}
				}
				else {
					std::this_thread::yield();
				}
			}
		}));
	}
	for (auto i:{1, 2, 3}) {
		executors.emplace_back(std::thread([this]() {
			while (!shouldStop) {
				if (!responses.empty()) {
					std::lock_guard<std::mutex> guard(queueMutex);
					if (!responses.empty()) {
						auto& task = responses.front();
						task.func();
						responses.pop_front();
					}
				}
				else {
					std::this_thread::yield();
				}
			}
		}));
	}
}

MClient::~MClient()
{
	shouldStop = true;
	for (auto& thread:executors) {
		thread.join();
	}
}

Message MClient::sendRequest(const void* data, uint64_t size)
{
	try {
		auto msgId = "Msg:" + std::to_string(++counter);
		ClientTask task;
		std::lock_guard<std::mutex> guard(queueMutex);

		responses.push_front(std::move(task));

		auto& prom = responses.front().promise;
		responses.front().func = [msgId, &prom, data, size, this]() mutable {
			zmq::message_t msg(const_cast<void*>(data), size, [](void*, void*) {}, nullptr);
			prom.set_value(SendMessage(msgId, msg));
		};
		return prom.get_future();
	}
	catch (const std::exception& ex) {
		throw;
	}
}

Message MClient::sendClonedRequest(const void* data, uint64_t size)
{
	try {
		auto msgId = "Msg:" + std::to_string(++counter);
		ClientTask task;
		std::lock_guard<std::mutex> guard(queueMutex);

		responses.push_front(std::move(task));

		auto& prom = responses.front().promise;
		responses.front().func = [msgId, &prom, clone {
				std::vector<uint8_t>(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size)}, this]() mutable {
			zmq::message_t msg(static_cast<void*>(clone.data()), clone.size(), [](void*, void*) {}, nullptr);
			prom.set_value(SendMessage(msgId, msg));
		};
		return prom.get_future();
	}
	catch (const std::exception& ex) {
		throw;
	}
}

std::vector<uint8_t> MClient::SendMessage(const std::string& msgId, zmq::message_t& msg)
{

	zmq::message_t identity(msgId.data(), msgId.size());
	socket.send(identity, ZMQ_SNDMORE);
	socket.send(msg);

	zmq::pollitem_t items[] = {{socket, 0, ZMQ_POLLIN, 0}};
	std::vector<zmq::message_t> messages;
	while (1) {
		zmq::poll(&items[0], 1, clientConfiguration.readTimeout.count());

		if (items[0].revents & ZMQ_POLLIN) {
			zmq::message_t reply;
			socket.recv(&reply);
			messages.emplace_back(std::move(reply));
			int more = 0;           //  Multipart detection
			size_t more_size = sizeof(more);
			socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
			if (!more) {
				break;
			}              //  Last message part
			else {
				//std::cout << "Multipart!" << std::endl;
			}
		}
		else {
			socket = zmq::socket_t(clientConfiguration.ctx, ZMQ_DEALER);
			configureSocket();
			throw std::runtime_error("Timeout");
		}
	}
	std::string messageId(static_cast<char*>(messages.begin()->data()), messages.begin()->size());
	if (messageId != msgId) {
		std::cout << "Message ID missmatch. Send: " << msgId << " Received: " << messageId << std::endl;
	}
	return std::vector<uint8_t>(static_cast<char*>(messages.back().data()),
								static_cast<char*>(messages.back().data()) + messages.back().size());
}

Message MClient::sendRequest(const std::string& str)
{
	return sendRequest(static_cast<const void*>(str.data()), str.size());
}

Message MClient::sendRequest(std::string&& str)
{
	return sendClonedRequest(static_cast<const void*>(str.data()), str.size());
}

void MClient::configureSocket()
{
	socket.connect(clientConfiguration.uri);
	int linger = 0;
	socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

	identity = "Socket:" + std::to_string(gen());
	socket.setsockopt(ZMQ_IDENTITY, identity.data(), identity.size());
}
