#include "security_backend_chrome.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY)

#include <limits.h>
#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <srtp2/srtp.h>

#define CHROME_SRTP_MASTER_KEY_BYTES 16u
#define CHROME_SRTP_MASTER_SALT_BYTES 14u
#define CHROME_SRTP_KEY_BYTES \
    (CHROME_SRTP_MASTER_KEY_BYTES + CHROME_SRTP_MASTER_SALT_BYTES)
#define CHROME_DTLS_SRTP_KEY_MATERIAL_BYTES \
    ((CHROME_SRTP_MASTER_KEY_BYTES * 2u) + \
     (CHROME_SRTP_MASTER_SALT_BYTES * 2u))
#define CHROME_DTLS_OUTGOING_CHUNK_BYTES 1500u
#define CHROME_SRTP_RTP_AUTH_TAG_BYTES 10u
#define CHROME_SRTP_RTCP_TRAILER_BYTES 14u

typedef struct chrome_security_session_t {
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *read_bio;
    BIO *write_bio;
    X509 *certificate;
    EVP_PKEY *private_key;
    srtp_t srtp_send;
    srtp_t srtp_recv;
    rtc_security_backend_event_cb event_cb;
    void *event_user_data;
    rtc_security_dtls_role_t role;
    int handshake_complete;
    int srtp_ready;
    int bios_attached;
} chrome_security_session_t;

static void chrome_destroy_session(void *opaque);

static rtc_status_t chrome_emit_error(chrome_security_session_t *session,
                                      rtc_status_t status,
                                      int detail_code)
{
    rtc_security_backend_event_t event;

    if (session != 0 && session->event_cb != 0) {
        event.type = RTC_SECURITY_BACKEND_EVENT_ERROR;
        event.datagram = 0;
        event.datagram_len = 0;
        event.status = status;
        event.detail_code = detail_code;
        session->event_cb(session->event_user_data, &event);
    }
    return status;
}

static rtc_status_t chrome_create_fail(chrome_security_session_t *session,
                                       rtc_status_t status, int detail_code)
{
    chrome_emit_error(session, status, detail_code);
    chrome_destroy_session(session);
    return status;
}

