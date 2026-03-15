#ifndef RTC_TEST_STUN_HELPERS_H_
#define RTC_TEST_STUN_HELPERS_H_

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <mbedtls/md.h>

#include "platform/rtc_platform.h"

#define RTC_TEST_STUN_TYPE_BINDING_REQUEST 0x0001u
#define RTC_TEST_STUN_TYPE_BINDING_RESPONSE 0x0101u
#define RTC_TEST_STUN_MAGIC_COOKIE 0x2112A442u
#define RTC_TEST_STUN_ATTR_USERNAME 0x0006u
#define RTC_TEST_STUN_ATTR_MESSAGE_INTEGRITY 0x0008u
#define RTC_TEST_STUN_ATTR_PRIORITY 0x0024u
#define RTC_TEST_STUN_ATTR_ICE_CONTROLLED 0x8029u
#define RTC_TEST_STUN_ATTR_USE_CANDIDATE 0x0025u
#define RTC_TEST_STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020u
#define RTC_TEST_STUN_ATTR_FINGERPRINT 0x8028u

#if defined(__GNUC__) || defined(__clang__)
#define RTC_TEST_MAYBE_UNUSED __attribute__((unused))
#else
#define RTC_TEST_MAYBE_UNUSED
#endif

static void rtc_test_stun_write_u16_be(uint8_t *dst, uint16_t value) {
  if (!dst) {
    return;
  }
  dst[0] = (uint8_t)((value >> 8) & 0xFFu);
  dst[1] = (uint8_t)(value & 0xFFu);
}

static void rtc_test_stun_write_u32_be(uint8_t *dst, uint32_t value) {
  if (!dst) {
    return;
  }
  dst[0] = (uint8_t)((value >> 24) & 0xFFu);
  dst[1] = (uint8_t)((value >> 16) & 0xFFu);
  dst[2] = (uint8_t)((value >> 8) & 0xFFu);
  dst[3] = (uint8_t)(value & 0xFFu);
}

static uint16_t rtc_test_stun_read_u16_be(const uint8_t *src) {
  return src ? (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]) : 0u;
}

static uint32_t rtc_test_stun_read_u32_be(const uint8_t *src) {
  if (!src) {
    return 0u;
  }
  return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) | (uint32_t)src[3];
}

