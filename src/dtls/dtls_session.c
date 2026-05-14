#include "dtls_session.h"

#include "common/mrtc_common.h"

#ifdef MRTC_HAVE_OPENSSL
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif

#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef MRTC_HAVE_OPENSSL
static X509 *g_dtls_certificate = 0;
static EVP_PKEY *g_dtls_private_key = 0;
static char g_dtls_fingerprint[96];

static int mrtc_dtls_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    (void) preverify_ok;
    (void) ctx;
    return 1;
}

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

static int mrtc_dtls_ensure_global_certificate(void)
{
    size_t required_len = 0;

    if (g_dtls_certificate != 0 && g_dtls_private_key != 0 && g_dtls_fingerprint[0] != '\0') {
        return 1;
    }
    if (!mrtc_dtls_create_self_signed_cert(&g_dtls_certificate, &g_dtls_private_key)) {
        return 0;
    }
    if (mrtc_dtls_fingerprint_from_cert(g_dtls_certificate,
                                        g_dtls_fingerprint,
                                        sizeof(g_dtls_fingerprint),
                                        &required_len) != MRTC_STATUS_OK) {
        X509_free(g_dtls_certificate);
        EVP_PKEY_free(g_dtls_private_key);
        g_dtls_certificate = 0;
        g_dtls_private_key = 0;
        g_dtls_fingerprint[0] = '\0';
        return 0;
    }
    return 1;
}

