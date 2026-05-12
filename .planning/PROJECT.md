# libmicrortc

## 这是什么

libmicrortc 是一个从 `awslabs/amazon-kinesis-video-streams-webrtc-sdk-c` 参考并抽取出来的通用 C WebRTC 协议栈项目。它的目标不是继续作为 AWS/KVS SDK，而是剥离 AWS、KVS signaling、存储和示例层耦合，保留并整理可复用的 WebRTC 核心能力。

v1 面向 Linux x86_64，优先保证 Chrome 浏览器与 C 端 demo 建立 WebRTC 连接，并完成双向 H264 视频与 Opus 音频媒体流动验证。核心库不负责媒体采集、编码或 signaling，只接收和输出编码后的媒体帧，并向应用层暴露 SDP、ICE candidate 和媒体回调等能力。

## 核心价值

把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] 从 AWS KVS WebRTC C SDK 中剥离 WebRTC 协议栈，移除 AWS/KVS 服务耦合。
- [ ] 生成可独立构建的 CMake 静态库，v1 面向 Linux x86_64。
- [ ] 对外 API v1 先从 AWS 公共头文件裁剪，尽量保持 AWS SDK 使用模型，后续再逐步改名和清理。
- [ ] 核心库只接收和输出编码后媒体帧，不包含采集、编码、GStreamer adapter 或应用层 signaling。
- [ ] 保留并整理 ICE、STUN、TURN、DTLS、SRTP、RTP/RTCP、SCTP/DataChannel、SDP 等 WebRTC 核心模块。
- [ ] v1 支持 C 端与 Chrome 浏览器建立 PeerConnection，并完成双向音视频媒体验证。
- [ ] v1 媒体优先支持 H264 视频和 Opus 音频。
- [ ] v1 支持 DataChannel 作为 PeerConnection 能力的一部分。
- [ ] v1 支持 TURN relay 场景，而不只限于本机或局域网 host candidate。
- [ ] demo/验收层尽可能自动化完成启动、SDP/candidate 交换和媒体流动断言。
- [ ] 同时保留协议单元测试和浏览器互通 E2E 测试，其中 E2E 是验收核心。
- [ ] 保留 Apache-2.0 许可证兼容路径、NOTICE、来源说明和第三方依赖声明。

### Out of Scope

- Safari 和 Firefox 互通 — v1 先保证 Chrome，降低浏览器兼容矩阵复杂度。
- GStreamer adapter — 核心库只处理编码后媒体帧，采集和编码适配后续再做。
- 核心库内置 signaling — signaling 属于应用层；demo 可以提供自动化交换机制，但不进入核心库边界。
- 媒体采集和编码 — v1 使用固定 H264/Opus 测试文件或外部编码后帧输入，库不承担编码器职责。
- 长期保持 AWS SDK 源码级兼容 — v1 可从 AWS 头文件裁剪起步，但项目方向是独立库和更清爽的 libmicrortc 风格。
- 多浏览器兼容优化和完整媒体质量优化 — v1 以剥离彻底和 Chrome 双向媒体验证为主。
- SFU、MCU、多方会议、录制、屏幕共享 — 这些不是协议栈剥离 v1 的核心目标。

## Context

参考项目为 AWS Labs 的 `amazon-kinesis-video-streams-webrtc-sdk-c`。该项目是 Apache-2.0 许可的 C WebRTC SDK，包含音视频、DataChannel、NACK、STUN/TURN、IPv4/IPv6、signaling client、存储相关能力，以及 OpenSSL、libsrtp、libjsmn、libusrsctp、libwebsockets 等依赖。

剥离基线以当前项目目录下的 `reflib/kvs-webrtc-sdk` 为准，而不是以 GitHub 上游最新状态为准。初始化时该本地参考库为 `v1.18.1`，短提交 `9eebcc4`，工作区干净。

本项目要利用其协议栈实现基础，但项目优先级不是保留 AWS 产品集成，而是把协议栈从 AWS/KVS 业务边界中分离出来。剥离策略暂不在初始化阶段锁死，需要后续单独讨论；当前只确定最终方向：独立 C 库、清晰模块边界、可自动验证的浏览器互通能力。

