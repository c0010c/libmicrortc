#include "media_transceiver.h"

#include "../rtcp/rtp_rolling_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MRTC_TRANSCEIVER_RTP_ROLLING_BUFFER_CAPACITY 128u

int mrtc_media_transceiver_kind_codec_valid(MRTC_MEDIA_KIND kind, MRTC_CODEC codec)
{
    return (kind == MRTC_MEDIA_KIND_VIDEO && codec == MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1) ||
           (kind == MRTC_MEDIA_KIND_AUDIO && codec == MRTC_CODEC_OPUS);
}

int mrtc_media_transceiver_direction_valid(MRTC_RTP_TRANSCEIVER_DIRECTION direction)
{
    return direction == MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV ||
           direction == MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY ||
           direction == MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY ||
           direction == MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE;
}

const char *mrtc_media_transceiver_direction_name(MRTC_RTP_TRANSCEIVER_DIRECTION direction)
{
    switch (direction) {
        case MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV:
            return "sendrecv";
        case MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY:
            return "sendonly";
        case MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY:
            return "recvonly";
        case MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE:
            return "inactive";
        default:
            return "inactive";
    }
}

MRTC_RTP_TRANSCEIVER_HANDLE mrtc_media_transceiver_alloc(MRTC_PEER_CONNECTION_HANDLE owner,
                                                         const MRTC_TRANSCEIVER_INIT *init,
                                                         void *user_data,
                                                         const char *mid,
                                                         uint32_t local_ssrc,
                                                         unsigned char payload_type)
{
    MRTC_RTP_TRANSCEIVER_HANDLE transceiver;

    if (owner == 0 || init == 0 || mid == 0 ||
        !mrtc_media_transceiver_kind_codec_valid(init->kind, init->codec) ||
        !mrtc_media_transceiver_direction_valid(init->direction)) {
        return 0;
    }

    transceiver = (MRTC_RTP_TRANSCEIVER_HANDLE) calloc(1, sizeof(*transceiver));
    if (transceiver == 0) {
        return 0;
    }

    transceiver->owner = owner;
    transceiver->kind = init->kind;
    transceiver->codec = init->codec;
    transceiver->direction = init->direction;
    transceiver->local_ssrc = local_ssrc;
    transceiver->payload_type = payload_type;
    transceiver->callbacks = init->callbacks;
    transceiver->user_data = user_data;
    (void) snprintf(transceiver->mid, sizeof(transceiver->mid), "%s", mid);
    if (mrtc_rtp_rolling_buffer_create(MRTC_TRANSCEIVER_RTP_ROLLING_BUFFER_CAPACITY,
                                       &transceiver->rtp_rolling_buffer) != MRTC_STATUS_OK) {
        free(transceiver);
        return 0;
    }
    return transceiver;
}

void mrtc_media_transceiver_free_internal(MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    if (transceiver != 0) {
        mrtc_rtp_rolling_buffer_free(transceiver->rtp_rolling_buffer);
        free(transceiver->receive_frame_buffer);
    }
    free(transceiver);
}
