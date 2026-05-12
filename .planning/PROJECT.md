# libmicrortc

## What This Is

libmicrortc 是一个纯 C 实现的跨平台 WebRTC 库，面向性能和内存受限设备，第一版优先支持 Linux。它提供与 Chrome 进行 1v1 音视频通话所需的媒体传输能力，但库本身不处理音视频编解码。

核心协议栈采用 Sans-I/O 设计：协议核心不直接依赖 socket、线程、时钟或平台事件循环，平台适配层负责网络、定时器、随机数、日志、内存和外部加密/SRTP 实现。第一版先提供回调驱动外壳，同时保持核心可被事件循环或轮询模型复用。

## Core Value

在固定资源预算内，让性能受限设备能够稳定、可观测地与 Chrome 建立生产级 1v1 H264/Opus WebRTC 通话。

## Requirements

### Validated

（暂无，待交付后验证）

### Active

- [ ] 以纯 C 实现 WebRTC 媒体传输库，第一版优先支持 Linux。
- [ ] 支持与 Chrome 进行 1v1 音视频通话。
- [ ] 支持真实公网 NAT 穿透，包括 ICE full、STUN 和 TURN。
- [ ] 支持 H264 与 Opus 的 RTP 封装/解封装，但不处理编解码。
- [ ] 支持 DTLS-SRTP 与 SRTP/SRTCP，DTLS 和 SRTP 通过 vtable 接入，默认实现为 mbedTLS 和 libsrtp。
- [ ] 在初始化时根据配置预算和用户提供的 allocator/arena 固定内存上限。
- [ ] 提供强可观测性，包括状态机事件、统计信息、错误码、内存水位、包级诊断钩子和关键协议 trace。
- [ ] 尽可能避免锁；第一版定义单个 rtc_context 由一个线程驱动，多 PeerConnection 共享同一 context/event loop。
- [ ] 支持多 PeerConnection 场景，为性能稍强设备上的 mesh 多路并发预留资源模型和调度模型。
- [ ] 第一版不实现拥塞控制，但保留后续接入点。

### Out of Scope

- 编码器和解码器 — 应用层负责音视频编解码，库只处理已编码媒体的 WebRTC 传输。
- DataChannel/SCTP — 第一版聚焦 1v1 音视频媒体通话。
- 拥塞控制 — 第一版暂不支持 TWCC/REMB/GCC 等拥塞控制闭环，只保留统计和扩展点。
- 完整 W3C JavaScript API 兼容层 — 本项目是嵌入式/跨平台 C 库，不复刻浏览器 API 表面。
- Simulcast、SVC、多编码器协商 — 第一版仅支持 H264 和 Opus 的基础互通。
- 信令传输 — 库只生成/解析 SDP offer/answer 和 ICE candidate，不内置 WebSocket、MQTT、HTTP 等信令通道。
- TURN over TCP/TLS — 第一版优先支持 UDP relay，TCP/TLS relay 作为后续能力评估。

## Context

目标设备资源受限，因此内存模型、状态机复杂度、锁使用、事件调度和可观测性都必须从一开始作为核心设计约束。库需要面向生产使用，而不是仅跑通 demo：错误路径、超时、协议状态、资源耗尽、重协商边界和多 PeerConnection 场景都需要可诊断。

WebRTC 互通目标是 Chrome，因此需要覆盖浏览器侧要求的关键媒体传输协议组合：SDP offer/answer、ICE、STUN/TURN、DTLS-SRTP、SRTP/SRTCP、RTP/RTCP mux、BUNDLE、H264/Opus RTP 载荷处理以及基础 RTCP 反馈。媒体输入输出面向已编码数据：发送侧应用喂入 H264 access unit 或 Opus frame，库负责 packetize；接收侧库通过回调吐出完整 H264 access unit 或 Opus frame。

架构选择已经确定为 Sans-I/O 协议核心加平台适配层。第一版落地形态为“一个回调驱动的 Linux 适配外壳驱动多个 Sans-I/O PeerConnection”，后续可增加 poll/event-loop 风格 API。

## Constraints

- **语言**: 纯 C — 便于跨平台、嵌入式集成和 ABI 控制。
- **首个平台**: Linux — 第一版先在 Linux 上建立协议正确性、测试能力和 Chrome 互通基线。
- **内存**: 配置预算加用户提供 allocator/arena — 初始化后固定资源上限，运行时不做不可控增长。
- **线程**: 单个 rtc_context 单线程驱动 — 跨线程投递由应用层负责，库内部尽可能无锁。
- **协议范围**: 第一版支持 H264 和 Opus — 只处理 RTP payload 封装/解封装，不处理编解码。
- **网络范围**: 支持真实公网 NAT 穿透 — ICE full、STUN、TURN 是 v1 必要能力。
- **依赖策略**: DTLS 与 SRTP 使用 vtable — 默认 mbedTLS 和 libsrtp，但允许替换实现。
- **可观测性**: 必须是一等能力 — 状态、指标、trace、错误码和资源水位需要贯穿核心设计。
- **兼容性**: 以 Chrome 1v1 音视频通话为第一互通目标 — 协议取舍优先服务该目标。

## Key Decisions

| 决策 | 理由 | 结果 |
|------|------|------|
| 采用 Sans-I/O 协议核心 | 隔离平台 I/O、线程和时钟，提升可测试性、跨平台性，并帮助实现固定内存和低锁设计。 | 待验证 |
| 第一版优先 Linux | 先建立稳定开发、测试和 Chrome 互通基线，再扩展其他平台。 | 待验证 |
| 支持 ICE full、STUN、TURN | 目标是真实公网 NAT 穿透，而不是只在受控网络中跑通。 | 待验证 |
| 内存模型为配置预算加用户 allocator/arena | 同时满足资源上限可预测和不同设备集成方式。 | 待验证 |
| DTLS/SRTP 通过 vtable 接入，默认 mbedTLS/libsrtp | 避免自研安全协议风险，同时保留可替换性和平台适配能力。 | 待验证 |
| 第一版先提供回调驱动外壳 | 贴近嵌入式和 C 库集成方式，同时保持核心可被其他调度模型复用。 | 待验证 |
| 单个 rtc_context 单线程驱动 | 明确无锁边界，多 PeerConnection 通过同一 context/event loop 调度。 | 待验证 |
| 不内置信令传输 | 信令通道差异大，库只负责 SDP 和 candidate 生成/解析。 | 待验证 |
| 第一版不做拥塞控制 | 降低 v1 复杂度，但通过 RTCP、stats 和扩展点保留后续演进空间。 | 待验证 |

## Evolution

本文档会在阶段切换和里程碑边界持续演进。

**每次阶段切换后**（通过 `$gsd-transition`）：
1. 如果需求被证明不成立，移动到 Out of Scope 并说明原因。
2. 如果需求已交付并验证，移动到 Validated 并标注阶段来源。
3. 如果出现新需求，加入 Active。
4. 如果产生关键决策，加入 Key Decisions。
5. 如果 What This Is 不再准确，及时更新。

**每个里程碑结束后**（通过 `$gsd-complete-milestone`）：
1. 完整复核所有章节。
2. 检查 Core Value 是否仍然是正确优先级。
3. 审核 Out of Scope 的理由是否仍然成立。
4. 用当前状态更新 Context。

---
*最后更新：2026-05-12，项目初始化后*
