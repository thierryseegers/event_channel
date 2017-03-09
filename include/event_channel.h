#pragma once

#include <any>
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

typedef std::any wrapped_event_t;								//!< Type of an event.
typedef std::vector<wrapped_event_t> tagged_wrapped_events_t;	//!< Type of a collection of events.

typedef std::map<handler_tag_t, std::function<void (wrapped_event_t)>> tagged_handlers_t;	//!< Type of handlers key'ed by their tags.
typedef std::map<std::type_index, tagged_handlers_t> dispatchers_t;							//!< Type of tagged handlers key'ed by event types.

}

//! Set of event dispatching policies to use with \ref event_channel::channel.
namespace dispatch_policy
{

//! Policy class to use with \ref event_channel::channel.
//! Serially invokes subscribed handlers for a given message.
struct sequential
{
	//! Dispatching function.
	static void dispatch(detail::tagged_wrapped_events_t const& events, detail::dispatchers_t const& dispatchers)
	{
		for(auto const& event : events)
		{
			for(auto const& dispatcher : dispatchers.at(event.type()))
			{
				dispatcher.second(event);
			}
		}
	}
};

//! Policy class to use with \ref event_channel::channel.
//! Invokes subscribed handlers in parallel for a given message.
struct parallel
{
	//! Dispatching function.
	static void dispatch(detail::tagged_wrapped_events_t const& events, detail::dispatchers_t const& dispatchers)
	{
		for(auto const& event : events)
		{
			std::vector<std::future<void>> waiters;

			for(auto const& dispatcher : dispatchers.at(event.type()))
			{
				waiters.push_back(std::async([&](){ dispatcher.second(event); }));
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
	using make_tuple_type_t = typename std::result_of<decltype(&std::make_tuple<Args...>)(Args...)>::type;

	// Convenience function to get a type_index out of a \ref tuple_type_t<Args...>.
	template<typename... Args>
	static std::type_index event_type_index()
	{
		return typeid(make_tuple_type_t<Args...>);
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

	std::mutex dispatchers_m_, dispatchers_pending_m_, events_m_;
	std::condition_variable events_cv_;
	std::thread run_t_;

	bool processing_;                           //!< Whether we are processing incoming events or not.
	
	unsigned long generic_handler_tagger_;      //!< The counter-style tag for \c Callable that can't be tracked otherwise.

	detail::tagged_wrapped_events_t events_;    //!< Holds unprocessed events.
	
	detail::dispatchers_t	dispatchers_pending_,   //!< Buffers subscribers.
							dispatchers_;           //!< Holds subscribers.

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
					
					// Move pending subscribers from \ref dispatchers_pending_ to \ref dispatchers_.
					// This allows users to add more subscribers while we process events.
					// If we didn't do that, subscribing would block while events are processed since \ref dispatchers_ must remain intact while that happens.
					// Mind you, as it is now, unsubscribing will still block while events are processed. To avoid this, we would need the equivalent of dispatchers_pending_ for removal.
					std::unique_lock<std::mutex> lgd(dispatchers_m_, std::defer_lock);
					{
						std::unique_lock<std::mutex> lgdp(dispatchers_pending_m_, std::defer_lock);
						std::lock(lgd, lgdp);
						
						for(auto& d : dispatchers_pending_)
						{
                            dispatchers_[d.first].insert(std::make_move_iterator(d.second.begin()), std::make_move_iterator(d.second.end()));
						}
						dispatchers_pending_.clear();
					}
					
					// Process events using given DispatchPolicy.
					DispatchPolicy::dispatch(events, dispatchers_);
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
		std::lock_guard<std::mutex> lg(dispatchers_pending_m_);
		
		dispatchers_pending_[event_type_index<Args...>()][make_tag(f)] =
			[f](detail::wrapped_event_t params)
			{
				std::apply(f, std::any_cast<make_tuple_type_t<Args...>>(params));
			};
	}

	//! Subscribe an object instance and a member function as an event handler.
	template<typename T, typename R, typename... Args>
	void subscribe(T* p, R (T::*f)(Args...))
	{
		std::lock_guard<std::mutex> lg(dispatchers_pending_m_);
		
		dispatchers_pending_[event_type_index<Args...>()][make_tag(p, f)] =
			[p, f](detail::wrapped_event_t params)
			{
				std::apply(f, std::tuple_cat(std::make_tuple(p), std::any_cast<make_tuple_type_t<Args...>>(params)));
			};
	}

	//!\brief Subscribe an object instance and a member function as an event handler.
	//!
	//! The \c weak_ptr<> is saved and invoked only if it can be locked.
	template<typename T, typename R, typename... Args>
	void subscribe(std::shared_ptr<T> const& p, R (T::*f)(Args...))
	{
		std::lock_guard<std::mutex> lg(dispatchers_pending_m_);
		
		dispatchers_pending_[event_type_index<Args...>()][make_tag(p.get(), f)] =
			[w = std::weak_ptr<T>(p), f](detail::wrapped_event_t params)
			{
				if(auto const p = w.lock())
				{
					std::apply(f, std::tuple_cat(std::make_tuple(p), std::any_cast<make_tuple_type_t<Args...>>(params)));
				}
			};
	}

	//!\brief Subscribe a \c Callable as an event handler.
	//!
	//!\return A tag to use with its \c unsubcribe counterpart.
	template<typename F, typename... Args>
	handler_tag_t subscribe(F f/*, typename std::enable_if<std::is_callable<F(Args...)>, void **>::type = nullptr*/)
	{
		std::lock_guard<std::mutex> lg(dispatchers_pending_m_);
		
		dispatchers_pending_[event_type_index<Args...>()][generic_handler_tagger_] =
			[f](detail::wrapped_event_t params)
			{
				std::apply(f, std::any_cast<make_tuple_type_t<Args...>>(params));
			};
		
		return generic_handler_tagger_++;
	};

	//! Unsubscribe a previously subscribed function.
	template<typename R, typename... Args>
	void unsubscribe(R (*f)(Args...))
	{
		std::unique_lock<std::mutex> lgd(dispatchers_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatchers_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		auto const t = event_type_index<Args...>();
		detail::dispatchers_t::iterator i;
		if((i = dispatchers_.find(t)) != dispatchers_.end())
		{
			i->second.erase(make_tag(f));
		}
		else if((i = dispatchers_pending_.find(t)) != dispatchers_pending_.end())
		{
			i->second.erase(make_tag(f));
		}
	};

	//! Unsubscribe a previously subscribed object instance and its member function.
	template<typename T, typename R, typename... Args>
	void unsubscribe(T* p, R (T::*f)(Args...))
	{
		std::unique_lock<std::mutex> lgd(dispatchers_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatchers_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		auto const t = event_type_index<Args...>();
		detail::dispatchers_t::iterator i;
		if((i = dispatchers_.find(t)) != dispatchers_.end())
		{
			i->second.erase(make_tag(p, f));
		}
		else if((i = dispatchers_pending_.find(t)) != dispatchers_pending_.end())
		{
			i->second.erase(make_tag(p, f));
		}
	};

	//! Unsubscribe a previously subscribed object instance and its member function.
	template<typename T, typename R, typename... Args>
	void unsubscribe(std::shared_ptr<T> const& p, R (T::*f)(Args...))
	{
		std::unique_lock<std::mutex> lgd(dispatchers_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatchers_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		auto const t = event_type_index<Args...>();
		detail::dispatchers_t::iterator i;
		if((i = dispatchers_.find(t)) != dispatchers_.end())
		{
			i->second.erase(make_tag(p.get(), f));
		}
		else if((i = dispatchers_pending_.find(t)) != dispatchers_pending_.end())
		{
			i->second.erase(make_tag(p.get(), f));
		}
	};

	//! Unsubscribe a previously subscribed \c Callable.
	void unsubscribe(handler_tag_t tag)
	{
		std::unique_lock<std::mutex> lgd(dispatchers_m_, std::defer_lock);
		std::unique_lock<std::mutex> lgdp(dispatchers_pending_m_, std::defer_lock);
		std::lock(lgd, lgdp);

		for(auto& d : dispatchers_)
		{
			d.second.erase(tag);
		}
		for(auto& d : dispatchers_pending_)
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
			events_.push_back(std::make_any<make_tuple_type_t<Args...>>(std::make_tuple(std::forward<Args>(args)...)));
			events_cv_.notify_one();
		}
	}
};

}
