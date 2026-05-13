# Roadmap: libmicrortc

**Created:** 2026-05-12
**Mode:** standard
**Granularity:** standard

## Overview

本路线图采用水平分层方式组织，因为 libmicrortc 是协议栈剥离和基础库项目，而不是常规产品 MVP。阶段顺序优先保证：源码基线可信、构建边界独立、API 和协议模块能脱离 AWS/KVS、媒体路径可验证，最后用 Chrome 自动化 E2E 证明真实双向音视频互通。

| Phase | Name | Goal | Requirements |
|-------|------|------|--------------|
| 1 | 基线、范围与合规边界 | 锁定本地 KVS WebRTC SDK 基线，建立溯源和剥离边界 | BASE-01, BASE-02, BASE-03, BASE-04 |
| 2 | 独立构建与库骨架 | 建立 libmicrortc 的 CMake 静态库骨架，并切断非核心组件构建依赖 | BUILD-01, BUILD-02, BUILD-03, BUILD-04 |
| 3 | AWS 风格 API 与 signaling-free PeerConnection | 从 AWS 头文件裁剪 v1 API，保留线程/回调模型，并让 PeerConnection 不依赖 KVS signaling | API-01, API-02, API-03, API-06, PROTO-06, PROTO-07, NET-05 |
| 4 | 传输、安全与 DataChannel 协议核心 | 剥离 ICE/STUN/TURN、DTLS、SRTP、SCTP/DataChannel，并验证 host/STUN/TURN 路径 | API-05, PROTO-01, PROTO-02, PROTO-03, PROTO-05, NET-01, NET-02, NET-03 |
| 5 | H264/Opus 媒体路径 | 剥离 RTP/RTCP 和编码后帧收发路径，实现双向 H264/Opus 媒体能力 | API-04, PROTO-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06, MEDIA-07 |
| 6 | Chrome 自动化 E2E 与测试收口 | 建立自动化浏览器互通验收，证明连接、DataChannel、TURN 和双向媒体真实可用 | NET-04, E2E-01, E2E-02, E2E-03, E2E-04, E2E-05, E2E-06, E2E-07, E2E-08, TEST-01, TEST-02, TEST-03, TEST-04 |

**Coverage:** 45/45 v1 requirements mapped.

## Phases

### Phase 1: 基线、范围与合规边界

**Goal:** 锁定本地 `reflib/kvs-webrtc-sdk` 作为唯一剥离基线，并建立许可证、来源和模块边界记录。

**Requirements:** BASE-01, BASE-02, BASE-03, BASE-04

**Success Criteria:**
1. 文档记录本地参考库路径、版本描述、提交哈希和工作区状态。
2. 形成来源模块清单，标明哪些模块属于核心协议栈，哪些模块属于 AWS/KVS 或 sample 层。
3. LICENSE、NOTICE 和第三方依赖说明有明确保留策略。
4. 后续搬迁文件能追溯到 `reflib/kvs-webrtc-sdk` 中的来源路径。

### Phase 2: 独立构建与库骨架

**Goal:** 创建 libmicrortc 独立 CMake 工程骨架，能构建 Linux x86_64 静态库，并把 AWS/KVS 非核心组件挡在核心库外。

**Requirements:** BUILD-01, BUILD-02, BUILD-03, BUILD-04

**Success Criteria:**
1. 项目根目录提供独立 `CMakeLists.txt` 和核心库 target。
2. Linux x86_64 上可生成静态库产物。
3. OpenSSL、libsrtp、usrsctp 等 v1 依赖路径明确可配置。
4. AWS/KVS signaling、credential、storage、GStreamer sample 不参与核心库构建。

### Phase 3: AWS 风格 API 与 signaling-free PeerConnection

**Goal:** 从 AWS 公共头文件裁剪 v1 API，保留主要使用模型和线程/回调模型，让 PeerConnection API 可以脱离 KVS signaling 工作。

**Requirements:** API-01, API-02, API-03, API-06, PROTO-06, PROTO-07, NET-05

