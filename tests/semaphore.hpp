#include <condition_variable> 
#include <mutex>

class semaphore
{
	int value_;
	std::mutex mutex_;
	std::condition_variable cond_;
 
public:
	semaphore(int value) : value_(value) {}

	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		value_--;
		cond_.wait(lock, [this]{ return value_ >= 0; });
	}

	void signal()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		value_++;
		cond_.notify_one();
	}
}; 
