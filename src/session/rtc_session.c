#include "session/rtc_session.h"

#include <string.h>

#define RTC_MODULE_ENGINE "session.engine"
#define RTC_MODULE_PEER "session.peer"
#define RTC_MODULE_ICE "ice"
#define RTC_MODULE_DTLS "dtls"
#define RTC_MODULE_SRTP "srtp"
#define RTC_MODULE_RTP "rtp"

static rtc_engine_t g_engine_pool[RTC_CFG_MAX_ENGINES];

static int rtc_engine_cfg_valid(const rtc_engine_config_t *cfg) {
  if (!cfg) {
    return 0;
  }
  if (cfg->version != RTC_API_VERSION) {
    return 0;
  }
  if (cfg->size < sizeof(rtc_engine_config_t)) {
    return 0;
  }
  return 1;
}

static int rtc_peer_cfg_valid(const rtc_peer_config_t *cfg) {
  if (!cfg) {
    return 0;
  }
  if (cfg->version != RTC_API_VERSION) {
    return 0;
  }
  if (cfg->size < sizeof(rtc_peer_config_t)) {
    return 0;
  }
  return 1;
}

static int rtc_engine_in_pool(const rtc_engine_t *engine, uint16_t *out_idx) {
  uint16_t i;
  if (!engine) {
    return 0;
  }
  for (i = 0; i < RTC_CFG_MAX_ENGINES; ++i) {
    if (&g_engine_pool[i] == engine) {
      if (out_idx) {
        *out_idx = i;
      }
      return 1;
    }
  }
  return 0;
}

static int rtc_find_peer(const rtc_peer_t *peer, rtc_engine_t **out_engine,
                         uint16_t *out_peer_idx) {
  uint16_t e;
  uint16_t p;
  if (!peer) {
    return 0;
  }
  for (e = 0; e < RTC_CFG_MAX_ENGINES; ++e) {
    for (p = 0; p < RTC_CFG_MAX_PEERS; ++p) {
      if (&g_engine_pool[e].peers[p] == peer) {
        if (out_engine) {
          *out_engine = &g_engine_pool[e];
        }
        if (out_peer_idx) {
          *out_peer_idx = p;
        }
        return 1;
      }
    }
  }
  return 0;
}

static void rtc_log_engine(const rtc_engine_t *engine, rtc_log_level_t level,
                           const char *module, uint32_t peer_id, rtc_result_t code,
                           const char *message) {
  if (!engine) {
    return;
  }
  rtc_platform_log(&engine->log_sink, level, module, peer_id, code, message);
}

static void rtc_log_peer(const rtc_peer_t *peer, rtc_log_level_t level,
                         const char *module, rtc_result_t code,
                         const char *message) {
  if (!peer || !peer->engine) {
    return;
  }
  rtc_platform_log(&peer->engine->log_sink, level, module, peer->peer_id, code,
                   message);
}

static void rtc_update_queue_stats(rtc_peer_t *peer) {
  if (!peer) {
    return;
  }
  peer->stats.rtp_tx_queue_depth = rtc_transport_tx_depth(&peer->transport);
  peer->stats.rtp_tx_queue_high_watermark =
      rtc_transport_tx_high_watermark(&peer->transport);
  peer->stats.rtp_rx_queue_depth = rtc_transport_rx_depth(&peer->transport);
  peer->stats.rtp_rx_queue_high_watermark =
      rtc_transport_rx_high_watermark(&peer->transport);
}

static void rtc_peer_set_state(rtc_peer_t *peer, rtc_peer_state_t next,
                               rtc_result_t code, const char *reason,
                               rtc_log_level_t level) {
  rtc_peer_state_t old_state;
  if (!peer) {
    return;
  }
  old_state = peer->state;
  if (old_state == next) {
    return;
  }
  peer->state = next;
  rtc_log_peer(peer, level, RTC_MODULE_PEER, code, reason);
  if (peer->config.on_state_change) {
    peer->config.on_state_change(peer, old_state, next, peer->config.user_data);
  }
}

static rtc_result_t rtc_validate_live_engine(rtc_engine_t *engine,
                                             uint16_t *out_engine_idx) {
  if (!engine) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_engine_in_pool(engine, out_engine_idx)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!engine->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  return RTC_OK;
}

static rtc_result_t rtc_validate_live_peer(rtc_peer_t *peer, rtc_engine_t **out_engine,
                                           uint16_t *out_peer_idx) {
  rtc_engine_t *engine = NULL;
  if (!peer) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_find_peer(peer, &engine, out_peer_idx)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!engine->in_use) {
    return RTC_ERR_NOT_INIT;
  }
  if (!peer->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  if (out_engine) {
    *out_engine = engine;
  }
  return RTC_OK;
}