**Success Criteria:**
1. 对外头文件包含 PeerConnection 生命周期、SDP、ICE candidate、回调和基础配置 API。
2. `createPeerConnection`、Offer/Answer、local/remote description、`addIceCandidate` 等路径能编译并通过基础 API 测试。
3. SDP 序列化和反序列化模块已脱离 AWS signaling client。
4. C 端 Answerer 路径先可用，并为同一 v1 路线内的 Offerer 路径保留明确接口。

**Plans:**
- Wave 1: `.planning/phases/03-aws-api-signaling-free-peerconnection/03-01-PLAN.md` — Complete. 恢复本地 KVS 基线，裁剪 public PeerConnection API，实现 signaling-free SDP/Answerer 基础流程，并补齐 CTest、package consumer 和来源追溯。

**Cross-cutting constraints:**
- 必须先恢复 `reflib/kvs-webrtc-sdk` 到 commit `9eebcc4`；不得用 GitHub upstream latest 替代本地基线。
- Public header 不暴露 AWS/PIC 基础类型；核心库不依赖 KVS signaling、libwebsockets、AWS SDK C++ 或 credential/storage。
- Phase 3 只做 Answerer 基础路径和 Offerer API 预留；真实 ICE/DTLS/SRTP/DataChannel/媒体/E2E 留给后续阶段。

### Phase 4: 传输、安全与 DataChannel 协议核心

**Goal:** 剥离传输、安全和 DataChannel 所需协议模块，使 PeerConnection 能在 host、STUN 和 TURN relay 路径上建立受保护连接。

**Requirements:** API-05, PROTO-01, PROTO-02, PROTO-03, PROTO-05, NET-01, NET-02, NET-03

**Success Criteria:**
1. ICE/STUN/TURN 模块可在核心库中独立构建并被 PeerConnection 使用。
2. DTLS OpenSSL 路径和 SRTP 会话路径可完成安全媒体通道初始化。
3. SCTP/DataChannel 支持创建、打开、关闭和消息收发。
4. host candidate、STUN srflx candidate、TURN relay candidate 路径均有可运行验证。

**Plans:**
- Wave 1: `.planning/phases/04-datachannel/04-01-PLAN.md` — Complete. 协议基础设施、依赖与测试骨架。
- Wave 2: `.planning/phases/04-datachannel/04-02-PLAN.md` — Complete. Public API、ICE 配置与 SDP Transport 语义。
- Wave 2: `.planning/phases/04-datachannel/04-03-PLAN.md` — Complete. ICE/STUN/TURN 候选与真实网络验证入口。
- Wave 3 *(blocked on Wave 2 completion)*: `.planning/phases/04-datachannel/04-04-PLAN.md` — Complete. DTLS role/fingerprint/key export 与 SRTP Session wrapper。
- Wave 4 *(blocked on Wave 3 completion)*: `.planning/phases/04-datachannel/04-05-PLAN.md` — Complete pending local real-network sign-off. SCTP/DataChannel lifecycle 与 Phase 4 验收命令收口。

**Cross-cutting constraints:**
- 真实 STUN/TURN 配置是 Phase 4 验收硬前置；缺失或不可读时真实网络验证必须失败。
- 真实 TURN credential 不写入计划、文档或示例配置；仓库只保留 placeholder 示例。
- DTLS 角色必须从 SDP `setup` 属性推导，fingerprint 必须写入 SDP 并在握手后校验。
- DataChannel 必须在真实 ICE/DTLS/SCTP 路径上验证文本和二进制消息，不能只用 API 或 loopback 单元测试代替。
- 当前代码和自动化测试已执行完成；Phase 4 仍需要开发者提供根目录 `mrtc-ice-servers.local.json` 并运行完整 real-network verifier 后才能标记为阶段完成。
- 核心库继续排除 KVS signaling、AWS credential/storage、libwebsockets、采集/编码和 Phase 5 RTP/RTCP 媒体路径。

### Phase 5: H264/Opus 媒体路径

**Goal:** 剥离 RTP/RTCP 和编码后帧收发路径，使 C 端和 Chrome 可以双向传输 H264 视频与 Opus 音频。

