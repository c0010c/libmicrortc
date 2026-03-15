#include "ice/rtc_ice.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/md.h>

#include "platform/rtc_platform.h"

#define RTC_SDP_MAX_LINE 384
#define RTC_MEDIA_NONE 0u
#define RTC_MEDIA_AUDIO 1u
#define RTC_MEDIA_VIDEO 2u
#define RTC_MEDIA_OTHER 3u

#define RTC_STUN_TYPE_BINDING_REQUEST 0x0001u
#define RTC_STUN_TYPE_BINDING_RESPONSE 0x0101u
#define RTC_STUN_MAGIC_COOKIE 0x2112A442u
#define RTC_STUN_ATTR_USERNAME 0x0006u
#define RTC_STUN_ATTR_MESSAGE_INTEGRITY 0x0008u
#define RTC_STUN_ATTR_PRIORITY 0x0024u
#define RTC_STUN_ATTR_USE_CANDIDATE 0x0025u
#define RTC_STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020u
#define RTC_STUN_ATTR_ICE_CONTROLLED 0x8029u
#define RTC_STUN_ATTR_FINGERPRINT 0x8028u
#define RTC_STUN_ATTR_MESSAGE_INTEGRITY_LEN 20u
#define RTC_STUN_LOCAL_HOST_PRIORITY 2130706431u

typedef struct rtc_h264_entry {
  int16_t pt;
  uint8_t has_fmtp;
  uint8_t packetization_mode_1;
  char fmtp[128];
} rtc_h264_entry_t;

typedef struct rtc_stun_attrs {
  const uint8_t *username;
  uint16_t username_len;
  const uint8_t *message_integrity;
  uint16_t message_integrity_len;
  const uint8_t *fingerprint;
  uint16_t fingerprint_len;
  uint16_t message_integrity_offset;
  uint16_t fingerprint_offset;
  uint32_t priority;
  uint8_t has_priority;
} rtc_stun_attrs_t;

static char rtc_ascii_tolower(char ch);
static int rtc_ascii_case_eq(const char *lhs, const char *rhs);
static void rtc_ice_reset_connectivity_state(rtc_ice_ctx_t *ctx);

static void rtc_ice_write_u16_be(uint8_t *dst, uint16_t value) {
  if (!dst) {
    return;
  }
  dst[0] = (uint8_t)((value >> 8) & 0xFFu);
  dst[1] = (uint8_t)(value & 0xFFu);
}

static void rtc_ice_write_u32_be(uint8_t *dst, uint32_t value) {
  if (!dst) {
    return;
  }
  dst[0] = (uint8_t)((value >> 24) & 0xFFu);
  dst[1] = (uint8_t)((value >> 16) & 0xFFu);
  dst[2] = (uint8_t)((value >> 8) & 0xFFu);
  dst[3] = (uint8_t)(value & 0xFFu);
}

static uint16_t rtc_ice_read_u16_be(const uint8_t *src) {
  if (!src) {
    return 0u;
  }
  return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

static uint32_t rtc_ice_read_u32_be(const uint8_t *src) {
  if (!src) {
    return 0u;
  }
  return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) | (uint32_t)src[3];
}

