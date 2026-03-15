#ifndef RTC_ICE_H_
#define RTC_ICE_H_

#include <stdint.h>

#include "rtc/rtc.h"

typedef enum rtc_ice_state {
  RTC_ICE_STATE_NEW = 0,
  RTC_ICE_STATE_GATHERING = 1,
  RTC_ICE_STATE_CHECKING = 2,
  RTC_ICE_STATE_CONNECTED = 3,
  RTC_ICE_STATE_FAILED = 4
} rtc_ice_state_t;

typedef enum rtc_ice_pair_state {
  RTC_ICE_PAIR_STATE_WAITING = 0,
  RTC_ICE_PAIR_STATE_INPROGRESS = 1,
  RTC_ICE_PAIR_STATE_SUCCEEDED = 2,
  RTC_ICE_PAIR_STATE_FAILED = 3
} rtc_ice_pair_state_t;

typedef struct rtc_ice_remote_candidate {
  uint8_t in_use;
  uint8_t ip[4];
  uint16_t port;
  uint32_t priority;
  char raw[RTC_CFG_MAX_CANDIDATE_LEN];
} rtc_ice_remote_candidate_t;

typedef struct rtc_ice_candidate_pair {
  uint8_t in_use;
  uint8_t remote_index;
  uint8_t state;
  uint8_t transaction_valid;
  uint16_t retry_count;
  uint32_t last_check_ms;
  uint32_t priority;
  uint8_t transaction_id[12];
} rtc_ice_candidate_pair_t;

typedef struct rtc_ice_stun_out {
  uint8_t ip[4];
  uint16_t port;
  uint16_t len;
  uint8_t data[RTC_CFG_MTU];
} rtc_ice_stun_out_t;

typedef struct rtc_ice_event {
  uint8_t emit_local_description;
  uint8_t emit_local_candidate;
  uint8_t retry_performed;
  uint8_t connected;
  uint8_t failed;
  rtc_result_t error;
} rtc_ice_event_t;

typedef struct rtc_ice_ctx {
  rtc_ice_state_t state;
  uint8_t local_description_emitted;
  uint8_t local_candidate_emitted;
  uint8_t remote_description_set;
  uint8_t answer_ready;
  uint8_t local_host_ready;
  uint8_t local_fingerprint_ready;
  uint8_t media_order_count;
  uint8_t media_order[2];
  uint8_t audio_offer_present;
  uint8_t video_offer_present;
  uint8_t audio_offer_direction;
  uint8_t video_offer_direction;
  uint8_t audio_accepted;
  uint8_t video_accepted;
  uint8_t audio_rtcp_mux;
  uint8_t video_rtcp_mux;
  uint8_t video_h264_found;
  uint8_t video_h264_pm1_found;
  int16_t audio_pcma_pt;
  int16_t audio_pcmu_pt;
  int16_t audio_selected_pt;
  int16_t audio_first_pt;
  int16_t video_h264_pt;
  int16_t video_h264_pm1_pt;
  int16_t video_selected_pt;
  int16_t video_first_pt;
  uint8_t audio_ssrc_present;
  uint8_t video_ssrc_present;
  uint32_t audio_remote_ssrc;
  uint32_t video_remote_ssrc;
  uint32_t peer_id;
  uint16_t local_candidate_count;
  uint16_t remote_candidate_count;
  uint16_t pair_count;
  uint16_t connect_ticks;
  uint16_t retry_count;
  int16_t active_pair_index;
  int16_t selected_pair_index;
  uint32_t last_tick_ms;
  uint32_t checks_sent;
  uint32_t checks_ok;
  uint32_t checks_failed;
  uint32_t checks_drop;
  uint8_t local_host_ip[4];
  uint16_t local_host_port;
  rtc_ice_remote_candidate_t remote_candidate_items[RTC_CFG_MAX_REMOTE_CANDIDATES];
  rtc_ice_candidate_pair_t pairs[RTC_CFG_MAX_CANDIDATE_PAIRS];
  rtc_ice_stun_out_t stun_out_queue[RTC_CFG_RTCP_FB_QUEUE];
  uint16_t stun_out_head;
  uint16_t stun_out_tail;
  uint16_t stun_out_size;
  uint16_t stun_out_high_watermark;
  char local_sdp[RTC_CFG_MAX_SDP_LEN];
  char local_ice_ufrag[32];
  char local_ice_pwd[64];
  char local_fingerprint[96];
  char local_description_type[16];
  char local_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  char remote_sdp[RTC_CFG_MAX_SDP_LEN];
  char remote_ice_ufrag[64];
  char remote_ice_pwd[64];
  char remote_fingerprint[96];
  char remote_setup[16];
  char audio_mid[16];
  char video_mid[16];
  char video_fmtp[128];
  char remote_description_type[16];
  char remote_candidates[RTC_CFG_MAX_REMOTE_CANDIDATES][RTC_CFG_MAX_CANDIDATE_LEN];
} rtc_ice_ctx_t;

void rtc_ice_init(rtc_ice_ctx_t *ctx);
void rtc_ice_set_peer_id(rtc_ice_ctx_t *ctx, uint32_t peer_id);
rtc_result_t rtc_ice_set_local_host(rtc_ice_ctx_t *ctx, const uint8_t ip[4],
                                    uint16_t port);
rtc_result_t rtc_ice_set_local_fingerprint(rtc_ice_ctx_t *ctx,
                                           const char *fingerprint_sha256);
rtc_result_t rtc_ice_start(rtc_ice_ctx_t *ctx, uint32_t peer_id, uint32_t now_ms);
rtc_result_t rtc_ice_set_remote_description(rtc_ice_ctx_t *ctx, const char *sdp,
                                            const char *type);
rtc_result_t rtc_ice_add_remote_candidate(rtc_ice_ctx_t *ctx, const char *candidate);
void rtc_ice_tick(rtc_ice_ctx_t *ctx, uint32_t now_ms, uint16_t retry_interval_ms,
                  uint16_t max_retries, rtc_ice_event_t *out_event);
rtc_result_t rtc_ice_handle_incoming_stun(rtc_ice_ctx_t *ctx, const uint8_t src_ip[4],
                                          uint16_t src_port, const uint8_t *buf,
                                          uint16_t len, uint32_t now_ms);
rtc_result_t rtc_ice_dequeue_outgoing_stun(rtc_ice_ctx_t *ctx, uint8_t out_ip[4],
                                           uint16_t *out_port, uint8_t *out_buf,
                                           uint16_t *io_len);
rtc_result_t rtc_ice_get_selected_remote(const rtc_ice_ctx_t *ctx, uint8_t out_ip[4],
                                         uint16_t *out_port);
uint16_t rtc_ice_stun_out_depth(const rtc_ice_ctx_t *ctx);
uint16_t rtc_ice_stun_out_high_watermark(const rtc_ice_ctx_t *ctx);

#endif  // RTC_ICE_H_
