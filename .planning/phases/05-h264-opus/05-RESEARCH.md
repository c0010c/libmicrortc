---
phase: 05-h264-opus
status: complete
researched_at: 2026-05-13
requirements: [API-04, PROTO-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06, MEDIA-07]
---

# Phase 5: H264/Opus 媒体路径 - Research

## User Constraints

### Locked Decisions From CONTEXT.md

- D-01: 媒体 API 采用 AWS 薄裁剪策略，保留 `addTransceiver`、`writeFrame`、`transceiverOnFrame`、`transceiverOnPictureLoss` 使用模型，但 public 命名使用 `MRTC_*` / `mrtc_*`，不暴露 AWS/PIC 类型。
- D-02: transceiver 创建接口包含 media kind、codec、direction、回调配置，并返回 opaque transceiver handle。
- D-03: `MRTC_FRAME` 贴近 AWS `Frame`，至少表达 data、size、presentation timestamp、decoding timestamp、duration、index 和 flags。
- D-04: 接收媒体通过 transceiver 级 `on_frame` 回调输出重组后的编码帧。
- D-05: 保留应用层响应 PLI 的能力，提供 transceiver 级 `on_picture_loss` 回调。
- D-06: H264 输入格式先锁定 Annex-B NALU / bytestream。
- D-07: H264 packetizer 沿用本地 KVS 路径：start code 解析，小 NALU 走 Single NALU，大 NALU 走 FU-A。
- D-08: SPS/PPS 不新增 extradata API；固定 H264 测试流应在关键帧前自带 SPS/PPS。
- D-09: H264 接收侧 `on_frame` 输出 Annex-B 重组帧。
- D-10: H264 SDP 优先继承 KVS 兼容策略：`packetization-mode=1`、`level-asymmetry-allowed=1`、`profile-level-id=42e01f`，并声明 `nack` / `nack pli`。
- D-11: Opus 与 H264 共用 `MRTC_FRAME` 和 `mrtc_transceiver_write_frame()`。
- D-12: Opus RTP timestamp 由 `MRTC_FRAME.presentation_ts` 提供，并按 48kHz clock rate 转换。
- D-13: Opus payloader/depayloader 采用直拷贝语义。
- D-14: Opus SDP/fmtp 使用 `opus/48000/2` + `minptime=10;useinbandfec=1`。
- D-15: 固定 Opus 测试源采用“长度前缀 + Opus packet 序列”fixture。
- D-16: Opus 接收验证到编码后包层：非空 payload frame，RTP timestamp 单调。
- D-17: RTCP 最小能力包含 Sender Report、Receiver Report、NACK 和 PLI；只解析不驱动行为不足。
- D-18: NACK 采用发送侧 RTP rolling buffer + NACK 触发重发，按 RTP 层统一处理。
- D-19: Phase 5 不要求完整 Chrome 自动化互通通过；必须用 C harness 证明双向 H264/Opus packetize、depacketize、SRTP media integration 和 `on_frame` 输出。
- D-20: Phase 5 完成标准采用分层验收：public API、RTP/codec unit、SRTP protect/unprotect 媒体集成、固定媒体 fixture 双向流动 harness。

### Deferred Ideas

- 完整 Chrome 自动化 E2E、浏览器页面、自动 signaling 和双向媒体断言留给 Phase 6。
- AVCC、MP4、Ogg Opus、GStreamer、FFmpeg 或其他媒体适配不进入 Phase 5 核心库范围。
- SPS/PPS 缓存重发、复杂关键帧调度、完整拥塞控制、码率调整和媒体质量优化留后续版本。
- Opus 解码验证或音频可播放性验证不作为 Phase 5 完成标准。

## Project Constraints (from AGENTS.md)

- Markdown 文档尽可能使用中文编写。[VERIFIED: `AGENTS.md`]
- 需求、路线图和当前状态以 `.planning/PROJECT.md`、`.planning/REQUIREMENTS.md`、`.planning/ROADMAP.md`、`.planning/STATE.md` 为准。[VERIFIED: `AGENTS.md`]
- AWS KVS WebRTC SDK 的剥离基线必须以本地 `reflib/kvs-webrtc-sdk` 为准，不默认使用 GitHub 上游最新版本。[VERIFIED: `AGENTS.md`]
- v1 优先 Linux x86_64、Chrome、H264、Opus、TURN relay、双向音视频和自动化 E2E 验收。[VERIFIED: `AGENTS.md`]
- 核心库不内置应用层 signaling，不做媒体采集和编码，只处理编码后媒体帧。[VERIFIED: `AGENTS.md`]

## Standard Stack

