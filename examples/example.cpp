#include "event_channel.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <typeinfo>

using namespace std;
using namespace chrono_literals;

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

#include <map>

int main()
{
	event_channel::channel<> ec;

    // Subscribe a global function.
	ec.subscribe(print_t<int>);

    // Publish an 'int' event and sleep for a second.
    // We should observe the print statement from print_t<int>.
	ec.send(1);
	this_thread::sleep_for(1s);


    // Subscribe two widgets, one stack-allocated and one heap-allocated through make_shared<>.
	widget w1;
	auto w2 = make_shared<widget>();
	ec.subscribe(&w1, &widget::print_int);
	ec.subscribe(w2, &widget::print_t<double>);

	// Publish an 'int' event and a 'double' event.
    // We should observe the print statements from print_t<int>, w1::print_int and w2::print_t<double>.
    ec.send(2);
	ec.send(33.3);
	this_thread::sleep_for(1s);


    // Unsubscribe the second widget.
	ec.unsubscribe(w2, &widget::print_t<double>);

    // Publish an 'int' event and a 'double' event.
    // We should observe the print statements from print_t<int>, w1::print_int.
    // We should not observe the print statement from w2::print_t<double>.
    ec.send(4);
	ec.send(55.5);
	this_thread::sleep_for(1s);

    
    // Subscribe a callable.
	auto name = "Simon";
	auto someone_says = ([&](const string& s){ cout << name << " says: " << s << endl; });
    event_channel::handler_tag_t tag1 = ec.subscribe<decltype(someone_says), string const&>(someone_says);
	
    // Send two strings.
    // We should observe the print statements from the someone_says lambda.
	ec.send("Touch your nose!"s);
	this_thread::sleep_for(1s);

	ec.send("Touch your chin!"s);
	this_thread::sleep_for(1s);
    
    // Unsubscribe the callable (via its tag).
    ec.unsubscribe(tag1);
    
    // Send a string.
    // We should not observe any print statement.
    ec.send("Touch your tail!"s);
    this_thread::sleep_for(1s);


	// Use a token to auto-unsubscribe the event handler when the token goes out of scope.
	{
		name = "Silvia";
		auto someone_says = ([&](string const& s){ cout << name << " says: " << s << endl; });
		auto const handle = ec.subscribe<decltype(someone_says), string const&>(event_channel::use_token{}, someone_says);

		// Send a string.
		// We should observe the print statements from the someone_says lambda.
		ec.send("Touch your knee!"s);
		this_thread::sleep_for(1s);
	}

	// Send a string.
	// We should not observe any print statement.
	ec.send("Touch your tailbone!"s);
	this_thread::sleep_for(1s);

    // We're done.
    // Everything will be cleaned-up automatically.
	return 0;
}
