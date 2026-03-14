#ifndef RTC_H
#define RTC_H

#include <stddef.h>
#include <stdint.h>

#include "rtc_platform.h"
#include "rtc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_session rtc_session_t;

typedef struct {
  void (*on_ice_state)(void *user, rtc_ice_state_t state);
  void (*on_dtls_state)(void *user, rtc_dtls_state_t state);
  void (*on_channel_open)(void *user, uint16_t channel_id, const char *label);
  void (*on_channel_message)(void *user,
                             uint16_t channel_id,
                             const uint8_t *data,
                             size_t len);
  void (*on_local_candidate)(void *user, const char *candidate_line);
  void (*on_error)(void *user, rtc_result_t err, const char *msg);
  void *user;
} rtc_callbacks_t;

rtc_session_t *rtc_session_create(const rtc_config_t *cfg,
                                  const rtc_platform_ops_t *ops,
                                  void *platform_user,
                                  const rtc_callbacks_t *cbs);

rtc_result_t rtc_session_set_remote_offer(rtc_session_t *s, const char *sdp_offer);
rtc_result_t rtc_session_add_remote_candidate(rtc_session_t *s,
                                              const char *candidate_line);
rtc_result_t rtc_session_create_answer(rtc_session_t *s,
                                       char *out_sdp,
                                       size_t out_len,
                                       size_t *written);
rtc_result_t rtc_session_start(rtc_session_t *s);
rtc_result_t rtc_session_poll(rtc_session_t *s);
rtc_result_t rtc_session_close(rtc_session_t *s);
void rtc_session_destroy(rtc_session_t *s);

rtc_result_t rtc_channel_open(rtc_session_t *s, const char *label, uint16_t *channel_id);
rtc_result_t rtc_channel_send(rtc_session_t *s,
                              uint16_t channel_id,
                              const uint8_t *data,
                              size_t len);
rtc_result_t rtc_channel_close(rtc_session_t *s, uint16_t channel_id);

#ifdef __cplusplus
}
#endif

#endif
