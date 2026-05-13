---
phase: 06-chrome-e2e
plan: "02"
subsystem: testing
tags: [chrome, e2e, c-demo, datachannel, h264, opus, jsonl]
requires:
  - phase: 05-h264-opus
    provides: H264/Opus 编码后媒体 API、固定 fixture、RTP/RTCP 媒体发送与 on_frame 回调路径
  - phase: 06-chrome-e2e
    provides: 06-01 建立的 Node/Playwright signaling skeleton 和 browser JSON message schema
provides:
  - C answerer demo 的 stdin/stdout JSON-line offer/answer/candidate 协议
  - DataChannel open/message/close 结构化事件和 ping:pong 回调路径
  - 固定 H264/Opus fixture 读取、发送和 media.sent 事件
  - C 侧 H264/Opus on_frame 统计、media.frame 事件和 done summary
affects: [06-chrome-e2e, examples, peer-connection-api, media-e2e]
tech-stack:
  added: []
  patterns: [C demo JSONL protocol, example-local redaction boundary, fixed fixture media self-tests, read-only DataChannel diagnostics]
key-files:
  created:
    - examples/chrome-e2e/mrtc_chrome_answerer.c
    - examples/chrome-e2e/README.md
  modified:
    - CMakeLists.txt
    - include/micrortc/peer_connection.h
    - src/peer_connection.c
    - tests/peer_connection/test_peer_connection_api.c
key-decisions:
  - "06-02: C answerer 主路径采用 stdin/stdout JSON-line 协议，继续由 tests/e2e signaling bridge 持有应用层 signaling。"
  - "06-02: DataChannel label/id 以 read-only public accessor 暴露，避免 demo 直接读取 private struct。"
  - "06-02: 媒体 fixture 发送在 example target 内使用现有 private media send hook 统计 RTP 包，不把该 hook 暴露到 public API。"
  - "06-02: --self-test-media-callbacks 通过 protected RTP loopback 证明 on_frame 回调层统计，不只停留在 write_frame 调用。"
patterns-established:
  - "Demo stdout 只输出 hello/answer/candidate/event/done/error JSON object line，便于 Node runner 解析。"
  - "真实 ICE/TURN 配置只作为输入进入 MRTC_PEER_CONNECTION_CONFIG，不输出 credential/password/username。"
  - "固定媒体源保持 Annex-B H264 与 length-prefixed Opus，不引入采集、编码、解码或容器解析依赖。"
requirements-completed: [E2E-01, E2E-02, E2E-03, E2E-05, E2E-07, E2E-08, TEST-01, TEST-04]
duration: 16min
completed: 2026-05-13
---

# Phase 06 Plan 02: C Answerer Demo、结构化事件与固定媒体源 Summary

**C answerer JSON-line demo 已能生成 Chrome answer、输出 DataChannel/媒体结构化事件，并用固定 H264/Opus fixture 自测发送与 on_frame 回调统计。**

## Performance

- **Duration:** 16 min
- **Started:** 2026-05-13T09:40:33Z
- **Completed:** 2026-05-13T09:56:03Z
- **Tasks:** 4
- **Files modified:** 6

## Accomplishments

- 新增 `mrtc_chrome_answerer` C demo，支持 `--fixtures`、`--ice-config`、`--session-id`、`--log-json`、`--self-test-media`、`--self-test-media-callbacks` 和 `--help`。
- 实现 offer/candidate/start-media/stop JSON-line 输入，以及 hello/answer/candidate/event/done/error JSON-line 输出。
- 添加 DataChannel label/id read-only public accessor，并让 demo 输出 `datachannel.open` 与 `datachannel.message` 事件；`ping:<nonce>` 会经 `mrtc_data_channel_send()` 回复 `pong:<nonce>`。
- 复用固定 H264 Annex-B 与 Opus length-prefixed fixture，经 `mrtc_transceiver_write_frame()` 发送并输出 `media.sent` 事件。
- 注册 H264/Opus transceiver `on_frame` 回调，输出 `media.frame` 事件，并在 done summary 中记录 C 侧媒体帧数、字节数和 timestamp monotonic failure。

## Task Commits

1. **Task 06-02-01: 实现 mrtc_chrome_answerer JSON line protocol** - `47b2ef8` (feat)
2. **Task 06-02-02: 接入 DataChannel ping/pong 和结构化事件** - `5ccd14d` (feat)
3. **Task 06-02-03: 复用固定 H264/Opus fixture 实现 C 到 Chrome 媒体发送** - `3b256f0` (feat)
4. **Task 06-02-04: 统计 Chrome 到 C 的 H264/Opus on_frame 回调** - `a0c7340` (feat)
5. **Plan documentation supplement** - `f603186` (docs)

## Files Created/Modified

- `examples/chrome-e2e/mrtc_chrome_answerer.c` - C answerer demo、JSON-line 协议、DataChannel 事件、fixture media send 和 on_frame 统计。
- `examples/chrome-e2e/README.md` - demo 构建/运行/self-test 用法和 ICE/TURN secret 处理约束。
- `CMakeLists.txt` - 注册 answerer example 时为该 target 添加 private `src` include，供 example 内部使用 media send hook。
- `include/micrortc/peer_connection.h` - 新增 `mrtc_data_channel_label()` 与 `mrtc_data_channel_id()` read-only accessor。
- `src/peer_connection.c` - 实现 DataChannel label/id accessor。
- `tests/peer_connection/test_peer_connection_api.c` - 覆盖新增 public accessor 的基础契约。

## Decisions Made

