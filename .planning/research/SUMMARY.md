# Project Research Summary

**Project:** libmicrortc  
**Domain:** 纯 C、Sans-I/O、资源受限设备 WebRTC 媒体传输库  
**Researched:** 2026-05-12  
**Confidence:** HIGH

## Executive Summary

libmicrortc 应被当作一个生产级 WebRTC 媒体传输内核来规划，而不是一个浏览器 PeerConnection API 复刻或 demo SDK。专家级实现的共同点是按协议边界拆分：SDP/JSEP、ICE/STUN/TURN、DTLS-SRTP、SRTP/SRTCP、RTP/RTCP、H264/Opus payload 各自可测试，再由单个 `rtc_context` 和 `rtc_pc` 聚合成可观测的连接状态。v1 目标应严格聚焦 Chrome 1v1 H264/Opus 互通、Linux 首个平台、固定内存预算、单线程多 PeerConnection 调度。

推荐路线是“自研 Sans-I/O 协议核心 + 可替换安全 vtable + 默认 mbedTLS/libsrtp + Linux 回调外壳”。核心不能依赖 socket、线程、时钟或第三方安全库类型；所有 I/O、时间、随机数、日志、allocator 和 DTLS/SRTP 实现都通过窄接口注入。v1 必须实现 ICE full、STUN、TURN UDP、Trickle ICE、BUNDLE、rtcp-mux、DTLS-SRTP、SRTP/SRTCP、基础 RTP/RTCP、H264 packetization-mode=1 和 Opus RTP，但明确不做编解码、信令传输、DataChannel/SCTP、拥塞控制闭环、TURN TCP/TLS、Simulcast/SVC。

最大风险不是某个单点协议，而是“看起来连上了但媒体不可用”：SDP/H264 fmtp、BUNDLE PT/MID/SSRC 映射、RTCP feedback、ICE/TURN 生命周期、DTLS role/fingerprint/key export、libsrtp buffer tailroom、固定内存和可观测性任何一处后补都会造成高返工。路线图应先建立内存、事件、trace、stats 和互通验收契约，再推进离线可测协议模块，最后做 Chrome 端到端互通和多 PC 压测。

## Key Findings

### Recommended Stack

STACK.md 的结论很明确：v1 不应引入 libwebrtc、libnice、GStreamer、FFmpeg、usrsctp 或 codec 库作为核心依赖。项目核心应自研协议状态机，默认安全后端选 mbedTLS 4.1.0 LTS 与 libsrtp 2.8.0，构建系统用 CMake 3.20.2+、CTest、Ninja，并用 Unity、libFuzzer、ASan/UBSan、coturn、Chrome Stable/Canary 建立测试和互通矩阵。

**Core technologies:**
- C99 公共 ABI：最大化嵌入式和跨平台兼容性，避免 C++/线程模型绑定。
- Sans-I/O core：保证固定内存、可重放测试、可替换平台 I/O、可诊断状态机。
- CMake + CTest + Ninja：支持安装导出、交叉编译、presets、CI 和协议测试。
- mbedTLS 4.1.0 LTS：默认 DTLS 1.2/DTLS-SRTP 后端，需窄 vtable 封装并保留 3.6.6 LTS 回退验证。
- libsrtp 2.8.0：默认 SRTP/SRTCP 后端，packet buffer 必须显式预留 auth tag tailroom。
- Chrome + coturn + tshark：Chrome 是 v1 gate，coturn 覆盖 TURN UDP，tshark/pcap 用于互通诊断。

### Expected Features

FEATURES.md 将 v1 table stakes 和后续能力划分得很清楚。v1 不是“最小能连通”，而是能在固定资源预算内与 Chrome 稳定完成 1v1 H264/Opus 媒体传输，并能诊断失败原因。

