#include "test_common.h"

#include <stdint.h>
#include <string.h>

#include "stun.h"

#if RTC_WITH_MBEDTLS
#include <mbedtls/md.h>
#endif

static void wr16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)v;
}
static void wr32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static size_t pad4(size_t n) { return (n + 3u) & ~3u; }

static int append_attr(uint8_t *buf, size_t cap, size_t *off, uint16_t type, const uint8_t *v, uint16_t len) {
  size_t padded = pad4((size_t)len);
  if (!buf || !off || *off + 4u + padded > cap) {
    return -1;
  }
  wr16(buf + *off, type);
  *off += 2;
  wr16(buf + *off, len);
  *off += 2;
  if (len > 0 && v) {
    memcpy(buf + *off, v, len);
  }
  if (padded > (size_t)len) {
    memset(buf + *off + len, 0, padded - (size_t)len);
  }
  *off += padded;
  return 0;
}

#if RTC_WITH_MBEDTLS
static int find_attr(const uint8_t *buf,
                     size_t len,
                     uint16_t type,
                     size_t *out_attr_off,
                     uint16_t *out_attr_len,
                     size_t *out_msg_end) {
  size_t off;
  size_t end;
  if (!buf || len < 20) {
    return -1;
  }
  end = 20u + (((size_t)buf[2] << 8) | (size_t)buf[3]);
  if (end > len) {
    return -1;
  }
  off = 20;
  while (off + 4u <= end) {
    uint16_t t = (uint16_t)(((uint16_t)buf[off] << 8) | buf[off + 1]);
    uint16_t l = (uint16_t)(((uint16_t)buf[off + 2] << 8) | buf[off + 3]);
    size_t padded = pad4((size_t)l);
    if (off + 4u + padded > end) {
      return -1;
    }
    if (t == type) {
      if (out_attr_off) {
        *out_attr_off = off;
      }
      if (out_attr_len) {
        *out_attr_len = l;
      }
      if (out_msg_end) {
        *out_msg_end = end;
      }
      return 0;
    }
    off += 4u + padded;
  }
  if (out_msg_end) {
    *out_msg_end = end;
  }
  return 1;
}

static int sign_message_integrity_rfc5389(uint8_t *buf,
                                          size_t len,
                                          size_t mi_attr_off,
                                          const char *key) {
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  unsigned char digest[20];
  uint8_t tmp[2048];
  size_t mi_end;
  if (!buf || !key || !key[0] || !md) {
    return -1;
  }
  mi_end = mi_attr_off + 24u;
  if (len < mi_end || mi_attr_off < 20u || mi_attr_off > sizeof(tmp)) {
    return -1;
  }
  memcpy(tmp, buf, mi_attr_off);
  wr16(tmp + 2, (uint16_t)(mi_end - 20u));
  if (mbedtls_md_hmac(md, (const unsigned char *)key, strlen(key), tmp, mi_attr_off, digest) != 0) {
    return -1;
  }
  memcpy(buf + mi_attr_off + 4u, digest, 20);
  return 0;
}

static int verify_message_integrity_rfc5389(const uint8_t *buf, size_t len, const char *key) {
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  unsigned char digest[20];
  uint8_t tmp[2048];
  size_t mi_attr_off = 0;
  size_t msg_end = 0;
  uint16_t mi_attr_len = 0;
  size_t mi_end = 0;
  int rc;
  if (!buf || !key || !key[0] || !md) {
    return -1;
  }
  rc = find_attr(buf, len, 0x0008, &mi_attr_off, &mi_attr_len, &msg_end);
  if (rc != 0) {
    return rc;
  }
  if (mi_attr_len != 20) {
    return -1;
  }
  mi_end = mi_attr_off + 24u;
  if (mi_end > msg_end || mi_attr_off < 20u || mi_attr_off > sizeof(tmp)) {
    return -1;
  }
  memcpy(tmp, buf, mi_attr_off);
  wr16(tmp + 2, (uint16_t)(mi_end - 20u));
  if (mbedtls_md_hmac(md, (const unsigned char *)key, strlen(key), tmp, mi_attr_off, digest) != 0) {
    return -1;
  }
  return memcmp(digest, buf + mi_attr_off + 4u, 20) == 0 ? 1 : 0;
}
#endif

