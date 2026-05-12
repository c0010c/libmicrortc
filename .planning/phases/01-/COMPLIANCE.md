# Phase 1 Compliance: 许可证、NOTICE 与第三方依赖策略

## 目标

`libmicrortc` 派生自 Apache-2.0 许可的 AWS KVS WebRTC C SDK。Phase 1 的目标是先建立可执行的合规策略，确保后续搬迁、裁剪或重写派生文件时不会丢失 LICENSE、NOTICE、来源说明和第三方依赖声明。

本文件记录策略，不在 Phase 1 直接修改根目录 LICENSE 或 NOTICE。后续发布包或根目录落地时必须以本文件为检查入口。

## Canonical Sources

| 来源 | 用途 |
|------|------|
| `reflib/kvs-webrtc-sdk/LICENSE` | Apache-2.0 许可证文本来源 |
| `reflib/kvs-webrtc-sdk/NOTICE` | AWS KVS WebRTC SDK 原 NOTICE 来源 |
| `reflib/kvs-webrtc-sdk/CMake/Dependencies` | 第三方依赖来源、版本和 commit 声明 |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | 后续派生文件的 source attribution 入口 |

## Apache-2.0 策略

1. 根目录或发布包最终必须包含 Apache-2.0 `LICENSE`。
2. 从 `reflib/kvs-webrtc-sdk` 派生的源码文件必须保留适用的版权和许可证头信息。
3. 修改过的派生文件应保留可识别的修改痕迹，至少能通过 `SOURCE-MANIFEST.md` 追溯来源和派生方式。
4. `libmicrortc` 自有新增文件可以使用项目选定的兼容许可证策略，但不得削弱对 AWS KVS WebRTC SDK 派生内容的 Apache-2.0 义务。
5. 任何发布前检查必须确认 `LICENSE` 与 `NOTICE` 同时进入源码包或二进制分发包的合适位置。

## NOTICE 策略

`reflib/kvs-webrtc-sdk/NOTICE` 当前声明 Amazon Kinesis Video Streams Webrtc SDK 及 Amazon.com, Inc. or its affiliates 的版权归属。后续根目录或发布包 NOTICE 应保留这份 NOTICE 内容，并添加 libmicrortc 对 AWS KVS WebRTC SDK 的派生说明。

建议后续 NOTICE 追加说明：

```text
libmicrortc includes code derived from the Amazon Kinesis Video Streams WebRTC C SDK.
The derivation baseline is the local reflib/kvs-webrtc-sdk snapshot at commit 9eebcc4, tag v1.18.1.
See SOURCE-MANIFEST.md for file-level source attribution.
```

如果某个发布包只包含自有代码、不包含任何 AWS 派生实现，仍应由 release checklist 明确确认这一事实，而不是省略检查。

## Source Attribution 策略

1. 每个派生文件必须在 `.planning/phases/01-/SOURCE-MANIFEST.md` 或后续迁移后的 manifest 中记录 `source_path`、`target_path`、`origin_commit`、`derivation` 和 `notes`。
2. `origin_commit` 默认是 `9eebcc4`。如果未来更换本地基线，必须先更新 `BASELINE.md`、`SOURCE-MANIFEST.md` 和本文件。
3. 对 `rewritten-derived` 文件，也必须记录来源路径，因为行为和结构仍来自 AWS KVS WebRTC SDK。
4. 对 `reference-only` 条目，PR 描述应说明没有复制来源代码。
5. 对 `excluded` 条目，必须记录排除原因，避免后续误引入。

## 第三方依赖表