static rtc_status_t chrome_emit_outgoing(chrome_security_session_t *session)
{
    uint8_t out[CHROME_DTLS_OUTGOING_CHUNK_BYTES];

    if (session == 0 || session->write_bio == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    while (BIO_ctrl_pending(session->write_bio) > 0) {
        int read_len = BIO_read(session->write_bio, out, (int)sizeof(out));
        rtc_security_backend_event_t event;

        if (read_len <= 0) {
            if (BIO_should_retry(session->write_bio)) {
                return RTC_STATUS_OK;
            }
            return chrome_emit_error(
                session, RTC_STATUS_BACKEND_ERROR,
                RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
        }

        if (session->event_cb != 0) {
            event.type = RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM;
            event.datagram = out;
            event.datagram_len = (size_t)read_len;
            event.status = RTC_STATUS_OK;
            event.detail_code = 0;
            session->event_cb(session->event_user_data, &event);
        }
    }

    return RTC_STATUS_OK;
}

static void chrome_emit_handshake_complete(chrome_security_session_t *session)
{
    rtc_security_backend_event_t event;

    if (session == 0 || session->handshake_complete ||
        SSL_is_init_finished(session->ssl) == 0) {
        return;
    }

    session->handshake_complete = 1;
    if (session->event_cb != 0) {
        event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
        event.datagram = 0;
        event.datagram_len = 0;
        event.status = RTC_STATUS_OK;
        event.detail_code = 0;
        session->event_cb(session->event_user_data, &event);
    }
}

static rtc_status_t chrome_drive_handshake(chrome_security_session_t *session)
{
    int ret;
    int ssl_error;
    rtc_status_t status;

    if (session == 0 || session->ssl == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    ret = SSL_do_handshake(session->ssl);
    status = chrome_emit_outgoing(session);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    if (ret == 1) {
        chrome_emit_handshake_complete(session);
        return RTC_STATUS_OK;
    }

    ssl_error = SSL_get_error(session->ssl, ret);
    if (ssl_error == SSL_ERROR_WANT_READ ||
        ssl_error == SSL_ERROR_WANT_WRITE) {
        return RTC_STATUS_OK;
    }

    ERR_clear_error();
    return chrome_emit_error(session, RTC_STATUS_PROTOCOL_ERROR,
                             RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
}

static rtc_status_t chrome_format_fingerprint(X509 *cert, char *out,
                                              size_t *inout_len)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    char formatted[8 + (EVP_MAX_MD_SIZE * 3)];
    size_t offset = 0;
    size_t i;

    if (cert == 0 || inout_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (X509_digest(cert, EVP_sha256(), digest, &digest_len) != 1 ||
        digest_len == 0) {
        return RTC_STATUS_BACKEND_ERROR;
    }

    memcpy(formatted, "sha-256 ", 8u);
    offset = 8u;
    for (i = 0; i < (size_t)digest_len; ++i) {
        static const char hex[] = "0123456789ABCDEF";
        if (i != 0) {
            formatted[offset++] = ':';
        }
        formatted[offset++] = hex[(digest[i] >> 4) & 0x0f];
        formatted[offset++] = hex[digest[i] & 0x0f];
    }
    formatted[offset] = '\0';

    if (out == 0 || *inout_len <= offset) {
        *inout_len = offset + 1u;
        return RTC_STATUS_CAPACITY;
    }

    memcpy(out, formatted, offset + 1u);
    *inout_len = offset;
    return RTC_STATUS_OK;
}

static EVP_PKEY *chrome_generate_key(void)
{
    EVP_PKEY_CTX *ctx = 0;
    EVP_PKEY *key = 0;

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, 0);
    if (ctx == 0) {
        return 0;
    }
    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &key) <= 0) {
        EVP_PKEY_free(key);
        key = 0;
    }
    EVP_PKEY_CTX_free(ctx);
    return key;
}

static X509 *chrome_generate_certificate(EVP_PKEY *key)
{
    X509 *cert;
    X509_NAME *name;
    ASN1_INTEGER *serial;

    if (key == 0) {
        return 0;
    }

    cert = X509_new();
    if (cert == 0) {
        return 0;
    }

    serial = X509_get_serialNumber(cert);
    if (serial == 0 || ASN1_INTEGER_set(serial, 1) != 1 ||
        X509_gmtime_adj(X509_get_notBefore(cert), 0) == 0 ||
        X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24) == 0 ||
        X509_set_version(cert, 2) != 1 ||
        X509_set_pubkey(cert, key) != 1) {
        X509_free(cert);
        return 0;
    }

    name = X509_get_subject_name(cert);
    if (name == 0 ||
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   (const unsigned char *)"rtc-chrome-e2e",
                                   -1, -1, 0) != 1 ||
        X509_set_issuer_name(cert, name) != 1 ||
        X509_sign(cert, key, EVP_sha256()) == 0) {
        X509_free(cert);
        return 0;
    }

    return cert;
}

