#include "dtls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if RTC_WITH_MBEDTLS
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/timing.h>
#include <mbedtls/x509_crt.h>
#endif

#if RTC_WITH_MBEDTLS
static const char kRtcDtlsServerKeyPem[] =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEICNMQBanSP/yYpwd2HQMI9eh8NkwrtnqSEdZ16JzuVB5oAoGCCqGSM49\n"
    "AwEHoUQDQgAEjHlWGSx6gN7D6yxDnXIMY7ImQ1Uoo72M6n55KnMO2LvkSNJe9bFY\n"
    "5nHoEvexW4TQ23vacAJDrzWBgVvcJQ/6iQ==\n"
    "-----END EC PRIVATE KEY-----\n";

static const char kRtcDtlsServerCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBdzCCAR2gAwIBAgIUGzV5vLewj+xI8YeEVED8FPisH5owCgYIKoZIzj0EAwIw\n"
    "ETEPMA0GA1UEAwwGcnRjLXMxMB4XDTI2MDMxNDA5MTYzMVoXDTM2MDMxMTA5MTYz\n"
    "MVowETEPMA0GA1UEAwwGcnRjLXMxMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\n"
    "jHlWGSx6gN7D6yxDnXIMY7ImQ1Uoo72M6n55KnMO2LvkSNJe9bFY5nHoEvexW4TQ\n"
    "23vacAJDrzWBgVvcJQ/6iaNTMFEwHQYDVR0OBBYEFICLxTeqkLGr2fm0bIyRVTeT\n"
    "+UnzMB8GA1UdIwQYMBaAFICLxTeqkLGr2fm0bIyRVTeT+UnzMA8GA1UdEwEB/wQF\n"
    "MAMBAf8wCgYIKoZIzj0EAwIDSAAwRQIhAIHYA9cM5SBOQMrhDjS75p2nSeHVnOVY\n"
    "VFSx2AI76nFKAiBKINvSeTt+WFd+1b4NFL0+B8KkSnFjLt4/gHkLmJqOsw==\n"
    "-----END CERTIFICATE-----\n";

typedef struct {
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_cookie_ctx cookie;
  mbedtls_timing_delay_context timer;
  mbedtls_x509_crt cert;
  mbedtls_pk_context pkey;
  uint8_t rx_buf[2048];
  size_t rx_len;
  size_t rx_off;
  uint8_t peer_transport_id[256];
  size_t peer_transport_id_len;
  int initialized;
  rtc_dtls_t *owner;
  char fingerprint_sha256[96];
} rtc_dtls_mbedtls_t;

static void dtls_emit_error(rtc_dtls_t *d, const char *msg) {
  if (d && d->on_error) {
    d->on_error(d->cb_user, msg);
  }
}

static void dtls_emit_mbedtls_error(rtc_dtls_t *d, int rc, const char *where) {
  char msg[256];
  char errbuf[128];
  if (!d) {
    return;
  }
  mbedtls_strerror(rc, errbuf, sizeof(errbuf));
  (void)snprintf(msg, sizeof(msg), "%s rc=%d (0x%04X): %s", where, rc, (unsigned)(-rc), errbuf);
  dtls_emit_error(d, msg);
}

static int mbedtls_send_cb(void *ctx, const unsigned char *buf, size_t len) {
  rtc_dtls_t *d = (rtc_dtls_t *)ctx;
  if (!d || !d->send_packet) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  if (d->send_packet(d->io_user, (const uint8_t *)buf, len) < 0) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  return (int)len;
}

static int mbedtls_recv_cb(void *ctx, unsigned char *buf, size_t len) {
  rtc_dtls_t *d = (rtc_dtls_t *)ctx;
  rtc_dtls_mbedtls_t *m;
  size_t n;
  if (!d || !buf || len == 0 || !d->impl) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  m = (rtc_dtls_mbedtls_t *)d->impl;
  if (m->rx_off >= m->rx_len) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  n = m->rx_len - m->rx_off;
  if (n > len) {
    n = len;
  }
  memcpy(buf, m->rx_buf + m->rx_off, n);
  m->rx_off += n;
  if (m->rx_off >= m->rx_len) {
    m->rx_len = 0;
    m->rx_off = 0;
  }
  return (int)n;
}

