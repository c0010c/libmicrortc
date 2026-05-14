---
phase: 06-chrome-e2e
plan: "04"
subsystem: chrome-host-media-e2e
tags: [chrome, e2e, h264, opus, media, playwright, sdp]
requires:
  - phase: 06-chrome-e2e
    provides: 06-08 strict Chrome transport smoke passed
provides:
  - 默认 host/local Chrome E2E spec
  - browser 侧 H264/Opus inbound RTP、DOM video 和 WebAudio/stats 断言
  - C answerer 侧 Chrome H264/Opus on_frame 事件断言
  - Chrome offer payload type 驱动的 SDP answer 和 RTP payload type 协商
  - fixture H264/Opus 短流发送，用于稳定浏览器 stats delta
affects: [phase-06, chrome-e2e, sdp, peer-connection, media, tests]
tech-stack:
  added: []
  patterns: [browser-stats-delta, bidirectional-media-e2e, payload-type-negotiation]
key-files:
  created:
    - tests/e2e/chrome-host.spec.js
    - tests/e2e/media-assertions.js
    - tests/e2e/summary.js
    - .planning/phases/06-chrome-e2e/06-04-SUMMARY.md
  modified:
    - src/sdp.c
    - src/peer_connection.c
    - examples/chrome-e2e/browser-client.js
    - examples/chrome-e2e/mrtc_chrome_answerer.c
    - tests/e2e/signaling-server.js
    - tests/sdp/test_sdp_roundtrip.c
    - tests/media/test_media_send.c
    - tests/fixtures/h264_annexb_sample.h264
key-decisions:
  - "06-04: SDP answer 必须采用 Chrome offer 中实际协商到的 H264/Opus payload type，不能继续硬编码 video=96。"
  - "06-04: browser media 断言以 RTP stats 递增加 DOM/WebAudio/RTCStats 解码证据为准。"
  - "06-04: C 侧媒体断言以 answerer 输出的 on_frame 结构化事件为准，不用 RTP packet 计数替代。"
requirements-addressed: [E2E-04, E2E-05, E2E-06, E2E-07, E2E-08, TEST-02, TEST-04]
requirements-completed: [E2E-06, E2E-07, E2E-08]
duration: 52 min
completed: 2026-05-14
---

# Phase 06 Plan 04: Chrome Host E2E 连接、DataChannel 与双向媒体断言 Summary

**默认 host/local Chromium E2E 已通过：真实 browser connection、DataChannel ping/pong、C→Chrome H264/Opus 播放证据和 Chrome→C H264/Opus `on_frame` 事件均满足断言。**

## Performance

- **Duration:** 52 min
- **Completed:** 2026-05-14T11:21:55+08:00
- **Tasks:** 4/4 completed
- **Files modified:** E2E spec/helper、browser client、C answerer、SDP/PeerConnection media path、media fixtures 和本 summary

## Accomplishments

- 新增 `tests/e2e/chrome-host.spec.js`，启动本地 signaling server、C answerer 和浏览器页面，按 `SIGNALING -> CONNECTION -> DATACHANNEL -> BROWSER MEDIA -> C MEDIA -> SUMMARY` 分阶段验收。
- 新增 `tests/e2e/media-assertions.js`，对 browser inbound RTP stats 做稳定 delta 采样，并要求视频 DOM ready/尺寸/framesDecoded 与音频 samples/energy 证据。
- 新增 `tests/e2e/summary.js`，统一写入 E2E summary 和 stage failure 分类。
- 浏览器端生成确定性 canvas video track 和 WebAudio synthetic audio track，并保持 AudioContext/节点强引用，避免 headless Chromium 偶发不发音频 RTP。
- C answerer 的 `start-media` 改为发送短 H264/Opus 流，避免一次性 burst 被 stats 采样错过。
- SDP parser 记录每个 media section 中 offer 的 H264/Opus payload type；answer 生成时同步到 transceiver 和 `m=` / `rtpmap` / `fmtp` / `rtcp-fb` 行。
- PeerConnection media send 支持真实 selected ICE pair 发送；hook-only harness 在无 selected pair 时仍可走 passthrough，保持本地媒体单测和 self-test 风格可用。

