#ifndef MRTC_MEDIA_TRANSCEIVER_H
#define MRTC_MEDIA_TRANSCEIVER_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

typedef MRTC_STATUS (*MRTC_MEDIA_SEND_HOOK)(void *user_data,
                                            MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                            MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                            const uint8_t *packet,
                                            size_t packet_size);

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
    uint64_t frames_sent;
    uint64_t frames_received;
    uint8_t *receive_frame_buffer;
    size_t receive_frame_size;
    size_t receive_frame_capacity;
    uint32_t receive_frame_timestamp;
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
MRTC_STATUS mrtc_peer_connection_set_media_send_hook(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                     MRTC_MEDIA_SEND_HOOK hook,
                                                     void *user_data);
int mrtc_peer_connection_media_is_srtp_passthrough(MRTC_PEER_CONNECTION_HANDLE peer_connection);
MRTC_STATUS mrtc_peer_connection_send_protected_media_packet(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                            MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                            const uint8_t *packet,
                                                            size_t packet_size);
MRTC_STATUS mrtc_peer_connection_receive_protected_media_packet(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                               const uint8_t *packet,
                                                               size_t packet_size);

#endif /* MRTC_MEDIA_TRANSCEIVER_H */