static void rtc_peer_dispatch_incoming(rtc_peer_t *peer, uint16_t max_packets) {
  uint16_t i;
  uint16_t pumped = 0;
  uint32_t dropped = 0;

  rtc_transport_pump_loopback(&peer->transport, max_packets, &pumped, &dropped);
  if (dropped > 0u) {
    peer->stats.dropped_packets += dropped;
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_RTP, RTC_ERR_OVERFLOW,
                 "rx queue full, drop packet");
  }

  for (i = 0; i < pumped; ++i) {
    rtc_rtp_packet_t packet;
    rtc_rtp_frame_t frame;
    rtc_result_t r;

    r = rtc_transport_dequeue_rx(&peer->transport, &packet);
    if (r != RTC_OK) {
      break;
    }

    r = rtc_srtp_unprotect(&peer->srtp, &packet);
    if (r != RTC_OK) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_SRTP, r,
                   "srtp unprotect failed");
      continue;
    }

    r = rtc_rtp_decode(&packet, &frame);
    if (r != RTC_OK) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_RTP, r, "rtp decode failed");
      continue;
    }

    if (frame.kind == RTC_PACKET_KIND_VIDEO) {
      peer->stats.rx_video_frames++;
      if (peer->config.on_video_frame) {
        peer->config.on_video_frame(peer, frame.payload, frame.payload_len,
                                    frame.timestamp, peer->config.user_data);
      }
    } else if (frame.kind == RTC_PACKET_KIND_AUDIO) {
      peer->stats.rx_audio_frames++;
      if (peer->config.on_audio_frame) {
        peer->config.on_audio_frame(peer, frame.audio_codec, frame.payload,
                                    frame.payload_len, frame.timestamp,
                                    peer->config.user_data);
      }
    }
  }

  rtc_update_queue_stats(peer);
}

static void rtc_peer_poll_state_machine(rtc_peer_t *peer, uint32_t now_ms) {
  if (!peer || !peer->in_use) {
    return;
  }

  if (peer->state == RTC_PEER_STATE_STARTING) {
    if (rtc_ice_start(&peer->ice, peer->peer_id, now_ms) != RTC_OK) {
      peer->stats.protocol_error_count++;
      rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, RTC_ERR_PROTOCOL,
                         "ice start failed", RTC_LOG_ERROR);
      return;
    }
    rtc_peer_set_state(peer, RTC_PEER_STATE_ICE_CHECKING, RTC_OK,
                       "ice checking", RTC_LOG_INFO);
    return;
  }

  if (peer->state == RTC_PEER_STATE_ICE_CHECKING) {
    rtc_ice_event_t ice_event;
    rtc_ice_tick(&peer->ice, now_ms, peer->config.retry_interval_ms,
                 peer->config.max_retries, &ice_event);

    peer->stats.local_candidate_count = peer->ice.local_candidate_count;
    peer->stats.remote_candidate_count = peer->ice.remote_candidate_count;

    if (ice_event.emit_local_description && peer->config.on_local_description) {
      peer->config.on_local_description(
          peer, peer->ice.local_sdp, peer->ice.local_description_type,
          peer->config.user_data);
      rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
                   "local description emitted");
    }
    if (ice_event.emit_local_candidate && peer->config.on_local_candidate) {
      peer->config.on_local_candidate(peer, peer->ice.local_candidate,
                                      peer->config.user_data);
      rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
                   "local candidate emitted");
    }

    if (ice_event.retry_performed) {
      peer->stats.retransmit_count++;
      rtc_log_peer(peer, RTC_LOG_DEBUG, RTC_MODULE_ICE, RTC_OK,
                   "connectivity retry");
    }

    if (ice_event.connected) {
      if (rtc_dtls_start(&peer->dtls) != RTC_OK) {
        peer->stats.protocol_error_count++;
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, RTC_ERR_PROTOCOL,
                           "dtls start failed", RTC_LOG_ERROR);
        return;
      }
      rtc_peer_set_state(peer, RTC_PEER_STATE_DTLS_HANDSHAKE, RTC_OK,
                         "dtls handshake", RTC_LOG_INFO);
      return;
    }

    if (ice_event.failed) {
      peer->stats.timeout_count++;
      rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, ice_event.error,
                         "ice timeout", RTC_LOG_WARN);
      return;
    }

    return;
  }

  if (peer->state == RTC_PEER_STATE_DTLS_HANDSHAKE) {
    rtc_dtls_event_t dtls_event;
    rtc_dtls_tick(&peer->dtls, &dtls_event);

    if (dtls_event.connected) {
      rtc_result_t r = rtc_srtp_activate(&peer->srtp);
      if (r != RTC_OK) {
        peer->stats.protocol_error_count++;
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, r,
                           "srtp activate failed", RTC_LOG_ERROR);
        return;
      }
      rtc_peer_set_state(peer, RTC_PEER_STATE_CONNECTED, RTC_OK,
                         "media connected", RTC_LOG_INFO);
      return;
    }

    if (dtls_event.failed) {
      peer->stats.protocol_error_count++;
      rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, dtls_event.error,
                         "dtls failed", RTC_LOG_ERROR);
      return;
    }
    return;
  }

  if (peer->state == RTC_PEER_STATE_CONNECTED) {
    rtc_peer_dispatch_incoming(peer, 4u);
  }
}