static size_t build_request_with_auth(uint8_t *out,
                                      size_t cap,
                                      const uint8_t txn[12],
                                      const char *username,
                                      const char *key,
                                      int with_use_candidate) {
  size_t off = 20;
  size_t mi_value_off = 0;
  if (!out || !txn || cap < 20) {
    return 0;
  }
  memset(out, 0, cap);
  wr16(out, 0x0001);
  wr16(out + 2, 0);
  wr32(out + 4, RTC_STUN_MAGIC_COOKIE);
  memcpy(out + 8, txn, 12);

  if (username && username[0]) {
    if (append_attr(out, cap, &off, 0x0006, (const uint8_t *)username, (uint16_t)strlen(username)) != 0) {
      return 0;
    }
  }
  if (with_use_candidate) {
    if (append_attr(out, cap, &off, 0x0025, 0, 0) != 0) {
      return 0;
    }
  }

  mi_value_off = off + 4u;
  if (append_attr(out, cap, &off, 0x0008, 0, 20) != 0) {
    return 0;
  }
  wr16(out + 2, (uint16_t)(off - 20u));

#if RTC_WITH_MBEDTLS
  if (key && key[0]) {
    if (sign_message_integrity_rfc5389(out, off, mi_value_off - 4u, key) != 0) {
      return 0;
    }
  }
#else
  (void)key;
#endif
  return off;
}