- Public API 继续放在 `include/micrortc/peer_connection.h`，使用 opaque handle、`MRTC_STATUS`、`MRTC_*` enum/struct 和 `mrtc_*` 函数风格。[VERIFIED: `include/micrortc/peer_connection.h`]
- 核心实现继续使用 C99、CMake 静态库 target `micrortc`、小型 C `main()` 测试和 CTest。[VERIFIED: `CMakeLists.txt`, `tests/peer_connection/test_peer_connection_api.c`]
- SRTP 仍通过 private `src/srtp/srtp_session.*` wrapper 接入；当前 wrapper 保存 DTLS 导出的 key/profile，还没有真实 RTP/RTCP protect/unprotect API。[VERIFIED: `src/srtp/srtp_session.c`]
- RTP/RTCP/H264/Opus 行为以本地 `reflib/kvs-webrtc-sdk` 为参考：`RtpPacket.*`、`RtpH264Payloader.*`、`RtpOpusPayloader.*`、`RtcpPacket.*`、`RtpRollingBuffer.*`、`PeerConnection/Rtp.c`、`PeerConnection/Rtcp.c`、`Retransmitter.*`。[VERIFIED: local reflib paths]

## Architecture Patterns

1. **接口先行，再挂编排。** 先扩展 public transceiver/frame/callback 合约和 private transceiver owner，再让 SDP、RTP、SRTP、RTCP 逐层消费该模型。[VERIFIED: current `src/peer_connection.c` owner pattern]
2. **RTP/codec primitives 独立于 PeerConnection。** `src/rtp/` 和 `src/rtp/codecs/` 应能被单元测试直接调用，避免把 H264/Opus packetization 藏进 PeerConnection 状态机。[VERIFIED: KVS separates `src/source/Rtp/` from `PeerConnection/Rtp.c`]
3. **SRTP wrapper 提供媒体 protect/unprotect 边界。** Phase 5 需要新增 RTP 与 RTCP 的 protect/unprotect 函数，并用当前 DTLS key direction 测试扩展覆盖。[VERIFIED: `src/srtp/srtp_session.*`]
4. **接收侧用 jitter/reassembly 到 frame 回调。** KVS 的接收路径在 jitter buffer frame-ready 后填充 `Frame` 并调用 `onFrame`；libmicrortc 应重写为 MRTC private jitter/reorder/reassembly，再触发 `MRTC_TRANSCEIVER_CALLBACKS.on_frame`。[VERIFIED: `reflib/.../PeerConnection/PeerConnection.c` around `onFrameReadyFunc`]
5. **NACK 以 RTP rolling buffer 为核心。** 发送侧在 SRTP 保护前或保护后按策略保存可重发 packet，RTCP NACK 解析后按 sequence list 查 buffer 并重发，不做 H264-only 特判。[VERIFIED: `RtpRollingBuffer.*`, `Retransmitter.c`, `PeerConnection/Rtp.c`]
6. **验收必须分层输出。** Phase 5 harness 应输出 API、H264、Opus、SRTP、RTCP、fixture flow 等明确 label，延续 Phase 4 verifier 的可读输出风格。[VERIFIED: `tests/integration/mrtc_phase4_network_verify.c`]

## Don't Hand-Roll

- 不从 GitHub 上游或浏览器样例重新找一套媒体协议实现；优先裁剪/重写本地 KVS `9eebcc4` 基线语义。[VERIFIED: project docs]
- 不实现采集器、编码器、GStreamer、FFmpeg、MP4、Ogg Opus、AVCC 转 Annex-B 等媒体适配。[VERIFIED: `05-CONTEXT.md`]
- 不把完整 Chrome 自动化 E2E 提前塞进 Phase 5；Phase 5 只允许低成本 smoke，真正自动化浏览器验收属于 Phase 6。[VERIFIED: `05-CONTEXT.md`]
- 不只生成 RTP bytes 而跳过 SRTP 和 PeerConnection/transceiver 集成；MEDIA-03 到 MEDIA-06 要有发送和接收路径证据。[VERIFIED: requirements + context]

## Common Pitfalls

