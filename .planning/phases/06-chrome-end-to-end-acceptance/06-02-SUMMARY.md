---
phase: 06-chrome-end-to-end-acceptance
plan: 02
subsystem: examples
tags: [chrome-e2e, c-example, websocket, udp, jsonl, observer]
requires:
  - phase: 06-chrome-end-to-end-acceptance
    plan: 01
    provides: 本机 Chrome 页面、WebSocket 信令服务和 smoke 编排入口
provides:
  - C 侧 `rtc_chrome_e2e` 示例进程 target
  - 示例层 WebSocket client、UDP socket pump 和 JSONL observer 输出
  - `run_e2e.mjs --c-example-smoke` smoke 验证
affects: [phase-06, examples, e2e, acceptance]
tech-stack:
  added: []
  patterns:
    - POSIX socket/WebSocket 仅位于 `examples/chrome_e2e/`
    - `observer.on_datagram` 立即 `memcpy` 到示例层发送队列
    - C 示例 JSONL 输出 process/status/error/trace/candidate/datagram/summary
key-files:
  created:
    - examples/chrome_e2e/rtc_chrome_e2e.c
    - examples/chrome_e2e/jsonl.c
    - examples/chrome_e2e/jsonl.h
  modified:
    - .gitignore
    - CMakeLists.txt
    - examples/chrome_e2e/run_e2e.mjs
    - examples/chrome_e2e/README.md
key-decisions:
  - "C 示例的 socket、WebSocket、JSONL 和发送队列全部保留在 examples/chrome_e2e/ 示例层，核心 src/ 不新增 socket/thread 责任。"
  - "WebSocket client 只支持本阶段本机 ws://host:port/ws、text frame 和 JSON 信令，不实现通用 WebSocket/HTTP 客户端。"
  - "在当前库尚无生产态 executor context public API 前，C 示例 target 私有包含 src/ 并使用既有内部 executor context helper 标注 signaling/network 调用亲和。"
patterns-established:
  - "C 示例 dry-run 会校验样本文件、创建输出目录并写入 JSONL summary。"
  - "`run_e2e.mjs --c-example-smoke` 同时验证 C dry-run JSONL 和源码 public API 串联标记。"
requirements-completed: [EXM-02]
duration: 7min
completed: 2026-05-11
---

# Phase 6 Plan 02: C 示例运行时与 JSONL Observer Summary

**C 侧 Chrome E2E 示例 target、dry-run、WebSocket/UDP 示例层 I/O 骨架和 JSONL observer 已建立。**

## Performance

- **Duration:** 7min
- **Started:** 2026-05-11T06:09:30Z
- **Completed:** 2026-05-11T06:16:29Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments

- 在 `RTC_BUILD_EXAMPLES` 中新增 `rtc_chrome_e2e` target，链接现有 `rtc` 静态库。
- 新增 `rtc_chrome_e2e.c`，支持 `--ws-url`、`--local-ip`、`--udp-port`、`--output-dir`、`--timeout-ms`、`--sample-opus`、`--sample-h264` 和 `--dry-run`。
- 新增 `jsonl.c/.h`，输出单行 JSONL event 与 `summary`，不输出媒体 payload。
- dry-run 会创建输出目录、校验 `sample1.opus` / `test-25fps.h264`，并写入 `process.started` 与通过 summary。
- C 示例层实现本机 WebSocket handshake/text frame、UDP socket bind/pump、远端 candidate 地址解析、`observer.on_datagram` 复制发送队列和 `sendto` flush。
- C 示例源码串联 `rtc_peer_connection_set_remote_description`、`create_answer`、`set_local_description`、`add_ice_candidate`、`gather_candidates`、`start_connectivity_checks` 和 `receive_datagram`。
- `run_e2e.mjs --c-example-smoke` 会启动 C dry-run，解析 JSONL，并检查源码 public API 串联与 datagram 复制防线。

## Task Commits

1. **Task 1: 新增 `rtc_chrome_e2e` 示例 target 和 CLI/runtime 骨架** - `ed8807e` (`feat`)
2. **Task 2: 接入 WebSocket 信令、UDP pump 和 PeerConnection observer** - `1c72ff4` (`feat`)
3. **Deviation fix: executor context 亲和修复** - `ad77093` (`fix`)

## Files Created/Modified

- `.gitignore` - 忽略 `examples/chrome_e2e/out/` dry-run 运行产物。
- `CMakeLists.txt` - 新增 `rtc_chrome_e2e` 示例 target，并为示例私有暴露 `src/` 以使用现有 executor context helper。
- `examples/chrome_e2e/rtc_chrome_e2e.c` - C 示例运行时、CLI、WebSocket client、UDP pump、observer 和 PeerConnection API 串联。
- `examples/chrome_e2e/jsonl.c` - JSON string escape、event 和 summary JSONL writer。
- `examples/chrome_e2e/jsonl.h` - JSONL writer public header。
- `examples/chrome_e2e/run_e2e.mjs` - 新增 `--c-example-smoke`。
- `examples/chrome_e2e/README.md` - 中文记录 C 示例边界、运行方式和后续互通范围。

