# Phase 1 Baseline: 本地 KVS WebRTC SDK 基线

## 基线结论

`libmicrortc` 的剥离基线固定为本地目录 `reflib/kvs-webrtc-sdk`。后续研究、搬迁、裁剪、重写派生和差异讨论都必须先读取这个本地目录；GitHub upstream latest 不是默认参考，也不能替代本地快照。

本阶段只记录基线、范围和合规边界，不搬迁协议栈代码，不重写构建系统，不设计最终 API。

## 复查命令

| 检查项 | 命令 | 当前结果 |
|--------|------|----------|
| 工作区状态 | `git -C reflib/kvs-webrtc-sdk status --short` | 无输出，干净 |
| 当前提交 | `git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD` | `9eebcc4` |
| 版本描述 | `git -C reflib/kvs-webrtc-sdk describe --tags --always --dirty` | `v1.18.1` |
| 模块目录 | `find reflib/kvs-webrtc-sdk/src/source -maxdepth 1 -type d | sort` | 见下方模块表 |

## 本地来源事实

| 字段 | 值 |
|------|----|
| source root | `reflib/kvs-webrtc-sdk` |
| CMake project | `KinesisVideoWebRTCClient` |
| CMake project version | `1.18.1` |
| tag/version | `v1.18.1` |
| origin commit | `9eebcc4` |
| dirty status | clean |
| license file | `reflib/kvs-webrtc-sdk/LICENSE` |
| notice file | `reflib/kvs-webrtc-sdk/NOTICE` |

## 关键目录结构

| 路径 | 基线观察 | Phase 1 分类 |
|------|----------|--------------|
| `reflib/kvs-webrtc-sdk/src/source` | WebRTC client 主要 C 源码目录 | 需要逐模块分类 |
| `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient` | AWS 风格公开头文件 | 后续 API 裁剪来源 |
| `reflib/kvs-webrtc-sdk/tst` | 协议和 signaling 测试 | `supporting/reference-only` |
| `reflib/kvs-webrtc-sdk/samples` | sample、固定媒体帧和 demo 参考 | `supporting/reference-only` 或 excluded |
| `reflib/kvs-webrtc-sdk/CMake/Dependencies` | 第三方依赖 ExternalProject 声明 | 合规和依赖策略来源 |

## 公开头文件

