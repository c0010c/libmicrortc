---
phase: 06-chrome-end-to-end-acceptance
plan: 05
subsystem: examples
tags: [chrome-e2e, orchestration, playwright, jsonl, diagnostics, failure-layers]
requires:
  - phase: 06-chrome-end-to-end-acceptance
    plan: 01
    provides: 本机 Chrome 页面、WebSocket 信令服务和 Playwright smoke 入口
  - phase: 06-chrome-end-to-end-acceptance
    plan: 02
    provides: C 示例运行时、WebSocket/UDP 示例层 I/O 和 JSONL observer
  - phase: 06-chrome-end-to-end-acceptance
    plan: 03
    provides: 样本解析、媒体发送、接收落盘和 media-file smoke
  - phase: 06-chrome-end-to-end-acceptance
    plan: 04
    provides: 默认关闭的可选 Chrome DTLS/SRTP backend gate 和安全失败分层
provides:
  - full E2E 编排入口，统一启动信令服务、C 示例和 Chrome 页面
  - 限定为 signaling/ice/dtls/srtp/rtp/rtcp/media_file 的失败层级判定
  - compact JSON summary，包含页面状态、C JSONL、媒体文件、latestEvents 和 VLC 人工 gate
affects: [phase-06, examples, e2e, acceptance, diagnostics]
tech-stack:
  added: []
  patterns:
    - Node 编排脚本直接聚合 Playwright 页面状态、C JSONL summary 和进程退出码
    - 默认可选安全 backend OFF 时 full run 以 dtls/optional_security_backend_disabled 明确失败
    - 自动化保持 manual_vlc_required=true，不把媒体文件存在性等同于 VLC 播放验收
key-files:
  created:
    - .planning/phases/06-chrome-end-to-end-acceptance/06-05-SUMMARY.md
  modified:
    - examples/chrome_e2e/run_e2e.mjs
    - examples/chrome_e2e/page/app.js
    - examples/chrome_e2e/README.md
key-decisions:
  - "failureLayers 固定为 signaling/ice/dtls/srtp/rtp/rtcp/media_file，未知失败不会扩展一级分类。"
  - "默认安全 backend gate 被视为 dtls 层失败；只有 --manual-security-ok 可让开发 smoke 以 0 退出。"
  - "manual_vlc_required 在自动化 summary 中保持 true，页面成功但未人工确认时显示 manual_vlc_pending。"
patterns-established:
  - "full run 输出单行 compact JSON，便于脚本和 CI 解析。"
  - "页面和 C JSONL 均只保留最近 5 条关键事件用于快速定位。"
requirements-completed: [TST-01, ACC-01]
duration: 8min
completed: 2026-05-11
---

# Phase 6 Plan 05: 端到端自动化编排和失败分层 Summary

**Chrome E2E 自动化现在能统一编排信令、C 示例和页面，并用固定失败层级输出可解析诊断 summary，同时保留 VLC 人工验收 gate。**

## Performance

- **Duration:** 8min
- **Started:** 2026-05-11T06:36:48Z
- **Completed:** 2026-05-11T06:44:24Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- `run_e2e.mjs` 默认 full run 会启动本机信令服务、`build/rtc_chrome_e2e` 和 Playwright Chrome 页面，点击 `Start Call`，读取 C JSONL 并输出 compact JSON。
- 失败层级固定为 `signaling`、`ice`、`dtls`、`srtp`、`rtp`、`rtcp`、`media_file`；当前默认 OFF 安全 backend 被清晰报告为 `dtls/optional_security_backend_disabled`。
- summary 包含 `pass`、`layer`、`reason`、`duration_ms`、`page`、`c_example`、`media_files`、`manual_vlc_required`、`manual_vlc_state` 和最近 5 条 `latestEvents`。
- 页面 `window.__chromeE2E` 暴露 `latestEvents`，`summary-layer` 在自动化成功但 VLC 未确认时显示 `manual_vlc_pending`。
- README 新增 full run 用法、`--manual-security-ok` 限制说明和七层失败排查表，明确自动化不替代 VLC 人工媒体播放验收。

## Task Commits

1. **Task 1: 实现 full E2E orchestration 和分层 summary 判定** - `534aed7` (`feat`)
2. **Task 2: 页面状态断言、JSONL 日志解析和自动化报告** - `1ff6195` (`feat`)

## Files Created/Modified

