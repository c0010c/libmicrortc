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
#ifdef MRTC_HAVE_MBEDTLS
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>
#include <mbedtls/x509_crt.h>
#endif

#include <stdio.h>
#include <stdlib.h>
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

#ifdef MRTC_HAVE_MBEDTLS
#define MRTC_MBEDTLS_IO_INITIAL_CAPACITY 4096u
#define MRTC_MBEDTLS_IO_MAX_CAPACITY 262144u

typedef struct MRTC_MBEDTLS_DTLS {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_timing_delay_context timer;
    unsigned char *inbound;
    size_t inbound_capacity;
    size_t inbound_len;
    size_t inbound_offset;
    unsigned char *outbound;
    size_t outbound_capacity;
    size_t outbound_len;
    size_t outbound_offset;
    MRTC_DTLS_KEYING_MATERIAL keying_material;
    int has_keying_material;
} MRTC_MBEDTLS_DTLS;

static mbedtls_x509_crt g_mbedtls_certificate;
static mbedtls_pk_context g_mbedtls_private_key;
static char g_mbedtls_fingerprint[96];
static int g_mbedtls_certificate_ready = 0;

static int mrtc_mbedtls_ensure_capacity(unsigned char **buffer, size_t *capacity, size_t needed)
{
    unsigned char *grown;
    size_t next_capacity = *capacity == 0u ? MRTC_MBEDTLS_IO_INITIAL_CAPACITY : *capacity;
    if (needed <= *capacity) {
        return 1;
    }
    while (next_capacity < needed && next_capacity < MRTC_MBEDTLS_IO_MAX_CAPACITY) {
        next_capacity *= 2u;
    }
    if (next_capacity < needed) {
        return 0;
    }
    grown = (unsigned char *) realloc(*buffer, next_capacity);
    if (grown == 0) {
        return 0;
    }
    *buffer = grown;
    *capacity = next_capacity;
    return 1;
}

static int mrtc_mbedtls_send(void *ctx, const unsigned char *buf, size_t len)
{
    MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) ctx;
    size_t used = dtls->outbound_len;
    if (dtls == 0 || (buf == 0 && len > 0u)) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    if (!mrtc_mbedtls_ensure_capacity(&dtls->outbound, &dtls->outbound_capacity, used + len)) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    memcpy(dtls->outbound + used, buf, len);
    dtls->outbound_len = used + len;
    return (int) len;
}

static int mrtc_mbedtls_recv(void *ctx, unsigned char *buf, size_t len)
{
    MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) ctx;
    size_t available;
    if (dtls == 0 || buf == 0 || len == 0u) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    if (dtls->inbound_offset >= dtls->inbound_len) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    available = dtls->inbound_len - dtls->inbound_offset;
    if (available > len) {
        available = len;
    }
    memcpy(buf, dtls->inbound + dtls->inbound_offset, available);
    dtls->inbound_offset += available;
    if (dtls->inbound_offset >= dtls->inbound_len) {
        dtls->inbound_len = 0;
        dtls->inbound_offset = 0;
    }
    return (int) available;
}

static int mrtc_mbedtls_p_hash(mbedtls_md_type_t md_type,
                               const unsigned char *secret,
                               size_t secret_len,
                               const unsigned char *seed,
                               size_t seed_len,
                               unsigned char *out,
                               size_t out_len)
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(md_type);
    unsigned char a[MBEDTLS_MD_MAX_SIZE];
    unsigned char digest[MBEDTLS_MD_MAX_SIZE];
    size_t md_len;
    size_t done = 0;
    if (info == 0) {
        return 0;
    }
    md_len = (size_t) mbedtls_md_get_size(info);
    if (mbedtls_md_hmac(info, secret, secret_len, seed, seed_len, a) != 0) {
        return 0;
    }
    while (done < out_len) {
        mbedtls_md_context_t ctx;
        size_t copy_len;
        mbedtls_md_init(&ctx);
        if (mbedtls_md_setup(&ctx, info, 1) != 0 ||
            mbedtls_md_hmac_starts(&ctx, secret, secret_len) != 0 ||
            mbedtls_md_hmac_update(&ctx, a, md_len) != 0 ||
            mbedtls_md_hmac_update(&ctx, seed, seed_len) != 0 ||
            mbedtls_md_hmac_finish(&ctx, digest) != 0) {
            mbedtls_md_free(&ctx);
            return 0;
        }
        mbedtls_md_free(&ctx);
        copy_len = out_len - done < md_len ? out_len - done : md_len;
        memcpy(out + done, digest, copy_len);
        done += copy_len;
        if (done < out_len && mbedtls_md_hmac(info, secret, secret_len, a, md_len, a) != 0) {
            return 0;
        }
    }
    return 1;
}

