#include "event_channel.h"

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <typeinfo>

using namespace std;

template<typename T>
void print_t(T t)
{
    cout << __FUNCTION__ << ": " << t << endl;
}

class widget
{
public:
    void print_int(int i)
	{
	    cout << this << ":" << __FUNCTION__ << ": " << i << endl;
	}
	
    template<typename T>
    void print_t(T t)
	{
        cout << this << ":" << __FUNCTION__ << ": " << t << "(" << typeid(T).name() << ")" << endl;
	}
};

class thread_pooled
{
	unique_ptr<boost::asio::io_service> io_service = make_unique<boost::asio::io_service>();
	unique_ptr<boost::asio::io_service::work> work = make_unique<boost::asio::io_service::work>(*io_service);
	vector<thread> threads;

public:
	thread_pooled(size_t const n_threads) : threads{n_threads}
	{
		for(auto& t : threads)
		{
			t = thread([&] { io_service->run(); });
		}
	}

	thread_pooled(thread_pooled&& rhs) = default;

	~thread_pooled()
	{
		work.reset();

		for_each(threads.begin(), threads.end(), [](thread& t)
				 {
					 t.join();
				 });
	}

	void operator()(event_channel::events_t const& events, event_channel::dispatchers_t const& dispatchers)
	{
		for(auto const& event : events)
		{
			for(auto const& dispatcher : dispatchers.at(event.type()))
			{
				io_service->post(boost::bind(dispatcher.second, event));
			}
		}
	}
};

int main()
{
	event_channel::channel<thread_pooled> ec{thread_pooled{4}};

    // Subscribe a global function.
	ec.subscribe(print_t<int>);

    // Publish an 'int' event and sleep for a second.
    // We should observe the print statement from print_t<int>.
	ec.send(1);
	this_thread::sleep_for(chrono::seconds(1));


    // Subscribe two widgets, one stack-allocated and one heap-allocated through make_shared<>.
	widget w1;
	auto w2 = make_shared<widget>();
	ec.subscribe(&w1, &widget::print_int);
	ec.subscribe(w2, &widget::print_t<double>);

	// Publish an 'int' event and a 'double' event.
    // We should observe the print statements from print_t<int>, w1::print_int and w2::print_t<double>.
    ec.send(2);
	ec.send(33.3);
	this_thread::sleep_for(chrono::seconds(1));


    // Unsubscribe the second widget.
	ec.unsubscribe(w2, &widget::print_t<double>);

    // Publish an 'int' event and a 'double' event.
    // We should observe the print statements from print_t<int>, w1::print_int.
    // We should not observe the print statement from w2::print_t<double>.
    ec.send(4);
	ec.send(55.5);
	this_thread::sleep_for(chrono::seconds(1));

    
    // Subscribe a callable.
	auto simon_says = ([](const string& s){ cout << "Simon says: " << s << endl; });
    event_channel::handler_tag_t tag1 = ec.subscribe<decltype(simon_says), string const&>(simon_says);
	
    // Send two strings.
    // We should observe the print statements from the simon_says lambda.
	ec.send(string("Touch your nose!"));
	this_thread::sleep_for(chrono::seconds(1));

	ec.send(string("Touch your chin!"));
	this_thread::sleep_for(chrono::seconds(1));
    
    
    // Unsubscribe the callable (via its tag).
    ec.unsubscribe(tag1);
    
    // Send a string.
    // We should not observe any print statement.
    ec.send(string("Touch your tail!"));
    this_thread::sleep_for(chrono::seconds(1));

    // We're done.
    // Everything will be cleaned-up automatically.
	return 0;
}
