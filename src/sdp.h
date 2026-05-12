#ifndef MRTC_SDP_H
#define MRTC_SDP_H

#include <micrortc/micrortc.h>

#include <stddef.h>

typedef struct MRTC_SDP MRTC_SDP;

MRTC_STATUS mrtc_sdp_parse(const char *sdp, MRTC_SDP **parsed_sdp);
MRTC_STATUS mrtc_sdp_serialize(const MRTC_SDP *parsed_sdp, char *buffer, size_t buffer_len, size_t *required_len);
MRTC_STATUS mrtc_sdp_create_answer(const char *remote_offer, char *buffer, size_t buffer_len, size_t *required_len);
void mrtc_sdp_free(MRTC_SDP *parsed_sdp);

#endif /* MRTC_SDP_H */
