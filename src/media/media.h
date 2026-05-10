#ifndef RTC_MEDIA_INTERNAL_H
#define RTC_MEDIA_INTERNAL_H

#include "api/peer_connection.h"
#include "rtc/status.h"

rtc_status_t rtc_media_send_frame(rtc_peer_connection_t *pc,
                                  const rtc_media_frame_t *frame);
rtc_status_t rtc_media_handle_rtp_datagram(rtc_peer_connection_t *pc,
                                           const uint8_t *data,
                                           size_t data_len);
rtc_status_t rtc_media_handle_rtcp_datagram(rtc_peer_connection_t *pc,
                                            const uint8_t *data,
                                            size_t data_len);
rtc_status_t rtc_media_send_rtcp_reports(rtc_peer_connection_t *pc);
rtc_status_t rtc_media_request_keyframe(rtc_peer_connection_t *pc,
                                        rtc_media_kind_t kind);
void rtc_media_emit_pli_feedback(rtc_peer_connection_t *pc,
                                 uint32_t media_ssrc);
void rtc_media_emit_nack_feedback(rtc_peer_connection_t *pc,
                                  uint32_t media_ssrc,
                                  const rtc_media_feedback_t *feedback);

#endif
