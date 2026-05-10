#ifndef RTC_API_PEER_CONNECTION_INTERNAL_H
#define RTC_API_PEER_CONNECTION_INTERNAL_H

#include "memory/arena.h"
#include "rtc/peer_connection.h"

struct rtc_peer_connection_t {
    rtc_arena_view_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_executors_t executors;
    int is_closed;
};

#endif
