#include "rtc/rtc.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTC_SIGNAL_CLI_MAX_LINE 4096u
#define RTC_SIGNAL_CLI_DEFAULT_BUDGET_US 500u
#define RTC_SIGNAL_CLI_DEFAULT_RUN_STEP_MS 10u

typedef struct rtc_signal_cli_app {
  rtc_engine_t *engine;
  rtc_peer_t *peer;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  uint32_t now_ms;
  uint8_t collecting_offer;
  uint8_t has_answer;
  uint8_t should_quit;
  size_t offer_len;
  char offer[RTC_CFG_MAX_SDP_LEN];
  char answer[RTC_CFG_MAX_SDP_LEN];
  char answer_type[16];
} rtc_signal_cli_app_t;

static const char *cli_result_text(rtc_result_t code) {
  switch (code) {
    case RTC_OK:
      return "ok";
    case RTC_ERR_INVALID_ARG:
      return "invalid_arg";
    case RTC_ERR_INVALID_STATE:
      return "invalid_state";
    case RTC_ERR_RESOURCE_EXHAUSTED:
      return "resource_exhausted";
    case RTC_ERR_PROTOCOL:
      return "protocol_error";
    case RTC_ERR_TIMEOUT:
      return "timeout";
    case RTC_ERR_NOT_INIT:
      return "not_init";
    case RTC_ERR_NOT_SUPPORTED:
      return "not_supported";
    case RTC_ERR_BUFFER_TOO_SMALL:
      return "buffer_too_small";
    case RTC_ERR_OVERFLOW:
      return "overflow";
    case RTC_ERR_DTLS_HANDSHAKE_FAILED:
      return "dtls_handshake_failed";
    case RTC_ERR_SRTP_ACTIVATE_FAILED:
      return "srtp_activate_failed";
    case RTC_ERR_AUTH_FAILED:
      return "auth_failed";
    default:
      return "error";
  }
}

static const char *cli_peer_state_text(rtc_peer_state_t state) {
  switch (state) {
    case RTC_PEER_STATE_NEW:
      return "new";
    case RTC_PEER_STATE_STARTING:
      return "starting";
    case RTC_PEER_STATE_ICE_CHECKING:
      return "ice_checking";
    case RTC_PEER_STATE_DTLS_HANDSHAKE:
      return "dtls_handshake";
    case RTC_PEER_STATE_CONNECTED:
      return "connected";
    case RTC_PEER_STATE_STOPPED:
      return "stopped";
    case RTC_PEER_STATE_FAILED:
      return "failed";
    default:
      return "unknown";
  }
}

static const char *cli_log_level_text(rtc_log_level_t level) {
  switch (level) {
    case RTC_LOG_ERROR:
      return "ERROR";
    case RTC_LOG_WARN:
      return "WARN";
    case RTC_LOG_INFO:
      return "INFO";
    case RTC_LOG_DEBUG:
      return "DEBUG";
    default:
      return "UNKNOWN";
  }
}

static const char *cli_skip_ws(const char *p) {
  if (!p) {
    return NULL;
  }
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  return p;
}

static void cli_rstrip_crlf(char *line) {
  size_t len;
  if (!line) {
    return;
  }
  len = strlen(line);
  while (len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r')) {
    len--;
  }
  line[len] = '\0';
}

static int cli_parse_u32_token(const char *input, uint32_t *value,
                               const char **out_rest) {
  const char *p;
  char *end = NULL;
  unsigned long parsed;
  if (!input || !value) {
    return 0;
  }
  p = cli_skip_ws(input);
  if (!p || *p == '\0') {
    return 0;
  }
  errno = 0;
  parsed = strtoul(p, &end, 10);
  if (errno != 0 || end == p || parsed > UINT32_MAX) {
    return 0;
  }
  *value = (uint32_t)parsed;
  if (out_rest) {
    *out_rest = end;
  }
  return 1;
}

static void cli_print_ok(const char *cmd) {
  printf("OK %s\n", cmd ? cmd : "");
  fflush(stdout);
}

