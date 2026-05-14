# Phase 1: 基线、范围与合规边界 - Research

**Research completed:** 2026-05-12T18:58:55+08:00
**Status:** Ready for planning

## RESEARCH COMPLETE

## 研究问题

Phase 1 需要回答的是：如何在不搬迁协议栈代码的前提下，把本地 `reflib/kvs-webrtc-sdk` 固定为唯一剥离基线，并给后续 Phase 2+ 的代码搬迁建立可审计、可复查、可持续追加的来源与合规约束。

本阶段不应改造 CMake、不应复制协议实现、不应设计最终 API。它的产物应是阶段目录内的规划/审计文档，为后续代码进入 `libmicrortc` 时提供硬约束。

## 本地基线事实

| 项 | 观察结果 |
|----|----------|
| 参考库路径 | `reflib/kvs-webrtc-sdk` |
| CMake 项目版本 | `KinesisVideoWebRTCClient VERSION 1.18.1` |
| Git 描述 | `v1.18.1` |
| 当前提交 | `9eebcc4` |
| 工作区状态 | `git -C reflib/kvs-webrtc-sdk status --short` 无输出，当前干净 |
| 许可证文件 | `reflib/kvs-webrtc-sdk/LICENSE` |
| NOTICE 文件 | `reflib/kvs-webrtc-sdk/NOTICE` |

关键约束：所有研究、搬迁和差异讨论必须以这个本地目录为准，不默认采用 GitHub 上游最新状态。

## 源码结构观察

`reflib/kvs-webrtc-sdk/src/source` 当前顶层模块包括：

| 模块 | 初始分类 | 规划含义 |
|------|----------|----------|
| `Ice` | `core` | ICE、STUN/TURN candidate 与网络连接路径核心候选 |
| `Stun` | `core` | STUN/TURN 相关消息处理核心候选 |
| `Crypto` | `core` | DTLS/TLS/OpenSSL/mbedTLS 相关实现，v1 优先 OpenSSL 路径 |
| `Srtp` | `core` | SRTP 会话核心候选 |
| `Sctp` | `core` | DataChannel 所需 SCTP 核心候选 |
| `Sdp` | `core` | SDP 序列化/反序列化核心候选 |
| `Rtp` | `core` | RTP packet 相关核心候选 |
| `Rtcp` | `core` | RTCP、rolling buffer、NACK 相关候选 |
| `PeerConnection` | `core` | PeerConnection、DataChannel、JitterBuffer、SessionDescription 等高层协议编排候选 |
| `Signaling` | `excluded` | AWS/KVS signaling client，不能进入核心库依赖 |
| `Threadpool` | `supporting/reference-only` | 可参考 AWS 线程池模型，但不是长期核心边界 |
| `Metrics` | `supporting/reference-only` | 可参考统计能力，不属于 Phase 1 核心边界 |

`src/include/com/amazonaws/kinesis/video/webrtcclient` 当前公开头文件为 `Include.h`、`NullableDefs.h`、`Stats.h`。这些文件会影响后续 AWS 风格 v1 API 裁剪，但 Phase 1 只记录来源，不做裁剪。

`tst/` 下协议相关测试可作为后续迁移参考，包括 `Ice*`、`Stun*`、`TurnConnection*`、`Dtls*`、`Srtp*`、`Rtp*`、`Rtcp*`、`Sdp*`、`DataChannel*`、`PeerConnection*`。`Signaling*Test` 只作为 excluded 模块参考，不能成为核心验收依据。

## 构建与依赖观察

顶层 `CMakeLists.txt` 的关键选项包括：

