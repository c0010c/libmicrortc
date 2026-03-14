#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "rtc/rtc.h"
#include "device_manual_logic.h"
#include "platform_linux.h"

static const char *k_log_path = "build/rtc_device.log";
static FILE *g_log_file = 0;

typedef struct {
  rtc_session_t *session;
  uint16_t active_channel_id;
  int has_active_channel;
} app_state_t;

static void make_timestamp(char *out, size_t out_len) {
  time_t now;
  struct tm tmv;
  if (!out || out_len == 0) {
    return;
  }
  now = time(0);
  if (now == (time_t)-1 || localtime_r(&now, &tmv) == 0 ||
      strftime(out, out_len, "%Y-%m-%dT%H:%M:%S%z", &tmv) == 0) {
    (void)snprintf(out, out_len, "1970-01-01T00:00:00+0000");
  }
}

static void write_log_line(const char *source, const char *level, const char *msg) {
  char ts[64];
  const char *src = source ? source : "device";
  const char *lvl = level ? level : "I";
  const char *text = msg ? msg : "";
  make_timestamp(ts, sizeof(ts));
  fprintf(stdout, "[%s] [%s][%s] %s\n", ts, src, lvl, text);
  fflush(stdout);
  if (g_log_file) {
    fprintf(g_log_file, "[%s] [%s][%s] %s\n", ts, src, lvl, text);
    fflush(g_log_file);
  }
}

static void app_logf(const char *source, const char *level, const char *fmt, ...) {
  char line[1024];
  va_list ap;
  if (!fmt) {
    return;
  }
  va_start(ap, fmt);
  (void)vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  write_log_line(source, level, line);
}

static void write_dual_text(const char *text, size_t len) {
  if (!text || len == 0) {
    return;
  }
  (void)fwrite(text, 1, len, stdout);
  if (text[len - 1] != '\n') {
    (void)fputc('\n', stdout);
  }
  fflush(stdout);
  if (g_log_file) {
    (void)fwrite(text, 1, len, g_log_file);
    if (text[len - 1] != '\n') {
      (void)fputc('\n', g_log_file);
    }
    fflush(g_log_file);
  }
}

static void open_log_sink(const char *path) {
  char ts[64];
  const char *effective = path ? path : k_log_path;
  g_log_file = fopen(effective, "a");
  if (!g_log_file) {
    app_logf("device",
             "W",
             "failed to open %s: %s (fallback to terminal-only logging)",
             effective,
             strerror(errno));
    return;
  }
  make_timestamp(ts, sizeof(ts));
  fprintf(g_log_file, "\n===== rtc_device session %s pid=%ld =====\n", ts, (long)getpid());
  fflush(g_log_file);
  app_logf("device", "I", "logging to %s", effective);
}

static void close_log_sink(void) {
  if (!g_log_file) {
    return;
  }
  fflush(g_log_file);
  fclose(g_log_file);
  g_log_file = 0;
}

static const char *rtc_level_tag(rtc_log_level_t level) {
  switch (level) {
  case RTC_LOG_ERROR:
    return "E";
  case RTC_LOG_WARN:
    return "W";
  case RTC_LOG_DEBUG:
    return "D";
  case RTC_LOG_INFO:
  default:
    return "I";
  }
}

static void rtc_log_sink(void *user, rtc_log_level_t level, const char *msg) {
  (void)user;
  app_logf("rtc", rtc_level_tag(level), "%s", msg ? msg : "");
}

static int send_channel_text(app_state_t *app, uint16_t channel_id, const char *text, const char *reason) {
  rtc_result_t rc;
  size_t len;
  if (!app || !app->session || !text || !reason) {
    return -1;
  }
  len = strlen(text);
  rc = rtc_channel_send(app->session, channel_id, (const uint8_t *)text, len);
  if (rc != RTC_OK) {
    app_logf("device", "W", "%s send failed id=%u rc=%d", reason, (unsigned)channel_id, (int)rc);
    if (rc == RTC_ERR_NOT_FOUND || rc == RTC_ERR_STATE) {
      app->has_active_channel = 0;
    }
    return -1;
  }
  app_logf("device", "I", "%s tx id=%u len=%zu: %s", reason, (unsigned)channel_id, len, text);
  return 0;
}

