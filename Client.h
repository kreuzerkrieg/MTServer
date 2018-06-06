#pragma once

#include <future>
#include <random>
#include <boost/lockfree/queue.hpp>
#include <deque>
#include <fstream>
#include <map>
#include "ZConfig.h"
#include "ObjectPool.h"

class MQConfig;

using Message = std::future<std::vector<uint8_t>>;

class MClient
{
public:
	explicit MClient(MQConfig&& config);

	~MClient();

	std::pair<std::future<void>, Message> sendRequest(const void* data, uint64_t size);

	std::pair<std::future<void>, Message> sendRequest(std::string&& str);

	std::pair<std::future<void>, Message> sendRequest(const std::string& str);

	template<typename T, std::enable_if_t<
			std::is_same<T, uint8_t>::value || std::is_same<T, int8_t>::value || std::is_same<T, char>::value ||
			std::is_same<T, unsigned char>::value>* = nullptr>
	std::pair<std::future<void>, Message> sendRequest(const std::vector<T>& buff)
	{
		return sendRequest(static_cast<const void*>(buff.data()), buff.size());
	}

private:
	void configureSocket();

	void SendMessage(uint64_t msgId, zmq::message_t& msg);

	void SendMessages(zmq::socket_t& soc, std::vector<zmq::message_t>& messages);

	std::random_device rd;
	std::mt19937 gen;
	std::uniform_int_distribution<uint64_t> distribution;
	std::unordered_map<uint64_t, std::promise<std::vector<uint8_t>>> promises;
	std::mutex queueMutex;
	std::map<std::chrono::steady_clock::time_point, uint64_t> timeouts;
	std::mutex timeoutMutex;
	std::mutex timeoutCVMutex;
	std::condition_variable timeoutCV;

	uint64_t identity;
	MQConfig clientConfiguration;
	zmq::socket_t socket;
	zmq::socket_t clientSocket;

	bool shouldStop = false;
	std::thread ioThread;
	std::thread timeoutThread;
};