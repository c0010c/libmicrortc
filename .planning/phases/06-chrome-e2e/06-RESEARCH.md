# Phase 6: Chrome 自动化 E2E 与测试收口 - Research

**Researched:** 2026-05-13
**Status:** Complete

## Research Question

Phase 6 要回答的问题是：怎样把现有 C WebRTC 核心库、Phase 4 DataChannel/ICE/TURN scaffolding、Phase 5 H264/Opus fixture 媒体路径，收束成一个可自动启动 C demo 与 Chrome、自动交换 signaling、并能清晰断言连接、DataChannel、TURN relay 和双向媒体流动的 v1 验收链路。

## Inputs Read

- `.planning/phases/06-chrome-e2e/06-CONTEXT.md`
- `.planning/PROJECT.md`
- `.planning/REQUIREMENTS.md`
- `.planning/ROADMAP.md`
- `.planning/STATE.md`
- `README.md`
- `CMakeLists.txt`
- `include/micrortc/peer_connection.h`
- `src/peer_connection.c`
- `src/sdp.c`
- `src/ice/ice_config.c`
- `src/data_channel/data_channel.*`
- `src/media/media_transceiver.*`
- `tests/integration/mrtc_phase4_network_verify.c`
- `tests/integration/mrtc_phase5_media_verify.c`
- `tests/fixtures/h264_annexb_sample.h264`
- `tests/fixtures/opus_packets.bin`

## External References

- Playwright 官方 Browser 文档：Playwright 可使用 bundled Chromium，也可配置 `channel: "chrome"` 跑 Google Chrome；官方文档特别指出媒体 codec 场景可能需要官方浏览器二进制而不是默认 Chromium。链接：https://playwright.dev/docs/browsers
- Playwright 官方 CI 文档：Linux CI 需要安装浏览器依赖，CI 中建议单 worker 提高稳定性；`npm ci`、`npx playwright install --with-deps`、`npx playwright test` 是官方基本路径。链接：https://playwright.dev/docs/ci
- W3C WebRTC 规范：signaling channel 由应用层提供，常见方式包括 WebSocket；`RTCConfiguration` 包含 `iceServers` 和 `iceTransportPolicy`，可用于 relay-only 测试。链接：https://www.w3.org/TR/webrtc/
- W3C WebRTC Stats 规范：`RTCTransportStats.selectedCandidatePairId` 指向选中 candidate pair；`RTCIceCandidateStats.candidateType` 可区分 `relay`，`RTCInboundRtpStreamStats`/`RTCDataChannelStats` 可用于媒体和 DataChannel 计数断言。链接：https://www.w3.org/TR/webrtc-stats/

## Current State Findings

### 已可复用

- Public API 已提供 PeerConnection、DataChannel、audio/video transceiver、`MRTC_FRAME`、`mrtc_transceiver_write_frame()`、`on_frame` 和 `on_picture_loss`。
- `src/sdp.c` 已能生成包含 audio/video/application m-line、H264/Opus fmtp/rtpmap、ICE ufrag/pwd、DTLS fingerprint、`setup` 和 DataChannel SCTP 属性的 SDP。
- `tests/integration/mrtc_phase5_media_verify.c` 已实现固定 H264 Annex-B 和 Opus packet fixture 读取逻辑、分层 success label 和 C 侧 media counter 断言。
- `tests/integration/mrtc_phase4_network_verify.c` 已建立“缺少真实 TURN config 时硬失败”的语义和分层 label 风格。
- `mrtc-ice-servers.example.json` 与 `src/ice/ice_config.c` 已定义根目录本地配置形态；真实 `mrtc-ice-servers.local.json` 已被 `.gitignore` 忽略。

### 关键缺口

- 当前没有 `examples/chrome-e2e/`，也没有 C demo、浏览器页面、Node/Playwright runner 或 WebSocket signaling server。
- C demo 需要一个不污染核心库的应用层 signaling 边界。为了避免新增 C WebSocket 依赖，推荐 C demo 使用 line-delimited JSON over stdio；Node runner 负责 WebSocket server 和 C demo stdio bridge。这样仍满足“本地 WebSocket signaling server 属于 demo/test 层”的决策，同时核心库和 C demo 都不依赖 libwebsockets。
- 当前 PeerConnection 把 `addIceCandidate()` 作为 selected-pair-ready 的触发点，Phase 4/5 多数 verifier 是 deterministic harness。Phase 6 计划必须把“真实 Chrome E2E”作为目标，并允许执行时补足真实网络 I/O、诊断 API 或 demo 层 adapter，而不是只复用 C harness 假阳性。
- Browser 侧不能只看 `connectionState === "connected"`。媒体成功需要同时看 DOM 可观察状态、`getStats()` counters delta、DataChannel ping/pong 和 C 端结构化事件。
- TURN relay 不能放入默认本地测试；需要单独命令强制 `iceTransportPolicy: "relay"`，缺少 config 时非零退出，并验证 selected local candidate `candidateType=relay`。

## Recommended Architecture

### 文件布局

- `examples/chrome-e2e/mrtc_chrome_answerer.c`：C answerer demo，使用 public API，读写 line-delimited JSON，输出结构化事件。
- `examples/chrome-e2e/index.html`：最小浏览器页面，创建 Chrome offer、建立 DataChannel、挂载 audio/video 元素和 WebAudio stats helper。
- `examples/chrome-e2e/browser-client.js`：浏览器 WebRTC 逻辑和可被 Playwright 调用的 `window.__mrtcE2E` 状态 API。
- `tests/e2e/package.json` 与 lockfile：Node/Playwright 依赖固定在测试目录。
- `tests/e2e/playwright.config.js`：只配置 Chrome/Chromium 项目，CI workers 为 1。
- `tests/e2e/signaling-server.js`：本地 WebSocket server、C demo stdio bridge、event aggregation、secret redaction。
- `tests/e2e/chrome-host.spec.js`：默认 host/local Chrome E2E。
- `tests/e2e/chrome-turn.spec.js`：显式 TURN relay E2E。
- `tests/e2e/summary.js`：机器可读 JSON/JUnit 摘要和阶段化输出。
- `scripts/verify-v1.sh`：总验收脚本，串起 CMake build、CTest、host Chrome E2E，并在显式 `--turn-config` 时运行 TURN E2E。

