#ifndef RTC_EXECUTOR_H
#define RTC_EXECUTOR_H

#include <stdint.h>

#include "rtc/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtc_executor_kind_t {
    RTC_EXECUTOR_SIGNALING = 0,
    RTC_EXECUTOR_MEDIA,
    RTC_EXECUTOR_NETWORK
} rtc_executor_kind_t;

typedef void (*rtc_executor_task_fn)(void *user_data);

typedef struct rtc_executor_vtable_t {
    rtc_status_t (*post)(void *user_data, rtc_executor_task_fn task,
                         void *task_user_data);
    rtc_status_t (*schedule_timer)(void *user_data, uint64_t delay_ms,
                                   rtc_executor_task_fn task,
                                   void *task_user_data,
                                   uint64_t *out_timer_id);
    rtc_status_t (*cancel_timer)(void *user_data, uint64_t timer_id);
    void *user_data;
} rtc_executor_vtable_t;

typedef struct rtc_executors_t {
    rtc_executor_vtable_t signaling;
    rtc_executor_vtable_t media;
    rtc_executor_vtable_t network;
} rtc_executors_t;

#ifdef __cplusplus
}
#endif

#endif