- `examples/chrome_e2e/run_e2e.mjs` - 新增 full E2E 默认路径、CLI 参数、JSONL 解析、失败层级选择、输出目录旧产物清理和 compact JSON summary。
- `examples/chrome_e2e/page/app.js` - 页面暴露 `latestEvents`，summary 默认带 `manual_vlc_required`，成功但未人工确认时显示 `manual_vlc_pending`。
- `examples/chrome_e2e/README.md` - 中文记录 full run、manual security smoke、VLC gate 和 failure layer 排查表。
- `.planning/phases/06-chrome-end-to-end-acceptance/06-05-SUMMARY.md` - 本计划执行总结。

## Decisions Made

- 使用同一 Node 进程内的 `createSignalingServer()` 启动信令服务，仍复用 `signaling.mjs` 的 HTTP/WebSocket 实现，退出时统一关闭浏览器与服务。
- full run 在启动前只移除本输出目录下本轮相关的 `rtc_chrome_e2e.jsonl`、`received-opus.packets` 和 `received-h264.264`，避免 stale 文件污染 summary，不清理其他工作树文件。
- `--manual-security-ok` 只把默认安全 backend gate 视为开发 smoke 通过；summary 仍保留 `layer:"dtls"` 和 `manual_vlc_required:true`。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 避免 stale media files 让失败 summary 误报媒体产物**
- **Found during:** Task 1
- **Issue:** 默认输出目录已有前序 smoke 产生的 `received-*` 文件；full run 在 `dtls/optional_security_backend_disabled` 早退时若直接扫描目录，会把旧文件误列入 `media_files`。
- **Fix:** full run 启动前只删除本轮会生成的 JSONL 和媒体输出文件，随后再执行编排和 summary 统计。
- **Files modified:** `examples/chrome_e2e/run_e2e.mjs`
- **Verification:** `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000` 输出 `media_files: []` 且 `layer:"dtls"`。
- **Committed in:** `534aed7`

---

**Total deviations:** 1 auto-fixed（Rule 1 bug）
**Impact on plan:** 修复防止自动化报告误导，没有扩大核心库、信令协议或媒体验收范围。

## Issues Encountered

- 当前默认构建没有启用真实 Chrome DTLS/SRTP backend，full run 按计划以非零退出并报告 `dtls/optional_security_backend_disabled`。这是 06-04 建立的 manual gate，不是本计划失败。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：通过，1/1 tests passed。
- `node examples/chrome_e2e/run_e2e.mjs --dry-run`：通过，文件和 npm script preflight 成功。
- `node examples/chrome_e2e/run_e2e.mjs --page-smoke`：通过，`offerCreated:true` 且 `permissionRequests:0`。
- `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000`：按当前默认安全 gate 非零退出，compact JSON 包含 `layer:"dtls"`、`reason:"optional_security_backend_disabled"`、`manual_vlc_required:true`。
- `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000 --manual-security-ok`：通过，用于开发 smoke；仍不声明真实媒体或 VLC 验收完成。
- grep 验收项：`failureLayers`、七层 literal、`--timeout-ms`、`--chrome-channel`、`--manual-security-ok`、`offer.received`、`answer.sent`、`ice.connected`、`srtp.ready`、`manual_vlc_required`、`manual_vlc_pending`、`duration_ms`、`media_files`、`latestEvents` 均通过。

## Known Stubs

None - 未发现阻止本计划目标达成的 stub。`manual_vlc_pending` 是计划要求的人工验收 gate 状态，不是自动化缺口。

## Auth Gates

None.

## Threat Flags

None - 本计划新增的是本地编排和报告聚合逻辑，已被计划 threat model `T-06-13`、`T-06-14`、`T-06-15` 覆盖。

## User Setup Required

真实 Chrome DTLS/SRTP full E2E 仍需要用户显式启用并配置 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 对应的宽松许可安全 backend。默认路径会停在清晰的 `dtls/optional_security_backend_disabled` gate。

## Next Phase Readiness

06-06 可以基于当前 compact JSON summary、README failure layer 表和 `manual_vlc_required:true` 收口 UAT、需求追踪和人工 VLC 验收说明。

## Self-Check: PASSED

- 文件存在性检查通过：`06-05-SUMMARY.md`、`run_e2e.mjs`、`page/app.js`、`README.md` 均存在。
- 提交存在性检查通过：`534aed7`、`1ff6195` 均可在 git log 中找到。
- 自动化复验通过：构建/CTest、`--dry-run`、`--page-smoke` 均成功；full run 正确报告 optional-security manual gate。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
