#ifndef MRTC_SDP_H
#define MRTC_SDP_H

#include <micrortc/micrortc.h>

#include <stddef.h>

typedef struct MRTC_SDP MRTC_SDP;

MRTC_STATUS mrtc_sdp_parse(const char *sdp, MRTC_SDP **parsed_sdp);
MRTC_STATUS mrtc_sdp_serialize(const MRTC_SDP *parsed_sdp, char *buffer, size_t buffer_len, size_t *required_len);
MRTC_STATUS mrtc_sdp_create_answer(const char *remote_offer, char *buffer, size_t buffer_len, size_t *required_len);
MRTC_STATUS mrtc_sdp_create_answer_ex(const char *remote_offer,
                                      int include_data_channel,
                                      const char *local_ice_ufrag,
                                      const char *local_ice_pwd,
                                      const char *local_fingerprint,
                                      char *buffer,
                                      size_t buffer_len,
                                      size_t *required_len);
MRTC_STATUS mrtc_sdp_create_answer_with_media(const char *remote_offer,
                                              int include_data_channel,
                                              MRTC_RTP_TRANSCEIVER_HANDLE transceivers,
                                              const char *local_ice_ufrag,
                                              const char *local_ice_pwd,
                                              const char *local_fingerprint,
                                              char *buffer,
                                              size_t buffer_len,
                                              size_t *required_len);
MRTC_STATUS mrtc_sdp_create_offer_with_media(int include_data_channel,
                                             MRTC_RTP_TRANSCEIVER_HANDLE transceivers,
                                             const char *local_ice_ufrag,
                                             const char *local_ice_pwd,
                                             const char *local_fingerprint,
                                             char *buffer,
                                             size_t buffer_len,
                                             size_t *required_len);
const char *mrtc_sdp_get_setup(const MRTC_SDP *parsed_sdp);
const char *mrtc_sdp_get_fingerprint(const MRTC_SDP *parsed_sdp);
int mrtc_sdp_has_application(const MRTC_SDP *parsed_sdp);
void mrtc_sdp_free(MRTC_SDP *parsed_sdp);

#endif /* MRTC_SDP_H */