| 来源头文件 | 后续用途 |
|------------|----------|
| `src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | v1 AWS 风格 API 裁剪的主要参考 |
| `src/include/com/amazonaws/kinesis/video/webrtcclient/NullableDefs.h` | API 类型和 nullable 语义参考 |
| `src/include/com/amazonaws/kinesis/video/webrtcclient/Stats.h` | 统计结构参考，是否进入 v1 由后续阶段确认 |

## 模块边界

| 模块或路径 | 分类 | 说明 |
|------------|------|------|
| `src/source/Ice` | `core` | ICE、candidate、STUN/TURN 路径的核心候选 |
| `src/source/Stun` | `core` | STUN/TURN 消息处理核心候选 |
| `src/source/Crypto` | `core` | DTLS/TLS 相关实现，v1 优先 OpenSSL 路径 |
| `src/source/Srtp` | `core` | SRTP 会话核心候选 |
| `src/source/Sctp` | `core` | DataChannel 所需 SCTP 核心候选 |
| `src/source/Sdp` | `core` | SDP 序列化和反序列化核心候选 |
| `src/source/Rtp` | `core` | RTP packet 和 codec packetization 核心候选 |
| `src/source/Rtcp` | `core` | RTCP、rolling buffer、NACK 相关候选 |
| `src/source/PeerConnection` | `core` | PeerConnection、DataChannel、JitterBuffer、SessionDescription 等编排候选 |
| `src/source/Signaling` | `excluded` | AWS/KVS signaling client，不能成为核心库依赖 |
| `src/source/Threadpool` | `supporting/reference-only` | 可参考 AWS 线程模型，长期不属于核心边界 |
| `src/source/Metrics` | `supporting/reference-only` | 可参考统计能力，是否进入核心由后续需求驱动 |
| `samples/common/StaticMedia.c` | `supporting/reference-only` | 后续 demo/E2E 固定媒体输入参考 |
| `samples/h264SampleFrames` | `supporting/reference-only` | H264 测试帧参考，不属于核心库 |
| `samples/opusSampleFrames` | `supporting/reference-only` | Opus 测试帧参考，不属于核心库 |
| `tst/` 协议相关测试 | `supporting/reference-only` | 后续迁移协议单元测试的行为参考 |
| `tst/Signaling*Test` | `excluded` | 只能作为 excluded 模块参考，不能成为核心验收 |
| `kvsCommonLws` / PIC 能力 | `supporting/reference-only` | allocator、logging、state、utils 等可参考或临时迁移，但不是长期核心边界 |

## 关键 CMake 选项

| 选项 | 默认值 | Phase 1 观察 |
|------|--------|--------------|
| `BUILD_DEPENDENCIES` | `ON` | 参考库可拉取第三方依赖，后续独立构建需要重新设计入口 |
| `USE_OPENSSL` | `ON` | v1 优先 OpenSSL 路径 |
| `USE_MBEDTLS` | `OFF` | mbedTLS 仅保留为参考路径 |
| `BUILD_STATIC_LIBS` | `OFF` | libmicrortc v1 目标是静态库，需要后续 Phase 2 明确 |
| `BUILD_SAMPLE` | `ON` | sample 属于应用/demo 层，不进入核心库 |
| `ENABLE_DATA_CHANNEL` | `ON` | DataChannel 是 v1 PeerConnection 能力的一部分 |
| `ENABLE_KVS_THREADPOOL` | `OFF` | signaling 线程池支持不进入核心边界 |
| `BUILD_TEST` | `OFF` | 测试默认不构建，但协议测试是迁移参考 |
| `BUILD_BENCHMARK` | `OFF` | benchmark 不属于 v1 核心交付 |
| `LINK_PROFILER` | `OFF` | gperftools 只作为 profiling reference 或 excluded |

## Source Target 和 Link 观察

顶层 `CMakeLists.txt` 当前将 `src/source/Crypto`、`Ice`、`PeerConnection`、`Rtcp`、`Rtp`、`Sdp`、`Srtp`、`Stun`、`Metrics` 等源文件纳入 `kvsWebrtcClient`。当 `ENABLE_DATA_CHANNEL=ON` 时，`src/source/PeerConnection/DataChannel.c` 和 `src/source/Sctp/Sctp.c` 也进入该 target。

`kvsWebrtcClient` 当前链接 `kvspicUtils`、`kvspicState`、线程库、OpenSSL、libsrtp、usrsctp、mbedTLS 和可选 gperftools。这里暴露出公共 PIC 能力耦合，Phase 2 需要决定是临时迁移、裁剪替换，还是建立 libmicrortc 自有基础设施。

`kvsWebrtcSignalingClient` 单独 glob `src/source/Signaling/*.c`，并链接 `kvsCommonLws` 和 libwebsockets。该 target 属于 AWS/KVS 应用层 signaling 方向，Phase 1 明确标注为 `excluded`。

`src/CMakeLists.txt` 中的备用/子目录 target 会递归包含 `source/*.c` 并链接 `client heap trace view mkvgen utils state jsmn crypto ssl`。这些依赖体现出参考库与 PIC/Producer C 公共库的历史耦合，不能被误认为 libmicrortc 的长期核心边界。

## 依赖入口快照

| 依赖 | 声明来源 | 版本或提交 | 初始 v1 状态 |
|------|----------|------------|--------------|
| OpenSSL | `CMake/Dependencies/libopenssl-CMakeLists.txt` | `OpenSSL_1_1_1t` | `runtime/core` |
| libsrtp | `CMake/Dependencies/libsrtp-CMakeLists.txt` | `bd0f27ec0e299ad101a396dde3f7c90d48efc8fc` | `runtime/core` |
| usrsctp | `CMake/Dependencies/libusrsctp-CMakeLists.txt` | `1ade45cbadfd19298d2c47dc538962d4425ad2dd` | `runtime/core` |
| libwebsockets | `CMake/Dependencies/libwebsockets-CMakeLists.txt` | `v4.3.5` | `excluded` 或 `supporting/reference-only` |
| jsmn | `CMake/Dependencies/libjsmn-CMakeLists.txt` | `v1.0.0` | `runtime/core` 候选 |
| kvsCommonLws/PIC | `CMake/Dependencies/libkvsCommonLws-CMakeLists.txt` | `v1.6.1` | `supporting/reference-only` |
| mbedTLS | `CMake/Dependencies/libmbedtls-CMakeLists.txt` | `v2.28.8` 或 `v3.6.3` | `reference-only` |
| AWS SDK C++ | `CMake/Dependencies/libawscpp-CMakeLists.txt` | `1.11.741` | `excluded` |
| googletest | `CMake/Dependencies/libgtest-CMakeLists.txt` | `release-1.12.1` | `test-only` |
| benchmark | `CMake/Dependencies/libbenchmark-CMakeLists.txt` | `v1.5.1` | `test-only` 或 `excluded` |
| gperftools | `CMake/Dependencies/libgperftools-CMakeLists.txt` | `gperftools-2.8.zip` | `excluded` 或 profiling reference |

## 下游规则

1. 后续任何代码搬迁都必须从 `reflib/kvs-webrtc-sdk` 的本地路径出发，并在 `SOURCE-MANIFEST.md` 记录 `source_path`、`target_path`、`origin_commit`、`derivation` 和 `notes`。
2. `origin_commit` 默认使用本基线提交 `9eebcc4`；若未来显式更新基线，必须先更新本文件和合规记录。
3. `Signaling`、AWS SDK C++、AWS credential/storage/KVS 服务路径不得进入核心库依赖。
4. `kvsCommonLws` / PIC 公共能力只能以 `supporting/reference-only` 进入讨论，后续如果临时迁移，必须明确裁剪和替换计划。
5. 核心库只处理编码后媒体帧，不把 sample、固定媒体帧、采集、编码、GStreamer adapter 或应用层 signaling 纳入核心边界。
6. Phase 2 之后所有搬迁 PR 都应能从本文件、`SOURCE-MANIFEST.md` 和 `COMPLIANCE.md` 找到来源、边界和合规依据。