static uint32_t rtc_ice_crc32_ieee(const uint8_t *buf, uint16_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  uint16_t i;
  uint8_t j;

  if (!buf) {
    return 0u;
  }

  for (i = 0u; i < len; ++i) {
    crc ^= (uint32_t)buf[i];
    for (j = 0u; j < 8u; ++j) {
      if ((crc & 1u) != 0u) {
        crc = (crc >> 1) ^ 0xEDB88320u;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

static int rtc_ice_hmac_sha1(const uint8_t *key, uint16_t key_len, const uint8_t *data,
                             uint16_t data_len, uint8_t out[20]) {
  const mbedtls_md_info_t *md_info = NULL;
  mbedtls_md_context_t md_ctx;
  int rc;

  if (!key || key_len == 0u || !data || !out) {
    return 0;
  }

  md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (!md_info) {
    return 0;
  }

  mbedtls_md_init(&md_ctx);
  rc = mbedtls_md_setup(&md_ctx, md_info, 1);
  if (rc != 0) {
    mbedtls_md_free(&md_ctx);
    return 0;
  }

  rc = mbedtls_md_hmac_starts(&md_ctx, key, (size_t)key_len);
  if (rc == 0) {
    rc = mbedtls_md_hmac_update(&md_ctx, data, (size_t)data_len);
  }
  if (rc == 0) {
    rc = mbedtls_md_hmac_finish(&md_ctx, out);
  }

  mbedtls_md_free(&md_ctx);
  return rc == 0;
}

static int rtc_ice_stun_append_attr(uint8_t *buf, uint16_t cap, uint16_t *io_offset,
                                    uint16_t attr_type, const uint8_t *value,
                                    uint16_t value_len) {
  uint16_t offset;
  uint16_t padded_len;
  uint16_t i;

  if (!buf || !io_offset) {
    return 0;
  }
  offset = *io_offset;
  padded_len = (uint16_t)((value_len + 3u) & 0xFFFCu);

  if ((uint32_t)offset + 4u + padded_len > (uint32_t)cap) {
    return 0;
  }

  rtc_ice_write_u16_be(&buf[offset], attr_type);
  rtc_ice_write_u16_be(&buf[offset + 2u], value_len);
  offset += 4u;

  if (value_len > 0u && value) {
    memcpy(&buf[offset], value, value_len);
  }
  if (padded_len > value_len) {
    for (i = value_len; i < padded_len; ++i) {
      buf[offset + i] = 0u;
    }
  }
  offset = (uint16_t)(offset + padded_len);
  *io_offset = offset;
  return 1;
}

static void rtc_ice_generate_transaction_id(rtc_ice_ctx_t *ctx, uint8_t pair_index,
                                            uint32_t now_ms, uint16_t retry_count,
                                            uint8_t out_tid[12]) {
  uint32_t seed;
  uint8_t i;

  if (!ctx || !out_tid) {
    return;
  }
  seed = now_ms ^ (ctx->peer_id << 16) ^ ((uint32_t)pair_index << 8) ^ (uint32_t)retry_count;
  for (i = 0u; i < 12u; ++i) {
    seed = seed * 1103515245u + 12345u;
    out_tid[i] = (uint8_t)((seed >> ((i & 3u) * 8u)) & 0xFFu);
  }
}

static int rtc_ice_parse_candidate_ipv4(const char *candidate, uint8_t out_ip[4],
                                        uint16_t *out_port, uint32_t *out_priority) {
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

  if (!candidate || !out_ip || !out_port || !out_priority) {
    return 0;
  }
  p = candidate;
  if (p[0] == 'a' && p[1] == '=') {
    p += 2;
  }
  if (strncmp(p, "candidate:", 10u) != 0) {
    return 0;
  }
  p += 10u;

  parsed = sscanf(p, "%63s %u %7s %u %63s %u %7s %15s", foundation, &component,
                  transport, &priority, ip, &port, typ_key, candidate_type);
  if (parsed != 8) {
    return 0;
  }
  (void)foundation;
  (void)component;
  if (!rtc_ascii_case_eq(transport, "udp")) {
    return 0;
  }
  if (!rtc_ascii_case_eq(typ_key, "typ")) {
    return 0;
  }
  if (!rtc_ascii_case_eq(candidate_type, "host") &&
      !rtc_ascii_case_eq(candidate_type, "prflx")) {
    return 0;
  }
  if (port == 0u || port > 65535u) {
    return 0;
  }
  if (!rtc_platform_parse_ipv4(ip, out_ip)) {
    return 0;
  }

  *out_port = (uint16_t)port;
  *out_priority = priority;
  return 1;
}

static int rtc_ice_find_remote_candidate_by_addr(const rtc_ice_ctx_t *ctx,
                                                 const uint8_t ip[4],
                                                 uint16_t port) {
  uint16_t i;
  if (!ctx || !ip || port == 0u) {
    return -1;
  }
  for (i = 0u; i < ctx->remote_candidate_count; ++i) {
    const rtc_ice_remote_candidate_t *cand = &ctx->remote_candidate_items[i];
    if (cand->in_use && cand->port == port && memcmp(cand->ip, ip, 4u) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int rtc_ice_find_pair_for_remote(const rtc_ice_ctx_t *ctx, uint8_t remote_index) {
  uint16_t i;
  if (!ctx) {
    return -1;
  }
  for (i = 0u; i < ctx->pair_count; ++i) {
    if (ctx->pairs[i].in_use && ctx->pairs[i].remote_index == remote_index) {
      return (int)i;
    }
  }
  return -1;
}

static int rtc_ice_find_pair_by_transaction(const rtc_ice_ctx_t *ctx,
                                            const uint8_t transaction_id[12]) {
  uint16_t i;
  if (!ctx || !transaction_id) {
    return -1;
  }
  for (i = 0u; i < ctx->pair_count; ++i) {
    const rtc_ice_candidate_pair_t *pair = &ctx->pairs[i];
    if (!pair->in_use || !pair->transaction_valid) {
      continue;
    }
    if (memcmp(pair->transaction_id, transaction_id, 12u) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int rtc_ice_queue_stun_out(rtc_ice_ctx_t *ctx, const uint8_t ip[4], uint16_t port,
                                  const uint8_t *buf, uint16_t len) {
  rtc_ice_stun_out_t *slot;

  if (!ctx || !ip || !buf || len == 0u || len > RTC_CFG_MTU || port == 0u) {
    return 0;
  }
  if (ctx->stun_out_size >= RTC_CFG_RTCP_FB_QUEUE) {
    ctx->checks_drop++;
    return 0;
  }

  slot = &ctx->stun_out_queue[ctx->stun_out_tail];
  memset(slot, 0, sizeof(*slot));
  memcpy(slot->ip, ip, 4u);
  slot->port = port;
  slot->len = len;
  memcpy(slot->data, buf, len);

  ctx->stun_out_tail = (uint16_t)((ctx->stun_out_tail + 1u) % RTC_CFG_RTCP_FB_QUEUE);
  ctx->stun_out_size++;
  return 1;
}

static int rtc_ice_parse_stun_attrs(const uint8_t *buf, uint16_t len,
                                    rtc_stun_attrs_t *out_attrs) {
  uint16_t msg_len;
  uint16_t pos;
  uint16_t end;

  if (!buf || len < 20u || !out_attrs) {
    return 0;
  }
  memset(out_attrs, 0, sizeof(*out_attrs));

  msg_len = rtc_ice_read_u16_be(&buf[2]);
  if ((msg_len & 0x0003u) != 0u) {
    return 0;
  }
  if ((uint32_t)msg_len + 20u > (uint32_t)len) {
    return 0;
  }
  end = (uint16_t)(20u + msg_len);

  pos = 20u;
  while ((uint32_t)pos + 4u <= (uint32_t)end) {
    uint16_t attr_type = rtc_ice_read_u16_be(&buf[pos]);
    uint16_t attr_len = rtc_ice_read_u16_be(&buf[pos + 2u]);
    uint16_t padded_len = (uint16_t)((attr_len + 3u) & 0xFFFCu);
    const uint8_t *value = &buf[pos + 4u];
    if ((uint32_t)pos + 4u + padded_len > (uint32_t)end) {
      return 0;
    }

    if (attr_type == RTC_STUN_ATTR_USERNAME) {
      out_attrs->username = value;
      out_attrs->username_len = attr_len;
    } else if (attr_type == RTC_STUN_ATTR_PRIORITY && attr_len == 4u) {
      out_attrs->priority = rtc_ice_read_u32_be(value);
      out_attrs->has_priority = 1u;
    } else if (attr_type == RTC_STUN_ATTR_MESSAGE_INTEGRITY &&
               attr_len == RTC_STUN_ATTR_MESSAGE_INTEGRITY_LEN) {
      out_attrs->message_integrity = value;
      out_attrs->message_integrity_len = attr_len;
      out_attrs->message_integrity_offset = pos;
    } else if (attr_type == RTC_STUN_ATTR_FINGERPRINT && attr_len == 4u) {
      out_attrs->fingerprint = value;
      out_attrs->fingerprint_len = attr_len;
      out_attrs->fingerprint_offset = pos;
    }

    pos = (uint16_t)(pos + 4u + padded_len);
  }

  return pos == end;
}

static int rtc_ice_stun_verify_fingerprint(const uint8_t *buf, uint16_t len,
                                           const rtc_stun_attrs_t *attrs) {
  uint32_t expected;
  uint32_t actual;

  if (!buf || !attrs || !attrs->fingerprint || attrs->fingerprint_len != 4u) {
    return 0;
  }
  if (attrs->fingerprint_offset < 20u || attrs->fingerprint_offset > len) {
    return 0;
  }

  expected = rtc_ice_crc32_ieee(buf, attrs->fingerprint_offset) ^ 0x5354554Eu;
  actual = rtc_ice_read_u32_be(attrs->fingerprint);
  return expected == actual;
}

static int rtc_ice_stun_verify_message_integrity(const uint8_t *buf, uint16_t len,
                                                 const rtc_stun_attrs_t *attrs,
                                                 const char *password) {
  uint8_t digest[20];
  uint8_t temp[RTC_CFG_MTU];
  uint16_t sign_len;
  uint16_t msg_len_for_hmac;

  if (!buf || !attrs || !password || password[0] == '\0') {
    return 0;
  }
  if (!attrs->message_integrity ||
      attrs->message_integrity_len != RTC_STUN_ATTR_MESSAGE_INTEGRITY_LEN) {
    return 0;
  }
  if (attrs->message_integrity_offset < 20u || attrs->message_integrity_offset > len) {
    return 0;
  }

  sign_len = attrs->message_integrity_offset;
  if (sign_len > len || len > RTC_CFG_MTU) {
    return 0;
  }
  memcpy(temp, buf, sign_len);

  msg_len_for_hmac = (uint16_t)((attrs->message_integrity_offset - 20u) + 24u);
  rtc_ice_write_u16_be(&temp[2], msg_len_for_hmac);
  if (!rtc_ice_hmac_sha1((const uint8_t *)password, (uint16_t)strlen(password), temp,
                         sign_len, digest)) {
    return 0;
  }
  return memcmp(digest, attrs->message_integrity,
                RTC_STUN_ATTR_MESSAGE_INTEGRITY_LEN) == 0;
}

static int rtc_ice_stun_username_equals(const rtc_stun_attrs_t *attrs,
                                        const char *expected) {
  size_t expected_len;
  if (!attrs || !attrs->username || !expected) {
    return 0;
  }
  expected_len = strlen(expected);
  if (expected_len == 0u || expected_len != attrs->username_len) {
    return 0;
  }
  return memcmp(attrs->username, expected, expected_len) == 0;
}

static int rtc_ice_build_stun_response(rtc_ice_ctx_t *ctx, const uint8_t transaction_id[12],
                                       const char *username, const uint8_t src_ip[4],
                                       uint16_t src_port, uint8_t *out_buf,
                                       uint16_t *out_len) {
  uint8_t xor_addr[8];
  uint8_t tie[8];
  uint16_t offset = 20u;
  uint16_t msg_len;
  uint16_t pre_fp_len;
  uint32_t fp;
  uint8_t hmac[20];

  if (!ctx || !transaction_id || !username || !src_ip || src_port == 0u ||
      !out_buf || !out_len) {
    return 0;
  }
  if (strlen(username) >= 256u) {
    return 0;
  }
  memset(out_buf, 0, RTC_CFG_MTU);

  rtc_ice_write_u16_be(&out_buf[0], RTC_STUN_TYPE_BINDING_RESPONSE);
  rtc_ice_write_u16_be(&out_buf[2], 0u);
  rtc_ice_write_u32_be(&out_buf[4], RTC_STUN_MAGIC_COOKIE);
  memcpy(&out_buf[8], transaction_id, 12u);

  memset(xor_addr, 0, sizeof(xor_addr));
  xor_addr[1] = 0x01u;
  xor_addr[2] = (uint8_t)(((src_port >> 8) & 0xFFu) ^ 0x21u);
  xor_addr[3] = (uint8_t)((src_port & 0xFFu) ^ 0x12u);
  xor_addr[4] = (uint8_t)(src_ip[0] ^ 0x21u);
  xor_addr[5] = (uint8_t)(src_ip[1] ^ 0x12u);
  xor_addr[6] = (uint8_t)(src_ip[2] ^ 0xA4u);
  xor_addr[7] = (uint8_t)(src_ip[3] ^ 0x42u);
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset,
                                RTC_STUN_ATTR_XOR_MAPPED_ADDRESS, xor_addr,
                                (uint16_t)sizeof(xor_addr))) {
    return 0;
  }
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset, RTC_STUN_ATTR_USERNAME,
                                (const uint8_t *)username,
                                (uint16_t)strlen(username))) {
    return 0;
  }
  memset(tie, 0, sizeof(tie));
  rtc_ice_write_u32_be(&tie[4], ctx->peer_id);
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset,
                                RTC_STUN_ATTR_ICE_CONTROLLED, tie,
                                (uint16_t)sizeof(tie))) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 24u);
  rtc_ice_write_u16_be(&out_buf[2], msg_len);
  if (!rtc_ice_hmac_sha1((const uint8_t *)ctx->local_ice_pwd,
                         (uint16_t)strlen(ctx->local_ice_pwd), out_buf, offset,
                         hmac)) {
    return 0;
  }
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset,
                                RTC_STUN_ATTR_MESSAGE_INTEGRITY, hmac,
                                RTC_STUN_ATTR_MESSAGE_INTEGRITY_LEN)) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 8u);
  rtc_ice_write_u16_be(&out_buf[2], msg_len);
  pre_fp_len = offset;
  fp = rtc_ice_crc32_ieee(out_buf, pre_fp_len) ^ 0x5354554Eu;
  rtc_ice_write_u32_be(&hmac[0], fp);
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset, RTC_STUN_ATTR_FINGERPRINT,
                                hmac, 4u)) {
    return 0;
  }

  rtc_ice_write_u16_be(&out_buf[2], (uint16_t)(offset - 20u));
  *out_len = offset;
  return 1;
}

