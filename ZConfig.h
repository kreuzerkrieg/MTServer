#pragma once

#include <zmq.hpp>

struct MQConfig
{
	MQConfig(zmq::context_t& context) : ctx(context)
	{};
	std::chrono::milliseconds readTimeout = std::chrono::milliseconds(1000);
	std::chrono::milliseconds writeTimeout = std::chrono::milliseconds(1000);
	std::string uri;
	zmq::context_t& ctx;
	uint16_t maxIOThreads = 1;
	uint16_t maxSockets = 1024;
	uint16_t serverWorkers = 1;
};
