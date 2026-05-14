# Phase 6: Chrome 自动化 E2E 与测试收口 - Context

**Gathered:** 2026-05-13T16:29:22+08:00
**Status:** Ready for planning

<domain>
## Phase Boundary

本阶段是 v1 的真实验收收口：建立自动化 Chrome 浏览器互通链路，启动 C 端 demo 和 Chrome 页面，通过 demo/测试层 signaling 交换 SDP/candidate，并断言 PeerConnection connected、DataChannel 双向消息、TURN relay 路径、H264 视频和 Opus 音频双向媒体流动都真实可用。

本阶段可以新增 demo 层和测试层的 signaling 编排，但核心库仍保持 signaling-free；可以引入 Node + Playwright 作为浏览器自动化测试工具，但不改变核心 C 库的依赖边界；可以沿用 Phase 4 的本地 TURN 配置和 Phase 5 的固定媒体 fixture/编码后 frame 模型，但不引入媒体采集、编码、GStreamer、FFmpeg、Ogg/MP4 容器解析、Safari/Firefox 兼容或浏览器矩阵扩展。

</domain>

<decisions>
## Implementation Decisions

### E2E Harness 拓扑

- **D-01:** Phase 6 的自动化 signaling 交换使用本地 WebSocket signaling server。该 server 属于 demo/测试层，只负责在 Chrome 页面和 C demo 之间转发 SDP/candidate/status，不进入核心 `micrortc` 库。
- **D-02:** 自动化 runner 使用 Node + Playwright 主导。Node runner 负责启动 WebSocket signaling server、C demo 子进程和 Chrome/Playwright 页面，并收集所有阶段的断言结果。
- **D-03:** E2E 主路径采用 Chrome offer、C demo answer。C 端在 Phase 6 中优先扮演 Answerer，延续 Phase 3 已锁定的 Answerer 先行路径；Offerer 完整 E2E 不作为 Phase 6 必须项。
- **D-04:** demo 和测试资产分层放置：C demo 与浏览器页面放在 `examples/chrome-e2e/`，Playwright runner、测试脚本、测试依赖和摘要逻辑放在 `tests/e2e/`。

### 媒体流动证明方式

- **D-05:** 浏览器侧收到 C 端 H264 视频时，主要断言使用 `HTMLVideoElement` 可观察状态和 `RTCPeerConnection.getStats()` 组合：视频元素应进入可播放/有尺寸状态，且 inbound RTP 的 frames/bytes/packets 在短时窗口内增长。
- **D-06:** 浏览器侧收到 C 端 Opus 音频时，主要断言使用 WebAudio 音频活动/能量检测和 `getStats()` 组合：不要求人工听音或生成可播放文件，但必须证明浏览器音频管线有活动且 inbound RTP bytes/packets 增长。
- **D-07:** C 端收到 Chrome 发来的 H264/Opus 时，主要断言到编码后 frame 回调层：C demo 需要统计 `on_frame` 回调、非空 frame、media kind、codec 和 timestamp 单调性。只统计 RTP packet 不足以证明 Phase 6 完成。
- **D-08:** 媒体流动成功采用短时稳定阈值，而不是“收到一次即可”。每路媒体应在限定时间内达到最小 packets/bytes/frames，并连续多次观察到 stats 或 C 端计数增长；具体数值由 planner 按 CI 稳定性和执行时间确定。

### TURN Relay 验收边界

- **D-09:** TURN relay E2E 使用单独真实网络命令，不放入默认本地/host E2E 必跑路径。缺少本地 TURN 配置时，该 relay 命令必须硬失败，不能静默跳过。
- **D-10:** Phase 6 沿用 Phase 4 的根目录本地 JSON ICE/TURN 配置形态，继续使用 `ice_servers`、`urls`、`username`、`credential`/`password` 等字段。仓库只提交无秘密模板，真实 credential 不写入 planning 文档、README、脚本默认值、测试日志或提交内容。
- **D-11:** relay-only 验收必须证明实际选中了 TURN relay path：Chrome 侧强制 `iceTransportPolicy=relay`，并用 selected candidate pair / local candidate stats 校验 candidate type 为 `relay`；C 端也需要输出 selected path 或 relay candidate 证据。
- **D-12:** TURN relay E2E 必须覆盖连接、DataChannel 和双向 H264/Opus 媒体流动。只证明 ICE connected 或只证明配置中存在 TURN URL 不足以满足 Phase 6。

