#ifndef RTC_STUN_H
#define RTC_STUN_H

#include <stddef.h>
#include <stdint.h>

#define RTC_STUN_MAGIC_COOKIE 0x2112A442u

int rtc_stun_is_message(const uint8_t *buf, size_t len);
int rtc_stun_is_binding_request(const uint8_t *buf, size_t len, uint8_t out_txn[12]);
int rtc_stun_has_use_candidate(const uint8_t *buf, size_t len);
int rtc_stun_has_message_integrity(const uint8_t *buf, size_t len);
int rtc_stun_parse_username(const uint8_t *buf, size_t len, char *out, size_t out_len);
/* Returns: 1 valid, 0 invalid/missing, -1 malformed, -2 unsupported backend. */
int rtc_stun_verify_message_integrity(const uint8_t *buf, size_t len, const char *key);
int rtc_stun_build_binding_request(const uint8_t txn[12], uint8_t *out, size_t cap, size_t *written);
int rtc_stun_build_binding_success_response(const uint8_t req_txn[12],
                                            const char *mapped_ip,
                                            uint16_t mapped_port,
                                            const char *integrity_key,
                                            uint8_t *out,
                                            size_t cap,
                                            size_t *written);
int rtc_stun_parse_binding_response(const uint8_t *buf,
                                    size_t len,
                                    const uint8_t txn[12],
                                    char *out_ip,
                                    size_t ip_len,
                                    uint16_t *out_port);

#endif
