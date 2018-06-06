#include <iostream>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <thread>
#include "ZConfig.h"
#include "Client.h"

int main(int argc, char** argv)
{
	zmq::context_t ctx;
	{
		MQConfig clientConfig(ctx);
		clientConfig.uri = "tcp://127.0.0.1:55556";
		MClient client(std::move(clientConfig));

		boost::uuids::random_generator gen;
		boost::uuids::uuid id = gen();
		auto clientId = boost::uuids::to_string(id);
		std::cout << "Client ID: " << clientId << std::endl;
		std::string str = "Hello";
		str += boost::uuids::to_string(id);
		str.resize(1024, 'a');
		auto cycles = 10'000ul;
		auto start = std::chrono::high_resolution_clock::now();
		double throughput = 0;

		auto msg = str + "_" + clientId;
		for (auto i = 0ul; i < cycles; ++i) {
			throughput += msg.size();
			try {
				auto resp = client.sendRequest(msg);
				resp.first.get();
				resp.second.get();
				//std::cout << std::string(reinterpret_cast<char*>(reply.data()), reply.size()) << std::endl;
			}
			catch (const std::exception& ex) {
				std::cout << "Client failed. Reason: " << ex.what() << std::endl;
			}
		}
		auto end = std::chrono::high_resolution_clock::now();
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		std::cout << "Latency: " << us / cycles << "us." << std::endl;
		std::cout << "Througput: " << std::fixed << throughput / us * 1'000'000 / 1024 / 1024 << "MiB/s." << std::endl;
	}

//	{
//		std::vector<std::unique_ptr<MClient>> clients;
//		for (auto i:{1,2}) {
//			MQConfig clientConfig(ctx);
//			clientConfig.uri = "tcp://127.0.0.1:55556";
//			clients.emplace_back(std::make_unique<MClient>(std::move(clientConfig)));
//		}
//
//		boost::uuids::random_generator gen;
//		boost::uuids::uuid id = gen();
//		auto clientId = boost::uuids::to_string(id);
//		std::cout << "Client ID: " << clientId << std::endl;
//		std::string str = "Hello";
//		str.resize(1024, 'a');
//		auto cycles = 100ul;
//		auto start = std::chrono::high_resolution_clock::now();
//		double throughput = 0;
//
//		auto poolSize = 10;
//
//		for (auto j = 0ul; j < cycles / poolSize; ++j) {
//			std::vector<std::pair<std::future<void>, Message>> pool;
//			for (auto i = 0ul; i < poolSize; ++i) {
//				auto msg = str + "_" + clientId;
//				throughput += msg.size();
//				try {
//					pool.emplace_back(clients[i % clients.size()]->sendRequest(msg));
//				}
//				catch (const std::exception& ex) {
//					std::cout << "Client failed. Reason: " << ex.what() << std::endl;
//				}
//			}
//			for (auto& message:pool) {
//				if (message.second.valid()) {
//					try {
//						message.first.get();
//						message.second.get();
//					}
//					catch (std::exception& ex) {
//						std::cout << "Future exception: " << ex.what() << std::endl;
//					}
//				}
//				else {
//					std::cout << "Invalid future." << std::endl;
//				}
//			}
//		}
//		auto end = std::chrono::high_resolution_clock::now();
//		auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
//		std::cout << "Async client latency: " << us / cycles << "us." << std::endl;
//		std::cout << "Througput: " << std::fixed << throughput / us * 1'000'000 / 1024 / 1024 << "MiB/s." << std::endl;
//	}

	//	{
	//		bool shouldStart = false;
	//		auto numberOfClients = 32ul;
	//		std::vector<std::thread> threads;
	//		std::atomic<uint64_t> bytes {0};
	//
	//		MQConfig clientConfig(ctx);
	//		clientConfig.uri = "tcp://127.0.0.1:55556";
	//		MClient client(std::move(clientConfig));
	//
	//		for (auto i = 0ul; i < numberOfClients; ++i) {
	//			threads.emplace_back(std::thread([&client, &bytes, &shouldStart]() {
	//				boost::uuids::random_generator gen;
	//				boost::uuids::uuid id = gen();
	//				auto clientId = boost::uuids::to_string(id);
	//				std::string str = "Hello";
	//				str.resize(1024, 'a');
	//				auto cycles = 10'000ul;
	//				auto msg = str + "_" + clientId;
	//				while (!shouldStart) {
	//					std::this_thread::yield();
	//				}
	//				auto poolSize = 100;
	//
	//				for (auto j = 0ul; j < cycles / poolSize; ++j) {
	//					std::vector<std::pair<std::future<void>, Message>> pool;
	//					for (auto k = 0ul; k < poolSize; ++k) {
	//						bytes += msg.size();
	//						pool.emplace_back(client.sendRequest(msg));
	//					}
	//					for (auto& message:pool) {
	//						if (message.second.valid()) {
	//							try {
	//								message.first.get();
	//								message.second.get();
	//							}
	//							catch (std::exception& ex) {
	//								std::cout << "Future exception: " << ex.what() << std::endl;
	//							}
	//						}
	//						else {
	//							std::cout << "Invalid future." << std::endl;
	//						}
	//					}
	//				}
	//			}));
	//		}
	//		shouldStart = true;
	//		auto start = std::chrono::high_resolution_clock::now();
	//		for (auto& thread:threads) {
	//			thread.join();
	//		}
	//		auto end = std::chrono::high_resolution_clock::now();
	//		auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	//		std::cout << "Througput of " << numberOfClients << " clients: " << std::fixed << double(bytes) / us * 1'000'000 / 1024 / 1024
	//				  << "MiB/s." << std::endl;
	//	}
	return 0;
}