---
phase: 06-chrome-end-to-end-acceptance
reviewed: 2026-05-11T06:56:49Z
depth: standard
files_reviewed: 19
files_reviewed_list:
  - CMakeLists.txt
  - cmake/FindRtcOptionalSecurity.cmake
  - examples/chrome_e2e/signaling.mjs
  - examples/chrome_e2e/run_e2e.mjs
  - examples/chrome_e2e/page/index.html
  - examples/chrome_e2e/page/styles.css
  - examples/chrome_e2e/page/app.js
  - examples/chrome_e2e/README.md
  - examples/chrome_e2e/rtc_chrome_e2e.c
  - examples/chrome_e2e/jsonl.c
  - examples/chrome_e2e/jsonl.h
  - examples/chrome_e2e/media_samples.c
  - examples/chrome_e2e/media_samples.h
  - examples/chrome_e2e/security_backend_chrome.c
  - examples/chrome_e2e/security_backend_chrome.h
  - tests/test_chrome_e2e_samples.c
  - tests/test_main.c
  - package.json
  - docs/API-执行器与内存契约.md
findings:
  critical: 5
  warning: 1
  info: 0
  total: 6
status: issues_found
---

# Phase 6: Code Review Report

**Reviewed:** 2026-05-11T06:56:49Z
**Depth:** standard
**Files Reviewed:** 19
**Status:** issues_found

## Summary

本次按标准深度审查了 Chrome E2E harness、C 示例、样本解析、可选安全 backend gate、测试入口和中文契约文档。实现当前不能完成真实 Chrome 端到端验收：信令 runId 隔离会让浏览器 offer 永远不到达 C 示例；可选安全 backend 即使启用也始终返回 unsupported；运行时没有成功退出路径；媒体发送使用错误 executor；WebSocket 读取对 TCP 分片不健壮。另有 E2E smoke 没有接入默认测试命令的测试缺口。

## Critical Issues

### CR-01: BLOCKER - Chrome 与 C 示例使用不同 runId，信令服务不会转发 offer/answer

**File:** `examples/chrome_e2e/page/app.js:2`, `examples/chrome_e2e/rtc_chrome_e2e.c:28`, `examples/chrome_e2e/signaling.mjs:143`

**Issue:** 信令服务只向同一 `runId` 的 peer 广播消息。页面端每次启动生成 `crypto.randomUUID()`，C 示例却硬编码 `E2E_RUN_ID "c-example"`。因此页面发送的 `offer` 留在随机 run 内，C 示例只加入 `c-example` run，`pump_ws()` 永远收不到 offer，full E2E 会超时或停在错误层级。这直接阻断 `EXM-02/ACC-01`。

**Fix:**
```c
/* C 示例增加 --run-id，并在 hello/answer/candidate/status 中统一使用 options->run_id。 */
options->run_id = "c-example";
```

```js
// 页面从 URL 读取同一个 runId；run_e2e.mjs 启动页面和 C 示例时传入相同值。
const params = new URLSearchParams(window.location.search);
const runId = params.get("runId") ?? crypto.randomUUID();
```

### CR-02: BLOCKER - 启用可选安全依赖后 backend 仍始终返回 UNSUPPORTED

**File:** `examples/chrome_e2e/security_backend_chrome.c:12`

**Issue:** `CMakeLists.txt` 在 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 时查找并链接 OpenSSL/libsrtp，但 `rtc_chrome_e2e_configure_security_backend()` 在该宏定义存在时仍直接 `return RTC_STATUS_UNSUPPORTED`，且没有填充 `config->security_backend`。这意味着启用依赖后 full E2E 也必然停在 `dtls/optional_security_backend_disabled`，README 中“启用后可能到达 `dtls.connected` 和 `srtp.ready`”的路径不存在，`ACC-01` 无法关闭。

**Fix:** 实现可选 backend vtable 并设置 `config->security_backend`；如果当前阶段只打算保留 gate，则 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 不应被文档和 CMake 宣称为可运行路径。

```c
#if defined(RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY)
    config->security_backend = &chrome_security_backend;
    return RTC_STATUS_OK;
#else
    config->security_backend = 0;
    return RTC_STATUS_UNSUPPORTED;
#endif
```

### CR-03: BLOCKER - C 示例 runtime 无成功条件，最终总是写失败 summary

**File:** `examples/chrome_e2e/rtc_chrome_e2e.c:1328`

**Issue:** `run_runtime()` 主循环只按 timeout 轮询 `pump_ws/pump_udp/pump_media/flush_datagrams`，没有在 `offer.received`、`answer.sent`、`ice.connected`、`srtp.ready`、媒体收发或文件落盘满足时退出。循环结束后无条件写入 `pass:false`、`layer:"signaling"`、`reason:"no offer received"`。即使修复 runId、启用安全 backend 并完成媒体收发，结果仍会被覆盖为 signaling 失败。