static int rtc_ice_build_stun_request(rtc_ice_ctx_t *ctx, rtc_ice_candidate_pair_t *pair,
                                      uint8_t pair_index, uint32_t now_ms,
                                      uint8_t *out_buf, uint16_t *out_len) {
  char username[128];
  uint16_t offset = 20u;
  uint16_t msg_len;
  uint16_t pre_fp_len;
  uint8_t value4[4];
  uint8_t hmac[20];
  uint8_t tie[8];
  uint32_t fp;

  if (!ctx || !pair || !out_buf || !out_len) {
    return 0;
  }
  if (snprintf(username, sizeof(username), "%s:%s", ctx->remote_ice_ufrag,
               ctx->local_ice_ufrag) <= 0) {
    return 0;
  }

  rtc_ice_generate_transaction_id(ctx, pair_index, now_ms, pair->retry_count,
                                  pair->transaction_id);
  pair->transaction_valid = 1u;

  memset(out_buf, 0, RTC_CFG_MTU);
  rtc_ice_write_u16_be(&out_buf[0], RTC_STUN_TYPE_BINDING_REQUEST);
  rtc_ice_write_u16_be(&out_buf[2], 0u);
  rtc_ice_write_u32_be(&out_buf[4], RTC_STUN_MAGIC_COOKIE);
  memcpy(&out_buf[8], pair->transaction_id, 12u);

  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset, RTC_STUN_ATTR_USERNAME,
                                (const uint8_t *)username,
                                (uint16_t)strlen(username))) {
    return 0;
  }
  rtc_ice_write_u32_be(value4, RTC_STUN_LOCAL_HOST_PRIORITY);
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset, RTC_STUN_ATTR_PRIORITY,
                                value4, 4u)) {
    return 0;
  }
  memset(tie, 0, sizeof(tie));
  rtc_ice_write_u32_be(&tie[4], ctx->peer_id);
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset,
                                RTC_STUN_ATTR_ICE_CONTROLLED, tie,
                                (uint16_t)sizeof(tie))) {
    return 0;
  }
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset,
                                RTC_STUN_ATTR_USE_CANDIDATE, NULL, 0u)) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 24u);
  rtc_ice_write_u16_be(&out_buf[2], msg_len);
  if (!rtc_ice_hmac_sha1((const uint8_t *)ctx->remote_ice_pwd,
                         (uint16_t)strlen(ctx->remote_ice_pwd), out_buf, offset,
                         hmac)) {
    return 0;
  }
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset,
                                RTC_STUN_ATTR_MESSAGE_INTEGRITY, hmac,
                                RTC_STUN_ATTR_MESSAGE_INTEGRITY_LEN)) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 8u);
  rtc_ice_write_u16_be(&out_buf[2], msg_len);
  pre_fp_len = offset;
  fp = rtc_ice_crc32_ieee(out_buf, pre_fp_len) ^ 0x5354554Eu;
  rtc_ice_write_u32_be(&hmac[0], fp);
  if (!rtc_ice_stun_append_attr(out_buf, RTC_CFG_MTU, &offset, RTC_STUN_ATTR_FINGERPRINT,
                                hmac, 4u)) {
    return 0;
  }

  rtc_ice_write_u16_be(&out_buf[2], (uint16_t)(offset - 20u));
  *out_len = offset;
  return 1;
}

static void rtc_ice_reset_connectivity_state(rtc_ice_ctx_t *ctx) {
  uint16_t i;
  if (!ctx) {
    return;
  }
  for (i = 0u; i < ctx->pair_count; ++i) {
    if (!ctx->pairs[i].in_use) {
      continue;
    }
    ctx->pairs[i].state = RTC_ICE_PAIR_STATE_WAITING;
    ctx->pairs[i].retry_count = 0u;
    ctx->pairs[i].last_check_ms = 0u;
    ctx->pairs[i].transaction_valid = 0u;
    memset(ctx->pairs[i].transaction_id, 0, sizeof(ctx->pairs[i].transaction_id));
  }
  ctx->active_pair_index = -1;
  ctx->selected_pair_index = -1;
  ctx->retry_count = 0u;
  ctx->last_tick_ms = 0u;
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

static int rtc_ascii_case_has_prefix(const char *text, const char *prefix) {
  size_t i = 0u;
  if (!text || !prefix) {
    return 0;
  }
  while (prefix[i] != '\0') {
    if (text[i] == '\0') {
      return 0;
    }
    if (rtc_ascii_tolower(text[i]) != rtc_ascii_tolower(prefix[i])) {
      return 0;
    }
    i++;
  }
  return 1;
}

static int rtc_ice_set_string(char *dst, uint16_t cap, const char *src) {
  if (!dst || !src || cap == 0u) {
    return 0;
  }
  return rtc_platform_copy_string(dst, cap, src);
}

static void rtc_ice_reset_offer_fields(rtc_ice_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  ctx->remote_description_set = 0u;
  ctx->answer_ready = 0u;
  ctx->local_description_emitted = 0u;
  ctx->local_candidate_emitted = 0u;
  ctx->media_order_count = 0u;
  memset(ctx->media_order, 0, sizeof(ctx->media_order));

  ctx->audio_offer_present = 0u;
  ctx->video_offer_present = 0u;
  ctx->audio_accepted = 0u;
  ctx->video_accepted = 0u;
  ctx->audio_rtcp_mux = 0u;
  ctx->video_rtcp_mux = 0u;
  ctx->video_h264_found = 0u;
  ctx->video_h264_pm1_found = 0u;
  ctx->audio_pcma_pt = -1;
  ctx->audio_pcmu_pt = -1;
  ctx->audio_selected_pt = -1;
  ctx->audio_first_pt = -1;
  ctx->video_h264_pt = -1;
  ctx->video_h264_pm1_pt = -1;
  ctx->video_selected_pt = -1;
  ctx->video_first_pt = -1;
  ctx->audio_ssrc_present = 0u;
  ctx->video_ssrc_present = 0u;
  ctx->audio_remote_ssrc = 0u;
  ctx->video_remote_ssrc = 0u;

  ctx->local_candidate_count = 0u;
  ctx->connect_ticks = 0u;
  ctx->retry_count = 0u;
  ctx->stun_out_head = 0u;
  ctx->stun_out_tail = 0u;
  ctx->stun_out_size = 0u;
  memset(ctx->stun_out_queue, 0, sizeof(ctx->stun_out_queue));
  memset(ctx->local_sdp, 0, sizeof(ctx->local_sdp));
  memset(ctx->local_candidate, 0, sizeof(ctx->local_candidate));
  memset(ctx->remote_ice_ufrag, 0, sizeof(ctx->remote_ice_ufrag));
  memset(ctx->remote_ice_pwd, 0, sizeof(ctx->remote_ice_pwd));
  memset(ctx->remote_fingerprint, 0, sizeof(ctx->remote_fingerprint));
  memset(ctx->remote_setup, 0, sizeof(ctx->remote_setup));
  memset(ctx->audio_mid, 0, sizeof(ctx->audio_mid));
  memset(ctx->video_mid, 0, sizeof(ctx->video_mid));
  memset(ctx->video_fmtp, 0, sizeof(ctx->video_fmtp));
  rtc_ice_reset_connectivity_state(ctx);
}

static void rtc_ice_prepare_local_credentials(rtc_ice_ctx_t *ctx) {
  if (!ctx || ctx->peer_id == 0u) {
    return;
  }
  if (ctx->local_ice_ufrag[0] == '\0') {
    (void)snprintf(ctx->local_ice_ufrag, sizeof(ctx->local_ice_ufrag), "u%u",
                   ctx->peer_id);
  }
  if (ctx->local_ice_pwd[0] == '\0') {
    (void)snprintf(ctx->local_ice_pwd, sizeof(ctx->local_ice_pwd),
                   "pw%08u%08u%08u", ctx->peer_id, ctx->peer_id * 3u + 7u,
                   ctx->peer_id * 11u + 3u);
  }
}

static void rtc_ice_add_media_order(rtc_ice_ctx_t *ctx, uint8_t media_kind) {
  uint8_t i;
  if (!ctx || media_kind == RTC_MEDIA_NONE || media_kind == RTC_MEDIA_OTHER) {
    return;
  }
  for (i = 0u; i < ctx->media_order_count; ++i) {
    if (ctx->media_order[i] == media_kind) {
      return;
    }
  }
  if (ctx->media_order_count < sizeof(ctx->media_order)) {
    ctx->media_order[ctx->media_order_count] = media_kind;
    ctx->media_order_count++;
  }
}

static rtc_result_t rtc_ice_sdp_append(char *dst, uint16_t cap, uint16_t *io_len,
                                       const char *fmt, ...) {
  va_list ap;
  int n;
  uint16_t offset;

  if (!dst || !io_len || !fmt) {
    return RTC_ERR_INVALID_ARG;
  }
  offset = *io_len;
  if (offset >= cap) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }

  va_start(ap, fmt);
  n = vsnprintf(dst + offset, (size_t)(cap - offset), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)(cap - offset)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  offset = (uint16_t)(offset + (uint16_t)n);
  if ((uint16_t)(offset + 2u) >= cap) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  dst[offset++] = '\r';
  dst[offset++] = '\n';
  dst[offset] = '\0';
  *io_len = offset;
  return RTC_OK;
}

