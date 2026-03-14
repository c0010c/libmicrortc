#include "sctp_adapter.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if RTC_WITH_USRSCTP
#include <usrsctp.h>
#endif

#define RTC_DCEP_PPID 50u
#define RTC_STRING_PPID 51u
#define RTC_DCEP_OPEN 0x03u
#define RTC_DCEP_ACK 0x02u

#if RTC_WITH_USRSCTP
static void sctp_emit_error(rtc_sctp_adapter_t *s, const char *msg) {
  if (s && s->on_error) {
    s->on_error(s->cb_user, msg);
  }
}

typedef struct {
  rtc_sctp_adapter_t *owner;
  struct socket *sock;
  uintptr_t local_addr;
  uintptr_t peer_addr;
  int initialized;
  int connected;
} rtc_sctp_usrsctp_t;

static int sctp_outbound_cb(void *addr,
                            void *buffer,
                            size_t length,
                            uint8_t tos,
                            uint8_t set_df) {
  rtc_sctp_usrsctp_t *ctx = (rtc_sctp_usrsctp_t *)addr;
  rtc_sctp_adapter_t *s;
  int rc = -1;
  (void)tos;
  (void)set_df;
  if (!ctx || !buffer) {
    return -1;
  }
  s = ctx->owner;
  if (s && s->send_packet) {
    rc = s->send_packet(s->io_user, (const uint8_t *)buffer, length);
  }
  return rc == 0 ? 0 : -1;
}

static int sctp_send_dcep_ack(rtc_sctp_adapter_t *s, uint16_t sid) {
  rtc_sctp_usrsctp_t *ctx;
  uint8_t ack = RTC_DCEP_ACK;
  struct sctp_sendv_spa spa;
  if (!s || !s->impl) {
    return -1;
  }
  ctx = (rtc_sctp_usrsctp_t *)s->impl;
  if (!ctx->sock) {
    return -1;
  }
  memset(&spa, 0, sizeof(spa));
  spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
  spa.sendv_sndinfo.snd_sid = sid;
  spa.sendv_sndinfo.snd_ppid = htonl(RTC_DCEP_PPID);
  if (usrsctp_sendv(ctx->sock,
                    &ack,
                    sizeof(ack),
                    NULL,
                    0,
                    &spa,
                    (socklen_t)sizeof(spa),
                    SCTP_SENDV_SPA,
                    0) < 0) {
    return -1;
  }
  return 0;
}

static void sctp_handle_dcep(rtc_sctp_adapter_t *s, uint16_t sid, const uint8_t *data, size_t len) {
  if (!s || !data || len < 1) {
    return;
  }
  if (data[0] == RTC_DCEP_OPEN) {
    char label[80];
    uint16_t label_len = 0;
    uint16_t proto_len = 0;
    size_t off = 12;
    if (len < 12) {
      return;
    }
    label_len = (uint16_t)((data[8] << 8) | data[9]);
    proto_len = (uint16_t)((data[10] << 8) | data[11]);
    if (off + (size_t)label_len + (size_t)proto_len > len) {
      return;
    }
    if (label_len >= sizeof(label)) {
      label_len = (uint16_t)(sizeof(label) - 1);
    }
    memcpy(label, data + off, label_len);
    label[label_len] = '\0';
    if (sctp_send_dcep_ack(s, sid) != 0) {
      sctp_emit_error(s, "failed to send dcep ack");
    }
    if (s->on_open) {
      s->on_open(s->cb_user, sid, label);
    }
    return;
  }
  if (data[0] == RTC_DCEP_ACK) {
    if (s->on_open) {
      s->on_open(s->cb_user, sid, "");
    }
  }
}

static int sctp_receive_cb(struct socket *sock,
                           union sctp_sockstore addr,
                           void *data,
                           size_t datalen,
                           struct sctp_rcvinfo rcv,
                           int flags,
                           void *ulp_info) {
  rtc_sctp_usrsctp_t *ctx = (rtc_sctp_usrsctp_t *)ulp_info;
  rtc_sctp_adapter_t *s;
  (void)sock;
  (void)addr;
  if (!ctx) {
    if (data) {
      free(data);
    }
    return 1;
  }
  s = ctx->owner;
  if (!data) {
    return 1;
  }

  if (flags & MSG_NOTIFICATION) {
    union sctp_notification *n = (union sctp_notification *)data;
    if (n->sn_header.sn_type == SCTP_ASSOC_CHANGE) {
      struct sctp_assoc_change *ac = &n->sn_assoc_change;
      if (ac->sac_state == SCTP_COMM_UP) {
        ctx->connected = 1;
      }
    }
    free(data);
    return 1;
  }

  {
    uint32_t ppid = ntohl(rcv.rcv_ppid);
    if (ppid == RTC_DCEP_PPID) {
      sctp_handle_dcep(s, rcv.rcv_sid, (const uint8_t *)data, datalen);
    } else if (s && s->on_message) {
      s->on_message(s->cb_user, rcv.rcv_sid, (const uint8_t *)data, datalen);
    }
  }

  free(data);
  return 1;
}

