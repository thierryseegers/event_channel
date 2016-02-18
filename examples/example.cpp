#include "event_channel.h"

#include <chrono>
#include <iostream>
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

#include <map>

int main()
{
	event_channel::channel<> ec;

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
