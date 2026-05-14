# v1.1 Research: Features

**Milestone:** v1.1 剥离收口
**Date:** 2026-05-14

## 研究问题

v1.1 的 feature 不是新增 WebRTC 能力，而是让 v1.0 的 WebRTC 能力更像独立库。研究目标是区分本里程碑的 table stakes、可选增强和明确不做内容。

## Feature categories

### 1. API 独立性与迁移

**Table stakes**

- 公共头文件以 `include/micrortc/` 为唯一 public include 根。
- public API 中不出现 AWS/KVS 产品概念、credential/storage/signaling service 概念或来源项目类型名。
- 对现有 v1.0 调用方可用的 API 给出兼容策略：继续保留、标记 deprecated、提供新别名或明确后续 major 移除。
- `mrtc_peer_connection_create_offer()` 等预留但未实现接口要么实现为可验证路径，要么在文档/API 状态中明确标注限制。

**Differentiators**

- 提供更贴近 WebRTC/JavaScript 心智模型的 C API 命名和示例，不再需要理解 AWS SDK 的历史结构。
- 生成 public API surface 清单，作为后续 review 和版本发布依据。

### 2. Package/release readiness

**Table stakes**

- install tree 可被独立 downstream 工程用 `find_package(micrortc CONFIG REQUIRED)` 消费。
- CMake package 不泄漏 build tree 绝对路径。
- package config 传递必需依赖，缺失依赖时错误清晰。
- 版本号、config version、`mrtc_version_string()` 和文档中的版本语义一致。

**Differentiators**

- 提供 release smoke 脚本，一次性验证 build、install、package consumer、CTest 和 v1 E2E。
- 可选生成 pkg-config 文件，但不作为 v1.1 必需项；当前项目已经以 CMake package 为主。

### 3. License/source compliance

**Table stakes**

- 保留本地 `reflib/kvs-webrtc-sdk` 作为剥离基线，不默认拉 GitHub 上游。
- 新增或继续派生的文件记录来源、license、NOTICE 影响。
- README 或 docs 明确哪些目录属于核心库、哪些是 demo/test/signaling harness。

**Differentiators**

- 使用 SPDX/REUSE 风格的文件级 license 标注，后续可机器检查。

### 4. Dependency boundary

**Table stakes**

- 核心库不引入 AWS SDK C++、KVS signaling、AWS credential/storage、GStreamer sample 或 libwebsockets。
- OpenSSL/libsrtp/usrsctp 仍作为 transport/media security 依赖存在，但文档和 CMake option 要表达清楚。
- OpenSSL 3 deprecated low-level API 风险要被审计并形成 backlog 或修复项。

**Differentiators**

- 形成依赖矩阵：必需、可选、test-only、demo-only、明确禁止。

### 5. Regression proof

**Table stakes**

- 清理后仍通过 `scripts/verify-v1.sh` 默认 host Chromium E2E。
- 清理后仍通过显式 TURN relay E2E，并保留 secret redaction。
- DataChannel、H264、Opus、双向媒体和 selected relay candidate evidence 不退化。
- summary JSON 继续可区分 build、CTest、host、TURN、媒体、failure stage。

**Differentiators**

- 增加 API/package 变更后的 focused regression，例如 include-order、headers-only compile、consumer compile、symbol/API list diff。

## 不建议放入 v1.1 的 feature

- Firefox/Safari/macOS 互通矩阵扩展。
- 新 codec、拥塞控制、SFU/MCU、多方会议。
- 核心库内置 signaling transport。
- 媒体采集、编码、GStreamer adapter。
- 大规模线程模型重写。

## Sources

- CMake Importing and Exporting Guide: https://cmake.org/cmake/help/v3.23/guide/importing-exporting/index.html
- Semantic Versioning 2.0.0: https://semver.org/
- REUSE Specification 3.3: https://reuse.software/spec-3.3/
- WebRTC Native Code overview: https://webrtc.github.io/webrtc-org/native-code/
- libdatachannel project positioning: https://libdatachannel.org/
