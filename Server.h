#pragma once

#include <thread>
#include <zmq.hpp>
#include <iostream>
#include "ZConfig.h"

template<typename Func>
class MServer
{
public:

	MServer(MQConfig&& config, Func&& functor) :
			serverConfiguration(std::move(config)), serverSocket(serverConfiguration.ctx, ZMQ_ROUTER),
			workersSocket(serverConfiguration.ctx, ZMQ_DEALER), requestProcessor(std::move(functor))
	{
		prepare();
	}

	void stop()
	{
		shouldStop = true;
		for (auto& workerThread:workers) {
			workerThread.join();
		}
	}

private:
	// Initialize ZMQ sockets
	void prepare() noexcept
	{
		serverSocket.bind(serverConfiguration.uri);
		workersSocket.bind("inproc://workers");

		for (auto i = 0ul; i < serverConfiguration.serverWorkers; ++i) {
			workers.emplace_back(std::thread([this]() {
				zmq::socket_t workerSocket(serverConfiguration.ctx, ZMQ_DEALER);
				workerSocket.connect("inproc://workers");

				while (!shouldStop) {
					try {
						std::vector<zmq::message_t> messages;

						while (1) {
							zmq::message_t msg;
							workerSocket.recv(&msg);
							messages.emplace_back(std::move(msg));
							int64_t more;
							size_t more_size = sizeof(more);
							workerSocket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
							if (!more) {
								break;
							}
						}
						for (auto i = 0ul; i < messages.size(); ++i) {
							if (i < messages.size() - 1) {
								workerSocket.send(messages[i], ZMQ_SNDMORE);
							}
							else {
								workerSocket.send(requestProcessor(messages[i]));
							}
						}
					}
					catch (const std::exception& ex) {
						std::string err = "Failed to send response. Reason: ";
						err += ex.what();
						try {
							serverSocket.send(zmq::message_t(err.data(), err.size()));
						}
						catch (const std::exception& e) {
							std::cout << "Failed to send response. Reason: " << ex.what() << ". Original reason: " << err << std::endl;
						}
					}
				}
			}));
		}
		zmq::proxy(serverSocket, workersSocket, nullptr);
	}

	std::vector<std::thread> workers;
	MQConfig serverConfiguration;
	zmq::socket_t serverSocket;
	zmq::socket_t workersSocket;
	Func requestProcessor;
	bool shouldStop = false;
};