static uint32_t rtc_test_stun_crc32_ieee(const uint8_t *buf, uint16_t len) {
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

static int rtc_test_stun_hmac_sha1(const uint8_t *key, uint16_t key_len,
                                   const uint8_t *data, uint16_t data_len,
                                   uint8_t out[20]) {
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

static int rtc_test_stun_append_attr(uint8_t *buf, uint16_t cap, uint16_t *io_offset,
                                     uint16_t attr_type, const uint8_t *value,
                                     uint16_t value_len) {
  uint16_t offset = 0u;
  uint16_t padded_len = 0u;
  uint16_t i;

  if (!buf || !io_offset) {
    return 0;
  }
  offset = *io_offset;
  padded_len = (uint16_t)((value_len + 3u) & 0xFFFCu);
  if ((uint32_t)offset + 4u + padded_len > (uint32_t)cap) {
    return 0;
  }

  rtc_test_stun_write_u16_be(&buf[offset], attr_type);
  rtc_test_stun_write_u16_be(&buf[offset + 2u], value_len);
  offset += 4u;
  if (value_len > 0u && value) {
    memcpy(&buf[offset], value, value_len);
  }
  for (i = value_len; i < padded_len; ++i) {
    buf[offset + i] = 0u;
  }
  offset = (uint16_t)(offset + padded_len);
  *io_offset = offset;
  return 1;
}

static int rtc_test_extract_sdp_attr(const char *sdp, const char *prefix, char *out,
                                     uint16_t out_cap) {
  const char *p;
  const char *line_end;
  size_t value_len;
  if (!sdp || !prefix || !out || out_cap == 0u) {
    return 0;
  }
  p = strstr(sdp, prefix);
  if (!p) {
    return 0;
  }
  p += strlen(prefix);
  line_end = strstr(p, "\r\n");
  if (!line_end) {
    line_end = strchr(p, '\n');
  }
  if (!line_end) {
    line_end = p + strlen(p);
  }
  value_len = (size_t)(line_end - p);
  if (value_len + 1u > out_cap) {
    return 0;
  }
  memcpy(out, p, value_len);
  out[value_len] = '\0';
  return 1;
}

static RTC_TEST_MAYBE_UNUSED int rtc_test_parse_candidate_host_ipv4(
    const char *candidate, uint8_t out_ip[4], uint16_t *out_port) {
  const char *p;
  char foundation[64];
  char transport[8];
  char ip[64];
  char typ_key[8];
  char candidate_type[16];
  unsigned component = 0u;
  unsigned priority = 0u;
  unsigned port = 0u;
  int parsed;

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
  p += 10u;
  parsed = sscanf(p, "%63s %u %7s %u %63s %u %7s %15s", foundation, &component,
                  transport, &priority, ip, &port, typ_key, candidate_type);
  if (parsed != 8) {
    return 0;
  }
  (void)foundation;
  (void)component;
  (void)priority;
  if (strcasecmp(transport, "udp") != 0 || strcasecmp(typ_key, "typ") != 0 ||
      strcasecmp(candidate_type, "host") != 0) {
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

static int rtc_test_build_host_candidate(char *out, uint16_t out_cap, uint16_t port,
                                         uint32_t priority) {
  if (!out || out_cap == 0u || port == 0u) {
    return 0;
  }
  return snprintf(out, out_cap, "candidate:9 1 udp %u 127.0.0.1 %u typ host",
                  priority, port) > 0;
}

static int rtc_test_stun_is_binding_request(const uint8_t *buf, uint16_t len) {
  if (!buf || len < 20u) {
    return 0;
  }
  if (rtc_test_stun_read_u16_be(&buf[0]) != RTC_TEST_STUN_TYPE_BINDING_REQUEST) {
    return 0;
  }
  if (rtc_test_stun_read_u32_be(&buf[4]) != RTC_TEST_STUN_MAGIC_COOKIE) {
    return 0;
  }
  return 1;
}

static int rtc_test_stun_get_transaction_id(const uint8_t *buf, uint16_t len,
                                            uint8_t out_tid[12]) {
  if (!buf || len < 20u || !out_tid) {
    return 0;
  }
  memcpy(out_tid, &buf[8], 12u);
  return 1;
}

static int rtc_test_stun_build_binding_response(const uint8_t transaction_id[12],
                                                const char *username,
                                                const char *password,
                                                const uint8_t mapped_ip[4],
                                                uint16_t mapped_port,
                                                uint8_t *out_buf,
                                                uint16_t *io_len) {
  uint16_t offset = 20u;
  uint16_t msg_len = 0u;
  uint16_t pre_fp_len = 0u;
  uint8_t xor_addr[8];
  uint8_t tie[8];
  uint8_t hmac[20];
  uint32_t fingerprint;

  if (!transaction_id || !username || !password || !mapped_ip || !out_buf || !io_len) {
    return 0;
  }
  if (*io_len < 64u) {
    return 0;
  }
  memset(out_buf, 0, *io_len);

  rtc_test_stun_write_u16_be(&out_buf[0], RTC_TEST_STUN_TYPE_BINDING_RESPONSE);
  rtc_test_stun_write_u16_be(&out_buf[2], 0u);
  rtc_test_stun_write_u32_be(&out_buf[4], RTC_TEST_STUN_MAGIC_COOKIE);
  memcpy(&out_buf[8], transaction_id, 12u);

  memset(xor_addr, 0, sizeof(xor_addr));
  xor_addr[1] = 0x01u;
  xor_addr[2] = (uint8_t)(((mapped_port >> 8) & 0xFFu) ^ 0x21u);
  xor_addr[3] = (uint8_t)((mapped_port & 0xFFu) ^ 0x12u);
  xor_addr[4] = (uint8_t)(mapped_ip[0] ^ 0x21u);
  xor_addr[5] = (uint8_t)(mapped_ip[1] ^ 0x12u);
  xor_addr[6] = (uint8_t)(mapped_ip[2] ^ 0xA4u);
  xor_addr[7] = (uint8_t)(mapped_ip[3] ^ 0x42u);
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_XOR_MAPPED_ADDRESS, xor_addr,
                                 (uint16_t)sizeof(xor_addr))) {
    return 0;
  }
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset, RTC_TEST_STUN_ATTR_USERNAME,
                                 (const uint8_t *)username,
                                 (uint16_t)strlen(username))) {
    return 0;
  }
  memset(tie, 0, sizeof(tie));
  rtc_test_stun_write_u32_be(&tie[4], 0xA0B0C0D0u);
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_ICE_CONTROLLED, tie,
                                 (uint16_t)sizeof(tie))) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 24u);
  rtc_test_stun_write_u16_be(&out_buf[2], msg_len);
  if (!rtc_test_stun_hmac_sha1((const uint8_t *)password, (uint16_t)strlen(password),
                               out_buf, offset, hmac)) {
    return 0;
  }
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_MESSAGE_INTEGRITY, hmac, 20u)) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 8u);
  rtc_test_stun_write_u16_be(&out_buf[2], msg_len);
  pre_fp_len = offset;
  fingerprint = rtc_test_stun_crc32_ieee(out_buf, pre_fp_len) ^ 0x5354554Eu;
  rtc_test_stun_write_u32_be(hmac, fingerprint);
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_FINGERPRINT, hmac, 4u)) {
    return 0;
  }

  rtc_test_stun_write_u16_be(&out_buf[2], (uint16_t)(offset - 20u));
  *io_len = offset;
  return 1;
}

