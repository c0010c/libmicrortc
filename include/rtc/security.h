#ifndef RTC_SECURITY_H
#define RTC_SECURITY_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtc_security_dtls_role_t {
    RTC_SECURITY_DTLS_ROLE_CLIENT = 0,
    RTC_SECURITY_DTLS_ROLE_SERVER
} rtc_security_dtls_role_t;

typedef enum rtc_security_backend_event_type_t {
    RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM = 0,
    RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE,
    RTC_SECURITY_BACKEND_EVENT_ERROR
} rtc_security_backend_event_type_t;

typedef enum rtc_security_detail_code_t {
    RTC_SECURITY_DETAIL_HANDSHAKE_FAILED = 4001,
    RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH = 4002,
    RTC_SECURITY_DETAIL_KEY_EXPORT_FAILED = 4003,
    RTC_SECURITY_DETAIL_SRTP_INIT_FAILED = 4004,
    RTC_SECURITY_DETAIL_SRTP_PROTECT_FAILED = 4005,
    RTC_SECURITY_DETAIL_SRTP_UNPROTECT_FAILED = 4006,
    RTC_SECURITY_DETAIL_SRTP_REPLAY_FAILED = 4007
} rtc_security_detail_code_t;

typedef struct rtc_security_backend_event_t {
    rtc_security_backend_event_type_t type;
    const uint8_t *datagram;
    size_t datagram_len;
    rtc_status_t status;
    int detail_code;
} rtc_security_backend_event_t;

typedef void (*rtc_security_backend_event_cb)(
    void *user_data,
    const rtc_security_backend_event_t *event);

typedef struct rtc_security_backend_vtable_t {
    rtc_status_t (*create_session)(void *backend_user_data, void *storage, size_t storage_len, rtc_security_backend_event_cb event_cb, void *event_user_data, void **out_session);
    void (*destroy_session)(void *session);
    rtc_status_t (*get_local_fingerprint)(void *session, char *out, size_t *inout_len);
    rtc_status_t (*start_dtls)(void *session, rtc_security_dtls_role_t role);
    rtc_status_t (*handle_dtls_datagram)(void *session, const uint8_t *packet, size_t packet_len);
    rtc_status_t (*get_peer_fingerprint)(void *session, char *out, size_t *inout_len);
    rtc_status_t (*export_keying_material)(void *session, const char *label, size_t label_len, uint8_t *out, size_t out_len);
    rtc_status_t (*init_srtp_context)(void *session, const uint8_t *keying_material, size_t keying_material_len, rtc_security_dtls_role_t local_role);
    rtc_status_t (*srtp_protect_rtp)(void *session, uint8_t *packet, size_t *inout_len, size_t capacity);
    rtc_status_t (*srtp_unprotect_rtp)(void *session, uint8_t *packet, size_t *inout_len);
    rtc_status_t (*srtcp_protect)(void *session, uint8_t *packet, size_t *inout_len, size_t capacity);
    rtc_status_t (*srtcp_unprotect)(void *session, uint8_t *packet, size_t *inout_len);
} rtc_security_backend_vtable_t;

typedef struct rtc_security_backend_config_t {
    const rtc_security_backend_vtable_t *vtable;
    void *user_data;
    size_t session_storage_bytes;
} rtc_security_backend_config_t;

#ifdef __cplusplus
}
#endif

#endif