int main(void) {
  uint8_t req[128];
  uint8_t txn[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  size_t req_len = 0;

  ASSERT_EQ_INT(0, rtc_stun_build_binding_request(txn, req, sizeof(req), &req_len));
  ASSERT_EQ_INT(20, (int)req_len);
  ASSERT_TRUE(rtc_stun_is_message(req, req_len));
  {
    uint8_t txn2[12] = {0};
    ASSERT_TRUE(rtc_stun_is_binding_request(req, req_len, txn2));
    ASSERT_TRUE(memcmp(txn, txn2, 12) == 0);
  }

  {
    uint8_t resp[128];
    char ip[64] = {0};
    uint16_t port = 0;
    size_t resp_len = 0;
    ASSERT_EQ_INT(0, rtc_stun_build_binding_success_response(txn, "10.11.12.13", 50000, 0, resp, sizeof(resp), &resp_len));
    ASSERT_TRUE(rtc_stun_is_message(resp, resp_len));
    ASSERT_EQ_INT(0, rtc_stun_parse_binding_response(resp, resp_len, txn, ip, sizeof(ip), &port));
    ASSERT_TRUE(strcmp(ip, "10.11.12.13") == 0);
    ASSERT_EQ_INT(50000, port);
    ASSERT_EQ_INT(0, rtc_stun_has_use_candidate(resp, resp_len));
    ASSERT_EQ_INT(0, rtc_stun_has_message_integrity(resp, resp_len));
  }

  {
    uint8_t resp[128];
    char ip[64] = {0};
    uint16_t port = 0;
    size_t resp_len = 0;
    ASSERT_EQ_INT(0,
                  rtc_stun_build_binding_success_response(
                      txn, "10.11.12.13", 50000, "k123", resp, sizeof(resp), &resp_len));
    ASSERT_TRUE(rtc_stun_is_message(resp, resp_len));
    ASSERT_EQ_INT(0, rtc_stun_parse_binding_response(resp, resp_len, txn, ip, sizeof(ip), &port));
    ASSERT_TRUE(strcmp(ip, "10.11.12.13") == 0);
    ASSERT_EQ_INT(50000, port);
    ASSERT_EQ_INT(1, rtc_stun_has_message_integrity(resp, resp_len));
#if RTC_WITH_MBEDTLS
    ASSERT_EQ_INT(1, verify_message_integrity_rfc5389(resp, resp_len, "k123"));
    ASSERT_EQ_INT(1, rtc_stun_verify_message_integrity(resp, resp_len, "k123"));
    ASSERT_EQ_INT(0, verify_message_integrity_rfc5389(resp, resp_len, "wrong"));
#else
    ASSERT_EQ_INT(-2, rtc_stun_verify_message_integrity(resp, resp_len, "k123"));
#endif
  }

  {
    uint8_t req2[128] = {0};
    char uname[64] = {0};
    const char *u = "abc:def";
    req_len = build_request_with_auth(req2, sizeof(req2), txn, u, "k123", 1);
    ASSERT_TRUE(req_len > 0);
    ASSERT_EQ_INT(0, rtc_stun_parse_username(req2, req_len, uname, sizeof(uname)));
    ASSERT_TRUE(strcmp(uname, u) == 0);
    ASSERT_EQ_INT(1, rtc_stun_has_use_candidate(req2, req_len));
    ASSERT_EQ_INT(1, rtc_stun_has_message_integrity(req2, req_len));
#if RTC_WITH_MBEDTLS
    ASSERT_EQ_INT(1, verify_message_integrity_rfc5389(req2, req_len, "k123"));
    ASSERT_EQ_INT(1, rtc_stun_verify_message_integrity(req2, req_len, "k123"));
    ASSERT_EQ_INT(0, rtc_stun_verify_message_integrity(req2, req_len, "wrong"));
    req2[24] ^= 0x01;
    ASSERT_EQ_INT(0, verify_message_integrity_rfc5389(req2, req_len, "k123"));
    ASSERT_EQ_INT(0, rtc_stun_verify_message_integrity(req2, req_len, "k123"));
#else
    ASSERT_EQ_INT(-2, rtc_stun_verify_message_integrity(req2, req_len, "k123"));
#endif
  }

  {
    uint8_t req3[96] = {0};
    req_len = build_request_with_auth(req3, sizeof(req3), txn, "x:y", 0, 0);
    ASSERT_TRUE(req_len > 0);
    ASSERT_EQ_INT(0, rtc_stun_has_use_candidate(req3, req_len));
    ASSERT_EQ_INT(1, rtc_stun_has_message_integrity(req3, req_len));
#if RTC_WITH_MBEDTLS
    ASSERT_EQ_INT(0, verify_message_integrity_rfc5389(req3, req_len, "k123"));
    ASSERT_EQ_INT(0, rtc_stun_verify_message_integrity(req3, req_len, "k123"));
#else
    ASSERT_EQ_INT(-2, rtc_stun_verify_message_integrity(req3, req_len, "k123"));
#endif
  }

  {
    uint8_t bad[128] = {0};
    size_t off = 0;
    wr16(bad + off, 0x0001);
    off += 2;
    wr16(bad + off, 0);
    off += 2;
    wr32(bad + off, RTC_STUN_MAGIC_COOKIE);
    off += 4;
    memcpy(bad + off, txn, 12);
    off += 12;
    ASSERT_EQ_INT(0, append_attr(bad, sizeof(bad), &off, 0x0008, 0, 4));
    wr16(bad + 2, (uint16_t)(off - 20u));
    ASSERT_EQ_INT(1, rtc_stun_has_message_integrity(bad, off));
    ASSERT_EQ_INT(-1, rtc_stun_verify_message_integrity(bad, off, "k123"));
  }

  {
    uint8_t bad2[24] = {0};
    size_t off = 0;
    wr16(bad2 + off, 0x0001);
    off += 2;
    wr16(bad2 + off, 8);
    off += 2;
    wr32(bad2 + off, RTC_STUN_MAGIC_COOKIE);
    off += 4;
    memcpy(bad2 + off, txn, 12);
    off += 12;
    wr16(bad2 + off, 0x0025);
    off += 2;
    wr16(bad2 + off, 4);
    off += 2;
    ASSERT_EQ_INT(0, rtc_stun_has_use_candidate(bad2, off));
    ASSERT_EQ_INT(0, rtc_stun_has_use_candidate((const uint8_t *)"not-stun", 8));
    ASSERT_EQ_INT(0, rtc_stun_has_message_integrity((const uint8_t *)"not-stun", 8));
  }

  return 0;
}
