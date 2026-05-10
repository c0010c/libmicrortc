#ifndef RTC_JSEP_JSEP_H
#define RTC_JSEP_JSEP_H

#include "rtc/status.h"
#include "sdp/sdp.h"

typedef enum rtc_jsep_state_t {
    RTC_JSEP_STABLE = 0,
    RTC_JSEP_HAVE_LOCAL_OFFER,
    RTC_JSEP_HAVE_REMOTE_OFFER
} rtc_jsep_state_t;

rtc_status_t rtc_jsep_can_create_offer(rtc_jsep_state_t state);
rtc_status_t rtc_jsep_can_create_answer(rtc_jsep_state_t state);
rtc_status_t rtc_jsep_apply_local(rtc_jsep_state_t state, rtc_sdp_type_t type,
                                  rtc_jsep_state_t *out_state);
rtc_status_t rtc_jsep_apply_remote(rtc_jsep_state_t state, rtc_sdp_type_t type,
                                   rtc_jsep_state_t *out_state);
const char *rtc_jsep_state_name(rtc_jsep_state_t state);

#endif
