#pragma once

#include <vector>
#include <memory>
#include <atomic>

template<typename T>
class ObjectPool
{
public:
	class CustomDeleter
	{
	public:
		explicit CustomDeleter(std::atomic<size_t>& idx) : index(idx)
		{}

		void operator()(T* ptr) const
		{
			--index;
		}

	private:
		std::atomic<size_t>& index;
	};

	template<typename... Args>
	ObjectPool(Args&& ...args):pool(1024, new T(std::forward<Args>(args)...))
	{
	}

	std::unique_ptr<T, CustomDeleter> GetObject()
	{
		return std::unique_ptr<T, CustomDeleter>(pool[index.fetch_add(1)], CustomDeleter(index));
	}

private:

	using Storage = std::vector<T*>;
	using Index = std::atomic<size_t>;
	//std::tuple<Args...> constructorArguments;
	Storage pool;
	Index index {0};

};