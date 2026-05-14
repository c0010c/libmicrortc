#include "../../src/stun/stun_message.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>

static void write_be16(uint8_t *value, uint16_t number)
{
    value[0] = (uint8_t) ((number >> 8) & 0xffu);
    value[1] = (uint8_t) (number & 0xffu);
}

static void write_be32(uint8_t *value, uint32_t number)
{
    value[0] = (uint8_t) ((number >> 24) & 0xffu);
    value[1] = (uint8_t) ((number >> 16) & 0xffu);
    value[2] = (uint8_t) ((number >> 8) & 0xffu);
    value[3] = (uint8_t) (number & 0xffu);
}

static size_t padded_len(size_t len)
{
    return (len + 3u) & ~3u;
}

static int append_attr(uint8_t *buffer, size_t buffer_len, size_t *offset, uint16_t type, const uint8_t *value, uint16_t len)
{
    size_t pad = padded_len(len);

    if (*offset + 4u + pad > buffer_len) {
        return 0;
    }
    write_be16(buffer + *offset, type);
    write_be16(buffer + *offset + 2u, len);
    if (len > 0u) {
        memcpy(buffer + *offset + 4u, value, len);
    }
    if (pad > len) {
        memset(buffer + *offset + 4u + len, 0, pad - len);
    }
    *offset += 4u + pad;
    return 1;
}

static int append_xor_address(uint8_t *buffer,
                              size_t buffer_len,
                              size_t *offset,
                              uint16_t type,
                              const char *ip,
                              unsigned short port)
{
    uint8_t value[8];
    struct in_addr address;
    uint32_t host_addr;

    if (inet_pton(AF_INET, ip, &address) != 1) {
        return 0;
    }
    memset(value, 0, sizeof(value));
    value[1] = 0x01u;
    write_be16(value + 2u, (uint16_t) (port ^ (uint16_t) (MRTC_STUN_MAGIC_COOKIE >> 16u)));
    host_addr = ntohl(address.s_addr) ^ MRTC_STUN_MAGIC_COOKIE;
    write_be32(value + 4u, host_addr);
    return append_attr(buffer, buffer_len, offset, type, value, (uint16_t) sizeof(value));
}

static int make_header(uint8_t *buffer,
                       size_t buffer_len,
                       uint16_t type,
                       const uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN])
{
    if (buffer_len < 20u) {
        return 0;
    }
    memset(buffer, 0, buffer_len);
    write_be16(buffer, type);
    write_be32(buffer + 4u, MRTC_STUN_MAGIC_COOKIE);
    memcpy(buffer + 8u, transaction_id, MRTC_STUN_TRANSACTION_ID_LEN);
    return 1;
}

