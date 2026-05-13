#include "retransmitter.h"

#include "../media/media_transceiver.h"
#include "rtcp_packet.h"
#include "rtp_rolling_buffer.h"

#include <stdlib.h>
#include <string.h>

static MRTC_RTP_TRANSCEIVER_HANDLE mrtc_find_sender_by_ssrc(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                            uint32_t media_ssrc)
{
    MRTC_RTP_TRANSCEIVER_HANDLE current;

    if (peer_connection == 0 || media_ssrc == 0u) {
        return 0;
    }
    current = mrtc_peer_connection_get_transceivers(peer_connection);
    while (current != 0) {
        if (current->local_ssrc == media_ssrc) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

MRTC_STATUS mrtc_rtcp_retransmit_nack(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                      const uint8_t *rtcp_packet,
                                      size_t rtcp_packet_size,
                                      MRTC_RTCP_RETRANSMIT_RESULT *result)
{
    uint32_t sender_ssrc = 0;
    uint32_t media_ssrc = 0;
    uint16_t *sequence_numbers = 0;
    size_t sequence_number_count = 0;
    size_t i;
    MRTC_RTP_TRANSCEIVER_HANDLE transceiver;
    MRTC_RTCP_RETRANSMIT_RESULT local_result;
    MRTC_STATUS status;

    (void) sender_ssrc;
    if (peer_connection == 0 || rtcp_packet == 0 || rtcp_packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(&local_result, 0, sizeof(local_result));
    status = mrtc_rtcp_parse_nack(rtcp_packet,
                                  rtcp_packet_size,
                                  &sender_ssrc,
                                  &media_ssrc,
                                  0,
                                  &sequence_number_count);
    if (status != MRTC_STATUS_OK || sequence_number_count == 0u) {
        if (result != 0) {
            *result = local_result;
        }
        return status;
    }

    transceiver = mrtc_find_sender_by_ssrc(peer_connection, media_ssrc);
    if (transceiver == 0 || transceiver->rtp_rolling_buffer == 0 ||
        transceiver->direction == MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY ||
        transceiver->direction == MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
        if (result != 0) {
            *result = local_result;
        }
        return MRTC_STATUS_INVALID_STATE;
    }

    sequence_numbers = (uint16_t *) calloc(sequence_number_count, sizeof(*sequence_numbers));
    if (sequence_numbers == 0) {
        if (result != 0) {
            *result = local_result;
        }
        return MRTC_STATUS_INVALID_ARG;
    }

    local_result.requested_count = sequence_number_count;
    status = mrtc_rtcp_parse_nack(rtcp_packet,
                                  rtcp_packet_size,
                                  &sender_ssrc,
                                  &media_ssrc,
                                  sequence_numbers,
                                  &sequence_number_count);
    if (status != MRTC_STATUS_OK) {
        free(sequence_numbers);
        if (result != 0) {
            *result = local_result;
        }
        return status;
    }

    for (i = 0; i < sequence_number_count; ++i) {
        const uint8_t *packet = 0;
        size_t packet_size = 0;

        status = mrtc_rtp_rolling_buffer_lookup(transceiver->rtp_rolling_buffer,
                                                sequence_numbers[i],
                                                &packet,
                                                &packet_size);
        if (status == MRTC_STATUS_INVALID_STATE) {
            local_result.missing_count++;
            continue;
        }
        if (status != MRTC_STATUS_OK) {
            free(sequence_numbers);
            if (result != 0) {
                *result = local_result;
            }
            return status;
        }
        status = mrtc_peer_connection_send_protected_media_packet(peer_connection, transceiver, packet, packet_size);
        if (status != MRTC_STATUS_OK) {
            free(sequence_numbers);
            if (result != 0) {
                *result = local_result;
            }
            return status;
        }
        local_result.retransmitted_count++;
    }

    transceiver->nack_packets_received++;
    transceiver->retransmitted_packets_sent += local_result.retransmitted_count;
    transceiver->retransmit_packets_missing += local_result.missing_count;

    free(sequence_numbers);
    if (result != 0) {
        *result = local_result;
    }
    return MRTC_STATUS_OK;
}
