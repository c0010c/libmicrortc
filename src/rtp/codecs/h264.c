#include "h264.h"

#include <string.h>

typedef struct MRTC_H264_NALU {
    const uint8_t *data;
    size_t size;
} MRTC_H264_NALU;

static int mrtc_h264_start_code_size_at(const uint8_t *data, size_t size, size_t offset)
{
    if (offset + 3u <= size && data[offset] == 0u && data[offset + 1u] == 0u && data[offset + 2u] == 1u) {
        return 3;
    }
    if (offset + 4u <= size && data[offset] == 0u && data[offset + 1u] == 0u &&
        data[offset + 2u] == 0u && data[offset + 3u] == 1u) {
        return 4;
    }
    return 0;
}

static MRTC_STATUS mrtc_h264_find_nalus(const uint8_t *annexb,
                                        size_t annexb_size,
                                        MRTC_H264_NALU *nalus,
                                        size_t *nalu_count)
{
    size_t offset = 0;
    size_t count = 0;
    size_t capacity;

    if (annexb == 0 || nalu_count == 0 || annexb_size < 4u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    capacity = *nalu_count;
    if (mrtc_h264_start_code_size_at(annexb, annexb_size, 0u) == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    while (offset < annexb_size) {
        int start_code_size = mrtc_h264_start_code_size_at(annexb, annexb_size, offset);
        size_t nalu_start;
        size_t next_start;

        if (start_code_size == 0) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        nalu_start = offset + (size_t) start_code_size;
        if (nalu_start >= annexb_size) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        next_start = nalu_start;
        while (next_start < annexb_size && mrtc_h264_start_code_size_at(annexb, annexb_size, next_start) == 0) {
            ++next_start;
        }
        if (next_start == nalu_start) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        if (nalus != 0) {
            if (count >= capacity) {
                return MRTC_STATUS_INVALID_ARG;
            }
            nalus[count].data = annexb + nalu_start;
            nalus[count].size = next_start - nalu_start;
        }
        ++count;
        offset = next_start;
    }

    *nalu_count = count;
    return count == 0 ? MRTC_STATUS_PARSE_ERROR : MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_h264_count_nalu_payloads(const MRTC_H264_NALU *nalu,
                                                 size_t mtu,
                                                 size_t *payload_size,
                                                 size_t *payload_count)
{
    size_t max_fragment_data;
    size_t remaining;

    if (nalu == 0 || payload_size == 0 || payload_count == 0 || nalu->data == 0 || nalu->size == 0u || mtu <= 2u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (nalu->size <= mtu) {
        *payload_size += nalu->size;
        *payload_count += 1u;
        return MRTC_STATUS_OK;
    }

    max_fragment_data = mtu - 2u;
    remaining = nalu->size - 1u;
    while (remaining > 0u) {
        size_t fragment_size = remaining < max_fragment_data ? remaining : max_fragment_data;
        *payload_size += 2u + fragment_size;
        *payload_count += 1u;
        remaining -= fragment_size;
    }
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_h264_write_nalu_payloads(const MRTC_H264_NALU *nalu,
                                                 size_t mtu,
                                                 uint8_t **payload_cursor,
                                                 size_t *payload_remaining,
                                                 size_t **length_cursor,
                                                 size_t *length_remaining)
{
    uint8_t nalu_header;
    uint8_t nalu_type;
    uint8_t nalu_nri;
    size_t max_fragment_data;
    size_t remaining;
    const uint8_t *fragment_cursor;

    if (nalu == 0 || payload_cursor == 0 || *payload_cursor == 0 || payload_remaining == 0 ||
        length_cursor == 0 || *length_cursor == 0 || length_remaining == 0 || mtu <= 2u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (nalu->size <= mtu) {
        if (*payload_remaining < nalu->size || *length_remaining < 1u) {
            return MRTC_STATUS_INVALID_ARG;
        }
        memcpy(*payload_cursor, nalu->data, nalu->size);
        **length_cursor = nalu->size;
        *payload_cursor += nalu->size;
        *payload_remaining -= nalu->size;
        *length_cursor += 1;
        *length_remaining -= 1u;
        return MRTC_STATUS_OK;
    }

    nalu_header = nalu->data[0];
    nalu_type = nalu_header & MRTC_H264_NAL_TYPE_MASK;
    nalu_nri = nalu_header & MRTC_H264_NRI_MASK;
    max_fragment_data = mtu - 2u;
    remaining = nalu->size - 1u;
    fragment_cursor = nalu->data + 1u;

    while (remaining > 0u) {
        size_t fragment_size = remaining < max_fragment_data ? remaining : max_fragment_data;
        uint8_t fu_header = nalu_type;

        if (*payload_remaining < 2u + fragment_size || *length_remaining < 1u) {
            return MRTC_STATUS_INVALID_ARG;
        }
        if (fragment_cursor == nalu->data + 1u) {
            fu_header |= 0x80u;
        }
        if (remaining == fragment_size) {
            fu_header |= 0x40u;
        }

        (*payload_cursor)[0] = nalu_nri | MRTC_H264_FU_A_TYPE;
        (*payload_cursor)[1] = fu_header;
        memcpy(*payload_cursor + 2u, fragment_cursor, fragment_size);
        **length_cursor = 2u + fragment_size;
        *payload_cursor += 2u + fragment_size;
        *payload_remaining -= 2u + fragment_size;
        *length_cursor += 1;
        *length_remaining -= 1u;
        fragment_cursor += fragment_size;
        remaining -= fragment_size;
    }
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_h264_packetize_annexb(const uint8_t *annexb,
                                       size_t annexb_size,
                                       size_t mtu,
                                       uint8_t *payloads,
                                       size_t *payloads_size,
                                       size_t *payload_lengths,
                                       size_t *payload_count)
{
    MRTC_H264_NALU stack_nalus[32];
    size_t nalu_count = 32u;
    size_t required_payload_size = 0;
    size_t required_payload_count = 0;
    size_t i;
    MRTC_STATUS status;
    int sizing_only;

    if (payloads_size == 0 || payload_count == 0 || mtu <= 2u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_h264_find_nalus(annexb, annexb_size, stack_nalus, &nalu_count);
    if (status != MRTC_STATUS_OK) {
        *payloads_size = 0;
        *payload_count = 0;
        return status;
    }

    for (i = 0; i < nalu_count; ++i) {
        status = mrtc_h264_count_nalu_payloads(stack_nalus + i, mtu, &required_payload_size, &required_payload_count);
        if (status != MRTC_STATUS_OK) {
            *payloads_size = 0;
            *payload_count = 0;
            return status;
        }
    }

    sizing_only = payloads == 0;
    if (!sizing_only) {
        uint8_t *payload_cursor = payloads;
        size_t payload_remaining = *payloads_size;
        size_t *length_cursor = payload_lengths;
        size_t length_remaining = *payload_count;

        if (payload_lengths == 0 || payload_remaining < required_payload_size || length_remaining < required_payload_count) {
            *payloads_size = required_payload_size;
            *payload_count = required_payload_count;
            return MRTC_STATUS_INVALID_ARG;
        }
        for (i = 0; i < nalu_count; ++i) {
            status = mrtc_h264_write_nalu_payloads(stack_nalus + i, mtu, &payload_cursor, &payload_remaining,
                                                   &length_cursor, &length_remaining);
            if (status != MRTC_STATUS_OK) {
                *payloads_size = required_payload_size;
                *payload_count = required_payload_count;
                return status;
            }
        }
    }

    *payloads_size = required_payload_size;
    *payload_count = required_payload_count;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_h264_depacketize_payload(const uint8_t *payload,
                                          size_t payload_size,
                                          uint8_t *annexb,
                                          size_t *annexb_size,
                                          int *is_start)
{
    static const uint8_t start_code[MRTC_H264_START_CODE_SIZE] = {0x00, 0x00, 0x00, 0x01};
    uint8_t nalu_type;
    size_t required_size;
    int starts_frame = 0;

    if (payload == 0 || annexb_size == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (payload_size == 0u) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    nalu_type = payload[0] & MRTC_H264_NAL_TYPE_MASK;
    if (nalu_type == MRTC_H264_FU_A_TYPE) {
        uint8_t fu_header;
        uint8_t reconstructed_header;
        size_t fragment_size;

        if (payload_size < 3u) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        fu_header = payload[1];
        starts_frame = (fu_header & 0x80u) != 0u;
        fragment_size = payload_size - 2u;
        required_size = fragment_size + (starts_frame ? MRTC_H264_START_CODE_SIZE + 1u : 0u);
        if (annexb == 0) {
            *annexb_size = required_size;
            if (is_start != 0) {
                *is_start = starts_frame;
            }
            return MRTC_STATUS_OK;
        }
        if (*annexb_size < required_size) {
            *annexb_size = required_size;
            return MRTC_STATUS_INVALID_ARG;
        }
        if (starts_frame) {
            memcpy(annexb, start_code, sizeof(start_code));
            reconstructed_header = (uint8_t) ((payload[0] & MRTC_H264_NRI_MASK) | (fu_header & MRTC_H264_NAL_TYPE_MASK));
            annexb[MRTC_H264_START_CODE_SIZE] = reconstructed_header;
            memcpy(annexb + MRTC_H264_START_CODE_SIZE + 1u, payload + 2u, fragment_size);
        } else {
            memcpy(annexb, payload + 2u, fragment_size);
        }
    } else if (nalu_type > 0u && nalu_type < 24u) {
        starts_frame = 1;
        required_size = MRTC_H264_START_CODE_SIZE + payload_size;
        if (annexb == 0) {
            *annexb_size = required_size;
            if (is_start != 0) {
                *is_start = starts_frame;
            }
            return MRTC_STATUS_OK;
        }
        if (*annexb_size < required_size) {
            *annexb_size = required_size;
            return MRTC_STATUS_INVALID_ARG;
        }
        memcpy(annexb, start_code, sizeof(start_code));
        memcpy(annexb + MRTC_H264_START_CODE_SIZE, payload, payload_size);
    } else {
        return MRTC_STATUS_PARSE_ERROR;
    }

    *annexb_size = required_size;
    if (is_start != 0) {
        *is_start = starts_frame;
    }
    return MRTC_STATUS_OK;
}