**Fix:** 在 `e2e_context_t` 中记录 offer/answer/ICE/SRTP/media 状态，满足验收条件时写 passing summary 并返回 0；timeout 时根据最后状态选择真实 failure layer。

```c
if (ctx.media_ready &&
    ctx.audio_bytes_received > 0u &&
    ctx.video_bytes_received > 0u) {
    rtc_e2e_jsonl_summary_media(jsonl, 1, "none", "e2e passed",
                                ctx.audio_frames_received,
                                ctx.video_frames_received,
                                ctx.audio_bytes_received,
                                ctx.video_bytes_received);
    cleanup_and_return_success();
}
```

### CR-04: BLOCKER - 媒体发送在 signaling executor 上调用，违反 public API 亲和契约

**File:** `examples/chrome_e2e/rtc_chrome_e2e.c:1053`, `docs/API-执行器与内存契约.md:137`

**Issue:** 文档和 public contract 明确 `rtc_peer_connection_send_media_frame` 必须在 `RTC_EXECUTOR_MEDIA` 上调用，但示例在 `send_sample_frame()` 中设置的是 `RTC_EXECUTOR_SIGNALING`。核心 API 会返回 `RTC_STATUS_AFFINITY_VIOLATION`，当前代码还丢弃返回值，导致 `srtp.ready` 后样本帧不会真正进入 RTP 发送路径。

**Fix:**
```c
rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
status = rtc_peer_connection_send_media_frame(ctx->pc, &frame);
rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
if (status != RTC_STATUS_OK) {
    rtc_e2e_jsonl_event(ctx->jsonl, "error", "rtp", "failed",
                        "send_media_frame_failed");
}
```

### CR-05: BLOCKER - WebSocket 读取假设单次 recv 得到完整 frame/payload

**File:** `examples/chrome_e2e/rtc_chrome_e2e.c:628`

**Issue:** `ws_recv_text()` 在非阻塞 TCP socket 上先 `recv(fd, header, 2, 0)`，随后一次 `recv()` 读取扩展长度和 payload，并要求返回字节数完全等于期望长度。TCP/WebSocket 不保证 frame header 或 payload 在一次 `recv()` 中完整返回；Chrome offer/answer SDP 往往较大，更容易被拆分。当前代码会把正常分片当作错误或直接丢弃，造成 signaling 层不稳定失败。

**Fix:** 为 WebSocket client 增加持久输入缓冲区和 frame 状态机；至少实现 `read_exact` 语义，在 `EAGAIN/EWOULDBLOCK` 时保留已读字节，等下一轮继续，而不是将部分 frame 判定为失败。

```c
/* 伪代码：在 ctx 中保存 ws_rx buffer/state */
while (need_more_bytes(state)) {
    n = recv(fd, state->buf + state->used, state->cap - state->used, 0);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;
    }
    if (n <= 0) return -1;
    state->used += (size_t)n;
}
return decode_complete_frame(state, out, out_len);
```

## Warnings

### WR-01: WARNING - E2E smoke 没有接入默认测试入口，关键回归只能手工发现

**File:** `package.json:2`, `CMakeLists.txt:71`

**Issue:** `package.json` 只暴露 `e2e:chrome` 和 `e2e:chrome:dry-run`；CMake/CTest 只运行 `rtc_tests`。`--page-smoke`、`--c-example-smoke`、`--media-file-smoke`、`--security-gate-smoke` 都没有进入任何默认验证入口，且 `run_e2e.mjs` 里的 c-example/media/security smoke 主要靠源码字符串扫描，不能防止 CR-01/CR-03/CR-04 这类运行时断链。阶段目标是验收 harness，缺少自动入口会让端到端断链长期保持“构建通过”。

**Fix:** 增加明确的 smoke scripts，并在 CI/CTest 或阶段验证命令中至少运行 dry-run、C 示例 dry-run、media-file smoke 和 security-gate smoke；对 runId/offer/answer 路径增加一个本机信令集成测试。

```json
{
  "scripts": {
    "e2e:chrome:page-smoke": "node examples/chrome_e2e/run_e2e.mjs --page-smoke",
    "e2e:chrome:c-smoke": "node examples/chrome_e2e/run_e2e.mjs --c-example-smoke",
    "e2e:chrome:media-smoke": "node examples/chrome_e2e/run_e2e.mjs --media-file-smoke",
    "e2e:chrome:security-gate": "node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke"
  }
}
```

---

_Reviewed: 2026-05-11T06:56:49Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
