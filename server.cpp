#include <iostream>
#include "ZConfig.h"
#include "Server.h"
#include "Client.h"

int main(int argc, char** argv)
{
	try {
		auto lambda = [counter = 0](auto&& msg) mutable {
//			++counter;
//			std::this_thread::sleep_for(std::chrono::microseconds(100 * (counter % 4)));
			return std::move(msg);
		};
		using functorType = decltype(lambda);
		zmq::context_t ctx;
		MQConfig serverConfig(ctx);
		serverConfig.uri = "tcp://*:55556";
		serverConfig.serverWorkers = 32;
		MServer<functorType> server(std::move(serverConfig), std::move(lambda));
		while (true) { std::this_thread::sleep_for(std::chrono::seconds(10)); }
	}
	catch (const std::exception& ex) {
		std::cout << "Server failed. Reason: " << ex.what() << std::endl;
	}
}