static int rtc_dtls_mbedtls_init(rtc_dtls_t *d, const char *cert_pem, const char *key_pem) {
  static const char kPers[] = "rtc-s1-dtls";
  rtc_dtls_mbedtls_t *ctx;
  int rc;

  if (!d) {
    return -1;
  }
  ctx = (rtc_dtls_mbedtls_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return -1;
  }

  mbedtls_ssl_init(&ctx->ssl);
  mbedtls_ssl_config_init(&ctx->conf);
  mbedtls_entropy_init(&ctx->entropy);
  mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
  mbedtls_ssl_cookie_init(&ctx->cookie);
  mbedtls_x509_crt_init(&ctx->cert);
  mbedtls_pk_init(&ctx->pkey);

  rc = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg,
                             mbedtls_entropy_func,
                             &ctx->entropy,
                             (const unsigned char *)kPers,
                             sizeof(kPers) - 1);
  if (rc != 0) {
    goto fail;
  }

  rc = mbedtls_ssl_config_defaults(&ctx->conf,
                                   MBEDTLS_SSL_IS_SERVER,
                                   MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                   MBEDTLS_SSL_PRESET_DEFAULT);
  if (rc != 0) {
    goto fail;
  }

  mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
  mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_NONE);

  if (!cert_pem) {
    cert_pem = kRtcDtlsServerCertPem;
  }
  if (!key_pem) {
    key_pem = kRtcDtlsServerKeyPem;
  }
  rc = mbedtls_x509_crt_parse(&ctx->cert, (const unsigned char *)cert_pem, strlen(cert_pem) + 1);
  if (rc != 0) {
    goto fail;
  }
  rc = mbedtls_pk_parse_key(&ctx->pkey,
                            (const unsigned char *)key_pem,
                            strlen(key_pem) + 1,
                            NULL,
                            0,
                            mbedtls_ctr_drbg_random,
                            &ctx->ctr_drbg);
  if (rc != 0) {
    goto fail;
  }
  rc = mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->cert, &ctx->pkey);
  if (rc != 0) {
    goto fail;
  }

  rc = mbedtls_ssl_cookie_setup(&ctx->cookie, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
  if (rc != 0) {
    goto fail;
  }

  mbedtls_ssl_conf_dtls_cookies(&ctx->conf,
                                mbedtls_ssl_cookie_write,
                                mbedtls_ssl_cookie_check,
                                &ctx->cookie);

  rc = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
  if (rc != 0) {
    goto fail;
  }

  mbedtls_ssl_set_timer_cb(&ctx->ssl,
                           &ctx->timer,
                           mbedtls_timing_set_delay,
                           mbedtls_timing_get_delay);

  {
    unsigned char digest[32];
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    size_t i;
    const char hex[] = "0123456789ABCDEF";
    if (!md || mbedtls_md(md, ctx->cert.raw.p, ctx->cert.raw.len, digest) != 0) {
      goto fail;
    }
    for (i = 0; i < 32; ++i) {
      ctx->fingerprint_sha256[i * 3 + 0] = hex[(digest[i] >> 4) & 0x0F];
      ctx->fingerprint_sha256[i * 3 + 1] = hex[digest[i] & 0x0F];
      ctx->fingerprint_sha256[i * 3 + 2] = (i == 31) ? '\0' : ':';
    }
  }

  ctx->owner = d;
  mbedtls_ssl_set_bio(&ctx->ssl, d, mbedtls_send_cb, mbedtls_recv_cb, 0);

  ctx->initialized = 1;
  d->impl = ctx;
  d->using_mbedtls = 1;
  return 0;

fail:
  mbedtls_ssl_free(&ctx->ssl);
  mbedtls_ssl_config_free(&ctx->conf);
  mbedtls_entropy_free(&ctx->entropy);
  mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
  mbedtls_ssl_cookie_free(&ctx->cookie);
  mbedtls_x509_crt_free(&ctx->cert);
  mbedtls_pk_free(&ctx->pkey);
  free(ctx);
  return -1;
}

static void rtc_dtls_mbedtls_free(rtc_dtls_t *d) {
  rtc_dtls_mbedtls_t *ctx;
  if (!d || !d->impl) {
    return;
  }
  ctx = (rtc_dtls_mbedtls_t *)d->impl;
  mbedtls_ssl_free(&ctx->ssl);
  mbedtls_ssl_config_free(&ctx->conf);
  mbedtls_entropy_free(&ctx->entropy);
  mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
  mbedtls_ssl_cookie_free(&ctx->cookie);
  mbedtls_x509_crt_free(&ctx->cert);
  mbedtls_pk_free(&ctx->pkey);
  free(ctx);
  d->impl = 0;
  d->using_mbedtls = 0;
}

