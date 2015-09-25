event_channel is a C++14 exploration project. It is yet another event-dispatcher/message-bus/signal-slot framework. As such, you'll probably be better served by [Boost.Signals2](http://www.boost.org/doc/libs/1_59_0/doc/html/signals2.html).

After having used a handful of messaging frameworks that forced the user to wrap messages and message handlers in boilerplate code, I set on to discover whether this was strictly necessary. That is, can I come up with a framework that given a function `foo(int)`, can send an `int` to that function asynchronously without having to provide the framework a wrapped `foo(int)` and a wrapped `int`.

Here's an example of what I am trying to avoid:

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
 
Depending on the framework in question, the wrapping code will be error-prone, tediously boilerplate or both. The ideal scenario is this one:

    void foo(int)
    {}
    
    the_framework::dispatcher d;
    
    d.subscribe(foo);
    
    d.send(22);  // foo(22) is invoked on some other thread.

Obviously, I can't restrict this framework to `int`s and global functions. This framework allows messages of any type and handlers of multiple nature (i.e. global functions, member functions and generic callables).

event_channel is a single header file found in `include/`.

Documentation and much more prose can be found [here](http://thierryseegers.github.io/event_channel).