**Must have (table stakes):**
- SDP/JSEP 子集：生成和解析 Chrome 可接受的 audio/video offer/answer，只声明真实支持的能力。
- BUNDLE + RTP/RTCP mux + packet demux：v1 默认单 5-tuple，必须正确区分 STUN/TURN/DTLS/RTP/RTCP。
- ICE full + STUN + TURN/UDP + Trickle ICE：覆盖真实公网 NAT 和 relay-only 路径。
- DTLS-SRTP + SRTP/SRTCP：通过 vtable 接入 mbedTLS/libsrtp，完成 fingerprint、role、profile、key export 和 protect/unprotect。
- RTP/RTCP core：SSRC/PT/sequence/timestamp、SR/RR/SDES/BYE、NACK、PLI/FIR、loss/jitter/RTT。
- H264 packetization-mode=1 与 Opus RTP：只做 payload 封装/解封装，不做编解码。
- Encoded media API：发送已编码 H264 access unit / Opus frame，接收完整 access unit / frame 回调。
- 固定内存预算、有界队列、背压、水位和 OOM reason：运行时无不可控扩容。
- 状态事件、W3C-like stats、包级诊断钩子和 Chrome 互通测试。
- 单线程 `rtc_context` 驱动多个 PeerConnection，至少验证多 PC 生命周期和公平调度。

**Should have (competitive):**
- 可重放 Sans-I/O 协议 trace：同一输入、时钟和随机数可复现 ICE/DTLS/RTP 问题。
- 内存预算规划器：启动前估算 PC、track、candidate、packet、DTLS/SRTP 的资源需求。
- W3C stats 命名对齐但 C-friendly：便于和 Chrome `getStats()` 对照。
- 可替换安全后端契约：DTLS/SRTP vtable 约束生命周期、allocator、key export 和错误映射。
- 多 PC 公平调度指标和互通故障报告包。

**Defer (v2+):**
- DataChannel/SCTP：独立可靠传输和背压系统，v1 不协商 `m=application`。
- 拥塞控制闭环：TWCC/REMB/GCC、pacing 和 encoder bitrate callback 留到后续系统设计。
- TURN over TCP/TLS、ICE-TCP：企业网络重要，但不阻塞 UDP relay v1。
- RTX、Simulcast/SVC/RID/FEC：需在单流、RTCP、stats 和重传缓存稳定后加入。
- 非 Linux 平台适配和 poll/event-loop 风格 API：Sans-I/O 核心稳定后扩展。

### Architecture Approach

ARCHITECTURE.md 推荐四层结构：公共 C API、Sans-I/O 协议核心、平台适配外壳、外部安全/网络依赖。`rtc_context` 是唯一运行时 owner，管理 allocator、对象池、timer wheel、event/output queue、trace/stats 和多个 `rtc_pc`；每个 `rtc_pc` 聚合 JSEP、一个 BUNDLE transport、ICE、DTLS、SRTP、RTP/RTCP 和媒体轨道。

**Major components:**
1. `rtc_context`：固定资源预算、对象池、timer、trace/stats、多个 PC 调度。
2. `rtc_pc`：单个对端的协商状态、传输状态、轨道、事件和错误聚合。
3. `sdp/jsep`：typed SDP model、offer/answer、codec/PT/fmtp、ICE、fingerprint、BUNDLE。
4. `ice/stun/turn`：ICE full、STUN transaction、TURN UDP allocation/permission/channel/refresh。
5. `transport`：RFC 7983 mux、DTLS vtable pump、SRTP vtable、selected ICE path。
6. `rtp/rtcp`：RTP session、SSRC/PT/timestamp、RTCP scheduler、feedback、stats。
7. `media`：H264/Opus packetize/depacketize，不含编码器和解码器。
8. `mem/platform/crypto adapters`：arena/pool/packet slab、Linux shell、mbedTLS/libsrtp 默认实现。

### Critical Pitfalls

1. **把 Chrome 互通误判为 ICE/DTLS connected**：必须从 Phase 0 定义 Chrome stats + 库 trace 双证据矩阵，验证 RTP/RTCP、frames、feedback 和 selected pair。
2. **SDP/H264 fmtp 处理过窄**：必须实现结构化 SDP/H264 capability/fmtp 模块，明确 packetization-mode=1、profile-level-id、in-band SPS/PPS 和 Annex-B/AVCC 边界。
3. **ICE/TURN 只做 happy path**：ICE 要有 checklist、role conflict、triggered checks、nomination、restart、consent；TURN 要有 allocation、permission、channel、nonce 和 refresh 生命周期。
4. **DTLS-SRTP/libsrtp 细节错误**：vtable 必须暴露 role、fingerprint、selected profile、key material、timer 和错误；SRTP buffer 必须检查 tailroom，SSRC/ROC/replay 错误要分层统计。
5. **固定内存和可观测性后补**：必须在 Phase 0 建全栈内存账本、事件模型、错误码、stats schema、trace taxonomy，覆盖 mbedTLS/libsrtp 峰值而不只统计自研 arena。

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 0: 基础契约、内存账本与互通验收基线

