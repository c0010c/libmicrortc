# Phase 5: H264/Opus 媒体路径 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-13T12:48:25+08:00
**Phase:** 5-H264/Opus 媒体路径
**Areas discussed:** 媒体 Public API 形态, H264 视频路径边界, Opus 音频路径边界, RTCP/NACK 与阶段验收深度

---

## 媒体 Public API 形态

| Question | Option | Selected |
|----------|--------|----------|
| Phase 5 的媒体 API 应该更偏哪种风格？ | AWS 薄裁剪 + MRTC_* 命名 | ✓ |
| Phase 5 的媒体 API 应该更偏哪种风格？ | 更早转向 libmicrortc 原生 API | |
| Phase 5 的媒体 API 应该更偏哪种风格？ | 只暴露最小媒体 API | |
| transceiver 创建接口要多完整？ | Kind + codec + direction + 回调，返回 opaque transceiver handle | ✓ |
| transceiver 创建接口要多完整？ | 只按 codec 添加，direction 默认 sendrecv | |
| transceiver 创建接口要多完整？ | 拆成 add_video_transceiver / add_audio_transceiver | |
| writeFrame / MRTC_FRAME 应该包含哪些字段？ | 数据 + 长度 + 时间戳 + flags | |
| writeFrame / MRTC_FRAME 应该包含哪些字段？ | 贴近 AWS Frame，字段更完整 | ✓ |
| writeFrame / MRTC_FRAME 应该包含哪些字段？ | 只传 bytes，让库内部生成时间戳 | |
| 接收媒体怎么回给应用层？ | transceiver 级 on_frame 回调 | ✓ |
| 接收媒体怎么回给应用层？ | PeerConnection 级统一 on_media_frame 回调 | |
| 接收媒体怎么回给应用层？ | 先只暴露 RTP packet 回调 | |

**User's choice:** AWS 薄裁剪 + `MRTC_*` 命名；kind/codec/direction/callbacks + opaque transceiver handle；贴近 AWS `Frame`；transceiver 级 `on_frame`。
**Notes:** 用户希望延续 AWS 使用模型以降低迁移风险，但 public 类型仍必须是 libmicrortc 自有边界。

---

## H264 视频路径边界

| Question | Option | Selected |
|----------|--------|----------|
| Phase 5 的 H264 输入格式先锁哪一种？ | Annex-B NALU / bytestream | ✓ |
| Phase 5 的 H264 输入格式先锁哪一种？ | AVCC length-prefixed | |
| Phase 5 的 H264 输入格式先锁哪一种？ | 两者都支持 | |
| SPS/PPS 怎么处理？ | 从 Annex-B 流中解析并缓存，关键帧前按需发送 | |
| SPS/PPS 怎么处理？ | 应用层必须显式提供 codec extra data | |
| SPS/PPS 怎么处理？ | Phase 5 只接受已经包含 SPS/PPS 的关键帧样本，不做缓存策略 | |
| SPS/PPS 怎么处理？ | 参考本地 KVS 实现后锁定：固定测试流自带 SPS/PPS，不把缓存重发作为必须项 | ✓ |
| 接收侧输出到 on_frame 时，H264 应该是什么形态？ | Annex-B 重组帧 | ✓ |
| 接收侧输出到 on_frame 时，H264 应该是什么形态？ | 原始 NALU 列表结构 | |
| 接收侧输出到 on_frame 时，H264 应该是什么形态？ | 保持 RTP payload，不做 H264 depacketize | |
| H264 的 PLI/关键帧处理做到哪一层？ | 暴露 on_picture_loss 回调，应用层负责后续关键帧 | ✓ |
| H264 的 PLI/关键帧处理做到哪一层？ | 只统计/记录 PLI，不暴露 public callback | |
| H264 的 PLI/关键帧处理做到哪一层？ | Phase 5 不处理 PLI，留到 Phase 6 | |

**User's choice:** Annex-B 输入与输出，沿用本地 KVS H264 payloader/depayloader 语义，暴露 PLI 回调。
**Notes:** 用户追问了 Kinesis 的实现。根据本地 `reflib/kvs-webrtc-sdk`，KVS H264 payloader 解析 Annex-B start code，Single NALU/FU-A 发送；未发现 payloader 层主动缓存并重发 SPS/PPS。因此锁定固定测试流自带 SPS/PPS。

---

## Opus 音频路径边界

