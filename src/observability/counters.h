#ifndef RTC_OBSERVABILITY_COUNTERS_H
#define RTC_OBSERVABILITY_COUNTERS_H

#include "rtc/counters.h"

void rtc_counters_init(rtc_peer_connection_counters_t *counters);
void rtc_counters_note_trace(rtc_peer_connection_counters_t *counters);
void rtc_counters_note_capacity_error(rtc_peer_connection_counters_t *counters);
void rtc_counters_note_affinity_error(rtc_peer_connection_counters_t *counters);
void rtc_counters_note_unsupported_api(rtc_peer_connection_counters_t *counters);

#endif