### Signaling Schema

推荐使用 JSON line / WebSocket message 共用 schema：

- `hello`: `{ "type": "hello", "role": "browser|answerer", "sessionId": "..." }`
- `offer`: `{ "type": "offer", "sdp": "..." }`
- `answer`: `{ "type": "answer", "sdp": "..." }`
- `candidate`: `{ "type": "candidate", "candidate": "..." }`
- `event`: `{ "type": "event", "name": "pc.connected", "fields": {...} }`
- `done`: `{ "type": "done", "summary": {...} }`
- `error`: `{ "type": "error", "stage": "...", "message": "redacted..." }`

所有日志和 summary 必须 mask `credential`、`password`、`username`、TURN URL query 中可能含秘密的字段。

### Browser Assertions

- 连接：`pc.connectionState === "connected"`，并至少一次 stats transport 的 `selectedCandidatePairId` 可解析到 `candidate-pair`。
- DataChannel：浏览器发送 `ping:<nonce>`，C demo 回 `pong:<nonce>`；stats `data-channel.messagesSent/messagesReceived` 增长。
- Browser inbound video：`HTMLVideoElement.readyState >= 2`，`videoWidth > 0`，`videoHeight > 0`，`inbound-rtp kind=video` 的 `bytesReceived`、`packetsReceived` 和 `framesDecoded` 或可用 frame counter 在稳定窗口内增长。
- Browser inbound audio：`inbound-rtp kind=audio` 的 `bytesReceived`、`packetsReceived` 增长，并结合 WebAudio energy 或 media playout stats 判断音频管线有活动。
- Browser outbound media 到 C：C demo 输出 `media.frame kind=video codec=h264 count=N` 和 `media.frame kind=audio codec=opus count=N`，每种媒体不少于短时阈值，timestamp 单调。

### TURN Assertions

- TURN E2E 使用 `iceTransportPolicy: "relay"`。
- Runner 从 `mrtc-ice-servers.local.json` 或显式 `--turn-config` 读取配置，缺失时硬失败。
- Browser stats 中选中 candidate pair 的 local candidate `candidateType` 必须等于 `relay`。
- C demo 也输出 selected candidate/candidate pair 证据；执行阶段如 public API 不足，应新增只读诊断 API 或 demo 层事件，不把 credential 写入日志。

## Validation Architecture

Phase 6 需要三层验证：

1. **确定性 C 层**：`ctest --test-dir build --output-on-failure` 保持默认协议单元测试、Phase 4/5 integration verifier 和 package consumer 边界。
2. **默认浏览器 E2E**：`npm --prefix tests/e2e ci && npx --prefix tests/e2e playwright test --project=chrome-host` 或项目脚本包装命令，启动 signaling server、C demo 和 Chrome，断言 host/local 连接、DataChannel、双向 H264/Opus。
3. **显式 TURN relay E2E**：`scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` 或专用 `npm --prefix tests/e2e run test:turn -- --turn-config ...`，强制 relay-only 并校验 selected candidate type。

建议阈值由执行阶段最终调优，但计划应要求：

- 每个媒体方向至少连续两次观测到 counters 增长。
- Browser inbound RTP 每路 `packetsReceived > 0` 且 `bytesReceived > 0`，视频还需 `framesDecoded` 或 DOM video frame 证据。
- C 端 `on_frame` 每路 `count >= 2`，`bytes > 0`，timestamp 单调。
- 失败 summary 必须能区分 build、CTest、signaling、connection、DataChannel、browser-media、c-media、TURN-relay、secret-redaction。

## Risks and Mitigations

| Risk | Severity | Mitigation |
|------|----------|------------|
| 使用默认 Chromium 导致 H264 支持不稳定 | HIGH | Playwright config 优先支持 `channel: "chrome"`，同时保留环境变量切换 bundled Chromium；README 说明 media codec 风险。 |
| Phase 6 只证明 harness 而不是真实 Chrome | HIGH | Plan 必须要求 Playwright 启动真实 browser 页面，使用 WebRTC stats 与 C demo event 双向断言。 |
| TURN credential 泄漏到日志或 planning 文档 | HIGH | 所有 runner/server/demo 输出走 redaction helper；计划和文档只写 placeholder。 |
| E2E flaky | MEDIUM | CI workers=1，短时稳定阈值允许多次采样，summary 包含原始 counters delta 和阶段超时。 |
| C demo 引入应用层 signaling 到核心库 | HIGH | WebSocket server、stdio bridge 和 signaling schema 只在 `examples/`、`tests/e2e/`、`scripts/`；`micrortc` target 不链接 Node/WS/libwebsockets。 |

## Planning Recommendation

拆成 5 个计划：

1. E2E tooling 与 signaling skeleton。
2. C answerer demo、结构化事件和固定媒体源。
3. Browser host E2E 的连接/DataChannel/双向媒体断言。
4. TURN relay E2E、relay stats 校验和 secret redaction。
5. v1 总验收脚本、文档、CTest/CI 收口和状态更新。

## RESEARCH COMPLETE