static rtc_status_t chrome_create_session(
    void *backend_user_data, void *storage, size_t storage_len,
    rtc_security_backend_event_cb event_cb, void *event_user_data,
    void **out_session)
{
    chrome_security_session_t *session;

    (void)backend_user_data;
    if (storage == 0 || out_session == 0 ||
        storage_len < sizeof(chrome_security_session_t)) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    memset(storage, 0, storage_len);
    session = (chrome_security_session_t *)storage;
    session->event_cb = event_cb;
    session->event_user_data = event_user_data;

    session->private_key = chrome_generate_key();
    session->certificate = chrome_generate_certificate(session->private_key);
    session->ctx = SSL_CTX_new(DTLS_method());
    if (session->private_key == 0 || session->certificate == 0 ||
        session->ctx == 0) {
        return chrome_create_fail(session, RTC_STATUS_BACKEND_ERROR,
                                  RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
    }

    SSL_CTX_set_verify(session->ctx, SSL_VERIFY_NONE, 0);
    SSL_CTX_set_read_ahead(session->ctx, 1);
    if (SSL_CTX_use_certificate(session->ctx, session->certificate) != 1 ||
        SSL_CTX_use_PrivateKey(session->ctx, session->private_key) != 1 ||
        SSL_CTX_check_private_key(session->ctx) != 1 ||
        SSL_CTX_set_tlsext_use_srtp(session->ctx,
                                    "SRTP_AES128_CM_SHA1_80") != 0) {
        return chrome_create_fail(session, RTC_STATUS_BACKEND_ERROR,
                                  RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
    }

    session->ssl = SSL_new(session->ctx);
    session->read_bio = BIO_new(BIO_s_mem());
    session->write_bio = BIO_new(BIO_s_mem());
    if (session->ssl == 0 || session->read_bio == 0 ||
        session->write_bio == 0) {
        return chrome_create_fail(session, RTC_STATUS_BACKEND_ERROR,
                                  RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
    }

    BIO_set_mem_eof_return(session->read_bio, -1);
    BIO_set_mem_eof_return(session->write_bio, -1);
    SSL_set_bio(session->ssl, session->read_bio, session->write_bio);
    session->bios_attached = 1;
    SSL_set_options(session->ssl, SSL_OP_NO_QUERY_MTU);
    SSL_set_mtu(session->ssl, 1200);

    *out_session = session;
    return RTC_STATUS_OK;
}

static void chrome_destroy_session(void *opaque)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;

    if (session == 0) {
        return;
    }
    if (session->srtp_send != 0) {
        srtp_dealloc(session->srtp_send);
        session->srtp_send = 0;
    }
    if (session->srtp_recv != 0) {
        srtp_dealloc(session->srtp_recv);
        session->srtp_recv = 0;
    }
    if (session->ssl != 0) {
        SSL_free(session->ssl);
        session->ssl = 0;
        if (session->bios_attached) {
            session->read_bio = 0;
            session->write_bio = 0;
        }
    }
    BIO_free(session->read_bio);
    BIO_free(session->write_bio);
    SSL_CTX_free(session->ctx);
    X509_free(session->certificate);
    EVP_PKEY_free(session->private_key);
    memset(session, 0, sizeof(*session));
}

static rtc_status_t chrome_get_local_fingerprint(void *opaque, char *out,
                                                 size_t *inout_len)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    return chrome_format_fingerprint(session != 0 ? session->certificate : 0,
                                     out, inout_len);
}

static rtc_status_t chrome_start_dtls(void *opaque,
                                      rtc_security_dtls_role_t role)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;

    if (session == 0 || session->ssl == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    session->role = role;
    if (role == RTC_SECURITY_DTLS_ROLE_CLIENT) {
        SSL_set_connect_state(session->ssl);
    } else {
        SSL_set_accept_state(session->ssl);
    }

    return chrome_drive_handshake(session);
}

static rtc_status_t chrome_handle_dtls_datagram(void *opaque,
                                                const uint8_t *packet,
                                                size_t packet_len)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    int written;

    if (session == 0 || session->read_bio == 0 || packet == 0 ||
        packet_len == 0 || packet_len > (size_t)INT_MAX) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    written = BIO_write(session->read_bio, packet, (int)packet_len);
    if (written != (int)packet_len) {
        return chrome_emit_error(session, RTC_STATUS_BACKEND_ERROR,
                                 RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
    }

    return chrome_drive_handshake(session);
}

static rtc_status_t chrome_get_peer_fingerprint(void *opaque, char *out,
                                                size_t *inout_len)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    X509 *peer = 0;
    rtc_status_t status;

    if (session == 0 || session->ssl == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    peer = SSL_get1_peer_certificate(session->ssl);
#else
    peer = SSL_get_peer_certificate(session->ssl);
#endif
    status = chrome_format_fingerprint(peer, out, inout_len);
    X509_free(peer);
    return status;
}

static rtc_status_t chrome_export_keying_material(void *opaque,
                                                  const char *label,
                                                  size_t label_len,
                                                  uint8_t *out,
                                                  size_t out_len)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;

    if (session == 0 || session->ssl == 0 || label == 0 || out == 0 ||
        label_len == 0 || label_len > (size_t)INT_MAX) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (label_len != strlen("EXTRACTOR-dtls_srtp") ||
        memcmp(label, "EXTRACTOR-dtls_srtp", label_len) != 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (SSL_export_keying_material(session->ssl, out, out_len, label,
                                   label_len, 0, 0, 0) != 1) {
        return chrome_emit_error(session, RTC_STATUS_BACKEND_ERROR,
                                 RTC_SECURITY_DETAIL_KEY_EXPORT_FAILED);
    }

    return RTC_STATUS_OK;
}

static void chrome_copy_srtp_key(uint8_t out[CHROME_SRTP_KEY_BYTES],
                                 const uint8_t *master_key,
                                 const uint8_t *master_salt)
{
    memcpy(out, master_key, CHROME_SRTP_MASTER_KEY_BYTES);
    memcpy(out + CHROME_SRTP_MASTER_KEY_BYTES, master_salt,
           CHROME_SRTP_MASTER_SALT_BYTES);
}

static rtc_status_t chrome_create_srtp_session(srtp_t *out_session,
                                               uint8_t *key,
                                               srtp_ssrc_type_t ssrc_type)
{
    srtp_policy_t policy;
    srtp_err_status_t err;

    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
    policy.ssrc.type = ssrc_type;
    policy.ssrc.value = 0;
    policy.key = key;
    policy.window_size = 1024;
    policy.allow_repeat_tx = 1;
    policy.next = 0;

    err = srtp_create(out_session, &policy);
    return err == srtp_err_status_ok ? RTC_STATUS_OK
                                     : RTC_STATUS_BACKEND_ERROR;
}

static rtc_status_t chrome_init_srtp_context(
    void *opaque, const uint8_t *keying_material, size_t keying_material_len,
    rtc_security_dtls_role_t local_role)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    const uint8_t *client_key;
    const uint8_t *server_key;
    const uint8_t *client_salt;
    const uint8_t *server_salt;
    uint8_t send_key[CHROME_SRTP_KEY_BYTES];
    uint8_t recv_key[CHROME_SRTP_KEY_BYTES];
    rtc_status_t status;
    static int srtp_initialized = 0;

    if (session == 0 || keying_material == 0 ||
        keying_material_len != CHROME_DTLS_SRTP_KEY_MATERIAL_BYTES) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (!srtp_initialized) {
        if (srtp_init() != srtp_err_status_ok) {
            return RTC_STATUS_BACKEND_ERROR;
        }
        srtp_initialized = 1;
    }

    client_key = keying_material;
    server_key = client_key + CHROME_SRTP_MASTER_KEY_BYTES;
    client_salt = server_key + CHROME_SRTP_MASTER_KEY_BYTES;
    server_salt = client_salt + CHROME_SRTP_MASTER_SALT_BYTES;

    if (local_role == RTC_SECURITY_DTLS_ROLE_CLIENT) {
        chrome_copy_srtp_key(send_key, client_key, client_salt);
        chrome_copy_srtp_key(recv_key, server_key, server_salt);
    } else {
        chrome_copy_srtp_key(send_key, server_key, server_salt);
        chrome_copy_srtp_key(recv_key, client_key, client_salt);
    }

    status = chrome_create_srtp_session(&session->srtp_send, send_key,
                                        ssrc_any_outbound);
    memset(send_key, 0, sizeof(send_key));
    if (status != RTC_STATUS_OK) {
        memset(recv_key, 0, sizeof(recv_key));
        return status;
    }

    status = chrome_create_srtp_session(&session->srtp_recv, recv_key,
                                        ssrc_any_inbound);
    memset(recv_key, 0, sizeof(recv_key));
    if (status != RTC_STATUS_OK) {
        return status;
    }

    session->srtp_ready = 1;
    return RTC_STATUS_OK;
}