## Files Created/Modified

- `tests/e2e/chrome-host.spec.js` - 默认 host Chrome E2E，覆盖 signaling、connection、DataChannel、browser media 和 C media。
- `tests/e2e/media-assertions.js` - browser inbound video/audio stats 与 DOM/WebAudio 断言。
- `tests/e2e/summary.js` - E2E summary/stage helper。
- `tests/e2e/signaling-server.js` - 支持给 answerer 传入 `--fixtures` 等参数。
- `examples/chrome-e2e/browser-client.js` - 合成双向媒体、remote media state、control message 和稳定 AudioContext。
- `examples/chrome-e2e/mrtc_chrome_answerer.c` - fixture 短流发送、媒体发送事件和错误诊断。
- `src/sdp.c` - offer payload type 解析与 answer payload type 协商。
- `src/peer_connection.c` - 真实 selected ICE pair 媒体发送，以及 hook-only passthrough harness fallback。
- `tests/sdp/test_sdp_roundtrip.c` - 覆盖非 96 H264 / 非默认 Opus payload type 协商。
- `tests/media/test_media_send.c` - 失败断言输出行号，便于定位媒体路径回归。
- `tests/fixtures/h264_annexb_sample.h264` - 重新生成为可被 Chromium 解码的 Annex-B SPS/PPS/IDR fixture。

## Issues Encountered

- 初次 browser media 失败时，Chrome 收到 RTP 但 `framesDecoded=0`。根因是 answer 仍硬编码 `m=video ... 96`，而 Chromium offer 中 PT 96 是 VP8；修复为使用 offer 中的 H264 PT 103 后，Chrome 正常解码。
- 视频/音频 fixture 一次性 burst 会让 stats delta 采样错过递增窗口；改为短流发送，并让 browser audio/video 断言并发观察。
- headless Chromium 中 WebAudio synthetic audio 偶发不稳定；通过保存 AudioContext/节点强引用并周期性 `resume()` 后，C 侧稳定收到 Opus frame。
- `mrtc_media_send_test` 暴露 hook-only harness 被 selected pair 要求挡住；发送路径改为 selected pair 或 media hook 二者至少具备其一。

## Verification

- `cmake --build build -j2` - passed。
- `cmake --build build-transport -j2` - passed。
- `ctest --test-dir build -R "sdp|peer_connection|media_send" --output-on-failure` - passed，3/3 tests。
- `ctest --test-dir build-transport -R "sdp|peer_connection|media_send|stun|ice|dtls|srtp|sctp|data_channel|transport" --output-on-failure` - passed，8/8 tests。
- `./build/examples/chrome-e2e/mrtc_chrome_answerer --fixtures ./tests/fixtures --self-test-media-callbacks` - passed。
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --project=chrome-host --grep "host Chrome E2E"` - passed，1/1 tests。
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` - passed，1/1 tests。
- `git diff --check` - passed。

## Known Stubs

- 本计划仍使用 Playwright bundled Chromium fallback，因为本机没有系统 Google Chrome。E2E 命令通过 `MRTC_E2E_BROWSER_CHANNEL=chromium` 显式记录该选择；后续如有系统 Chrome，可切回 `chrome` channel。
- TURN relay 还未验收；06-05 需要在本计划通过的 host media E2E 基础上增加 relay-only candidate/stats 判据。

## Next Phase Readiness

06-05 已解除阻塞，可以执行 TURN relay E2E、selected relay candidate stats 校验和 secret redaction。

## Self-Check: PASSED

- 默认 host Chrome E2E 通过真实 browser transport、DataChannel 和双向 H264/Opus 媒体断言。
- 06-08 strict transport smoke 回归通过。
- C 单测、strict transport CTest 和 whitespace check 均通过。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-14*
