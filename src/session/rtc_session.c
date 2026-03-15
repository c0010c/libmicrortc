#include "session/rtc_session.h"

#include <stdio.h>
#include <string.h>

#define RTC_MODULE_ENGINE "session.engine"
#define RTC_MODULE_PEER "session.peer"
#define RTC_MODULE_ICE "ice"
#define RTC_MODULE_DTLS "dtls"
#define RTC_MODULE_SRTP "srtp"
#define RTC_MODULE_RTP "rtp"
#define RTC_MODULE_TRANSPORT "transport"

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

static void rtc_update_ice_stats(rtc_peer_t *peer) {
  if (!peer) {
    return;
  }
  peer->stats.local_candidate_count = peer->ice.local_candidate_count;
  peer->stats.remote_candidate_count = peer->ice.remote_candidate_count;
  peer->stats.ice_checks_sent = peer->ice.checks_sent;
  peer->stats.ice_checks_ok = peer->ice.checks_ok;
  peer->stats.ice_checks_failed = peer->ice.checks_failed;
  peer->stats.ice_checks_drop = peer->ice.checks_drop;
}

static void rtc_update_security_stats(rtc_peer_t *peer, uint32_t now_ms) {
  if (!peer) {
    return;
  }
  peer->stats.dtls_state = (uint8_t)peer->dtls.state;
  peer->stats.srtp_active =
      (uint8_t)(peer->srtp.state == RTC_SRTP_STATE_ACTIVE ? 1u : 0u);
  peer->stats.dtls_handshake_elapsed_ms =
      rtc_dtls_get_handshake_elapsed_ms(&peer->dtls, now_ms);
  if (rtc_dtls_get_last_error(&peer->dtls) != RTC_OK) {
    peer->stats.dtls_last_error = (int16_t)rtc_dtls_get_last_error(&peer->dtls);
  }
}

static char rtc_ascii_tolower(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (char)(ch - 'A' + 'a');
  }
  return ch;
}

static int rtc_ascii_case_eq(const char *lhs, const char *rhs) {
  size_t i = 0u;
  if (!lhs || !rhs) {
    return 0;
  }
  while (lhs[i] != '\0' && rhs[i] != '\0') {
    if (rtc_ascii_tolower(lhs[i]) != rtc_ascii_tolower(rhs[i])) {
      return 0;
    }
    i++;
  }
  return lhs[i] == '\0' && rhs[i] == '\0';
}

static int rtc_parse_candidate_host_ipv4(const char *candidate, uint8_t out_ip[4],
                                         uint16_t *out_port) {
  const char *p;
  char foundation[64];
  char transport[8];
  char ip[64];
  char typ_key[8];
  char candidate_type[16];
  unsigned component = 0u;
  unsigned priority = 0u;
  unsigned port = 0u;
  int parsed = 0;

  if (!candidate || !out_ip || !out_port) {
    return 0;
  }

  p = candidate;
  if (p[0] == 'a' && p[1] == '=') {
    p += 2;
  }
  if (strncmp(p, "candidate:", 10u) != 0) {
    return 0;
  }
  p += 10;

  parsed = sscanf(p, "%63s %u %7s %u %63s %u %7s %15s", foundation, &component,
                  transport, &priority, ip, &port, typ_key, candidate_type);
  if (parsed != 8) {
    return 0;
  }
  (void)foundation;
  (void)component;
  (void)priority;
  if (!rtc_ascii_case_eq(transport, "udp")) {
    return 0;
  }
  if (!rtc_ascii_case_eq(typ_key, "typ")) {
    return 0;
  }
  if (!rtc_ascii_case_eq(candidate_type, "host")) {
    return 0;
  }
  if (port == 0u || port > 65535u) {
    return 0;
  }
  if (!rtc_platform_parse_ipv4(ip, out_ip)) {
    return 0;
  }

  *out_port = (uint16_t)port;
  return 1;
}

static int rtc_peer_addr_is_known_remote_candidate(const rtc_peer_t *peer,
                                                    const uint8_t ip[4],
                                                    uint16_t port) {
  uint16_t i;
  if (!peer || !ip || port == 0u) {
    return 0;
  }
  for (i = 0u; i < peer->ice.remote_candidate_count; ++i) {
    const rtc_ice_remote_candidate_t *cand = &peer->ice.remote_candidate_items[i];
    if (!cand->in_use || cand->port != port) {
      continue;
    }
    if (memcmp(cand->ip, ip, 4u) == 0) {
      return 1;
    }
  }
  return 0;
}

