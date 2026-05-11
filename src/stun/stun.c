#include "stun/stun.h"

#include <stdio.h>
#include <string.h>

#define RTC_STUN_HEADER_BYTES 20u
#define RTC_STUN_ATTR_USERNAME 0x0006u
#define RTC_STUN_ATTR_MESSAGE_INTEGRITY 0x0008u
#define RTC_STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020u
#define RTC_STUN_ATTR_PRIORITY 0x0024u
#define RTC_STUN_ATTR_ERROR_CODE 0x0009u
#define RTC_STUN_ATTR_USE_CANDIDATE 0x0025u
#define RTC_STUN_ATTR_FINGERPRINT 0x8028u
#define RTC_STUN_ATTR_ICE_CONTROLLED 0x8029u
#define RTC_STUN_ATTR_ICE_CONTROLLING 0x802au
#define RTC_STUN_FAMILY_IPV4 0x01u
#define RTC_STUN_FAMILY_IPV6 0x02u
#define RTC_STUN_MESSAGE_INTEGRITY_BYTES 20u
#define RTC_STUN_FINGERPRINT_XOR 0x5354554eu

static uint16_t rtc_stun_read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t rtc_stun_read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static uint64_t rtc_stun_read_u64(const uint8_t *data)
{
    return ((uint64_t)rtc_stun_read_u32(data) << 32) |
           rtc_stun_read_u32(data + 4);
}

static void rtc_stun_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void rtc_stun_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void rtc_stun_write_u64(uint8_t *data, uint64_t value)
{
    rtc_stun_write_u32(data, (uint32_t)(value >> 32));
    rtc_stun_write_u32(data + 4, (uint32_t)value);
}

typedef struct rtc_sha1_t {
    uint32_t h[5];
    uint64_t len;
    uint8_t block[64];
    size_t block_len;
} rtc_sha1_t;

static uint32_t rtc_rotl32(uint32_t value, unsigned bits)
{
    return (value << bits) | (value >> (32u - bits));
}

static void rtc_sha1_init(rtc_sha1_t *ctx)
{
    ctx->h[0] = 0x67452301u;
    ctx->h[1] = 0xefcdab89u;
    ctx->h[2] = 0x98badcfeu;
    ctx->h[3] = 0x10325476u;
    ctx->h[4] = 0xc3d2e1f0u;
    ctx->len = 0;
    ctx->block_len = 0;
}

static void rtc_sha1_process(rtc_sha1_t *ctx, const uint8_t block[64])
{
    uint32_t w[80];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t i;

    for (i = 0; i < 16u; ++i) {
        w[i] = rtc_stun_read_u32(block + (size_t)i * 4u);
    }
    for (i = 16u; i < 80u; ++i) {
        w[i] = rtc_rotl32(w[i - 3u] ^ w[i - 8u] ^ w[i - 14u] ^ w[i - 16u], 1);
    }

    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];

    for (i = 0; i < 80u; ++i) {
        uint32_t f;
        uint32_t k;
        uint32_t temp;

        if (i < 20u) {
            f = (b & c) | ((~b) & d);
            k = 0x5a827999u;
        } else if (i < 40u) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1u;
        } else if (i < 60u) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcu;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6u;
        }
        temp = rtc_rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rtc_rotl32(b, 30);
        b = a;
        a = temp;
    }

    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
}