- DataChannel label/id 只读 accessor 使用 `mrtc_*` 命名和标准 C 类型，保持 public API 边界简单。
- `mrtc_chrome_answerer` 使用 public PeerConnection/DataChannel/transceiver API 作为主路径；只有媒体包统计使用 example-local private hook，因为当前 `write_frame()` 需要应用层 packet delivery hook 才能完成发送。
- `--self-test-media-callbacks` 不伪造 on_frame 调用，而是回放 demo 刚发出的 protected RTP 包进入接收路径，证明统计到达 public `on_frame` 回调层。
- 未把真实 TURN secret、credential、password 或 username 写入 demo 输出；ICE config 仅被解析进 peer connection config。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build` - passed。
- `./build/examples/chrome-e2e/mrtc_chrome_answerer --help` - passed，列出 required options、self-test flags 和 JSON message types。
- `printf 'not-json\n' | ./build/examples/chrome-e2e/mrtc_chrome_answerer` - passed，输出 JSON `error` 并返回 1。
- 使用 `tests/fixtures/minimal_offer.sdp` 生成 offer JSON 输入 - passed，输出非空 `answer.sdp`。
- `./build/examples/chrome-e2e/mrtc_chrome_answerer --fixtures ./tests/fixtures --self-test-media` - passed，输出 video/audio `media.sent`。
- `./build/examples/chrome-e2e/mrtc_chrome_answerer --fixtures ./tests/fixtures --self-test-media-callbacks` - passed，输出 video/audio `media.frame` 且 summary monotonic failure 为 0。
- `ctest --test-dir build -R "data_channel|peer_connection|media|package" --output-on-failure` - passed，5/5 tests passed。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 重新 configure 以注册条件化 example target**
- **Found during:** Task 06-02-01
- **Issue:** 现有 `build/` 是在 `mrtc_chrome_answerer.c` 出现前生成的，`cmake --build build` 不会自动创建该 executable。
- **Fix:** 执行 `cmake -S . -B build -DMRTC_BUILD_TESTS=ON` 重新生成构建系统。
- **Files modified:** 仅 build 目录生成物，未纳入 git。
- **Verification:** `./build/examples/chrome-e2e/mrtc_chrome_answerer --help` 成功运行。
- **Committed in:** N/A

**2. [Rule 3 - Blocking] 补充 C 前置声明修复编译失败**
- **Found during:** Task 06-02-03
- **Issue:** fixture loader 在 `emit_error()` 定义前调用该函数，C99 编译出现 implicit declaration / static declaration 冲突。
- **Fix:** 在 `AnswererApp` 定义后加入 `emit_error()` 前置声明。
- **Files modified:** `examples/chrome-e2e/mrtc_chrome_answerer.c`
- **Verification:** `cmake --build build` 与 `--self-test-media` 通过。
- **Committed in:** `3b256f0`

**3. [Rule 2 - Missing Critical] 补充 demo README 的 secret 处理约束**
- **Found during:** Plan summary scan
- **Issue:** 计划 frontmatter 声明 `examples/chrome-e2e/README.md`，且 D-10 要求真实 TURN credential 不出现在 demo 输出或文档中；缺少 demo 使用文档会让本地 ICE 配置边界不清晰。
- **Fix:** 新增中文 README，记录命令、message types、fixture 边界和真实 ICE/TURN secret 不提交/不输出的要求。
- **Files modified:** `examples/chrome-e2e/README.md`
- **Verification:** 文档未包含真实 hostname、username、password、token 或 credential。
- **Committed in:** `f603186`

---

**Total deviations:** 3 auto-fixed (1 Rule 2, 2 Rule 3)
**Impact on plan:** 均为完成计划正确性、构建可运行性和 secret 使用边界所需；没有引入应用层 signaling 到核心库。

## Known Stubs

None - 已扫描本计划创建/修改文件中的 TODO/FIXME/placeholder/coming soon/not available 和明显空数据占位；`null` 仅用于 JSON candidate 的可选 `sdpMid` 输出，不阻塞计划目标。

## Threat Flags

None - 新增 JSON-line signaling demo 和 ICE config 输入均在本计划 threat model 覆盖范围内；核心库未新增网络 endpoint、auth path、文件访问信任边界或内置 signaling 依赖。

## Issues Encountered

- CMake configure 仍报告系统缺少 libsrtp/usrsctp；当前 `MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=OFF`，这是既有可选依赖模式，构建和验证均通过。
- `build/`、`reflib/`、`.idea/` 在执行前已为未跟踪目录，未清理、未重置、未纳入提交。
- 计划 frontmatter 列出的 `tests/e2e/fixtures.js` 和 `tests/e2e/signaling-server.js` 本计划未改动；06-02 tasks 没有要求 runner 参数透传或 browser spec 断言，后续 06-03/06-04 可在已有 JSON schema 上接入。

## User Setup Required

None - 本计划不需要外部服务配置。真实 TURN relay 验收仍应使用本地忽略文件提供 ICE config，不能提交真实 secret。

## Next Phase Readiness

Plan 06-03 可以基于 `mrtc_chrome_answerer` 的 JSON-line protocol、`media.sent`、`media.frame`、`datachannel.open` 和 done summary 字段添加 Playwright host spec 与短时稳定媒体断言。TURN relay 计划可继续复用 `--ice-config` 输入，并在 Node runner 层验证 relay candidate/path。

## Self-Check: PASSED

- 关键创建/修改文件均存在。
- 任务提交 `47b2ef8`、`5ccd14d`、`3b256f0`、`a0c7340`、`f603186` 均可在 git log 中找到。
- 计划级验证全部通过。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-13*
