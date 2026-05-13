#ifndef MRTC_STUN_MESSAGE_H
#define MRTC_STUN_MESSAGE_H

#include <micrortc/micrortc.h>

#include <stddef.h>
#include <stdint.h>

#define MRTC_STUN_MAGIC_COOKIE 0x2112A442u
#define MRTC_STUN_TRANSACTION_ID_LEN 12u

typedef struct MRTC_STUN_XOR_MAPPED_ADDRESS {
    char ip[64];
    unsigned short port;
} MRTC_STUN_XOR_MAPPED_ADDRESS;

typedef struct MRTC_STUN_BINDING_REQUEST {
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN];
    char username[128];
    int has_message_integrity;
    int message_integrity_valid;
    int has_fingerprint;
    int fingerprint_valid;
} MRTC_STUN_BINDING_REQUEST;

MRTC_STATUS mrtc_stun_write_binding_request(uint8_t *buffer,
                                            size_t buffer_len,
                                            size_t *written_len,
                                            uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN]);
MRTC_STATUS mrtc_stun_write_binding_request_with_credentials(uint8_t *buffer,
                                                             size_t buffer_len,
                                                             size_t *written_len,
                                                             uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                             const char *username,
                                                             const char *password);
MRTC_STATUS mrtc_stun_validate_header(const uint8_t *buffer, size_t buffer_len);
MRTC_STATUS mrtc_stun_parse_binding_request(const uint8_t *buffer,
                                            size_t buffer_len,
                                            const char *password,
                                            MRTC_STUN_BINDING_REQUEST *request);
MRTC_STATUS mrtc_stun_write_binding_success_response(uint8_t *buffer,
                                                     size_t buffer_len,
                                                     size_t *written_len,
                                                     const uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                     const char *mapped_ip,
                                                     unsigned short mapped_port,
                                                     const char *password);
MRTC_STATUS mrtc_stun_parse_xor_mapped_address(const uint8_t *buffer,
                                               size_t buffer_len,
                                               MRTC_STUN_XOR_MAPPED_ADDRESS *address);
MRTC_STATUS mrtc_stun_validate_message_integrity_input(const char *username, const char *password);

#endif /* MRTC_STUN_MESSAGE_H */
