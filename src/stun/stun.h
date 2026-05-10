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

int rtc_stun_is_datagram(const uint8_t *data, size_t len);
rtc_status_t rtc_stun_write_binding_request(
    uint8_t *out, size_t out_capacity,
    const uint8_t transaction_id[RTC_STUN_TRANSACTION_ID_BYTES],
    size_t *out_len);
rtc_status_t rtc_stun_parse_header(const uint8_t *data, size_t len,
                                   rtc_stun_header_t *out_header);
rtc_status_t rtc_stun_parse_xor_mapped_address(
    const uint8_t *data, size_t len, rtc_stun_xor_mapped_address_t *out_addr);

#ifdef __cplusplus
}
#endif

#endif