static rtc_status_t chrome_srtp_status(srtp_err_status_t err, int unprotect)
{
    if (err == srtp_err_status_ok) {
        return RTC_STATUS_OK;
    }
    if (unprotect && (err == srtp_err_status_replay_fail ||
                      err == srtp_err_status_replay_old)) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    return RTC_STATUS_BACKEND_ERROR;
}

static rtc_status_t chrome_srtp_protect_rtp(void *opaque, uint8_t *packet,
                                            size_t *inout_len,
                                            size_t capacity)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    int len;
    srtp_err_status_t err;

    if (session == 0 || session->srtp_send == 0 || packet == 0 ||
        inout_len == 0 || *inout_len > (size_t)INT_MAX ||
        capacity < *inout_len + CHROME_SRTP_RTP_AUTH_TAG_BYTES) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    len = (int)*inout_len;
    err = srtp_protect(session->srtp_send, packet, &len);
    if (err != srtp_err_status_ok) {
        return chrome_srtp_status(err, 0);
    }
    *inout_len = (size_t)len;
    return RTC_STATUS_OK;
}

static rtc_status_t chrome_srtp_unprotect_rtp(void *opaque, uint8_t *packet,
                                              size_t *inout_len)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    int len;
    srtp_err_status_t err;

    if (session == 0 || session->srtp_recv == 0 || packet == 0 ||
        inout_len == 0 || *inout_len > (size_t)INT_MAX) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    len = (int)*inout_len;
    err = srtp_unprotect(session->srtp_recv, packet, &len);
    if (err != srtp_err_status_ok) {
        return chrome_srtp_status(err, 1);
    }
    *inout_len = (size_t)len;
    return RTC_STATUS_OK;
}

