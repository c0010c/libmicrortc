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

v1.0 已通过默认 host Chromium E2E 和显式 TURN relay E2E。v1.1 Phase 7 已完成 API 边界冻结：`docs/api-v1.1-boundary.md` 固定 public/private/test-only 边界，`docs/api-v1.1-symbol-map.md` 固定当前 public symbol 旧名到新名映射，`scripts/scan-api-residuals.sh` 生成 Phase 8 改名前的 residual baseline。

```bash
scripts/verify-v1.sh
scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json
```

最终机器可读证据固定在 `build/reports/mrtc-v1-summary.json`，其中 build、CTest、Chrome host、TURN relay、连接、DataChannel、浏览器侧媒体和 C 侧媒体均已通过。

## 核心价值

把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。

## Current Milestone: v1.1 API 清理

**Goal:** 将 v1.0 继承自 AWS/KVS 风格的公共 API 和代码命名收敛为独立、统一、符合 `.clang-tidy` 的 libmicrortc 风格。

**Target features:**
- 破坏式清理公共 API：允许重命名类型、函数、枚举、宏、头文件暴露面，并同步更新 demo、测试和文档。
- 代码风格以 `.clang-tidy` 为准：类型 `CamelCase`，函数/变量/参数/member `lower_case`，枚举常量和宏 `UPPER_CASE`。
- 清理 AWS/KVS 命名残留：公共头、示例、测试和内部边界尽量摆脱 AWS 产品语义。
- 保持 v1.0 已验证能力不退化：Chrome、DataChannel、TURN relay、H264/Opus 双向媒体、`scripts/verify-v1.sh` 仍作为验收核心。
- 新增或更新迁移文档，让使用者知道旧 API 到新 API 的映射。

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
- [x] v1.1 Phase 7 已冻结 public/private/test-only API 边界、完整 public symbol 旧名到新名映射、旧 public names 删除策略和 residual scan 分类规则。

### Active

- [ ] v1.1 public headers 和核心 API 按 Phase 7 symbol map 迁移到 `rtc_*`、`Rtc*`、`RTC_*`，并移除旧 public names。
- [ ] v1.1 代码命名和头文件暴露面按 `.clang-tidy` 约束收口，减少 AWS/KVS 产品语义残留。
- [ ] v1.1 demo、测试、文档和迁移说明同步更新，证明新 API 可用且 v1.0 已验证能力不退化。

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
- **代码风格:** v1.1 代码命名以仓库 `.clang-tidy` 为准；函数、变量、参数、成员使用 `lower_case`，类型使用 `CamelCase`，宏和枚举常量使用 `UPPER_CASE`。

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
| v1.1 允许破坏式 API 命名清理 | 用户明确选择以 API/代码风格清理为里程碑目标，优先形成独立 libmicrortc 风格而不是长期保留 AWS 风格兼容层 | Pending in v1.1 |
| v1.1 public API 边界以 installed `include/micrortc/*.h` 和 `micrortc::micrortc` 为事实来源 | Phase 7 验证了 CMake install/export 边界、public/private/test-only 分类和 symbol map 覆盖 | Validated in Phase 7 |
| v1.1 旧 `mrtc_*`、`MRTC_*`、AWS/KVS public names 默认删除且不提供 wrapper/alias | API-04 要求破坏式清理，不通过兼容层掩盖旧 public API | Validated in Phase 7 |
| residual scan baseline 可在报告 `status: failed` 时返回 0 | Phase 7 发生在机械改名前，需要成功生成 forbidden public residual baseline 供 Phase 8 消费 | Validated in Phase 7 |

## Next Milestone Goals

v1.1 聚焦 API 命名和代码风格清理：公共 API 可以破坏式改名，目标是摆脱 AWS/KVS 风格残留、统一 libmicrortc 命名，并让 demo、测试、文档和自动化验收全部跟随新 API。

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `$gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `$gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-15 after Phase 7 completion*
