#ifndef MRTC_DTLS_SESSION_H
#define MRTC_DTLS_SESSION_H

#include <micrortc/micrortc.h>

#include <stddef.h>
#include <stdint.h>

typedef enum MRTC_DTLS_ROLE {
    MRTC_DTLS_ROLE_CLIENT = 1,
    MRTC_DTLS_ROLE_SERVER = 2
} MRTC_DTLS_ROLE;

typedef enum MRTC_DTLS_STATE {
    MRTC_DTLS_STATE_NEW = 0,
    MRTC_DTLS_STATE_CONNECTING = 1,
    MRTC_DTLS_STATE_CONNECTED = 2,
    MRTC_DTLS_STATE_FAILED = 3
} MRTC_DTLS_STATE;

typedef struct MRTC_DTLS_KEYING_MATERIAL {
    uint8_t client_write_key[16];
    uint8_t server_write_key[16];
    uint8_t client_write_salt[14];
    uint8_t server_write_salt[14];
    char profile[32];
} MRTC_DTLS_KEYING_MATERIAL;

typedef struct MRTC_DTLS_SESSION {
    MRTC_DTLS_ROLE role;
    MRTC_DTLS_STATE state;
    char local_fingerprint[96];
    char peer_fingerprint[96];
    int remote_fingerprint_verified;
    MRTC_DTLS_KEYING_MATERIAL keying_material;
} MRTC_DTLS_SESSION;

MRTC_STATUS mrtc_dtls_generate_fingerprint(char *buffer, size_t buffer_len, size_t *required_len);
MRTC_DTLS_ROLE mrtc_dtls_role_from_remote_setup(const char *remote_setup);
MRTC_STATUS mrtc_dtls_session_init(MRTC_DTLS_SESSION *session, MRTC_DTLS_ROLE role, const char *local_fingerprint);
void mrtc_dtls_session_deinit(MRTC_DTLS_SESSION *session);
MRTC_STATUS mrtc_dtls_session_pair_connect(MRTC_DTLS_SESSION *left, MRTC_DTLS_SESSION *right);
MRTC_STATUS mrtc_dtls_session_set_verified_peer_fingerprint(MRTC_DTLS_SESSION *session, const char *peer_fingerprint);
MRTC_STATUS mrtc_dtls_session_verify_remote_fingerprint(const MRTC_DTLS_SESSION *session, const char *expected_fingerprint);
MRTC_STATUS mrtc_dtls_session_export_srtp_keying_material(const MRTC_DTLS_SESSION *session,
                                                          MRTC_DTLS_KEYING_MATERIAL *keying_material);

#endif /* MRTC_DTLS_SESSION_H */
