# 第 5 阶段：RTP/RTCP 媒体平面 - 上下文

**Gathered:** 2026-05-10T23:00:00+08:00
**Status:** 准备进入规划

<domain>
## 阶段边界

本阶段交付 1 路 Opus 音频和 1 路 H264 视频的 RTP/RTCP 媒体平面：用户提交编码后的 Opus frame 或 H264 access unit，库负责 RTP packetize、timestamp、sequence、SRTP/SRTCP protect、datagram 输出；接收方向库负责 RTP/RTCP demux 后的 SRTP/SRTCP unprotect、depacketize、H264 access unit 重组和 typed media frame 回调。

本阶段不处理音视频编解码，不创建 socket，不创建线程，不做运行期动态内存增长，不实现 NACK 重传或拥塞控制闭环，不扩展到多路音视频，也不把真实 Chrome 页面验收提前拉入第 5 阶段。Chrome 端到端验收仍属于第 6 阶段。

</domain>

<decisions>
## 实现决策

### 媒体帧 API 形状

- **D-01:** 第 5 阶段采用统一媒体帧 API，而不是分别暴露 `send_opus_frame` 和 `send_h264_access_unit` 两套入口。推荐形状为类似 `rtc_peer_connection_send_media_frame(pc, &frame)` 的单一发送入口。
- **D-02:** 统一 API 必须是 typed generic API：`rtc_media_frame_t` 或等价结构中必须包含稳定媒体类型枚举，v1 只接受 `RTC_MEDIA_KIND_AUDIO_OPUS` 和 `RTC_MEDIA_KIND_VIDEO_H264` 或等价命名。
- **D-03:** 非法 media kind、与 codec 不匹配的 metadata、空 frame、超出固定容量的 frame 必须返回稳定错误，并通过 observer error、trace 和 counter 暴露原因。
- **D-04:** 接收方向应新增 typed media observer 形状，回调中不仅包含 `data` 和 `data_len`，还应包含媒体类型和最小 metadata。现有 `observer.on_media_frame(data,len)` 太瘦，不应作为第 5 阶段主要媒体出口；planner 可决定是否保留兼容包装或演进 vtable。

### Opus 与 H264 RTP 打包边界

- **D-05:** 用户提交完整编码帧，库负责 RTP payload format。Opus 发送方向一次提交一个 Opus frame；H264 发送方向一次提交一个 H264 access unit。
- **D-06:** Opus 首版按一个 frame 到一个 RTP packet 的基本路径实现；不做 Opus repacketization、多帧聚合、FEC/DTX 策略或音频编码控制。
- **D-07:** H264 发送方向由库解析 access unit 中的 NALU 边界。能放入单个 RTP payload 的 NALU 使用 single NALU packet；超过 payload 容量的 NALU 使用 FU-A 分片。
- **D-08:** H264 接收方向必须支持 single NALU 和 FU-A 重组为 access unit；有限支持 STAP-A 接收，目标是能拆出其中 NALU 并纳入 access unit 输出。
- **D-09:** RTP payload/MTU 上限不应作为每次 send 调用参数传入；应由 create-time 配置、`limits.rtp` 或 planner 选择的等价固定容量决定。超过固定 packetize 或 reassembly 容量时返回或上报容量错误，不动态扩容。
- **D-10:** 库负责 RTP timestamp、sequence number 和 marker bit 等基础 RTP 字段。用户可通过 typed frame metadata 提供 timestamp 或采样时间输入，具体字段命名由 planner 决定，但必须保持 Opus/H264 语义可诊断。

### RTCP 与反馈语义

- **D-11:** 库自动维护基础 RTCP：Sender Report、Receiver Report 和 SDES 由库基于收发统计和固定定时器/调度策略生成，不要求用户手动组包。
- **D-12:** PLI 采用用户显式请求模型。应提供类似 `rtc_peer_connection_request_keyframe` 或 typed media feedback API 的公共入口，由用户在需要关键帧时触发，库负责生成并通过 SRTCP protect 后输出 datagram。
- **D-13:** 收到远端 PLI 时通过 observer/trace/counter 上报给用户，用户负责让编码器产生关键帧；库不处理编码器控制。
- **D-14:** NACK 首版只解析并上报，不触发 RTP 重传，不引入重传缓存调度语义。NACK 上报至少应能表达媒体类型或 SSRC、PID/BLP 或等价丢包范围、远端请求时间点和“不重传”的语义。
- **D-15:** RTCP 错误和未自动修复事件通过 observer error、trace reason 和 counters 表达。公共 `rtc_status_t` 保持粗粒度稳定，细节不要扩散成大量 public status。

### Media / Network executor 分层

