#ifndef MRTC_RTP_CODECS_OPUS_H
#define MRTC_RTP_CODECS_OPUS_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRTC_OPUS_CLOCK_RATE 48000u
#define MRTC_100NS_PER_SECOND 10000000ull

MRTC_STATUS mrtc_opus_payload_size(const MRTC_FRAME *frame, size_t *payload_size);
MRTC_STATUS mrtc_opus_payload_frame(const MRTC_FRAME *frame, uint8_t *payload, size_t *payload_size);
MRTC_STATUS mrtc_opus_depayload(const uint8_t *payload, size_t payload_size, uint8_t *frame_data, size_t *frame_size);
uint32_t mrtc_opus_rtp_timestamp_from_frame(const MRTC_FRAME *frame);
uint32_t mrtc_opus_timestamp_100ns_to_rtp(uint64_t timestamp_100ns);

#ifdef __cplusplus
}
#endif

#endif /* MRTC_RTP_CODECS_OPUS_H */
