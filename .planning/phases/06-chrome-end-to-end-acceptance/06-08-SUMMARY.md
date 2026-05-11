---
phase: 06-chrome-end-to-end-acceptance
plan: 08
subsystem: chrome-e2e-runtime
tags: [chrome-e2e, runtime-state, media-executor, jsonl, smoke]

requires:
  - phase: 06-chrome-end-to-end-acceptance
    provides: "06-07 让 Chrome 页面、run_e2e 和 C 示例共享 runId，并修复 C WebSocket 分片读取"
provides:
  - "C 示例 runtime 具备真实成功条件和分层 timeout summary"
  - "样本媒体发送遵守 RTC_EXECUTOR_MEDIA 亲和，并记录 RTP 发送失败"
  - "full E2E summary 不再被 manual-security-ok 或缺失媒体文件误判为通过"
affects: [06-09, 06-10, ACC-01]

tech-stack:
  added: []
  patterns:
    - "C 示例用固定字段追踪 E2E runtime 状态，不引入动态分配或线程"
    - "E2E 自动化 pass 由 C summary、进程退出码和非空媒体文件共同决定"

key-files:
  created:
    - ".planning/phases/06-chrome-end-to-end-acceptance/06-08-SUMMARY.md"
  modified:
    - "examples/chrome_e2e/rtc_chrome_e2e.c"
    - "examples/chrome_e2e/run_e2e.mjs"
    - "package.json"

key-decisions:
  - "C runtime success 以 offer、answer、ICE、SRTP 和音视频媒体文件均 ready 为准；timeout 按最早缺失层报告。"
  - "`rtc_peer_connection_send_media_frame` 只在 `RTC_EXECUTOR_MEDIA` 下调用；失败进入 `rtp/send_media_frame_failed`。"
  - "`--manual-security-ok` 只输出 `manual_security_override:true`，不能把 DTLS security gate 失败变成 `pass:true`。"
  - "`ACC-01` 仍不标记完成；真实可选安全 backend、secure full E2E 和 VLC/ffplay 人工验收仍属于 06-09/06-10。"

patterns-established:
  - "C 示例 summary 的 `pass:true` 路径必须在关闭 PeerConnection、媒体文件和 socket 后写出。"
  - "Node full E2E summary 必须同时检查 `failure.layer === \"none\"`、C 进程退出码、C summary 和指定媒体文件大小。"

requirements-completed: []

duration: 5min
completed: 2026-05-11
---

# Phase 06 Plan 08: Chrome E2E runtime 状态机与 pass 判定 Summary

**C 示例 runtime 现在按真实链路状态写成功或准确失败层，媒体发送遵守 media executor 亲和，full E2E 自动化不会把安全 gate 或空媒体文件误报为通过。**

## Performance

- **Duration:** 约 5 min
- **Started:** 2026-05-11T09:13:13Z
- **Completed:** 2026-05-11T09:17:55Z
- **Tasks:** 3/3
- **Files modified:** 4

## Accomplishments

- 在 `e2e_context_t` 中新增 offer、answer、ICE、DTLS、SRTP、RTP、RTCP、媒体文件和最后失败原因状态位。
- `run_runtime()` 在 `offer_received && answer_sent && ice_connected && srtp_ready && media_files_ready` 时清理资源、写 `pass:true/layer:none/e2e passed` 并返回 0。
- timeout summary 改为按最早缺失层报告 `signaling`、`ice`、`dtls`、`srtp` 或 `media_file`，不再无条件写 `signaling/no offer received`。
- `send_sample_frame()` 在 `RTC_EXECUTOR_MEDIA` 下调用 `rtc_peer_connection_send_media_frame`，保存并检查 `rtc_status_t status`；失败写 `rtp/send_media_frame_failed`，且不推进下一帧时间。
- `run_e2e.mjs` 的 full E2E `pass` 现在要求 failure layer 为 `none`、C 进程退出码为 0、C summary pass 为 true，且 `received-opus.packets` 与 `received-h264.264` 均存在且非空。
- `package.json` 新增 `e2e:chrome:page-smoke`、`e2e:chrome:c-smoke`、`e2e:chrome:media-smoke` 和 `e2e:chrome:security-gate` 显式入口。

## Task Commits

1. **Task 1: 增加 C runtime 状态位和准确退出条件** - `74eb5a5` (`fix`)
2. **Task 2: 修复媒体发送 executor 亲和并记录发送失败** - `74eb5a5` (`fix`)
3. **Task 3: 收紧 full E2E pass 判定和 smoke 覆盖** - `a3762d0` (`fix`)