### 测试命令与输出收口

- **D-13:** 默认 `ctest` 继续只跑确定性 C/协议测试和现有 C integration verifier；Chrome E2E 使用单独显式命令运行，避免所有开发环境都被 Node/Playwright/Chrome 依赖卡住。
- **D-14:** Phase 6 需要一个总验收脚本用于 v1 收口。默认串起 build、`ctest` 和 host/local Chrome E2E；TURN relay 通过显式参数 opt-in，例如 `--turn-config ./mrtc-ice-servers.local.json`，且缺配置时硬失败。
- **D-15:** 测试输出需要分阶段清晰 banner，并生成机器可读摘要。输出至少要区分 build、CTest、Chrome host E2E、Chrome TURN E2E、DataChannel、media-flow 等失败层，便于本地和 CI 快速定位。
- **D-16:** Node/Playwright 依赖固定在 `tests/e2e/package.json`，必要时配套 lockfile。不要把根目录改造成 JS 项目；Playwright 是测试工具依赖，不是核心库依赖。

### the agent's Discretion

Planner 可以决定 WebSocket signaling message schema、Node runner 文件名、Playwright test organization、C demo CLI 参数、具体短时稳定阈值、机器可读摘要格式、总验收脚本名称、是否生成 JUnit/JSON 两种报告、是否把 host Chrome E2E 注册为可选 CTest label、以及 examples 目录下页面的最小 UI 形态。但不得改变以下约束：本地 WebSocket signaling server；Node + Playwright 主导；Chrome offer/C answer；demo 与测试分层；浏览器侧用可观察媒体管线 + stats；C 侧断言编码后 frame；TURN relay 是单独真实网络硬验且覆盖连接、DataChannel、双向媒体；真实 TURN credential 不进入仓库或 planning 文档。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目规划

- `.planning/PROJECT.md` — 项目定位、v1 范围、Chrome/H264/Opus/TURN 优先约束，以及核心库不做采集、编码、应用层 signaling 的边界。
- `.planning/REQUIREMENTS.md` — Phase 6 对应 `NET-04`、`E2E-01` 到 `E2E-08`、`TEST-01` 到 `TEST-04`。
- `.planning/ROADMAP.md` — Phase 6 目标、成功标准和“Phase 6 是 v1 真正验收点”的依赖说明。
- `.planning/STATE.md` — 当前阶段、已完成 Phase 1-5 状态和需要继承的项目级决策。
- `README.md` — 当前构建、CTest、Phase 4 网络 verifier、Phase 5 媒体 verifier 和现有 public API 状态。

### 前置阶段决策

- `.planning/phases/01-/01-CONTEXT.md` — 本地 KVS 基线、核心/非核心边界和 signaling excluded 决策。
- `.planning/phases/01-/BASELINE.md` — 本地 `reflib/kvs-webrtc-sdk` 基线、模块分类和依赖观察。
- `.planning/phases/01-/SOURCE-MANIFEST.md` — AWS 派生文件来源追溯规则；Phase 6 若从 KVS samples/tests 参考或派生代码，需要继续更新 manifest。
- `.planning/phases/01-/COMPLIANCE.md` — Apache-2.0、NOTICE、第三方依赖和 excluded 组件策略。
- `.planning/phases/02-/02-CONTEXT.md` — `micrortc` 静态库、CMake/install/export、public include 和测试布局边界。
- `.planning/phases/03-aws-api-signaling-free-peerconnection/03-CONTEXT.md` — AWS 薄裁剪 API、SDP/candidate 字符串边界、Answerer 先行和 signaling-free 约束。
- `.planning/phases/04-datachannel/04-CONTEXT.md` — ICE/STUN/TURN、DTLS/SRTP、SCTP/DataChannel、真实网络验证和本地 TURN 配置决策。
- `.planning/phases/05-h264-opus/05-CONTEXT.md` — H264/Opus public media API、Annex-B/Opus fixture、RTCP/NACK/PLI 和 Phase 6 浏览器 E2E 边界。