static int rtc_is_resource_error(rtc_result_t r) {
  return r == RTC_ERR_RESOURCE_EXHAUSTED || r == RTC_ERR_OVERFLOW;
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
  uint16_t sent = 0;
  uint16_t received = 0;
  uint32_t dropped = 0;
  uint32_t rx_drop_before;
  uint32_t rx_drop_after;
  uint32_t rx_dropped = 0u;
  uint32_t tx_dropped = 0u;
  rtc_result_t io_result;

  rx_drop_before = rtc_transport_rx_drop_count(&peer->transport);
  io_result = rtc_transport_pump_io(&peer->transport, max_packets, &sent, &received,
                                    &dropped);
  rx_drop_after = rtc_transport_rx_drop_count(&peer->transport);
  if (rx_drop_after >= rx_drop_before) {
    rx_dropped = rx_drop_after - rx_drop_before;
  } else {
    rx_dropped = rx_drop_after;
  }
  if (dropped > rx_dropped) {
    tx_dropped = dropped - rx_dropped;
  }
  (void)sent;
  (void)received;
  if (io_result != RTC_OK) {
    peer->stats.protocol_error_count++;
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, io_result,
                 "pump io reported transport error");
  }
  if (dropped > 0u) {
    peer->stats.dropped_packets += dropped;
  }
  if (rx_dropped > 0u) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_RTP, RTC_ERR_OVERFLOW,
                 "rx queue full, drop packet");
  }
  if (tx_dropped > 0u) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, RTC_ERR_INVALID_STATE,
                 "tx drop on transport io/state");
  }

  for (i = 0; i < max_packets; ++i) {
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

static int rtc_peer_drive_ice_transport_io(rtc_peer_t *peer, uint32_t now_ms,
                                           rtc_result_t *out_fatal_error) {
  uint16_t sent = 0u;
  uint16_t received = 0u;
  uint32_t dropped = 0u;
  rtc_result_t io_result;

  if (!peer || !peer->in_use || !out_fatal_error) {
    return 0;
  }
  *out_fatal_error = RTC_OK;

  io_result = rtc_transport_pump_io(&peer->transport, 4u, &sent, &received, &dropped);
  (void)sent;
  (void)received;
  if (io_result != RTC_OK) {
    peer->stats.protocol_error_count++;
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, io_result,
                 "pump io reported transport error");
  }
  if (dropped > 0u) {
    peer->stats.dropped_packets += dropped;
  }

  for (;;) {
    rtc_transport_stun_packet_t stun_pkt;
    rtc_result_t r = rtc_transport_dequeue_stun(&peer->transport, &stun_pkt);
    if (r == RTC_ERR_TIMEOUT) {
      break;
    }
    if (r != RTC_OK) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, r,
                   "dequeue stun packet failed");
      break;
    }
    if (stun_pkt.src_addr.family != RTC_PLATFORM_IP_FAMILY_IPV4) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, RTC_ERR_NOT_SUPPORTED,
                   "stun source family not supported");
      continue;
    }

    r = rtc_ice_handle_incoming_stun(&peer->ice, stun_pkt.src_addr.addr,
                                     stun_pkt.src_addr.port, stun_pkt.data,
                                     stun_pkt.len, now_ms);
    if (r != RTC_OK) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, r,
                   "stun packet validation/handling failed");
      if (rtc_is_resource_error(r)) {
        *out_fatal_error = RTC_ERR_RESOURCE_EXHAUSTED;
        return 1;
      }
      if ((r == RTC_ERR_PROTOCOL || r == RTC_ERR_AUTH_FAILED) &&
          rtc_peer_addr_is_known_remote_candidate(
              peer, stun_pkt.src_addr.addr, stun_pkt.src_addr.port)) {
        *out_fatal_error = RTC_ERR_PROTOCOL;
        return 1;
      }
      continue;
    }
    rtc_log_peer(peer, RTC_LOG_DEBUG, RTC_MODULE_ICE, RTC_OK,
                 "stun packet handled");
  }

  for (;;) {
    uint8_t ip[4];
    uint16_t port = 0u;
    uint8_t buf[RTC_CFG_MTU];
    uint16_t len = (uint16_t)sizeof(buf);
    uint16_t sent_len = 0u;
    rtc_platform_net_addr_t dst;
    rtc_result_t r;

    r = rtc_ice_dequeue_outgoing_stun(&peer->ice, ip, &port, buf, &len);
    if (r == RTC_ERR_TIMEOUT) {
      break;
    }
    if (r != RTC_OK) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, r,
                   "dequeue stun tx failed");
      if (rtc_is_resource_error(r)) {
        *out_fatal_error = RTC_ERR_RESOURCE_EXHAUSTED;
      } else {
        *out_fatal_error = RTC_ERR_PROTOCOL;
      }
      return 1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.family = RTC_PLATFORM_IP_FAMILY_IPV4;
    dst.port = port;
    memcpy(dst.addr, ip, 4u);

    r = rtc_transport_send_stun(&peer->transport, &dst, buf, len, &sent_len);
    if (r == RTC_ERR_TIMEOUT) {
      rtc_log_peer(peer, RTC_LOG_DEBUG, RTC_MODULE_ICE, RTC_ERR_TIMEOUT,
                   "stun send would block");
      continue;
    }
    if (r != RTC_OK || sent_len != len) {
      peer->stats.protocol_error_count++;
      rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, r,
                   "stun send failed");
      if (r == RTC_OK) {
        *out_fatal_error = RTC_ERR_PROTOCOL;
      } else if (rtc_is_resource_error(r)) {
        *out_fatal_error = RTC_ERR_RESOURCE_EXHAUSTED;
      } else {
        *out_fatal_error = RTC_ERR_PROTOCOL;
      }
      return 1;
    }
    rtc_log_peer(peer, RTC_LOG_DEBUG, RTC_MODULE_ICE, RTC_OK,
                 "stun check sent");
  }

  rtc_update_queue_stats(peer);
  return 0;
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
    rtc_result_t fatal_ice_error = RTC_OK;
    if (rtc_peer_drive_ice_transport_io(peer, now_ms, &fatal_ice_error)) {
      rtc_update_ice_stats(peer);
      peer->stats.ice_last_error = (int16_t)fatal_ice_error;
      peer->stats.protocol_error_count++;
      if (fatal_ice_error == RTC_ERR_RESOURCE_EXHAUSTED) {
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, RTC_ERR_RESOURCE_EXHAUSTED,
                           "ice resource exhausted", RTC_LOG_ERROR);
      } else {
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, RTC_ERR_PROTOCOL,
                           "ice protocol failure", RTC_LOG_ERROR);
      }
      return;
    }

    rtc_ice_tick(&peer->ice, now_ms, peer->config.retry_interval_ms,
                 peer->config.max_retries, &ice_event);
    rtc_update_ice_stats(peer);

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
      uint8_t selected_ip[4];
      uint16_t selected_port = 0u;
      rtc_result_t sr =
          rtc_ice_get_selected_remote(&peer->ice, selected_ip, &selected_port);
      if (sr == RTC_OK) {
        sr = rtc_transport_set_remote_ipv4(&peer->transport, selected_ip[0],
                                           selected_ip[1], selected_ip[2],
                                           selected_ip[3], selected_port);
        if (sr == RTC_OK) {
          rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_TRANSPORT, RTC_OK,
                       "transport remote selected from ice");
        } else {
          rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, sr,
                       "transport remote select from ice failed");
        }
      }
      if (rtc_dtls_start(&peer->dtls, now_ms) != RTC_OK) {
        peer->stats.dtls_last_error = RTC_ERR_DTLS_HANDSHAKE_FAILED;
        peer->stats.protocol_error_count++;
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED,
                           RTC_ERR_DTLS_HANDSHAKE_FAILED,
                           "dtls start failed", RTC_LOG_ERROR);
        return;
      }
      rtc_peer_set_state(peer, RTC_PEER_STATE_DTLS_HANDSHAKE, RTC_OK,
                         "dtls handshake", RTC_LOG_INFO);
      return;
    }

    if (ice_event.failed) {
      peer->stats.timeout_count++;
      peer->stats.ice_last_error = (int16_t)ice_event.error;
      rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, ice_event.error,
                         "ice timeout", RTC_LOG_WARN);
      return;
    }

    return;
  }

  if (peer->state == RTC_PEER_STATE_DTLS_HANDSHAKE) {
    rtc_dtls_event_t dtls_event;
    rtc_dtls_tick(&peer->dtls, now_ms, &dtls_event);
    rtc_update_security_stats(peer, now_ms);

    if (dtls_event.connected) {
      rtc_srtp_key_material_t keys;
      const rtc_dtls_key_material_t *km = rtc_dtls_get_key_material(&peer->dtls);
      rtc_result_t r;

      if (!km || km->key_len != 30u) {
        peer->stats.dtls_last_error = RTC_ERR_DTLS_HANDSHAKE_FAILED;
        peer->stats.protocol_error_count++;
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED,
                           RTC_ERR_DTLS_HANDSHAKE_FAILED, "dtls keying failed",
                           RTC_LOG_ERROR);
        return;
      }

      memset(&keys, 0, sizeof(keys));
      keys.key_len = km->key_len;
      keys.profile = (uint8_t)km->profile;
      memcpy(keys.outbound_key, km->client_write_key, keys.key_len);
      memcpy(keys.inbound_key, km->client_write_key, keys.key_len);

      r = rtc_srtp_activate(&peer->srtp, &keys);
      if (r != RTC_OK) {
        peer->stats.dtls_last_error = RTC_ERR_SRTP_ACTIVATE_FAILED;
        peer->stats.protocol_error_count++;
        rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED,
                           RTC_ERR_SRTP_ACTIVATE_FAILED,
                           "srtp activate failed", RTC_LOG_ERROR);
        return;
      }
      rtc_update_security_stats(peer, now_ms);
      rtc_peer_set_state(peer, RTC_PEER_STATE_CONNECTED, RTC_OK,
                         "media connected", RTC_LOG_INFO);
      return;
    }

    if (dtls_event.failed) {
      peer->stats.dtls_last_error = dtls_event.error;
      peer->stats.protocol_error_count++;
      rtc_peer_set_state(peer, RTC_PEER_STATE_FAILED, dtls_event.error,
                         "dtls failed", RTC_LOG_ERROR);
      return;
    }
    return;
  }

  if (peer->state == RTC_PEER_STATE_CONNECTED) {
    rtc_peer_dispatch_incoming(peer, 4u);
    rtc_update_ice_stats(peer);
    rtc_update_security_stats(peer, now_ms);
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
  engine->dtls_cert_mode = config->dtls_cert_mode;
  engine->dtls_debug_enabled = (uint8_t)(config->dtls_debug_enabled ? 1u : 0u);
  engine->dtls_backend_log_level = config->dtls_backend_log_level;
  if (engine->dtls_cert_mode > RTC_DTLS_CERT_MODE_EPHEMERAL) {
    engine->dtls_cert_mode = RTC_DTLS_CERT_MODE_STATIC;
  }
  if (engine->dtls_backend_log_level > RTC_LOG_DEBUG) {
    engine->dtls_backend_log_level = RTC_LOG_WARN;
  }

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
  uint8_t local_ip[4];
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
  if (peer->config.dtls_handshake_timeout_ms == 0u) {
    peer->config.dtls_handshake_timeout_ms = RTC_CFG_DTLS_HANDSHAKE_TIMEOUT_MS;
  }
  if (peer->config.dtls_handshake_max_retries == 0u) {
    peer->config.dtls_handshake_max_retries = 16u;
  }

  rtc_ice_init(&peer->ice);
  rtc_ice_set_peer_id(&peer->ice, peer->peer_id);
  rtc_dtls_init(&peer->dtls);
  rtc_dtls_configure(&peer->dtls, peer->peer_id,
                     peer->config.dtls_handshake_timeout_ms,
                     peer->config.dtls_handshake_max_retries,
                     peer->engine->dtls_debug_enabled);
  rtc_srtp_init(&peer->srtp);
  rtc_rtp_init(&peer->rtp, peer->peer_id);
  rtc_transport_init(&peer->transport);
  if (rtc_transport_local_port(&peer->transport) == 0u) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_TRANSPORT,
                 RTC_ERR_RESOURCE_EXHAUSTED, "transport init failed");
    memset(peer, 0, sizeof(*peer));
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }
  vr = rtc_ice_set_local_fingerprint(&peer->ice, rtc_dtls_get_local_fingerprint_sha256());
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_DTLS, vr,
                 "local fingerprint setup failed");
    rtc_transport_deinit(&peer->transport);
    memset(peer, 0, sizeof(*peer));
    return vr;
  }
  vr = rtc_platform_get_default_ipv4(local_ip);
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_TRANSPORT, vr,
                 "local ipv4 discovery failed");
    rtc_transport_deinit(&peer->transport);
    memset(peer, 0, sizeof(*peer));
    return vr;
  }
  vr = rtc_ice_set_local_host(&peer->ice, local_ip,
                              rtc_transport_local_port(&peer->transport));
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                 "local host setup failed");
    rtc_transport_deinit(&peer->transport);
    memset(peer, 0, sizeof(*peer));
    return vr;
  }

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
  rtc_transport_deinit(&peer->transport);
  rtc_srtp_deinit(&peer->srtp);
  rtc_dtls_deinit(&peer->dtls);
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
  rtc_dtls_configure(&peer->dtls, peer->peer_id,
                     peer->config.dtls_handshake_timeout_ms,
                     peer->config.dtls_handshake_max_retries,
                     peer->engine->dtls_debug_enabled);
  rtc_srtp_deinit(&peer->srtp);
  rtc_srtp_init(&peer->srtp);
  peer->stats.dtls_last_error = RTC_OK;
  peer->stats.ice_last_error = RTC_OK;
  peer->stats.ice_checks_sent = 0u;
  peer->stats.ice_checks_ok = 0u;
  peer->stats.ice_checks_failed = 0u;
  peer->stats.ice_checks_drop = 0u;

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

  rtc_srtp_deinit(&peer->srtp);
  rtc_dtls_deinit(&peer->dtls);
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
  uint8_t applied_transport_remote = 0u;
  uint16_t i;
  uint8_t ip[4];
  uint16_t port = 0u;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }

  vr = rtc_ice_set_remote_description(&peer->ice, sdp, type);
  if (vr != RTC_OK) {
    if (vr == RTC_ERR_PROTOCOL) {
      rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                   "missing required attr or malformed offer");
    } else if (vr == RTC_ERR_NOT_SUPPORTED) {
      rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                   "unsupported offer codec/setup/fingerprint");
    }
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                 "set remote description failed");
    return vr;
  }

  for (i = 0u; i < peer->ice.remote_candidate_count; ++i) {
    if (!rtc_parse_candidate_host_ipv4(peer->ice.remote_candidates[i], ip, &port)) {
      continue;
    }
    vr = rtc_transport_set_remote_ipv4(&peer->transport, ip[0], ip[1], ip[2], ip[3], port);
    if (vr != RTC_OK) {
      rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, vr,
                   "set transport remote failed");
      return vr;
    }
    rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_TRANSPORT, RTC_OK,
                 "transport remote updated");
    applied_transport_remote = 1u;
    break;
  }
  if (!applied_transport_remote) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, RTC_OK,
                 "offer has no candidate");
  }
  if (peer->ice.audio_offer_present && !peer->ice.audio_accepted) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, RTC_ERR_NOT_SUPPORTED,
                 "unsupported codec m-line rejected");
  }
  if (peer->ice.video_offer_present && !peer->ice.video_accepted) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_ICE, RTC_ERR_NOT_SUPPORTED,
                 "unsupported codec m-line rejected");
  }

  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
               "remote offer parsed");
  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
               "answer generated");
  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_ICE, RTC_OK,
               "remote description set");
  return RTC_OK;
}

