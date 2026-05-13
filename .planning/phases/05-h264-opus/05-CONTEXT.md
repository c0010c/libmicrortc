# Phase 5: H264/Opus 媒体路径 - Context

**Gathered:** 2026-05-13T12:48:25+08:00
**Status:** Ready for planning

<domain>
## Phase Boundary

本阶段把 Phase 4 已经建立的 PeerConnection、ICE/DTLS/SRTP/SCTP 基础接入真实编码后音视频媒体路径。交付重点是：扩展 AWS 薄裁剪风格的 transceiver 和 `writeFrame` 类 public API；剥离 RTP/RTCP、H264 和 Opus packetize/depacketize；通过 SRTP protect/unprotect 后的媒体集成路径发送和接收编码后帧；用固定 H264/Opus 测试媒体源证明 C 端双向媒体路径可观察。

本阶段不实现媒体采集、音视频编码、GStreamer/Ogg/MP4 等容器适配，不把应用层 signaling 放入核心库，不要求完整 Chrome 自动化 E2E 验收。完整浏览器页面、自动 signaling、Chrome 双向媒体断言和 TURN relay 自动化收口留给 Phase 6；Phase 5 可提供低成本半自动浏览器 smoke，但不能把它作为替代分层 C harness 的唯一验收。

</domain>

<decisions>
## Implementation Decisions

### 媒体 Public API 形态

- **D-01:** Phase 5 媒体 API 采用 AWS 薄裁剪策略：保留 `addTransceiver`、`writeFrame`、`transceiverOnFrame`、`transceiverOnPictureLoss` 这一类使用模型，但 public 命名使用 `MRTC_*` / `mrtc_*`，不暴露 AWS/PIC 类型。
- **D-02:** transceiver 创建接口需要包含 media kind、codec、direction 和回调配置，并返回 opaque transceiver handle。推荐形态类似 `mrtc_peer_connection_add_transceiver(peer_connection, track/init/callbacks, user_data, &transceiver)`，具体字段名由 planner 按现有 public API 风格决定。
- **D-03:** `MRTC_FRAME` 采用贴近 AWS `Frame` 的较完整结构，而不是只传裸 bytes。至少应表达 frame data、size、presentation timestamp、decoding timestamp、duration、index 和 flags 等信息；字段类型必须是标准 C / MRTC 自有类型。
- **D-04:** 接收媒体通过 transceiver 级 `on_frame` 回调交给应用层。回调输出重组后的编码帧，不要求应用层处理 RTP packet。
- **D-05:** public API 需要保留应用层响应 PLI 的能力。Phase 5 应提供 transceiver 级 `on_picture_loss` 回调；核心库只检测/触发关键帧请求，不生成关键帧，因为核心库不编码。

### H264 视频路径边界

- **D-06:** H264 输入格式先锁定 Annex-B NALU / bytestream。Phase 5 不要求 AVCC length-prefixed 输入，也不要求同时支持两种格式。
- **D-07:** H264 packetizer 沿用本地 KVS 路径：解析 `0x000001` / `0x00000001` start code，按 NALU 生成 RTP payload，小 NALU 走 Single NALU，大 NALU 走 FU-A。
- **D-08:** SPS/PPS 处理不新增 extradata API，也不把缓存重发策略作为 Phase 5 必须项。固定 H264 测试流应在关键帧前自带 SPS/PPS；库可以做轻量检测或测试断言，但不需要比本地 KVS 多实现复杂缓存重发。
- **D-09:** H264 接收侧 `on_frame` 输出 Annex-B 重组帧。depacketizer 应把 RTP payload 还原为带 start code 的编码帧/NALU 聚合，方便写文件和验证。
- **D-10:** SDP H264 方向应优先继承本地 KVS 兼容策略：`packetization-mode=1`、`level-asymmetry-allowed=1`、偏好 `profile-level-id=42e01f`，并声明 H264 `nack` / `nack pli`。

### Opus 音频路径边界

