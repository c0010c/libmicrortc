#ifndef MRTC_MEDIA_TRANSCEIVER_H
#define MRTC_MEDIA_TRANSCEIVER_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

typedef struct MRTC_RTP_ROLLING_BUFFER MRTC_RTP_ROLLING_BUFFER;
typedef struct MRTC_RTCP_RETRANSMIT_RESULT MRTC_RTCP_RETRANSMIT_RESULT;

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
    uint64_t rtp_packets_sent;
    uint64_t rtp_octets_sent;
    uint32_t last_rtp_timestamp;
    uint64_t nack_packets_received;
    uint64_t retransmitted_packets_sent;
    uint64_t retransmit_packets_missing;
    uint64_t picture_loss_count;
    uint64_t sender_reports_received;
    uint64_t receiver_reports_received;
    uint8_t last_receiver_fraction_lost;
    uint32_t last_receiver_cumulative_lost;
    uint32_t last_receiver_highest_sequence_number;
    uint32_t last_receiver_jitter;
    uint8_t *receive_frame_buffer;
    size_t receive_frame_size;
    size_t receive_frame_capacity;
    uint32_t receive_frame_timestamp;
    MRTC_RTP_ROLLING_BUFFER *rtp_rolling_buffer;
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
MRTC_STATUS mrtc_peer_connection_receive_protected_rtcp_packet(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                              const uint8_t *packet,
                                                              size_t packet_size,
                                                              MRTC_RTCP_RETRANSMIT_RESULT *retransmit_result);
MRTC_RTP_TRANSCEIVER_HANDLE mrtc_peer_connection_get_transceivers(MRTC_PEER_CONNECTION_HANDLE peer_connection);
MRTC_STATUS mrtc_transceiver_generate_sender_report(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                    uint64_t ntp_timestamp,
                                                    uint8_t *raw,
                                                    size_t raw_capacity,
                                                    size_t *raw_size);
MRTC_STATUS mrtc_transceiver_generate_receiver_report(MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                      uint32_t sender_ssrc,
                                                      uint8_t fraction_lost,
                                                      uint32_t cumulative_lost,
                                                      uint32_t jitter,
                                                      uint8_t *raw,
                                                      size_t raw_capacity,
                                                      size_t *raw_size);

#endif /* MRTC_MEDIA_TRANSCEIVER_H */
