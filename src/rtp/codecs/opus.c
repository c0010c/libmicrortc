#include "opus.h"

#include <string.h>

MRTC_STATUS mrtc_opus_payload_size(const MRTC_FRAME *frame, size_t *payload_size)
{
    if (frame == 0 || payload_size == 0 || (frame->data == 0 && frame->size > 0u) || frame->size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    *payload_size = frame->size;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_opus_payload_frame(const MRTC_FRAME *frame, uint8_t *payload, size_t *payload_size)
{
    size_t required_size;
    MRTC_STATUS status = mrtc_opus_payload_size(frame, &required_size);

    if (status != MRTC_STATUS_OK) {
        if (payload_size != 0) {
            *payload_size = 0;
        }
        return status;
    }
    if (payload_size == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (payload == 0) {
        *payload_size = required_size;
        return MRTC_STATUS_OK;
    }
    if (*payload_size < required_size) {
        *payload_size = required_size;
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(payload, frame->data, required_size);
    *payload_size = required_size;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_opus_depayload(const uint8_t *payload, size_t payload_size, uint8_t *frame_data, size_t *frame_size)
{
    if (payload == 0 || frame_size == 0 || payload_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (frame_data == 0) {
        *frame_size = payload_size;
        return MRTC_STATUS_OK;
    }
    if (*frame_size < payload_size) {
        *frame_size = payload_size;
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(frame_data, payload, payload_size);
    *frame_size = payload_size;
    return MRTC_STATUS_OK;
}

uint32_t mrtc_opus_rtp_timestamp_from_frame(const MRTC_FRAME *frame)
{
    if (frame == 0) {
        return 0;
    }
    return mrtc_opus_timestamp_100ns_to_rtp(frame->presentation_ts);
}

uint32_t mrtc_opus_timestamp_100ns_to_rtp(uint64_t timestamp_100ns)
{
    return (uint32_t) ((timestamp_100ns * (uint64_t) MRTC_OPUS_CLOCK_RATE) / MRTC_100NS_PER_SECOND);
}