static int rtc_dtls_mbedtls_reset_handshake(rtc_dtls_t *d) {
  rtc_dtls_mbedtls_t *ctx;
  int rc;
  if (!d || !d->impl) {
    return -1;
  }
  ctx = (rtc_dtls_mbedtls_t *)d->impl;
  rc = mbedtls_ssl_session_reset(&ctx->ssl);
  if (rc != 0) {
    dtls_emit_mbedtls_error(d, rc, "mbedtls session reset failed");
    return -1;
  }
  mbedtls_ssl_set_timer_cb(&ctx->ssl,
                           &ctx->timer,
                           mbedtls_timing_set_delay,
                           mbedtls_timing_get_delay);
  mbedtls_ssl_set_bio(&ctx->ssl, d, mbedtls_send_cb, mbedtls_recv_cb, 0);
  if (ctx->peer_transport_id_len > 0) {
    rc = mbedtls_ssl_set_client_transport_id(&ctx->ssl,
                                             ctx->peer_transport_id,
                                             ctx->peer_transport_id_len);
    if (rc != 0) {
      dtls_emit_mbedtls_error(d, rc, "set client transport id failed");
      return -1;
    }
  }
  ctx->rx_len = 0;
  ctx->rx_off = 0;
  return 0;
}

static void rtc_dtls_mbedtls_poll(rtc_dtls_t *d) {
  rtc_dtls_mbedtls_t *ctx;
  int rc;
  if (!d || !d->impl) {
    return;
  }
  ctx = (rtc_dtls_mbedtls_t *)d->impl;

  if (d->state == RTC_DTLS_CONNECTING) {
    rc = mbedtls_ssl_handshake(&ctx->ssl);
    if (rc == 0) {
      d->state = RTC_DTLS_CONNECTED;
    } else if (rc == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
      if (rtc_dtls_mbedtls_reset_handshake(d) != 0) {
        d->state = RTC_DTLS_FAILED;
      }
      return;
    } else if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
      d->state = RTC_DTLS_FAILED;
      dtls_emit_mbedtls_error(d, rc, "mbedtls handshake failed");
      return;
    }
  }

  if (d->state == RTC_DTLS_CONNECTED) {
    uint8_t app_buf[1600];
    for (;;) {
      rc = mbedtls_ssl_read(&ctx->ssl, app_buf, sizeof(app_buf));
      if (rc > 0) {
        if (d->on_appdata) {
          d->on_appdata(d->cb_user, app_buf, (size_t)rc);
        }
        continue;
      }
      if (rc == 0 || rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        return;
      }
      if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return;
      }
      d->state = RTC_DTLS_FAILED;
      dtls_emit_mbedtls_error(d, rc, "mbedtls read failed");
      return;
    }
  }
}

static void rtc_dtls_mbedtls_feed(rtc_dtls_t *d, const uint8_t *pkt, size_t len) {
  rtc_dtls_mbedtls_t *ctx;
  size_t n;
  if (!d || !d->impl || !pkt || len == 0) {
    return;
  }
  ctx = (rtc_dtls_mbedtls_t *)d->impl;
  if (ctx->rx_len > 0) {
    return;
  }
  n = len;
  if (n > sizeof(ctx->rx_buf)) {
    n = sizeof(ctx->rx_buf);
  }
  memcpy(ctx->rx_buf, pkt, n);
  ctx->rx_len = n;
  ctx->rx_off = 0;
}
#endif

void rtc_dtls_init(rtc_dtls_t *d, const char *cert_pem, const char *key_pem) {
  if (!d) {
    return;
  }
  d->state = RTC_DTLS_NEW;
  d->handshake_started_ms = 0;
  d->simulated_handshake_ms = 120;
  d->using_mbedtls = 0;
  d->impl = 0;
  d->send_packet = 0;
  d->io_user = 0;
  d->on_appdata = 0;
  d->on_error = 0;
  d->cb_user = 0;
#if RTC_WITH_MBEDTLS
  if (rtc_dtls_mbedtls_init(d, cert_pem, key_pem) != 0) {
    d->using_mbedtls = 0;
    d->impl = 0;
  }
#else
  (void)cert_pem;
  (void)key_pem;
#endif
}

void rtc_dtls_deinit(rtc_dtls_t *d) {
  if (!d) {
    return;
  }
#if RTC_WITH_MBEDTLS
  rtc_dtls_mbedtls_free(d);
#else
  (void)d;
#endif
}

