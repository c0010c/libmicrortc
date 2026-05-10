#ifndef RTC_OBSERVABILITY_OBSERVER_H
#define RTC_OBSERVABILITY_OBSERVER_H

#include "rtc/observer.h"

void rtc_observer_emit_state(const rtc_observer_vtable_t *observer,
                             const char *state);
void rtc_observer_emit_error(const rtc_observer_vtable_t *observer,
                             rtc_status_t status, const char *subsystem,
                             const char *operation, int detail_code);
void rtc_observer_emit_trace(const rtc_observer_vtable_t *observer,
                             const char *event,
                             const rtc_trace_field_t *fields,
                             size_t field_count);

#endif
