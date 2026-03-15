#ifndef RTC_RTC_H_
#define RTC_RTC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc_config.h"

#define RTC_API_VERSION 1u

typedef struct rtc_engine rtc_engine_t;
typedef struct rtc_peer rtc_peer_t;

typedef enum rtc_log_level {
  RTC_LOG_ERROR = 0,
  RTC_LOG_WARN = 1,
  RTC_LOG_INFO = 2,
  RTC_LOG_DEBUG = 3
} rtc_log_level_t;

typedef enum rtc_result {
  RTC_OK = 0,
  RTC_ERR_INVALID_ARG = -1,
  RTC_ERR_INVALID_STATE = -2,
  RTC_ERR_RESOURCE_EXHAUSTED = -3,
  RTC_ERR_PROTOCOL = -4,
  RTC_ERR_TIMEOUT = -5,
  RTC_ERR_NOT_INIT = -6,
  RTC_ERR_NOT_SUPPORTED = -7,
  RTC_ERR_BUFFER_TOO_SMALL = -8,
  RTC_ERR_OVERFLOW = -9,
  RTC_ERR_DTLS_HANDSHAKE_FAILED = -10,
  RTC_ERR_SRTP_ACTIVATE_FAILED = -11,
  RTC_ERR_AUTH_FAILED = -12
} rtc_result_t;

typedef enum rtc_peer_state {
  RTC_PEER_STATE_NEW = 0,
  RTC_PEER_STATE_STARTING = 1,
  RTC_PEER_STATE_ICE_CHECKING = 2,
  RTC_PEER_STATE_DTLS_HANDSHAKE = 3,
  RTC_PEER_STATE_CONNECTED = 4,
  RTC_PEER_STATE_STOPPED = 5,
  RTC_PEER_STATE_FAILED = 6
} rtc_peer_state_t;

typedef enum rtc_audio_codec {
  RTC_AUDIO_CODEC_PCMA = 0,
  RTC_AUDIO_CODEC_PCMU = 1
} rtc_audio_codec_t;

typedef enum rtc_dtls_state {
  RTC_DTLS_STATE_NEW = 0,
  RTC_DTLS_STATE_HANDSHAKE = 1,
  RTC_DTLS_STATE_CONNECTED = 2,
  RTC_DTLS_STATE_FAILED = 3
} rtc_dtls_state_t;

typedef enum rtc_dtls_cert_mode {
  RTC_DTLS_CERT_MODE_STATIC = 0,
  RTC_DTLS_CERT_MODE_EPHEMERAL = 1
} rtc_dtls_cert_mode_t;

typedef void (*rtc_log_callback_t)(rtc_log_level_t level, const char *module, uint32_t peer_id,
                                   rtc_result_t code, const char *message, void *user_data);

typedef void (*rtc_peer_state_change_cb_t)(rtc_peer_t *peer, rtc_peer_state_t old_state,
                                           rtc_peer_state_t new_state, void *user_data);

typedef void (*rtc_local_description_cb_t)(rtc_peer_t *peer, const char *sdp, const char *type,
                                           void *user_data);

typedef void (*rtc_local_candidate_cb_t)(rtc_peer_t *peer, const char *candidate,
                                         void *user_data);

typedef void (*rtc_video_frame_cb_t)(rtc_peer_t *peer, const uint8_t *payload,
                                     uint16_t payload_len, uint32_t timestamp90k, void *user_data);

typedef void (*rtc_audio_frame_cb_t)(rtc_peer_t *peer, rtc_audio_codec_t codec,
                                     const uint8_t *payload, uint16_t payload_len,
                                     uint32_t timestamp8k, void *user_data);

typedef void (*rtc_keyframe_request_cb_t)(rtc_peer_t *peer, uint32_t media_ssrc,
                                          void *user_data);

typedef struct rtc_engine_config {
  uint16_t version;
  uint16_t size;
  rtc_log_level_t min_log_level;
  rtc_log_callback_t log_cb;
  void *log_user_data;
  uint16_t active_peer_limit;
  uint8_t dtls_cert_mode;
  uint8_t dtls_debug_enabled;
  rtc_log_level_t dtls_backend_log_level;
  uint8_t reserved;
} rtc_engine_config_t;

typedef struct rtc_peer_config {
  uint16_t version;
  uint16_t size;
  uint16_t max_retries;
  uint16_t retry_interval_ms;
  uint16_t dtls_handshake_timeout_ms;
  uint16_t dtls_handshake_max_retries;
  rtc_peer_state_change_cb_t on_state_change;
  rtc_local_description_cb_t on_local_description;
  rtc_local_candidate_cb_t on_local_candidate;
  rtc_video_frame_cb_t on_video_frame;
  rtc_audio_frame_cb_t on_audio_frame;
  void *user_data;
  rtc_keyframe_request_cb_t on_keyframe_request;
} rtc_peer_config_t;

