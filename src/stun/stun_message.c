#include "stun_message.h"

#include "common/mrtc_common.h"

#ifdef MRTC_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif
#ifdef MRTC_HAVE_MBEDTLS
#include <mbedtls/md.h>
#endif

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#define MRTC_STUN_ATTR_MAPPED_ADDRESS 0x0001u
#define MRTC_STUN_ATTR_USERNAME 0x0006u
#define MRTC_STUN_ATTR_MESSAGE_INTEGRITY 0x0008u
#define MRTC_STUN_ATTR_ERROR_CODE 0x0009u
#define MRTC_STUN_ATTR_REALM 0x0014u
#define MRTC_STUN_ATTR_NONCE 0x0015u
#define MRTC_STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020u
#define MRTC_STUN_ATTR_REQUESTED_TRANSPORT 0x0019u
#define MRTC_STUN_ATTR_XOR_RELAYED_ADDRESS 0x0016u
#define MRTC_STUN_ATTR_XOR_PEER_ADDRESS 0x0012u
#define MRTC_STUN_ATTR_DATA 0x0013u
#define MRTC_STUN_ATTR_FINGERPRINT 0x8028u
#define MRTC_STUN_FINGERPRINT_XOR 0x5354554eu
#define MRTC_TURN_REQUESTED_TRANSPORT_UDP 0x11000000u

static void mrtc_write_be16(uint8_t *value, uint16_t number)
{
    value[0] = (uint8_t) ((number >> 8) & 0xffu);
    value[1] = (uint8_t) (number & 0xffu);
}