static int rtc_ice_parse_int16(const char *text, int16_t *out_value) {
  long value;
  char *end_ptr = NULL;
  if (!text || !out_value) {
    return 0;
  }
  value = strtol(text, &end_ptr, 10);
  if (end_ptr == text || value < -32768L || value > 32767L) {
    return 0;
  }
  *out_value = (int16_t)value;
  return 1;
}

static int rtc_ice_parse_ssrc_value(const char *text, uint32_t *out_ssrc) {
  unsigned long value;
  char *end_ptr = NULL;
  if (!text || !out_ssrc) {
    return 0;
  }
  value = strtoul(text, &end_ptr, 10);
  if (end_ptr == text || value > 0xFFFFFFFFUL) {
    return 0;
  }
  while (*end_ptr == ' ' || *end_ptr == '\t') {
    end_ptr++;
  }
  *out_ssrc = (uint32_t)value;
  return 1;
}

static int rtc_ice_parse_m_first_pt(const char *mline_content, int16_t *out_first_pt) {
  char media[16];
  char proto[40];
  unsigned port = 0u;
  int first_pt = -1;
  int parsed;

  if (!mline_content || !out_first_pt) {
    return 0;
  }
  *out_first_pt = -1;

  parsed = sscanf(mline_content, "%15s %u %39s %d", media, &port, proto, &first_pt);
  if (parsed < 3) {
    return 0;
  }
  if (parsed >= 4 && first_pt >= 0 && first_pt <= 127) {
    *out_first_pt = (int16_t)first_pt;
  }
  return 1;
}

static int rtc_ice_parse_rtpmap(const char *rtpmap_value, int16_t *out_pt,
                                char *codec, uint16_t codec_cap, int *out_clock_rate) {
  const char *space;
  const char *slash;
  int16_t pt = -1;
  size_t codec_len;
  long clock_rate;
  char *clock_end = NULL;

  if (!rtpmap_value || !out_pt || !codec || codec_cap == 0u || !out_clock_rate) {
    return 0;
  }
  *out_pt = -1;
  codec[0] = '\0';
  *out_clock_rate = 0;

  if (!rtc_ice_parse_int16(rtpmap_value, &pt)) {
    return 0;
  }
  space = strchr(rtpmap_value, ' ');
  if (!space || *(space + 1) == '\0') {
    return 0;
  }
  slash = strchr(space + 1, '/');
  if (!slash || slash == space + 1) {
    return 0;
  }

  codec_len = (size_t)(slash - (space + 1));
  if (codec_len + 1u > codec_cap) {
    return 0;
  }
  memcpy(codec, space + 1, codec_len);
  codec[codec_len] = '\0';

  clock_rate = strtol(slash + 1, &clock_end, 10);
  if (clock_end == slash + 1 || clock_rate <= 0 || clock_rate > 192000L) {
    return 0;
  }

  *out_pt = pt;
  *out_clock_rate = (int)clock_rate;
  return 1;
}

static int rtc_ice_parse_fmtp(const char *fmtp_value, int16_t *out_pt,
                              const char **out_params) {
  const char *space;
  int16_t pt = -1;

  if (!fmtp_value || !out_pt || !out_params) {
    return 0;
  }
  *out_pt = -1;
  *out_params = NULL;

  if (!rtc_ice_parse_int16(fmtp_value, &pt)) {
    return 0;
  }
  space = strchr(fmtp_value, ' ');
  if (!space || *(space + 1) == '\0') {
    return 0;
  }
  *out_pt = pt;
  *out_params = space + 1;
  return 1;
}

static int rtc_ice_find_h264_entry(rtc_h264_entry_t *entries, uint8_t count,
                                   int16_t pt) {
  uint8_t i;
  for (i = 0u; i < count; ++i) {
    if (entries[i].pt == pt) {
      return (int)i;
    }
  }
  return -1;
}

static int rtc_ice_ensure_h264_entry(rtc_h264_entry_t *entries, uint8_t *io_count,
                                     int16_t pt) {
  int idx;
  if (!entries || !io_count) {
    return -1;
  }
  idx = rtc_ice_find_h264_entry(entries, *io_count, pt);
  if (idx >= 0) {
    return idx;
  }
  if (*io_count >= 8u) {
    return -1;
  }
  entries[*io_count].pt = pt;
  entries[*io_count].has_fmtp = 0u;
  entries[*io_count].packetization_mode_1 = 0u;
  entries[*io_count].fmtp[0] = '\0';
  (*io_count)++;
  return (int)(*io_count - 1u);
}

static int rtc_ice_fmtp_packetization_mode_is_1(const char *params) {
  const char *cursor;
  const char *key = "packetization-mode=";
  const uint16_t key_len = 19u;

  if (!params) {
    return 0;
  }

  cursor = params;
  while ((cursor = strstr(cursor, key)) != NULL) {
    const char *value = cursor + key_len;
    char *end_ptr = NULL;
    long mode = 0;

    if (cursor != params) {
      char prev = *(cursor - 1);
      if (prev != ';' && prev != ' ' && prev != '\t') {
        cursor = value;
        continue;
      }
    }

    while (*value == ' ' || *value == '\t') {
      value++;
    }

    mode = strtol(value, &end_ptr, 10);
    if (end_ptr == value) {
      cursor = value;
      continue;
    }

    while (*end_ptr == ' ' || *end_ptr == '\t') {
      end_ptr++;
    }
    if (*end_ptr != '\0' && *end_ptr != ';') {
      cursor = end_ptr;
      continue;
    }

    if (mode == 1L) {
      return 1;
    }
    cursor = end_ptr;
  }

  return 0;
}

static rtc_result_t rtc_ice_build_answer(rtc_ice_ctx_t *ctx) {
  uint16_t sdp_len = 0u;
  uint8_t i;
  uint8_t candidate_written = 0u;
  char bundle[48];
  uint8_t accepted_media_count = 0u;
  rtc_result_t r;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!ctx->local_host_ready || !ctx->local_fingerprint_ready) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!ctx->remote_description_set) {
    return RTC_ERR_INVALID_STATE;
  }
  if (rtc_ascii_case_eq(ctx->remote_setup, "passive")) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!rtc_ascii_case_eq(ctx->remote_setup, "actpass") &&
      !rtc_ascii_case_eq(ctx->remote_setup, "active")) {
    return RTC_ERR_PROTOCOL;
  }

  memset(bundle, 0, sizeof(bundle));
  for (i = 0u; i < ctx->media_order_count; ++i) {
    if (ctx->media_order[i] == RTC_MEDIA_AUDIO && ctx->audio_accepted) {
      const char *mid = ctx->audio_mid[0] != '\0' ? ctx->audio_mid : "0";
      if (bundle[0] != '\0') {
        (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle),
                       " ");
      }
      (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle), "%s",
                     mid);
      accepted_media_count++;
    } else if (ctx->media_order[i] == RTC_MEDIA_VIDEO && ctx->video_accepted) {
      const char *mid = ctx->video_mid[0] != '\0' ? ctx->video_mid : "1";
      if (bundle[0] != '\0') {
        (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle),
                       " ");
      }
      (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle), "%s",
                     mid);
      accepted_media_count++;
    }
  }
  if (accepted_media_count == 0u) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  (void)snprintf(ctx->local_candidate, sizeof(ctx->local_candidate),
                 "candidate:%u 1 udp 2130706431 %u.%u.%u.%u %u typ host",
                 (unsigned int)ctx->peer_id, ctx->local_host_ip[0], ctx->local_host_ip[1],
                 ctx->local_host_ip[2], ctx->local_host_ip[3], ctx->local_host_port);

  memset(ctx->local_sdp, 0, sizeof(ctx->local_sdp));
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "v=0");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                         "o=- %u 2 IN IP4 %u.%u.%u.%u", (unsigned int)ctx->peer_id,
                         ctx->local_host_ip[0], ctx->local_host_ip[1], ctx->local_host_ip[2],
                         ctx->local_host_ip[3]);
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "s=-");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "t=0 0");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                         "a=group:BUNDLE %s", bundle);
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "a=ice-lite");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                         "a=fingerprint:sha-256 %s", ctx->local_fingerprint);
  if (r != RTC_OK) {
    return r;
  }

  for (i = 0u; i < ctx->media_order_count; ++i) {
    uint8_t media_kind = ctx->media_order[i];
    uint8_t accepted = 0u;
    int16_t selected_pt = -1;
    int16_t fallback_pt = -1;
    const char *mid = NULL;

    if (media_kind == RTC_MEDIA_AUDIO) {
      accepted = ctx->audio_accepted;
      selected_pt = ctx->audio_selected_pt;
      fallback_pt = ctx->audio_first_pt;
      mid = ctx->audio_mid[0] != '\0' ? ctx->audio_mid : "0";
      if (fallback_pt < 0) {
        fallback_pt = 0;
      }

      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "m=audio %u UDP/TLS/RTP/SAVPF %d",
                             accepted ? 9u : 0u, accepted ? selected_pt : fallback_pt);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "c=IN IP4 0.0.0.0");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "a=mid:%s",
                             mid);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=setup:passive");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-ufrag:%s", ctx->local_ice_ufrag);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-pwd:%s", ctx->local_ice_pwd);
      if (r != RTC_OK) {
        return r;
      }

      if (accepted) {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=rtcp-mux");
        if (r != RTC_OK) {
          return r;
        }
        if (selected_pt == ctx->audio_pcma_pt) {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=rtpmap:%d PCMA/8000", selected_pt);
        } else {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=rtpmap:%d PCMU/8000", selected_pt);
        }
        if (r != RTC_OK) {
          return r;
        }
        if (!candidate_written) {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=%s", ctx->local_candidate);
          if (r != RTC_OK) {
            return r;
          }
          candidate_written = 1u;
        }
      } else {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=inactive");
        if (r != RTC_OK) {
          return r;
        }
      }
    } else if (media_kind == RTC_MEDIA_VIDEO) {
      accepted = ctx->video_accepted;
      selected_pt = ctx->video_selected_pt;
      fallback_pt = ctx->video_first_pt;
      mid = ctx->video_mid[0] != '\0' ? ctx->video_mid : "1";
      if (fallback_pt < 0) {
        fallback_pt = 96;
      }

      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "m=video %u UDP/TLS/RTP/SAVPF %d",
                             accepted ? 9u : 0u, accepted ? selected_pt : fallback_pt);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "c=IN IP4 0.0.0.0");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "a=mid:%s",
                             mid);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=setup:passive");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-ufrag:%s", ctx->local_ice_ufrag);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-pwd:%s", ctx->local_ice_pwd);
      if (r != RTC_OK) {
        return r;
      }

      if (accepted) {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=rtcp-mux");
        if (r != RTC_OK) {
          return r;
        }
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=rtpmap:%d H264/90000", selected_pt);
        if (r != RTC_OK) {
          return r;
        }
        if (ctx->video_fmtp[0] != '\0') {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=fmtp:%d %s", selected_pt, ctx->video_fmtp);
          if (r != RTC_OK) {
            return r;
          }
        }
        if (!candidate_written) {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=%s", ctx->local_candidate);
          if (r != RTC_OK) {
            return r;
          }
          candidate_written = 1u;
        }
      } else {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=inactive");
        if (r != RTC_OK) {
          return r;
        }
      }
    }
  }

  if (!candidate_written) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  (void)snprintf(ctx->local_description_type, sizeof(ctx->local_description_type), "%s",
                 "answer");
  ctx->local_candidate_count = 1u;
  ctx->answer_ready = 1u;
  return RTC_OK;
}