static void rtc_sha1_update(rtc_sha1_t *ctx, const uint8_t *data, size_t len)
{
    size_t i;

    ctx->len += (uint64_t)len * 8u;
    for (i = 0; i < len; ++i) {
        ctx->block[ctx->block_len++] = data[i];
        if (ctx->block_len == sizeof(ctx->block)) {
            rtc_sha1_process(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static void rtc_sha1_final(rtc_sha1_t *ctx, uint8_t out[20])
{
    uint64_t bit_len = ctx->len;
    size_t i;

    ctx->block[ctx->block_len++] = 0x80u;
    if (ctx->block_len > 56u) {
        while (ctx->block_len < 64u) {
            ctx->block[ctx->block_len++] = 0;
        }
        rtc_sha1_process(ctx, ctx->block);
        ctx->block_len = 0;
    }
    while (ctx->block_len < 56u) {
        ctx->block[ctx->block_len++] = 0;
    }
    for (i = 0; i < 8u; ++i) {
        ctx->block[56u + i] = (uint8_t)(bit_len >> (56u - i * 8u));
    }
    rtc_sha1_process(ctx, ctx->block);
    for (i = 0; i < 5u; ++i) {
        rtc_stun_write_u32(out + i * 4u, ctx->h[i]);
    }
}

static void rtc_hmac_sha1(const uint8_t *key, size_t key_len,
                          const uint8_t *data, size_t data_len,
                          uint8_t out[20])
{
    uint8_t key_block[64];
    uint8_t inner_pad[64];
    uint8_t outer_pad[64];
    uint8_t inner_hash[20];
    rtc_sha1_t sha1;
    size_t i;

    memset(key_block, 0, sizeof(key_block));
    if (key_len > sizeof(key_block)) {
        rtc_sha1_init(&sha1);
        rtc_sha1_update(&sha1, key, key_len);
        rtc_sha1_final(&sha1, key_block);
    } else if (key != 0 && key_len > 0u) {
        memcpy(key_block, key, key_len);
    }

    for (i = 0; i < sizeof(key_block); ++i) {
        inner_pad[i] = (uint8_t)(key_block[i] ^ 0x36u);
        outer_pad[i] = (uint8_t)(key_block[i] ^ 0x5cu);
    }

    rtc_sha1_init(&sha1);
    rtc_sha1_update(&sha1, inner_pad, sizeof(inner_pad));
    rtc_sha1_update(&sha1, data, data_len);
    rtc_sha1_final(&sha1, inner_hash);

    rtc_sha1_init(&sha1);
    rtc_sha1_update(&sha1, outer_pad, sizeof(outer_pad));
    rtc_sha1_update(&sha1, inner_hash, sizeof(inner_hash));
    rtc_sha1_final(&sha1, out);
}

static uint32_t rtc_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffu;
    size_t i;

    for (i = 0; i < len; ++i) {
        unsigned bit;
        crc ^= data[i];
        for (bit = 0; bit < 8u; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int)(crc & 1u));
        }
    }
    return ~crc;
}

static size_t rtc_stun_attr_padded_len(size_t attr_len)
{
    return (attr_len + 3u) & ~(size_t)3u;
}

static rtc_status_t rtc_stun_write_attr(uint8_t *out, size_t out_capacity,
                                        size_t *pos, uint16_t attr_type,
                                        const uint8_t *value,
                                        uint16_t value_len)
{
    size_t padded_len = rtc_stun_attr_padded_len(value_len);

    if (*pos + 4u + padded_len > out_capacity) {
        return RTC_STATUS_CAPACITY;
    }
    rtc_stun_write_u16(out + *pos, attr_type);
    rtc_stun_write_u16(out + *pos + 2u, value_len);
    if (value_len > 0u && value != 0) {
        memcpy(out + *pos + 4u, value, value_len);
    }
    if (padded_len > value_len) {
        memset(out + *pos + 4u + value_len, 0, padded_len - value_len);
    }
    *pos += 4u + padded_len;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_stun_write_message_integrity(
    uint8_t *out, size_t out_capacity, size_t *pos, const char *pwd,
    size_t pwd_len)
{
    uint8_t digest[RTC_STUN_MESSAGE_INTEGRITY_BYTES];
    size_t attr_pos;

    if (pwd == 0 || pwd_len == 0u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (*pos + 24u > out_capacity) {
        return RTC_STATUS_CAPACITY;
    }
    attr_pos = *pos;
    rtc_stun_write_u16(out + attr_pos, RTC_STUN_ATTR_MESSAGE_INTEGRITY);
    rtc_stun_write_u16(out + attr_pos + 2u, RTC_STUN_MESSAGE_INTEGRITY_BYTES);
    rtc_stun_write_u16(out + 2u,
                       (uint16_t)(attr_pos + 24u - RTC_STUN_HEADER_BYTES));
    rtc_hmac_sha1((const uint8_t *)pwd, pwd_len, out, attr_pos, digest);
    memcpy(out + attr_pos + 4u, digest, sizeof(digest));
    *pos += 24u;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_stun_write_fingerprint(uint8_t *out,
                                               size_t out_capacity,
                                               size_t *pos)
{
    uint32_t fingerprint;

    if (*pos + 8u > out_capacity) {
        return RTC_STATUS_CAPACITY;
    }
    rtc_stun_write_u16(out + *pos, RTC_STUN_ATTR_FINGERPRINT);
    rtc_stun_write_u16(out + *pos + 2u, 4u);
    rtc_stun_write_u16(out + 2u,
                       (uint16_t)(*pos + 8u - RTC_STUN_HEADER_BYTES));
    fingerprint = rtc_crc32(out, *pos) ^ RTC_STUN_FINGERPRINT_XOR;
    rtc_stun_write_u32(out + *pos + 4u, fingerprint);
    *pos += 8u;
    return RTC_STATUS_OK;
}

static int rtc_stun_parse_ipv4(const char *ip, uint32_t *out)
{
    unsigned a;
    unsigned b;
    unsigned c;
    unsigned d;
    char tail;

    if (ip == 0 || out == 0) {
        return 0;
    }
    if (sscanf(ip, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) {
        return 0;
    }
    if (a > 255u || b > 255u || c > 255u || d > 255u) {
        return 0;
    }
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 1;
}

int rtc_stun_is_datagram(const uint8_t *data, size_t len)
{
    if (data == 0 || len < RTC_STUN_HEADER_BYTES) {
        return 0;
    }
    if ((data[0] & 0xC0u) != 0) {
        return 0;
    }
    return rtc_stun_read_u32(data + 4) == RTC_STUN_MAGIC_COOKIE;
}

rtc_status_t rtc_stun_write_binding_request(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    size_t *out_len)
{
    if (out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_len = RTC_STUN_HEADER_BYTES;
    if (out == 0 || transaction_id == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (out_capacity < RTC_STUN_HEADER_BYTES) {
        return RTC_STATUS_CAPACITY;
    }

    rtc_stun_write_u16(out, RTC_STUN_BINDING_REQUEST);
    rtc_stun_write_u16(out + 2, 0);
    rtc_stun_write_u32(out + 4, RTC_STUN_MAGIC_COOKIE);
    memcpy(out + 8, transaction_id, RTC_STUN_TRANSACTION_ID_BYTES);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_stun_write_ice_binding_request(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    int use_candidate, int controlling, uint64_t tie_breaker,
    size_t *out_len)
{
    size_t pos;
    uint16_t role_attr;

    if (out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_len = 0;
    if (out == 0 || transaction_id == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    pos = RTC_STUN_HEADER_BYTES;
    if (out_capacity < pos + 12u + (use_candidate ? 4u : 0u)) {
        return RTC_STATUS_CAPACITY;
    }

    rtc_stun_write_u16(out, RTC_STUN_BINDING_REQUEST);
    rtc_stun_write_u32(out + 4, RTC_STUN_MAGIC_COOKIE);
    memcpy(out + 8, transaction_id, RTC_STUN_TRANSACTION_ID_BYTES);

    role_attr = controlling ? RTC_STUN_ATTR_ICE_CONTROLLING
                            : RTC_STUN_ATTR_ICE_CONTROLLED;
    rtc_stun_write_u16(out + pos, role_attr);
    rtc_stun_write_u16(out + pos + 2u, 8u);
    rtc_stun_write_u64(out + pos + 4u, tie_breaker);
    pos += 12u;

    if (use_candidate) {
        rtc_stun_write_u16(out + pos, RTC_STUN_ATTR_USE_CANDIDATE);
        rtc_stun_write_u16(out + pos + 2u, 0u);
        pos += 4u;
    }

    rtc_stun_write_u16(out + 2, (uint16_t)(pos - RTC_STUN_HEADER_BYTES));
    *out_len = pos;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_stun_write_ice_binding_request_authenticated(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    const char *local_ufrag, size_t local_ufrag_len,
    const char *remote_ufrag, size_t remote_ufrag_len,
    const char *remote_pwd, size_t remote_pwd_len, uint32_t priority,
    int use_candidate, int controlling, uint64_t tie_breaker,
    size_t *out_len)
{
    uint8_t value[128];
    size_t pos;
    size_t username_len;
    uint16_t role_attr;
    rtc_status_t status;

    if (out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_len = 0;
    if (out == 0 || transaction_id == 0 || local_ufrag == 0 ||
        remote_ufrag == 0 || remote_pwd == 0 || local_ufrag_len == 0u ||
        remote_ufrag_len == 0u || remote_pwd_len == 0u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    username_len = remote_ufrag_len + 1u + local_ufrag_len;
    if (username_len > sizeof(value)) {
        return RTC_STATUS_CAPACITY;
    }

    pos = RTC_STUN_HEADER_BYTES;
    if (out_capacity < pos) {
        return RTC_STATUS_CAPACITY;
    }
    rtc_stun_write_u16(out, RTC_STUN_BINDING_REQUEST);
    rtc_stun_write_u16(out + 2u, 0);
    rtc_stun_write_u32(out + 4u, RTC_STUN_MAGIC_COOKIE);
    memcpy(out + 8u, transaction_id, RTC_STUN_TRANSACTION_ID_BYTES);

    memcpy(value, remote_ufrag, remote_ufrag_len);
    value[remote_ufrag_len] = ':';
    memcpy(value + remote_ufrag_len + 1u, local_ufrag, local_ufrag_len);
    status = rtc_stun_write_attr(out, out_capacity, &pos,
                                 RTC_STUN_ATTR_USERNAME, value,
                                 (uint16_t)username_len);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    rtc_stun_write_u32(value, priority);
    status = rtc_stun_write_attr(out, out_capacity, &pos,
                                 RTC_STUN_ATTR_PRIORITY, value, 4u);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    role_attr = controlling ? RTC_STUN_ATTR_ICE_CONTROLLING
                            : RTC_STUN_ATTR_ICE_CONTROLLED;
    rtc_stun_write_u64(value, tie_breaker);
    status = rtc_stun_write_attr(out, out_capacity, &pos, role_attr, value, 8u);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    if (use_candidate) {
        status = rtc_stun_write_attr(out, out_capacity, &pos,
                                     RTC_STUN_ATTR_USE_CANDIDATE, 0, 0u);
        if (status != RTC_STATUS_OK) {
            return status;
        }
    }

    status = rtc_stun_write_message_integrity(out, out_capacity, &pos,
                                              remote_pwd, remote_pwd_len);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_stun_write_fingerprint(out, out_capacity, &pos);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    *out_len = pos;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_stun_write_binding_success_response_authenticated(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    const char *mapped_ip, uint16_t mapped_port,
    const char *local_pwd, size_t local_pwd_len, size_t *out_len)
{
    uint8_t value[8];
    uint32_t ip;
    size_t pos;
    rtc_status_t status;

    if (out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_len = 0;
    if (out == 0 || transaction_id == 0 || !rtc_stun_parse_ipv4(mapped_ip, &ip) ||
        mapped_port == 0u || local_pwd == 0 || local_pwd_len == 0u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (out_capacity < RTC_STUN_HEADER_BYTES) {
        return RTC_STATUS_CAPACITY;
    }

    pos = RTC_STUN_HEADER_BYTES;
    rtc_stun_write_u16(out, RTC_STUN_BINDING_SUCCESS_RESPONSE);
    rtc_stun_write_u16(out + 2u, 0);
    rtc_stun_write_u32(out + 4u, RTC_STUN_MAGIC_COOKIE);
    memcpy(out + 8u, transaction_id, RTC_STUN_TRANSACTION_ID_BYTES);

    value[0] = 0;
    value[1] = RTC_STUN_FAMILY_IPV4;
    rtc_stun_write_u16(value + 2u,
                       (uint16_t)(mapped_port ^
                                  (RTC_STUN_MAGIC_COOKIE >> 16)));
    rtc_stun_write_u32(value + 4u, ip ^ RTC_STUN_MAGIC_COOKIE);
    status = rtc_stun_write_attr(out, out_capacity, &pos,
                                 RTC_STUN_ATTR_XOR_MAPPED_ADDRESS, value, 8u);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_stun_write_message_integrity(out, out_capacity, &pos,
                                              local_pwd, local_pwd_len);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_stun_write_fingerprint(out, out_capacity, &pos);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    *out_len = pos;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_stun_parse_header(const uint8_t *data, size_t len,
                                   rtc_stun_header_t *out_header)
{
    uint16_t message_len;

    if (data == 0 || out_header == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (!rtc_stun_is_datagram(data, len)) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    message_len = rtc_stun_read_u16(data + 2);
    if ((message_len & 0x03u) != 0 ||
        len != RTC_STUN_HEADER_BYTES + (size_t)message_len) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    out_header->type = rtc_stun_read_u16(data);
    out_header->length = message_len;
    memcpy(out_header->transaction_id, data + 8,
           RTC_STUN_TRANSACTION_ID_BYTES);
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_stun_find_attr(const uint8_t *data, size_t len,
                                       uint16_t target_type,
                                       size_t *out_attr_pos,
                                       size_t *out_value_pos,
                                       uint16_t *out_attr_len)
{
    rtc_stun_header_t header;
    size_t pos;

    if (out_attr_pos == 0 || out_value_pos == 0 || out_attr_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (rtc_stun_parse_header(data, len, &header) != RTC_STATUS_OK) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    pos = RTC_STUN_HEADER_BYTES;
    while (pos + 4u <= len) {
        uint16_t attr_type = rtc_stun_read_u16(data + pos);
        uint16_t attr_len = rtc_stun_read_u16(data + pos + 2);
        size_t value_pos = pos + 4u;
        size_t next_pos = value_pos + attr_len;
        size_t padded_next_pos = (next_pos + 3u) & ~(size_t)3u;

        if (next_pos > len || padded_next_pos > len) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        if (attr_type == target_type) {
            *out_attr_pos = pos;
            *out_value_pos = value_pos;
            *out_attr_len = attr_len;
            return RTC_STATUS_OK;
        }
        pos = padded_next_pos;
    }

    return RTC_STATUS_PROTOCOL_ERROR;
}

rtc_status_t rtc_stun_validate_message_integrity(
    const uint8_t *data, size_t len, const char *pwd, size_t pwd_len)
{
    size_t attr_pos;
    size_t value_pos;
    uint16_t attr_len;
    uint8_t scratch[1024];
    uint8_t digest[RTC_STUN_MESSAGE_INTEGRITY_BYTES];

    if (data == 0 || pwd == 0 || pwd_len == 0u || len > sizeof(scratch)) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (rtc_stun_find_attr(data, len, RTC_STUN_ATTR_MESSAGE_INTEGRITY,
                           &attr_pos, &value_pos, &attr_len) !=
        RTC_STATUS_OK) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (attr_len != RTC_STUN_MESSAGE_INTEGRITY_BYTES ||
        value_pos + RTC_STUN_MESSAGE_INTEGRITY_BYTES > len) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    memcpy(scratch, data, attr_pos);
    rtc_stun_write_u16(scratch + 2u,
                       (uint16_t)(attr_pos + 24u - RTC_STUN_HEADER_BYTES));
    rtc_hmac_sha1((const uint8_t *)pwd, pwd_len, scratch, attr_pos, digest);
    return memcmp(digest, data + value_pos, sizeof(digest)) == 0
               ? RTC_STATUS_OK
               : RTC_STATUS_PROTOCOL_ERROR;
}

rtc_status_t rtc_stun_validate_fingerprint(const uint8_t *data, size_t len)
{
    size_t attr_pos;
    size_t value_pos;
    uint16_t attr_len;
    uint32_t expected;
    uint32_t actual;

    if (rtc_stun_find_attr(data, len, RTC_STUN_ATTR_FINGERPRINT, &attr_pos,
                           &value_pos, &attr_len) != RTC_STATUS_OK) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (attr_len != 4u || value_pos + 4u > len || attr_pos + 8u != len) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    expected = rtc_crc32(data, attr_pos) ^ RTC_STUN_FINGERPRINT_XOR;
    actual = rtc_stun_read_u32(data + value_pos);
    return expected == actual ? RTC_STATUS_OK : RTC_STATUS_PROTOCOL_ERROR;
}

static rtc_status_t rtc_stun_parse_binding_request_attrs_internal(
    const uint8_t *data, size_t len, const char *local_ufrag,
    size_t local_ufrag_len, const char *remote_ufrag, size_t remote_ufrag_len,
    const char *local_pwd, size_t local_pwd_len,
    rtc_stun_binding_request_attrs_t *out_attrs)
{
    rtc_stun_header_t header;
    size_t pos;

    if (out_attrs == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    memset(out_attrs, 0, sizeof(*out_attrs));
    if (rtc_stun_parse_header(data, len, &header) != RTC_STATUS_OK) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (header.type != RTC_STUN_BINDING_REQUEST) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    pos = RTC_STUN_HEADER_BYTES;
    while (pos + 4u <= len) {
        uint16_t attr_type = rtc_stun_read_u16(data + pos);
        uint16_t attr_len = rtc_stun_read_u16(data + pos + 2);
        size_t value_pos = pos + 4u;
        size_t next_pos = value_pos + attr_len;
        size_t padded_next_pos = (next_pos + 3u) & ~(size_t)3u;

        if (next_pos > len || padded_next_pos > len) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }

        if (attr_type == RTC_STUN_ATTR_USERNAME) {
            if (attr_len >= sizeof(out_attrs->username)) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            memcpy(out_attrs->username, data + value_pos, attr_len);
            out_attrs->username[attr_len] = '\0';
            out_attrs->username_len = attr_len;
        } else if (attr_type == RTC_STUN_ATTR_PRIORITY) {
            if (attr_len != 4u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            out_attrs->has_priority = 1;
            out_attrs->priority = rtc_stun_read_u32(data + value_pos);
        } else if (attr_type == RTC_STUN_ATTR_MESSAGE_INTEGRITY) {
            if (attr_len != RTC_STUN_MESSAGE_INTEGRITY_BYTES) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            out_attrs->has_message_integrity = 1;
        } else if (attr_type == RTC_STUN_ATTR_FINGERPRINT) {
            if (attr_len != 4u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            out_attrs->has_fingerprint = 1;
        } else if (attr_type == RTC_STUN_ATTR_USE_CANDIDATE) {
            if (attr_len != 0u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            out_attrs->use_candidate = 1;
        } else if (attr_type == RTC_STUN_ATTR_ICE_CONTROLLING ||
                   attr_type == RTC_STUN_ATTR_ICE_CONTROLLED) {
            if (attr_len != 8u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            out_attrs->tie_breaker = rtc_stun_read_u64(data + value_pos);
            if (attr_type == RTC_STUN_ATTR_ICE_CONTROLLING) {
                out_attrs->has_ice_controlling = 1;
            } else {
                out_attrs->has_ice_controlled = 1;
            }
        }

        pos = padded_next_pos;
    }

    if (out_attrs->has_fingerprint) {
        out_attrs->fingerprint_valid =
            rtc_stun_validate_fingerprint(data, len) == RTC_STATUS_OK;
        if (!out_attrs->fingerprint_valid) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
    }

    if (out_attrs->has_message_integrity && local_pwd != 0 &&
        local_pwd_len > 0u) {
        out_attrs->message_integrity_valid =
            rtc_stun_validate_message_integrity(data, len, local_pwd,
                                                local_pwd_len) ==
            RTC_STATUS_OK;
        if (!out_attrs->message_integrity_valid) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
    }

    if (local_ufrag != 0 && local_ufrag_len > 0u && remote_ufrag != 0 &&
        remote_ufrag_len > 0u && out_attrs->username_len > 0u) {
        if (out_attrs->username_len !=
                local_ufrag_len + 1u + remote_ufrag_len ||
            memcmp(out_attrs->username, local_ufrag, local_ufrag_len) != 0 ||
            out_attrs->username[local_ufrag_len] != ':' ||
            memcmp(out_attrs->username + local_ufrag_len + 1u, remote_ufrag,
                   remote_ufrag_len) != 0) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
    }

    return RTC_STATUS_OK;
}

rtc_status_t rtc_stun_parse_binding_request_attrs(
    const uint8_t *data, size_t len, rtc_stun_binding_request_attrs_t *out_attrs)
{
    return rtc_stun_parse_binding_request_attrs_internal(
        data, len, 0, 0, 0, 0, 0, 0, out_attrs);
}

rtc_status_t rtc_stun_parse_binding_request_attrs_auth(
    const uint8_t *data, size_t len, const char *local_ufrag,
    size_t local_ufrag_len, const char *remote_ufrag, size_t remote_ufrag_len,
    const char *local_pwd, size_t local_pwd_len,
    rtc_stun_binding_request_attrs_t *out_attrs)
{
    return rtc_stun_parse_binding_request_attrs_internal(
        data, len, local_ufrag, local_ufrag_len, remote_ufrag,
        remote_ufrag_len, local_pwd, local_pwd_len, out_attrs);
}

rtc_status_t rtc_stun_parse_error_code(const uint8_t *data, size_t len,
                                       uint16_t *out_code)
{
    rtc_stun_header_t header;
    size_t pos;

    if (out_code == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_code = 0;
    if (rtc_stun_parse_header(data, len, &header) != RTC_STATUS_OK) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (header.type != RTC_STUN_BINDING_ERROR_RESPONSE) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    pos = RTC_STUN_HEADER_BYTES;
    while (pos + 4u <= len) {
        uint16_t attr_type = rtc_stun_read_u16(data + pos);
        uint16_t attr_len = rtc_stun_read_u16(data + pos + 2);
        size_t value_pos = pos + 4u;
        size_t next_pos = value_pos + attr_len;
        size_t padded_next_pos = (next_pos + 3u) & ~(size_t)3u;

        if (next_pos > len || padded_next_pos > len) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        if (attr_type == RTC_STUN_ATTR_ERROR_CODE) {
            if (attr_len < 4u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            *out_code =
                (uint16_t)((data[value_pos + 2u] & 0x07u) * 100u +
                           data[value_pos + 3u]);
            return RTC_STATUS_OK;
        }
        pos = padded_next_pos;
    }

    return RTC_STATUS_PROTOCOL_ERROR;
}

rtc_status_t rtc_stun_parse_xor_mapped_address(
    const uint8_t *data, size_t len, rtc_stun_xor_mapped_address_t *out_addr)
{
    rtc_stun_header_t header;
    size_t pos;

    if (out_addr == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (rtc_stun_parse_header(data, len, &header) != RTC_STATUS_OK) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (header.type != RTC_STUN_BINDING_SUCCESS_RESPONSE) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    pos = RTC_STUN_HEADER_BYTES;
    while (pos + 4u <= len) {
        uint16_t attr_type = rtc_stun_read_u16(data + pos);
        uint16_t attr_len = rtc_stun_read_u16(data + pos + 2);
        size_t value_pos = pos + 4u;
        size_t next_pos = value_pos + attr_len;
        size_t padded_next_pos = (next_pos + 3u) & ~(size_t)3u;

        if (next_pos > len || padded_next_pos > len) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }

        if (attr_type == RTC_STUN_ATTR_XOR_MAPPED_ADDRESS) {
            uint16_t xport;
            uint32_t xaddr;
            uint32_t addr;

            if (attr_len < 4u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            out_addr->family = data[value_pos + 1u];
            if (out_addr->family == RTC_STUN_FAMILY_IPV6) {
                return RTC_STATUS_UNSUPPORTED;
            }
            if (out_addr->family != RTC_STUN_FAMILY_IPV4 || attr_len != 8u) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }

            xport = rtc_stun_read_u16(data + value_pos + 2u);
            xaddr = rtc_stun_read_u32(data + value_pos + 4u);
            out_addr->port = (uint16_t)(xport ^ (RTC_STUN_MAGIC_COOKIE >> 16));
            addr = xaddr ^ RTC_STUN_MAGIC_COOKIE;
            snprintf(out_addr->ip, sizeof(out_addr->ip), "%u.%u.%u.%u",
                     (unsigned)((addr >> 24) & 0xFFu),
                     (unsigned)((addr >> 16) & 0xFFu),
                     (unsigned)((addr >> 8) & 0xFFu),
                     (unsigned)(addr & 0xFFu));
            return RTC_STATUS_OK;
        }

        pos = padded_next_pos;
    }

    return RTC_STATUS_PROTOCOL_ERROR;
}