| 依赖 | 声明来源 | 版本或提交 | 来源用途 | v1 状态 |
|------|----------|------------|----------|---------|
| OpenSSL | `CMake/Dependencies/libopenssl-CMakeLists.txt` | `OpenSSL_1_1_1t` | DTLS/TLS 和 crypto 路径 | `runtime/core` |
| libsrtp | `CMake/Dependencies/libsrtp-CMakeLists.txt` | `bd0f27ec0e299ad101a396dde3f7c90d48efc8fc` | SRTP 媒体保护 | `runtime/core` |
| usrsctp | `CMake/Dependencies/libusrsctp-CMakeLists.txt` | `1ade45cbadfd19298d2c47dc538962d4425ad2dd` | SCTP/DataChannel | `runtime/core` |
| libwebsockets | `CMake/Dependencies/libwebsockets-CMakeLists.txt` | `v4.3.5` | KVS signaling client websocket | `excluded` |
| jsmn | `CMake/Dependencies/libjsmn-CMakeLists.txt` | `v1.0.0` | JSON helper, PeerConnection/SDP 辅助路径需后续确认 | `runtime/core` candidate |
| kvsCommonLws/PIC | `CMake/Dependencies/libkvsCommonLws-CMakeLists.txt` | `v1.6.1` | PIC 公共库能力、allocator、logging、state、utils 等 | `supporting/reference-only` |
| mbedTLS | `CMake/Dependencies/libmbedtls-CMakeLists.txt` | `v2.28.8` 或 `v3.6.3` | 替代 crypto backend | `reference-only` |
| AWS SDK C++ | `CMake/Dependencies/libawscpp-CMakeLists.txt` | `1.11.741` | AWS 服务测试或集成路径 | `excluded` |
| googletest | `CMake/Dependencies/libgtest-CMakeLists.txt` | `release-1.12.1` | 协议单元测试框架 | `test-only` |
| benchmark | `CMake/Dependencies/libbenchmark-CMakeLists.txt` | `v1.5.1` | benchmark 测试 | `test-only` 或 `excluded` |
| gperftools | `CMake/Dependencies/libgperftools-CMakeLists.txt` | `gperftools-2.8.zip` | 可选 profiling 和 allocator 实验 | `excluded` 或 profiling reference |

## Excluded AWS/KVS Components

| 组件 | 状态 | 原因 |
|------|------|------|
| `src/source/Signaling` | `excluded` | AWS/KVS signaling client 属于应用层服务集成，不得成为核心库依赖 |
| `kvsWebrtcSignalingClient` target | `excluded` | 链接 `kvsCommonLws` 和 libwebsockets，服务 KVS signaling |
| AWS SDK C++ dependency | `excluded` | v1 核心库不做 AWS 服务客户端 |
| AWS credential/storage/KVS service paths | `excluded` | 超出协议栈边界 |
| sample application signaling exchange | `supporting/reference-only` | demo 可参考，但核心库不内置应用层 signaling |

## v1 合规边界

`runtime/core` 依赖可以进入 v1 构建讨论，但仍需在 Phase 2 明确依赖入口、发现方式和发布声明。`supporting/reference-only` 依赖只能作为迁移或替换设计参考。`excluded` 依赖不得进入核心库 target，也不得成为 E2E 通过的隐式前置条件。

## Release Checklist

- [ ] 根目录或发布包包含 Apache-2.0 `LICENSE`。
- [ ] 根目录或发布包包含 `NOTICE`，保留 AWS 原 NOTICE，并添加 libmicrortc derivative notice。
- [ ] 所有派生文件都能通过 `SOURCE-MANIFEST.md` 追溯 `source_path`、`target_path`、`origin_commit`、`derivation` 和 `notes`。
- [ ] 第三方依赖声明包含 OpenSSL、libsrtp、usrsctp、libwebsockets、jsmn、kvsCommonLws/PIC、mbedTLS、AWS SDK C++、googletest、benchmark 和 gperftools。
- [ ] `Signaling`、AWS SDK C++、AWS credential/storage/KVS 服务路径仍为 `excluded`。
- [ ] 发布说明区分 `runtime/core`、`reference-only`、`test-only` 和 `excluded`。
- [ ] 若某个依赖被替换或删除，更新本文件和构建文档，不只修改 CMake。

## 后续维护

Phase 2+ 每次引入或移除来源文件、第三方依赖、license notice 或 target 链接，都必须复查本文件。合规文档不是一次性材料；它是后续剥离是否仍然可审计的门禁。