### 当前代码与测试入口

- `CMakeLists.txt` — 当前 `micrortc` target、CTest 注册方式、Phase 4/5 integration verifier 接入模式；Phase 6 需要在不污染默认 CTest 的前提下新增 E2E/verification 入口。
- `include/micrortc/peer_connection.h` — PeerConnection、DataChannel、transceiver、`MRTC_FRAME`、ICE server config 和 callback public API；C demo 应优先使用这些 public API。
- `include/micrortc/micrortc.h` — umbrella public header 和核心状态码。
- `src/peer_connection.c` — PeerConnection private owner，当前连接状态、SDP、ICE、DataChannel、media transceiver 挂载点。
- `src/sdp.c` 和 `src/sdp.h` — SDP offer/answer、ICE attributes、DTLS fingerprint、media m-line 生成/解析路径。
- `src/ice/ice_config.c` 和 `src/ice/ice_config.h` — Phase 4 本地 JSON ICE/TURN 配置 loader；Phase 6 应沿用配置形态。
- `mrtc-ice-servers.example.json` — 不含秘密的 ICE/TURN 配置模板；真实本地配置不得提交。
- `src/data_channel/data_channel.c` 和 `src/sctp/sctp_session.c` — DataChannel/SCTP 路径；Chrome E2E 需要证明双向消息在真实 PeerConnection 上可用。
- `src/media/media_transceiver.c` 和 `src/media/media_transceiver.h` — transceiver、media send/receive hook、SRTP media path helper；C demo 的媒体断言应使用 public frame callback，测试可参考 private helpers。
- `src/rtp/`、`src/rtcp/`、`src/srtp/`、`src/dtls/` — H264/Opus RTP、RTCP、SRTP、DTLS 支撑模块；E2E 失败诊断需要能区分这些层。
- `tests/integration/mrtc_phase4_network_verify.c` — Phase 4 真实网络 verifier 输出风格和 TURN 配置硬失败模式。
- `tests/integration/mrtc_phase5_media_verify.c` — Phase 5 fixture media verifier，包含 H264/Opus fixture 读取、send/receive、RTCP/NACK/PLI 和分层输出参考。
- `tests/fixtures/h264_annexb_sample.h264` — Phase 5 H264 Annex-B fixture；Phase 6 C demo 可复用或扩展为发送媒体源。
- `tests/fixtures/opus_packets.bin` — Phase 5 big-endian 16-bit length-prefixed Opus fixture；Phase 6 C demo 可复用或扩展为发送媒体源。

### 本地 KVS 参考库

- `reflib/kvs-webrtc-sdk` — 唯一本地剥离基线；不得默认使用 GitHub upstream latest。
- `reflib/kvs-webrtc-sdk/samples/p2p/kvsWebRTCClientMaster.c` — KVS C sample 的音视频/DataChannel 组织参考；只能作为本地基线参考，不能引入 KVS signaling/AWS credential 到核心库。
- `reflib/kvs-webrtc-sdk/samples/p2p/kvsWebRTCClientViewer.c` — KVS viewer/offerer sample 参考；Phase 6 主路径仍锁定 Chrome offer/C answer。
- `reflib/kvs-webrtc-sdk/tst/PeerConnectionFunctionalityTest.cpp` — KVS PeerConnection 行为测试参考。
- `reflib/kvs-webrtc-sdk/tst/DataChannelFunctionalityTest.cpp` — KVS DataChannel 行为测试参考。
- `reflib/kvs-webrtc-sdk/tst/RtpFunctionalityTest.cpp`、`reflib/kvs-webrtc-sdk/tst/RtcpFunctionalityTest.cpp`、`reflib/kvs-webrtc-sdk/tst/IceFunctionalityTest.cpp`、`reflib/kvs-webrtc-sdk/tst/TurnConnectionFunctionalityTest.cpp` — 协议测试参考，仍以本地基线为准。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `include/micrortc/peer_connection.h` 已提供 PeerConnection、ICE server、DataChannel、transceiver、`MRTC_FRAME` 和 media callback public API；Phase 6 C demo 不需要发明另一套应用 API。
- `tests/integration/mrtc_phase5_media_verify.c` 已实现 H264/Opus fixture 读取、media send hook、protected packet receive、RTCP NACK/PLI 验证和清晰输出，可作为 C demo 媒体源、C 侧计数和 failure taxonomy 的参考。
- `tests/integration/mrtc_phase4_network_verify.c` 已体现“真实配置缺失即失败”的 verifier 风格；Phase 6 TURN relay 命令应沿用这种硬失败语义。
- `mrtc-ice-servers.example.json` 和 `src/ice/ice_config.*` 已提供本地 ICE/TURN 配置形态，Phase 6 不需要新增另一套 secret 管理。
- `tests/fixtures/h264_annexb_sample.h264` 和 `tests/fixtures/opus_packets.bin` 已能作为 C 端向 Chrome 发送的固定编码后媒体源。

