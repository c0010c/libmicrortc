---
phase: 06-chrome-e2e
plan: "01"
subsystem: testing
tags: [playwright, chrome, websocket, e2e, cmake]
requires:
  - phase: 05-h264-opus
    provides: H264/Opus 编码后媒体路径、fixture verifier 和 public transceiver API
provides:
  - tests/e2e 下固定的 Node/Playwright E2E 工具骨架
  - 本地 WebSocket signaling server 与 C answerer stdio JSON-line bridge skeleton
  - examples/chrome-e2e 下最小 Chrome 页面与 browser-client 状态 API
  - 不污染默认 CTest 的 CMake E2E 示例入口
affects: [06-chrome-e2e, e2e, examples, tests]
tech-stack:
  added: [@playwright/test, ws]
  patterns: [Node runner owns browser orchestration, C demo stdio JSON signaling, browser state exposed via window.__mrtcE2E]
key-files:
  created:
    - tests/e2e/package.json
    - tests/e2e/package-lock.json
    - tests/e2e/playwright.config.js
    - tests/e2e/signaling-server.js
    - tests/e2e/run-host-e2e.js
    - examples/chrome-e2e/index.html
    - examples/chrome-e2e/browser-client.js
  modified:
    - .gitignore
    - CMakeLists.txt
key-decisions:
  - "06-01: Playwright/Node 依赖只固定在 tests/e2e，根目录不创建 package.json。"
  - "06-01: WebSocket signaling server 只存在于 tests/e2e，C answerer 通过 stdio JSON line 接入，核心库不新增 signaling 依赖。"
  - "06-01: 默认 CTest 不注册 Chrome/Playwright E2E；浏览器验收保持显式 npm 命令。"
patterns-established:
  - "E2E message schema 固定为 hello/offer/answer/candidate/event/done/error。"
  - "所有 signaling 日志通过 redaction helper 屏蔽 credential/password/username 和 TURN URL。"
requirements-completed: [E2E-01, E2E-02, E2E-03, E2E-04, TEST-03, TEST-04]
duration: 11min
completed: 2026-05-13
---

# Phase 6 Plan 01: E2E Tooling 与 Signaling Skeleton Summary

**Node/Playwright Chrome E2E 骨架、本地 WebSocket signaling bridge 和最小浏览器 offer 页面已建立，且默认 CTest 仍保持 browser-free。**

## Performance

- **Duration:** 11 min
- **Started:** 2026-05-13T09:21:11Z
- **Completed:** 2026-05-13T09:32:28Z
- **Tasks:** 4
- **Files modified:** 9

## Accomplishments

- 在 `tests/e2e/` 内固定 `@playwright/test` 与 `ws` 依赖，配置单一 `chrome-host` 项目，报告输出到 `tests/e2e/artifacts/`。
- 实现 `tests/e2e/signaling-server.js` 和 `tests/e2e/run-host-e2e.js`，支持本地 WebSocket、C answerer stdio JSON-line bridge、结构化事件和 secret redaction。
- 创建 `examples/chrome-e2e/index.html` 与 `browser-client.js`，浏览器端会创建 DataChannel、音视频 transceiver、发送 offer、处理 answer/candidate，并暴露 `window.__mrtcE2E`。
- 在 CMake 中增加 `MRTC_BUILD_EXAMPLES` 与 future `mrtc_chrome_answerer` 条件化注册，同时不把 Chrome E2E 加入默认 `ctest`。

## Task Commits

1. **Task 06-01-01: 创建 tests/e2e Playwright 项目与 Chrome 配置** - `ea09e31` (feat)
2. **Task 06-01-02: 实现本地 signaling server 和 C stdio bridge skeleton** - `198ac1c` (feat)
3. **Task 06-01-03: 创建最小 Chrome 页面和 browser-client signaling API** - `554fff3` (feat)
4. **Task 06-01-04: 注册 E2E skeleton 的构建入口但不污染默认 CTest** - `25e483b` (feat)

## Files Created/Modified