static int mrtc_mbedtls_export_keys(void *ctx,
                                    const unsigned char *master_secret,
                                    const unsigned char *key_block,
                                    size_t maclen,
                                    size_t keylen,
                                    size_t ivlen,
                                    const unsigned char client_random[32],
                                    const unsigned char server_random[32],
                                    mbedtls_tls_prf_types tls_prf_type)
{
    static const char label[] = "EXTRACTOR-dtls_srtp";
    MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) ctx;
    unsigned char seed[sizeof(label) - 1u + 64u];
    unsigned char material[60];
    mbedtls_md_type_t md_type = tls_prf_type == MBEDTLS_SSL_TLS_PRF_SHA384 ? MBEDTLS_MD_SHA384 : MBEDTLS_MD_SHA256;
    (void) key_block;
    (void) maclen;
    (void) keylen;
    (void) ivlen;
    if (dtls == 0 || master_secret == 0 || client_random == 0 || server_random == 0) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    memcpy(seed, label, sizeof(label) - 1u);
    memcpy(seed + sizeof(label) - 1u, client_random, 32u);
    memcpy(seed + sizeof(label) - 1u + 32u, server_random, 32u);
    if (!mrtc_mbedtls_p_hash(md_type, master_secret, 48u, seed, sizeof(seed), material, sizeof(material))) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    memset(&dtls->keying_material, 0, sizeof(dtls->keying_material));
    memcpy(dtls->keying_material.client_write_key, material, sizeof(dtls->keying_material.client_write_key));
    memcpy(dtls->keying_material.server_write_key, material + 16u, sizeof(dtls->keying_material.server_write_key));
    memcpy(dtls->keying_material.client_write_salt, material + 32u, sizeof(dtls->keying_material.client_write_salt));
    memcpy(dtls->keying_material.server_write_salt, material + 46u, sizeof(dtls->keying_material.server_write_salt));
    (void) snprintf(dtls->keying_material.profile, sizeof(dtls->keying_material.profile), "SRTP_AES128_CM_SHA1_80");
    dtls->has_keying_material = 1;
    return 0;
}