- **Public ABI 泄漏 AWS/PIC 类型。** 计划和执行都要 grep `PCHAR|PVOID|BOOL|UINT64|STATUS` 等 token；`MRTC_STATUS` 是项目自有例外。[VERIFIED: Phase 4 patterns]
- **SDP 仍只回显第一条 m-line。** 当前 `mrtc_sdp_create_answer_ex()` 以 `offer->first_media_line` 生成最小 answer，Phase 5 必须变成 transceiver/media-aware 输出。[VERIFIED: `src/sdp.c`]
- **SRTP session “ready” 但不保护媒体。** 当前 `mrtc_srtp_session_init_from_dtls()` 只保存 key；计划必须要求 RTP/RTCP protect/unprotect API 和测试。[VERIFIED: `src/srtp/srtp_session.c`]
- **H264 fixture 没有 SPS/PPS 或 start code。** 因 D-06/D-08 锁定 Annex-B，测试必须检查 start code 和关键帧前 SPS/PPS 存在。
- **Opus timestamp 被固定 20ms 猜测。** D-12 要求用 `MRTC_FRAME.presentation_ts` 转 48kHz RTP timestamp。
- **NACK 只解析不重发。** D-17/D-18 明确要求 rolling buffer + NACK 触发重发。
- **Phase 5 验收误报为 Chrome 互通完成。** 文档和 STATE/ROADMAP 不得把 Phase 6 的 E2E-06/07/08 标为完成。

## Implementation Recommendations

| Area | Recommendation | Confidence |
|------|----------------|------------|
| Public API | 新增 `MRTC_RTP_TRANSCEIVER_HANDLE`、`MRTC_MEDIA_KIND`、`MRTC_CODEC`、`MRTC_RTP_TRANSCEIVER_DIRECTION`、`MRTC_FRAME`、`MRTC_TRANSCEIVER_INIT`、`MRTC_TRANSCEIVER_CALLBACKS`，函数命名为 `mrtc_peer_connection_add_transceiver()`、`mrtc_transceiver_set_callbacks()`、`mrtc_transceiver_write_frame()`。 | HIGH |
| H264 | 新建 private `src/rtp/codecs/h264.*`，覆盖 Annex-B start code、Single NALU、FU-A packetize、FU-A depay 到 Annex-B。 | HIGH |
| Opus | 新建 private `src/rtp/codecs/opus.*`，一个 Opus packet 直拷贝为一个 RTP payload，depay 输出非空 packet。 | HIGH |
| RTP packet | 新建 private `src/rtp/rtp_packet.*`，支持 RTP v2 header parse/serialize、sequence、timestamp、payload type、SSRC、marker。 | HIGH |
| RTCP | 新建 private `src/rtcp/rtcp_packet.*`、`src/rtcp/rtp_rolling_buffer.*`、`src/rtcp/retransmitter.*`，先覆盖 SR/RR/NACK/PLI。 | HIGH |
| Integration | 在 `src/peer_connection.c` 中持有 transceiver list；可把媒体逻辑拆到 `src/media/media_transceiver.*` 以控制文件大小。 | MEDIUM |
| Fixture harness | 新增 `tests/media/` 与 `tests/integration/mrtc_phase5_media_verify.c`，固定 H264 Annex-B 和 Opus length-prefixed packet fixture。 | HIGH |

## Validation Architecture

Phase 5 的验证必须分四层，任一层缺失都不应标记 Phase 5 planned/executed 完成：

1. **Public API layer:** `mrtc_peer_connection_add_transceiver()`、`mrtc_transceiver_set_callbacks()`、`mrtc_transceiver_write_frame()` 参数校验、handle 生命周期、public header ABI grep。
2. **RTP/codec unit layer:** H264 Annex-B packetize/depacketize、FU-A 拆分重组、Opus direct payload、RTP parse/serialize、RTCP NACK/PLI/SR/RR parse/generate。
3. **SRTP media integration layer:** RTP 与 RTCP protect/unprotect wrapper 被调用；发送路径保存 rolling buffer；接收路径 unprotect 后进入 depay/on_frame。
4. **Fixed fixture flow layer:** `mrtc_phase5_media_verify` 使用固定 H264 Annex-B 和 Opus length-prefixed fixture，输出 `h264 send ok`、`h264 receive ok`、`opus send ok`、`opus receive ok`、`rtcp nack retransmit ok`、`pli callback ok` 等 label。

## Open Questions For Execution

- `MRTC_FRAME.presentation_ts` 的单位建议命名为 `presentation_ts_100ns`，以贴近 KVS 时间换算并减少歧义；如果执行阶段选择纳秒或微秒，必须同步测试和文档。
- 是否实现真实 libsrtp protect/unprotect 取决于本地依赖可用性；若依赖缺失，wrapper 仍应提供 deterministic fallback test，但 Phase 5 real media integration 在有 libsrtp 环境下必须跑通。

## Research Summary

Phase 5 适合拆成 5 个计划：API/SDP 合约、RTP/H264/Opus primitives、SRTP + PeerConnection media integration、RTCP/NACK/PLI、fixture/harness/docs 收口。这个拆分让 API-04、PROTO-04 和 MEDIA-01 到 MEDIA-07 都有可执行任务，并保持 Phase 6 浏览器自动化边界清晰。

## RESEARCH COMPLETE
