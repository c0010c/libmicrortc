#include "stun/stun.h"

#include <stdio.h>
#include <string.h>

#define RTC_STUN_HEADER_BYTES 20u
#define RTC_STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020u
#define RTC_STUN_ATTR_ERROR_CODE 0x0009u
#define RTC_STUN_ATTR_USE_CANDIDATE 0x0025u
#define RTC_STUN_ATTR_ICE_CONTROLLED 0x8029u
#define RTC_STUN_ATTR_ICE_CONTROLLING 0x802au
#define RTC_STUN_FAMILY_IPV4 0x01u
#define RTC_STUN_FAMILY_IPV6 0x02u

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

rtc_status_t rtc_stun_parse_binding_request_attrs(
    const uint8_t *data, size_t len, rtc_stun_binding_request_attrs_t *out_attrs)
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

        if (attr_type == RTC_STUN_ATTR_USE_CANDIDATE) {
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

    return RTC_STATUS_OK;
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
