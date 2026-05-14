#ifndef MRTC_STUN_MESSAGE_H
#define MRTC_STUN_MESSAGE_H

#include <micrortc/micrortc.h>

#include <stddef.h>
#include <stdint.h>

#define MRTC_STUN_MAGIC_COOKIE 0x2112A442u
#define MRTC_STUN_TRANSACTION_ID_LEN 12u
#define MRTC_STUN_LONG_TERM_KEY_LEN 16u

#define MRTC_STUN_TYPE_BINDING_REQUEST 0x0001u
#define MRTC_STUN_TYPE_BINDING_SUCCESS_RESPONSE 0x0101u
#define MRTC_STUN_TYPE_ALLOCATE_REQUEST 0x0003u
#define MRTC_STUN_TYPE_ALLOCATE_SUCCESS_RESPONSE 0x0103u
#define MRTC_STUN_TYPE_ALLOCATE_ERROR_RESPONSE 0x0113u
#define MRTC_STUN_TYPE_CREATE_PERMISSION_REQUEST 0x0008u
#define MRTC_STUN_TYPE_CREATE_PERMISSION_SUCCESS_RESPONSE 0x0108u
#define MRTC_STUN_TYPE_CREATE_PERMISSION_ERROR_RESPONSE 0x0118u
#define MRTC_STUN_TYPE_SEND_INDICATION 0x0016u
#define MRTC_STUN_TYPE_DATA_INDICATION 0x0017u

typedef struct MRTC_STUN_XOR_MAPPED_ADDRESS {
    char ip[64];
    unsigned short port;
} MRTC_STUN_XOR_MAPPED_ADDRESS;

typedef MRTC_STUN_XOR_MAPPED_ADDRESS MRTC_STUN_ADDRESS;

typedef struct MRTC_STUN_BINDING_REQUEST {
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN];
    char username[128];
    int has_message_integrity;
    int message_integrity_valid;
    int has_fingerprint;
    int fingerprint_valid;
} MRTC_STUN_BINDING_REQUEST;

typedef struct MRTC_STUN_MESSAGE {
    uint16_t message_type;
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN];
    int has_xor_mapped_address;
    MRTC_STUN_ADDRESS xor_mapped_address;
    int has_xor_relayed_address;
    MRTC_STUN_ADDRESS xor_relayed_address;
    int has_xor_peer_address;
    MRTC_STUN_ADDRESS xor_peer_address;
    int has_realm;
    char realm[128];
    int has_nonce;
    char nonce[256];
    int has_error_code;
    unsigned int error_code;
    char error_reason[128];
    int has_data;
    const uint8_t *data;
    size_t data_len;
    int has_message_integrity;
    int message_integrity_valid;
    int has_fingerprint;
    int fingerprint_valid;
} MRTC_STUN_MESSAGE;

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
MRTC_STATUS mrtc_stun_make_long_term_key(const char *username,
                                         const char *realm,
                                         const char *password,
                                         uint8_t key[MRTC_STUN_LONG_TERM_KEY_LEN]);
MRTC_STATUS mrtc_stun_parse_message(const uint8_t *buffer,
                                    size_t buffer_len,
                                    const uint8_t *message_integrity_key,
                                    size_t message_integrity_key_len,
                                    MRTC_STUN_MESSAGE *message);
MRTC_STATUS mrtc_stun_parse_response(const uint8_t *buffer,
                                     size_t buffer_len,
                                     uint16_t expected_message_type,
                                     const uint8_t expected_transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                     const uint8_t *message_integrity_key,
                                     size_t message_integrity_key_len,
                                     MRTC_STUN_MESSAGE *message);
MRTC_STATUS mrtc_stun_parse_turn_allocate_response(const uint8_t *buffer,
                                                   size_t buffer_len,
                                                   const uint8_t expected_transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                   const uint8_t *message_integrity_key,
                                                   size_t message_integrity_key_len,
                                                   MRTC_STUN_MESSAGE *message);
MRTC_STATUS mrtc_stun_parse_turn_data_indication(const uint8_t *buffer,
                                                 size_t buffer_len,
                                                 MRTC_STUN_MESSAGE *message);
MRTC_STATUS mrtc_stun_write_turn_allocate_request(uint8_t *buffer,
                                                  size_t buffer_len,
                                                  size_t *written_len,
                                                  uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN]);
MRTC_STATUS mrtc_stun_write_turn_allocate_request_with_credentials(
    uint8_t *buffer,
    size_t buffer_len,
    size_t *written_len,
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
    const char *username,
    const char *realm,
    const char *nonce,
    const uint8_t key[MRTC_STUN_LONG_TERM_KEY_LEN]);
MRTC_STATUS mrtc_stun_write_turn_create_permission_request(
    uint8_t *buffer,
    size_t buffer_len,
    size_t *written_len,
    uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
    const char *peer_ip,
    unsigned short peer_port,
    const char *username,
    const char *realm,
    const char *nonce,
    const uint8_t key[MRTC_STUN_LONG_TERM_KEY_LEN]);
MRTC_STATUS mrtc_stun_write_turn_send_indication(uint8_t *buffer,
                                                 size_t buffer_len,
                                                 size_t *written_len,
                                                 uint8_t transaction_id[MRTC_STUN_TRANSACTION_ID_LEN],
                                                 const char *peer_ip,
                                                 unsigned short peer_port,
                                                 const uint8_t *data,
                                                 size_t data_len);

#endif /* MRTC_STUN_MESSAGE_H */
