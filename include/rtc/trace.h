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
#define RTC_TRACE_SDP_PARSE "sdp.parse"
#define RTC_TRACE_SDP_WRITE "sdp.write"
#define RTC_TRACE_JSEP_TRANSITION "jsep.transition"
#define RTC_TRACE_JSEP_REJECT "jsep.reject"
#define RTC_TRACE_ICE_CANDIDATE_STORED "ice.candidate.stored"

#define RTC_TRACE_FIELD_SUBSYSTEM "subsystem"
#define RTC_TRACE_FIELD_OPERATION "operation"
#define RTC_TRACE_FIELD_STATUS "status"
#define RTC_TRACE_FIELD_RESOURCE "resource"
#define RTC_TRACE_FIELD_REQUIRED "required"
#define RTC_TRACE_FIELD_USED "used"
#define RTC_TRACE_FIELD_STATE "state"
#define RTC_TRACE_FIELD_TARGET_STATE "target_state"
#define RTC_TRACE_FIELD_DESCRIPTION_TYPE "description_type"
#define RTC_TRACE_FIELD_REASON "reason"

typedef struct rtc_trace_field_t {
    const char *key;
    const char *value;
    uint64_t number;
} rtc_trace_field_t;

#ifdef __cplusplus
}
#endif

#endif