### Established Patterns

- Public API 使用 `MRTC_*` 类型、`mrtc_*` 函数和 opaque handle，不暴露 AWS/PIC 类型。
- 核心库 target 不引入 AWS/KVS signaling、credential/storage、libwebsockets、媒体采集或编码依赖；应用层 signaling 只能出现在 demo/test 层。
- C 测试以 CTest + 小型 C executable 为主；Phase 6 的 Node/Playwright 依赖应限制在 `tests/e2e/`。
- 真实 TURN credential 不提交，planning 文档和日志也不能记录真实 hostname、username、password、token 或 credential。
- Phase 5 已经证明 C fixture 层媒体路径，但 Phase 6 必须用 Chrome E2E 证明浏览器互通，不能用 C harness 替代。

### Integration Points

- `examples/chrome-e2e/` 应包含 C demo 和浏览器页面：C demo 使用 public PeerConnection/DataChannel/transceiver API，浏览器页面使用 WebRTC API、video/audio/WebAudio 和 stats 断言 helper。
- `tests/e2e/` 应包含 Playwright/Node runner、WebSocket signaling server、host E2E 和 TURN relay E2E 测试逻辑，并固定 Node 依赖。
- 总验收脚本应串起 CMake build、默认 CTest、host Chrome E2E，并在显式 TURN 参数存在时运行 relay-only Chrome E2E。
- E2E 输出需要把 C demo logs、browser console/stats、signaling server events 和最终摘要汇总，避免只留下难以定位的异步进程日志。
- TURN relay 测试需要 Chrome 侧 `iceTransportPolicy=relay`，并检查 selected candidate pair/local candidate stats；C 端也需要输出 relay/selected path 证据供 runner 校验。

</code_context>

<specifics>
## Specific Ideas

- 建议 host/local Chrome E2E 和 TURN relay E2E 共用同一套 Playwright runner，只通过配置切换 `iceTransportPolicy`、TURN config 和验收门槛。
- 建议浏览器页面暴露结构化状态给 Playwright，例如 connected、datachannel ping/pong、video ready/dimensions、audio energy、stats deltas。
- 建议 C demo 输出结构化事件行，例如 `pc.connected`、`datachannel.message`、`media.frame kind=video codec=h264 count=N`、`selected_candidate type=relay`，便于 Node runner 解析。
- 建议机器可读摘要至少覆盖阶段名、是否运行、耗时、通过/失败、失败原因和关键计数；具体 JSON/JUnit schema 由 planner 决定。
- 建议保留 `ctest` 的确定性边界，把 Chrome/Playwright 作为显式 E2E 命令，避免普通 C 开发循环被浏览器依赖拖慢。

</specifics>

<deferred>
## Deferred Ideas

- C 端 Offerer 完整 Chrome E2E 可后续补充；Phase 6 主路径锁定 Chrome offer/C answer。
- Firefox/Safari 互通、跨浏览器矩阵、移动浏览器和复杂媒体质量优化留给 v2 或后续阶段。
- 媒体采集、编码、GStreamer、FFmpeg、Ogg/MP4 容器解析仍不纳入 v1 核心库范围。
- 网络层强制封锁 host/srflx 来证明 relay 的方式暂不作为 Phase 6 必须项；当前选择是 `iceTransportPolicy=relay` + selected candidate 校验。

</deferred>

---

*Phase: 6-Chrome 自动化 E2E 与测试收口*
*Context gathered: 2026-05-13T16:29:22+08:00*
