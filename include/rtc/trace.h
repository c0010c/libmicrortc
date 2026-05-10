#ifndef RTC_TRACE_H
#define RTC_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_TRACE_PC_CREATE "pc.create"
#define RTC_TRACE_PC_DESTROY "pc.destroy"
#define RTC_TRACE_CAPACITY_FAILURE "capacity.failure"
#define RTC_TRACE_AFFINITY_VIOLATION "affinity.violation"
#define RTC_TRACE_UNSUPPORTED_API "api.unsupported"

#define RTC_TRACE_FIELD_SUBSYSTEM "subsystem"
#define RTC_TRACE_FIELD_OPERATION "operation"
#define RTC_TRACE_FIELD_STATUS "status"
#define RTC_TRACE_FIELD_RESOURCE "resource"
#define RTC_TRACE_FIELD_REQUIRED "required"
#define RTC_TRACE_FIELD_USED "used"

typedef struct rtc_trace_field_t {
    const char *key;
    const char *value;
    uint64_t number;
} rtc_trace_field_t;

#ifdef __cplusplus
}
#endif

#endif