- **D-16:** 第 5 阶段采用严格 executor 分层。媒体帧输入输出属于 `RTC_EXECUTOR_MEDIA`；datagram、ICE、DTLS、SRTP/SRTCP protect/unprotect 和 `observer.on_datagram` 属于 `RTC_EXECUTOR_NETWORK`。
- **D-17:** 发送方向：用户在 media executor 调用统一媒体发送 API；库在 media executor 完成 Opus/H264 packetize、RTP timestamp/sequence 处理，然后投递到 network executor 做 SRTP/SRTCP protect 并通过 `observer.on_datagram` 输出。
- **D-18:** 接收方向：用户在 network executor 调用 `rtc_peer_connection_receive_datagram`；库 demux RTP/RTCP 并完成 SRTP/SRTCP unprotect 后，投递到 media executor 做 depacketize/reassembly，最后通过 typed media observer 输出 frame 或 feedback 事件。
- **D-19:** 跨 executor 投递必须使用固定内存队列/槽或等价固定容量结构。post 失败、队列满、packet/frame 槽不足必须有稳定错误、trace reason 和 counter；不得临时 malloc 或保存调用方 buffer 指针。
- **D-20:** 用户无论把三个 executor 映射到同一线程、三个线程还是 superloop，都应看到稳定亲和语义：媒体帧回调在 media，UDP datagram 回调在 network，counters 快照仍按既有 signaling 亲和。

### 智能体的自由裁量

用户未锁定具体 API 函数名、`rtc_media_frame_t` 字段全集、media kind 枚举命名、RTP/RTCP 内部文件拆分、RTCP 周期、timestamp metadata 的精确类型、trace event 常量全集、counter 字段命名或测试 fixture 命名。planner 可以在不突破上述决策的前提下决定这些实现细节，但必须保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发，并避免 GPL/LGPL 依赖。

</decisions>

<canonical_refs>
## 权威引用

**下游智能体在规划或实现前必须阅读这些文档。**

### 项目范围与需求

- `.planning/PROJECT.md`：项目价值、首版边界、固定内存/无线程/用户 UDP/socket 边界、媒体边界、NACK 不重传和 1 音频 + 1 视频限制。
- `.planning/REQUIREMENTS.md`：第 5 阶段覆盖的 `RTP-01`、`RTP-02`、`RTP-03`、`RTP-04`、`RTP-05`、`RTCP-01`、`RTCP-02`、`RTCP-03`、`RTCP-04`、`OBS-04` 需求及需求追踪。
- `.planning/ROADMAP.md`：第 5 阶段目标、成功标准和第 6 阶段 Chrome 端到端验收边界。
- `.planning/STATE.md`：当前项目状态和工作流配置。

### 已锁定的上游边界

- `.planning/phases/01-core-skeleton-boundary-contract/01-CONTEXT.md`：公共 API、固定 arena、executor 亲和、observer、trace、counter、测试策略和 allocator 防线。
- `.planning/phases/02-sdp-jsep-offer-answer/02-CONTEXT.md`：Chrome 1v1 SDP/JSEP、Opus/H264 最小画像、`sendrecv`/`recvonly` 和 offer/answer 状态机。
- `.planning/phases/03-ice-stun-datagram-network-layer/03-CONTEXT.md`：显式 network API、STUN/DTLS/RTP/RTCP datagram demux、`observer.on_datagram` 输出模型和 buffer 生命周期。
- `.planning/phases/04-dtls-srtp-secure-transport/04-CONTEXT.md`：单一 `security_backend`、ICE connected 后 DTLS、内部 SRTP/SRTCP wrapper、`srtp.ready`、protect/unprotect 失败语义和第 5 阶段调用边界。
- `docs/000-设计边界记录.md`：纯 C、固定内存、无线程、用户负责 UDP/socket、编码帧级媒体 API、H264/Opus 边界、RTCP/NACK 语义和依赖策略。
- `docs/API-执行器与内存契约.md`：executor 亲和、arena/limits、observer/trace/counter、datagram buffer 生命周期、第 3/4 阶段网络与安全语义。

### 现有代码入口

- `include/rtc/peer_connection.h`：当前 public API 入口，需要新增统一媒体发送、PLI 请求或等价媒体 feedback API。
- `include/rtc/observer.h`：已有 `on_media_frame(data,len)`、`on_datagram`、`on_state`、`on_error`、`on_trace`；第 5 阶段需要演进 typed media observer 或新增等价回调。
- `include/rtc/limits.h`：已有 `limits.rtp.max_packet_cache` 和 `limits.rtcp.max_reports`，需要扩展或解释 RTP packet queue、H264 reassembly、RTCP report、跨 executor 槽和 payload 容量。
- `include/rtc/counters.h`：已有 `rtp`/`rtcp` 尚未细化，当前有 `srtp`、`dtls`、`net` counters 模式可复用。
- `include/rtc/security.h`：安全 backend 已提供 `srtp_protect_rtp`、`srtp_unprotect_rtp`、`srtcp_protect`、`srtcp_unprotect`。
- `src/api/peer_connection.c`：`receive_datagram` 当前已 demux RTP/RTCP 但只返回 OK；第 5 阶段需要接入 RTP/RTCP 处理和跨 executor 投递。
- `src/net/demux.c`：已按首字节分类 STUN、DTLS、RTP、RTCP，是第 5 阶段 RTP/RTCP 输入路由基础。
- `src/srtp/srtp.c` 和 `src/srtp/srtp.h`：第 4 阶段内部 SRTP/SRTCP wrapper，第 5 阶段发送和接收路径必须通过这些 wrapper。
- `tests/test_security.c`：已有 deterministic backend 和 SRTP/SRTCP wrapper 测试模式，可作为媒体路径安全集成测试参考。

