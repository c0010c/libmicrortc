#include "observability/trace.h"

#include "observability/observer.h"

void rtc_trace_emit(const rtc_observer_vtable_t *observer, const char *event,
                    const rtc_trace_field_t *fields, size_t field_count)
{
    rtc_observer_emit_trace(observer, event, fields, field_count);
}
