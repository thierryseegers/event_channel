#include "event_channel.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "semaphore.hpp"

#include <functional>
#include <string>

using namespace std;

template<typename T>
class receiver
{
	semaphore* message_received_;

	vector<T> values_;

public:
	receiver(semaphore* message_received) : message_received_(message_received) {}

	void receive(const T& v)
	{
		values_.push_back(v);

		message_received_->signal();
	}

	const vector<T>& values() const
	{
		return values_;
	}
};

template<typename MessageType, typename DispatchPolicy>
void test(const MessageType message, const unsigned short message_count, const unsigned short receiver_count)
{
	// Tests with receivers instantiated on the stack.
	{
		semaphore messages_acknowledged(1 - message_count * receiver_count);

		event_channel::channel<DispatchPolicy> c;

		vector<receiver<MessageType>> receivers(receiver_count, receiver<MessageType>(&messages_acknowledged));
		for(unsigned short i = 0; i != receiver_count; ++i)
		{
			c.subscribe(&receivers[i], &receiver<MessageType>::receive);
		}

		for(unsigned short i = 0; i != message_count; ++i)
		{
			c.send(message);
		}

		messages_acknowledged.wait();

		for(const auto& r : receivers)
		{
			for(const auto& v : r.values())
			{
				REQUIRE(v == message);
			}
		}
	}

	// Tests with receivers allocated through std::make_shared.
	{
		semaphore messages_acknowledged(1 - message_count * receiver_count);

		event_channel::channel<DispatchPolicy> c;

		vector<shared_ptr<receiver<MessageType>>> receivers;
		for(unsigned short i = 0; i != receiver_count; ++i)
		{
			receivers.push_back(make_shared<receiver<MessageType>>(&messages_acknowledged));
			c.subscribe(receivers[i], &receiver<MessageType>::receive);
		}

		for(unsigned short i = 0; i != message_count; ++i)
		{
			c.send(message);
		}

		messages_acknowledged.wait();

		for(const auto& r : receivers)
		{
			for(const auto& v : r->values())
			{
				REQUIRE(v == message);
			}
		}
	}

	// Tests with lambda receivers.
	{
		semaphore messages_acknowledged(1 - message_count * receiver_count);

		event_channel::channel<DispatchPolicy> c;

		vector<vector<MessageType>> messages_received(receiver_count);
		for(unsigned short i = 0; i != receiver_count; ++i)
		{

            auto f = [&messages_acknowledged, &messages_received, i](const MessageType& message)
            {
                messages_received[i].push_back(message);
                messages_acknowledged.signal();
            };

            c.template subscribe<decltype(f), const MessageType&>(f);

        }

		for(unsigned short i = 0; i != message_count; ++i)
		{
			c.send(message);
		}

		messages_acknowledged.wait();

		for(const auto& i : messages_received)
		{
			for(const auto& j : i)
			{
				REQUIRE(j == message);
			} 
		}
	}
}

TEST_CASE("listen_and_forget", "")
{
	event_channel::channel<> c;

	{
		receiver<int> r(nullptr);

		c.subscribe(&r, &receiver<int>::receive);

		c.unsubscribe(&r, &receiver<int>::receive);
	}

	{
		receiver<int> r(nullptr);

		c.subscribe(&r, &receiver<int>::receive);

		c.unsubscribe(&r, &receiver<int>::receive);
	}

}

// Simple sanity check test cases that vary a single parameter between: type, number of messages sent, 
// number of receivers the message is sent to, the priority policy and the dispatch_policy.
TEST_CASE("i_1_1_f_s", "")
{
	test<int, event_channel::dispatch_policy::sequential>(22, 1, 1);
}

TEST_CASE("s_1_1_f_s", "")
{
	test<string, event_channel::dispatch_policy::sequential>("orange", 1, 1);
}

TEST_CASE("i_3_1_f_s", "")
{
	test<int, event_channel::dispatch_policy::sequential>(22, 3, 1);
}

TEST_CASE("i_1_3_f_s", "")
{
	test<int, event_channel::dispatch_policy::sequential>(22, 1, 3);
}

TEST_CASE("i_1_1_a_s", "")
{
	test<int, event_channel::dispatch_policy::sequential>(22, 1, 1);
}

TEST_CASE("i_1_1_f_p", "")
{
	test<int, event_channel::dispatch_policy::parallel>(22, 1, 1);
}


// Tests combinations of policies when multiple message are sent to multiple receivers.
TEST_CASE("i_3_3_f_s", "")
{
	test<int, event_channel::dispatch_policy::sequential>(22, 3, 3);
}

TEST_CASE("i_3_3_a_s", "")
{
	test<int, event_channel::dispatch_policy::sequential>(22, 3, 3);
}

TEST_CASE("i_3_3_f_p", "")
{
	test<int, event_channel::dispatch_policy::parallel>(22, 3, 3);
}

TEST_CASE("i_3_3_a_p", "")
{
	test<int, event_channel::dispatch_policy::parallel>(22, 3, 3);
}