static void cli_print_err(rtc_result_t code, const char *reason) {
  const char *text = reason;
  if (!text || text[0] == '\0') {
    text = cli_result_text(code);
  }
  printf("ERR %d %s\n", (int)code, text);
  fflush(stdout);
}

static void cli_emit_local_description(const char *sdp, const char *type) {
  if (!sdp || !type) {
    return;
  }
  printf("EVENT LOCAL_DESCRIPTION_BEGIN %s\n", type);
  fputs(sdp, stdout);
  if (sdp[0] != '\0') {
    size_t len = strlen(sdp);
    if (len == 0u || (sdp[len - 1u] != '\n' && sdp[len - 1u] != '\r')) {
      fputc('\n', stdout);
    }
  }
  printf("EVENT LOCAL_DESCRIPTION_END\n");
  fflush(stdout);
}

static void cli_log_cb(rtc_log_level_t level, const char *module, uint32_t peer_id,
                       rtc_result_t code, const char *message, void *user_data) {
  (void)user_data;
  fprintf(stderr, "%s\t%s\tpeer=%" PRIu32 "\tcode=%d\t%s\n",
          cli_log_level_text(level), module ? module : "core", peer_id, (int)code,
          message ? message : "");
}

static void cli_state_cb(rtc_peer_t *peer, rtc_peer_state_t old_state,
                         rtc_peer_state_t new_state, void *user_data) {
  (void)peer;
  (void)user_data;
  printf("EVENT STATE %s %s\n", cli_peer_state_text(old_state),
         cli_peer_state_text(new_state));
  fflush(stdout);
}

static void cli_local_desc_cb(rtc_peer_t *peer, const char *sdp, const char *type,
                              void *user_data) {
  rtc_signal_cli_app_t *app = (rtc_signal_cli_app_t *)user_data;
  (void)peer;

  if (!app || !sdp || !type) {
    return;
  }

  (void)snprintf(app->answer, sizeof(app->answer), "%s", sdp);
  (void)snprintf(app->answer_type, sizeof(app->answer_type), "%s", type);
  app->has_answer = 1u;
  cli_emit_local_description(sdp, type);
}

static void cli_local_candidate_cb(rtc_peer_t *peer, const char *candidate,
                                   void *user_data) {
  (void)peer;
  (void)user_data;
  if (!candidate) {
    return;
  }
  printf("EVENT LOCAL_CANDIDATE %s\n", candidate);
  fflush(stdout);
}

static rtc_result_t cli_create_peer(rtc_signal_cli_app_t *app) {
  rtc_result_t r;
  if (!app || !app->engine) {
    return RTC_ERR_INVALID_ARG;
  }
  app->peer = NULL;
  r = rtc_peer_create(app->engine, &app->peer_cfg, &app->peer);
  if (r != RTC_OK) {
    return r;
  }
  app->collecting_offer = 0u;
  app->offer_len = 0u;
  app->offer[0] = '\0';
  app->has_answer = 0u;
  app->answer[0] = '\0';
  app->answer_type[0] = '\0';
  return RTC_OK;
}

static rtc_result_t cli_restart_peer(rtc_signal_cli_app_t *app) {
  rtc_result_t r;
  if (!app || !app->engine || !app->peer) {
    return RTC_ERR_INVALID_STATE;
  }
  r = rtc_peer_destroy(app->peer);
  if (r != RTC_OK) {
    return r;
  }
  app->peer = NULL;
  return cli_create_peer(app);
}