Task 1 和 Task 2 修改同一个 C runtime 状态与错误记录路径，合并在一个提交中，避免把互相依赖的状态机和媒体失败记录拆成不可验证的中间状态。

## Files Created/Modified

- `examples/chrome_e2e/rtc_chrome_e2e.c` - 新增 runtime 状态机、passing summary、分层 timeout、统一清理和 media executor 发送。
- `examples/chrome_e2e/run_e2e.mjs` - 收紧 full E2E pass 判定，保留 `manual_security_override` 但不改变 `pass`。
- `package.json` - 新增四个 Chrome E2E smoke npm script。
- `.planning/phases/06-chrome-end-to-end-acceptance/06-08-SUMMARY.md` - 本执行摘要。

## Verification

- `rg -n "offer_received|answer_sent|ice_connected|srtp_ready|media_files_ready" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "rtc_e2e_jsonl_summary_media\\([^\\n]+1, \"none\", \"e2e passed\"" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `! rg -n "summary_media\\([^\\n]+0, \"signaling\", \"no offer received\"" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "rtc_executor_set_current_for_test\\(RTC_EXECUTOR_MEDIA\\)" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "rtc_status_t status" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "send_media_frame_failed" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `! rg -n "\\(void\\)rtc_peer_connection_send_media_frame" examples/chrome_e2e/rtc_chrome_e2e.c`：PASS
- `rg -n "received-opus\\.packets|received-h264\\.264" examples/chrome_e2e/run_e2e.mjs`：PASS
- `! rg -n "pass: pass \\|\\| \\(args\\.manualSecurityOk" examples/chrome_e2e/run_e2e.mjs`：PASS
- `rg -n "e2e:chrome:page-smoke|e2e:chrome:c-smoke|e2e:chrome:media-smoke|e2e:chrome:security-gate" package.json`：PASS
- `cmake --build build --target rtc_chrome_e2e`：PASS
- `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke`：PASS，summary 中音频/视频帧和字节数均大于 0。
- `node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke`：PASS，停在 `dtls/optional_security_backend_disabled` 且 `pass:false`。
- `npm run e2e:chrome:dry-run`：PASS，所有 smoke script 入口存在。

## Decisions Made

- `dtls_connected`、`rtp_seen` 和 `rtcp_seen` 作为 runtime 观测状态保留，但本计划的 passing 条件遵循 PLAN：offer、answer、ICE、SRTP 和媒体文件 ready 即通过。
- `manual-security-ok` 不删除，避免破坏现有 CLI；它只作为 `manual_security_override` 诊断字段输出，不能改变验收结果。
- 不更新 `ACC-01` 为完成，因为 06-09/06-10 仍需真实安全 backend、secure full E2E 和 VLC/ffplay 人工播放批准。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 合并 Task 1 和 Task 2 的 C runtime 提交**
- **Found during:** Task 2
- **Issue:** media send failure 需要写入同一套 `last_failure_layer`/`last_failure_reason` 状态；若按任务拆开会产生一个缺少 RTP failure 记录的中间 runtime 状态。
- **Fix:** 将 C runtime 状态机、timeout 分层和 media executor 发送修复合并在 `74eb5a5`，Task 3 仍单独提交。
- **Files modified:** `examples/chrome_e2e/rtc_chrome_e2e.c`
- **Verification:** `cmake --build build --target rtc_chrome_e2e`、`node examples/chrome_e2e/run_e2e.mjs --media-file-smoke`
- **Committed in:** `74eb5a5`

---

**Total deviations:** 1 auto-fixed (Rule 3)
**Impact on plan:** 只影响提交拆分方式，不改变行为范围；所有任务验收命令均通过。

## Known Stubs

None. 扫描到的 `=[]`、`=null`、`=""` 仅为 Node 函数默认参数或 stdout/stderr 累积初始值，不是 UI 或 runtime stub。

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

`06-09` 可以继续实现默认关闭的真实可选 OpenSSL/libsrtp DTLS/SRTP backend。`06-08` 已关闭 C runtime 无成功条件、media API executor 亲和和 full E2E pass 判定误报 blocker；`ACC-01` 仍需 06-09/06-10 后才能关闭。

## Self-Check: PASSED

- `06-08-SUMMARY.md` 存在。
- 任务提交 `74eb5a5` 和 `a3762d0` 均可在 git log 中找到。
- 最终验证命令 `cmake --build build --target rtc_chrome_e2e`、`node examples/chrome_e2e/run_e2e.mjs --media-file-smoke`、`node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke` 和 `npm run e2e:chrome:dry-run` 均通过。
- 未发现本计划提交包含意外删除文件。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
