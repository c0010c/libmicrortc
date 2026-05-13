#ifndef MRTC_RTP_CODECS_H264_H
#define MRTC_RTP_CODECS_H264_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRTC_H264_FU_A_TYPE 28u
#define MRTC_H264_NAL_TYPE_MASK 0x1fu
#define MRTC_H264_NRI_MASK 0x60u
#define MRTC_H264_START_CODE_SIZE 4u

MRTC_STATUS mrtc_h264_packetize_annexb(const uint8_t *annexb,
                                       size_t annexb_size,
                                       size_t mtu,
                                       uint8_t *payloads,
                                       size_t *payloads_size,
                                       size_t *payload_lengths,
                                       size_t *payload_count);

MRTC_STATUS mrtc_h264_depacketize_payload(const uint8_t *payload,
                                          size_t payload_size,
                                          uint8_t *annexb,
                                          size_t *annexb_size,
                                          int *is_start);

#ifdef __cplusplus
}
#endif

#endif /* MRTC_RTP_CODECS_H264_H */