rtc_result_t rtc_session_engine_create(const rtc_engine_config_t *config,
                                       rtc_engine_t **out_engine) {
  uint16_t i;
  rtc_engine_t *engine = NULL;

  if (!config || !out_engine) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_engine_cfg_valid(config)) {
    return RTC_ERR_INVALID_ARG;
  }

  for (i = 0; i < RTC_CFG_MAX_ENGINES; ++i) {
    if (!g_engine_pool[i].in_use) {
      engine = &g_engine_pool[i];
      break;
    }
  }
  if (!engine) {
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }

  memset(engine, 0, sizeof(*engine));
  engine->in_use = 1u;
  engine->next_peer_id = 1u;
  engine->active_peer_limit = config->active_peer_limit;
  if (engine->active_peer_limit == 0u ||
      engine->active_peer_limit > RTC_CFG_MAX_PEERS) {
    engine->active_peer_limit = RTC_CFG_DEFAULT_ACTIVE_PEERS;
  }
  engine->log_sink.min_level = config->min_log_level;
  engine->log_sink.cb = config->log_cb;
  engine->log_sink.user_data = config->log_user_data;

  *out_engine = engine;
  rtc_log_engine(engine, RTC_LOG_INFO, RTC_MODULE_ENGINE, 0, RTC_OK,
                 "engine created");
  return RTC_OK;
}

rtc_result_t rtc_session_engine_destroy(rtc_engine_t *engine) {
  uint16_t engine_idx;
  uint16_t i;

  if (!engine) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_engine_in_pool(engine, &engine_idx)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!engine->in_use) {
    return RTC_OK;
  }

  for (i = 0; i < RTC_CFG_MAX_PEERS; ++i) {
    if (engine->peers[i].in_use) {
      rtc_session_peer_destroy(&engine->peers[i]);
    }
  }

  rtc_log_engine(engine, RTC_LOG_INFO, RTC_MODULE_ENGINE, 0, RTC_OK,
                 "engine destroyed");
  memset(&g_engine_pool[engine_idx], 0, sizeof(g_engine_pool[engine_idx]));
  return RTC_OK;
}

rtc_result_t rtc_session_engine_poll(rtc_engine_t *engine, uint32_t now_ms,
                                     uint32_t budget_us) {
  uint16_t i;
  uint32_t budget_steps;
  uint16_t processed_peers = 0u;
  rtc_result_t vr;

  vr = rtc_validate_live_engine(engine, NULL);
  if (vr != RTC_OK) {
    return vr;
  }

  engine->stats.poll_count++;
  budget_steps = budget_us / 100u;
  if (budget_steps == 0u) {
    budget_steps = 1u;
  }

  for (i = 0; i < RTC_CFG_MAX_PEERS; ++i) {
    if (!engine->peers[i].in_use) {
      continue;
    }
    if (budget_steps == 0u) {
      break;
    }
    rtc_peer_poll_state_machine(&engine->peers[i], now_ms);
    budget_steps--;
    processed_peers++;
  }

  if (processed_peers < engine->active_peer_count) {
    engine->stats.poll_budget_exhaust_count++;
    rtc_log_engine(engine, RTC_LOG_DEBUG, RTC_MODULE_ENGINE, 0, RTC_OK,
                   "poll budget exhausted");
  }

  return RTC_OK;
}

rtc_result_t rtc_session_engine_get_stats(const rtc_engine_t *engine,
                                          rtc_engine_stats_t *out_stats) {
  if (!engine || !out_stats) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!engine->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  *out_stats = engine->stats;
  return RTC_OK;
}