static rtc_result_t cli_offer_append_line(rtc_signal_cli_app_t *app,
                                          const char *line) {
  size_t line_len;
  if (!app || !line) {
    return RTC_ERR_INVALID_ARG;
  }
  line_len = strlen(line);
  if (app->offer_len + line_len + 2u >= sizeof(app->offer)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  memcpy(app->offer + app->offer_len, line, line_len);
  app->offer_len += line_len;
  app->offer[app->offer_len++] = '\r';
  app->offer[app->offer_len++] = '\n';
  app->offer[app->offer_len] = '\0';
  return RTC_OK;
}

static rtc_result_t cli_cmd_tick(rtc_signal_cli_app_t *app, const char *args) {
  rtc_result_t r;
  uint32_t now_ms;
  uint32_t budget_us = RTC_SIGNAL_CLI_DEFAULT_BUDGET_US;
  const char *rest = NULL;
  const char *tail = NULL;

  if (!app || !args) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!cli_parse_u32_token(args, &now_ms, &rest)) {
    return RTC_ERR_INVALID_ARG;
  }
  tail = cli_skip_ws(rest);
  if (tail && *tail != '\0') {
    if (!cli_parse_u32_token(tail, &budget_us, &tail)) {
      return RTC_ERR_INVALID_ARG;
    }
    tail = cli_skip_ws(tail);
    if (tail && *tail != '\0') {
      return RTC_ERR_INVALID_ARG;
    }
  }
  r = rtc_engine_poll(app->engine, now_ms, budget_us);
  if (r != RTC_OK) {
    return r;
  }
  app->now_ms = now_ms;
  return RTC_OK;
}

static rtc_result_t cli_cmd_run(rtc_signal_cli_app_t *app, const char *args) {
  rtc_result_t r;
  uint32_t duration_ms;
  uint32_t step_ms = RTC_SIGNAL_CLI_DEFAULT_RUN_STEP_MS;
  uint32_t budget_us = RTC_SIGNAL_CLI_DEFAULT_BUDGET_US;
  const char *rest = NULL;
  const char *tail = NULL;
  uint32_t elapsed = 0u;

  if (!app || !args) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!cli_parse_u32_token(args, &duration_ms, &rest)) {
    return RTC_ERR_INVALID_ARG;
  }
  tail = cli_skip_ws(rest);
  if (tail && *tail != '\0') {
    if (!cli_parse_u32_token(tail, &step_ms, &tail)) {
      return RTC_ERR_INVALID_ARG;
    }
    tail = cli_skip_ws(tail);
    if (tail && *tail != '\0') {
      if (!cli_parse_u32_token(tail, &budget_us, &tail)) {
        return RTC_ERR_INVALID_ARG;
      }
      tail = cli_skip_ws(tail);
      if (tail && *tail != '\0') {
        return RTC_ERR_INVALID_ARG;
      }
    }
  }
  if (step_ms == 0u) {
    return RTC_ERR_INVALID_ARG;
  }

  while (elapsed < duration_ms) {
    uint32_t advance = step_ms;
    if (duration_ms - elapsed < advance) {
      advance = duration_ms - elapsed;
    }
    if (UINT32_MAX - app->now_ms < advance) {
      return RTC_ERR_OVERFLOW;
    }
    app->now_ms += advance;
    elapsed += advance;
    r = rtc_engine_poll(app->engine, app->now_ms, budget_us);
    if (r != RTC_OK) {
      return r;
    }
  }
  return RTC_OK;
}

static rtc_result_t cli_cmd_get_answer(rtc_signal_cli_app_t *app) {
  if (!app) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!app->has_answer) {
    return RTC_ERR_INVALID_STATE;
  }
  cli_emit_local_description(app->answer, app->answer_type);
  return RTC_OK;
}

static rtc_result_t cli_cmd_state(rtc_signal_cli_app_t *app) {
  rtc_peer_state_t state;
  rtc_result_t r;
  if (!app) {
    return RTC_ERR_INVALID_ARG;
  }
  r = rtc_peer_get_state(app->peer, &state);
  if (r != RTC_OK) {
    return r;
  }
  printf("STATE %s\n", cli_peer_state_text(state));
  fflush(stdout);
  return RTC_OK;
}