- **D-11:** Opus 沿用本地 KVS 模型，与 H264 共用 `MRTC_FRAME` 和 `mrtc_transceiver_write_frame()`。Opus `frame.data` 是单个编码后 Opus packet，不定义单独音频 frame public API。
- **D-12:** Opus RTP timestamp 由 `MRTC_FRAME.presentation_ts` 提供，并按 48kHz clock rate 转换。库不盲猜固定 20ms timestamp。
- **D-13:** Opus payloader/depayloader 采用直拷贝语义：发送侧一个 Opus packet 对应一个 RTP payload；接收侧 `on_frame` 直接输出非空 Opus payload。
- **D-14:** Opus SDP/fmtp 沿用本地 KVS 默认：`opus/48000/2` + `minptime=10;useinbandfec=1`。Phase 5 不要求应用层配置 Opus fmtp。
- **D-15:** 固定 Opus 测试源采用“长度前缀 + Opus packet 序列”fixture。demo/harness 逐包读取编码后 packet 并填入 `MRTC_FRAME`；不把 Ogg Opus 容器解析放入核心库。
- **D-16:** Opus 接收验证到编码后包层：断言收到非空 Opus payload frame，并验证 RTP timestamp 单调。不引入音频解码或可播放性验证作为 Phase 5 必须项。

### RTCP/NACK 与验收深度

- **D-17:** RTCP 最小能力包含 Sender Report、Receiver Report、NACK 和 PLI。只解析不驱动行为不足以满足 Phase 5。
- **D-18:** NACK 采用发送侧 RTP rolling buffer + NACK 触发重发。优先参考本地 KVS `RtpRollingBuffer` / retransmitter 路径，按 RTP 层统一处理，不只对 H264 特判。
- **D-19:** Phase 5 不要求完整 Chrome 自动化互通通过。必须用 C harness 证明双向 H264/Opus packetize、depacketize、SRTP media integration 和 `on_frame` 输出；Chrome 全自动验收留给 Phase 6。
- **D-20:** Phase 5 完成标准采用分层验收：public API 测试、RTP/codec unit 测试、SRTP protect/unprotect 媒体集成测试、固定媒体 fixture 双向流动 harness。验收输出应能区分是哪一层失败。

### the agent's Discretion

Planner 可以决定具体函数名、结构字段排序、timestamp 单位命名、fixture 文件扩展名、测试 executable 拆分、是否提供一个汇总 `phase5` verifier 命令，以及半自动 Chrome smoke 是否低成本纳入。但不得改变以下约束：API 采用 AWS 薄裁剪 + MRTC 命名；H264 先锁 Annex-B；Opus 共用 `MRTC_FRAME`；RTCP 包含 SR/RR/NACK/PLI；NACK 要有 rolling buffer 重传；Phase 5 验收必须分层证明编码后媒体路径，而不是只靠编译或只靠 RTP packet 到达。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目规划

- `.planning/PROJECT.md` — 项目定位、v1 范围、核心库不做采集/编码、不内置 signaling、Chrome/H264/Opus 优先约束。
- `.planning/REQUIREMENTS.md` — Phase 5 对应 `API-04`、`PROTO-04`、`MEDIA-01` 到 `MEDIA-07`。
- `.planning/ROADMAP.md` — Phase 5 目标、成功标准，以及 Phase 6 浏览器自动化边界。
- `.planning/STATE.md` — 当前阶段、Phase 4 完成状态和需要继承的项目级决策。

### 前置阶段决策

- `.planning/phases/01-/01-CONTEXT.md` — 本地 KVS 基线、核心/非核心边界和 signaling excluded 决策。
- `.planning/phases/01-/BASELINE.md` — 本地 `reflib/kvs-webrtc-sdk` 基线、模块分类和依赖观察。
- `.planning/phases/01-/SOURCE-MANIFEST.md` — AWS 派生文件来源追溯规则；Phase 5 搬迁或重写派生 RTP/RTCP/媒体文件必须更新。
- `.planning/phases/01-/COMPLIANCE.md` — Apache-2.0、NOTICE、第三方依赖和 excluded 组件策略。
- `.planning/phases/02-/02-CONTEXT.md` — `micrortc` target、CMake/install/export、public include 和最小库边界。
- `.planning/phases/03-aws-api-signaling-free-peerconnection/03-CONTEXT.md` — AWS 薄裁剪 API、opaque handle、SDP/candidate 字符串边界、Answerer 先行和 signaling-free 约束。
- `.planning/phases/04-datachannel/04-CONTEXT.md` — ICE/DTLS/SRTP/SCTP/DataChannel、真实网络验证和 SRTP session 初始化决策。
- `.planning/phases/04-datachannel/04-PATTERNS.md` — 当前 public API、PeerConnection、CMake、CTest 和 KVS 来源追溯模式。

