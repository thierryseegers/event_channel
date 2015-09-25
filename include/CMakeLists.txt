add_library(libevent_channel INTERFACE)

target_sources(libevent_channel
    INTERFACE event_channel.h)

add_custom_target(event_channel
    SOURCES event_channel.h)
