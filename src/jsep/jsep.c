#include "jsep/jsep.h"

rtc_status_t rtc_jsep_can_create_offer(rtc_jsep_state_t state)
{
    return state == RTC_JSEP_STABLE ? RTC_STATUS_OK
                                    : RTC_STATUS_INVALID_STATE;
}

rtc_status_t rtc_jsep_can_create_answer(rtc_jsep_state_t state)
{
    return state == RTC_JSEP_HAVE_REMOTE_OFFER ? RTC_STATUS_OK
                                               : RTC_STATUS_INVALID_STATE;
}

rtc_status_t rtc_jsep_apply_local(rtc_jsep_state_t state, rtc_sdp_type_t type,
                                  rtc_jsep_state_t *out_state)
{
    if (out_state == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (state == RTC_JSEP_STABLE && type == RTC_SDP_TYPE_OFFER) {
        *out_state = RTC_JSEP_HAVE_LOCAL_OFFER;
        return RTC_STATUS_OK;
    }
    if (state == RTC_JSEP_HAVE_REMOTE_OFFER && type == RTC_SDP_TYPE_ANSWER) {
        *out_state = RTC_JSEP_STABLE;
        return RTC_STATUS_OK;
    }
    return RTC_STATUS_INVALID_STATE;
}

rtc_status_t rtc_jsep_apply_remote(rtc_jsep_state_t state, rtc_sdp_type_t type,
                                   rtc_jsep_state_t *out_state)
{
    if (out_state == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (state == RTC_JSEP_STABLE && type == RTC_SDP_TYPE_OFFER) {
        *out_state = RTC_JSEP_HAVE_REMOTE_OFFER;
        return RTC_STATUS_OK;
    }
    if (state == RTC_JSEP_HAVE_LOCAL_OFFER && type == RTC_SDP_TYPE_ANSWER) {
        *out_state = RTC_JSEP_STABLE;
        return RTC_STATUS_OK;
    }
    return RTC_STATUS_INVALID_STATE;
}

const char *rtc_jsep_state_name(rtc_jsep_state_t state)
{
    switch (state) {
    case RTC_JSEP_STABLE:
        return "stable";
    case RTC_JSEP_HAVE_LOCAL_OFFER:
        return "have-local-offer";
    case RTC_JSEP_HAVE_REMOTE_OFFER:
        return "have-remote-offer";
    default:
        return "unknown";
    }
}
