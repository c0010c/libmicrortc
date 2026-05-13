#ifndef MRTC_MEDIA_TRANSCEIVER_H
#define MRTC_MEDIA_TRANSCEIVER_H

#include <micrortc/peer_connection.h>

#include <stdint.h>

struct MRTC_RTP_TRANSCEIVER {
    MRTC_PEER_CONNECTION_HANDLE owner;
    MRTC_MEDIA_KIND kind;
    MRTC_CODEC codec;
    MRTC_RTP_TRANSCEIVER_DIRECTION direction;
    char mid[16];
    uint32_t local_ssrc;
    uint32_t remote_ssrc;
    unsigned char payload_type;
    uint16_t sequence_number;
    MRTC_TRANSCEIVER_CALLBACKS callbacks;
    void *user_data;
    struct MRTC_RTP_TRANSCEIVER *next;
};

MRTC_RTP_TRANSCEIVER_HANDLE mrtc_media_transceiver_alloc(MRTC_PEER_CONNECTION_HANDLE owner,
                                                         const MRTC_TRANSCEIVER_INIT *init,
                                                         void *user_data,
                                                         const char *mid,
                                                         uint32_t local_ssrc,
                                                         unsigned char payload_type);
void mrtc_media_transceiver_free_internal(MRTC_RTP_TRANSCEIVER_HANDLE transceiver);
int mrtc_media_transceiver_kind_codec_valid(MRTC_MEDIA_KIND kind, MRTC_CODEC codec);
int mrtc_media_transceiver_direction_valid(MRTC_RTP_TRANSCEIVER_DIRECTION direction);
const char *mrtc_media_transceiver_direction_name(MRTC_RTP_TRANSCEIVER_DIRECTION direction);

#endif /* MRTC_MEDIA_TRANSCEIVER_H */
