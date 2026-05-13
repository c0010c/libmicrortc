#include "media_transceiver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    return transceiver;
}

void mrtc_media_transceiver_free_internal(MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    free(transceiver);
}