static rtc_result_t cli_cmd_stats(rtc_signal_cli_app_t *app) {
  rtc_result_t r;
  rtc_peer_stats_t peer_stats;
  rtc_engine_stats_t engine_stats;
  rtc_peer_state_t state;

  if (!app) {
    return RTC_ERR_INVALID_ARG;
  }
  r = rtc_peer_get_state(app->peer, &state);
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_peer_get_stats(app->peer, &peer_stats);
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_engine_get_stats(app->engine, &engine_stats);
  if (r != RTC_OK) {
    return r;
  }

  printf(
      "STATS state=%s local_candidates=%u remote_candidates=%u "
      "ice_sent=%u ice_ok=%u ice_failed=%u dtls_state=%u srtp_active=%u "
      "dtls_last_error=%d dropped=%u retransmit=%u queue_overflow=%u "
      "poll_count=%u poll_budget_exhaust=%u\n",
      cli_peer_state_text(state), (unsigned)peer_stats.local_candidate_count,
      (unsigned)peer_stats.remote_candidate_count, (unsigned)peer_stats.ice_checks_sent,
      (unsigned)peer_stats.ice_checks_ok, (unsigned)peer_stats.ice_checks_failed,
      (unsigned)peer_stats.dtls_state, (unsigned)peer_stats.srtp_active,
      (int)peer_stats.dtls_last_error, (unsigned)peer_stats.dropped_packets,
      (unsigned)peer_stats.retransmit_count, (unsigned)peer_stats.queue_overflow_count,
      (unsigned)engine_stats.poll_count, (unsigned)engine_stats.poll_budget_exhaust_count);
  fflush(stdout);
  return RTC_OK;
}

static rtc_result_t cli_handle_command(rtc_signal_cli_app_t *app, char *line,
                                       const char **out_cmd_name) {
  char *cmd;
  char *args;

  if (!app || !line || !out_cmd_name) {
    return RTC_ERR_INVALID_ARG;
  }

  if (app->collecting_offer) {
    if (strcmp(line, "set-offer-end") == 0) {
      app->collecting_offer = 0u;
      *out_cmd_name = "set-offer-end";
      return rtc_peer_set_remote_description(app->peer, app->offer, "offer");
    }
    *out_cmd_name = NULL;
    return cli_offer_append_line(app, line);
  }

  cmd = line;
  args = line;
  while (*args != '\0' && *args != ' ' && *args != '\t') {
    args++;
  }
  if (*args != '\0') {
    *args = '\0';
    args++;
  }
  args = (char *)cli_skip_ws(args);
  if (!cmd[0]) {
    return RTC_OK;
  }

  if (strcmp(cmd, "set-offer-begin") == 0) {
    app->collecting_offer = 1u;
    app->offer_len = 0u;
    app->offer[0] = '\0';
    *out_cmd_name = "set-offer-begin";
    return RTC_OK;
  }
  if (strcmp(cmd, "add-remote-candidate") == 0) {
    *out_cmd_name = "add-remote-candidate";
    if (!args || args[0] == '\0') {
      return RTC_ERR_INVALID_ARG;
    }
    return rtc_peer_add_remote_candidate(app->peer, args);
  }
  if (strcmp(cmd, "start") == 0) {
    *out_cmd_name = "start";
    return rtc_peer_start(app->peer);
  }
  if (strcmp(cmd, "tick") == 0) {
    *out_cmd_name = "tick";
    return cli_cmd_tick(app, args);
  }
  if (strcmp(cmd, "run") == 0) {
    *out_cmd_name = "run";
    return cli_cmd_run(app, args);
  }
  if (strcmp(cmd, "get-answer") == 0) {
    *out_cmd_name = "get-answer";
    return cli_cmd_get_answer(app);
  }
  if (strcmp(cmd, "state") == 0) {
    *out_cmd_name = "state";
    return cli_cmd_state(app);
  }
  if (strcmp(cmd, "stats") == 0) {
    *out_cmd_name = "stats";
    return cli_cmd_stats(app);
  }
  if (strcmp(cmd, "restart-peer") == 0) {
    *out_cmd_name = "restart-peer";
    return cli_restart_peer(app);
  }
  if (strcmp(cmd, "quit") == 0) {
    *out_cmd_name = "quit";
    app->should_quit = 1u;
    return RTC_OK;
  }

  *out_cmd_name = cmd;
  return RTC_ERR_NOT_SUPPORTED;
}