## Decisions Made

- C 示例只实现本阶段需要的最小 WebSocket client：本机 `ws://host:port/ws`、HTTP upgrade、text frame 收发；不引入新的 C 依赖。
- Chrome candidate 对象在 C 示例中提取内层 `candidate` 字符串；C 侧本地 candidate 回发为 `RTCIceCandidateInit` 形状，便于页面调用 `addIceCandidate`。
- `observer.on_datagram` 使用局部 `copy` 和示例层 ring queue 双重复制路径，确保不在回调后借用核心库 buffer。
- 由于当前库的 executor 当前值仍由内部 helper 表达，示例 target 私有包含 `src/` 并在 public API 调用前切换 signaling/network context；这不改变核心 public API，也不把 socket/thread 放入核心库。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 重新配置 CMake 以发现新增示例 target**
- **Found during:** Task 1
- **Issue:** 现有 `build/` 未包含新 target，直接 `cmake --build build --target rtc_chrome_e2e` 报 “No rule to make target”。
- **Fix:** 执行 `cmake -S . -B build` 后再构建。
- **Files modified:** 无。
- **Verification:** `cmake --build build --target rtc_chrome_e2e`
- **Committed in:** N/A

**2. [Rule 1 - Bug] 修正 C 示例 WebSocket/JSON 运行期细节**
- **Found during:** Task 2
- **Issue:** answer SDP 需要 JSON 转义；Chrome 发送的 candidate 是对象而不是直接字符串；`usleep` / `strtok_r` 在当前 C99 配置下产生隐式声明警告。
- **Fix:** 新增 JSON string escape、candidate 内层字段提取、候选地址解析，并改用 C99 可用实现消除编译警告。
- **Files modified:** `examples/chrome_e2e/rtc_chrome_e2e.c`
- **Verification:** `cmake --build build --target rtc_chrome_e2e`
- **Committed in:** `1c72ff4`

**3. [Rule 3 - Blocking] 为示例运行期显式设置 executor context**
- **Found during:** Task 2 final verification
- **Issue:** 当前库的 executor 亲和检查依赖内部当前 executor 值；非 dry-run 收到 offer 后调用 network API 会被亲和拒绝。
- **Fix:** 示例 target 私有包含 `src/`，并使用既有 `rtc_executor_set_current_for_test` 在 signaling/network public API 调用前切换上下文。
- **Files modified:** `CMakeLists.txt`, `examples/chrome_e2e/rtc_chrome_e2e.c`
- **Verification:** `cmake --build build --target rtc_chrome_e2e`; `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke`
- **Committed in:** `ad77093`

---

**Total deviations:** 3 auto-fixed
**Impact on plan:** 变更均限定在示例层或构建发现流程；核心库仍未新增 socket、线程、WebSocket 或运行期依赖。

## Verification

- `cmake -S . -B build && cmake --build build --target rtc_chrome_e2e`：通过。
- `build/rtc_chrome_e2e --dry-run --output-dir examples/chrome_e2e/out`：通过，JSONL 包含 `process.started` 与 `summary.pass=true`。
- `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke`：通过，输出 `ok: true`。
- `grep` 验收项：`--ws-url`、`--udp-port`、`--sample-opus`、`--sample-h264`、`rtc_e2e_jsonl_event`、`summary`、所有计划列出的 PeerConnection public API、`on_datagram`、`memcpy`、`signaling.connected` 均通过。

## Known Stubs

None - 没有阻止本计划目标达成的 stub。完整 Chrome 媒体互通仍依赖后续 `06-03` 样本解析、`06-04` 真实 DTLS/SRTP backend gate 和 `06-05` full E2E 编排，这是已规划范围。

## Auth Gates

None.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: websocket-client | `examples/chrome_e2e/rtc_chrome_e2e.c` | 新增示例层 WebSocket client；仅支持本机 `ws://host:port/ws`、text frame 和阶段 JSON 信令，未进入核心库。 |
| threat_flag: udp-socket | `examples/chrome_e2e/rtc_chrome_e2e.c` | 新增示例层 UDP socket pump；核心库仍只通过 `receive_datagram` 和 `observer.on_datagram` 与用户 I/O 交互。 |

## User Setup Required

None - no external service configuration required for the smoke path. 非 dry-run 仍需要先启动 `examples/chrome_e2e/signaling.mjs` 和 Chrome 页面。

## Next Phase Readiness

06-03 可以在当前 C 示例中接入 `sample1.opus` Ogg packet 提取、`test-25fps.h264` Annex B access unit 切分、按节奏发送和接收媒体落盘。06-04 仍需处理真实 Chrome 互通所需的安全 backend gate。

## Self-Check: PASSED

- 文件存在性检查通过：`CMakeLists.txt`、`rtc_chrome_e2e.c`、`jsonl.c`、`jsonl.h`、`run_e2e.mjs`、`README.md` 均存在。
- 提交存在性检查通过：`ed8807e`、`1c72ff4`、`ad77093` 均可在 git log 中找到。
- 自动化复验通过：`cmake --build build --target rtc_chrome_e2e` 和 `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke` 均成功。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
