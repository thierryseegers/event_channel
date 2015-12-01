#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <vector>

//! Encompasses everything related to event channel.
namespace event_channel
{

typedef uintptr_t handler_tag_t;	//!< Tag returned when subscribing callable.

//! Private namespace, not to be used by end-users.
namespace detail
{

typedef std::type_index event_tag_t;	//!< Type of the tag of an event's type.

typedef std::function<const void* ()> wrapped_event_t;					//!< Type of functor that hides an event's type.
typedef std::pair<event_tag_t, wrapped_event_t> tagged_wrapped_event_t;	//!< Type of the association of an event's type tag, and it's associated wrapper functor.
typedef std::vector<tagged_wrapped_event_t> tagged_wrapped_events_t;	//!< Type of a set of events.

typedef std::map<handler_tag_t, std::function<void (const void*)>> tagged_handlers_t;	//!< Type of handlers key'ed by their tags.
typedef std::map<event_tag_t, tagged_handlers_t> dispatcher_t;							//!< Type of tagged handlers key'ed by event types.

}

//! Set of event dispatching policies to use with \ref event_channel::channel.
namespace dispatch_policy
{

//! Policy class to use with \ref event_channel::channel.
//! Serially invokes subscribed handlers for a given message.
struct sequential
{
	//! Dispatching function.
	static void dispatch(detail::tagged_wrapped_events_t const& events, detail::dispatcher_t const& dispatcher)
	{
		for(auto const& e : events)
		{
			for(auto const& d : dispatcher.at(e.first))
			{
				d.second(e.second());
			}
		}
	}
};

//! Policy class to use with \ref event_channel::channel.
//! Invokes subscribed handlers in parallel for a given message.
struct parallel
{
	//! Dispatching function.
	static void dispatch(detail::tagged_wrapped_events_t const& events, detail::dispatcher_t const& dispatcher)
	{
		for(auto const& e : events)
		{
			std::vector<std::future<void>> waiters;

			for(auto const& d : dispatcher.at(e.first))
			{
				waiters.push_back(std::async([=](){ d.second(e.second()); }));
			}

			for(auto& w : waiters)
			{
				w.wait();
			}
		}
	}
};

}

//! Set of idle policies to use with \ref event_channel::channel.
namespace idle_policy
{

bool const keep_events = true;          //!< When stopped, retain unprocessed and incoming events.
bool const drop_events = !keep_events;  //!< When stopped, drop unprocessed and incoming events.

}

//! The event channel. Handles subscriptions and message dispatching.
//!
//! \tparam DispatchPolicy How to dispatch events. A type from \ref dispatch_policy.
//! \tparam IdlePolicy What to do with incoming events when idle. A value from idle_policy.
template<class DispatchPolicy = dispatch_policy::sequential, bool IdlePolicy = idle_policy::keep_events>
class channel
{
	// Since the type returned by std::make_tuple<Args...> may not be exactly std::tuple<Args...>,
	// we use this type alias to ensure we're using the same type everywhere.
	template<typename... Args>
	using tuple_type_t = typename std::result_of<decltype(&std::make_tuple<Args...>)(Args...)>::type;

	// Convenience function to get a type_index out of a \ref tuple_type_t<Args...>.
	template<typename... Args>
	static auto tuple_type_index()
	{
		return std::type_index(typeid(tuple_type_t<Args...>));
	}

	// Convenience function to invoke a \c Callable with arguments packaged in a \c std::tuple<>.
	template<typename F, typename P, size_t... Is>
	static void invoke(F&& f, P&& params, std::index_sequence<Is...> const&)
	{
		std::forward<F>(f)(std::get<Is>(std::forward<P>(params))...);
	}
	
	// Convenience function to invoke a member function with arguments packaged in a \c std::tuple<>.
	template<typename O, typename F, typename P, size_t... Is>
	static void invoke(O&& o, F&& f, P&& params, std::index_sequence<Is...> const&)
	{
		(o->*std::forward<F>(f))(std::get<Is>(std::forward<P>(params))...);
	}
	
	// Convenience function to map a function to a \ref handler_tag_t.
	template<typename R, typename... Args>
	handler_tag_t make_tag(R (*f)(Args...))
	{
		return reinterpret_cast<handler_tag_t>(f);
	}

