# 第 5 阶段：RTP/RTCP 媒体平面 - Research

**Researched:** 2026-05-10  
**Domain:** WebRTC RTP/RTCP、Opus/H264 RTP payload format、SRTP/SRTCP 集成、固定内存媒体队列  
**Confidence:** HIGH

<user_constraints>
## User Constraints

### Locked Decisions

- **D-01..D-04:** 采用统一 typed media frame API；发送入口类似 `rtc_peer_connection_send_media_frame(pc, &frame)`，v1 只接受 Opus audio 和 H264 video；接收方向需要 typed media observer 或等价回调。
- **D-05..D-10:** 用户提交完整编码帧；Opus 一帧一 RTP packet；H264 access unit 由库解析 NALU，发送 single NALU 或 FU-A，接收支持 single NALU、FU-A 重组和有限 STAP-A；MTU/payload/reassembly 容量来自 create-time limits。
- **D-11..D-15:** SR/RR/SDES 由库自动维护；PLI 使用用户显式请求入口；收到 PLI/NACK 只上报给用户；NACK 不触发重传；错误细节通过 observer error、trace reason 和 counters 表达。
- **D-16..D-20:** media frame 输入输出在 `RTC_EXECUTOR_MEDIA`；datagram、SRTP/SRTCP protect/unprotect 和 `observer.on_datagram` 在 `RTC_EXECUTOR_NETWORK`；跨 executor 投递必须使用固定槽，不保存调用方 buffer 指针。

### Project Boundaries

- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发。
- 不引入 GPL/LGPL 依赖；第 5 阶段不要求真实 Chrome 页面或信令示例，Chrome E2E 留到第 6 阶段。
- 公共 `rtc_status_t` 保持粗粒度；新增细节优先用 detail code、trace reason 和 counter。
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| RTP-01 | 用户提交 Opus frame，库负责 RTP packetize、时间戳、序列号和 SRTP 保护。 | Opus RTP payload 可按一个 Opus frame 映射一个 RTP packet；本项目首版不做 repacketization，因此发送路径可以先做最小 payload wrapper。 |
| RTP-02 | 用户可以接收从 RTP depacketize 得到的 Opus frame。 | 接收方向在 SRTP unprotect 成功后按 payload type/SSRC/marker/timestamp 解析，并把 payload 作为 typed Opus frame 输出。 |
| RTP-03 | 用户提交 H264 access unit，库支持 single NALU 和 FU-A 发送。 | H264 over RTP 常见首版路径是解析 Annex B 或长度前缀 NALU，能放入 payload 的走 single NALU，超限走 FU-A。 |
| RTP-04 | 用户接收 H264 access unit，接收方向有限支持 STAP-A。 | 需要固定 reassembly buffer 和 NALU table；FU-A 丢片或乱序必须丢弃当前 AU 并可观测。STAP-A 只拆出内部 NALU，不追求完整泛化。 |
| RTP-05 | 单个 `PeerConnection` 最多支持 1 路音频和 1 路视频。 | `rtc_media_kind_t` 和每 kind 一套 sender/receiver state 足够；超出或未知 kind 返回诊断错误。 |
| RTCP-01 | 支持 RTCP Sender Report 和 Receiver Report。 | 维护 per-kind packet/octet/loss/jitter/timestamp stats，network executor 定时生成 SR/RR。 |
| RTCP-02 | 支持 RTCP SDES。 | 首版可输出 CNAME item；CNAME 来源可为 create-time fixed buffer 或稳定默认值。 |
| RTCP-03 | 支持发送和接收 PLI。 | 发送由用户显式请求触发；接收上报 observer feedback，用户控制编码器出关键帧。 |
| RTCP-04 | 解析 NACK 并通过 observer 上报，不执行重传。 | 解析 PID/BLP 或展开丢包范围；必须 trace/counter 明确 `nack_no_retransmit`。 |
| OBS-04 | NACK、PLI、ICE/DTLS/SRTP 等未自动修复事件都有可观测输出。 | 第 5 阶段新增 media/rtp/rtcp counters、trace event 和 detail code，复用第 4 阶段 SRTP 失败语义。 |
</phase_requirements>

## Summary

第 5 阶段应按“公共媒体契约 -> RTP 发送 -> RTP 接收 -> RTCP 基础 -> 反馈与可观测性 -> 文档收口”的顺序规划。现有代码已经提供三项上游基础：`rtc_peer_connection_receive_datagram` 的 RTP/RTCP demux 分支、第 4 阶段内部 `rtc_srtp_protect_*` / `rtc_srtp_unprotect_*` wrapper、以及 observer/trace/counter 的可诊断模式。

核心风险有三类。第一，跨 executor 不能保存调用方 buffer 指针，因此 media 到 network、network 到 media 的数据都必须拷贝到 create-time 固定槽。第二，H264 reassembly 容易在容量、乱序、丢片和 STAP-A 边界上变成隐式动态行为，计划必须把 buffer、NALU index、packet cache 和错误 drop 语义写清楚。第三，RTCP NACK/PLI 是反馈而不是自动修复机制；尤其 NACK 不得引入重传缓存或发送节奏控制。

**Primary recommendation:** 建立 `include/rtc/media.h` 作为 typed media API 与 feedback 事件契约；新增 `src/rtp`、`src/rtcp`、`src/media` 子系统；`src/api/peer_connection.c` 只做亲和检查和路由；所有受保护 datagram 输出继续使用既有 `observer.on_datagram`。

