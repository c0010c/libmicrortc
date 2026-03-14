#include "stun.h"

#include <stdio.h>
#include <string.h>

#if RTC_WITH_MBEDTLS
#include <mbedtls/md.h>
#endif

static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
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

static int parse_ipv4_u32(const char *ip, uint32_t *out) {
  unsigned a, b, c, d;
  if (!ip || !out) {
    return -1;
  }
  if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
    return -1;
  }
  if (a > 255u || b > 255u || c > 255u || d > 255u) {
    return -1;
  }
  *out = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
  return 0;
}

static uint32_t crc32_ieee(const uint8_t *buf, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  size_t i;
  if (!buf) {
    return 0;
  }
  for (i = 0; i < len; ++i) {
    uint32_t c = (uint32_t)(buf[i]);
    int b;
    crc ^= c;
    for (b = 0; b < 8; ++b) {
      if (crc & 1u) {
        crc = (crc >> 1) ^ 0xEDB88320u;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

int rtc_stun_is_message(const uint8_t *buf, size_t len) {
  if (!buf || len < 20) {
    return 0;
  }
  if ((buf[0] & 0xC0u) != 0) {
    return 0;
  }
  return rd32(buf + 4) == RTC_STUN_MAGIC_COOKIE;
}

int rtc_stun_is_binding_request(const uint8_t *buf, size_t len, uint8_t out_txn[12]) {
  if (!rtc_stun_is_message(buf, len)) {
    return 0;
  }
  if (rd16(buf) != 0x0001) {
    return 0;
  }
  if (out_txn) {
    memcpy(out_txn, buf + 8, 12);
  }
  return 1;
}

static int stun_get_message_end(const uint8_t *buf, size_t len, size_t *out_end) {
  size_t end = 0;
  if (!buf || !out_end || !rtc_stun_is_message(buf, len)) {
    return -1;
  }
  end = 20u + (size_t)rd16(buf + 2);
  if (end > len) {
    return -1;
  }
  *out_end = end;
  return 0;
}

static int stun_find_attr(const uint8_t *buf,
                          size_t len,
                          uint16_t attr_type,
                          size_t *out_attr_off,
                          uint16_t *out_attr_len,
                          size_t *out_msg_end) {
  size_t off = 20;
  size_t end = 0;
  if (stun_get_message_end(buf, len, &end) != 0) {
    return -1;
  }
  while (off + 4 <= end) {
    uint16_t t = rd16(buf + off);
    uint16_t l = rd16(buf + off + 2);
    size_t padded = (size_t)((l + 3u) & ~3u);
    if (off + 4 + padded > end) {
      return -1;
    }
    if (t == attr_type) {
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
    off += 4 + padded;
  }
  if (out_msg_end) {
    *out_msg_end = end;
  }
  return 1;
}

int rtc_stun_has_use_candidate(const uint8_t *buf, size_t len) {
  return stun_find_attr(buf, len, 0x0025, 0, 0, 0) == 0 ? 1 : 0;
}

int rtc_stun_has_message_integrity(const uint8_t *buf, size_t len) {
  return stun_find_attr(buf, len, 0x0008, 0, 0, 0) == 0 ? 1 : 0;
}

int rtc_stun_parse_username(const uint8_t *buf, size_t len, char *out, size_t out_len) {
  size_t attr_off = 0;
  uint16_t attr_len = 0;
  int rc;
  if (!buf || !out || out_len == 0) {
    return -1;
  }
  rc = stun_find_attr(buf, len, 0x0006, &attr_off, &attr_len, 0);
  if (rc != 0) {
    return -1;
  }
  {
    size_t n = attr_len;
    if (n >= out_len) {
      n = out_len - 1;
    }
    memcpy(out, buf + attr_off + 4, n);
    out[n] = '\0';
  }
  return 0;
}

int rtc_stun_verify_message_integrity(const uint8_t *buf, size_t len, const char *key) {
  if (!buf || !key || !key[0]) {
    return 0;
  }
#if !RTC_WITH_MBEDTLS
  (void)len;
  return -2;
#else
  size_t mi_off = 0;
  size_t msg_end = 0;
  size_t mi_end = 0;
  uint16_t mi_len = 0;
  int rc;
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  unsigned char digest[20];
  uint8_t tmp[2048];
  if (!md) {
    return -1;
  }
  rc = stun_find_attr(buf, len, 0x0008, &mi_off, &mi_len, &msg_end);
  if (rc == 1) {
    return 0;
  }
  if (rc != 0) {
    return -1;
  }
  if (mi_len != 20) {
    return -1;
  }
  mi_end = mi_off + 4u + (size_t)mi_len;
  if (mi_end > msg_end || mi_end > len || mi_off > sizeof(tmp) || mi_off < 20) {
    return -1;
  }
  memcpy(tmp, buf, mi_off);
  /* RFC 5389 section 15.4: the header length is adjusted to include MESSAGE-INTEGRITY,
     but the HMAC input stops before the MESSAGE-INTEGRITY attribute itself. */
  wr16(tmp + 2, (uint16_t)(mi_end - 20u));
  if (mbedtls_md_hmac(md, (const unsigned char *)key, strlen(key), tmp, mi_off, digest) != 0) {
    return -1;
  }
  return memcmp(digest, buf + mi_off + 4u, 20) == 0 ? 1 : 0;
#endif
}

int rtc_stun_build_binding_request(const uint8_t txn[12], uint8_t *out, size_t cap, size_t *written) {
  if (!txn || !out || cap < 20 || !written) {
    return -1;
  }
  memset(out, 0, 20);
  wr16(out, 0x0001);
  wr16(out + 2, 0);
  wr32(out + 4, RTC_STUN_MAGIC_COOKIE);
  memcpy(out + 8, txn, 12);
  *written = 20;
  return 0;
}

int rtc_stun_build_binding_success_response(const uint8_t req_txn[12],
                                            const char *mapped_ip,
                                            uint16_t mapped_port,
                                            const char *integrity_key,
                                            uint8_t *out,
                                            size_t cap,
                                            size_t *written) {
  size_t off = 0;
  uint32_t ipv4 = 0;
  size_t fp_off = 0;
  uint16_t body_len;

  if (!req_txn || !mapped_ip || mapped_port == 0 || !out || !written) {
    return -1;
  }
  if (parse_ipv4_u32(mapped_ip, &ipv4) != 0) {
    return -1;
  }
  if (cap < 40) {
    return -1;
  }

  memset(out, 0, cap);
  wr16(out + off, 0x0101);
  off += 2;
  wr16(out + off, 0);
  off += 2;
  wr32(out + off, RTC_STUN_MAGIC_COOKIE);
  off += 4;
  memcpy(out + off, req_txn, 12);
  off += 12;

  wr16(out + off, 0x0020);
  off += 2;
  wr16(out + off, 8);
  off += 2;
  out[off + 0] = 0;
  out[off + 1] = 0x01;
  wr16(out + off + 2, (uint16_t)(mapped_port ^ (RTC_STUN_MAGIC_COOKIE >> 16)));
  wr32(out + off + 4, (uint32_t)(ipv4 ^ RTC_STUN_MAGIC_COOKIE));
  off += 8;

  if (integrity_key && integrity_key[0]) {
#if RTC_WITH_MBEDTLS
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    unsigned char digest[20];
    size_t mi_off = 0;
    if (!md) {
      return -1;
    }
    if (off + 24 > cap) {
      return -1;
    }
    mi_off = off;
    wr16(out + off, 0x0008);
    off += 2;
    wr16(out + off, 20);
    off += 2;
    memset(out + off, 0, 20);
    off += 20;
    body_len = (uint16_t)(off - 20);
    wr16(out + 2, body_len);
    /* RFC 5389 section 15.4: the length includes MESSAGE-INTEGRITY, but the signed bytes stop
       before the MESSAGE-INTEGRITY attribute header. */
    if (mbedtls_md_hmac(md,
                        (const unsigned char *)integrity_key,
                        strlen(integrity_key),
                        out,
                        mi_off,
                        digest) != 0) {
      return -1;
    }
    memcpy(out + mi_off + 4, digest, 20);
#else
    body_len = (uint16_t)(off - 20);
    wr16(out + 2, body_len);
#endif
  } else {
    body_len = (uint16_t)(off - 20);
    wr16(out + 2, body_len);
  }

  if (off + 8 > cap) {
    return -1;
  }
  fp_off = off;
  wr16(out + off, 0x8028);
  off += 2;
  wr16(out + off, 4);
  off += 2;
  wr32(out + off, 0);
  off += 4;

  body_len = (uint16_t)(off - 20);
  wr16(out + 2, body_len);
  {
    uint32_t fp = crc32_ieee(out, fp_off) ^ 0x5354554eu;
    wr32(out + fp_off + 4, fp);
  }

  *written = off;
  return 0;
}

int rtc_stun_parse_binding_response(const uint8_t *buf,
                                    size_t len,
                                    const uint8_t txn[12],
                                    char *out_ip,
                                    size_t ip_len,
                                    uint16_t *out_port) {
  size_t off;
  if (!buf || len < 20 || !txn || !out_ip || !out_port) {
    return -1;
  }
  if (rd16(buf) != 0x0101 || rd32(buf + 4) != RTC_STUN_MAGIC_COOKIE) {
    return -1;
  }
  if (memcmp(buf + 8, txn, 12) != 0) {
    return -1;
  }
  off = 20;
  while (off + 4 <= len) {
    uint16_t t = rd16(buf + off);
    uint16_t l = rd16(buf + off + 2);
    size_t padded = (size_t)((l + 3) & ~3);
    if (off + 4 + padded > len) {
      return -1;
    }
    if (t == 0x0020 && l >= 8) {
      const uint8_t *v = buf + off + 4;
      uint8_t fam = v[1];
      if (fam != 0x01) {
        return -1;
      }
      uint16_t xport = rd16(v + 2);
      uint16_t port = (uint16_t)(xport ^ (RTC_STUN_MAGIC_COOKIE >> 16));
      uint32_t xaddr = rd32(v + 4);
      uint32_t addr = xaddr ^ RTC_STUN_MAGIC_COOKIE;
      unsigned a = (addr >> 24) & 0xFFu;
      unsigned b = (addr >> 16) & 0xFFu;
      unsigned c = (addr >> 8) & 0xFFu;
      unsigned d = addr & 0xFFu;
      (void)snprintf(out_ip, ip_len, "%u.%u.%u.%u", a, b, c, d);
      *out_port = port;
      return 0;
    }
    off += 4 + padded;
  }
  return -1;
}