</canonical_refs>

<code_context>
## 既有代码洞察

### 可复用资产

- `rtc_peer_connection_receive_datagram` 已在 network executor 上完成 STUN/DTLS/RTP/RTCP demux；RTP/RTCP 分支是第 5 阶段自然接入点。
- `src/srtp` 内部 wrapper 已封装 SRTP/SRTCP protect/unprotect、`srtp.ready` 检查、失败 counter、observer error 和 trace reason；媒体平面不应直接调用 backend vtable。
- `observer.on_datagram` 已用于 STUN 和 DTLS outgoing datagram，回调后 buffer 失效；受保护 RTP/RTCP datagram 应复用该输出模型。
- `rtc_peer_connection_counters_t` 已按子系统分组，是新增 RTP/RTCP/media counters 的自然位置。
- 纯 C 自研 test runner 和 deterministic security backend 已存在，可用于无真实 OpenSSL/libsrtp 的 packetize/protect/depacketize 测试。

### 既有模式

- Signaling API 在 `RTC_EXECUTOR_SIGNALING`，network API 在 `RTC_EXECUTOR_NETWORK`；第 5 阶段新增媒体 API 必须在 `RTC_EXECUTOR_MEDIA` 上验证亲和。
- 创建成功后运行期不得动态增长；RTP packet cache、H264 reassembly buffer、RTCP report slots、cross-executor queue slots、NACK/PLI event slots 必须来自 create-time arena/limits。
- 公共 status 保持稳定粗粒度；细节通过 observer detail code、trace reason 和 counter 暴露。
- 用户负责 UDP/socket 发送和接收；库只通过 `observer.on_datagram` 输出待发送 payload，只在调用期间读取输入 datagram。
- Markdown 文档使用中文；技术标识符、API 名称、协议字段、trace event 和需求 ID 可以保留英文。

### 集成点

- 新增代码应围绕 `src/rtp`、`src/rtcp`、`src/media` 或等价子系统拆分，避免把 packetize/depacketize 和 RTCP 状态机塞进 `src/api/peer_connection.c`。
- `rtc_peer_connection_t` 内部需要保存 RTP sequence/timestamp/SSRC、media kind 状态、H264 reassembly 状态、RTCP sender/receiver stats、PLI/NACK 事件、跨 executor 队列槽和媒体 counters。
- SDP/JSEP 层已固定 Opus/H264 Chrome 画像；第 5 阶段应匹配现有 payload type/codec profile，而不是引入通用 codec 协商。
- 第 6 阶段 Chrome 页面和信令示例会消费第 5 阶段媒体 API，因此第 5 阶段 public API 和 observer metadata 需要足够清晰，不能只靠内部测试语义。

</code_context>

<specifics>
## 具体想法

- 用户先询问是否可以统一 API，最终锁定“统一 API + typed frame 结构”，而不是完全分裂的 Opus/H264 函数，也不是裸 `data,len`。
- 用户接受全部四个灰区的推荐方案：typed generic media API、完整编码帧输入、库负责 RTP payload format、基础 RTCP 自动维护、显式 PLI、NACK 只上报、严格 media/network executor 分层。
- 本阶段优先让用户集成体验清晰：用户面向编码帧和媒体反馈工作，不直接组 RTP/RTCP，也不直接碰 SRTP/SRTCP backend。

</specifics>

<deferred>
## 延后事项

- NACK 触发 RTP 重传：v1 不支持，未来 `V2-RTP-01` 再评估。
- 拥塞控制或发送节奏控制闭环：v1 不支持，未来 `V2-RTP-02` 再评估。
- 多路音频或多路视频：v1 固定单个 `PeerConnection` 最多 1 路音频 + 1 路视频，未来 `V2-MED-01` 再评估。
- Opus repacketization、多帧聚合、FEC/DTX 策略：不属于第 5 阶段首版边界。
- 通用 codec 协商、更多 codec、完整 RTP header extension 生态：不属于第 5 阶段首版边界。
- Chrome 端到端页面和信令示例验收：留到第 6 阶段。

</deferred>

---

*阶段：5-RTP/RTCP 媒体平面*
*上下文收集时间：2026-05-10T23:00:00+08:00*