int main(void)
{
    uint8_t request[20];
    uint8_t credentialed_request[256];
    uint8_t txn[MRTC_STUN_TRANSACTION_ID_LEN];
    uint8_t response[32] = {
        0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b,
        0x00, 0x20, 0x00, 0x08, 0x00, 0x01, 0x9f, 0xc4,
        0xe1, 0x12, 0xa6, 0x43
    };
    size_t written = 0;
    MRTC_STUN_XOR_MAPPED_ADDRESS address;
    MRTC_STUN_BINDING_REQUEST parsed_request;
    MRTC_STUN_MESSAGE parsed_message;
    uint8_t turn_key[MRTC_STUN_LONG_TERM_KEY_LEN];
    const uint8_t expected_key[MRTC_STUN_LONG_TERM_KEY_LEN] = {
        0xab, 0xca, 0x35, 0x35, 0x6f, 0x4b, 0x00, 0xfb,
        0xc3, 0x3e, 0x2d, 0x8c, 0x2c, 0x43, 0xb9, 0xd6
    };
    uint8_t turn_packet[512];
    uint8_t parsed_txn[MRTC_STUN_TRANSACTION_ID_LEN];
    uint8_t payload[] = {0xde, 0xad, 0xbe, 0xef};
    size_t offset;

    if (mrtc_stun_write_binding_request(request, sizeof(request), &written, txn) != MRTC_STATUS_OK || written != 20) {
        return 1;
    }
    if (request[4] != 0x21 || request[5] != 0x12 || request[6] != 0xa4 || request[7] != 0x42) {
        return 1;
    }
    if (mrtc_stun_validate_header(request, sizeof(request)) != MRTC_STATUS_OK) {
        return 1;
    }
    request[4] = 0x20;
    if (mrtc_stun_validate_header(request, sizeof(request)) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }
    if (mrtc_stun_validate_header(response, 8) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }
    if (mrtc_stun_parse_xor_mapped_address(response, sizeof(response), &address) != MRTC_STATUS_OK) {
        return 1;
    }
    if (strcmp(address.ip, "192.0.2.1") != 0 || address.port != 48854) {
        return 1;
    }
    if (mrtc_stun_validate_message_integrity_input("user", "pass") != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_stun_write_binding_request_with_credentials(credentialed_request,
                                                         sizeof(credentialed_request),
                                                         &written,
                                                         txn,
                                                         "remote:local",
                                                         "pass") != MRTC_STATUS_OK ||
        written <= 20) {
        return 1;
    }
    if (mrtc_stun_parse_binding_request(credentialed_request,
                                        written,
                                        "pass",
                                        &parsed_request) != MRTC_STATUS_OK ||
        memcmp(parsed_request.transaction_id, txn, MRTC_STUN_TRANSACTION_ID_LEN) != 0 ||
        strcmp(parsed_request.username, "remote:local") != 0 ||
        !parsed_request.has_message_integrity ||
        !parsed_request.message_integrity_valid ||
        !parsed_request.has_fingerprint ||
        !parsed_request.fingerprint_valid) {
        return 1;
    }
    credentialed_request[written - 1u] ^= 0x01u;
    if (mrtc_stun_parse_binding_request(credentialed_request,
                                        written,
                                        "pass",
                                        &parsed_request) != MRTC_STATUS_OK ||
        parsed_request.fingerprint_valid) {
        return 1;
    }
    credentialed_request[written - 1u] ^= 0x01u;
    if (mrtc_stun_parse_binding_request(credentialed_request,
                                        written,
                                        "wrong",
                                        &parsed_request) != MRTC_STATUS_OK ||
        parsed_request.message_integrity_valid) {
        return 1;
    }
    if (mrtc_stun_write_binding_success_response(credentialed_request,
                                                 sizeof(credentialed_request),
                                                 &written,
                                                 txn,
                                                 "192.0.2.1",
                                                 48854,
                                                 "pass") != MRTC_STATUS_OK ||
        mrtc_stun_parse_xor_mapped_address(credentialed_request, written, &address) != MRTC_STATUS_OK ||
        strcmp(address.ip, "192.0.2.1") != 0 ||
        address.port != 48854) {
        return 1;
    }
    memcpy(parsed_txn, response + 8u, MRTC_STUN_TRANSACTION_ID_LEN);
    if (mrtc_stun_parse_response(response,
                                 sizeof(response),
                                 MRTC_STUN_TYPE_BINDING_SUCCESS_RESPONSE,
                                 parsed_txn,
                                 0,
                                 0,
                                 &parsed_message) != MRTC_STATUS_OK ||
        !parsed_message.has_xor_mapped_address ||
        strcmp(parsed_message.xor_mapped_address.ip, "192.0.2.1") != 0 ||
        parsed_message.xor_mapped_address.port != 48854) {
        return 1;
    }
    parsed_txn[0] ^= 0x01u;
    if (mrtc_stun_parse_response(response,
                                 sizeof(response),
                                 MRTC_STUN_TYPE_BINDING_SUCCESS_RESPONSE,
                                 parsed_txn,
                                 0,
                                 0,
                                 &parsed_message) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }
    if (mrtc_stun_make_long_term_key("user", "example.org", "pass", turn_key) != MRTC_STATUS_OK ||
        memcmp(turn_key, expected_key, sizeof(turn_key)) != 0) {
        return 1;
    }
    if (mrtc_stun_write_turn_allocate_request(turn_packet,
                                              sizeof(turn_packet),
                                              &written,
                                              txn) != MRTC_STATUS_OK ||
        written <= 20u ||
        mrtc_stun_parse_message(turn_packet, written, 0, 0, &parsed_message) != MRTC_STATUS_OK ||
        parsed_message.message_type != MRTC_STUN_TYPE_ALLOCATE_REQUEST ||
        parsed_message.has_message_integrity) {
        return 1;
    }
    if (mrtc_stun_write_turn_allocate_request_with_credentials(turn_packet,
                                                               sizeof(turn_packet),
                                                               &written,
                                                               txn,
                                                               "user",
                                                               "example.org",
                                                               "nonce-1",
                                                               turn_key) != MRTC_STATUS_OK ||
        mrtc_stun_parse_message(turn_packet,
                                written,
                                turn_key,
                                MRTC_STUN_LONG_TERM_KEY_LEN,
                                &parsed_message) != MRTC_STATUS_OK ||
        parsed_message.message_type != MRTC_STUN_TYPE_ALLOCATE_REQUEST ||
        !parsed_message.has_message_integrity ||
        !parsed_message.message_integrity_valid ||
        !parsed_message.has_fingerprint ||
        !parsed_message.fingerprint_valid) {
        return 1;
    }
    if (mrtc_stun_write_turn_create_permission_request(turn_packet,
                                                       sizeof(turn_packet),
                                                       &written,
                                                       txn,
                                                       "203.0.113.9",
                                                       5004,
                                                       "user",
                                                       "example.org",
                                                       "nonce-1",
                                                       turn_key) != MRTC_STATUS_OK ||
        mrtc_stun_parse_message(turn_packet,
                                written,
                                turn_key,
                                MRTC_STUN_LONG_TERM_KEY_LEN,
                                &parsed_message) != MRTC_STATUS_OK ||
        parsed_message.message_type != MRTC_STUN_TYPE_CREATE_PERMISSION_REQUEST ||
        !parsed_message.has_xor_peer_address ||
        strcmp(parsed_message.xor_peer_address.ip, "203.0.113.9") != 0 ||
        parsed_message.xor_peer_address.port != 5004 ||
        !parsed_message.message_integrity_valid) {
        return 1;
    }
    if (mrtc_stun_write_turn_send_indication(turn_packet,
                                             sizeof(turn_packet),
                                             &written,
                                             txn,
                                             "203.0.113.9",
                                             5004,
                                             payload,
                                             sizeof(payload)) != MRTC_STATUS_OK ||
        mrtc_stun_parse_message(turn_packet, written, 0, 0, &parsed_message) != MRTC_STATUS_OK ||
        parsed_message.message_type != MRTC_STUN_TYPE_SEND_INDICATION ||
        !parsed_message.has_xor_peer_address ||
        !parsed_message.has_data ||
        parsed_message.data_len != sizeof(payload) ||
        memcmp(parsed_message.data, payload, sizeof(payload)) != 0) {
        return 1;
    }
    write_be16(turn_packet, MRTC_STUN_TYPE_DATA_INDICATION);
    if (mrtc_stun_parse_turn_data_indication(turn_packet, written, &parsed_message) != MRTC_STATUS_OK ||
        strcmp(parsed_message.xor_peer_address.ip, "203.0.113.9") != 0 ||
        parsed_message.xor_peer_address.port != 5004 ||
        parsed_message.data_len != sizeof(payload) ||
        memcmp(parsed_message.data, payload, sizeof(payload)) != 0) {
        return 1;
    }

    if (!make_header(turn_packet, sizeof(turn_packet), MRTC_STUN_TYPE_ALLOCATE_ERROR_RESPONSE, txn)) {
        return 1;
    }
    offset = 20u;
    {
        uint8_t error_code[16] = {
            0x00, 0x00, 0x04, 0x01,
            'U', 'n', 'a', 'u', 't', 'h', 'o', 'r', 'i', 'z', 'e', 'd'
        };
        if (!append_attr(turn_packet, sizeof(turn_packet), &offset, 0x0009u, error_code, sizeof(error_code)) ||
            !append_attr(turn_packet,
                         sizeof(turn_packet),
                         &offset,
                         0x0014u,
                         (const uint8_t *) "example.org",
                         (uint16_t) strlen("example.org")) ||
            !append_attr(turn_packet,
                         sizeof(turn_packet),
                         &offset,
                         0x0015u,
                         (const uint8_t *) "nonce-1",
                         (uint16_t) strlen("nonce-1"))) {
            return 1;
        }
    }
    write_be16(turn_packet + 2u, (uint16_t) (offset - 20u));
    if (mrtc_stun_parse_turn_allocate_response(turn_packet,
                                               offset,
                                               txn,
                                               0,
                                               0,
                                               &parsed_message) != MRTC_STATUS_OK ||
        parsed_message.message_type != MRTC_STUN_TYPE_ALLOCATE_ERROR_RESPONSE ||
        parsed_message.error_code != 401u ||
        strcmp(parsed_message.realm, "example.org") != 0 ||
        strcmp(parsed_message.nonce, "nonce-1") != 0) {
        return 1;
    }
    if (!make_header(turn_packet, sizeof(turn_packet), MRTC_STUN_TYPE_ALLOCATE_SUCCESS_RESPONSE, txn)) {
        return 1;
    }
    offset = 20u;
    if (!append_xor_address(turn_packet, sizeof(turn_packet), &offset, 0x0016u, "203.0.113.7", 49152)) {
        return 1;
    }
    write_be16(turn_packet + 2u, (uint16_t) (offset - 20u));
    if (mrtc_stun_parse_turn_allocate_response(turn_packet,
                                               offset,
                                               txn,
                                               0,
                                               0,
                                               &parsed_message) != MRTC_STATUS_OK ||
        !parsed_message.has_xor_relayed_address ||
        strcmp(parsed_message.xor_relayed_address.ip, "203.0.113.7") != 0 ||
        parsed_message.xor_relayed_address.port != 49152) {
        return 1;
    }
    return 0;
}