	// Convenience function to map a member function to a \ref handler_tag_t.
	template<typename T, typename R, typename... Args>
	handler_tag_t make_tag(T* p, R (T::*f)(Args...))
	{
		return reinterpret_cast<handler_tag_t>(p) + typeid(f).hash_code() * 37;
	}

	std::mutex dispatcher_m_, dispatcher_pending_m_, events_m_;
	std::condition_variable events_cv_;
	std::thread run_t_;

	bool processing_;                           //!< Whether we are processing incoming events or not.
	
	unsigned long generic_handler_tagger_;      //!< The counter-style tag for \c Callable that can't be tracked otherwise.

	detail::tagged_wrapped_events_t events_;    //!< Holds unprocessed events.
	
	detail::dispatcher_t dispatcher_pending_,   //!< Buffers subscribers.
						 dispatcher_;           //!< Holds subscribers.

public:
	channel() : processing_(false), generic_handler_tagger_(0)
	{
		start();
	}

	virtual ~channel()
	{
		stop();
	}

	//! Start dispatching events.
	void start()
	{
		std::lock_guard<std::mutex> lg(events_m_);
		
		if(!processing_)
		{
			processing_ = true;
		}
		else
		{
			return;
		}

		run_t_ = std::thread([this]()
			{
				while(processing_)
				{
					detail::tagged_wrapped_events_t events;
					
					// Wait until we are told to stop processing events or until we have events to process.
					{
						std::unique_lock<std::mutex> ul(events_m_);
						events_cv_.wait(ul, [this]{ return !processing_ || !events_.empty(); });
					
						if(!processing_)
						{
							return;
						}
						else
						{
							// Move pending events from \ref events_ to a local variable.
							std::swap(events, events_);
						}
					}
					
					// Move pending subscribers from \ref dispatcher_pending_ to \ref dispatcher_.
					// This allows users to add more subscribers while we process events.
					// If we didn't do that, subscribing would block while events are being processed since \ref dispatcher_ must remain intact while that happens.
					{
						std::unique_lock<std::mutex> lgd(dispatcher_m_, std::defer_lock);
						std::unique_lock<std::mutex> lgdp(dispatcher_pending_m_, std::defer_lock);
						std::lock(lgd, lgdp);
						
						for(auto& d : dispatcher_pending_)
						{
                            dispatcher_[d.first].insert(std::make_move_iterator(d.second.begin()), std::make_move_iterator(d.second.end()));
						}
						dispatcher_pending_.clear();
					}
					
					// Process events using given DispatchPolicy.
					DispatchPolicy::dispatch(events, dispatcher_);
				}
			});
	}

	//! \brief Stop dispatching events.
    //!
	//! Resume by calling \ref start.
    //! The value of \p IdlePolicy will dictate what to do with incoming events in the meantime.
	void stop()
	{
		{
			std::lock_guard<std::mutex> lg(events_m_);

			if(IdlePolicy == idle_policy::drop_events)
			{
				events_.clear();
			}

			processing_ = false;
			events_cv_.notify_one();
		}

		run_t_.join();
	}
	
	//! Suscribe a function as an event handler.
	template<typename R, typename... Args>
	void subscribe(R (*f)(Args...))
	{
		std::lock_guard<std::mutex> lg(dispatcher_pending_m_);
		
		dispatcher_pending_[tuple_type_index<Args...>()][make_tag(f)] =
			[f](const void* params)
			{
				invoke(f, *static_cast<const tuple_type_t<Args...>*>(params), std::index_sequence_for<Args...>{});
			};
	}

	//! Subscribe an object instance and a member function as an event handler.
	template<typename T, typename R, typename... Args>
	void subscribe(T* p, R (T::*f)(Args...))
	{
		std::lock_guard<std::mutex> lg(dispatcher_pending_m_);
		
		dispatcher_pending_[tuple_type_index<Args...>()][make_tag(p, f)] =
			[p, f](const void* params)
			{
				invoke(p, f, *static_cast<const tuple_type_t<Args...>*>(params), std::index_sequence_for<Args...>{});
			};
	}

