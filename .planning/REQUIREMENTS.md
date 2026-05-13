# Requirements: libmicrortc

**Defined:** 2026-05-12
**Core Value:** 把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。

## v1 Requirements

### 源码基线与合规

- [x] **BASE-01**: 项目必须以本地 `reflib/kvs-webrtc-sdk` 作为剥离基线，而不是以 GitHub 上游最新状态作为实现依据。
- [x] **BASE-02**: 项目必须记录本地参考库版本、提交、来源路径和后续搬迁差异。
- [x] **BASE-03**: 项目必须保留 Apache-2.0 许可证兼容路径，包括 LICENSE、NOTICE、来源说明和第三方依赖声明。
- [x] **BASE-04**: 从 AWS 参考库搬迁或派生的文件必须能追溯到来源模块。

### 构建与库形态

- [x] **BUILD-01**: 项目必须提供独立 CMake 构建入口，不依赖 AWS/KVS sample 工程才能构建核心库。
- [x] **BUILD-02**: 项目必须生成面向 Linux x86_64 的静态库构建产物。
- [x] **BUILD-03**: v1 构建可以继续使用 OpenSSL、libsrtp、usrsctp 等参考库依赖。
- [x] **BUILD-04**: 构建系统必须排除或可关闭 AWS/KVS signaling、credential、storage、GStreamer sample 等非核心库组件。

### API 与运行模型

- [x] **API-01**: v1 对外头文件必须先从 AWS WebRTC 公共 API 裁剪，保留主要调用模型。
- [x] **API-02**: API 必须支持创建和释放 PeerConnection。
- [x] **API-03**: API 必须支持设置本地/远端 SDP、创建 Offer、创建 Answer，以及添加 ICE candidate。
- [x] **API-04**: API 必须支持添加音视频 transceiver，并通过 `writeFrame` 类接口发送编码后媒体帧。
- [ ] **API-05**: API 必须支持 DataChannel 创建、打开、关闭和消息收发。
- [x] **API-06**: v1 必须先沿用 AWS SDK 的线程和回调模型。

### 协议栈模块

- [ ] **PROTO-01**: 核心库必须包含可独立构建的 ICE/STUN/TURN 模块。
- [ ] **PROTO-02**: 核心库必须包含 DTLS 模块，并优先使用 OpenSSL 路径。
- [ ] **PROTO-03**: 核心库必须包含 SRTP 会话模块，用于音视频 RTP 保护。
- [x] **PROTO-04**: 核心库必须包含 RTP/RTCP 模块，支持基本 packetize/depacketize、RTCP 处理和 NACK 相关路径。
- [ ] **PROTO-05**: 核心库必须包含 SCTP/DataChannel 模块。
- [x] **PROTO-06**: 核心库必须包含 SDP 序列化和反序列化能力。
- [x] **PROTO-07**: 核心库不得依赖 AWS KVS signaling client 才能完成 PeerConnection 协议流程。

### 媒体能力

- [x] **MEDIA-01**: 核心库只接收和输出编码后媒体帧，不实现音视频采集。
- [x] **MEDIA-02**: 核心库不实现音视频编码器，v1 使用外部或固定测试文件提供编码后帧。
- [x] **MEDIA-03**: v1 必须优先支持 H264 视频发送到 Chrome。
- [x] **MEDIA-04**: v1 必须优先支持从 Chrome 接收 H264 视频路径，并能在 demo/测试中证明收到媒体。
- [x] **MEDIA-05**: v1 必须优先支持 Opus 音频发送到 Chrome。
- [x] **MEDIA-06**: v1 必须优先支持从 Chrome 接收 Opus 音频路径，并能在 demo/测试中证明收到媒体。
- [x] **MEDIA-07**: C 端 E2E 发送媒体源必须支持读取固定 H264/Opus 测试文件。

### ICE 与网络互通

- [ ] **NET-01**: v1 必须支持 Chrome 与 C 端在 host candidate 场景建立连接。
- [ ] **NET-02**: v1 必须支持 STUN server-reflexive candidate 路径。
- [ ] **NET-03**: v1 必须支持 TURN relay candidate 路径。
- [ ] **NET-04**: TURN relay 路径必须进入自动化或半自动化验收范围，而不是只保留未验证代码。
- [x] **NET-05**: C 端必须先支持 Answerer 模式，后续同一 v1 路线内补齐 Offerer 模式。

### Demo 与自动化验收

- [x] **E2E-01**: 项目必须提供 Chrome 浏览器页面和 C 端 demo，用于验证浏览器互通。
- [x] **E2E-02**: demo 层可以实现自动化 signaling 交换，但核心库不得内置应用层 signaling。
- [x] **E2E-03**: 验收脚本必须能自动启动 C demo 和 Chrome，并完成 SDP/candidate 交换。
- [x] **E2E-04**: 验收脚本必须断言 PeerConnection 连接成功。
- [ ] **E2E-05**: 验收脚本必须断言 DataChannel 可收发消息。
- [ ] **E2E-06**: 验收脚本必须断言浏览器实际收到来自 C 端的 H264 视频帧或可观察视频输出。
- [ ] **E2E-07**: 验收脚本必须断言 C 端实际收到来自 Chrome 的视频媒体包或编码后帧路径输出。
- [ ] **E2E-08**: 验收脚本必须断言双向 Opus 音频路径有媒体流动证据。

