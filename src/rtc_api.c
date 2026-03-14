#include "rtc/rtc.h"

#include "session/rtc_session.h"

rtc_result_t rtc_engine_create(const rtc_engine_config_t *config,
                               rtc_engine_t **out_engine) {
  return rtc_session_engine_create(config, out_engine);
}

rtc_result_t rtc_engine_destroy(rtc_engine_t *engine) {
  return rtc_session_engine_destroy(engine);
}

rtc_result_t rtc_engine_poll(rtc_engine_t *engine, uint32_t now_ms,
                             uint32_t budget_us) {
  return rtc_session_engine_poll(engine, now_ms, budget_us);
}

rtc_result_t rtc_engine_get_stats(const rtc_engine_t *engine,
                                  rtc_engine_stats_t *out_stats) {
  return rtc_session_engine_get_stats(engine, out_stats);
}

rtc_result_t rtc_peer_create(rtc_engine_t *engine, const rtc_peer_config_t *config,
                             rtc_peer_t **out_peer) {
  return rtc_session_peer_create(engine, config, out_peer);
}

rtc_result_t rtc_peer_destroy(rtc_peer_t *peer) {
  return rtc_session_peer_destroy(peer);
}

rtc_result_t rtc_peer_start(rtc_peer_t *peer) {
  return rtc_session_peer_start(peer);
}

rtc_result_t rtc_peer_stop(rtc_peer_t *peer) {
  return rtc_session_peer_stop(peer);
}

rtc_result_t rtc_peer_get_id(const rtc_peer_t *peer, uint32_t *out_peer_id) {
  return rtc_session_peer_get_id(peer, out_peer_id);
}

rtc_result_t rtc_peer_get_state(const rtc_peer_t *peer,
                                rtc_peer_state_t *out_state) {
  return rtc_session_peer_get_state(peer, out_state);
}

rtc_result_t rtc_peer_get_stats(const rtc_peer_t *peer,
                                rtc_peer_stats_t *out_stats) {
  return rtc_session_peer_get_stats(peer, out_stats);
}

rtc_result_t rtc_peer_set_remote_description(rtc_peer_t *peer, const char *sdp,
                                             const char *type) {
  return rtc_session_peer_set_remote_description(peer, sdp, type);
}

rtc_result_t rtc_peer_add_remote_candidate(rtc_peer_t *peer,
                                           const char *candidate) {
  return rtc_session_peer_add_remote_candidate(peer, candidate);
}

rtc_result_t rtc_peer_send_video_h264(rtc_peer_t *peer, const uint8_t *payload,
                                      uint16_t payload_len,
                                      uint32_t timestamp90k, uint8_t marker) {
  return rtc_session_peer_send_video_h264(peer, payload, payload_len, timestamp90k,
                                          marker);
}

rtc_result_t rtc_peer_send_audio_g711(rtc_peer_t *peer, rtc_audio_codec_t codec,
                                      const uint8_t *payload,
                                      uint16_t payload_len,
                                      uint32_t timestamp8k) {
  return rtc_session_peer_send_audio_g711(peer, codec, payload, payload_len,
                                          timestamp8k);
}

rtc_result_t rtc_peer_datachannel_open(rtc_peer_t *peer, const char *label,
                                       uint16_t *out_stream_id) {
  return rtc_session_peer_datachannel_open(peer, label, out_stream_id);
}

rtc_result_t rtc_peer_datachannel_send(rtc_peer_t *peer, uint16_t stream_id,
                                       const uint8_t *payload,
                                       uint16_t payload_len) {
  return rtc_session_peer_datachannel_send(peer, stream_id, payload, payload_len);
}

rtc_result_t rtc_peer_datachannel_close(rtc_peer_t *peer, uint16_t stream_id) {
  return rtc_session_peer_datachannel_close(peer, stream_id);
}

const char *rtc_engine_threading_model(void) {
  return "single-thread tick/poll; caller serialized";
}

int rtc_engine_internal_worker_enabled(void) {
#if RTC_CFG_ENABLE_INTERNAL_WORKER
  return 1;
#else
  return 0;
#endif
}
