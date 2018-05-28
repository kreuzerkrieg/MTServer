#pragma once

#include <future>
#include <random>
#include <boost/lockfree/queue.hpp>
#include <deque>
#include <fstream>
#include "ZConfig.h"

class MQConfig;

using Message = std::future<std::vector<uint8_t>>;

class MClient
{
public:
	explicit MClient(MQConfig&& config);

	~MClient();

	Message sendClonedRequest(const void* data, uint64_t size);

	Message sendRequest(const void* data, uint64_t size);

	Message sendRequest(std::string&& str);

	template<typename T, std::enable_if_t<
			std::is_same<T, uint8_t>::value || std::is_same<T, int8_t>::value || std::is_same<T, char>::value ||
			std::is_same<T, unsigned char>::value>* = nullptr>
	Message sendRequest(std::vector<T>&& buff)
	{
		return sendClonedRequest(static_cast<const void*>(buff.data()), buff.size());
	}

	Message sendRequest(const std::string& str);

	template<typename T, std::enable_if_t<
			std::is_same<T, uint8_t>::value || std::is_same<T, int8_t>::value || std::is_same<T, char>::value ||
			std::is_same<T, unsigned char>::value>* = nullptr>
	Message sendRequest(const std::vector<T>& buff)
	{
		return sendRequest(static_cast<const void*>(buff.data()), buff.size());
	}

private:
	void configureSocket();

	struct ClientTask
	{
		std::function<void()> func;
	};
	std::random_device rd;
	std::mt19937 gen;
	std::uniform_int_distribution<uint64_t> distribution;
	std::string identity;
	MQConfig clientConfiguration;
	zmq::socket_t socket;
	std::deque<std::function<void()>> responses;
	std::unordered_map<std::string, std::promise<std::vector<uint8_t>>> promises;
	std::mutex queueMutex;
	std::thread executor;
	std::atomic<uint64_t> counter{0};
	bool shouldStop = false;

	std::vector<uint8_t> SendMessage(const std::string& msgId, zmq::message_t& msg);

};