rtc_result_t rtc_session_peer_add_remote_candidate(rtc_peer_t *peer,
                                                    const char *candidate) {
  rtc_result_t vr;
  uint8_t ip[4];
  uint16_t port = 0u;

  vr = rtc_validate_live_peer(peer, NULL, NULL);
  if (vr != RTC_OK) {
    return vr;
  }
  if (!candidate) {
    return RTC_ERR_INVALID_ARG;
  }

  if (!rtc_parse_candidate_host_ipv4(candidate, ip, &port)) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, RTC_ERR_NOT_SUPPORTED,
                 "remote candidate not supported by transport");
    return RTC_ERR_NOT_SUPPORTED;
  }

  vr = rtc_ice_add_remote_candidate(&peer->ice, candidate);
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_ERROR, RTC_MODULE_ICE, vr,
                 "add remote candidate failed");
    return vr;
  }
  vr = rtc_transport_set_remote_ipv4(&peer->transport, ip[0], ip[1], ip[2], ip[3], port);
  if (vr != RTC_OK) {
    rtc_log_peer(peer, RTC_LOG_WARN, RTC_MODULE_TRANSPORT, vr,
                 "set transport remote failed");
    return vr;
  }
  rtc_log_peer(peer, RTC_LOG_INFO, RTC_MODULE_TRANSPORT, RTC_OK,
               "transport remote updated");

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