| 选项 | 默认值 | Phase 1 记录意义 |
|------|--------|------------------|
| `BUILD_DEPENDENCIES` | `ON` | 参考库可从源码拉取第三方依赖；后续独立构建需要重新决定依赖入口 |
| `USE_OPENSSL` | `ON` | v1 优先 OpenSSL 路径 |
| `USE_MBEDTLS` | `OFF` | mbedTLS 是参考路径，不是 v1 优先路径 |
| `BUILD_SAMPLE` | `ON` | sample 属于应用/demo 层，不能进入核心库 |
| `ENABLE_DATA_CHANNEL` | `ON` | DataChannel 属于 v1 PeerConnection 能力 |
| `ENABLE_KVS_THREADPOOL` | `OFF` | signaling 线程池支持不进入核心边界 |
| `BUILD_TEST` | `OFF` | 测试默认不构建，但协议测试是后续迁移参考 |
| `BUILD_BENCHMARK` | `OFF` | benchmark 不属于 v1 核心交付 |

顶层构建里 `kvsWebrtcClient` 链接 `kvspicUtils`、`kvspicState`、OpenSSL、libsrtp、usrsctp 等；`kvsWebrtcSignalingClient` 额外链接 `kvsCommonLws` 和 libwebsockets。Phase 1 应明确：`kvsWebrtcSignalingClient` 相关路径是剥离对象，不应成为 libmicrortc 核心库依赖。

第三方依赖声明来自 `reflib/kvs-webrtc-sdk/CMake/Dependencies`：

| 依赖 | 声明来源 | 版本/提交 | 初始 v1 状态 |
|------|----------|-----------|--------------|
| OpenSSL | `libopenssl-CMakeLists.txt` | `OpenSSL_1_1_1t` | `runtime/core` |
| libsrtp | `libsrtp-CMakeLists.txt` | `bd0f27ec0e299ad101a396dde3f7c90d48efc8fc` | `runtime/core` |
| usrsctp | `libusrsctp-CMakeLists.txt` | `1ade45cbadfd19298d2c47dc538962d4425ad2dd` | `runtime/core` |
| libwebsockets | `libwebsockets-CMakeLists.txt` | `v4.3.5` | `excluded` 或 `supporting/reference-only`，仅 signaling 需要 |
| jsmn | `libjsmn-CMakeLists.txt` | `v1.0.0` | `runtime/core` 候选，PeerConnection/SDP JSON 辅助路径需后续确认 |
| kvsCommonLws/PIC | `libkvsCommonLws-CMakeLists.txt` | `v1.6.1` | `supporting/reference-only` |
| mbedTLS | `libmbedtls-CMakeLists.txt` | `v2.28.8` 或 `v3.6.3` | `reference-only`，v1 非优先 |
| AWS SDK C++ | `libawscpp-CMakeLists.txt` | `1.11.741` | `excluded` |
| googletest | `libgtest-CMakeLists.txt` | `release-1.12.1` | `test-only` |
| benchmark | `libbenchmark-CMakeLists.txt` | `v1.5.1` | `test-only` 或 `excluded` |
| gperftools | `libgperftools-CMakeLists.txt` | `gperftools-2.8.zip` | `excluded` 或可选 profiling reference |

## 建议产物结构

Phase 1 应生成三份阶段文档，全部先放在 `.planning/phases/01-/`：

| 文件 | 作用 |
|------|------|
| `.planning/phases/01-/BASELINE.md` | 记录本地参考库版本、提交、dirty 状态、关键目录、关键构建选项、核心候选与排除模块 |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | 定义后续派生文件的文件级来源追溯表；Phase 1 可先写表头、字段定义和分类规则，不预填全量 reflib 源码 |
| `.planning/phases/01-/COMPLIANCE.md` | 记录 Apache-2.0/NOTICE 保留策略、第三方依赖声明、发布包合规检查清单 |

不建议在 Phase 1 修改根目录 LICENSE/NOTICE，除非用户特别要求。当前讨论决策是先在阶段目录建立策略，后续发布/整理阶段再决定根目录落地。

## Manifest 建议字段

`SOURCE-MANIFEST.md` 的最低字段应严格包含：

