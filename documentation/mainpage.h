/*!

\file event_channel.h
\brief The only file you need.
\author Thierry Seegers

\mainpage event_channel

\tableofcontents

\section introduction Introduction

\ref event_channel is an exploration project of mine.
After having used a handful of messaging frameworks that forced the user to wrap messages and message handlers in boilerplate code, I set on to discover whether this was strictly necessary.
That is, can I come up with a framework that given a function \c foo(int), can send an \c int to that function asynchronously without having to provide the framework a wrapped \c foo(int) and a wrapped \c int.

Here's an example of what I am trying to avoid:
 
\code

void foo(int)
{}

// Must wrap event data in some "event" class which itself must derive from some base "event" class.
class my_event : public the_framework::event_base
{
    int data_;
};

// Must wrap event handler in some handler functor which itself must derive from some base "handler" class.
class my_event_foo_handler : public the_framework::event_handler_base<my_event>
{
public:
    void operator()(my_event const& e)
    {
        foo(e.data_);
    }
};

the_framework::dispatcher d;
 
my_event_foo_handler h;
d.subscribe(h);
 
my_event e{22};
d.send(e);  // foo(22) is invoked on some other thread.
 
\endcode
 
Depending on the framework in question, the wrapping code will be error-prone, tediously boilerplate or both.
 
The ideal scenario is this one:

\code

void foo(int)
{}

the_framework::dispatcher d;

d.subscribe(foo);

d.send(22); // foo(22) is invoked on some other thread.
 
\endcode

Obvisouly, I can't restrict this framework to <tt>int</tt>s and global functions.
This framework allows messages of any type and handlers of multiple nature (i.e. global functions, member functions and generic callables).
 
\section considerations Technical considerations

One of my goals with \ref event_channel was to learn and explore some new features of C++14.
The first feature I used, albeit more as a convenience than a necessity, is <a href="https://en.wikipedia.org/wiki/C%2B%2B14#Lambda_capture_expressions">lambda capture</a>.
The second feature used is <a href="http://en.cppreference.com/w/cpp/utility/integer_sequence">std::integer_sequence</a>.
That feature is fundamental to \ref event_channel in that it helps us to invoke functions with parameters aggregated in a <a href="http://en.cppreference.com/w/cpp/utility/tuple">std::tuple</a>.
This technique is described <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3915.pdf">here</a>.
Note that this technique may become standard and obviate the need for that private code in \ref event_channel::channel.

This code compiles successfully with AppleClang 7.0.0, VC++ 14 and g++ 5.1.1.
 
\section principles Design principles

\subsection policies Policy-based design

Like other libraries of mine, I often follow principles of <a href="https://en.wikipedia.org/wiki/Policy-based_design">policy-based design</a>.
\ref event_channel::channel currently supports two policies.

\subsubsection dispatch Event dispatching policy
 
The first policy dictates how an event is processed.
When \ref event_channel::channel is instantiated with its dispatch policy set to \ref event_channel::dispatch_policy::sequential, handlers for a given event will be invoked sequentially.
On the other hand, when the dispatch policy is set to \ref event_channel::dispatch_policy::parallel, handlers for a given event will be invoked simultaneously in parallel.
 
\subsubsection idle Idle policy
 
The second policy dictates what happens to events when the \ref event_channel::channel is idle (e.g. hasn't been \ref event_channel::channel::start "started" yet or has been \ref event_channel::channel::stop "stopped").
When \ref event_channel::channel is instantiated with its idle policy set to \ref event_channel::idle_policy::keep_events, unprocessed and incoming events will kept in the queue and processed when the channel is restarted.
Conversely, when the idle policy is set to \ref event_channel::idle_policy::drop_events, unprocessed and incoming events will be discarded as long as the channel is idle.

\section improvements Future improvements
 
More test cases. More. More!
 
\section sample Sample code

\include examples/example.cpp

\section license License

\verbatim
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
\endverbatim

*/