**Rationale:** 固定内存、事件序列、错误码、stats/trace 和 Chrome 互通矩阵会约束所有后续模块；后补成本最高。  
**Delivers:** C API 初稿、`rtc_context` 配置模型、allocator/arena/pool/packet buffer contract、handle/generation 对象模型、timer/event/output queue、run-to-completion 回调规则、错误码分层、stats/trace schema、Chrome/coturn/tshark 验收矩阵。  
**Addresses:** 固定内存预算、有界队列、生产状态机事件、W3C-like stats、包级诊断、单线程 context。  
**Avoids:** 固定内存漏算、回调重入、可观测性后补、Chrome 互通误判、多 PC 调度无预算。

### Phase 1: 离线协议与媒体核心

**Rationale:** SDP、RTP/RTCP、H264/Opus payload、RFC 7983 classifier 都可以在无网络条件下单测和 fuzz，应先稳定语义和 buffer 边界。  
**Delivers:** typed SDP/JSEP 子集、Chrome H264/Opus capability profile、BUNDLE/PT/MID/SSRC 规则、RTP/RTCP parser/writer、SR/RR/SDES/BYE/NACK/PLI/FIR、H264 FU-A/STAP-A/single NAL、Opus RTP、AU 重组上限、parser fuzz corpus。  
**Addresses:** SDP/JSEP、BUNDLE/rtcp-mux、RTP/RTCP core、H264/Opus packetize/depacketize、encoded media API。  
**Avoids:** SDP 字符串拼接、H264 fmtp 过窄、RTCP 延后、同 socket demux 错误、H264 固定内存冲突。

### Phase 2: ICE full、STUN 与 TURN UDP

**Rationale:** DTLS 和媒体都依赖稳定 selected candidate pair；公网 NAT、relay-only、consent 和 restart 必须在安全层之前可诊断。  
**Delivers:** ICE full agent、host/srflx/relay candidate、Trickle ICE、candidate pair checklist、regular nomination、role conflict、peer reflexive candidate、ICE restart 最小处理、STUN transaction、TURN UDP Allocate/Refresh/CreatePermission/ChannelBind/Send/Data、consent freshness、NAT lab。  
**Uses:** Sans-I/O core、timer/event queue、coturn、Linux netns/tc/iptables、Chrome relay-only 测试准备。  
**Avoids:** ICE happy path、TURN 生命周期缺失、consent freshness 缺失、old ufrag/old transaction 污染。

### Phase 3: DTLS-SRTP 与 SRTP/SRTCP 安全传输

**Rationale:** SRTP ready 是媒体发送的硬前置条件；mbedTLS 和 libsrtp 的 allocator、timer、buffer、key direction 风险必须独立关掉。  
**Delivers:** DTLS vtable contract、mbedTLS adapter、DTLS role/setup 映射、fingerprint 校验、`use_srtp` profile 协商、key export、SRTP/SRTCP vtable、libsrtp adapter、tailroom 检查、SSRC/session/ROC/replay/error mapping、security adapter tests。  
**Uses:** mbedTLS 4.1.0 LTS、libsrtp 2.8.0、RFC 5764/3711 vectors、ASan/UBSan。  
**Avoids:** fingerprint 绕过、key/salt 方向错误、libsrtp 越界、SRTP auth/replay 被当普通丢包、第三方依赖内存漏算。

### Phase 4: PeerConnection 聚合、Linux 外壳与 Chrome 端到端互通

