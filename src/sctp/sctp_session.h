#ifndef MRTC_SCTP_SESSION_H
#define MRTC_SCTP_SESSION_H

#include <micrortc/micrortc.h>

#include <stddef.h>
#include <stdint.h>

#define MRTC_SCTP_PORT 5000u
#define MRTC_SCTP_PPID_DCEP 50u
#define MRTC_SCTP_PPID_STRING 51u
#define MRTC_SCTP_PPID_STRING_EMPTY 56u
#define MRTC_SCTP_PPID_BINARY 53u
#define MRTC_SCTP_PPID_BINARY_EMPTY 57u

typedef void (*MRTC_SCTP_OUTBOUND_CALLBACK)(void *user_data,
                                            const uint8_t *packet,
                                            size_t packet_len);
typedef void (*MRTC_SCTP_MESSAGE_CALLBACK)(void *user_data,
                                           uint16_t stream_id,
                                           uint32_t ppid,
                                           const uint8_t *message,
                                           size_t message_len);

typedef struct MRTC_SCTP_SESSION {
    int initialized;
    int connected;
    int dcep_open_received;
    int dcep_ack_sent;
    MRTC_SCTP_OUTBOUND_CALLBACK on_outbound;
    MRTC_SCTP_MESSAGE_CALLBACK on_message;
    void *user_data;
} MRTC_SCTP_SESSION;

MRTC_STATUS mrtc_sctp_global_init(void);
void mrtc_sctp_global_deinit(void);
MRTC_STATUS mrtc_sctp_session_init(MRTC_SCTP_SESSION *session);
void mrtc_sctp_session_set_callbacks(MRTC_SCTP_SESSION *session,
                                     MRTC_SCTP_OUTBOUND_CALLBACK on_outbound,
                                     MRTC_SCTP_MESSAGE_CALLBACK on_message,
                                     void *user_data);
MRTC_STATUS mrtc_sctp_session_connect(MRTC_SCTP_SESSION *session);
MRTC_STATUS mrtc_sctp_session_receive_dcep_open(MRTC_SCTP_SESSION *session,
                                                uint16_t stream_id,
                                                const char *label);
MRTC_STATUS mrtc_sctp_session_receive_message(MRTC_SCTP_SESSION *session,
                                              uint16_t stream_id,
                                              uint32_t ppid,
                                              const uint8_t *message,
                                              size_t message_len);
MRTC_STATUS mrtc_sctp_session_write_message(MRTC_SCTP_SESSION *session,
                                            uint16_t stream_id,
                                            int is_binary,
                                            const uint8_t *message,
                                            size_t message_len);
void mrtc_sctp_session_deinit(MRTC_SCTP_SESSION *session);

#endif /* MRTC_SCTP_SESSION_H */