static rtc_result_t rtc_ice_parse_offer(rtc_ice_ctx_t *ctx, const char *sdp) {
  const char *cursor = NULL;
  uint8_t current_media = RTC_MEDIA_NONE;
  rtc_h264_entry_t h264_entries[8];
  uint8_t h264_count = 0u;
  uint8_t h264_selected_idx = 0xFFu;

  if (!ctx || !sdp) {
    return RTC_ERR_INVALID_ARG;
  }
  memset(h264_entries, 0, sizeof(h264_entries));

  cursor = sdp;
  while (cursor && *cursor != '\0') {
    const char *nl = strchr(cursor, '\n');
    size_t line_len = nl ? (size_t)(nl - cursor) : strlen(cursor);
    char line[RTC_SDP_MAX_LINE];

    if (line_len > 0u && cursor[line_len - 1u] == '\r') {
      line_len--;
    }
    if (line_len >= sizeof(line)) {
      return RTC_ERR_PROTOCOL;
    }
    memcpy(line, cursor, line_len);
    line[line_len] = '\0';

    if (line[0] == 'm' && line[1] == '=') {
      if (strncmp(line + 2, "audio ", 6u) == 0) {
        current_media = RTC_MEDIA_AUDIO;
        ctx->audio_offer_present = 1u;
        rtc_ice_add_media_order(ctx, RTC_MEDIA_AUDIO);
        (void)rtc_ice_parse_m_first_pt(line + 2, &ctx->audio_first_pt);
        if (ctx->audio_mid[0] == '\0') {
          (void)snprintf(ctx->audio_mid, sizeof(ctx->audio_mid), "%s", "0");
        }
      } else if (strncmp(line + 2, "video ", 6u) == 0) {
        current_media = RTC_MEDIA_VIDEO;
        ctx->video_offer_present = 1u;
        rtc_ice_add_media_order(ctx, RTC_MEDIA_VIDEO);
        (void)rtc_ice_parse_m_first_pt(line + 2, &ctx->video_first_pt);
        if (ctx->video_mid[0] == '\0') {
          (void)snprintf(ctx->video_mid, sizeof(ctx->video_mid), "%s", "1");
        }
      } else {
        current_media = RTC_MEDIA_OTHER;
      }
    } else if (line[0] == 'a' && line[1] == '=') {
      const char *attr = line + 2;

      if (strncmp(attr, "ice-ufrag:", 10u) == 0) {
        if (!rtc_ice_set_string(ctx->remote_ice_ufrag, sizeof(ctx->remote_ice_ufrag),
                                attr + 10u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "ice-pwd:", 8u) == 0) {
        if (!rtc_ice_set_string(ctx->remote_ice_pwd, sizeof(ctx->remote_ice_pwd),
                                attr + 8u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "fingerprint:", 12u) == 0) {
        const char *value = attr + 12u;
        if (!rtc_ascii_case_has_prefix(value, "sha-256 ")) {
          return RTC_ERR_NOT_SUPPORTED;
        }
        if (!rtc_ice_set_string(ctx->remote_fingerprint, sizeof(ctx->remote_fingerprint),
                                value + 8u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "setup:", 6u) == 0) {
        if (!rtc_ice_set_string(ctx->remote_setup, sizeof(ctx->remote_setup), attr + 6u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "candidate:", 10u) == 0) {
        rtc_result_t r = rtc_ice_add_remote_candidate(ctx, attr);
        if (r != RTC_OK) {
          return r;
        }
      } else if (strncmp(attr, "ssrc:", 5u) == 0) {
        uint32_t ssrc = 0u;
        if (!rtc_ice_parse_ssrc_value(attr + 5u, &ssrc)) {
          return RTC_ERR_PROTOCOL;
        }
        if (current_media == RTC_MEDIA_AUDIO && !ctx->audio_ssrc_present) {
          ctx->audio_remote_ssrc = ssrc;
          ctx->audio_ssrc_present = 1u;
        } else if (current_media == RTC_MEDIA_VIDEO && !ctx->video_ssrc_present) {
          ctx->video_remote_ssrc = ssrc;
          ctx->video_ssrc_present = 1u;
        }
      } else if (strncmp(attr, "mid:", 4u) == 0) {
        if (current_media == RTC_MEDIA_AUDIO) {
          if (!rtc_ice_set_string(ctx->audio_mid, sizeof(ctx->audio_mid), attr + 4u)) {
            return RTC_ERR_BUFFER_TOO_SMALL;
          }
        } else if (current_media == RTC_MEDIA_VIDEO) {
          if (!rtc_ice_set_string(ctx->video_mid, sizeof(ctx->video_mid), attr + 4u)) {
            return RTC_ERR_BUFFER_TOO_SMALL;
          }
        }
      } else if (rtc_ascii_case_eq(attr, "rtcp-mux")) {
        if (current_media == RTC_MEDIA_AUDIO) {
          ctx->audio_rtcp_mux = 1u;
        } else if (current_media == RTC_MEDIA_VIDEO) {
          ctx->video_rtcp_mux = 1u;
        }
      } else if (strncmp(attr, "rtpmap:", 7u) == 0) {
        int16_t pt = -1;
        char codec[24];
        int clock_rate = 0;

        if (!rtc_ice_parse_rtpmap(attr + 7u, &pt, codec, sizeof(codec), &clock_rate)) {
          return RTC_ERR_PROTOCOL;
        }
        if (current_media == RTC_MEDIA_AUDIO) {
          if (clock_rate == 8000 && rtc_ascii_case_eq(codec, "PCMA")) {
            ctx->audio_pcma_pt = pt;
          } else if (clock_rate == 8000 && rtc_ascii_case_eq(codec, "PCMU")) {
            ctx->audio_pcmu_pt = pt;
          }
        } else if (current_media == RTC_MEDIA_VIDEO) {
          if (clock_rate == 90000 && rtc_ascii_case_eq(codec, "H264")) {
            int idx = rtc_ice_ensure_h264_entry(h264_entries, &h264_count, pt);
            if (idx < 0) {
              return RTC_ERR_PROTOCOL;
            }
            ctx->video_h264_found = 1u;
          }
        }
      } else if (strncmp(attr, "fmtp:", 5u) == 0) {
        int16_t pt = -1;
        const char *params = NULL;

        if (!rtc_ice_parse_fmtp(attr + 5u, &pt, &params)) {
          return RTC_ERR_PROTOCOL;
        }
        if (current_media == RTC_MEDIA_VIDEO) {
          int idx = rtc_ice_find_h264_entry(h264_entries, h264_count, pt);
          if (idx >= 0) {
            rtc_h264_entry_t *entry = &h264_entries[idx];
            if (!rtc_ice_set_string(entry->fmtp, sizeof(entry->fmtp), params)) {
              return RTC_ERR_BUFFER_TOO_SMALL;
            }
            entry->has_fmtp = 1u;
            if (rtc_ice_fmtp_packetization_mode_is_1(params)) {
              entry->packetization_mode_1 = 1u;
            }
          }
        }
      }
    }

    if (!nl) {
      break;
    }
    cursor = nl + 1;
  }

  if (ctx->remote_ice_ufrag[0] == '\0' || ctx->remote_ice_pwd[0] == '\0' ||
      ctx->remote_fingerprint[0] == '\0' || ctx->remote_setup[0] == '\0') {
    return RTC_ERR_PROTOCOL;
  }

  if (ctx->audio_pcma_pt >= 0) {
    ctx->audio_selected_pt = ctx->audio_pcma_pt;
  } else if (ctx->audio_pcmu_pt >= 0) {
    ctx->audio_selected_pt = ctx->audio_pcmu_pt;
  } else {
    ctx->audio_selected_pt = -1;
  }

  if (ctx->video_h264_found && h264_count > 0u) {
    uint8_t i;
    h264_selected_idx = 0u;
    for (i = 0u; i < h264_count; ++i) {
      if (h264_entries[i].packetization_mode_1) {
        h264_selected_idx = i;
        ctx->video_h264_pm1_found = 1u;
        break;
      }
    }
    ctx->video_selected_pt = h264_entries[h264_selected_idx].pt;
    ctx->video_h264_pt = h264_entries[0].pt;
    if (ctx->video_h264_pm1_found) {
      ctx->video_h264_pm1_pt = h264_entries[h264_selected_idx].pt;
    }
    if (h264_entries[h264_selected_idx].has_fmtp) {
      (void)snprintf(ctx->video_fmtp, sizeof(ctx->video_fmtp), "%s",
                     h264_entries[h264_selected_idx].fmtp);
    }
  }

  ctx->audio_accepted =
      (uint8_t)(ctx->audio_offer_present && ctx->audio_selected_pt >= 0 &&
                ctx->audio_rtcp_mux);
  ctx->video_accepted =
      (uint8_t)(ctx->video_offer_present && ctx->video_selected_pt >= 0 &&
                ctx->video_rtcp_mux);

  if (!ctx->audio_accepted && !ctx->video_accepted) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (ctx->audio_accepted && !ctx->audio_ssrc_present) {
    return RTC_ERR_PROTOCOL;
  }
  if (ctx->video_accepted && !ctx->video_ssrc_present) {
    return RTC_ERR_PROTOCOL;
  }
  return RTC_OK;
}

void rtc_ice_init(rtc_ice_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_ICE_STATE_NEW;
  ctx->audio_pcma_pt = -1;
  ctx->audio_pcmu_pt = -1;
  ctx->audio_selected_pt = -1;
  ctx->audio_first_pt = -1;
  ctx->video_h264_pt = -1;
  ctx->video_h264_pm1_pt = -1;
  ctx->video_selected_pt = -1;
  ctx->video_first_pt = -1;
  ctx->audio_ssrc_present = 0u;
  ctx->video_ssrc_present = 0u;
  ctx->audio_remote_ssrc = 0u;
  ctx->video_remote_ssrc = 0u;
  ctx->active_pair_index = -1;
  ctx->selected_pair_index = -1;
}

void rtc_ice_set_peer_id(rtc_ice_ctx_t *ctx, uint32_t peer_id) {
  if (!ctx) {
    return;
  }
  ctx->peer_id = peer_id;
  rtc_ice_prepare_local_credentials(ctx);
}

rtc_result_t rtc_ice_set_local_host(rtc_ice_ctx_t *ctx, const uint8_t ip[4],
                                    uint16_t port) {
  if (!ctx || !ip || port == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  memcpy(ctx->local_host_ip, ip, 4u);
  ctx->local_host_port = port;
  ctx->local_host_ready = 1u;
  return RTC_OK;
}

rtc_result_t rtc_ice_set_local_fingerprint(rtc_ice_ctx_t *ctx,
                                           const char *fingerprint_sha256) {
  if (!ctx || !fingerprint_sha256 || fingerprint_sha256[0] == '\0') {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_ice_set_string(ctx->local_fingerprint, sizeof(ctx->local_fingerprint),
                          fingerprint_sha256)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  ctx->local_fingerprint_ready = 1u;
  return RTC_OK;
}

rtc_result_t rtc_ice_start(rtc_ice_ctx_t *ctx, uint32_t peer_id, uint32_t now_ms) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }

  if (peer_id != 0u) {
    ctx->peer_id = peer_id;
  }
  rtc_ice_prepare_local_credentials(ctx);

  ctx->local_description_emitted = 0u;
  ctx->local_candidate_emitted = 0u;
  ctx->stun_out_head = 0u;
  ctx->stun_out_tail = 0u;
  ctx->stun_out_size = 0u;
  memset(ctx->stun_out_queue, 0, sizeof(ctx->stun_out_queue));
  rtc_ice_reset_connectivity_state(ctx);
  ctx->checks_sent = 0u;
  ctx->checks_ok = 0u;
  ctx->checks_failed = 0u;
  ctx->checks_drop = 0u;
  ctx->state = RTC_ICE_STATE_GATHERING;
  ctx->last_tick_ms = now_ms;

  return RTC_OK;
}

rtc_result_t rtc_ice_set_remote_description(rtc_ice_ctx_t *ctx, const char *sdp,
                                            const char *type) {
  uint16_t remote_candidate_count_backup = 0u;
  uint16_t pair_count_backup = 0u;
  int16_t active_pair_backup = -1;
  int16_t selected_pair_backup = -1;
  char remote_candidates_backup[RTC_CFG_MAX_REMOTE_CANDIDATES][RTC_CFG_MAX_CANDIDATE_LEN];
  rtc_ice_remote_candidate_t remote_items_backup[RTC_CFG_MAX_REMOTE_CANDIDATES];
  rtc_ice_candidate_pair_t pairs_backup[RTC_CFG_MAX_CANDIDATE_PAIRS];

  if (!ctx || !sdp || !type) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_ascii_case_eq(type, "offer")) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!ctx->local_host_ready || !ctx->local_fingerprint_ready) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  memset(remote_candidates_backup, 0, sizeof(remote_candidates_backup));
  memset(remote_items_backup, 0, sizeof(remote_items_backup));
  memset(pairs_backup, 0, sizeof(pairs_backup));
  remote_candidate_count_backup = ctx->remote_candidate_count;
  pair_count_backup = ctx->pair_count;
  active_pair_backup = ctx->active_pair_index;
  selected_pair_backup = ctx->selected_pair_index;
  if (remote_candidate_count_backup > 0u) {
    memcpy(remote_candidates_backup, ctx->remote_candidates,
           sizeof(remote_candidates_backup));
    memcpy(remote_items_backup, ctx->remote_candidate_items, sizeof(remote_items_backup));
  }
  if (pair_count_backup > 0u) {
    memcpy(pairs_backup, ctx->pairs, sizeof(pairs_backup));
  }
  rtc_ice_reset_offer_fields(ctx);
  if (!rtc_platform_copy_string(ctx->remote_sdp, sizeof(ctx->remote_sdp), sdp)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  if (!rtc_platform_copy_string(ctx->remote_description_type,
                                sizeof(ctx->remote_description_type), type)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }

  {
    rtc_result_t r = rtc_ice_parse_offer(ctx, sdp);
    if (r != RTC_OK) {
      memset(ctx->remote_candidates, 0, sizeof(ctx->remote_candidates));
      memset(ctx->remote_candidate_items, 0, sizeof(ctx->remote_candidate_items));
      memset(ctx->pairs, 0, sizeof(ctx->pairs));
      if (remote_candidate_count_backup > 0u) {
        memcpy(ctx->remote_candidates, remote_candidates_backup,
               sizeof(remote_candidates_backup));
        memcpy(ctx->remote_candidate_items, remote_items_backup,
               sizeof(remote_items_backup));
      }
      if (pair_count_backup > 0u) {
        memcpy(ctx->pairs, pairs_backup, sizeof(pairs_backup));
      }
      ctx->remote_candidate_count = remote_candidate_count_backup;
      ctx->pair_count = pair_count_backup;
      ctx->active_pair_index = active_pair_backup;
      ctx->selected_pair_index = selected_pair_backup;
      return r;
    }
    ctx->remote_description_set = 1u;
    r = rtc_ice_build_answer(ctx);
    if (r != RTC_OK) {
      rtc_ice_reset_offer_fields(ctx);
      memset(ctx->remote_candidates, 0, sizeof(ctx->remote_candidates));
      memset(ctx->remote_candidate_items, 0, sizeof(ctx->remote_candidate_items));
      memset(ctx->pairs, 0, sizeof(ctx->pairs));
      if (remote_candidate_count_backup > 0u) {
        memcpy(ctx->remote_candidates, remote_candidates_backup,
               sizeof(remote_candidates_backup));
        memcpy(ctx->remote_candidate_items, remote_items_backup,
               sizeof(remote_items_backup));
      }
      if (pair_count_backup > 0u) {
        memcpy(ctx->pairs, pairs_backup, sizeof(pairs_backup));
      }
      ctx->remote_candidate_count = remote_candidate_count_backup;
      ctx->pair_count = pair_count_backup;
      ctx->active_pair_index = active_pair_backup;
      ctx->selected_pair_index = selected_pair_backup;
      return r;
    }
  }

  return RTC_OK;
}

rtc_result_t rtc_ice_add_remote_candidate(rtc_ice_ctx_t *ctx, const char *candidate) {
  uint16_t i;
  int by_addr_idx = -1;
  uint8_t ip[4];
  uint16_t port = 0u;
  uint32_t priority = 0u;

  if (!ctx || !candidate) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_ice_parse_candidate_ipv4(candidate, ip, &port, &priority)) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  for (i = 0u; i < ctx->remote_candidate_count; ++i) {
    if (strcmp(ctx->remote_candidates[i], candidate) == 0) {
      return RTC_OK;
    }
  }
  by_addr_idx = rtc_ice_find_remote_candidate_by_addr(ctx, ip, port);
  if (by_addr_idx >= 0) {
    rtc_ice_remote_candidate_t *cand = &ctx->remote_candidate_items[by_addr_idx];
    if (priority > cand->priority) {
      int pair_idx = rtc_ice_find_pair_for_remote(ctx, (uint8_t)by_addr_idx);
      cand->priority = priority;
      if (pair_idx >= 0) {
        ctx->pairs[pair_idx].priority = priority;
      }
    }
    if (!rtc_platform_copy_string(cand->raw, sizeof(cand->raw), candidate)) {
      return RTC_ERR_BUFFER_TOO_SMALL;
    }
    return RTC_OK;
  }

  if (ctx->remote_candidate_count >= RTC_CFG_MAX_REMOTE_CANDIDATES ||
      ctx->pair_count >= RTC_CFG_MAX_CANDIDATE_PAIRS) {
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }
  if (!rtc_platform_copy_string(
          ctx->remote_candidates[ctx->remote_candidate_count], RTC_CFG_MAX_CANDIDATE_LEN,
          candidate)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }

  {
    uint16_t idx = ctx->remote_candidate_count;
    rtc_ice_remote_candidate_t *cand = &ctx->remote_candidate_items[idx];
    rtc_ice_candidate_pair_t *pair = &ctx->pairs[ctx->pair_count];

    memset(cand, 0, sizeof(*cand));
    cand->in_use = 1u;
    memcpy(cand->ip, ip, 4u);
    cand->port = port;
    cand->priority = priority;
    (void)rtc_platform_copy_string(cand->raw, sizeof(cand->raw), candidate);

    memset(pair, 0, sizeof(*pair));
    pair->in_use = 1u;
    pair->remote_index = (uint8_t)idx;
    pair->state = RTC_ICE_PAIR_STATE_WAITING;
    pair->priority = priority;
    ctx->pair_count++;
  }
  ctx->remote_candidate_count++;
  return RTC_OK;
}

static int rtc_ice_find_best_waiting_pair(const rtc_ice_ctx_t *ctx) {
  int best_idx = -1;
  uint32_t best_priority = 0u;
  uint16_t i;
  if (!ctx) {
    return -1;
  }
  for (i = 0u; i < ctx->pair_count; ++i) {
    const rtc_ice_candidate_pair_t *pair = &ctx->pairs[i];
    if (!pair->in_use || pair->state != RTC_ICE_PAIR_STATE_WAITING) {
      continue;
    }
    if (best_idx < 0 || pair->priority > best_priority) {
      best_idx = (int)i;
      best_priority = pair->priority;
    }
  }
  return best_idx;
}

static int rtc_ice_all_pairs_failed(const rtc_ice_ctx_t *ctx) {
  uint16_t i;
  if (!ctx || ctx->pair_count == 0u) {
    return 0;
  }
  for (i = 0u; i < ctx->pair_count; ++i) {
    const rtc_ice_candidate_pair_t *pair = &ctx->pairs[i];
    if (!pair->in_use) {
      continue;
    }
    if (pair->state != RTC_ICE_PAIR_STATE_FAILED) {
      return 0;
    }
  }
  return 1;
}

static void rtc_ice_mark_pair_failed(rtc_ice_ctx_t *ctx, int pair_index) {
  rtc_ice_candidate_pair_t *pair;
  if (!ctx || pair_index < 0 || pair_index >= (int)ctx->pair_count) {
    return;
  }
  pair = &ctx->pairs[pair_index];
  pair->state = RTC_ICE_PAIR_STATE_FAILED;
  pair->transaction_valid = 0u;
  memset(pair->transaction_id, 0, sizeof(pair->transaction_id));
  if (ctx->active_pair_index == pair_index) {
    ctx->active_pair_index = -1;
  }
  ctx->checks_failed++;
}

static void rtc_ice_mark_pair_succeeded(rtc_ice_ctx_t *ctx, int pair_index) {
  rtc_ice_candidate_pair_t *pair;
  if (!ctx || pair_index < 0 || pair_index >= (int)ctx->pair_count) {
    return;
  }
  pair = &ctx->pairs[pair_index];
  pair->state = RTC_ICE_PAIR_STATE_SUCCEEDED;
  pair->transaction_valid = 0u;
  memset(pair->transaction_id, 0, sizeof(pair->transaction_id));
  ctx->selected_pair_index = pair_index;
  ctx->active_pair_index = pair_index;
  ctx->checks_ok++;
}

static rtc_result_t rtc_ice_add_dynamic_remote_candidate(rtc_ice_ctx_t *ctx,
                                                          const uint8_t ip[4],
                                                          uint16_t port,
                                                          uint32_t priority) {
  char candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  if (!ctx || !ip || port == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  if (priority == 0u) {
    priority = 1u;
  }
  if (snprintf(candidate, sizeof(candidate),
               "candidate:%u 1 udp %u %u.%u.%u.%u %u typ prflx",
               (unsigned int)(ctx->peer_id + ctx->remote_candidate_count + 1u), priority,
               ip[0], ip[1], ip[2], ip[3], port) <= 0) {
    return RTC_ERR_PROTOCOL;
  }
  return rtc_ice_add_remote_candidate(ctx, candidate);
}

static int rtc_ice_queue_binding_request_for_pair(rtc_ice_ctx_t *ctx, int pair_index,
                                                   uint32_t now_ms) {
  rtc_ice_candidate_pair_t *pair;
  rtc_ice_remote_candidate_t *remote;
  uint8_t packet[RTC_CFG_MTU];
  uint16_t packet_len = 0u;

  if (!ctx || pair_index < 0 || pair_index >= (int)ctx->pair_count) {
    return 0;
  }
  pair = &ctx->pairs[pair_index];
  if (!pair->in_use || pair->remote_index >= ctx->remote_candidate_count) {
    return 0;
  }
  remote = &ctx->remote_candidate_items[pair->remote_index];
  if (!remote->in_use || remote->port == 0u) {
    return 0;
  }

  if (!rtc_ice_build_stun_request(ctx, pair, (uint8_t)pair_index, now_ms, packet,
                                  &packet_len)) {
    return 0;
  }
  if (!rtc_ice_queue_stun_out(ctx, remote->ip, remote->port, packet, packet_len)) {
    return 0;
  }

  pair->retry_count++;
  pair->last_check_ms = now_ms;
  ctx->checks_sent++;
  return 1;
}

rtc_result_t rtc_ice_handle_incoming_stun(rtc_ice_ctx_t *ctx, const uint8_t src_ip[4],
                                          uint16_t src_port, const uint8_t *buf,
                                          uint16_t len, uint32_t now_ms) {
  uint16_t msg_type;
  rtc_stun_attrs_t attrs;
  char expected_username[128];
  int pair_idx = -1;
  int remote_idx = -1;
  int r;
  (void)now_ms;

  if (!ctx || !src_ip || src_port == 0u || !buf || len < 20u) {
    return RTC_ERR_INVALID_ARG;
  }
  if (rtc_ice_read_u32_be(&buf[4]) != RTC_STUN_MAGIC_COOKIE) {
    return RTC_ERR_PROTOCOL;
  }
  msg_type = rtc_ice_read_u16_be(&buf[0]);
  if (msg_type != RTC_STUN_TYPE_BINDING_REQUEST &&
      msg_type != RTC_STUN_TYPE_BINDING_RESPONSE) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!rtc_ice_parse_stun_attrs(buf, len, &attrs)) {
    return RTC_ERR_PROTOCOL;
  }
  if (!rtc_ice_stun_verify_fingerprint(buf, len, &attrs)) {
    ctx->checks_failed++;
    return RTC_ERR_AUTH_FAILED;
  }

  if (msg_type == RTC_STUN_TYPE_BINDING_REQUEST) {
    if (snprintf(expected_username, sizeof(expected_username), "%s:%s",
                 ctx->local_ice_ufrag, ctx->remote_ice_ufrag) <= 0) {
      return RTC_ERR_PROTOCOL;
    }
    if (!rtc_ice_stun_verify_message_integrity(buf, len, &attrs, ctx->local_ice_pwd)) {
      ctx->checks_failed++;
      return RTC_ERR_AUTH_FAILED;
    }
    if (!rtc_ice_stun_username_equals(&attrs, expected_username)) {
      ctx->checks_failed++;
      return RTC_ERR_AUTH_FAILED;
    }

    remote_idx = rtc_ice_find_remote_candidate_by_addr(ctx, src_ip, src_port);
    if (remote_idx < 0) {
      r = rtc_ice_add_dynamic_remote_candidate(ctx, src_ip, src_port,
                                               attrs.has_priority ? attrs.priority : 1u);
      if (r != RTC_OK) {
        ctx->checks_drop++;
        return r;
      }
      remote_idx = rtc_ice_find_remote_candidate_by_addr(ctx, src_ip, src_port);
    }
    if (remote_idx >= 0) {
      pair_idx = rtc_ice_find_pair_for_remote(ctx, (uint8_t)remote_idx);
      if (pair_idx >= 0) {
        rtc_ice_mark_pair_succeeded(ctx, pair_idx);
      }
    }

    {
      uint8_t out_buf[RTC_CFG_MTU];
      uint16_t out_len = 0u;
      if (!rtc_ice_build_stun_response(ctx, &buf[8], expected_username, src_ip, src_port,
                                       out_buf, &out_len)) {
        ctx->checks_failed++;
        return RTC_ERR_PROTOCOL;
      }
      if (!rtc_ice_queue_stun_out(ctx, src_ip, src_port, out_buf, out_len)) {
        return RTC_ERR_OVERFLOW;
      }
    }
    return RTC_OK;
  }

  if (snprintf(expected_username, sizeof(expected_username), "%s:%s",
               ctx->remote_ice_ufrag, ctx->local_ice_ufrag) <= 0) {
    return RTC_ERR_PROTOCOL;
  }
  if (!rtc_ice_stun_verify_message_integrity(buf, len, &attrs, ctx->remote_ice_pwd)) {
    ctx->checks_failed++;
    return RTC_ERR_AUTH_FAILED;
  }
  if (!rtc_ice_stun_username_equals(&attrs, expected_username)) {
    ctx->checks_failed++;
    return RTC_ERR_AUTH_FAILED;
  }

  pair_idx = rtc_ice_find_pair_by_transaction(ctx, &buf[8]);
  if (pair_idx < 0) {
    ctx->checks_failed++;
    return RTC_ERR_PROTOCOL;
  }
  if (ctx->pairs[pair_idx].remote_index >= ctx->remote_candidate_count) {
    ctx->checks_failed++;
    return RTC_ERR_PROTOCOL;
  }
  if (memcmp(src_ip,
             ctx->remote_candidate_items[ctx->pairs[pair_idx].remote_index].ip,
             4u) != 0 ||
      src_port != ctx->remote_candidate_items[ctx->pairs[pair_idx].remote_index].port) {
    ctx->checks_failed++;
    return RTC_ERR_PROTOCOL;
  }

  rtc_ice_mark_pair_succeeded(ctx, pair_idx);
  return RTC_OK;
}

rtc_result_t rtc_ice_dequeue_outgoing_stun(rtc_ice_ctx_t *ctx, uint8_t out_ip[4],
                                           uint16_t *out_port, uint8_t *out_buf,
                                           uint16_t *io_len) {
  rtc_ice_stun_out_t *slot;
  if (!ctx || !out_ip || !out_port || !out_buf || !io_len) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->stun_out_size == 0u) {
    return RTC_ERR_TIMEOUT;
  }

  slot = &ctx->stun_out_queue[ctx->stun_out_head];
  if (*io_len < slot->len) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }

  memcpy(out_ip, slot->ip, 4u);
  *out_port = slot->port;
  memcpy(out_buf, slot->data, slot->len);
  *io_len = slot->len;

  memset(slot, 0, sizeof(*slot));
  ctx->stun_out_head = (uint16_t)((ctx->stun_out_head + 1u) % RTC_CFG_RTCP_FB_QUEUE);
  ctx->stun_out_size--;
  return RTC_OK;
}

