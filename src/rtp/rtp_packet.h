#ifndef MRTC_RTP_PACKET_H
#define MRTC_RTP_PACKET_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRTC_RTP_VERSION 2u
#define MRTC_RTP_FIXED_HEADER_SIZE 12u

typedef struct MRTC_RTP_PACKET {
    uint8_t version;
    uint8_t marker;
    uint8_t payload_type;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
    const uint8_t *payload;
    size_t payload_size;
    const uint8_t *raw;
    size_t raw_size;

    uint8_t csrc_count;
    uint8_t has_extension;
    uint8_t has_padding;
    size_t header_size;
} MRTC_RTP_PACKET;

MRTC_STATUS mrtc_rtp_packet_build(MRTC_RTP_PACKET *packet,
                                  uint8_t marker,
                                  uint8_t payload_type,
                                  uint16_t sequence_number,
                                  uint32_t timestamp,
                                  uint32_t ssrc,
                                  const uint8_t *payload,
                                  size_t payload_size);

MRTC_STATUS mrtc_rtp_packet_parse(const uint8_t *raw, size_t raw_size, MRTC_RTP_PACKET *packet);
MRTC_STATUS mrtc_rtp_packet_serialize(const MRTC_RTP_PACKET *packet,
                                      uint8_t *raw,
                                      size_t raw_capacity,
                                      size_t *raw_size);
size_t mrtc_rtp_packet_header_size(const MRTC_RTP_PACKET *packet);
size_t mrtc_rtp_packet_serialized_size(const MRTC_RTP_PACKET *packet);
uint16_t mrtc_rtp_sequence_next(uint16_t sequence_number);

#ifdef __cplusplus
}
#endif

#endif /* MRTC_RTP_PACKET_H */
