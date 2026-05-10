#include "observability/observer.h"

void rtc_observer_emit_state(const rtc_observer_vtable_t *observer,
                             const char *state)
{
    if (observer != 0 && observer->on_state != 0) {
        observer->on_state(observer->user_data, state);
    }
}

void rtc_observer_emit_error(const rtc_observer_vtable_t *observer,
                             rtc_status_t status, const char *subsystem,
                             const char *operation, int detail_code)
{
    if (observer != 0 && observer->on_error != 0) {
        observer->on_error(observer->user_data, status, subsystem, operation,
                           detail_code);
    }
}

void rtc_observer_emit_trace(const rtc_observer_vtable_t *observer,
                             const char *event,
                             const rtc_trace_field_t *fields,
                             size_t field_count)
{
    if (observer != 0 && observer->on_trace != 0) {
        observer->on_trace(observer->user_data, event, fields, field_count);
    }
}