static void mrtc_write_be32(uint8_t *value, uint32_t number)
{
    value[0] = (uint8_t) ((number >> 24) & 0xffu);
    value[1] = (uint8_t) ((number >> 16) & 0xffu);
    value[2] = (uint8_t) ((number >> 8) & 0xffu);
    value[3] = (uint8_t) (number & 0xffu);
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

static size_t mrtc_stun_padded_len(uint16_t attr_len)
{
    return (size_t) ((attr_len + 3u) & ~3u);
}

static uint32_t mrtc_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;

    crc = ~crc;
    for (i = 0; i < len; ++i) {
        unsigned int bit;
        crc ^= data[i];
        for (bit = 0; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t) (0u - (crc & 1u));
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

static uint32_t mrtc_stun_fingerprint_value(const uint8_t *buffer, size_t len)
{
    return mrtc_crc32_update(0u, buffer, len) ^ MRTC_STUN_FINGERPRINT_XOR;
}

static MRTC_STATUS mrtc_stun_write_attr(uint8_t *buffer,
                                        size_t buffer_len,
                                        size_t *offset,
                                        uint16_t attr_type,
                                        const uint8_t *value,
                                        uint16_t value_len);
static MRTC_STATUS mrtc_stun_write_xor_address_attr(uint8_t *buffer,
                                                    size_t buffer_len,
                                                    size_t *offset,
                                                    uint16_t attr_type,
                                                    const char *ip,
                                                    unsigned short port);
static MRTC_STATUS mrtc_stun_parse_xor_address_value(const uint8_t *value,
                                                     uint16_t value_len,
                                                     MRTC_STUN_ADDRESS *address);

static int mrtc_stun_hmac_sha1(const uint8_t *buffer,
                               size_t buffer_len,
                               const char *password,
                               uint8_t digest[20])
{
#ifdef MRTC_HAVE_OPENSSL
    unsigned int digest_len = 0;
    unsigned char *result;

    if (password == 0 || password[0] == '\0') {
        return 0;
    }
    result = HMAC(EVP_sha1(),
                  password,
                  (int) strlen(password),
                  buffer,
                  buffer_len,
                  digest,
                  &digest_len);
    return result != 0 && digest_len == 20u;
#elif defined(MRTC_HAVE_MBEDTLS)
    const mbedtls_md_info_t *info;
    if (password == 0 || password[0] == '\0') {
        return 0;
    }
    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    return info != 0 &&
           mbedtls_md_hmac(info,
                           (const unsigned char *) password,
                           strlen(password),
                           buffer,
                           buffer_len,
                           digest) == 0;
#else
    (void) buffer;
    (void) buffer_len;
    (void) password;
    (void) digest;
    return 0;
#endif
}

static int mrtc_stun_hmac_sha1_key(const uint8_t *buffer,
                                   size_t buffer_len,
                                   const uint8_t *key,
                                   size_t key_len,
                                   uint8_t digest[20])
{
#ifdef MRTC_HAVE_OPENSSL
    unsigned int digest_len = 0;
    unsigned char *result;

    if (key == 0 || key_len == 0u || key_len > 0x7fffffffu) {
        return 0;
    }
    result = HMAC(EVP_sha1(), key, (int) key_len, buffer, buffer_len, digest, &digest_len);
    return result != 0 && digest_len == 20u;
#elif defined(MRTC_HAVE_MBEDTLS)
    const mbedtls_md_info_t *info;
    if (key == 0 || key_len == 0u) {
        return 0;
    }
    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    return info != 0 && mbedtls_md_hmac(info, key, key_len, buffer, buffer_len, digest) == 0;
#else
    (void) buffer;
    (void) buffer_len;
    (void) key;
    (void) key_len;
    (void) digest;
    return 0;
#endif
}

static MRTC_STATUS mrtc_stun_write_header(uint8_t *buffer,
                                          size_t buffer_len,
                                          uint16_t message_type,
                                          uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN])
{
    if (buffer == 0 || transaction_id == 0 || buffer_len < 20u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_random_bytes(transaction_id, MRTC_STUN_TRANSACTION_ID_LEN) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }
    memset(buffer, 0, buffer_len);
    mrtc_write_be16(buffer, message_type);
    mrtc_write_be16(buffer + 2u, 0u);
    mrtc_write_be32(buffer + 4u, MRTC_STUN_MAGIC_COOKIE);
    memcpy(buffer + 8u, transaction_id, MRTC_STUN_TRANSACTION_ID_LEN);
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_stun_write_message_integrity_key(uint8_t *buffer,
                                                         size_t buffer_len,
                                                         size_t *offset,
                                                         const uint8_t *key,
                                                         size_t key_len)
{
    uint8_t digest[20];
    size_t mi_attr_offset;

    if (buffer == 0 || offset == 0 || key == 0 || key_len == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    mi_attr_offset = *offset;
    if (mi_attr_offset + 24u > buffer_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    mrtc_write_be16(buffer + mi_attr_offset, MRTC_STUN_ATTR_MESSAGE_INTEGRITY);
    mrtc_write_be16(buffer + mi_attr_offset + 2u, 20u);
    *offset += 24u;
    mrtc_write_be16(buffer + 2u, (uint16_t) (*offset - 20u));
    if (!mrtc_stun_hmac_sha1_key(buffer, mi_attr_offset, key, key_len, digest)) {
        return MRTC_STATUS_INVALID_STATE;
    }
    memcpy(buffer + mi_attr_offset + 4u, digest, sizeof(digest));
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_stun_write_fingerprint(uint8_t *buffer, size_t buffer_len, size_t *offset)
{
    uint32_t fingerprint;
    size_t fp_attr_offset;

    if (buffer == 0 || offset == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    fp_attr_offset = *offset;
    if (fp_attr_offset + 8u > buffer_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    mrtc_write_be16(buffer + fp_attr_offset, MRTC_STUN_ATTR_FINGERPRINT);
    mrtc_write_be16(buffer + fp_attr_offset + 2u, 4u);
    *offset += 8u;
    mrtc_write_be16(buffer + 2u, (uint16_t) (*offset - 20u));
    fingerprint = mrtc_stun_fingerprint_value(buffer, fp_attr_offset);
    mrtc_write_be32(buffer + fp_attr_offset + 4u, fingerprint);
    return MRTC_STATUS_OK;
}

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

    mrtc_write_be16(buffer, MRTC_STUN_TYPE_BINDING_REQUEST);
    mrtc_write_be16(buffer + 2, 0);
    mrtc_write_be32(buffer + 4, MRTC_STUN_MAGIC_COOKIE);
    for (i = 0; i < MRTC_STUN_TRANSACTION_ID_LEN; ++i) {
        buffer[8 + i] = transaction_id[i];
    }

    *written_len = 20;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_write_binding_request_with_credentials(uint8_t *buffer,
                                                             size_t buffer_len,
                                                             size_t *written_len,
                                                             uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                             const char *username,
                                                             const char *password)
{
    size_t offset = 20u;
    size_t username_len;

    if (mrtc_stun_write_binding_request(buffer, buffer_len, written_len, transaction_id) != MRTC_STATUS_OK ||
        username == 0 || username[0] == '\0' || password == 0 || password[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }
    username_len = strlen(username);
    if (username_len > 0xffffu) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_stun_write_attr(buffer,
                             buffer_len,
                             &offset,
                             MRTC_STUN_ATTR_USERNAME,
                             (const uint8_t *) username,
                             (uint16_t) username_len) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }
    {
        uint8_t digest[20];
        size_t mi_attr_offset = offset;

        if (offset + 24u > buffer_len) {
            return MRTC_STATUS_INVALID_ARG;
        }
        mrtc_write_be16(buffer + mi_attr_offset, MRTC_STUN_ATTR_MESSAGE_INTEGRITY);
        mrtc_write_be16(buffer + mi_attr_offset + 2u, 20u);
        offset += 24u;
        mrtc_write_be16(buffer + 2u, (uint16_t) (offset - 20u));
        if (!mrtc_stun_hmac_sha1(buffer, mi_attr_offset, password, digest)) {
            return MRTC_STATUS_INVALID_STATE;
        }
        memcpy(buffer + mi_attr_offset + 4u, digest, sizeof(digest));
    }
    {
        uint32_t fingerprint;
        size_t fp_attr_offset = offset;

        if (offset + 8u > buffer_len) {
            return MRTC_STATUS_INVALID_ARG;
        }
        mrtc_write_be16(buffer + fp_attr_offset, MRTC_STUN_ATTR_FINGERPRINT);
        mrtc_write_be16(buffer + fp_attr_offset + 2u, 4u);
        offset += 8u;
        mrtc_write_be16(buffer + 2u, (uint16_t) (offset - 20u));
        fingerprint = mrtc_stun_fingerprint_value(buffer, fp_attr_offset);
        mrtc_write_be32(buffer + fp_attr_offset + 4u, fingerprint);
    }
    *written_len = offset;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_validate_header(const uint8_t *buffer, size_t buffer_len)
{
    uint16_t message_len;

    if (buffer == 0 || buffer_len < 20) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    if ((buffer[0] & 0xc0u) != 0u) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    if (buffer[4] != 0x21 || buffer[5] != 0x12 || buffer[6] != 0xA4 || buffer[7] != 0x42) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    message_len = mrtc_read_be16(buffer + 2);
    if ((message_len & 0x0003u) != 0u || 20u + (size_t) message_len > buffer_len) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_stun_write_attr(uint8_t *buffer,
                                        size_t buffer_len,
                                        size_t *offset,
                                        uint16_t attr_type,
                                        const uint8_t *value,
                                        uint16_t value_len)
{
    size_t padded_len;

    if (buffer == 0 || offset == 0 || (value == 0 && value_len > 0u)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    padded_len = mrtc_stun_padded_len(value_len);
    if (*offset + 4u + padded_len > buffer_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    mrtc_write_be16(buffer + *offset, attr_type);
    mrtc_write_be16(buffer + *offset + 2u, value_len);
    if (value_len > 0u) {
        memcpy(buffer + *offset + 4u, value, value_len);
    }
    if (padded_len > value_len) {
        memset(buffer + *offset + 4u + value_len, 0, padded_len - value_len);
    }
    *offset += 4u + padded_len;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_stun_write_xor_address_attr(uint8_t *buffer,
                                                    size_t buffer_len,
                                                    size_t *offset,
                                                    uint16_t attr_type,
                                                    const char *ip,
                                                    unsigned short port)
{
    uint8_t value[8];
    struct in_addr address;
    uint32_t host_addr;

    if (ip == 0 || port == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (inet_pton(AF_INET, ip, &address) != 1) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(value, 0, sizeof(value));
    value[1] = 0x01u;
    mrtc_write_be16(value + 2u, (uint16_t) (port ^ (uint16_t) (MRTC_STUN_MAGIC_COOKIE >> 16u)));
    host_addr = ntohl(address.s_addr) ^ MRTC_STUN_MAGIC_COOKIE;
    mrtc_write_be32(value + 4u, host_addr);
    return mrtc_stun_write_attr(buffer, buffer_len, offset, attr_type, value, (uint16_t) sizeof(value));
}

static MRTC_STATUS mrtc_stun_parse_xor_address_value(const uint8_t *value,
                                                     uint16_t value_len,
                                                     MRTC_STUN_ADDRESS *address)
{
    uint16_t x_port;
    uint32_t x_addr;

    if (value == 0 || address == 0 || value_len < 8u || value[1] != 0x01u) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    x_port = (uint16_t) (mrtc_read_be16(value + 2u) ^ (uint16_t) (MRTC_STUN_MAGIC_COOKIE >> 16u));
    x_addr = mrtc_read_be32(value + 4u) ^ MRTC_STUN_MAGIC_COOKIE;
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

MRTC_STATUS mrtc_stun_parse_binding_request(const uint8_t *buffer,
                                            size_t buffer_len,
                                            const char *password,
                                            MRTC_STUN_BINDING_REQUEST *request)
{
    uint16_t message_len;
    size_t offset = 20u;
    size_t message_end;

    if (request == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_stun_validate_header(buffer, buffer_len) != MRTC_STATUS_OK ||
        mrtc_read_be16(buffer) != MRTC_STUN_TYPE_BINDING_REQUEST) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    memset(request, 0, sizeof(*request));
    memcpy(request->transaction_id, buffer + 8u, MRTC_STUN_TRANSACTION_ID_LEN);
    message_len = mrtc_read_be16(buffer + 2u);
    message_end = 20u + (size_t) message_len;

    while (offset + 4u <= message_end) {
        uint16_t attr_type = mrtc_read_be16(buffer + offset);
        uint16_t attr_len = mrtc_read_be16(buffer + offset + 2u);
        size_t value_offset = offset + 4u;
        size_t padded_len = mrtc_stun_padded_len(attr_len);

        if (value_offset + attr_len > message_end || value_offset + padded_len > message_end) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        if (attr_type == MRTC_STUN_ATTR_USERNAME) {
            size_t copy_len = attr_len < sizeof(request->username) - 1u ? attr_len : sizeof(request->username) - 1u;
            memcpy(request->username, buffer + value_offset, copy_len);
            request->username[copy_len] = '\0';
        } else if (attr_type == MRTC_STUN_ATTR_MESSAGE_INTEGRITY) {
            uint8_t digest[20];
            uint8_t scratch[2048];
            size_t mi_attr_offset = value_offset - 4u;
            size_t hmac_len = mi_attr_offset;

            request->has_message_integrity = 1;
            if (attr_len != 20u || hmac_len > sizeof(scratch) || value_offset + 20u > message_end) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            memcpy(scratch, buffer, hmac_len);
            mrtc_write_be16(scratch + 2u, (uint16_t) (mi_attr_offset + 24u - 20u));
            if (mrtc_stun_hmac_sha1(scratch, hmac_len, password, digest)) {
                request->message_integrity_valid = memcmp(digest, buffer + value_offset, sizeof(digest)) == 0;
            }
        } else if (attr_type == MRTC_STUN_ATTR_FINGERPRINT) {
            uint8_t scratch[2048];
            uint32_t expected;
            uint32_t actual;
            size_t fingerprint_len = value_offset - 4u;

            request->has_fingerprint = 1;
            if (attr_len != 4u || fingerprint_len > sizeof(scratch)) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            memcpy(scratch, buffer, fingerprint_len);
            mrtc_write_be16(scratch + 2u, (uint16_t) (fingerprint_len + 8u - 20u));
            expected = mrtc_stun_fingerprint_value(scratch, fingerprint_len);
            actual = mrtc_read_be32(buffer + value_offset);
            request->fingerprint_valid = expected == actual;
        }
        offset = value_offset + padded_len;
    }

    return request->username[0] == '\0' ? MRTC_STATUS_PARSE_ERROR : MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_write_binding_success_response(uint8_t *buffer,
                                                     size_t buffer_len,
                                                     size_t *written_len,
                                                     const uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                     const char *mapped_ip,
                                                     unsigned short mapped_port,
                                                     const char *password)
{
    uint8_t xor_mapped[8];
    struct in_addr address;
    uint32_t host_addr;
    size_t offset = 20u;

    if (buffer == 0 || written_len == 0 || transaction_id == 0 || mapped_ip == 0 || mapped_port == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (buffer_len < 20u + 12u + 24u + 8u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (inet_pton(AF_INET, mapped_ip, &address) != 1) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(buffer, 0, buffer_len);
    mrtc_write_be16(buffer, MRTC_STUN_TYPE_BINDING_SUCCESS_RESPONSE);
    mrtc_write_be16(buffer + 2u, 0);
    mrtc_write_be32(buffer + 4u, MRTC_STUN_MAGIC_COOKIE);
    memcpy(buffer + 8u, transaction_id, MRTC_STUN_TRANSACTION_ID_LEN);

    memset(xor_mapped, 0, sizeof(xor_mapped));
    xor_mapped[1] = 0x01;
    mrtc_write_be16(xor_mapped + 2u, (uint16_t) (mapped_port ^ (uint16_t) (MRTC_STUN_MAGIC_COOKIE >> 16u)));
    host_addr = ntohl(address.s_addr) ^ MRTC_STUN_MAGIC_COOKIE;
    mrtc_write_be32(xor_mapped + 4u, host_addr);
    if (mrtc_stun_write_attr(buffer,
                             buffer_len,
                             &offset,
                             MRTC_STUN_ATTR_XOR_MAPPED_ADDRESS,
                             xor_mapped,
                             (uint16_t) sizeof(xor_mapped)) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (password != 0 && password[0] != '\0') {
        uint8_t digest[20];
        size_t mi_attr_offset = offset;

        if (offset + 24u > buffer_len) {
            return MRTC_STATUS_INVALID_ARG;
        }
        mrtc_write_be16(buffer + mi_attr_offset, MRTC_STUN_ATTR_MESSAGE_INTEGRITY);
        mrtc_write_be16(buffer + mi_attr_offset + 2u, 20u);
        offset += 24u;
        mrtc_write_be16(buffer + 2u, (uint16_t) (offset - 20u));
        if (!mrtc_stun_hmac_sha1(buffer, mi_attr_offset, password, digest)) {
            return MRTC_STATUS_INVALID_STATE;
        }
        memcpy(buffer + mi_attr_offset + 4u, digest, sizeof(digest));
    }

    {
        uint32_t fingerprint;
        size_t fp_attr_offset = offset;

        if (offset + 8u > buffer_len) {
            return MRTC_STATUS_INVALID_ARG;
        }
        mrtc_write_be16(buffer + fp_attr_offset, MRTC_STUN_ATTR_FINGERPRINT);
        mrtc_write_be16(buffer + fp_attr_offset + 2u, 4u);
        offset += 8u;
        mrtc_write_be16(buffer + 2u, (uint16_t) (offset - 20u));
        fingerprint = mrtc_stun_fingerprint_value(buffer, fp_attr_offset);
        mrtc_write_be32(buffer + fp_attr_offset + 4u, fingerprint);
    }

    *written_len = offset;
    return MRTC_STATUS_OK;
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

MRTC_STATUS mrtc_stun_make_long_term_key(const char *username,
                                         const char *realm,
                                         const char *password,
                                         uint8_t key[MRTC_STUN_LONG_TERM_KEY_LEN])
{
#ifdef MRTC_HAVE_OPENSSL
    EVP_MD_CTX *ctx;
    unsigned int digest_len = 0;
    int ok;

    if (username == 0 || username[0] == '\0' ||
        realm == 0 || realm[0] == '\0' ||
        password == 0 || password[0] == '\0' ||
        key == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    ok = EVP_DigestInit_ex(ctx, EVP_md5(), 0) == 1 &&
         EVP_DigestUpdate(ctx, username, strlen(username)) == 1 &&
         EVP_DigestUpdate(ctx, ":", 1u) == 1 &&
         EVP_DigestUpdate(ctx, realm, strlen(realm)) == 1 &&
         EVP_DigestUpdate(ctx, ":", 1u) == 1 &&
         EVP_DigestUpdate(ctx, password, strlen(password)) == 1 &&
         EVP_DigestFinal_ex(ctx, key, &digest_len) == 1 &&
         digest_len == MRTC_STUN_LONG_TERM_KEY_LEN;
    EVP_MD_CTX_free(ctx);
    return ok ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
#elif defined(MRTC_HAVE_MBEDTLS)
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info;
    unsigned char colon = ':';
    int ok;

    if (username == 0 || username[0] == '\0' ||
        realm == 0 || realm[0] == '\0' ||
        password == 0 || password[0] == '\0' ||
        key == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    if (info == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    mbedtls_md_init(&ctx);
    ok = mbedtls_md_setup(&ctx, info, 0) == 0 &&
         mbedtls_md_starts(&ctx) == 0 &&
         mbedtls_md_update(&ctx, (const unsigned char *) username, strlen(username)) == 0 &&
         mbedtls_md_update(&ctx, &colon, 1u) == 0 &&
         mbedtls_md_update(&ctx, (const unsigned char *) realm, strlen(realm)) == 0 &&
         mbedtls_md_update(&ctx, &colon, 1u) == 0 &&
         mbedtls_md_update(&ctx, (const unsigned char *) password, strlen(password)) == 0 &&
         mbedtls_md_finish(&ctx, key) == 0;
    mbedtls_md_free(&ctx);
    return ok ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
#else
    (void) username;
    (void) realm;
    (void) password;
    (void) key;
    return MRTC_STATUS_INVALID_STATE;
#endif
}

MRTC_STATUS mrtc_stun_parse_message(const uint8_t *buffer,
                                    size_t buffer_len,
                                    const uint8_t *message_integrity_key,
                                    size_t message_integrity_key_len,
                                    MRTC_STUN_MESSAGE *message)
{
    uint16_t message_len;
    size_t offset = 20u;
    size_t message_end;

    if (message == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_stun_validate_header(buffer, buffer_len) != MRTC_STATUS_OK) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    memset(message, 0, sizeof(*message));
    message->message_type = mrtc_read_be16(buffer);
    memcpy(message->transaction_id, buffer + 8u, MRTC_STUN_TRANSACTION_ID_LEN);
    message_len = mrtc_read_be16(buffer + 2u);
    message_end = 20u + (size_t) message_len;

    while (offset + 4u <= message_end) {
        uint16_t attr_type = mrtc_read_be16(buffer + offset);
        uint16_t attr_len = mrtc_read_be16(buffer + offset + 2u);
        size_t value_offset = offset + 4u;
        size_t padded_len = mrtc_stun_padded_len(attr_len);

        if (value_offset + attr_len > message_end || value_offset + padded_len > message_end) {
            return MRTC_STATUS_PARSE_ERROR;
        }

        if (attr_type == MRTC_STUN_ATTR_XOR_MAPPED_ADDRESS) {
            if (mrtc_stun_parse_xor_address_value(buffer + value_offset,
                                                  attr_len,
                                                  &message->xor_mapped_address) != MRTC_STATUS_OK) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            message->has_xor_mapped_address = 1;
        } else if (attr_type == MRTC_STUN_ATTR_XOR_RELAYED_ADDRESS) {
            if (mrtc_stun_parse_xor_address_value(buffer + value_offset,
                                                  attr_len,
                                                  &message->xor_relayed_address) != MRTC_STATUS_OK) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            message->has_xor_relayed_address = 1;
        } else if (attr_type == MRTC_STUN_ATTR_XOR_PEER_ADDRESS) {
            if (mrtc_stun_parse_xor_address_value(buffer + value_offset,
                                                  attr_len,
                                                  &message->xor_peer_address) != MRTC_STATUS_OK) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            message->has_xor_peer_address = 1;
        } else if (attr_type == MRTC_STUN_ATTR_REALM) {
            size_t copy_len = attr_len < sizeof(message->realm) - 1u ? attr_len : sizeof(message->realm) - 1u;
            memcpy(message->realm, buffer + value_offset, copy_len);
            message->realm[copy_len] = '\0';
            message->has_realm = 1;
        } else if (attr_type == MRTC_STUN_ATTR_NONCE) {
            size_t copy_len = attr_len < sizeof(message->nonce) - 1u ? attr_len : sizeof(message->nonce) - 1u;
            memcpy(message->nonce, buffer + value_offset, copy_len);
            message->nonce[copy_len] = '\0';
            message->has_nonce = 1;
        } else if (attr_type == MRTC_STUN_ATTR_ERROR_CODE) {
            size_t reason_len;
            size_t copy_len;

            if (attr_len < 4u) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            message->error_code = (unsigned int) ((buffer[value_offset + 2u] & 0x07u) * 100u) +
                                  (unsigned int) buffer[value_offset + 3u];
            reason_len = (size_t) attr_len - 4u;
            copy_len = reason_len < sizeof(message->error_reason) - 1u ?
                       reason_len :
                       sizeof(message->error_reason) - 1u;
            if (copy_len > 0u) {
                memcpy(message->error_reason, buffer + value_offset + 4u, copy_len);
            }
            message->error_reason[copy_len] = '\0';
            message->has_error_code = 1;
        } else if (attr_type == MRTC_STUN_ATTR_DATA) {
            message->data = buffer + value_offset;
            message->data_len = attr_len;
            message->has_data = 1;
        } else if (attr_type == MRTC_STUN_ATTR_MESSAGE_INTEGRITY) {
            uint8_t digest[20];
            uint8_t scratch[2048];
            size_t mi_attr_offset = value_offset - 4u;
            size_t hmac_len = mi_attr_offset;

            message->has_message_integrity = 1;
            if (attr_len != 20u || value_offset + 20u > message_end) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            if (message_integrity_key != 0 && message_integrity_key_len > 0u && hmac_len <= sizeof(scratch)) {
                memcpy(scratch, buffer, hmac_len);
                mrtc_write_be16(scratch + 2u, (uint16_t) (mi_attr_offset + 24u - 20u));
                if (mrtc_stun_hmac_sha1_key(scratch,
                                            hmac_len,
                                            message_integrity_key,
                                            message_integrity_key_len,
                                            digest)) {
                    message->message_integrity_valid =
                        mrtc_constant_time_equal(digest, buffer + value_offset, sizeof(digest));
                }
            }
        } else if (attr_type == MRTC_STUN_ATTR_FINGERPRINT) {
            uint8_t scratch[2048];
            uint32_t expected;
            uint32_t actual;
            size_t fingerprint_len = value_offset - 4u;

            message->has_fingerprint = 1;
            if (attr_len != 4u || fingerprint_len > sizeof(scratch)) {
                return MRTC_STATUS_PARSE_ERROR;
            }
            memcpy(scratch, buffer, fingerprint_len);
            mrtc_write_be16(scratch + 2u, (uint16_t) (fingerprint_len + 8u - 20u));
            expected = mrtc_stun_fingerprint_value(scratch, fingerprint_len);
            actual = mrtc_read_be32(buffer + value_offset);
            message->fingerprint_valid = expected == actual;
        }

        offset = value_offset + padded_len;
    }

    return offset == message_end ? MRTC_STATUS_OK : MRTC_STATUS_PARSE_ERROR;
}

MRTC_STATUS mrtc_stun_parse_response(const uint8_t *buffer,
                                     size_t buffer_len,
                                     uint16_t expected_message_type,
                                     const uint8_t expected_transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                     const uint8_t *message_integrity_key,
                                     size_t message_integrity_key_len,
                                     MRTC_STUN_MESSAGE *message)
{
    MRTC_STATUS status;

    if (expected_transaction_id == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    status = mrtc_stun_parse_message(buffer,
                                     buffer_len,
                                     message_integrity_key,
                                     message_integrity_key_len,
                                     message);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    if (message->message_type != expected_message_type ||
        memcmp(message->transaction_id, expected_transaction_id, MRTC_STUN_TRANSACTION_ID_LEN) != 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_parse_turn_allocate_response(const uint8_t *buffer,
                                                   size_t buffer_len,
                                                   const uint8_t expected_transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                   const uint8_t *message_integrity_key,
                                                   size_t message_integrity_key_len,
                                                   MRTC_STUN_MESSAGE *message)
{
    MRTC_STATUS status;

    if (expected_transaction_id == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    status = mrtc_stun_parse_message(buffer,
                                     buffer_len,
                                     message_integrity_key,
                                     message_integrity_key_len,
                                     message);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    if (memcmp(message->transaction_id, expected_transaction_id, MRTC_STUN_TRANSACTION_ID_LEN) != 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    if (message->message_type == MRTC_STUN_TYPE_ALLOCATE_SUCCESS_RESPONSE) {
        return message->has_xor_relayed_address ? MRTC_STATUS_OK : MRTC_STATUS_PARSE_ERROR;
    }
    if (message->message_type == MRTC_STUN_TYPE_ALLOCATE_ERROR_RESPONSE &&
        message->has_error_code &&
        message->error_code == 401u) {
        return MRTC_STATUS_OK;
    }
    return MRTC_STATUS_PARSE_ERROR;
}

MRTC_STATUS mrtc_stun_parse_turn_data_indication(const uint8_t *buffer,
                                                 size_t buffer_len,
                                                 MRTC_STUN_MESSAGE *message)
{
    MRTC_STATUS status = mrtc_stun_parse_message(buffer, buffer_len, 0, 0u, message);

    if (status != MRTC_STATUS_OK) {
        return status;
    }
    if (message->message_type != MRTC_STUN_TYPE_DATA_INDICATION ||
        !message->has_xor_peer_address ||
        !message->has_data) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_write_turn_allocate_request(uint8_t *buffer,
                                                  size_t buffer_len,
                                                  size_t *written_len,
                                                  uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN])
{
    uint8_t requested_transport[4];
    size_t offset = 20u;
    MRTC_STATUS status;

    if (buffer == 0 || written_len == 0 || transaction_id == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    status = mrtc_stun_write_header(buffer, buffer_len, MRTC_STUN_TYPE_ALLOCATE_REQUEST, transaction_id);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    mrtc_write_be32(requested_transport, MRTC_TURN_REQUESTED_TRANSPORT_UDP);
    if (mrtc_stun_write_attr(buffer,
                             buffer_len,
                             &offset,
                             MRTC_STUN_ATTR_REQUESTED_TRANSPORT,
                             requested_transport,
                             (uint16_t) sizeof(requested_transport)) != MRTC_STATUS_OK ||
        mrtc_stun_write_fingerprint(buffer, buffer_len, &offset) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }
    *written_len = offset;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_write_turn_allocate_request_with_credentials(
    uint8_t *buffer,
    size_t buffer_len,
    size_t *written_len,
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
    const char *username,
    const char *realm,
    const char *nonce,
    const uint8_t key[MRTC_STUN_LONG_TERM_KEY_LEN])
{
    uint8_t requested_transport[4];
    size_t offset = 20u;
    size_t username_len;
    size_t realm_len;
    size_t nonce_len;
    MRTC_STATUS status;

    if (buffer == 0 || written_len == 0 || transaction_id == 0 ||
        username == 0 || username[0] == '\0' ||
        realm == 0 || realm[0] == '\0' ||
        nonce == 0 || nonce[0] == '\0' ||
        key == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    username_len = strlen(username);
    realm_len = strlen(realm);
    nonce_len = strlen(nonce);
    if (username_len > 0xffffu || realm_len > 0xffffu || nonce_len > 0xffffu) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_stun_write_header(buffer, buffer_len, MRTC_STUN_TYPE_ALLOCATE_REQUEST, transaction_id);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    mrtc_write_be32(requested_transport, MRTC_TURN_REQUESTED_TRANSPORT_UDP);
    status = mrtc_stun_write_attr(buffer,
                                  buffer_len,
                                  &offset,
                                  MRTC_STUN_ATTR_REQUESTED_TRANSPORT,
                                  requested_transport,
                                  (uint16_t) sizeof(requested_transport));
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_attr(buffer,
                                      buffer_len,
                                      &offset,
                                      MRTC_STUN_ATTR_USERNAME,
                                      (const uint8_t *) username,
                                      (uint16_t) username_len);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_attr(buffer,
                                      buffer_len,
                                      &offset,
                                      MRTC_STUN_ATTR_REALM,
                                      (const uint8_t *) realm,
                                      (uint16_t) realm_len);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_attr(buffer,
                                      buffer_len,
                                      &offset,
                                      MRTC_STUN_ATTR_NONCE,
                                      (const uint8_t *) nonce,
                                      (uint16_t) nonce_len);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_message_integrity_key(buffer,
                                                       buffer_len,
                                                       &offset,
                                                       key,
                                                       MRTC_STUN_LONG_TERM_KEY_LEN);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_fingerprint(buffer, buffer_len, &offset);
    }
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    *written_len = offset;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_write_turn_create_permission_request(
    uint8_t *buffer,
    size_t buffer_len,
    size_t *written_len,
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
    const char *peer_ip,
    unsigned short peer_port,
    const char *username,
    const char *realm,
    const char *nonce,
    const uint8_t key[MRTC_STUN_LONG_TERM_KEY_LEN])
{
    size_t offset = 20u;
    size_t username_len;
    size_t realm_len;
    size_t nonce_len;
    MRTC_STATUS status;

    if (buffer == 0 || written_len == 0 || transaction_id == 0 ||
        peer_ip == 0 || peer_port == 0u ||
        username == 0 || username[0] == '\0' ||
        realm == 0 || realm[0] == '\0' ||
        nonce == 0 || nonce[0] == '\0' ||
        key == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    username_len = strlen(username);
    realm_len = strlen(realm);
    nonce_len = strlen(nonce);
    if (username_len > 0xffffu || realm_len > 0xffffu || nonce_len > 0xffffu) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_stun_write_header(buffer, buffer_len, MRTC_STUN_TYPE_CREATE_PERMISSION_REQUEST, transaction_id);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_stun_write_xor_address_attr(buffer,
                                              buffer_len,
                                              &offset,
                                              MRTC_STUN_ATTR_XOR_PEER_ADDRESS,
                                              peer_ip,
                                              peer_port);
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_attr(buffer,
                                      buffer_len,
                                      &offset,
                                      MRTC_STUN_ATTR_USERNAME,
                                      (const uint8_t *) username,
                                      (uint16_t) username_len);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_attr(buffer,
                                      buffer_len,
                                      &offset,
                                      MRTC_STUN_ATTR_REALM,
                                      (const uint8_t *) realm,
                                      (uint16_t) realm_len);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_attr(buffer,
                                      buffer_len,
                                      &offset,
                                      MRTC_STUN_ATTR_NONCE,
                                      (const uint8_t *) nonce,
                                      (uint16_t) nonce_len);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_message_integrity_key(buffer,
                                                       buffer_len,
                                                       &offset,
                                                       key,
                                                       MRTC_STUN_LONG_TERM_KEY_LEN);
    }
    if (status == MRTC_STATUS_OK) {
        status = mrtc_stun_write_fingerprint(buffer, buffer_len, &offset);
    }
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    *written_len = offset;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_stun_write_turn_send_indication(uint8_t *buffer,
                                                 size_t buffer_len,
                                                 size_t *written_len,
                                                 uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                 const char *peer_ip,
                                                 unsigned short peer_port,
                                                 const uint8_t *data,
                                                 size_t data_len)
{
    size_t offset = 20u;
    MRTC_STATUS status;

    if (buffer == 0 || written_len == 0 || transaction_id == 0 ||
        peer_ip == 0 || peer_port == 0u ||
        (data == 0 && data_len > 0u) ||
        data_len > 0xffffu) {
        return MRTC_STATUS_INVALID_ARG;
    }
    status = mrtc_stun_write_header(buffer, buffer_len, MRTC_STUN_TYPE_SEND_INDICATION, transaction_id);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    if (mrtc_stun_write_xor_address_attr(buffer,
                                         buffer_len,
                                         &offset,
                                         MRTC_STUN_ATTR_XOR_PEER_ADDRESS,
                                         peer_ip,
                                         peer_port) != MRTC_STATUS_OK ||
        mrtc_stun_write_attr(buffer,
                             buffer_len,
                             &offset,
                             MRTC_STUN_ATTR_DATA,
                             data,
                             (uint16_t) data_len) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }
    mrtc_write_be16(buffer + 2u, (uint16_t) (offset - 20u));
    *written_len = offset;
    return MRTC_STATUS_OK;
}
