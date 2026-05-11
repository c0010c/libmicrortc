---
phase: 06-chrome-end-to-end-acceptance
plan: 01
subsystem: examples
tags: [chrome-e2e, websocket, playwright, synthetic-media, diagnostics]
requires:
  - phase: 05-rtp-rtcp-media-plane
    provides: typed media/RTP/RTCP public API and Chrome E2E acceptance boundary
provides:
  - 本机 Chrome E2E 页面、WebSocket 信令服务和 smoke 编排入口
  - JSON-only SDP/candidate/status/summary 信令协议边界
  - canvas/Web Audio 合成媒体页面与七阶段 diagnostics DOM
affects: [phase-06, examples, e2e, acceptance]
tech-stack:
  added: [ws, "@playwright/test"]
  patterns:
    - Node HTTP + WebSocket 服务同目录提供静态页面
    - 原生 HTML/CSS/JS 页面通过稳定 data-testid 暴露验收信号
    - Playwright smoke 拦截 getUserMedia 以验证无摄像头/麦克风权限请求
key-files:
  created:
    - .gitignore
    - examples/chrome_e2e/README.md
    - examples/chrome_e2e/signaling.mjs
    - examples/chrome_e2e/run_e2e.mjs
    - examples/chrome_e2e/page/index.html
    - examples/chrome_e2e/page/styles.css
    - examples/chrome_e2e/page/app.js
  modified:
    - package.json
    - package-lock.json
key-decisions:
  - "信令服务只转发同一 runId 内的 JSON SDP/candidate/status/summary/error，不接受二进制或媒体字段。"
  - "Chrome 页面只使用 canvas captureStream(25) 与 Web Audio oscillator 合成媒体，不调用 getUserMedia。"
  - "页面 smoke 使用 Playwright 启动真实 Chromium，并通过 data-testid 与 window.__chromeE2E 检查 offer 生成。"
patterns-established:
  - "E2E 页面首屏即为诊断工具：双视频、七阶段状态条和 compact diagnostics 同屏呈现。"
  - "run_e2e.mjs 先提供 dry-run/page-smoke，后续计划可在同一入口扩展 C 示例和 full E2E。"
requirements-completed: [EXM-01, EXM-02]
duration: 54min
completed: 2026-05-11
---

# Phase 6 Plan 01: Chrome E2E Harness Summary

**本地 Chrome 合成媒体验收页面、JSON-only WebSocket 信令服务和 Playwright smoke 入口已建立。**

## Performance

- **Duration:** 54min
- **Started:** 2026-05-11T05:10:53Z
- **Completed:** 2026-05-11T06:04:41Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- 新增 `examples/chrome_e2e/` harness：`signaling.mjs` 同时提供静态页面和 `/ws` WebSocket 信令。
- 信令协议固定 `allowedTypes`，拒绝非 JSON、未知 `type`、二进制帧，以及 `media`、`payload`、`binary` 字段。
- Chrome 页面第一屏直接展示双视频、七阶段状态条和 compact diagnostics，所有计划要求的 `data-testid` 均存在。
- 页面使用 canvas `captureStream(25)` 与 `AudioContext.createMediaStreamDestination()` 生成合成音视频，Playwright smoke 验证未调用 `getUserMedia`。
- `run_e2e.mjs` 支持 `--dry-run` 和 `--page-smoke`，并输出 `summary.layer: "none"` 的 JSON 成功摘要。

## Task Commits

1. **Task 1: 新增信令服务、消息协议和 dry-run 检查** - `faf6aef` (`feat`)
2. **Task 2: 实现 Chrome 合成媒体页面和 compact diagnostics** - `721f16d` (`feat`)

## Files Created/Modified

- `package.json` - 新增 `e2e:chrome` / `e2e:chrome:dry-run` scripts 和 `ws`、`@playwright/test` devDependencies。
- `package-lock.json` - 锁定新增 Node 依赖版本。
- `.gitignore` - 忽略本地验证产生的 `.cache/` 运行时目录。
- `examples/chrome_e2e/README.md` - 中文说明信令边界、运行入口和新增依赖许可证。
- `examples/chrome_e2e/signaling.mjs` - HTTP 静态页面服务与 JSON-only WebSocket 信令服务。
- `examples/chrome_e2e/run_e2e.mjs` - dry-run 与 Playwright page-smoke 编排入口。
- `examples/chrome_e2e/page/index.html` - 双视频、七阶段状态条、diagnostics 和操作按钮 DOM。
- `examples/chrome_e2e/page/styles.css` - UI-SPEC 约束的颜色、字号、间距和响应式布局。
- `examples/chrome_e2e/page/app.js` - 合成媒体、RTCPeerConnection offer/candidate、WebSocket 和 stats 更新逻辑。

