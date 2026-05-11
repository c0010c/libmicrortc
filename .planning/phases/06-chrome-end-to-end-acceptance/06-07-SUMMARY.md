---
phase: 06-chrome-end-to-end-acceptance
plan: 07
subsystem: chrome-e2e-signaling
tags: [chrome-e2e, websocket, signaling, runId, pure-c]

requires:
  - phase: 06-chrome-end-to-end-acceptance
    provides: "06-01..06-06 建立 Chrome 页面、WebSocket 信令服务、C 示例、媒体样本和 E2E 编排基础"
provides:
  - "Chrome 页面、run_e2e 编排脚本和 C 示例共享同一个 runId"
  - "C 示例支持 --run-id，并用 options->run_id 发送 hello/answer/candidate/status"
  - "C 示例 WebSocket text frame 读取可跨 EAGAIN/EWOULDBLOCK 和 TCP 分片保留进度"
affects: [06-08, 06-09, 06-10, EXM-02, ACC-01]

tech-stack:
  added: []
  patterns:
    - "示例层固定大小 WebSocket RX buffer，不引入动态分配或线程"
    - "E2E full run 由编排脚本生成共享 runId 并注入两端"

key-files:
  created:
    - ".planning/phases/06-chrome-end-to-end-acceptance/06-07-SUMMARY.md"
  modified:
    - "examples/chrome_e2e/page/app.js"
    - "examples/chrome_e2e/run_e2e.mjs"
    - "examples/chrome_e2e/rtc_chrome_e2e.c"
    - "examples/chrome_e2e/README.md"

key-decisions:
  - "runId 由 full E2E 编排脚本统一生成，页面从 URL query 读取，C 示例通过 --run-id 接收。"
  - "WebSocket reader 保持示例层固定内存状态机，只支持本阶段需要的 text frame、<126 和 126 扩展长度。"
  - "ACC-01 仍不标记完成；本计划只关闭信令互达和 WebSocket 分片 blocker。"

patterns-established:
  - "run_e2e.mjs full run 同时向浏览器 URL 和 C 示例 CLI 注入共享运行标识。"
  - "非阻塞 socket reader 在 EAGAIN/EWOULDBLOCK 时返回 0，并保留已读 frame 进度。"

requirements-completed: [EXM-02]

duration: 35min
completed: 2026-05-11
---

# Phase 06 Plan 07: Chrome E2E 信令互达修复 Summary

**共享 runId 和固定内存 WebSocket 分片读取状态机，让 Chrome 页面与 C 示例可以进入同一个信令 run 并可靠接收较大的 SDP text frame。**

## Performance

- **Duration:** 约 35 min
- **Started:** 2026-05-11T08:33:00Z
- **Completed:** 2026-05-11T09:08:15Z
- **Tasks:** 2/2
- **Files modified:** 5

## Accomplishments

- Chrome 页面初始化时通过 `new URLSearchParams(window.location.search)` 读取 `runId`，没有 query 时才回退到 `crypto.randomUUID()`。
- `run_e2e.mjs` full run 生成共享 `runId`，页面 URL 使用 `encodeURIComponent(runId)`，C 示例 spawn 参数追加 `--run-id`。
- `rtc_chrome_e2e` 增加 `e2e_options_t.run_id` 和 `--run-id VALUE`，删除 `E2E_RUN_ID` 直接依赖。
- C 示例 WebSocket reader 增加 `ws_rx_buffer[8192]`、`ws_rx_used`、`ws_rx_expected`、`ws_rx_header_len` 和 `ws_rx_payload_len`，支持 payload 长度 `<126` 和 `126`，在 `EAGAIN/EWOULDBLOCK` 时保留进度。

## Task Commits

1. **Task 1: 让页面、编排脚本和 C 示例共享 runId** - `36983ba` (`feat`)
2. **Task 2: 为 C 示例 WebSocket client 增加分片读取状态机** - `81d967b` (`fix`)

## Files Created/Modified

- `examples/chrome_e2e/page/app.js` - 页面从 URL query 读取共享 `runId`。
- `examples/chrome_e2e/run_e2e.mjs` - full run 生成并注入共享 `runId`。
- `examples/chrome_e2e/rtc_chrome_e2e.c` - C 示例支持 `--run-id`，并实现固定内存 WebSocket RX 状态机。
- `examples/chrome_e2e/README.md` - 说明 full run 的共享 `runId` 注入方式。
- `.planning/phases/06-chrome-end-to-end-acceptance/06-07-SUMMARY.md` - 本执行摘要。

## Verification

- `rg -n "new URLSearchParams\\(window\\.location\\.search\\)" examples/chrome_e2e/page/app.js`：PASS
- `rg -n "params\\.get\\(\"runId\"\\) \\?\\? crypto\\.randomUUID\\(\\)" examples/chrome_e2e/page/app.js`：PASS
- `rg -n "const char \\*run_id" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "\"--run-id\"" examples/chrome_e2e/rtc_chrome_e2e.c examples/chrome_e2e/run_e2e.mjs`：PASS
- `rg -n "options->run_id" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "encodeURIComponent\\(runId\\)" examples/chrome_e2e/run_e2e.mjs`：PASS
- `! rg -n "E2E_RUN_ID" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "ws_rx_buffer\\[8192\\]|ws_rx_used|ws_rx_payload_len" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "EAGAIN|EWOULDBLOCK" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "len == 126u|payload_len" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `! rg -n "recv\\([^\\n]+\\) != \\(ssize_t\\)len" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `cmake --build build --target rtc_chrome_e2e`：PASS
- `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke`：PASS
- `node examples/chrome_e2e/run_e2e.mjs --page-smoke`：PASS，`offerCreated:true`，`permissionRequests:0`

## Decisions Made

- 保留 `c-example` 作为 C 示例默认 `run_id`，但 full run 必须通过 `--run-id` 覆盖，避免页面和 C 示例分裂到不同信令 run。
- WebSocket reader 不扩展为通用 WebSocket client；本计划只支持服务端未 masked text frame、close 检测、binary 拒绝、`127` 长度拒绝和固定 8192 字节上限。

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: signaling_cli_input | `examples/chrome_e2e/rtc_chrome_e2e.c` | 新增 `--run-id` 信令标识输入；仅用于示例层 JSON run 隔离，不进入核心库，不承载媒体 payload。 |

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

`06-08` 可以基于同一信令 run 继续修复 C runtime 成功状态机、media executor 亲和和 full E2E pass 判定。`ACC-01` 仍待 06-08..06-10 关闭真实安全 backend、secure full E2E 和 VLC/ffplay 人工验收。

## Self-Check: PASSED

- `06-07-SUMMARY.md` 存在。
- 任务提交 `36983ba` 和 `81d967b` 均可在 git log 中找到。
- 最终验证命令 `cmake --build build --target rtc_chrome_e2e`、`node examples/chrome_e2e/run_e2e.mjs --c-example-smoke`、`node examples/chrome_e2e/run_e2e.mjs --page-smoke` 均通过。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
