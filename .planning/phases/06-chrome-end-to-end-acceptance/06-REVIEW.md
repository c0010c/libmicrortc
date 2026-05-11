---
phase: 06-chrome-end-to-end-acceptance
reviewed: 2026-05-11T10:10:01Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - examples/chrome_e2e/rtc_chrome_e2e.c
  - examples/chrome_e2e/run_e2e.mjs
  - CMakeLists.txt
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 06: Code Review Report

**Reviewed:** 2026-05-11T10:10:01Z
**Depth:** standard
**Files Reviewed:** 3
**Status:** clean

## Summary

本次按用户指定 scope 对上一轮 4 个发现进行 re-review：`examples/chrome_e2e/rtc_chrome_e2e.c`、`examples/chrome_e2e/run_e2e.mjs` 和 `CMakeLists.txt`。检查重点为 WebSocket 发送完整性与失败传播、`runId` JSON 转义、嵌套输出目录创建，以及可选安全依赖在 examples 关闭时的 CMake gate。

CR-01、CR-02、WR-01、WR-02 均已解决。未发现这些修复引入新的 blocker 或 warning。

## Resolved Findings

### CR-01: RESOLVED - 非阻塞 WebSocket 发送可能部分写入，但仍把 answer/candidate 标记为已发送

**File:** `examples/chrome_e2e/rtc_chrome_e2e.c:641`

**Resolution:** `ws_send_text()` 现在循环发送完整 WebSocket frame，处理短写、`EAGAIN/EWOULDBLOCK` 和 `EINTR`。`hello`、`answer`、`candidate` 和 `status` 发送路径现在检查 `ws_send_text()` 返回值；`handle_offer()` 只有在 answer 发送成功后才设置 `ctx->answer_sent = 1`，失败时记录 `answer.send_failed` 并返回错误。

### CR-02: RESOLVED - media/security smoke 在干净 checkout 上不能创建嵌套输出目录

**File:** `examples/chrome_e2e/run_e2e.mjs:478`, `examples/chrome_e2e/run_e2e.mjs:537`, `examples/chrome_e2e/rtc_chrome_e2e.c:255`

**Resolution:** `mediaFileSmoke()` 和 `securityGateSmoke()` 都在运行 C 示例前使用 `mkdirSync(..., { recursive: true })` 创建输出目录。C 侧 `ensure_output_dir()` 也改为逐级创建目录，支持嵌套 output path。给定证据中的 direct nested dry-run、media smoke 和 security gate smoke 均已通过。

### WR-01: RESOLVED - examples 关闭时仍会触发可选 OpenSSL/libsrtp 依赖 fatal gate

**File:** `CMakeLists.txt:13`

**Resolution:** `cmake/FindRtcOptionalSecurity.cmake` 现在只在 `RTC_BUILD_EXAMPLES` 和 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` 同时开启时 include。`rtc_chrome_e2e` target 的可选 include/link 仍保留在 examples target 内，避免 `RTC_BUILD_EXAMPLES=OFF` 时触发 Chrome E2E 专属依赖。给定证据中的 `RTC_BUILD_EXAMPLES=OFF` + optional security ON 配置已通过。

### WR-02: RESOLVED - `--run-id` 直接拼进 JSON，特殊字符会破坏信令消息

**File:** `examples/chrome_e2e/rtc_chrome_e2e.c:363`, `examples/chrome_e2e/rtc_chrome_e2e.c:712`, `examples/chrome_e2e/rtc_chrome_e2e.c:862`, `examples/chrome_e2e/rtc_chrome_e2e.c:1005`, `examples/chrome_e2e/rtc_chrome_e2e.c:1326`, `examples/chrome_e2e/rtc_chrome_e2e.c:1574`

**Resolution:** 新增的 `escape_run_id()` 复用 `json_escape_string()`，对 `"`、`\`、换行、回车、制表符和其他控制字符进行 JSON 转义。`send_status()`、`on_local_candidate()`、`handle_offer()` 和 hello 发送路径均使用转义后的 run id；转义失败会记录 signaling failure 或写失败 summary。

## Verification

本次 re-review 采信并核对了以下通过证据：cmake build、direct nested dry-run、`npm run e2e:chrome:media-smoke`、`npm run e2e:chrome:security-gate`、`node c-example-smoke`、`npm run e2e:chrome:dry-run`、`ctest`，以及 `RTC_BUILD_EXAMPLES=OFF` + optional security ON 的 CMake 配置。

---

_Reviewed: 2026-05-11T10:10:01Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