void rtc_dtls_set_io(rtc_dtls_t *d, rtc_dtls_send_packet_fn send_packet, void *io_user) {
  if (!d) {
    return;
  }
  d->send_packet = send_packet;
  d->io_user = io_user;
}

void rtc_dtls_set_callbacks(rtc_dtls_t *d,
                            rtc_dtls_on_appdata_fn on_appdata,
                            rtc_dtls_on_error_fn on_error,
                            void *cb_user) {
  if (!d) {
    return;
  }
  d->on_appdata = on_appdata;
  d->on_error = on_error;
  d->cb_user = cb_user;
}

void rtc_dtls_start(rtc_dtls_t *d, uint64_t now_ms) {
  if (!d || d->state != RTC_DTLS_NEW) {
    return;
  }
  d->state = RTC_DTLS_CONNECTING;
  d->handshake_started_ms = now_ms;
}

void rtc_dtls_poll(rtc_dtls_t *d, uint64_t now_ms) {
  if (!d || (d->state != RTC_DTLS_CONNECTING && d->state != RTC_DTLS_CONNECTED)) {
    return;
  }
#if RTC_WITH_MBEDTLS
  if (d->using_mbedtls) {
    rtc_dtls_mbedtls_poll(d);
    return;
  }
#endif
  if (now_ms - d->handshake_started_ms >= d->simulated_handshake_ms) {
    d->state = RTC_DTLS_CONNECTED;
  }
}

void rtc_dtls_handle_incoming(rtc_dtls_t *d, const uint8_t *pkt, size_t len, uint64_t now_ms) {
  if (!d || !pkt || len == 0) {
    return;
  }
#if RTC_WITH_MBEDTLS
  if (d->using_mbedtls) {
    rtc_dtls_mbedtls_feed(d, pkt, len);
    rtc_dtls_mbedtls_poll(d);
    return;
  }
#endif
  (void)now_ms;
}

int rtc_dtls_send_application_data(rtc_dtls_t *d, const uint8_t *data, size_t len) {
  if (!d || (!data && len > 0) || d->state != RTC_DTLS_CONNECTED) {
    return -1;
  }
#if RTC_WITH_MBEDTLS
  if (d->using_mbedtls && d->impl) {
    rtc_dtls_mbedtls_t *ctx = (rtc_dtls_mbedtls_t *)d->impl;
    int rc = mbedtls_ssl_write(&ctx->ssl, data, len);
    if (rc < 0) {
      if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return 0;
      }
      dtls_emit_mbedtls_error(d, rc, "mbedtls write failed");
      return -1;
    }
    return 0;
  }
#endif
  if (d->send_packet) {
    return d->send_packet(d->io_user, data, len);
  }
  return -1;
}

int rtc_dtls_get_local_fingerprint_sha256(rtc_dtls_t *d, char *out, size_t out_len) {
  if (!d || !out || out_len == 0) {
    return -1;
  }
#if RTC_WITH_MBEDTLS
  if (d->using_mbedtls && d->impl) {
    rtc_dtls_mbedtls_t *ctx = (rtc_dtls_mbedtls_t *)d->impl;
    size_t n = strlen(ctx->fingerprint_sha256);
    if (n + 1 > out_len) {
      return -1;
    }
    memcpy(out, ctx->fingerprint_sha256, n + 1);
    return 0;
  }
#endif
  if (out_len < 96) {
    return -1;
  }
  strcpy(out, "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00");
  return 0;
}

int rtc_dtls_set_peer_transport_id(rtc_dtls_t *d, const uint8_t *id, size_t len) {
  if (!d || !id || len == 0) {
    return -1;
  }
#if RTC_WITH_MBEDTLS
  if (d->using_mbedtls && d->impl) {
    rtc_dtls_mbedtls_t *ctx = (rtc_dtls_mbedtls_t *)d->impl;
    if (len > sizeof(ctx->peer_transport_id)) {
      return -1;
    }
    memcpy(ctx->peer_transport_id, id, len);
    ctx->peer_transport_id_len = len;
    int rc = mbedtls_ssl_set_client_transport_id(&ctx->ssl, id, len);
    if (rc != 0) {
      dtls_emit_mbedtls_error(d, rc, "set client transport id failed");
      return -1;
    }
    return 0;
  }
#endif
  return 0;
}

const char *rtc_dtls_backend_name(const rtc_dtls_t *d) {
#if RTC_WITH_MBEDTLS
  if (d && d->using_mbedtls) {
    return "mbedtls";
  }
#else
  (void)d;
#endif
  return "stub";
}