| Question | Option | Selected |
|----------|--------|----------|
| Opus 帧 API 要多通用？ | 使用同一个 MRTC_FRAME，由 codec/transceiver 决定 Opus 语义 | |
| Opus 帧 API 要多通用？ | 为 Opus 单独定义轻量音频 frame 结构 | |
| Opus 帧 API 要多通用？ | 应用层只给 bytes，库按固定 20ms 生成 timestamp | |
| Opus 帧 API 要多通用？ | 参考本地 KVS 实现后锁定：共用 MRTC_FRAME，Opus packet 直送 | ✓ |
| Opus SDP/fmtp 怎么处理？ | 沿用 KVS 默认：opus/48000/2 + minptime=10;useinbandfec=1 | ✓ |
| Opus SDP/fmtp 怎么处理？ | 只声明 opus/48000/2，不写 fmtp | |
| Opus SDP/fmtp 怎么处理？ | 允许应用层配置 fmtp | |
| 固定测试 Opus 文件用什么边界？ | 测试 fixture 存长度前缀 + Opus packet 序列 | ✓ |
| 固定测试 Opus 文件用什么边界？ | 直接使用 Ogg Opus 文件 | |
| 固定测试 Opus 文件用什么边界？ | 生成硬编码小 Opus packet fixture | |
| Opus 接收侧验证到什么层？ | 断言收到非空 Opus payload frame + RTP timestamp 单调 | ✓ |
| Opus 接收侧验证到什么层？ | 额外用外部工具解码验证音频可播放 | |
| Opus 接收侧验证到什么层？ | 只断言 RTP packet 到达，不要求 on_frame 输出 | |

**User's choice:** Opus 沿用 KVS 模型：共用 frame/write API，Opus packet 直拷贝，SDP 用 KVS 默认，fixture 用长度前缀 packet 序列。
**Notes:** 用户追问了 Kinesis 的实现。根据本地 KVS，Opus payloader/depayloader 基本直拷贝，timestamp 来自 `Frame.presentationTs` 并按 48kHz 转 RTP。

---

## RTCP/NACK 与阶段验收深度

| Question | Option | Selected |
|----------|--------|----------|
| RTCP 最小能力要包含哪些？ | SR/RR + NACK + PLI | ✓ |
| RTCP 最小能力要包含哪些？ | 只做 NACK + PLI | |
| RTCP 最小能力要包含哪些？ | 只解析 RTCP，不实际驱动重传/回调 | |
| NACK 重传缓冲做到哪一层？ | 发送侧 RTP rolling buffer + NACK 触发重发 | ✓ |
| NACK 重传缓冲做到哪一层？ | 只记录 NACK，不重发 | |
| NACK 重传缓冲做到哪一层？ | 只对 H264 做重传，Opus 不做 | |
| Phase 5 要不要做 Chrome 浏览器互通？ | 不要求完整 Chrome 自动化，但可提供本地/半自动浏览器 smoke | ✓ |
| Phase 5 要不要做 Chrome 浏览器互通？ | Phase 5 就要求 Chrome 双向媒体互通通过 | |
| Phase 5 要不要做 Chrome 浏览器互通？ | 完全不碰浏览器，只做单元测试 | |
| Phase 5 的完成标准怎么切分？ | 分层验收：API + RTP/codec unit + SRTP media integration + fixture harness | ✓ |
| Phase 5 的完成标准怎么切分？ | 以单个 phase5 verifier 命令为主 | |
| Phase 5 的完成标准怎么切分？ | 只要求 CTest 全部通过 | |

**User's choice:** RTCP 包含 SR/RR/NACK/PLI；NACK 使用 rolling buffer 触发重传；Phase 5 用分层 C harness 验收，完整 Chrome 自动化留 Phase 6。
**Notes:** 半自动 Chrome smoke 可作为辅助，但不能替代 Phase 5 的分层验收。

---

## the agent's Discretion

- 具体函数名、字段名、fixture 长度前缀宽度/endian、测试 executable 拆分和是否提供汇总 verifier 命令由 planner 按现有代码模式决定。
- planner 可以决定是否加入低成本半自动 Chrome smoke，但 Phase 5 不以完整 Chrome 自动化为完成标准。

## Deferred Ideas

- 完整 Chrome 自动化 E2E、浏览器页面、自动 signaling 和 TURN relay 媒体收口留给 Phase 6。
- AVCC、Ogg Opus、MP4、GStreamer、FFmpeg、媒体解码和更复杂媒体质量优化留给后续阶段。