static MRTC_STATUS mrtc_mbedtls_fingerprint_from_der(const unsigned char *der,
                                                     size_t der_len,
                                                     char *buffer,
                                                     size_t buffer_len,
                                                     size_t *required_len)
{
    unsigned char digest[32];
    char text[96];
    size_t offset = 0;
    size_t i;
    if (der == 0 || der_len == 0u || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mbedtls_sha256_ret(der, der_len, digest, 0) != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    for (i = 0; i < sizeof(digest); ++i) {
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

static int mrtc_mbedtls_ensure_global_certificate(void)
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509write_cert writer;
    mbedtls_mpi serial;
    unsigned char der[4096];
    const char *personalization = "libmicrortc-dtls";
    int ret;
    size_t required_len = 0;

    if (g_mbedtls_certificate_ready) {
        return 1;
    }

    mbedtls_x509_crt_init(&g_mbedtls_certificate);
    mbedtls_pk_init(&g_mbedtls_private_key);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_x509write_crt_init(&writer);
    mbedtls_mpi_init(&serial);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                                mbedtls_entropy_func,
                                &entropy,
                                (const unsigned char *) personalization,
                                strlen(personalization));
    if (ret != 0 ||
        mbedtls_pk_setup(&g_mbedtls_private_key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
        mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                            mbedtls_pk_ec(g_mbedtls_private_key),
                            mbedtls_ctr_drbg_random,
                            &ctr_drbg) != 0 ||
        mbedtls_mpi_lset(&serial, 1) != 0) {
        goto fail;
    }

    mbedtls_x509write_crt_set_version(&writer, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&writer, MBEDTLS_MD_SHA256);
    if (mbedtls_x509write_crt_set_serial(&writer, &serial) != 0 ||
        mbedtls_x509write_crt_set_subject_name(&writer, "CN=libmicrortc") != 0 ||
        mbedtls_x509write_crt_set_issuer_name(&writer, "CN=libmicrortc") != 0 ||
        mbedtls_x509write_crt_set_validity(&writer, "20240101000000", "20340101000000") != 0) {
        goto fail;
    }
    mbedtls_x509write_crt_set_subject_key(&writer, &g_mbedtls_private_key);
    mbedtls_x509write_crt_set_issuer_key(&writer, &g_mbedtls_private_key);
    ret = mbedtls_x509write_crt_der(&writer, der, sizeof(der), mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret <= 0) {
        goto fail;
    }
    if (mbedtls_x509_crt_parse_der(&g_mbedtls_certificate, der + sizeof(der) - (size_t) ret, (size_t) ret) != 0 ||
        mrtc_mbedtls_fingerprint_from_der(g_mbedtls_certificate.raw.p,
                                          g_mbedtls_certificate.raw.len,
                                          g_mbedtls_fingerprint,
                                          sizeof(g_mbedtls_fingerprint),
                                          &required_len) != MRTC_STATUS_OK) {
        goto fail;
    }
    g_mbedtls_certificate_ready = 1;
    mbedtls_mpi_free(&serial);
    mbedtls_x509write_crt_free(&writer);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return 1;

fail:
    mbedtls_mpi_free(&serial);
    mbedtls_x509write_crt_free(&writer);
    mbedtls_pk_free(&g_mbedtls_private_key);
    mbedtls_x509_crt_free(&g_mbedtls_certificate);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    g_mbedtls_fingerprint[0] = '\0';
    return 0;
}

static MRTC_STATUS mrtc_mbedtls_verify_connected_fingerprint(MRTC_DTLS_SESSION *session)
{
    MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
    const mbedtls_x509_crt *peer;
    char fingerprint[96];
    size_t required_len = 0;
    mbedtls_dtls_srtp_info srtp_info;

    if (session == 0 || dtls == 0 || session->expected_peer_fingerprint[0] == '\0') {
        return MRTC_STATUS_INVALID_STATE;
    }
    peer = mbedtls_ssl_get_peer_cert(&dtls->ssl);
    if (peer == 0 ||
        mrtc_mbedtls_fingerprint_from_der(peer->raw.p, peer->raw.len, fingerprint, sizeof(fingerprint), &required_len) != MRTC_STATUS_OK ||
        strcasecmp(fingerprint, session->expected_peer_fingerprint) != 0) {
        session->state = MRTC_DTLS_STATE_FAILED;
        return MRTC_STATUS_INVALID_STATE;
    }
    mbedtls_ssl_get_dtls_srtp_negotiation_result(&dtls->ssl, &srtp_info);
    if (srtp_info.chosen_dtls_srtp_profile != MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80) {
        session->state = MRTC_DTLS_STATE_FAILED;
        return MRTC_STATUS_INVALID_STATE;
    }
    (void) snprintf(session->verified_peer_fingerprint, sizeof(session->verified_peer_fingerprint), "%s", fingerprint);
    session->remote_fingerprint_verified = 1;
    session->state = MRTC_DTLS_STATE_CONNECTED;
    if (dtls->has_keying_material) {
        session->keying_material = dtls->keying_material;
    }
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_mbedtls_do_handshake(MRTC_DTLS_SESSION *session)
{
    MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
    int ret;
    if (dtls == 0 || session->state == MRTC_DTLS_STATE_FAILED) {
        return MRTC_STATUS_INVALID_STATE;
    }
    ret = mbedtls_ssl_handshake(&dtls->ssl);
    if (ret == 0) {
        return mrtc_mbedtls_verify_connected_fingerprint(session);
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return MRTC_STATUS_OK;
    }
    session->state = MRTC_DTLS_STATE_FAILED;
    return MRTC_STATUS_INVALID_STATE;
}

static MRTC_STATUS mrtc_mbedtls_create_ssl(MRTC_DTLS_SESSION *session)
{
    MRTC_MBEDTLS_DTLS *dtls;
    const char *personalization = "libmicrortc-session";
    static const mbedtls_ssl_srtp_profile srtp_profiles[] = {
        MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
        MBEDTLS_TLS_SRTP_UNSET
    };
    static const int ciphersuites[] = {
#ifdef MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
#endif
        0
    };
    int endpoint;

    if (!mrtc_mbedtls_ensure_global_certificate()) {
        return MRTC_STATUS_INVALID_STATE;
    }
    dtls = (MRTC_MBEDTLS_DTLS *) calloc(1u, sizeof(*dtls));
    if (dtls == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    mbedtls_ssl_init(&dtls->ssl);
    mbedtls_ssl_config_init(&dtls->conf);
    mbedtls_ctr_drbg_init(&dtls->ctr_drbg);
    mbedtls_entropy_init(&dtls->entropy);
    endpoint = session->role == MRTC_DTLS_ROLE_SERVER ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    if (mbedtls_ctr_drbg_seed(&dtls->ctr_drbg,
                              mbedtls_entropy_func,
                              &dtls->entropy,
                              (const unsigned char *) personalization,
                              strlen(personalization)) != 0 ||
        mbedtls_ssl_config_defaults(&dtls->conf, endpoint, MBEDTLS_SSL_TRANSPORT_DATAGRAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        goto fail;
    }
    mbedtls_ssl_conf_min_version(&dtls->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&dtls->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_authmode(&dtls->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    if (ciphersuites[0] != 0) {
        mbedtls_ssl_conf_ciphersuites(&dtls->conf, ciphersuites);
    }
    mbedtls_ssl_conf_rng(&dtls->conf, mbedtls_ctr_drbg_random, &dtls->ctr_drbg);
    mbedtls_ssl_conf_export_keys_ext_cb(&dtls->conf, mrtc_mbedtls_export_keys, dtls);
    mbedtls_ssl_conf_srtp_mki_value_supported(&dtls->conf, MBEDTLS_SSL_DTLS_SRTP_MKI_UNSUPPORTED);
    if (mbedtls_ssl_conf_dtls_srtp_protection_profiles(&dtls->conf, srtp_profiles) != 0 ||
        mbedtls_ssl_conf_own_cert(&dtls->conf, &g_mbedtls_certificate, &g_mbedtls_private_key) != 0 ||
        mbedtls_ssl_setup(&dtls->ssl, &dtls->conf) != 0) {
        goto fail;
    }
    mbedtls_ssl_set_bio(&dtls->ssl, dtls, mrtc_mbedtls_send, mrtc_mbedtls_recv, 0);
    mbedtls_ssl_set_timer_cb(&dtls->ssl, &dtls->timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
    session->ssl = dtls;
    session->ssl_ctx = &dtls->conf;
    session->certificate = &g_mbedtls_certificate;
    session->private_key = &g_mbedtls_private_key;
    return MRTC_STATUS_OK;

fail:
    mbedtls_ssl_free(&dtls->ssl);
    mbedtls_ssl_config_free(&dtls->conf);
    mbedtls_ctr_drbg_free(&dtls->ctr_drbg);
    mbedtls_entropy_free(&dtls->entropy);
    free(dtls->inbound);
    free(dtls->outbound);
    free(dtls);
    return MRTC_STATUS_INVALID_STATE;
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
#elif defined(MRTC_HAVE_MBEDTLS)
    if (required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!mrtc_mbedtls_ensure_global_certificate()) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *required_len = strlen(g_mbedtls_fingerprint) + 1u;
    if (buffer == 0 || buffer_len < *required_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, g_mbedtls_fingerprint, *required_len);
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
#elif defined(MRTC_HAVE_MBEDTLS)
    if (mrtc_mbedtls_create_ssl(session) != MRTC_STATUS_OK) {
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
#elif defined(MRTC_HAVE_MBEDTLS)
        {
            MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
            if (dtls != 0) {
                mbedtls_ssl_free(&dtls->ssl);
                mbedtls_ssl_config_free(&dtls->conf);
                mbedtls_ctr_drbg_free(&dtls->ctr_drbg);
                mbedtls_entropy_free(&dtls->entropy);
                free(dtls->inbound);
                free(dtls->outbound);
                free(dtls);
            }
        }
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
#elif defined(MRTC_HAVE_MBEDTLS)
    if (session->ssl == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (session->state == MRTC_DTLS_STATE_NEW) {
        session->state = MRTC_DTLS_STATE_CONNECTING;
    }
    return mrtc_mbedtls_do_handshake(session);
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
#elif defined(MRTC_HAVE_MBEDTLS)
    {
        MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
        if (dtls == 0 || session->state == MRTC_DTLS_STATE_FAILED) {
            return MRTC_STATUS_INVALID_STATE;
        }
        if (!mrtc_mbedtls_ensure_capacity(&dtls->inbound, &dtls->inbound_capacity, packet_len)) {
            session->state = MRTC_DTLS_STATE_FAILED;
            return MRTC_STATUS_INVALID_STATE;
        }
        memcpy(dtls->inbound, packet, packet_len);
        dtls->inbound_len = packet_len;
        dtls->inbound_offset = 0;
        if (session->state != MRTC_DTLS_STATE_CONNECTED) {
            MRTC_STATUS status = mrtc_mbedtls_do_handshake(session);
            if (status != MRTC_STATUS_OK && session->state == MRTC_DTLS_STATE_FAILED) {
                return status;
            }
        }
        if (session->state == MRTC_DTLS_STATE_CONNECTED) {
            uint8_t app_data[2048];
            for (;;) {
                int ret = mbedtls_ssl_read(&dtls->ssl, app_data, sizeof(app_data));
                if (ret > 0) {
                    if (session->on_application_data != 0) {
                        session->on_application_data(session->application_data_user_data, app_data, (size_t) ret);
                    }
                    continue;
                }
                if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                    break;
                }
                if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    session->state = MRTC_DTLS_STATE_FAILED;
                    return MRTC_STATUS_INVALID_STATE;
                }
                break;
            }
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
#elif defined(MRTC_HAVE_MBEDTLS)
    {
        MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
        size_t available;
        if (dtls == 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        if (dtls->outbound_offset >= dtls->outbound_len) {
            dtls->outbound_len = 0;
            dtls->outbound_offset = 0;
            return MRTC_STATUS_OK;
        }
        available = dtls->outbound_len - dtls->outbound_offset;
        if (available > packet_capacity) {
            return MRTC_STATUS_INVALID_ARG;
        }
        memcpy(packet, dtls->outbound + dtls->outbound_offset, available);
        dtls->outbound_offset += available;
        if (dtls->outbound_offset >= dtls->outbound_len) {
            dtls->outbound_len = 0;
            dtls->outbound_offset = 0;
        }
        *packet_len = available;
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
#elif defined(MRTC_HAVE_MBEDTLS)
    {
        MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
        int ret;
        if (dtls == 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        ret = mbedtls_ssl_write(&dtls->ssl, data, data_len);
        return ret == (int) data_len || ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE
                   ? MRTC_STATUS_OK
                   : MRTC_STATUS_INVALID_STATE;
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
#elif defined(MRTC_HAVE_MBEDTLS)
    {
        MRTC_MBEDTLS_DTLS *dtls = (MRTC_MBEDTLS_DTLS *) session->ssl;
        if (dtls == 0 || !dtls->has_keying_material) {
            return MRTC_STATUS_INVALID_STATE;
        }
        *keying_material = dtls->keying_material;
    }
#else
    *keying_material = session->keying_material;
#endif
    return MRTC_STATUS_OK;
}