static void on_ice(void *user, rtc_ice_state_t st) {
  (void)user;
  app_logf("device", "I", "ice state=%d", (int)st);
}

static void on_dtls(void *user, rtc_dtls_state_t st) {
  (void)user;
  app_logf("device", "I", "dtls state=%d", (int)st);
}

static void on_dc_open(void *user, uint16_t channel_id, const char *label) {
  app_state_t *app = (app_state_t *)user;
  char greeting[128];
  int greeting_len;
  app_logf("device", "I", "channel open id=%u label=%s", (unsigned)channel_id, label ? label : "");
  if (!app) {
    return;
  }
  app->active_channel_id = channel_id;
  app->has_active_channel = 1;
  greeting_len = rtc_device_build_open_greeting(label, greeting, sizeof(greeting));
  if (greeting_len > 0) {
    (void)send_channel_text(app, channel_id, greeting, "welcome");
  }
}

static void on_dc_message(void *user, uint16_t channel_id, const uint8_t *data, size_t len) {
  app_state_t *app = (app_state_t *)user;
  char reply[256];
  int reply_len;
  app_logf("device",
           "I",
           "channel message id=%u len=%zu: %.*s",
           (unsigned)channel_id,
           len,
           (int)len,
           data ? (const char *)data : "");
  if (!app) {
    return;
  }
  app->active_channel_id = channel_id;
  app->has_active_channel = 1;
  reply_len = rtc_device_build_echo_reply(data, len, reply, sizeof(reply));
  if (reply_len > 0) {
    (void)send_channel_text(app, channel_id, reply, "echo");
  }
}

static void on_error(void *user, rtc_result_t err, const char *msg) {
  (void)user;
  app_logf("device", "E", "error=%d msg=%s", (int)err, msg ? msg : "");
}

static void on_local_candidate(void *user, const char *candidate_line) {
  (void)user;
  app_logf("device", "I", "local candidate: %s", candidate_line ? candidate_line : "");
}

static int read_offer_block(char *offer, size_t cap) {
  size_t used = 0;
  char line[1024];
  if (!offer || cap == 0) {
    return -1;
  }
  app_logf("device", "I", "Paste offer SDP lines, then input END_OFFER on its own line.");
  for (;;) {
    if (!fgets(line, sizeof(line), stdin)) {
      break;
    }
    if (strcmp(line, "END_OFFER\n") == 0 || strcmp(line, "END_OFFER\r\n") == 0) {
      break;
    }
    if (used + strlen(line) + 1 >= cap) {
      return -1;
    }
    memcpy(offer + used, line, strlen(line));
    used += strlen(line);
  }
  offer[used] = '\0';
  return used > 0 ? 0 : -1;
}