static rtc_status_t chrome_srtcp_protect(void *opaque, uint8_t *packet,
                                         size_t *inout_len, size_t capacity)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    int len;
    srtp_err_status_t err;

    if (session == 0 || session->srtp_send == 0 || packet == 0 ||
        inout_len == 0 || *inout_len > (size_t)INT_MAX ||
        capacity < *inout_len + CHROME_SRTP_RTCP_TRAILER_BYTES) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    len = (int)*inout_len;
    err = srtp_protect_rtcp(session->srtp_send, packet, &len);
    if (err != srtp_err_status_ok) {
        return chrome_srtp_status(err, 0);
    }
    *inout_len = (size_t)len;
    return RTC_STATUS_OK;
}

static rtc_status_t chrome_srtcp_unprotect(void *opaque, uint8_t *packet,
                                           size_t *inout_len)
{
    chrome_security_session_t *session = (chrome_security_session_t *)opaque;
    int len;
    srtp_err_status_t err;

    if (session == 0 || session->srtp_recv == 0 || packet == 0 ||
        inout_len == 0 || *inout_len > (size_t)INT_MAX) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    len = (int)*inout_len;
    err = srtp_unprotect_rtcp(session->srtp_recv, packet, &len);
    if (err != srtp_err_status_ok) {
        return chrome_srtp_status(err, 1);
    }
    *inout_len = (size_t)len;
    return RTC_STATUS_OK;
}

static const rtc_security_backend_vtable_t chrome_security_vtable = {
    chrome_create_session,
    chrome_destroy_session,
    chrome_get_local_fingerprint,
    chrome_start_dtls,
    chrome_handle_dtls_datagram,
    chrome_get_peer_fingerprint,
    chrome_export_keying_material,
    chrome_init_srtp_context,
    chrome_srtp_protect_rtp,
    chrome_srtp_unprotect_rtp,
    chrome_srtcp_protect,
    chrome_srtcp_unprotect,
};

static rtc_security_backend_config_t chrome_security_config;

#endif

rtc_status_t rtc_chrome_e2e_configure_security_backend(
    rtc_peer_connection_config_t *config)
{
    if (config == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

#if defined(RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY)
    chrome_security_config.vtable = &chrome_security_vtable;
    chrome_security_config.user_data = 0;
    chrome_security_config.session_storage_bytes = sizeof(chrome_security_session_t);
    config->security_backend = &chrome_security_config;
    return RTC_STATUS_OK;
#else
    config->security_backend = 0;
    return RTC_STATUS_UNSUPPORTED;
#endif
}