- `.gitignore` - 忽略 `tests/e2e/node_modules/` 与 `tests/e2e/artifacts/` 生成物。
- `tests/e2e/package.json` - Playwright/WS 依赖和 `test:host`、`test:turn`、`install:browsers` 脚本。
- `tests/e2e/package-lock.json` - 固定 Node 测试依赖解析结果。
- `tests/e2e/playwright.config.js` - 单一 `chrome-host` 项目、Chrome/channel 环境变量、CI worker 和 artifacts 配置。
- `tests/e2e/signaling-server.js` - WebSocket signaling server、answerer stdio bridge、schema normalization 和 redaction self-test。
- `tests/e2e/run-host-e2e.js` - Host E2E runner CLI、dry-run、summary artifact 和后续 Playwright 接入点。
- `examples/chrome-e2e/index.html` - 最小 audio/video 测试页面。
- `examples/chrome-e2e/browser-client.js` - Chrome offer、DataChannel、candidate forwarding、stats/state bridge。
- `CMakeLists.txt` - `MRTC_BUILD_EXAMPLES` option、future `mrtc_chrome_answerer` 条件化注册和显式 E2E help target。

## Decisions Made

- Playwright 默认使用 `MRTC_E2E_BROWSER_CHANNEL=chrome` 对应的 Chrome channel；设置为 `chromium` 时回退到 Playwright bundled Chromium。
- `test:host -- --list` 在 skeleton 阶段使用 Playwright `--pass-with-no-tests`，为后续计划添加真实 specs 留出入口。
- `mrtc_chrome_answerer` 仅在未来 C source 出现且 `MRTC_BUILD_TESTS=ON` 或 `MRTC_BUILD_EXAMPLES=ON` 时注册，避免当前骨架构建失败。

## Verification

- `npm --prefix tests/e2e install` - passed，5 个 npm 包 audit 无漏洞。
- `npm --prefix tests/e2e run test:host -- --list` - passed，当前 skeleton 列出 0 tests。
- `node tests/e2e/run-host-e2e.js --help` - passed，输出 stages 和 message types。
- `node tests/e2e/signaling-server.js --help` - passed during Task 06-01-02 verification。
- `node tests/e2e/signaling-server.js --self-test-redaction` - passed，样例 credential/password/username/TURN URL 被屏蔽。
- `node tests/e2e/run-host-e2e.js --page examples/chrome-e2e/index.html --dry-run` - passed，写入 runner summary artifact。
- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure` - passed，18/18 CTest 通过；默认 CTest 未运行 Node、Playwright 或 Chrome。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 忽略 E2E 生成物**
- **Found during:** Task 06-01-01
- **Issue:** `npm install` 与 Playwright `--list` 生成 `tests/e2e/node_modules/` 和 `tests/e2e/artifacts/`，执行协议要求不能留下本任务生成的未跟踪文件。
- **Fix:** 在 `.gitignore` 中加入对应忽略规则。
- **Files modified:** `.gitignore`
- **Verification:** `git status --short` 不再显示这些由本计划生成的目录。
- **Committed in:** `ea09e31`

---

**Total deviations:** 1 auto-fixed (Rule 3)
**Impact on plan:** 只处理本任务生成物，不改变核心库或 E2E 行为边界。

## Known Stubs

None - 已扫描本计划创建/修改文件中的 TODO/FIXME/placeholder 和 UI 空数据占位；`null`/空对象仅用于运行时状态初始化，不阻塞计划目标。

## Threat Flags

None - 新增 WebSocket signaling surface 已在计划 threat model 中覆盖，且只位于 `tests/e2e/`；核心 `micrortc` target 未新增 Node/WebSocket/signaling 依赖。

## Issues Encountered

- CMake configure 报告系统未安装 libsrtp/usrsctp，但当前 `MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=OFF`，这是既有可选依赖模式；构建和 18 个 CTest 均通过。
- `build/`、`reflib/`、`.idea/` 在执行前已为未跟踪目录，未清理、未重置、未纳入提交。

## User Setup Required

None - 本计划只建立 host E2E skeleton；真实 Chrome browser install 可通过后续显式命令 `npm --prefix tests/e2e run install:browsers` 执行。

## Next Phase Readiness

Plan 06-02 可以在现有 `examples/chrome-e2e/` 和 `tests/e2e/` 骨架上添加真实 `mrtc_chrome_answerer.c`，使用已固定的 JSON message schema 与 runner bridge 接入 DataChannel、固定 H264/Opus 媒体源和 C 侧结构化事件。

## Self-Check: PASSED

- 关键创建/修改文件均存在。
- 任务提交 `ea09e31`、`198ac1c`、`554fff3`、`25e483b` 均可在 git log 中找到。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-13*