### 当前代码与本地参考库

- `include/micrortc/peer_connection.h` — 当前 PeerConnection/DataChannel public API；Phase 5 需要扩展 transceiver、codec、frame、write frame 和 media callbacks。
- `include/micrortc/micrortc.h` — 当前 umbrella public header 和 `MRTC_STATUS` 状态码集合。
- `src/peer_connection.c` — 当前 PeerConnection private owner，已持有 DTLS/SRTP/SCTP/DataChannel 对象；Phase 5 应在此或拆分模块中挂入 transceiver/media state。
- `src/sdp.c` 和 `src/sdp.h` — 当前 SDP helper；Phase 5 需要扩展音视频 m-line、rtpmap、fmtp、rtcp-fb、SSRC/mid/msid 等媒体 SDP 语义。
- `src/srtp/srtp_session.c` 和 `src/srtp/srtp_session.h` — 当前 SRTP session wrapper；Phase 5 需要从 key/session 初始化推进到 RTP/RTCP protect/unprotect 媒体路径。
- `CMakeLists.txt` — 当前 `micrortc` 静态库 target、CTest 测试入口和 OpenSSL/libsrtp/usrsctp 依赖发现路径。
- `tests/fixtures/minimal_offer.sdp` — 当前 H264 SDP fixture；Phase 5 可扩展为音视频 SDP 样本。
- `tests/peer_connection/test_peer_connection_api.c` — 可扩展 public media API 行为测试。
- `tests/transport/test_dtls_srtp.c` — SRTP keying material/session 测试，可作为媒体 protect/unprotect 集成测试的前置 analog。
- `tests/integration/mrtc_phase4_network_verify.c` — Phase 4 verifier 输出模式；Phase 5 可沿用清晰分层输出风格。
- `reflib/kvs-webrtc-sdk` — 唯一本地剥离基线；不得默认使用 GitHub upstream latest。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` — AWS media public API、`Frame`、transceiver、`writeFrame`、`transceiverOnFrame`、`transceiverOnPictureLoss` 的主要参考。
- `reflib/kvs-webrtc-sdk/src/source/Rtp/` — RTP packet、H264/Opus payloader/depayloader 等主要来源参考。
- `reflib/kvs-webrtc-sdk/src/source/Rtcp/` — RTCP packet、rolling buffer、NACK 相关来源参考。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.c` — KVS `writeFrame` 编排：payload、RTP packet 构造、SRTP 加密、ICE send、rolling buffer、stats。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtcp.c` — RTCP SR/RR/NACK/PLI/TWCC 等处理参考；Phase 5 优先裁剪 SR/RR/NACK/PLI。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/JitterBuffer.*` — 接收侧 RTP packet 到 frame 的重组参考。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Retransmitter.*` — NACK 重传路径参考。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.*` — H264/Opus SDP、payload type、fmtp、rtcp-fb、transceiver direction 等参考。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `include/micrortc/peer_connection.h`：已经建立 `MRTC_PEER_CONNECTION_HANDLE`、`MRTC_DATA_CHANNEL_HANDLE`、config/callback table 和 `mrtc_*` public API 风格；Phase 5 应在同一风格下新增 `MRTC_RTP_TRANSCEIVER_HANDLE`、codec/kind/direction、`MRTC_FRAME` 和 frame callbacks。
- `src/peer_connection.c`：当前 private struct 已经管理 remote/local SDP、ICE credentials、DTLS fingerprint、SRTP session、SCTP session 和 DataChannel list；可扩展为 transceiver list 和 media transport owner，也可以把媒体逻辑拆入 private `src/media`/`src/rtp` 模块。
- `src/srtp/srtp_session.*`：当前只保存 DTLS 导出的 key 和 profile，尚未做真实 protect/unprotect；Phase 5 应把 libsrtp 媒体保护路径补上，并覆盖 RTP 与 RTCP。
- `src/sdp.*`：已有最小 parse/answer helper 和 H264 fixture 测试基础；Phase 5 需要从“保留第一条 m-line”扩展到音视频 transceiver 驱动的 SDP 生成/解析。
- `CMakeLists.txt`：已有核心 target、CTest、可选 OpenSSL/libsrtp/usrsctp 发现和 Phase 4 测试布局；RTP/RTCP/media sources 和 fixture harness 应接入同一 target/CTest 模式。
- `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpH264Payloader.c`：直接支持 Annex-B start code 解析、Single NALU、FU-A、接收侧 Annex-B 重组，是 H264 裁剪的核心参考。
- `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpOpusPayloader.c`：Opus payload/depay 直拷贝，可作为低风险裁剪参考。