## Architecture Patterns

### Data Flow

```text
用户编码器
  |
  v
rtc_peer_connection_send_media_frame(pc, frame)  [media executor]
  |
  +-- validate typed frame / capacity / media kind
  +-- Opus packetize 或 H264 NALU split/FU-A
  +-- 写入固定 media->network 槽
  |
  v
network executor task
  |
  +-- rtc_srtp_protect_rtp()
  +-- observer.on_datagram(protected_rtp)

用户 UDP receive
  |
  v
rtc_peer_connection_receive_datagram(pc, datagram) [network executor]
  |
  +-- net demux RTP/RTCP
  +-- rtc_srtp_unprotect_rtp/rtcp()
  +-- RTP/RTCP parse 后写入固定 network->media 槽
  |
  v
media executor task
  |
  +-- Opus depacketize 或 H264 reassembly
  +-- typed media frame / feedback observer
```

### Recommended Project Structure

```text
include/rtc/
├── media.h             # typed media frame、kind、metadata、feedback API
├── observer.h          # typed media frame / feedback observer callback
├── limits.h            # RTP packet、payload、reassembly、cross-executor 槽容量
├── counters.h          # rtp/rtcp/media counters
└── trace.h             # rtp/rtcp/media trace events and fields
src/
├── media/              # public API implementation, fixed queues, dispatch helpers
├── rtp/                # RTP header, Opus/H264 packetize/depacketize
├── rtcp/               # SR/RR/SDES/PLI/NACK parse/write and stats
└── api/                # thin peer_connection integration
tests/
├── test_media_api.c
├── test_rtp.c
└── test_rtcp.c
```

## Implementation Notes

- Opus: v1 可固定 payload type 使用当前 SDP profile 中的 Opus PT；每次 `send_media_frame` 生成一个 RTP packet，marker bit 可为 0，timestamp 按 48 kHz 音频 clock 或用户 metadata 增量。
- H264: 发送路径需要支持 Annex B start code；若现有用户 API 不声明 length-prefixed，首版可只支持 Annex B 并对其他格式返回 `RTC_STATUS_UNSUPPORTED`，但计划应让文档明确。
- RTP sequence 和 timestamp: 每 kind 保存独立 sequence、SSRC、last timestamp、packet/octet counters；不要按每 m-line 动态分配。
- RTCP: 首版 SR/RR/SDES 生成可由 network timer 或显式 internal tick 驱动；测试中应避免真实时间依赖，用 test executor 触发 timer task。
- Feedback: 新增 callback 比复用 `on_media_frame(data,len)` 更清晰。兼容策略可以保留旧 callback 为空路径，但第 5 阶段的验收应以 typed callback 为准。
- Capacity: `RTC_STATUS_CAPACITY_PACKET_CACHE` 可继续用于 RTP packet/output slot 不足；若需要更细粒度，新增 capacity resource 但不要扩展大量 public status。

## Validation Architecture

第 5 阶段适合使用纯 C unit/integration tests + CTest。每个计划都应运行：

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

关键采样：

- API/limits/counters 任务：grep public header 中的 `rtc_media_frame_t`、`RTC_MEDIA_KIND_AUDIO_OPUS`、typed observer、RTP/RTCP counters。
- RTP 发送任务：deterministic security backend 断言 `srtp_protect_rtp` 被调用且 `observer.on_datagram` 收到带 auth tag 的 datagram；protect 失败时 datagram 计数不增加。
- RTP 接收任务：构造受保护 RTP 后 unprotect，断言 Opus/H264 typed frame 输出；unprotect/replay 失败时没有媒体回调。
- RTCP 任务：构造 SR/RR/SDES/PLI/NACK bytes，断言 parse/write、counters、trace reason 和 observer feedback。
- 文档收口：`docs/API-执行器与内存契约.md` 和 `docs/000-设计边界记录.md` 明确 NACK 不重传、用户负责编码器 keyframe、Chrome E2E 留到第 6 阶段。

## Planning Recommendation

| Plan | Focus | Why |
|------|-------|-----|
| 05-01 | typed media API、observer、limits、counters、trace、固定队列模型 | 先锁公共契约和容量，避免后续 RTP/RTCP 任务各自发明接口。 |
| 05-02 | RTP header、Opus/H264 发送 packetize、SRTP protect 和 datagram 输出 | 先打通编码帧到受保护 RTP datagram 的发送主链路。 |
| 05-03 | RTP 接收、SRTP unprotect、Opus/H264 depacketize/reassembly、typed frame 输出 | 对称打通 UDP datagram 到编码帧回调的接收主链路。 |
| 05-04 | RTCP SR/RR/SDES stats 与自动生成/解析 | 建立基础 RTCP 维护能力。 |
| 05-05 | PLI/NACK feedback API、parse/write、无重传语义和 OBS-04 可观测性 | 单独隔离反馈语义，避免 NACK 扩成重传缓存。 |
| 05-06 | 文档、需求追踪、阶段状态和 API 示例收口 | 让第 6 阶段 Chrome E2E 可以消费清晰媒体 API。 |

## Out of Scope

- NACK 触发 RTP 重传、发送 pacing、拥塞控制、RTX/FEC、音视频编解码。
- 多路音频/视频、多 BUNDLE transport、多 codec 协商。
- 真实 Chrome 页面、信令示例和端到端音视频验收。

## Research Complete

本研究足以支撑第 5 阶段规划：范围、上游集成点、容量边界、测试策略和验收风险均已明确。
