#ifndef RTC_STUN_STUN_H
#define RTC_STUN_STUN_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_STUN_MAGIC_COOKIE 0x2112A442u
#define RTC_STUN_TRANSACTION_ID_BYTES 12u

#define RTC_STUN_BINDING_REQUEST 0x0001u
#define RTC_STUN_BINDING_SUCCESS_RESPONSE 0x0101u
#define RTC_STUN_BINDING_ERROR_RESPONSE 0x0111u
#define RTC_STUN_ERROR_ROLE_CONFLICT 487u

typedef struct rtc_stun_header_t {
    uint16_t type;
    uint16_t length;
    uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES];
} rtc_stun_header_t;

typedef struct rtc_stun_xor_mapped_address_t {
    uint8_t family;
    char ip[64];
    uint16_t port;
} rtc_stun_xor_mapped_address_t;

typedef struct rtc_stun_binding_request_attrs_t {
    int use_candidate;
    int has_priority;
    int has_ice_controlling;
    int has_ice_controlled;
    int has_message_integrity;
    int has_fingerprint;
    int message_integrity_valid;
    int fingerprint_valid;
    char username[128];
    size_t username_len;
    uint32_t priority;
    uint64_t tie_breaker;
} rtc_stun_binding_request_attrs_t;

int rtc_stun_is_datagram(const uint8_t *data, size_t len);
rtc_status_t rtc_stun_write_binding_request(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    size_t *out_len);
rtc_status_t rtc_stun_write_ice_binding_request(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    int use_candidate, int controlling, uint64_t tie_breaker,
    size_t *out_len);
rtc_status_t rtc_stun_write_ice_binding_request_authenticated(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    const char *local_ufrag, size_t local_ufrag_len,
    const char *remote_ufrag, size_t remote_ufrag_len,
    const char *remote_pwd, size_t remote_pwd_len, uint32_t priority,
    int use_candidate, int controlling, uint64_t tie_breaker,
    size_t *out_len);
rtc_status_t rtc_stun_write_binding_success_response_authenticated(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    const char *mapped_ip, uint16_t mapped_port,
    const char *local_pwd, size_t local_pwd_len, size_t *out_len);
rtc_status_t rtc_stun_parse_header(const uint8_t *data, size_t len,
                                   rtc_stun_header_t *out_header);
rtc_status_t rtc_stun_parse_binding_request_attrs(
    const uint8_t *data, size_t len, rtc_stun_binding_request_attrs_t *out_attrs);
rtc_status_t rtc_stun_parse_binding_request_attrs_auth(
    const uint8_t *data, size_t len, const char *local_ufrag,
    size_t local_ufrag_len, const char *remote_ufrag, size_t remote_ufrag_len,
    const char *local_pwd, size_t local_pwd_len,
    rtc_stun_binding_request_attrs_t *out_attrs);
rtc_status_t rtc_stun_parse_error_code(const uint8_t *data, size_t len,
                                       uint16_t *out_code);
rtc_status_t rtc_stun_parse_xor_mapped_address(
    const uint8_t *data, size_t len, rtc_stun_xor_mapped_address_t *out_addr);
rtc_status_t rtc_stun_validate_message_integrity(
    const uint8_t *data, size_t len, const char *pwd, size_t pwd_len);
rtc_status_t rtc_stun_validate_fingerprint(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