**Rationale:** 前三阶段交付模块正确性，本阶段把 JSEP、ICE、DTLS、SRTP、RTP/RTCP 聚合成用户可用连接，并以 Chrome 作为 v1 gate。  
**Delivers:** `rtc_pc` aggregate state、offer/answer flow、track API、encoded frame send/receive、Linux UDP/poll shell、示例 signaling glue、Chrome offer/answer 双向、audio-only、video-only、AV、TURN relay、packet loss、ICE restart smoke、webrtc-internals/getStats 对照。  
**Implements:** Public C API、platform Linux adapter、Chrome interop harness。  
**Avoids:** “transport connected 但媒体黑屏”、RTCP/PLI 未触达应用、stats 无法和 Chrome 对齐。

### Phase 5: 多 PeerConnection、资源公平与生产硬化

**Rationale:** 多 PC 不是简单创建多个连接，而是全局 pool、timer、packet queue、DTLS/SRTP state 和控制包优先级的综合压力测试。  
**Delivers:** per-PC/global budget 隔离、bounded round-robin 或 deadline-priority 调度、control > DTLS/ICE > RTCP > RTP 优先级、queue depth/timer lag/starvation 指标、多 PC soak test、并发握手峰值、资源拒绝和 OOM reason 回归。  
**Addresses:** 单线程 context 多 PC、有界队列和背压、内存水位、多 PC 公平调度指标。  
**Avoids:** 一个高码率视频 PC 饿死 ICE/RTCP、共享 SRTP/SSRC 状态污染、全量扫描导致 timer 抖动。

### Phase 6: v1 发布验收与文档化边界

**Rationale:** v1 必须清楚声明支持矩阵和不支持项，避免用户误以为这是完整 WebRTC endpoint 或浏览器 API。  
**Delivers:** API 文档、集成指南、内存预算指南、Chrome 互通报告、known limitations、diagnostic bundle 格式、v1.x/v2 backlog。  
**Addresses:** 生产可观测性、用户集成体验、out-of-scope 清晰度。  
**Avoids:** 对 TURN TCP/TLS、DataChannel、拥塞控制、codec、signaling、Simulcast/SVC 的误承诺。

### Phase Ordering Rationale

- 先做 Phase 0，因为内存、事件、错误码和可观测性是所有协议模块的验收基础。
- Phase 1 先做离线协议和 payload，是为了用单测/fuzz 锁定 SDP/RTP/RTCP/H264/Opus 语义，避免在 Chrome 互通时混杂网络和安全问题。
- Phase 2 必须早于 Phase 3，因为 DTLS 运行在 ICE selected path 上，NAT/TURN/consent 问题会掩盖 DTLS/SRTP 问题。
- Phase 3 必须早于媒体端到端，因为 SRTP/SRTCP ready 是 WebRTC 媒体发送前置条件。
- Phase 4 才做完整 PeerConnection 和 Chrome gate，目标是集成验证，不应首次补核心协议。
- Phase 5 专门处理多 PC 和公平性，因为它依赖前面所有模块的资源模型和定时器行为。
- Phase 6 固化文档和边界，防止 v1 被解读为完整 W3C API、完整 WebRTC transport conformance 或一站式媒体 pipeline。

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** Chrome 当前 H264 SDP profile、`profile-level-id`、SPS/PPS in-band 策略、RTCP feedback 最小集合需要样本和互通验证。
- **Phase 2:** ICE consent 定时参数、TURN ChannelData vs Send/Data Indication 默认策略、公网 NAT lab 场景需要专项验证。
- **Phase 3:** mbedTLS 4.1.0 DTLS-SRTP API、Chrome 当前 SRTP protection profile、libsrtp 2.8.0 tag/发布物/构建矩阵需要锁定。
- **Phase 4:** Chrome Stable/Canary 自动化 harness、getStats 字段对照、webrtc-internals dump 格式需要实现前确认。
- **Phase 5:** 默认 pool size、per-PC 配额、timer 优先级和公平调度阈值需要压测数据驱动。