rtc_result_t rtc_session_peer_create(rtc_engine_t *engine,
                                     const rtc_peer_config_t *config,
                                     rtc_peer_t **out_peer) {
  uint16_t i;
  rtc_peer_t *peer = NULL;
  rtc_result_t vr;

  if (!engine || !config || !out_peer) {
    return RTC_ERR_INVALID_ARG;
  }
  vr = rtc_validate_live_engine(engine, NULL);
  if (vr != RTC_OK) {
    return vr;
  }
  if (!rtc_peer_cfg_valid(config)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (engine->active_peer_count >= engine->active_peer_limit) {
    rtc_log_engine(engine, RTC_LOG_ERROR, RTC_MODULE_ENGINE, 0,
                   RTC_ERR_RESOURCE_EXHAUSTED, "active peer limit reached");
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }

  for (i = 0; i < RTC_CFG_MAX_PEERS; ++i) {
    if (!engine->peers[i].in_use) {
      peer = &engine->peers[i];
      break;
    }
  }
  if (!peer) {
    rtc_log_engine(engine, RTC_LOG_ERROR, RTC_MODULE_ENGINE, 0,
                   RTC_ERR_RESOURCE_EXHAUSTED, "peer pool exhausted");
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }

  memset(peer, 0, sizeof(*peer));
  peer->in_use = 1u;
  peer->engine = engine;
  peer->peer_id = engine->next_peer_id++;
  peer->state = RTC_PEER_STATE_NEW;
  peer->config = *config;
  if (peer->config.max_retries == 0u) {
    peer->config.max_retries = 3u;
  }
  if (peer->config.retry_interval_ms == 0u) {
    peer->config.retry_interval_ms = 20u;
  }

  rtc_ice_init(&peer->ice);
  rtc_dtls_init(&peer->dtls);
  rtc_srtp_init(&peer->srtp);
  rtc_rtp_init(&peer->rtp, peer->peer_id);
  rtc_transport_init(&peer->transport);

  engine->active_peer_count++;
  engine->stats.active_peers = engine->active_peer_count;
  if (engine->active_peer_count > engine->stats.active_peers_high_watermark) {
    engine->stats.active_peers_high_watermark = engine->active_peer_count;
  }

  *out_peer = peer;
  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_PEER, RTC_OK, "peer created");
  return RTC_OK;
}

rtc_result_t rtc_session_peer_destroy(rtc_peer_t *peer) {
  rtc_engine_t *engine = NULL;
  uint16_t idx;

  if (!peer) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_find_peer(peer, &engine, &idx)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!engine->in_use) {
    return RTC_OK;
  }
  if (!peer->in_use) {
    return RTC_OK;
  }

  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_PEER, RTC_OK, "peer destroyed");
  memset(&engine->peers[idx], 0, sizeof(engine->peers[idx]));
  if (engine->active_peer_count > 0u) {
    engine->active_peer_count--;
  }
  engine->stats.active_peers = engine->active_peer_count;
  return RTC_OK;
}

rtc_result_t rtc_session_peer_start(rtc_peer_t *peer) {
  rtc_result_t vr;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }

  if (peer->state == RTC_PEER_STATE_STARTING ||
      peer->state == RTC_PEER_STATE_ICE_CHECKING ||
      peer->state == RTC_PEER_STATE_DTLS_HANDSHAKE ||
      peer->state == RTC_PEER_STATE_CONNECTED) {
    return RTC_OK;
  }

  if (peer->state == RTC_PEER_STATE_FAILED) {
    return RTC_ERR_INVALID_STATE;
  }

  rtc_dtls_init(&peer->dtls);
  rtc_srtp_init(&peer->srtp);
  rtc_transport_init(&peer->transport);

  rtc_peer_set_state(peer, RTC_PEER_STATE_STARTING, RTC_OK, "peer start",
                     RTC_LOG_INFO);
  return RTC_OK;
}

rtc_result_t rtc_session_peer_stop(rtc_peer_t *peer) {
  rtc_result_t vr;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }

  if (peer->state == RTC_PEER_STATE_STOPPED) {
    return RTC_OK;
  }

  rtc_peer_set_state(peer, RTC_PEER_STATE_STOPPED, RTC_OK, "peer stop",
                     RTC_LOG_INFO);
  return RTC_OK;
}

rtc_result_t rtc_session_peer_get_id(const rtc_peer_t *peer, uint32_t *out_peer_id) {
  if (!peer || !out_peer_id) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!peer->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  *out_peer_id = peer->peer_id;
  return RTC_OK;
}

rtc_result_t rtc_session_peer_get_state(const rtc_peer_t *peer,
                                        rtc_peer_state_t *out_state) {
  if (!peer || !out_state) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!peer->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  *out_state = peer->state;
  return RTC_OK;
}

rtc_result_t rtc_session_peer_get_stats(const rtc_peer_t *peer,
                                        rtc_peer_stats_t *out_stats) {
  if (!peer || !out_stats) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!peer->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  *out_stats = peer->stats;
  return RTC_OK;
}

