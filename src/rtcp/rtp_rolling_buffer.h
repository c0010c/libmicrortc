#ifndef MRTC_RTCP_RTP_ROLLING_BUFFER_H
#define MRTC_RTCP_RTP_ROLLING_BUFFER_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

typedef struct MRTC_RTP_ROLLING_BUFFER MRTC_RTP_ROLLING_BUFFER;

MRTC_STATUS mrtc_rtp_rolling_buffer_create(size_t capacity, MRTC_RTP_ROLLING_BUFFER **buffer);
void mrtc_rtp_rolling_buffer_free(MRTC_RTP_ROLLING_BUFFER *buffer);
MRTC_STATUS mrtc_rtp_rolling_buffer_add(MRTC_RTP_ROLLING_BUFFER *buffer,
                                        uint16_t sequence_number,
                                        const uint8_t *packet,
                                        size_t packet_size);
MRTC_STATUS mrtc_rtp_rolling_buffer_lookup(const MRTC_RTP_ROLLING_BUFFER *buffer,
                                           uint16_t sequence_number,
                                           const uint8_t **packet,
                                           size_t *packet_size);
MRTC_STATUS mrtc_rtp_rolling_buffer_copy(const MRTC_RTP_ROLLING_BUFFER *buffer,
                                         uint16_t sequence_number,
                                         uint8_t *packet,
                                         size_t packet_capacity,
                                         size_t *packet_size);
MRTC_STATUS mrtc_rtp_rolling_buffer_remove(MRTC_RTP_ROLLING_BUFFER *buffer, uint16_t sequence_number);
size_t mrtc_rtp_rolling_buffer_size(const MRTC_RTP_ROLLING_BUFFER *buffer);

#endif /* MRTC_RTCP_RTP_ROLLING_BUFFER_H */
