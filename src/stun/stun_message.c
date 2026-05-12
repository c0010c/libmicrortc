#include "stun_message.h"

#include "common/mrtc_common.h"

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