Phases with standard patterns (skip research-phase unless requirements change):
- **Phase 0:** arena/pool/handle、错误码、ring trace、run-to-completion 事件模型有成熟嵌入式 C 模式，可直接设计和评审。
- **Phase 1 parser/fuzzer 基础:** STUN/SDP/RTP/RTCP/H264/Opus 的离线 parser、round-trip、corpus regression 和 libFuzzer 模式标准化程度高。
- **Phase 6:** 发布文档、limitations、integration guide、diagnostic guide 主要是整理前面验收证据，不需要独立研究。

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | 协议基线来自 RFC/W3C/官方文档；mbedTLS、libsrtp、CMake、libFuzzer 等都有官方来源。Chrome 具体协商偏好仍需互通验证。 |
| Features | HIGH | v1 table stakes 与 WebRTC 媒体传输必需子集、Chrome 互通目标和资源受限约束一致。v1.x/v2 延后项边界清晰。 |
| Architecture | HIGH | Sans-I/O、BUNDLE 单 transport、vtable 安全后端、fixed pools、分层状态机与项目约束高度匹配。具体预算参数需实测。 |
| Pitfalls | HIGH | 关键风险均来自 RFC 语义、浏览器互通经验、mbedTLS/libsrtp 官方约束和资源受限场景。 |

**Overall confidence:** HIGH

### Gaps to Address

- Chrome 当前 H264/Opus SDP 样本：在 Phase 1/4 固定 Stable/Canary 版本，保存 SDP、pcap、getStats、webrtc-internals 样本作为回归 corpus。
- mbedTLS 4.1.0 集成风险：Phase 3 保持 DTLS vtable 足够窄，保留 mbedTLS 3.6.6 LTS 构建 preset 作为回退验证。
- libsrtp 2.8.0 发布物和 tag 锁定：首次集成时记录 tag SHA、构建方式和是否使用官方 tarball/包管理器。
- TURN/TCP/TLS 缺失带来的企业网络风险：v1 文档明确限制，candidate model 保留 transport 枚举，Phase 后续单独研究。
- 不做拥塞控制的弱网体验：v1 不宣称 QoE 完整性，只提供 RTCP feedback、stats、关键帧请求、queue/backpressure 和后续 pacing/bitrate hook。
- 多 PC 默认容量和公平调度参数：Phase 5 通过 soak test 和水位数据确定，而不是在 API 中写死乐观默认值。

## Sources

### Primary (HIGH confidence)

- `.planning/research/STACK.md` — 推荐技术栈、版本、依赖策略、协议标准基线、测试矩阵。
- `.planning/research/FEATURES.md` — v1 table stakes、差异化能力、反功能、优先级和依赖关系。
- `.planning/research/ARCHITECTURE.md` — 四层架构、组件职责、Sans-I/O 模式、数据流、状态流和构建顺序。
- `.planning/research/PITFALLS.md` — 关键风险、预防策略、phase mapping、性能和安全陷阱。
- RFC 8834/8835/8829/8843/8445/8489/8656/5764/3711/6184/7587/7983/7675 — WebRTC media transport、ICE/STUN/TURN、DTLS-SRTP、SRTP、H264/Opus、demux、consent。
- W3C WebRTC 与 WebRTC Stats — 状态聚合、stats 字段和 Chrome 对照模型。
- mbedTLS 官方 release/API 文档 — 4.1.0 LTS、DTLS、use_srtp、timer callback、key export。
- Cisco libsrtp README/CHANGES — SRTP/SRTCP、protect/unprotect、tailroom、replay、CMake/tag 信息。

### Secondary (MEDIUM confidence)

- Chrome/libwebrtc g3doc — payload type、BUNDLE collision、DTLS transport、RTP 行为和浏览器实现细节。
- libdatachannel、Pion、Kinesis Video Streams WebRTC C SDK、GStreamer webrtcbin — 竞品能力、stats/interop/API 形态参考。
- MDN WebRTC codecs guide — 浏览器 H264/Opus 实践摘要。
- USENIX Security 2026 prepublication — DTLS-SRTP 认证绕过风险的生态研究，作为安全风险提醒而非 v1 设计唯一依据。

### Tertiary (LOW confidence)

- 无单独采用的低置信来源。所有 roadmap 关键决策均由至少一份研究文件和高置信协议/官方来源支撑。

---
*Research completed: 2026-05-12*  
*Ready for roadmap: yes*