## Decisions Made

- 使用 `runId` 隔离同一信令服务内的广播范围，避免不同验收运行串线。
- `--page-smoke` 只验证页面合成媒体和 offer 生成，不等待 C 示例 answer；C 示例互通留给后续 06-02..06-05。
- 为当前无系统 Chrome 的环境下载 Playwright Chromium；仓库内临时运行库放在 `.cache/` 并被 `.gitignore` 忽略。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 为 page-smoke 处理本地 Chromium 运行环境缺口**
- **Found during:** Task 2
- **Issue:** 当前环境没有系统 Chrome；Playwright Chromium 主包下载后还缺少 `libnspr4`、`libnss3`、`libasound`，导致浏览器无法启动。
- **Fix:** 下载 Playwright Chromium，并在 `run_e2e.mjs` 中允许使用已下载 Chromium executable；本地解压的运行库通过 `.cache/` 提供并被 `.gitignore` 忽略。
- **Files modified:** `.gitignore`, `examples/chrome_e2e/run_e2e.mjs`
- **Verification:** `node examples/chrome_e2e/run_e2e.mjs --page-smoke`
- **Committed in:** `721f16d`

---

**Total deviations:** 1 auto-fixed (Rule 3 blocking)
**Impact on plan:** 仅用于让计划要求的真实浏览器 smoke 在当前环境可运行；没有改变核心库边界或信令/媒体协议范围。

## Issues Encountered

- `npm audit --audit-level=moderate` 报告既有 GSD 工具链依赖 `@anthropic-ai/sdk` 的 moderate vulnerability；已记录到 `deferred-items.md`，未在本计划内升级工具链。

## Verification

- `node examples/chrome_e2e/run_e2e.mjs --dry-run`：通过，`summary.layer` 为 `none`。
- `node examples/chrome_e2e/run_e2e.mjs --page-smoke`：通过，`offerCreated: true`，`permissionRequests: 0`。
- `grep` 验收项：`allowedTypes`、`offer`、`answer`、`candidate`、`payload`、页面 `data-testid`、按钮 copy、`captureStream(25)`、`createMediaStreamDestination`、`getStats`、`#0B6B5E` 均通过。

## Known Stubs

None - 没有阻止本计划目标达成的 stub。Task 2 的远端媒体显示会等待后续 C 示例 answer/track，这是 06-02..06-05 的计划范围。

## Auth Gates

None.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: websocket-signaling | `examples/chrome_e2e/signaling.mjs` | 新增本地 WebSocket 信令入口；已按 T-06-01 限制为 JSON SDP/candidate/status/summary/error，拒绝媒体字段和二进制。 |
| threat_flag: browser-media | `examples/chrome_e2e/page/app.js` | 新增浏览器媒体源；已按 T-06-02 使用 canvas/Web Audio 合成媒体，不调用摄像头或麦克风。 |

## User Setup Required

None - no external service configuration required. 首次运行 `--page-smoke` 可能需要 Playwright 下载 Chromium；当前执行环境已完成主 Chromium 下载。

## Next Phase Readiness

06-02 可以复用 `allowedTypes`、`runId`、`offer`/`answer`/`candidate` 消息形状和 `run_e2e.mjs` 入口，接入 C 示例 WebSocket client、UDP socket pump 和 JSONL summary。

## Self-Check: PASSED

- 文件存在性检查通过：`06-01-SUMMARY.md`、`deferred-items.md`、`signaling.mjs`、`page/app.js` 均存在。
- 提交存在性检查通过：`faf6aef`、`721f16d` 均可在 git log 中找到。
- 自动化复验通过：`node examples/chrome_e2e/run_e2e.mjs --dry-run` 和 `node examples/chrome_e2e/run_e2e.mjs --page-smoke` 均成功。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