v1 demo 的理想形态是一个 C 端进程和一个浏览器页面。C 端先实现 Answerer 路径，后续也需要支持 Offerer；自动化验收应尽可能启动 C demo 和 Chrome，自动完成 SDP/candidate 交换，并断言双向 H264/Opus 媒体实际流动。C 端 E2E 媒体源优先读取固定 H264/Opus 测试文件。

## Constraints

- **来源项目**: 以 AWS KVS WebRTC C SDK 为参考和代码来源 — 需要保留许可证、NOTICE、文件来源和第三方依赖说明。
- **源码基线**: 以本地 `reflib/kvs-webrtc-sdk` 的版本为准 — 后续分析、搬迁和差异讨论都应先读取本地参考库，避免无意跟随上游变化。
- **平台**: v1 先支持 Linux x86_64 — 先把独立构建和互通跑通，再扩展平台矩阵。
- **构建系统**: 继续使用 CMake — 贴近来源项目和 C/C++ 生态，静态库优先。
- **依赖策略**: v1 接受 OpenSSL、libsrtp、usrsctp 等现有依赖 — 优先快速剥离并保持协议能力，不在 v1 强行替换依赖。
- **API 形态**: v1 先从 AWS 对外头文件裁剪 — 降低迁移初期风险，后续逐步改成 libmicrortc 风格。
- **线程模型**: 先沿用 AWS SDK 的线程和回调模型 — 避免在剥离早期同时重写并发模型。
- **媒体边界**: 库只处理编码后媒体帧 — 不把采集、编码、GStreamer adapter 放入核心库。
- **互通范围**: v1 先保证 Chrome — Safari/Firefox 兼容留到后续阶段。
- **验收方式**: E2E 为验收核心，协议单元测试为支撑 — 不能只以编译通过或 ICE connected 作为完成标准。

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| 项目定位为协议栈抽取，而不是 AWS SDK fork | 用户希望剥离 WebRTC 协议栈，去掉 AWS/KVS 产品耦合 | Pending |
| 剥离基线锁定为本地 `reflib/kvs-webrtc-sdk` | 用户明确要求以当前项目目录下参考库版本为准，不以远端最新版本为准 | Pending |
| v1 API 先按 AWS 公共头文件裁剪 | 初期降低迁移成本，保留可用的调用模型 | Pending |
| 核心库只接收和输出编码后媒体帧 | 保持协议栈边界干净，不把采集和编码拉进核心库 | Pending |
| 先沿用 AWS 的线程和回调模型 | 降低早期剥离风险，避免同时重构并发模型 | Pending |
| CMake 作为构建系统，静态库优先 | 贴近现有 C 项目实践和来源项目结构 | Pending |
| v1 浏览器目标先限定 Chrome | 控制互通测试范围，降低浏览器差异带来的干扰 | Pending |
| v1 优先 H264 和 Opus | 面向实际浏览器互通和嵌入式媒体场景 | Pending |
| v1 要覆盖 TURN relay | 不把互通能力限制在本机或局域网场景 | Pending |
| demo/验收尽可能自动化 | 用浏览器 E2E 证明真实媒体流动，而不只证明 API 或连接状态 | Pending |
| 许可证和溯源作为早期工作处理 | 代码派生自 Apache-2.0 项目，必须保证合规路径清晰 | Pending |

## Evolution

本文档会在阶段切换和里程碑边界持续演进。

**每次阶段切换后**（通过 `$gsd-transition`）：
1. 如果需求被证伪，将其移动到 Out of Scope 并记录原因。
2. 如果需求被验证，将其移动到 Validated 并标注阶段来源。
3. 如果出现新需求，将其加入 Active。
4. 如果产生关键决策，将其加入 Key Decisions。
5. 如果“这是什么”已经不准确，及时更新项目描述。

**每个里程碑完成后**（通过 `$gsd-complete-milestone`）：
1. 完整复查所有章节。
2. 复查核心价值是否仍然是正确优先级。
3. 复查 Out of Scope 的理由是否仍然成立。
4. 用当前状态更新 Context。

---
*Last updated: 2026-05-12 after initialization*