### 测试与质量

- [ ] **TEST-01**: 项目必须保留或迁移协议单元测试，覆盖 STUN、ICE、TURN、DTLS、SRTP、RTP/RTCP、SDP、SCTP/DataChannel 等核心模块。
- [ ] **TEST-02**: E2E 浏览器互通测试必须作为 v1 完成的核心验收标准。
- [x] **TEST-03**: 构建和测试必须可通过一组明确命令在 Linux x86_64 上运行。
- [x] **TEST-04**: 测试输出必须能区分编译成功、协议单元测试成功、浏览器互通成功和媒体流动成功。

## v2 Requirements

### 平台与浏览器

- **V2-PLAT-01**: 支持 Firefox 浏览器互通。
- **V2-PLAT-02**: 支持 Safari 浏览器互通。
- **V2-PLAT-03**: 扩展到 ARM 或嵌入式 Linux 构建目标。
- **V2-PLAT-04**: 评估 macOS/Windows 支持。

### API 与依赖演进

- **V2-API-01**: 将裁剪后的 AWS 风格 API 逐步改名和清理为更清爽的 libmicrortc 风格。
- **V2-API-02**: 评估将线程/回调模型演进为更可控的 event-loop 或 poll 风格。
- **V2-DEP-01**: 评估 OpenSSL、libsrtp、usrsctp 等依赖的可替换适配层。

### 媒体与应用适配

- **V2-MEDIA-01**: 增加 GStreamer adapter。
- **V2-MEDIA-02**: 增加 FFmpeg 或其他编码后媒体输入适配。
- **V2-MEDIA-03**: 增加更完整的拥塞控制、码率调整和媒体质量优化。

## Out of Scope

| Feature | Reason |
|---------|--------|
| 核心库内置 signaling 服务 | signaling 属于应用层；v1 只要求 demo/测试层自动化交换 |
| 媒体采集和编码 | 核心库边界是编码后媒体帧，采集/编码交给外部组件 |
| GStreamer adapter | 明确排除在 v1 之外，避免把媒体管线适配和协议栈剥离混在一起 |
| Safari/Firefox | v1 先保证 Chrome，减少浏览器兼容差异 |
| SFU/MCU/多方会议 | 不是协议栈剥离 v1 的目标 |
| 录制和屏幕共享 | 不影响 WebRTC 核心协议栈独立化 |
| 长期 AWS API 源码级兼容承诺 | v1 从 AWS 头文件裁剪起步，但方向是独立 libmicrortc API |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| BASE-01 | Phase 1 | Complete |
| BASE-02 | Phase 1 | Complete |
| BASE-03 | Phase 1 | Complete |
| BASE-04 | Phase 1 | Complete |
| BUILD-01 | Phase 2 | Complete |
| BUILD-02 | Phase 2 | Complete |
| BUILD-03 | Phase 2 | Complete |
| BUILD-04 | Phase 2 | Complete |
| API-01 | Phase 3 | Complete |
| API-02 | Phase 3 | Complete |
| API-03 | Phase 3 | Complete |
| API-04 | Phase 5 | Complete |
| API-05 | Phase 4 | Pending |
| API-06 | Phase 3 | Complete |
| PROTO-01 | Phase 4 | Pending |
| PROTO-02 | Phase 4 | Pending |
| PROTO-03 | Phase 4 | Pending |
| PROTO-04 | Phase 5 | Complete |
| PROTO-05 | Phase 4 | Pending |
| PROTO-06 | Phase 3 | Complete |
| PROTO-07 | Phase 3 | Complete |
| MEDIA-01 | Phase 5 | Complete |
| MEDIA-02 | Phase 5 | Complete |
| MEDIA-03 | Phase 5 | Complete |
| MEDIA-04 | Phase 5 | Complete |
| MEDIA-05 | Phase 5 | Complete |
| MEDIA-06 | Phase 5 | Complete |
| MEDIA-07 | Phase 5 | Complete |
| NET-01 | Phase 4 | Pending |
| NET-02 | Phase 4 | Pending |
| NET-03 | Phase 4 | Pending |
| NET-04 | Phase 6 | Pending |
| NET-05 | Phase 3 | Complete |
| E2E-01 | Phase 6 | Complete |
| E2E-02 | Phase 6 | Complete |
| E2E-03 | Phase 6 | Complete |
| E2E-04 | Phase 6 | Complete |
| E2E-05 | Phase 6 | Pending |
| E2E-06 | Phase 6 | Pending |
| E2E-07 | Phase 6 | Pending |
| E2E-08 | Phase 6 | Pending |
| TEST-01 | Phase 6 | Pending |
| TEST-02 | Phase 6 | Pending |
| TEST-03 | Phase 6 | Complete |
| TEST-04 | Phase 6 | Complete |

**Coverage:**
- v1 requirements: 45 total
- Mapped to phases: 45
- Unmapped: 0

---
*Requirements defined: 2026-05-12*
*Last updated: 2026-05-12 after roadmap creation*