static RTC_TEST_MAYBE_UNUSED int rtc_test_stun_build_binding_request(
    const uint8_t transaction_id[12], const char *username, const char *password,
    uint32_t priority, uint8_t *out_buf, uint16_t *io_len) {
  uint16_t offset = 20u;
  uint16_t msg_len = 0u;
  uint16_t pre_fp_len = 0u;
  uint8_t value4[4];
  uint8_t tie[8];
  uint8_t hmac[20];
  uint32_t fingerprint;

  if (!transaction_id || !username || !password || !out_buf || !io_len) {
    return 0;
  }
  if (*io_len < 64u) {
    return 0;
  }
  memset(out_buf, 0, *io_len);

  rtc_test_stun_write_u16_be(&out_buf[0], RTC_TEST_STUN_TYPE_BINDING_REQUEST);
  rtc_test_stun_write_u16_be(&out_buf[2], 0u);
  rtc_test_stun_write_u32_be(&out_buf[4], RTC_TEST_STUN_MAGIC_COOKIE);
  memcpy(&out_buf[8], transaction_id, 12u);

  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset, RTC_TEST_STUN_ATTR_USERNAME,
                                 (const uint8_t *)username,
                                 (uint16_t)strlen(username))) {
    return 0;
  }
  rtc_test_stun_write_u32_be(value4, priority);
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset, RTC_TEST_STUN_ATTR_PRIORITY,
                                 value4, 4u)) {
    return 0;
  }
  memset(tie, 0, sizeof(tie));
  rtc_test_stun_write_u32_be(&tie[4], 0x01020304u);
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_ICE_CONTROLLED, tie,
                                 (uint16_t)sizeof(tie))) {
    return 0;
  }
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_USE_CANDIDATE, NULL, 0u)) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 24u);
  rtc_test_stun_write_u16_be(&out_buf[2], msg_len);
  if (!rtc_test_stun_hmac_sha1((const uint8_t *)password, (uint16_t)strlen(password),
                               out_buf, offset, hmac)) {
    return 0;
  }
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_MESSAGE_INTEGRITY, hmac, 20u)) {
    return 0;
  }

  msg_len = (uint16_t)((offset - 20u) + 8u);
  rtc_test_stun_write_u16_be(&out_buf[2], msg_len);
  pre_fp_len = offset;
  fingerprint = rtc_test_stun_crc32_ieee(out_buf, pre_fp_len) ^ 0x5354554Eu;
  rtc_test_stun_write_u32_be(hmac, fingerprint);
  if (!rtc_test_stun_append_attr(out_buf, *io_len, &offset,
                                 RTC_TEST_STUN_ATTR_FINGERPRINT, hmac, 4u)) {
    return 0;
  }

  rtc_test_stun_write_u16_be(&out_buf[2], (uint16_t)(offset - 20u));
  *io_len = offset;
  return 1;
}

#endif  // RTC_TEST_STUN_HELPERS_H_