static void cli_drain_line(FILE *in) {
  int ch;
  if (!in) {
    return;
  }
  do {
    ch = fgetc(in);
  } while (ch != EOF && ch != '\n');
}

static int cli_init(rtc_signal_cli_app_t *app) {
  rtc_result_t r;
  if (!app) {
    return 1;
  }

  memset(app, 0, sizeof(*app));

  memset(&app->engine_cfg, 0, sizeof(app->engine_cfg));
  app->engine_cfg.version = RTC_API_VERSION;
  app->engine_cfg.size = (uint16_t)sizeof(app->engine_cfg);
  app->engine_cfg.min_log_level = RTC_LOG_INFO;
  app->engine_cfg.log_cb = cli_log_cb;
  app->engine_cfg.log_user_data = app;
  app->engine_cfg.active_peer_limit = RTC_CFG_DEFAULT_ACTIVE_PEERS;

  memset(&app->peer_cfg, 0, sizeof(app->peer_cfg));
  app->peer_cfg.version = RTC_API_VERSION;
  app->peer_cfg.size = (uint16_t)sizeof(app->peer_cfg);
  app->peer_cfg.max_retries = 5u;
  app->peer_cfg.retry_interval_ms = 10u;
  app->peer_cfg.dtls_handshake_timeout_ms = 3000u;
  app->peer_cfg.dtls_handshake_max_retries = 16u;
  app->peer_cfg.on_state_change = cli_state_cb;
  app->peer_cfg.on_local_description = cli_local_desc_cb;
  app->peer_cfg.on_local_candidate = cli_local_candidate_cb;
  app->peer_cfg.user_data = app;

  r = rtc_engine_create(&app->engine_cfg, &app->engine);
  if (r != RTC_OK) {
    fprintf(stderr, "rtc_engine_create failed: %d (%s)\n", (int)r, cli_result_text(r));
    return 1;
  }
  r = cli_create_peer(app);
  if (r != RTC_OK) {
    fprintf(stderr, "rtc_peer_create failed: %d (%s)\n", (int)r, cli_result_text(r));
    (void)rtc_engine_destroy(app->engine);
    app->engine = NULL;
    return 1;
  }

  return 0;
}

static void cli_deinit(rtc_signal_cli_app_t *app) {
  if (!app) {
    return;
  }
  if (app->peer) {
    (void)rtc_peer_destroy(app->peer);
    app->peer = NULL;
  }
  if (app->engine) {
    (void)rtc_engine_destroy(app->engine);
    app->engine = NULL;
  }
}

int main(void) {
  rtc_signal_cli_app_t app;
  char line[RTC_SIGNAL_CLI_MAX_LINE];

  if (cli_init(&app) != 0) {
    return 1;
  }

  while (!app.should_quit && fgets(line, sizeof(line), stdin) != NULL) {
    rtc_result_t cmd_result;
    const char *cmd_name = "";
    size_t len = strlen(line);
    int line_has_newline = (len > 0u && line[len - 1u] == '\n');

    if (!line_has_newline && !feof(stdin)) {
      cli_drain_line(stdin);
      app.collecting_offer = 0u;
      cli_print_err(RTC_ERR_BUFFER_TOO_SMALL, "line_too_long");
      continue;
    }

    cli_rstrip_crlf(line);
    if (line[0] == '\0' && !app.collecting_offer) {
      continue;
    }

    cmd_result = cli_handle_command(&app, line, &cmd_name);
    if (cmd_name == NULL && cmd_result == RTC_OK) {
      continue;
    }
    if (cmd_result == RTC_OK) {
      cli_print_ok(cmd_name);
    } else {
      char reason[96];
      (void)snprintf(reason, sizeof(reason), "%s failed", cmd_name ? cmd_name : "cmd");
      cli_print_err(cmd_result, reason);
      if (app.collecting_offer && cmd_result != RTC_OK) {
        app.collecting_offer = 0u;
      }
    }
  }

  cli_deinit(&app);
  return 0;
}