**Requirements:** API-04, PROTO-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06, MEDIA-07

**Success Criteria:**
1. API 支持添加音视频 transceiver，并通过 `writeFrame` 类接口发送编码后媒体帧。
2. RTP/RTCP 模块支持 H264/Opus 所需的基本发送、接收、RTCP 和 NACK 相关路径。
3. 核心库不包含采集器或编码器，C 端发送媒体来自固定 H264/Opus 测试文件。
4. C 端到 Chrome、Chrome 到 C 端的 H264 和 Opus 媒体路径都有可观察输出。

**Plans:**
- Wave 1: `.planning/phases/05-h264-opus/05-01-PLAN.md` — Complete. 媒体 Public API、Transceiver 与 SDP 合约。
- Wave 1: `.planning/phases/05-h264-opus/05-02-PLAN.md` — Complete. RTP Packet 与 H264/Opus Codec Primitives。
- Wave 2 *(blocked on Wave 1 completion)*: `.planning/phases/05-h264-opus/05-03-PLAN.md` — Complete. SRTP 媒体发送/接收集成。
- Wave 3 *(blocked on Wave 2 completion)*: `.planning/phases/05-h264-opus/05-04-PLAN.md` — Planned. RTCP SR/RR、NACK 重传与 PLI 回调。
- Wave 4 *(blocked on Wave 3 completion)*: `.planning/phases/05-h264-opus/05-05-PLAN.md` — Planned. 固定媒体 Fixture、双向 Harness 与收口。

**Cross-cutting constraints:**
- 媒体 public API 必须保留 AWS 薄裁剪使用模型，但只暴露 `MRTC_*` / `mrtc_*` 和标准 C 类型。
- H264 输入先锁定 Annex-B；固定 H264 fixture 必须在关键帧前包含 SPS/PPS。
- Opus 使用长度前缀 packet fixture，不引入 Ogg、MP4、GStreamer、FFmpeg 或编码/解码职责。
- RTCP/NACK 必须驱动行为：NACK 触发 rolling buffer 重发，PLI 触发 `on_picture_loss` 回调。
- Phase 5 使用 C fixture harness 证明编码后媒体路径；完整 Chrome 自动化 E2E 仍属于 Phase 6。

### Phase 6: Chrome 自动化 E2E 与测试收口

**Goal:** 建立自动化验收链路，启动 C demo 和 Chrome，完成 signaling 交换并断言连接、DataChannel、TURN relay、双向 H264/Opus 媒体真实流动。

**Requirements:** NET-04, E2E-01, E2E-02, E2E-03, E2E-04, E2E-05, E2E-06, E2E-07, E2E-08, TEST-01, TEST-02, TEST-03, TEST-04

**Success Criteria:**
1. 提供 Chrome 页面和 C 端 demo，demo 层可自动完成 SDP/candidate 交换。
2. 验收脚本能自动启动 C demo 和 Chrome，并断言 PeerConnection 连接成功。
3. 验收脚本能断言 DataChannel 双向消息收发成功。
4. 验收脚本能证明 TURN relay 路径进入实际验收。
5. 验收脚本能证明浏览器和 C 端之间 H264/Opus 双向媒体实际流动。
6. 协议单元测试和 E2E 测试有清晰命令，输出能区分构建、协议测试、互通和媒体流动结果。

## Dependency Notes

- Phase 1 是所有阶段的前置条件，避免后续搬迁失去来源依据。
- Phase 2 先建立独立构建边界，后续每个协议模块搬迁都必须进入核心库 target。
- Phase 3 先打通 API、SDP 和 signaling-free PeerConnection，给 Phase 4/5 的协议和媒体模块提供可挂载入口。
- Phase 4 必须先于完整媒体 E2E，因为媒体路径依赖 ICE/DTLS/SRTP。
- Phase 6 是 v1 真正验收点；Phase 2-5 的局部验证不能替代浏览器互通验收。

---
*Roadmap created: 2026-05-12*