### Established Patterns

- Public API 使用 opaque handle、`MRTC_STATUS`、`MRTC_*` 类型和 `mrtc_*` 函数，不暴露 AWS/PIC `STATUS`、`PBYTE`、`UINT64`、`BOOL` 等 typedef。
- 核心库继续排除 AWS/KVS signaling、credential/storage、libwebsockets、采集、编码和媒体容器解析。
- 测试使用小型 C `main()` + CTest 注册；fixture 放在 `tests/fixtures/`。
- 从 AWS 参考库搬迁、裁剪或 rewritten-derived 的文件都必须更新 `.planning/phases/01-/SOURCE-MANIFEST.md`。
- 真实 credential 不写入计划、文档或示例配置；Phase 5 本身不新增 TURN credential 决策。

### Integration Points

- PeerConnection SDP offer/answer 生成需要根据 transceiver list 写入 H264/Opus m-line、direction、rtpmap、fmtp、rtcp-fb、rtcp-mux、mid/msid/ssrc 等属性。
- `mrtc_transceiver_write_frame()` 需要从 transceiver 找到 codec、payload type、SSRC、sequence number、SRTP session 和 ICE send path。
- 接收路径需要从 SRTP unprotect 后的 RTP packet 进入 jitter/reorder/depay，最后调用 transceiver `on_frame`。
- RTCP 路径需要能解析/生成 SR/RR，处理 NACK 触发 rolling buffer 重传，处理 PLI 触发 `on_picture_loss`。
- Fixed media fixture harness 需要分别覆盖 H264 Annex-B 输入/输出、Opus length-prefixed packet 输入/输出、timestamp 单调和非空 frame 回调。

</code_context>

<specifics>
## Specific Ideas

- H264 固定测试文件应采用 Annex-B bytestream，关键帧前自带 SPS/PPS。Phase 5 不需要 AVCC 支持，也不需要独立 codec extradata API。
- H264 接收输出应是 Annex-B 重组帧，可直接写文件用于人工检查或后续 Phase 6 浏览器/媒体验收。
- Opus 固定测试 fixture 采用简单长度前缀格式，例如连续的 `[uint16_or_uint32 length][opus packet bytes]`。具体 endian 和长度宽度由 planner 决定，但必须文档化并有 fixture reader 测试。
- H264 SDP 优先参考本地 KVS 默认 `level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f`。
- Opus SDP 优先参考本地 KVS 默认 `opus/48000/2` 和 `minptime=10;useinbandfec=1`。

</specifics>

<deferred>
## Deferred Ideas

- 完整 Chrome 自动化 E2E、浏览器页面、自动 signaling 交换和双向 H264/Opus 媒体流动断言留给 Phase 6。
- AVCC、MP4、Ogg Opus、GStreamer、FFmpeg 或其他媒体适配不纳入 Phase 5 核心库范围。
- SPS/PPS 缓存重发、复杂关键帧调度、完整拥塞控制、码率调整和媒体质量优化可留给后续版本。
- Opus 解码验证或音频可播放性验证可作为 Phase 6 或后续增强，不作为 Phase 5 完成标准。

</deferred>

---

*Phase: 5-H264/Opus 媒体路径*
*Context gathered: 2026-05-13T12:48:25+08:00*