static int rtc_sctp_usrsctp_init(rtc_sctp_adapter_t *s) {
  rtc_sctp_usrsctp_t *ctx;
  struct sockaddr_conn sconn;
  struct linger linger_opt;
  struct sctp_event event;

  if (!s) {
    return -1;
  }
  ctx = (rtc_sctp_usrsctp_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return -1;
  }
  ctx->owner = s;
  ctx->local_addr = (uintptr_t)ctx;
  ctx->peer_addr = ctx->local_addr;

  usrsctp_init(0, sctp_outbound_cb, 0);
  usrsctp_sysctl_set_sctp_ecn_enable(0);
  usrsctp_register_address((void *)ctx->local_addr);

  ctx->sock = usrsctp_socket(AF_CONN,
                             SOCK_STREAM,
                             IPPROTO_SCTP,
                             sctp_receive_cb,
                             NULL,
                             0,
                             ctx);
  if (!ctx->sock) {
    goto fail;
  }
  usrsctp_set_non_blocking(ctx->sock, 1);

  memset(&linger_opt, 0, sizeof(linger_opt));
  linger_opt.l_onoff = 1;
  linger_opt.l_linger = 0;
  if (usrsctp_setsockopt(ctx->sock,
                         SOL_SOCKET,
                         SO_LINGER,
                         &linger_opt,
                         (socklen_t)sizeof(linger_opt)) < 0) {
    goto fail;
  }

  memset(&event, 0, sizeof(event));
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  event.se_type = SCTP_ASSOC_CHANGE;
  if (usrsctp_setsockopt(ctx->sock,
                         IPPROTO_SCTP,
                         SCTP_EVENT,
                         &event,
                         (socklen_t)sizeof(event)) < 0) {
    goto fail;
  }

  memset(&sconn, 0, sizeof(sconn));
#ifdef HAVE_SCONN_LEN
  sconn.sconn_len = sizeof(sconn);
#endif
  sconn.sconn_family = AF_CONN;
  sconn.sconn_port = htons(5000);
  sconn.sconn_addr = (void *)ctx->local_addr;
  if (usrsctp_bind(ctx->sock, (struct sockaddr *)&sconn, (socklen_t)sizeof(sconn)) < 0) {
    goto fail;
  }

  memset(&sconn, 0, sizeof(sconn));
#ifdef HAVE_SCONN_LEN
  sconn.sconn_len = sizeof(sconn);
#endif
  sconn.sconn_family = AF_CONN;
  sconn.sconn_port = htons(5000);
  sconn.sconn_addr = (void *)ctx->peer_addr;
  if (usrsctp_connect(ctx->sock, (struct sockaddr *)&sconn, (socklen_t)sizeof(sconn)) < 0) {
    if (errno != EINPROGRESS) {
      goto fail;
    }
  }

  ctx->initialized = 1;
  s->impl = ctx;
  s->using_usrsctp = 1;
  return 0;

fail:
  if (ctx->sock) {
    usrsctp_close(ctx->sock);
    ctx->sock = NULL;
  }
  usrsctp_deregister_address((void *)ctx->local_addr);
  usrsctp_finish();
  free(ctx);
  return -1;
}

static void rtc_sctp_usrsctp_deinit(rtc_sctp_adapter_t *s) {
  rtc_sctp_usrsctp_t *ctx;
  if (!s || !s->impl) {
    return;
  }
  ctx = (rtc_sctp_usrsctp_t *)s->impl;
  if (ctx->sock) {
    usrsctp_close(ctx->sock);
    ctx->sock = NULL;
  }
  usrsctp_deregister_address((void *)ctx->local_addr);
  usrsctp_finish();
  free(ctx);
  s->impl = 0;
  s->using_usrsctp = 0;
}