typedef struct rtc_peer_stats {
  uint32_t tx_video_frames;
  uint32_t tx_audio_frames;
  uint32_t rx_video_frames;
  uint32_t rx_audio_frames;
  uint32_t dropped_packets;
  uint32_t retransmit_count;
  uint32_t timeout_count;
  uint32_t protocol_error_count;
  uint16_t rtp_tx_queue_depth;
  uint16_t rtp_tx_queue_high_watermark;
  uint16_t rtp_rx_queue_depth;
  uint16_t rtp_rx_queue_high_watermark;
  uint16_t local_candidate_count;
  uint16_t remote_candidate_count;
  uint8_t dtls_state;
  uint8_t srtp_active;
  int16_t dtls_last_error;
  uint32_t dtls_handshake_elapsed_ms;
  uint32_t ice_checks_sent;
  uint32_t ice_checks_ok;
  uint32_t ice_checks_failed;
  uint32_t ice_checks_drop;
  int16_t ice_last_error;
  uint32_t dtls_rx_pkts;
  uint32_t dtls_tx_pkts;
  uint32_t srtp_unprotect_fail;
  uint32_t rtcp_rx_pkts;
  uint32_t rtcp_rr_rx;
  uint32_t rtcp_pli_rx;
  uint32_t rtcp_nack_rx;
  uint32_t rtcp_nack_retx;
  uint16_t stun_rx_queue_depth;
  uint16_t stun_rx_queue_high_watermark;
  uint16_t dtls_rx_queue_depth;
  uint16_t dtls_rx_queue_high_watermark;
  uint16_t ice_stun_tx_queue_depth;
  uint16_t ice_stun_tx_queue_high_watermark;
  uint32_t transport_rx_drop_pkts;
  uint32_t transport_stun_drop_pkts;
  uint32_t transport_dtls_drop_pkts;
  uint32_t transport_io_error_count;
  uint32_t queue_overflow_count;
} rtc_peer_stats_t;

typedef struct rtc_engine_stats {
  uint16_t active_peers;
  uint16_t active_peers_high_watermark;
  uint32_t poll_count;
  uint32_t poll_budget_exhaust_count;
} rtc_engine_stats_t;

rtc_result_t rtc_engine_create(const rtc_engine_config_t *config, rtc_engine_t **out_engine);
rtc_result_t rtc_engine_destroy(rtc_engine_t *engine);

rtc_result_t rtc_engine_poll(rtc_engine_t *engine, uint32_t now_ms, uint32_t budget_us);
rtc_result_t rtc_engine_get_stats(const rtc_engine_t *engine, rtc_engine_stats_t *out_stats);

rtc_result_t rtc_peer_create(rtc_engine_t *engine, const rtc_peer_config_t *config,
                             rtc_peer_t **out_peer);
rtc_result_t rtc_peer_destroy(rtc_peer_t *peer);
rtc_result_t rtc_peer_start(rtc_peer_t *peer);
rtc_result_t rtc_peer_stop(rtc_peer_t *peer);

rtc_result_t rtc_peer_get_id(const rtc_peer_t *peer, uint32_t *out_peer_id);
rtc_result_t rtc_peer_get_state(const rtc_peer_t *peer, rtc_peer_state_t *out_state);
rtc_result_t rtc_peer_get_stats(const rtc_peer_t *peer, rtc_peer_stats_t *out_stats);

rtc_result_t rtc_peer_set_remote_description(rtc_peer_t *peer, const char *sdp,
                                             const char *type);
rtc_result_t rtc_peer_add_remote_candidate(rtc_peer_t *peer, const char *candidate);

rtc_result_t rtc_peer_send_video_h264(rtc_peer_t *peer, const uint8_t *payload,
                                      uint16_t payload_len, uint32_t timestamp90k,
                                      uint8_t marker);
rtc_result_t rtc_peer_send_audio_g711(rtc_peer_t *peer, rtc_audio_codec_t codec,
                                      const uint8_t *payload, uint16_t payload_len,
                                      uint32_t timestamp8k);

rtc_result_t rtc_peer_datachannel_open(rtc_peer_t *peer, const char *label,
                                       uint16_t *out_stream_id);
rtc_result_t rtc_peer_datachannel_send(rtc_peer_t *peer, uint16_t stream_id,
                                       const uint8_t *payload, uint16_t payload_len);
rtc_result_t rtc_peer_datachannel_close(rtc_peer_t *peer, uint16_t stream_id);

const char *rtc_engine_threading_model(void);
int rtc_engine_internal_worker_enabled(void);

#ifdef __cplusplus
}
#endif

#endif  // RTC_RTC_H_