static int process_runtime_stdin(app_state_t *app) {
  fd_set rfds;
  struct timeval tv;
  char line[1024];
  char message[512];
  int rc;
  rtc_device_input_kind_t kind;
  if (!app || !app->session) {
    return 0;
  }
  FD_ZERO(&rfds);
  FD_SET(STDIN_FILENO, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
  if (rc <= 0 || !FD_ISSET(STDIN_FILENO, &rfds)) {
    return 0;
  }
  if (!fgets(line, sizeof(line), stdin)) {
    return 0;
  }
  kind = rtc_device_classify_input(line);
  if (kind == RTC_DEVICE_INPUT_REMOTE_CANDIDATE && strncmp(line, "candidate:", 10) == 0) {
    if (rtc_session_add_remote_candidate(app->session, line) == RTC_OK) {
      app_logf("device", "I", "remote candidate added");
    } else {
      app_logf("device", "W", "remote candidate add failed");
    }
    return 0;
  }
  if (kind == RTC_DEVICE_INPUT_REMOTE_CANDIDATE && strncmp(line, "a=candidate:", 12) == 0) {
    if (rtc_session_add_remote_candidate(app->session, line + 2) == RTC_OK) {
      app_logf("device", "I", "remote candidate added");
    } else {
      app_logf("device", "W", "remote candidate add failed");
    }
    return 0;
  }
  if (kind == RTC_DEVICE_INPUT_QUIT) {
    app_logf("device", "I", "quit requested");
    return 1;
  }
  if (kind == RTC_DEVICE_INPUT_MESSAGE) {
    if (!app->has_active_channel) {
      app_logf("device", "W", "no open channel yet; text not sent");
      return 0;
    }
    if (rtc_device_copy_message_text(line, message, sizeof(message)) < 0) {
      app_logf("device", "W", "message too long or empty");
      return 0;
    }
    (void)send_channel_text(app, app->active_channel_id, message, "manual");
    return 0;
  }
  app_logf("device", "D", "ignored stdin line: %s", line);
  return 0;
}

int main(void) {
  rtc_platform_ops_t ops;
  rtc_config_t cfg;
  rtc_callbacks_t cbs;
  rtc_session_t *s = 0;
  app_state_t app;
  char answer[2048];
  size_t written = 0;
  int exit_code = 1;
  int started = 0;

  open_log_sink(k_log_path);

  memset(&app, 0, sizeof(app));
  memset(&cfg, 0, sizeof(cfg));
  memset(&cbs, 0, sizeof(cbs));

  rtc_linux_default_ops(&ops);
  ops.log = rtc_log_sink;

  cfg.stun_server_ip = "74.125.250.129"; /* stun.l.google.com */
  cfg.stun_server_port = 19302;
  cfg.bind_ip = "192.168.31.182"; /* set your device LAN IP here */
  cfg.bind_port = 50000;          /* fixed port to emit host candidate in answer */
  cfg.max_channels = 4;
  cfg.max_message_size = 1200;
  cfg.mempool_bytes = 512 * 1024;
  cfg.log_level = RTC_LOG_DEBUG;

  cbs.on_ice_state = on_ice;
  cbs.on_dtls_state = on_dtls;
  cbs.on_channel_open = on_dc_open;
  cbs.on_channel_message = on_dc_message;
  cbs.on_local_candidate = on_local_candidate;
  cbs.on_error = on_error;
  cbs.user = &app;

  s = rtc_session_create(&cfg, &ops, 0, &cbs);
  if (!s) {
    app_logf("device", "E", "session create failed");
    goto cleanup;
  }
  app.session = s;

  app_logf("device",
           "I",
           "Paste browser offer SDP. After start, you can paste trickle lines: candidate:... or a=candidate:...");
  app_logf("device", "I", "Type quit to exit.");
  {
    static char offer[2048];
    if (read_offer_block(offer, sizeof(offer)) != 0) {
      app_logf("device", "E", "read offer failed (use END_OFFER terminator)");
      goto cleanup;
    }
    if (rtc_session_set_remote_offer(s, offer) != RTC_OK) {
      app_logf("device", "E", "set remote offer failed");
      goto cleanup;
    }
  }

  if (rtc_session_create_answer(s, answer, sizeof(answer), &written) != RTC_OK) {
    app_logf("device", "E", "create answer failed");
    goto cleanup;
  }
  app_logf("device", "I", "=== COPY ANSWER TO BROWSER ===");
  write_dual_text(answer, written);

  if (rtc_session_start(s) != RTC_OK) {
    app_logf("device", "E", "session start failed");
    goto cleanup;
  }
  started = 1;
  exit_code = 0;

  for (;;) {
    if (process_runtime_stdin(&app)) {
      break;
    }
    rtc_session_poll(s);
    usleep(1000);
  }

cleanup:
  if (s) {
    if (started) {
      (void)rtc_session_close(s);
    }
    rtc_session_destroy(s);
  }
  close_log_sink();
  return exit_code;
}