| 字段 | 含义 |
|------|------|
| `source_path` | 来源文件在 `reflib/kvs-webrtc-sdk` 下的相对路径 |
| `target_path` | 后续进入 libmicrortc 的目标路径 |
| `origin_commit` | 来源提交，Phase 1 基线为 `9eebcc4` |
| `derivation` | `copied`、`trimmed`、`renamed`、`rewritten-derived`、`reference-only` 等派生方式 |
| `notes` | 裁剪、保留、排除或后续确认说明 |

可增加但不强制的列：`module_class`、`requirement`、`phase_added`、`review_status`。每条记录不要求文件 hash，避免早期表格过重。

## 合规策略

Apache-2.0 兼容路径应包括：

- 发布包或根目录最终必须包含 Apache-2.0 `LICENSE`。
- `NOTICE` 应保留 AWS KVS WebRTC SDK 原 NOTICE，并添加 libmicrortc 对 AWS KVS WebRTC SDK 的派生说明。
- 每个从参考库搬迁或派生的文件必须可通过 `SOURCE-MANIFEST.md` 追溯到 `reflib/kvs-webrtc-sdk` 的来源路径。
- 第三方依赖必须按依赖名、来源、版本/commit、用途、v1 状态记录。
- `Signaling`、AWS SDK C++、AWS credential/storage/KVS 服务路径必须被明确标注为 excluded，避免后续无意进入核心库。

## 风险与规划约束

| 风险 | 后果 | Phase 1 约束 |
|------|------|--------------|
| 使用 GitHub 上游最新状态而不是本地 `reflib` | 来源追溯失真，后续搬迁无法复现 | 文档必须反复声明本地基线优先 |
| 预先列全量源码 manifest | 产生维护负担，实际派生文件反而难审 | manifest 只定义格式和后续追加规则 |
| 将 `Signaling` 当作核心协议依赖 | 核心库继续绑定 AWS/KVS 应用层 | `Signaling` 必须在边界表中列为 `excluded` |
| 忽略 `kvspicUtils` / `kvspicState` 依赖 | 后续构建剥离会低估公共库耦合 | 将 PIC 公共库列为 `supporting/reference-only` |
| 合规文档只写 LICENSE 不写 NOTICE/第三方依赖 | 发布合规路径不完整 | `COMPLIANCE.md` 必须覆盖 LICENSE、NOTICE、第三方依赖和来源说明 |

## Validation Architecture

Phase 1 是文档/审计类阶段，验证应以文件存在、关键内容断言和本地基线命令为主。推荐验证命令：

- `test -f .planning/phases/01-/BASELINE.md`
- `test -f .planning/phases/01-/SOURCE-MANIFEST.md`
- `test -f .planning/phases/01-/COMPLIANCE.md`
- `git -C reflib/kvs-webrtc-sdk status --short`
- `git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD`
- `rg -n "reflib/kvs-webrtc-sdk|9eebcc4|v1.18.1|Signaling|excluded|Apache-2.0|NOTICE|source_path|target_path|origin_commit|derivation|notes" .planning/phases/01-/`

计划中的 acceptance criteria 应避免“文档合理”这类主观表述，改用具体字符串、表格列和命令输出断言。

## 规划建议

推荐用一个顺序执行计划覆盖 Phase 1，因为三个文档共享同一批本地基线事实，拆成并行计划容易造成字段和分类不一致。该计划应包括：

1. 重新读取并记录本地 `reflib/kvs-webrtc-sdk` 基线事实。
2. 创建 `BASELINE.md`，覆盖路径、版本、提交、dirty 状态、目录结构、模块边界、关键 CMake 选项和依赖入口。
3. 创建 `SOURCE-MANIFEST.md`，定义文件级 manifest 表格、派生类型和后续追加规则。
4. 创建 `COMPLIANCE.md`，覆盖 LICENSE/NOTICE/第三方依赖/排除模块策略。
5. 用命令断言验证三份文档满足 `BASE-01` 到 `BASE-04`。
