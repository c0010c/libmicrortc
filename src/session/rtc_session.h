#ifndef RTC_SESSION_H_
#define RTC_SESSION_H_

#include <stdint.h>

#include "dtls/rtc_dtls.h"
#include "ice/rtc_ice.h"
#include "platform/rtc_platform.h"
#include "rtp/rtc_rtp.h"
#include "rtc/rtc.h"
#include "srtp/rtc_srtp.h"
#include "transport/rtc_transport.h"

#ifndef RTC_CFG_MAX_ENGINES
#define RTC_CFG_MAX_ENGINES 1u
#endif

struct rtc_engine;

struct rtc_peer {
  uint8_t in_use;
  uint32_t peer_id;
  rtc_peer_state_t state;
  rtc_peer_config_t config;
  rtc_peer_stats_t stats;
  struct rtc_engine *engine;

  rtc_ice_ctx_t ice;
  rtc_dtls_ctx_t dtls;
  rtc_srtp_ctx_t srtp;
  rtc_rtp_ctx_t rtp;
  rtc_transport_ctx_t transport;
};

struct rtc_engine {
  uint8_t in_use;
  uint16_t active_peer_limit;
  uint16_t active_peer_count;
  uint32_t next_peer_id;
  rtc_log_sink_t log_sink;
  rtc_engine_stats_t stats;
  rtc_peer_t peers[RTC_CFG_MAX_PEERS];
};

rtc_result_t rtc_session_engine_create(const rtc_engine_config_t *config,
                                       rtc_engine_t **out_engine);
rtc_result_t rtc_session_engine_destroy(rtc_engine_t *engine);
rtc_result_t rtc_session_engine_poll(rtc_engine_t *engine, uint32_t now_ms,
                                     uint32_t budget_us);
rtc_result_t rtc_session_engine_get_stats(const rtc_engine_t *engine,
                                          rtc_engine_stats_t *out_stats);

rtc_result_t rtc_session_peer_create(rtc_engine_t *engine,
                                     const rtc_peer_config_t *config,
                                     rtc_peer_t **out_peer);
rtc_result_t rtc_session_peer_destroy(rtc_peer_t *peer);
rtc_result_t rtc_session_peer_start(rtc_peer_t *peer);
rtc_result_t rtc_session_peer_stop(rtc_peer_t *peer);
rtc_result_t rtc_session_peer_get_id(const rtc_peer_t *peer, uint32_t *out_peer_id);
rtc_result_t rtc_session_peer_get_state(const rtc_peer_t *peer,
                                        rtc_peer_state_t *out_state);
rtc_result_t rtc_session_peer_get_stats(const rtc_peer_t *peer,
                                        rtc_peer_stats_t *out_stats);
rtc_result_t rtc_session_peer_set_remote_description(rtc_peer_t *peer,
                                                      const char *sdp,
                                                      const char *type);
rtc_result_t rtc_session_peer_add_remote_candidate(rtc_peer_t *peer,
                                                    const char *candidate);
rtc_result_t rtc_session_peer_send_video_h264(rtc_peer_t *peer,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint32_t timestamp90k,
                                               uint8_t marker);
rtc_result_t rtc_session_peer_send_audio_g711(rtc_peer_t *peer,
                                               rtc_audio_codec_t codec,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint32_t timestamp8k);
rtc_result_t rtc_session_peer_datachannel_open(rtc_peer_t *peer, const char *label,
                                                uint16_t *out_stream_id);
rtc_result_t rtc_session_peer_datachannel_send(rtc_peer_t *peer,
                                                uint16_t stream_id,
                                                const uint8_t *payload,
                                                uint16_t payload_len);
rtc_result_t rtc_session_peer_datachannel_close(rtc_peer_t *peer,
                                                 uint16_t stream_id);

#endif  // RTC_SESSION_H_