	//!\brief Subscribe an object instance and a member function as an event handler.
	//!
	//! The \c weak_ptr<> is saved and invoked only if it can be locked.
	template<typename T, typename R, typename... Args>
	void subscribe(std::shared_ptr<T> const& p, R (T::*f)(Args...))
	{
		std::lock_guard<std::mutex> lg(dispatcher_pending_m_);
		
		dispatcher_pending_[tuple_type_index<Args...>()][make_tag(p.get(), f)] =
			[w = std::weak_ptr<T>(p), f](const void* params)
			{
				if(auto const p = w.lock())
				{
					invoke(p.get(), f, *static_cast<const tuple_type_t<Args...>*>(params), std::index_sequence_for<Args...>{});
				}
			};
	}

	//!\brief Subscribe a \c Callable as an event handler.
	//!
	//!\return A tag to use with its \c unsubcribe counterpart.
	template<typename F, typename... Args>
	handler_tag_t subscribe(F f)
	{
		std::lock_guard<std::mutex> lg(dispatcher_pending_m_);
		
		dispatcher_pending_[tuple_type_index<Args...>()][generic_handler_tagger_] =
			[f](const void* params)
			{
				invoke(f, *static_cast<const tuple_type_t<Args...>*>(params), std::index_sequence_for<Args...>{});
			};
		
		return generic_handler_tagger_++;
	};

	//! Unsubscribe a previously subscribed function.
	template<typename R, typename... Args>
	void unsubscribe(R (*f)(Args...))
	{
		std::unique_lock<std::mutex> lgd(dispatcher_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatcher_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		auto const t = tuple_type_index<Args...>();
		detail::dispatcher_t::iterator i;
		if((i = dispatcher_.find(t)) != dispatcher_.end())
		{
			i->second.erase(make_tag(f));
		}
		else if((i = dispatcher_pending_.find(t)) != dispatcher_pending_.end())
		{
			i->second.erase(make_tag(f));
		}
	};

	//! Unsubscribe a previously subscribed object instance and its member function.
	template<typename T, typename R, typename... Args>
	void unsubscribe(T* p, R (T::*f)(Args...))
	{
		std::unique_lock<std::mutex> lgd(dispatcher_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatcher_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		auto const t = tuple_type_index<Args...>();
		detail::dispatcher_t::iterator i;
		if((i = dispatcher_.find(t)) != dispatcher_.end())
		{
			i->second.erase(make_tag(p, f));
		}
		else if((i = dispatcher_pending_.find(t)) != dispatcher_pending_.end())
		{
			i->second.erase(make_tag(p, f));
		}
	};

	//! Unsubscribe a previously subscribed object instance and its member function.
	template<typename T, typename R, typename... Args>
	void unsubscribe(std::shared_ptr<T> const& p, R (T::*f)(Args...))
	{
		std::unique_lock<std::mutex> lgd(dispatcher_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatcher_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		auto const t = tuple_type_index<Args...>();
		detail::dispatcher_t::iterator i;
		if((i = dispatcher_.find(t)) != dispatcher_.end())
		{
			i->second.erase(make_tag(p.get(), f));
		}
		else if((i = dispatcher_pending_.find(t)) != dispatcher_pending_.end())
		{
			i->second.erase(make_tag(p.get(), f));
		}
	};

	//! Unsubscribe a previously subscribed \c Callable.
	void unsubscribe(handler_tag_t tag)
	{
		std::unique_lock<std::mutex> lgd(dispatcher_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatcher_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		for(auto& d : dispatcher_)
		{
			d.second.erase(tag);
		}
		for(auto& d : dispatcher_pending_)
		{
			d.second.erase(tag);
		}
	};

	//! Send an event.
	template<typename... Args>
	void send(Args&&... args)
	{
		std::lock_guard<std::mutex> lg(events_m_);
		
		if(processing_ || IdlePolicy == idle_policy::keep_events)
		{
			events_.push_back(std::make_pair(tuple_type_index<Args...>(), [e = std::make_tuple(std::forward<Args>(args)...)]{ return &e; }));
			events_cv_.notify_one();
		}
	}
};

}
