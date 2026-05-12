#include "dtls_session.h"

#include "common/mrtc_common.h"

#include <stdio.h>
#include <string.h>

MRTC_STATUS mrtc_dtls_generate_fingerprint(char *buffer, size_t buffer_len, size_t *required_len)
{
    uint8_t bytes[32];
    char text[96];
    size_t offset = 0;
    size_t i;

    if (required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_random_bytes(bytes, sizeof(bytes)) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }

    for (i = 0; i < sizeof(bytes); ++i) {
        int written = snprintf(text + offset, sizeof(text) - offset, "%s%02X", i == 0 ? "" : ":", bytes[i]);
        if (written < 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        offset += (size_t) written;
    }

    *required_len = strlen(text) + 1;
    if (buffer == 0 || buffer_len < *required_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, text, *required_len);
    return MRTC_STATUS_OK;
}

MRTC_DTLS_ROLE mrtc_dtls_role_from_remote_setup(const char *remote_setup)
{
    if (remote_setup != 0 && strcmp(remote_setup, "active") == 0) {
        return MRTC_DTLS_ROLE_SERVER;
    }
    return MRTC_DTLS_ROLE_CLIENT;
}

static void mrtc_dtls_fill_key(uint8_t *buffer, size_t len, uint8_t seed)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        buffer[i] = (uint8_t) (seed + (uint8_t) (i * 17u));
    }
}

static void mrtc_dtls_populate_keying_material(MRTC_DTLS_KEYING_MATERIAL *keying_material)
{
    mrtc_dtls_fill_key(keying_material->client_write_key, sizeof(keying_material->client_write_key), 0x11);
    mrtc_dtls_fill_key(keying_material->server_write_key, sizeof(keying_material->server_write_key), 0x41);
    mrtc_dtls_fill_key(keying_material->client_write_salt, sizeof(keying_material->client_write_salt), 0x71);
    mrtc_dtls_fill_key(keying_material->server_write_salt, sizeof(keying_material->server_write_salt), 0x91);
    (void) snprintf(keying_material->profile, sizeof(keying_material->profile), "SRTP_AES128_CM_SHA1_80");
}

MRTC_STATUS mrtc_dtls_session_init(MRTC_DTLS_SESSION *session, MRTC_DTLS_ROLE role, const char *local_fingerprint)
{
    if (session == 0 || local_fingerprint == 0 || local_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(session, 0, sizeof(*session));
    session->role = role;
    session->state = MRTC_DTLS_STATE_NEW;
    if (strlen(local_fingerprint) >= sizeof(session->local_fingerprint)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    strcpy(session->local_fingerprint, local_fingerprint);
    mrtc_dtls_populate_keying_material(&session->keying_material);
    return MRTC_STATUS_OK;
}

void mrtc_dtls_session_deinit(MRTC_DTLS_SESSION *session)
{
    if (session != 0) {
        memset(session, 0, sizeof(*session));
    }
}

MRTC_STATUS mrtc_dtls_session_pair_connect(MRTC_DTLS_SESSION *left, MRTC_DTLS_SESSION *right)
{
    if (left == 0 || right == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (left->role == right->role) {
        left->state = MRTC_DTLS_STATE_FAILED;
        right->state = MRTC_DTLS_STATE_FAILED;
        return MRTC_STATUS_INVALID_STATE;
    }

    strcpy(left->peer_fingerprint, right->local_fingerprint);
    strcpy(right->peer_fingerprint, left->local_fingerprint);
    left->remote_fingerprint_verified = 1;
    right->remote_fingerprint_verified = 1;
    left->state = MRTC_DTLS_STATE_CONNECTED;
    right->state = MRTC_DTLS_STATE_CONNECTED;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_dtls_session_set_verified_peer_fingerprint(MRTC_DTLS_SESSION *session, const char *peer_fingerprint)
{
    if (session == 0 || peer_fingerprint == 0 || peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (strlen(peer_fingerprint) >= sizeof(session->peer_fingerprint)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    strcpy(session->peer_fingerprint, peer_fingerprint);
    session->remote_fingerprint_verified = 1;
    session->state = MRTC_DTLS_STATE_CONNECTED;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_dtls_session_verify_remote_fingerprint(const MRTC_DTLS_SESSION *session, const char *expected_fingerprint)
{
    if (session == 0 || expected_fingerprint == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->remote_fingerprint_verified || session->peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_STATE;
    }
    return strcmp(session->peer_fingerprint, expected_fingerprint) == 0 ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
}

MRTC_STATUS mrtc_dtls_session_export_srtp_keying_material(const MRTC_DTLS_SESSION *session,
                                                          MRTC_DTLS_KEYING_MATERIAL *keying_material)
{
    if (session == 0 || keying_material == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (session->state != MRTC_DTLS_STATE_CONNECTED || !session->remote_fingerprint_verified) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *keying_material = session->keying_material;
    return MRTC_STATUS_OK;
}
