#ifndef RTC_OBSERVER_H
#define RTC_OBSERVER_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"
#include "rtc/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_observer_vtable_t {
    void (*on_state)(void *user_data, const char *state);
    void (*on_error)(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code);
    void (*on_trace)(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count);
    void (*on_local_candidate)(void *user_data, const char *candidate,
                               size_t candidate_len);
    void (*on_media_frame)(void *user_data, const uint8_t *data,
                           size_t data_len);
    void (*on_datagram)(void *user_data, const uint8_t *data, size_t data_len);
    void *user_data;
} rtc_observer_vtable_t;

#ifdef __cplusplus
}
#endif

#endif