rtc_result_t rtc_ice_get_selected_remote(const rtc_ice_ctx_t *ctx, uint8_t out_ip[4],
                                         uint16_t *out_port) {
  int idx;
  const rtc_ice_candidate_pair_t *pair;
  const rtc_ice_remote_candidate_t *remote;
  if (!ctx || !out_ip || !out_port) {
    return RTC_ERR_INVALID_ARG;
  }
  idx = ctx->selected_pair_index;
  if (idx < 0 || idx >= (int)ctx->pair_count) {
    return RTC_ERR_INVALID_STATE;
  }
  pair = &ctx->pairs[idx];
  if (!pair->in_use || pair->state != RTC_ICE_PAIR_STATE_SUCCEEDED ||
      pair->remote_index >= ctx->remote_candidate_count) {
    return RTC_ERR_INVALID_STATE;
  }
  remote = &ctx->remote_candidate_items[pair->remote_index];
  if (!remote->in_use || remote->port == 0u) {
    return RTC_ERR_INVALID_STATE;
  }
  memcpy(out_ip, remote->ip, 4u);
  *out_port = remote->port;
  return RTC_OK;
}

void rtc_ice_tick(rtc_ice_ctx_t *ctx, uint32_t now_ms, uint16_t retry_interval_ms,
                  uint16_t max_retries, rtc_ice_event_t *out_event) {
  rtc_ice_candidate_pair_t *active = NULL;
  uint32_t elapsed;

  if (!ctx || !out_event) {
    return;
  }
  memset(out_event, 0, sizeof(*out_event));

  if (ctx->state == RTC_ICE_STATE_GATHERING) {
    if (ctx->answer_ready && !ctx->local_description_emitted) {
      out_event->emit_local_description = 1u;
      ctx->local_description_emitted = 1u;
    }
    if (ctx->answer_ready && !ctx->local_candidate_emitted) {
      out_event->emit_local_candidate = 1u;
      ctx->local_candidate_emitted = 1u;
    }
    ctx->state = RTC_ICE_STATE_CHECKING;
    ctx->last_tick_ms = now_ms;
    return;
  }

  if (ctx->state != RTC_ICE_STATE_CHECKING) {
    return;
  }

  if (ctx->answer_ready && !ctx->local_description_emitted) {
    out_event->emit_local_description = 1u;
    ctx->local_description_emitted = 1u;
  }
  if (ctx->answer_ready && !ctx->local_candidate_emitted) {
    out_event->emit_local_candidate = 1u;
    ctx->local_candidate_emitted = 1u;
  }

  if (!ctx->remote_description_set) {
    return;
  }

  if (ctx->selected_pair_index >= 0 &&
      ctx->selected_pair_index < (int)ctx->pair_count &&
      ctx->pairs[ctx->selected_pair_index].state == RTC_ICE_PAIR_STATE_SUCCEEDED) {
    ctx->state = RTC_ICE_STATE_CONNECTED;
    out_event->connected = 1u;
    return;
  }

  if (ctx->remote_candidate_count == 0u || ctx->pair_count == 0u) {
    elapsed = now_ms - ctx->last_tick_ms;
    if (elapsed < retry_interval_ms) {
      return;
    }
    ctx->last_tick_ms = now_ms;
    if (ctx->retry_count < max_retries) {
      ctx->retry_count++;
      out_event->retry_performed = 1u;
      return;
    }
    ctx->state = RTC_ICE_STATE_FAILED;
    out_event->failed = 1u;
    out_event->error = RTC_ERR_TIMEOUT;
    return;
  }

  if (ctx->active_pair_index < 0 || ctx->active_pair_index >= (int)ctx->pair_count ||
      !ctx->pairs[ctx->active_pair_index].in_use ||
      ctx->pairs[ctx->active_pair_index].state != RTC_ICE_PAIR_STATE_INPROGRESS) {
    int best_waiting = rtc_ice_find_best_waiting_pair(ctx);
    if (best_waiting >= 0) {
      ctx->active_pair_index = best_waiting;
      ctx->pairs[best_waiting].state = RTC_ICE_PAIR_STATE_INPROGRESS;
      ctx->pairs[best_waiting].retry_count = 0u;
      ctx->pairs[best_waiting].last_check_ms = 0u;
      ctx->pairs[best_waiting].transaction_valid = 0u;
    } else if (rtc_ice_all_pairs_failed(ctx)) {
      ctx->state = RTC_ICE_STATE_FAILED;
      out_event->failed = 1u;
      out_event->error = RTC_ERR_TIMEOUT;
      return;
    } else {
      return;
    }
  }

  active = &ctx->pairs[ctx->active_pair_index];
  if (active->retry_count >= max_retries) {
    rtc_ice_mark_pair_failed(ctx, ctx->active_pair_index);
    out_event->retry_performed = 1u;
    return;
  }

  if (active->last_check_ms != 0u) {
    elapsed = now_ms - active->last_check_ms;
    // Keep one full interval for an in-flight transaction before rotating TID.
    if (elapsed <= retry_interval_ms) {
      return;
    }
  }

  if (rtc_ice_queue_binding_request_for_pair(ctx, ctx->active_pair_index, now_ms)) {
    out_event->retry_performed = 1u;
  } else {
    ctx->checks_drop++;
    out_event->retry_performed = 1u;
  }
}