rtc_result_t rtc_session_peer_set_remote_description(rtc_peer_t *peer,
                                                      const char *sdp,
                                                      const char *type) {
  rtc_result_t vr;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }

  vr = rtc_ice_set_remote_description(&peer->ice, sdp, type);
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                 "set remote description failed");
    return vr;
  }
  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
               "remote description set");
  return RTC_OK;
}

rtc_result_t rtc_session_peer_add_remote_candidate(rtc_peer_t *peer,
                                                    const char *candidate) {
  rtc_result_t vr;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }

  vr = rtc_ice_add_remote_candidate(&peer->ice, candidate);
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                 "add remote candidate failed");
    return vr;
  }
  peer->stats.remote_candidate_count = peer->ice.remote_candidate_count;
  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
               "remote candidate added");
  return RTC_OK;
}

rtc_result_t rtc_session_peer_send_video_h264(rtc_peer_t *peer,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint32_t timestamp90k,
                                               uint8_t marker) {
  rtc_rtp_packet_t packet;
  rtc_result_t vr;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }
  if (!payload || payload_len == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  if (peer->state != RTC_PEER_STATE_CONNECTED) {
    return RTC_ERR_INVALID_STATE;
  }

  vr = rtc_rtp_encode_video_h264(&peer->rtp, payload, payload_len, timestamp90k,
                                 marker, &packet);
  if (vr != RTC_OK) {
    return vr;
  }

  vr = rtc_srtp_protect(&peer->srtp, &packet);
  if (vr != RTC_OK) {
    peer->stats.protocol_error_count++;
    return vr;
  }

  vr = rtc_transport_enqueue_tx(&peer->transport, &packet);
  if (vr != RTC_OK) {
    peer->stats.dropped_packets++;
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_RTP, vr, "tx queue full");
    rtc_update_queue_stats(peer);
    return vr;
  }

  peer->stats.tx_video_frames++;
  rtc_update_queue_stats(peer);
  return RTC_OK;
}

rtc_result_t rtc_session_peer_send_audio_g711(rtc_peer_t *peer,
                                               rtc_audio_codec_t codec,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint32_t timestamp8k) {
  rtc_rtp_packet_t packet;
  rtc_result_t vr;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }
  if (!payload || payload_len == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  if (peer->state != RTC_PEER_STATE_CONNECTED) {
    return RTC_ERR_INVALID_STATE;
  }

  vr = rtc_rtp_encode_audio_g711(&peer->rtp, codec, payload, payload_len,
                                 timestamp8k, &packet);
  if (vr != RTC_OK) {
    return vr;
  }

  vr = rtc_srtp_protect(&peer->srtp, &packet);
  if (vr != RTC_OK) {
    peer->stats.protocol_error_count++;
    return vr;
  }

  vr = rtc_transport_enqueue_tx(&peer->transport, &packet);
  if (vr != RTC_OK) {
    peer->stats.dropped_packets++;
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_RTP, vr, "tx queue full");
    rtc_update_queue_stats(peer);
    return vr;
  }

  peer->stats.tx_audio_frames++;
  rtc_update_queue_stats(peer);
  return RTC_OK;
}

rtc_result_t rtc_session_peer_datachannel_open(rtc_peer_t *peer, const char *label,
                                                uint16_t *out_stream_id) {
  (void)label;
  (void)out_stream_id;
  if (!peer) {
    return RTC_ERR_INVALID_ARG;
  }
  rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_PEER, RTC_ERR_NOT_SUPPORTED,
               "datachannel not supported in v1");
  return RTC_ERR_NOT_SUPPORTED;
}

rtc_result_t rtc_session_peer_datachannel_send(rtc_peer_t *peer,
                                                uint16_t stream_id,
                                                const uint8_t *payload,
                                                uint16_t payload_len) {
  (void)stream_id;
  (void)payload;
  (void)payload_len;
  if (!peer) {
    return RTC_ERR_INVALID_ARG;
  }
  rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_PEER, RTC_ERR_NOT_SUPPORTED,
               "datachannel not supported in v1");
  return RTC_ERR_NOT_SUPPORTED;
}

rtc_result_t rtc_session_peer_datachannel_close(rtc_peer_t *peer,
                                                 uint16_t stream_id) {
  (void)stream_id;
  if (!peer) {
    return RTC_ERR_INVALID_ARG;
  }
  rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_PEER, RTC_ERR_NOT_SUPPORTED,
               "datachannel not supported in v1");
  return RTC_ERR_NOT_SUPPORTED;
}
