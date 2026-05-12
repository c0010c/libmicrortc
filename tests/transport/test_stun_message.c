#include "../../src/stun/stun_message.h"

#include <stdint.h>
#include <string.h>

int main(void)
{
    uint8_t request[20];
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
    return 0;
}
