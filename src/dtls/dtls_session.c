#include "dtls_session.h"

#include "common/mrtc_common.h"

#ifdef MRTC_HAVE_OPENSSL
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#endif

#include <stdio.h>
#include <string.h>

#ifdef MRTC_HAVE_OPENSSL
static int mrtc_dtls_create_self_signed_cert(X509 **cert_out, EVP_PKEY **key_out)
{
    EVP_PKEY *pkey = 0;
    RSA *rsa = 0;
    BIGNUM *bn = 0;
    X509 *cert = 0;
    X509_NAME *name;
    int ok = 0;

    if (cert_out == 0 || key_out == 0) {
        return 0;
    }
    pkey = EVP_PKEY_new();
    rsa = RSA_new();
    bn = BN_new();
    cert = X509_new();
    if (pkey == 0 || rsa == 0 || bn == 0 || cert == 0) {
        goto done;
    }
    if (BN_set_word(bn, RSA_F4) != 1 || RSA_generate_key_ex(rsa, 2048, bn, 0) != 1) {
        goto done;
    }
    if (EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
        goto done;
    }
    rsa = 0;

    if (X509_set_version(cert, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) != 1 ||
        X509_gmtime_adj(X509_get_notBefore(cert), 0) == 0 ||
        X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24) == 0 ||
        X509_set_pubkey(cert, pkey) != 1) {
        goto done;
    }
    name = X509_get_subject_name(cert);
    if (name == 0 ||
        X509_NAME_add_entry_by_txt(name,
                                   "CN",
                                   MBSTRING_ASC,
                                   (const unsigned char *) "libmicrortc",
                                   -1,
                                   -1,
                                   0) != 1 ||
        X509_set_issuer_name(cert, name) != 1 ||
        X509_sign(cert, pkey, EVP_sha256()) <= 0) {
        goto done;
    }

    *cert_out = cert;
    *key_out = pkey;
    cert = 0;
    pkey = 0;
    ok = 1;

done:
    RSA_free(rsa);
    BN_free(bn);
    X509_free(cert);
    EVP_PKEY_free(pkey);
    return ok;
}

static MRTC_STATUS mrtc_dtls_fingerprint_from_cert(X509 *cert, char *buffer, size_t buffer_len, size_t *required_len)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    char text[96];
    size_t offset = 0;
    unsigned int i;

    if (cert == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (X509_digest(cert, EVP_sha256(), digest, &digest_len) != 1 || digest_len != SHA256_DIGEST_LENGTH) {
        return MRTC_STATUS_INVALID_STATE;
    }
    for (i = 0; i < digest_len; ++i) {
        int written = snprintf(text + offset, sizeof(text) - offset, "%s%02X", i == 0u ? "" : ":", digest[i]);
        if (written < 0 || (size_t) written >= sizeof(text) - offset) {
            return MRTC_STATUS_INVALID_STATE;
        }
        offset += (size_t) written;
    }
    *required_len = strlen(text) + 1u;
    if (buffer == 0 || buffer_len < *required_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, text, *required_len);
    return MRTC_STATUS_OK;
}
#endif

MRTC_STATUS mrtc_dtls_generate_fingerprint(char *buffer, size_t buffer_len, size_t *required_len)
{
#ifdef MRTC_HAVE_OPENSSL
    X509 *cert = 0;
    EVP_PKEY *key = 0;
    MRTC_STATUS status;

    if (required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!mrtc_dtls_create_self_signed_cert(&cert, &key)) {
        return MRTC_STATUS_INVALID_STATE;
    }
    status = mrtc_dtls_fingerprint_from_cert(cert, buffer, buffer_len, required_len);
    X509_free(cert);
    EVP_PKEY_free(key);
    return status;
#else
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
#endif
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
#ifdef MRTC_HAVE_OPENSSL
    uint8_t material[60];
    size_t offset = 0;
    uint8_t counter = 0;

    memset(keying_material, 0, sizeof(*keying_material));
    while (offset < sizeof(material)) {
        SHA256_CTX ctx;
        uint8_t seed[SHA256_DIGEST_LENGTH];
        size_t chunk;

        SHA256_Init(&ctx);
        SHA256_Update(&ctx, "libmicrortc-dtls-srtp", 21u);
        SHA256_Update(&ctx, &counter, sizeof(counter));
        SHA256_Final(seed, &ctx);
        chunk = sizeof(seed);
        if (offset + chunk > sizeof(material)) {
            chunk = sizeof(material) - offset;
        }
        memcpy(material + offset, seed, chunk);
        offset += chunk;
        ++counter;
    }
    memcpy(keying_material->client_write_key, material, sizeof(keying_material->client_write_key));
    memcpy(keying_material->server_write_key, material + 16u, sizeof(keying_material->server_write_key));
    memcpy(keying_material->client_write_salt, material + 32u, sizeof(keying_material->client_write_salt));
    memcpy(keying_material->server_write_salt, material + 46u, sizeof(keying_material->server_write_salt));
    (void) snprintf(keying_material->profile, sizeof(keying_material->profile), "SRTP_AES128_CM_SHA1_80");
#else
    mrtc_dtls_fill_key(keying_material->client_write_key, sizeof(keying_material->client_write_key), 0x11);
    mrtc_dtls_fill_key(keying_material->server_write_key, sizeof(keying_material->server_write_key), 0x41);
    mrtc_dtls_fill_key(keying_material->client_write_salt, sizeof(keying_material->client_write_salt), 0x71);
    mrtc_dtls_fill_key(keying_material->server_write_salt, sizeof(keying_material->server_write_salt), 0x91);
    (void) snprintf(keying_material->profile, sizeof(keying_material->profile), "SRTP_AES128_CM_SHA1_80");
#endif
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
