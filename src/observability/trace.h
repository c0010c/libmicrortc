#ifndef RTC_OBSERVABILITY_TRACE_H
#define RTC_OBSERVABILITY_TRACE_H

#include "rtc/observer.h"
#include "rtc/trace.h"

void rtc_trace_emit(const rtc_observer_vtable_t *observer, const char *event,
                    const rtc_trace_field_t *fields, size_t field_count);

#endif