static int rtc_sctp_usrsctp_send(rtc_sctp_adapter_t *s,
                                 uint16_t sid,
                                 uint32_t ppid,
                                 const uint8_t *data,
                                 size_t len) {
  rtc_sctp_usrsctp_t *ctx;
  struct sctp_sendv_spa spa;
  if (!s || !s->impl || (!data && len > 0)) {
    return -1;
  }
  ctx = (rtc_sctp_usrsctp_t *)s->impl;
  if (!ctx->sock) {
    return -1;
  }
  memset(&spa, 0, sizeof(spa));
  spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
  spa.sendv_sndinfo.snd_sid = sid;
  spa.sendv_sndinfo.snd_ppid = htonl(ppid);
  if (usrsctp_sendv(ctx->sock,
                    data,
                    len,
                    NULL,
                    0,
                    &spa,
                    (socklen_t)sizeof(spa),
                    SCTP_SENDV_SPA,
                    0) < 0) {
    return -1;
  }
  return 0;
}

static int rtc_sctp_send_dcep_open(rtc_sctp_adapter_t *s, uint16_t sid, const char *label) {
  uint8_t msg[256];
  size_t label_len;
  size_t total;
  if (!s || !label) {
    return -1;
  }
  label_len = strlen(label);
  if (label_len > 128u) {
    label_len = 128u;
  }
  total = 12u + label_len;
  if (total > sizeof(msg)) {
    return -1;
  }
  memset(msg, 0, total);
  msg[0] = RTC_DCEP_OPEN;
  msg[1] = 0x00; /* reliable ordered */
  msg[8] = (uint8_t)((label_len >> 8) & 0xFFu);
  msg[9] = (uint8_t)(label_len & 0xFFu);
  memcpy(msg + 12, label, label_len);
  return rtc_sctp_usrsctp_send(s, sid, RTC_DCEP_PPID, msg, total);
}
#endif

void rtc_sctp_init(rtc_sctp_adapter_t *s) {
  if (!s) {
    return;
  }
  memset(s, 0, sizeof(*s));
  s->next_id = 0;
#if RTC_WITH_USRSCTP
  if (rtc_sctp_usrsctp_init(s) != 0) {
    s->using_usrsctp = 0;
    s->impl = 0;
  }
#endif
}

void rtc_sctp_deinit(rtc_sctp_adapter_t *s) {
  if (!s) {
    return;
  }
#if RTC_WITH_USRSCTP
  rtc_sctp_usrsctp_deinit(s);
#endif
}

void rtc_sctp_set_io(rtc_sctp_adapter_t *s, rtc_sctp_send_packet_fn fn, void *io_user) {
  if (!s) {
    return;
  }
  s->send_packet = fn;
  s->io_user = io_user;
}

void rtc_sctp_set_callbacks(rtc_sctp_adapter_t *s,
                            rtc_sctp_on_open_fn on_open,
                            rtc_sctp_on_message_fn on_message,
                            rtc_sctp_on_error_fn on_error,
                            void *cb_user) {
  if (!s) {
    return;
  }
  s->on_open = on_open;
  s->on_message = on_message;
  s->on_error = on_error;
  s->cb_user = cb_user;
}

void rtc_sctp_start(rtc_sctp_adapter_t *s) {
  if (!s) {
    return;
  }
  s->started = 1;
}

void rtc_sctp_handle_incoming(rtc_sctp_adapter_t *s, const uint8_t *pkt, size_t len) {
#if RTC_WITH_USRSCTP
  rtc_sctp_usrsctp_t *ctx;
#endif
  if (!s || !s->started || !pkt || len == 0) {
    return;
  }
#if RTC_WITH_USRSCTP
  if (s->using_usrsctp && s->impl) {
    ctx = (rtc_sctp_usrsctp_t *)s->impl;
    usrsctp_conninput((void *)ctx->local_addr, pkt, len, 0);
    return;
  }
#endif
}

int rtc_sctp_open_channel(rtc_sctp_adapter_t *s, const char *label, uint16_t *id) {
  uint16_t sid;
  if (!s || !label || !id || !s->started) {
    return -1;
  }
  sid = s->next_id;
  s->next_id = (uint16_t)(s->next_id + 2u);
  *id = sid;
#if RTC_WITH_USRSCTP
  if (s->using_usrsctp) {
    if (rtc_sctp_send_dcep_open(s, sid, label) != 0) {
      sctp_emit_error(s, "failed to send dcep open");
      return -1;
    }
  }
#endif
  return 0;
}

int rtc_sctp_send(rtc_sctp_adapter_t *s, uint16_t id, const uint8_t *data, size_t len) {
  if (!s || !s->started || (!data && len > 0)) {
    return -1;
  }
#if RTC_WITH_USRSCTP
  if (s->using_usrsctp) {
    return rtc_sctp_usrsctp_send(s, id, RTC_STRING_PPID, data, len);
  }
#endif
  (void)id;
  return 0;
}

int rtc_sctp_close_channel(rtc_sctp_adapter_t *s, uint16_t id) {
  (void)id;
  if (!s || !s->started) {
    return -1;
  }
  return 0;
}