static MRTC_STATUS mrtc_dtls_verify_connected_fingerprint(MRTC_DTLS_SESSION *session)
{
    X509 *peer;
    char fingerprint[96];
    size_t required_len = 0;

    if (session == 0 || session->ssl == 0 || session->expected_peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_STATE;
    }
    peer = SSL_get_peer_certificate((SSL *) session->ssl);
    if (peer == 0 ||
        mrtc_dtls_fingerprint_from_cert(peer, fingerprint, sizeof(fingerprint), &required_len) != MRTC_STATUS_OK ||
        strcasecmp(fingerprint, session->expected_peer_fingerprint) != 0) {
        X509_free(peer);
        session->state = MRTC_DTLS_STATE_FAILED;
        return MRTC_STATUS_INVALID_STATE;
    }
    X509_free(peer);
    (void) snprintf(session->verified_peer_fingerprint,
                    sizeof(session->verified_peer_fingerprint),
                    "%s",
                    fingerprint);
    session->remote_fingerprint_verified = 1;
    session->state = MRTC_DTLS_STATE_CONNECTED;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_dtls_complete_if_ready(MRTC_DTLS_SESSION *session)
{
    if (session == 0 || session->ssl == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (SSL_is_init_finished((SSL *) session->ssl)) {
        if (!session->remote_fingerprint_verified) {
            return mrtc_dtls_verify_connected_fingerprint(session);
        }
        session->state = MRTC_DTLS_STATE_CONNECTED;
    }
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_dtls_do_handshake(MRTC_DTLS_SESSION *session)
{
    int ret;
    int err;

    if (session == 0 || session->ssl == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (session->state == MRTC_DTLS_STATE_FAILED) {
        return MRTC_STATUS_INVALID_STATE;
    }
    ERR_clear_error();
    ret = SSL_do_handshake((SSL *) session->ssl);
    if (ret == 1) {
        return mrtc_dtls_complete_if_ready(session);
    }
    err = SSL_get_error((SSL *) session->ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        return mrtc_dtls_complete_if_ready(session);
    }
    session->state = MRTC_DTLS_STATE_FAILED;
    return MRTC_STATUS_INVALID_STATE;
}

static MRTC_STATUS mrtc_dtls_create_ssl(MRTC_DTLS_SESSION *session)
{
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *read_bio;
    BIO *write_bio;

    if (!mrtc_dtls_ensure_global_certificate()) {
        return MRTC_STATUS_INVALID_STATE;
    }
    ctx = SSL_CTX_new(DTLS_method());
    if (ctx == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ctx, DTLS1_2_VERSION);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, mrtc_dtls_verify_callback);
    if (SSL_CTX_set_tlsext_use_srtp(ctx, "SRTP_AES128_CM_SHA1_80") != 0 ||
        SSL_CTX_use_certificate(ctx, g_dtls_certificate) != 1 ||
        SSL_CTX_use_PrivateKey(ctx, g_dtls_private_key) != 1 ||
        SSL_CTX_check_private_key(ctx) != 1 ||
        SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5:!RC4") != 1) {
        SSL_CTX_free(ctx);
        return MRTC_STATUS_INVALID_STATE;
    }

    ssl = SSL_new(ctx);
    read_bio = BIO_new(BIO_s_mem());
    write_bio = BIO_new(BIO_s_mem());
    if (ssl == 0 || read_bio == 0 || write_bio == 0) {
        SSL_free(ssl);
        BIO_free(read_bio);
        BIO_free(write_bio);
        SSL_CTX_free(ctx);
        return MRTC_STATUS_INVALID_STATE;
    }
    BIO_set_mem_eof_return(read_bio, -1);
    BIO_set_mem_eof_return(write_bio, -1);
    SSL_set_bio(ssl, read_bio, write_bio);

    session->ssl_ctx = ctx;
    session->ssl = ssl;
    session->certificate = g_dtls_certificate;
    session->private_key = g_dtls_private_key;
    return MRTC_STATUS_OK;
}
#endif

MRTC_STATUS mrtc_dtls_generate_fingerprint(char *buffer, size_t buffer_len, size_t *required_len)
{
#ifdef MRTC_HAVE_OPENSSL
    if (required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!mrtc_dtls_ensure_global_certificate()) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *required_len = strlen(g_dtls_fingerprint) + 1u;
    if (buffer == 0 || buffer_len < *required_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, g_dtls_fingerprint, *required_len);
    return MRTC_STATUS_OK;
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
    memset(keying_material, 0, sizeof(*keying_material));
#ifndef MRTC_HAVE_OPENSSL
    mrtc_dtls_fill_key(keying_material->client_write_key, sizeof(keying_material->client_write_key), 0x11);
    mrtc_dtls_fill_key(keying_material->server_write_key, sizeof(keying_material->server_write_key), 0x41);
    mrtc_dtls_fill_key(keying_material->client_write_salt, sizeof(keying_material->client_write_salt), 0x71);
    mrtc_dtls_fill_key(keying_material->server_write_salt, sizeof(keying_material->server_write_salt), 0x91);
#else
    (void) mrtc_dtls_fill_key;
#endif
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
#ifdef MRTC_HAVE_OPENSSL
    if (mrtc_dtls_create_ssl(session) != MRTC_STATUS_OK) {
        memset(session, 0, sizeof(*session));
        return MRTC_STATUS_INVALID_STATE;
    }
#endif
    return MRTC_STATUS_OK;
}

void mrtc_dtls_session_deinit(MRTC_DTLS_SESSION *session)
{
    if (session != 0) {
#ifdef MRTC_HAVE_OPENSSL
        SSL_free((SSL *) session->ssl);
        SSL_CTX_free((SSL_CTX *) session->ssl_ctx);
#endif
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

    strcpy(left->verified_peer_fingerprint, right->local_fingerprint);
    strcpy(right->verified_peer_fingerprint, left->local_fingerprint);
    left->remote_fingerprint_verified = 1;
    right->remote_fingerprint_verified = 1;
    left->state = MRTC_DTLS_STATE_CONNECTED;
    right->state = MRTC_DTLS_STATE_CONNECTED;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_dtls_session_set_remote_fingerprint(MRTC_DTLS_SESSION *session, const char *peer_fingerprint)
{
    if (session == 0 || peer_fingerprint == 0 || peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (strlen(peer_fingerprint) >= sizeof(session->expected_peer_fingerprint)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    strcpy(session->expected_peer_fingerprint, peer_fingerprint);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_dtls_session_set_verified_peer_fingerprint(MRTC_DTLS_SESSION *session, const char *peer_fingerprint)
{
    return mrtc_dtls_session_set_remote_fingerprint(session, peer_fingerprint);
}

MRTC_STATUS mrtc_dtls_session_verify_remote_fingerprint(const MRTC_DTLS_SESSION *session, const char *expected_fingerprint)
{
    if (session == 0 || expected_fingerprint == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->remote_fingerprint_verified || session->verified_peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_STATE;
    }
    return strcasecmp(session->verified_peer_fingerprint, expected_fingerprint) == 0 ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
}

MRTC_STATUS mrtc_dtls_session_start(MRTC_DTLS_SESSION *session)
{
    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (session->expected_peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_OPENSSL
    if (session->ssl == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (session->state == MRTC_DTLS_STATE_NEW) {
        if (session->role == MRTC_DTLS_ROLE_SERVER) {
            SSL_set_accept_state((SSL *) session->ssl);
        } else {
            SSL_set_connect_state((SSL *) session->ssl);
        }
        session->state = MRTC_DTLS_STATE_CONNECTING;
    }
    return mrtc_dtls_do_handshake(session);
#else
    session->state = MRTC_DTLS_STATE_CONNECTING;
    return MRTC_STATUS_OK;
#endif
}

MRTC_STATUS mrtc_dtls_session_handle_inbound_packet(MRTC_DTLS_SESSION *session, const uint8_t *packet, size_t packet_len)
{
    if (session == 0 || packet == 0 || packet_len == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
#ifdef MRTC_HAVE_OPENSSL
    if (session->ssl == 0 || session->state == MRTC_DTLS_STATE_FAILED) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (BIO_write(SSL_get_rbio((SSL *) session->ssl), packet, (int) packet_len) <= 0) {
        session->state = MRTC_DTLS_STATE_FAILED;
        return MRTC_STATUS_INVALID_STATE;
    }
    if (session->state != MRTC_DTLS_STATE_CONNECTED) {
        MRTC_STATUS status = mrtc_dtls_do_handshake(session);
        if (status != MRTC_STATUS_OK && session->state == MRTC_DTLS_STATE_FAILED) {
            return status;
        }
    }
    if (session->state == MRTC_DTLS_STATE_CONNECTED) {
        uint8_t app_data[2048];
        for (;;) {
            int ret;
            int err;

            ERR_clear_error();
            ret = SSL_read((SSL *) session->ssl, app_data, (int) sizeof(app_data));
            if (ret > 0) {
                if (session->on_application_data != 0) {
                    session->on_application_data(session->application_data_user_data, app_data, (size_t) ret);
                }
                continue;
            }
            err = SSL_get_error((SSL *) session->ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                break;
            }
            if (ret == 0 && err == SSL_ERROR_ZERO_RETURN) {
                session->state = MRTC_DTLS_STATE_FAILED;
                return MRTC_STATUS_INVALID_STATE;
            }
            break;
        }
    }
    return MRTC_STATUS_OK;
#else
    (void) packet;
    (void) packet_len;
    return MRTC_STATUS_NOT_IMPLEMENTED;
#endif
}

MRTC_STATUS mrtc_dtls_session_drain_outbound_packet(MRTC_DTLS_SESSION *session,
                                                    uint8_t *packet,
                                                    size_t packet_capacity,
                                                    size_t *packet_len)
{
    if (packet_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    *packet_len = 0;
    if (session == 0 || packet == 0 || packet_capacity == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
#ifdef MRTC_HAVE_OPENSSL
    {
        long pending;
        int read_len;

        if (session->ssl == 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        pending = BIO_ctrl_pending(SSL_get_wbio((SSL *) session->ssl));
        if (pending <= 0) {
            return MRTC_STATUS_OK;
        }
        if ((size_t) pending > packet_capacity) {
            return MRTC_STATUS_INVALID_ARG;
        }
        read_len = BIO_read(SSL_get_wbio((SSL *) session->ssl), packet, (int) packet_capacity);
        if (read_len <= 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        *packet_len = (size_t) read_len;
    }
#else
    (void) session;
    (void) packet;
    (void) packet_capacity;
#endif
    return MRTC_STATUS_OK;
}

int mrtc_dtls_session_is_connected(const MRTC_DTLS_SESSION *session)
{
    return session != 0 && session->state == MRTC_DTLS_STATE_CONNECTED && session->remote_fingerprint_verified;
}

MRTC_STATUS mrtc_dtls_session_set_application_data_callback(MRTC_DTLS_SESSION *session,
                                                            MRTC_DTLS_APPLICATION_DATA_CALLBACK callback,
                                                            void *user_data)
{
    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    session->on_application_data = callback;
    session->application_data_user_data = user_data;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_dtls_session_send_application_data(MRTC_DTLS_SESSION *session,
                                                    const uint8_t *data,
                                                    size_t data_len)
{
    if (session == 0 || (data == 0 && data_len > 0u) || data_len > (size_t) 0x7fffffff) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!mrtc_dtls_session_is_connected(session)) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_OPENSSL
    {
        int ret;
        int err;

        ERR_clear_error();
        ret = SSL_write((SSL *) session->ssl, data, (int) data_len);
        if (ret == (int) data_len) {
            return MRTC_STATUS_OK;
        }
        err = SSL_get_error((SSL *) session->ssl, ret);
        return (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) ? MRTC_STATUS_OK : MRTC_STATUS_INVALID_STATE;
    }
#else
    (void) data;
    (void) data_len;
    return MRTC_STATUS_NOT_IMPLEMENTED;
#endif
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
#ifdef MRTC_HAVE_OPENSSL
    {
        uint8_t material[60];

        if (session->ssl == 0 ||
            SSL_export_keying_material((SSL *) session->ssl,
                                       material,
                                       sizeof(material),
                                       "EXTRACTOR-dtls_srtp",
                                       strlen("EXTRACTOR-dtls_srtp"),
                                       0,
                                       0,
                                       0) != 1) {
            return MRTC_STATUS_INVALID_STATE;
        }
        memset(keying_material, 0, sizeof(*keying_material));
        memcpy(keying_material->client_write_key, material, sizeof(keying_material->client_write_key));
        memcpy(keying_material->server_write_key, material + 16u, sizeof(keying_material->server_write_key));
        memcpy(keying_material->client_write_salt, material + 32u, sizeof(keying_material->client_write_salt));
        memcpy(keying_material->server_write_salt, material + 46u, sizeof(keying_material->server_write_salt));
        (void) snprintf(keying_material->profile, sizeof(keying_material->profile), "SRTP_AES128_CM_SHA1_80");
    }
#else
    *keying_material = session->keying_material;
#endif
    return MRTC_STATUS_OK;
}
