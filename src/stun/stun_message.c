#include "stun_message.h"

#include "common/mrtc_common.h"

#include <stdio.h>
#include <string.h>

MRTC_STATUS mrtc_stun_write_binding_request(uint8_t *buffer,
                                            size_t buffer_len,
                                            size_t *written_len,
                                            uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN])
{
    size_t i;

    if (buffer == 0 || written_len == 0 || transaction_id == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (buffer_len < 20) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_random_bytes(transaction_id, MRTC_STUN_TRANSACTION_ID_LEN) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }

    buffer[0] = 0x00;
    buffer[1] = 0x01;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    buffer[4] = 0x21;
    buffer[5] = 0x12;
    buffer[6] = 0xA4;
    buffer[7] = 0x42;
    for (i = 0; i < MRTC_STUN_TRANSACTION_ID_LEN; ++i) {
        buffer[8 + i] = transaction_id[i];
    }

    *written_len = 20;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_validate_header(const uint8_t *buffer, size_t buffer_len)
{
    if (buffer == 0 || buffer_len < 20) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    if (buffer[4] != 0x21 || buffer[5] != 0x12 || buffer[6] != 0xA4 || buffer[7] != 0x42) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    return MRTC_STATUS_OK;
}

static uint16_t mrtc_read_be16(const uint8_t *value)
{
    return (uint16_t) (((uint16_t) value[0] << 8) | (uint16_t) value[1]);
}

static uint32_t mrtc_read_be32(const uint8_t *value)
{
    return ((uint32_t) value[0] << 24) |
           ((uint32_t) value[1] << 16) |
           ((uint32_t) value[2] << 8) |
           (uint32_t) value[3];
}

MRTC_STATUS mrtc_stun_parse_xor_mapped_address(const uint8_t *buffer,
                                               size_t buffer_len,
                                               MRTC_STUN_XOR_MAPPED_ADDRESS *address)
{
    size_t offset = 20;
    uint16_t message_len;

    if (mrtc_stun_validate_header(buffer, buffer_len) != MRTC_STATUS_OK || address == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    message_len = mrtc_read_be16(buffer + 2);
    if ((size_t) message_len + 20 > buffer_len) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    memset(address, 0, sizeof(*address));
    while (offset + 4 <= 20 + (size_t) message_len) {
        uint16_t attr_type = mrtc_read_be16(buffer + offset);
        uint16_t attr_len = mrtc_read_be16(buffer + offset + 2);
        size_t value_offset = offset + 4;
        size_t padded_len = (size_t) ((attr_len + 3u) & ~3u);

        if (value_offset + attr_len > buffer_len) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        if (attr_type == 0x0020u) {
            uint16_t x_port;
            uint32_t x_addr;
            if (attr_len < 8 || buffer[value_offset + 1] != 0x01) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            x_port = (uint16_t) (mrtc_read_be16(buffer + value_offset + 2) ^ (uint16_t) (MRTC_STUN_MAGIC_COOKIE >> 16));
            x_addr = mrtc_read_be32(buffer + value_offset + 4) ^ MRTC_STUN_MAGIC_COOKIE;
            address->port = x_port;
            (void) snprintf(address->ip,
                            sizeof(address->ip),
                            "%u.%u.%u.%u",
                            (unsigned int) ((x_addr >> 24) & 0xffu),
                            (unsigned int) ((x_addr >> 16) & 0xffu),
                            (unsigned int) ((x_addr >> 8) & 0xffu),
                            (unsigned int) (x_addr & 0xffu));
            return MRTC_STATUS_OK;
        }
        offset = value_offset + padded_len;
    }

    return MRTC_STATUS_PARSE_ERROR;
}

MRTC_STATUS mrtc_stun_validate_message_integrity_input(const char *username, const char *password)
{
    if (username == 0 || username[0] == '\0' || password == 0 || password[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }
    return MRTC_STATUS_OK;
}
