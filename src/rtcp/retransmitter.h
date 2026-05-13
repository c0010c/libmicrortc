#ifndef MRTC_RTCP_RETRANSMITTER_H
#define MRTC_RTCP_RETRANSMITTER_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

typedef struct MRTC_RTCP_RETRANSMIT_RESULT {
    size_t requested_count;
    size_t retransmitted_count;
    size_t missing_count;
} MRTC_RTCP_RETRANSMIT_RESULT;

MRTC_STATUS mrtc_rtcp_retransmit_nack(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                      const uint8_t *rtcp_packet,
                                      size_t rtcp_packet_size,
                                      MRTC_RTCP_RETRANSMIT_RESULT *result);

#endif /* MRTC_RTCP_RETRANSMITTER_H */
