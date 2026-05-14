# libmicrortc

## 这是什么

libmicrortc 是一个从 `awslabs/amazon-kinesis-video-streams-webrtc-sdk-c` 参考并抽取出来的通用 C WebRTC 协议栈项目。它的目标不是继续作为 AWS/KVS SDK，而是剥离 AWS、KVS signaling、存储和示例层耦合，保留并整理可复用的 WebRTC 核心能力。

v1.0 已面向 Linux x86_64 完成：Chrome 浏览器与 C 端 demo 可以建立 WebRTC 连接，完成 DataChannel 双向消息、TURN relay 路径和双向 H264 视频 / Opus 音频媒体流动验证。核心库不负责媒体采集、编码或 signaling，只接收和输出编码后的媒体帧，并向应用层暴露 SDP、ICE candidate、DataChannel 和媒体回调等能力。

## 当前状态

**Shipped version:** v1.0 libmicrortc v1
**Shipped date:** 2026-05-14
**Milestone archive:** `.planning/milestones/v1.0-ROADMAP.md`、`.planning/milestones/v1.0-REQUIREMENTS.md`、`.planning/milestones/v1.0-MILESTONE-AUDIT.md`
**Phase archive:** `.planning/milestones/v1.0-phases/`
**Acceptance entrypoint:** `scripts/verify-v1.sh`

v1.0 已通过默认 host Chromium E2E 和显式 TURN relay E2E：

```bash
scripts/verify-v1.sh
scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json
```

最终机器可读证据固定在 `build/reports/mrtc-v1-summary.json`，其中 build、CTest、Chrome host、TURN relay、连接、DataChannel、浏览器侧媒体和 C 侧媒体均已通过。

## 核心价值

把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。

## Requirements

### Validated

- [x] v1.0 以本地 `reflib/kvs-webrtc-sdk` 为唯一剥离基线，不默认跟随 GitHub 上游最新状态。
- [x] 本地参考库路径、版本 `v1.18.1`、提交 `9eebcc4`、来源追溯规则和 Apache-2.0/NOTICE 合规策略已记录。
- [x] 项目提供独立 CMake 静态库、install/export package 和 package consumer 验证。
- [x] AWS/KVS signaling、credential/storage、GStreamer sample、libwebsockets 和 AWS SDK C++ 不进入核心库构建边界。
- [x] v1 API 从 AWS 公共头文件裁剪，覆盖 PeerConnection lifecycle、SDP、ICE candidate、DataChannel、transceiver、encoded frame send 和回调模型。
- [x] 核心库包含并验证 ICE/STUN/TURN、DTLS、SRTP、RTP/RTCP、SCTP/DataChannel、SDP 等 v1 协议模块。
- [x] 核心库只接收和输出编码后媒体帧，不包含采集、编码、GStreamer adapter 或应用层 signaling。
- [x] v1 媒体支持 H264 视频和 Opus 音频，并通过 C fixture harness 与 Chrome E2E 双层验证。
- [x] v1 支持 Chrome host/local 连接、DataChannel、TURN relay 和双向 H264/Opus 媒体自动化验收。
- [x] v1 提供 `scripts/verify-v1.sh` 作为总验收入口，并输出可区分 build、CTest、Chrome host、TURN 和媒体状态的 JSON summary。

### Active

- [ ] 定义下一里程碑需求。候选方向包括 API 命名清理、Offerer 主路径、更多浏览器/平台、媒体适配层、依赖抽象、拥塞控制和发布包整理。

### Out of Scope

- Safari 和 Firefox 互通 — v1 先保证 Chrome；后续 milestone 可单独评估。
- GStreamer adapter — 核心库只处理编码后媒体帧；采集和编码适配留给后续。
- 核心库内置 signaling — signaling 属于应用层；demo/test 可以自动化交换，但不进入核心库边界。
- 媒体采集和编码 — v1 使用固定 H264/Opus 测试文件或外部编码后帧输入，库不承担编码器职责。
- 长期保持 AWS SDK 源码级兼容 — v1 从 AWS 头文件裁剪起步，但项目方向是独立 libmicrortc API。
- 多方会议、SFU、MCU、录制、屏幕共享 — 不是协议栈剥离 v1 的核心目标。

## Context

参考项目为 AWS Labs 的 `amazon-kinesis-video-streams-webrtc-sdk-c`。本项目使用当前工作区的 `reflib/kvs-webrtc-sdk` 作为剥离基线，而不是以 GitHub 上游最新状态为准。初始化时该本地参考库为 `v1.18.1`，短提交 `9eebcc4`。

v1.0 已完成从基线、构建、API、传输、安全、DataChannel、媒体路径到 Chrome 自动化 E2E 的完整闭环。后续里程碑应在这个基础上继续清理 API、扩展互通矩阵、加强发布/打包质量或补充真实应用适配层。

## Constraints

- **源码基线:** 后续分析、搬迁和差异讨论仍以本地 `reflib/kvs-webrtc-sdk` 为准。
- **许可证与溯源:** 从 AWS 参考库搬迁或派生的文件必须保留来源记录和 Apache-2.0/NOTICE 策略。
- **平台:** 当前已验证 Linux x86_64；新增平台需要独立规划。
- **构建系统:** 继续使用 CMake，静态库优先。
- **依赖策略:** v1 接受 OpenSSL、libsrtp、usrsctp；后续替换或抽象依赖需要单独设计。
- **媒体边界:** 核心库只处理编码后媒体帧，不承担采集和编码职责。
- **signaling 边界:** 应用层 signaling 不进入核心库。
- **验收方式:** 浏览器 E2E 是互通能力的核心证据，不能只以编译通过或内部 connected 状态作为完成标准。

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| 项目定位为协议栈抽取，而不是 AWS SDK fork | 用户希望剥离 WebRTC 协议栈，去掉 AWS/KVS 产品耦合 | Validated in v1.0 |
| 剥离基线锁定为本地 `reflib/kvs-webrtc-sdk` | 用户明确要求以当前项目目录下参考库版本为准，不以远端最新版本为准 | Validated in Phase 1 |
| v1 API 先按 AWS 公共头文件裁剪 | 初期降低迁移成本，保留可用调用模型 | Validated in Phase 3-6 |
| 核心库只接收和输出编码后媒体帧 | 保持协议栈边界干净，不把采集和编码拉进核心库 | Validated in Phase 5-6 |
| 先沿用 AWS 的线程和回调模型 | 降低早期剥离风险，避免同时重构并发模型 | Validated in Phase 3 |
| CMake 作为构建系统，静态库优先 | 贴近现有 C 项目实践和来源项目结构 | Validated in Phase 2 |
| v1 浏览器目标先限定 Chrome | 控制互通测试范围，降低浏览器差异干扰 | Validated in Phase 6 |
| v1 优先 H264 和 Opus | 面向实际浏览器互通和嵌入式媒体场景 | Validated in Phase 5-6 |
| v1 覆盖 TURN relay | 不把互通能力限制在本机或局域网场景 | Validated in Phase 6 |
| demo/验收尽可能自动化 | 用浏览器 E2E 证明真实媒体流动，而不只证明 API 或连接状态 | Validated in Phase 6 |

## Next Milestone Goals

下一里程碑尚未定义。建议从 `$gsd-new-milestone` 开始，重新收集目标、约束和需求，再生成新的 `REQUIREMENTS.md` 与 `ROADMAP.md`。

---
*Last updated: 2026-05-14 after v1.0 milestone*
