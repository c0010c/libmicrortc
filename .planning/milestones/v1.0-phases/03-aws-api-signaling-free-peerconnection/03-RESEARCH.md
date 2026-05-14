# Phase 3: AWS 风格 API 与 signaling-free PeerConnection - Research

## User Constraints

### Locked decisions from CONTEXT.md

- D-01: Phase 3 采用“薄裁剪 AWS API”策略：保留 AWS/WebRTC 的主要使用模型和函数语义，但不追求 AWS SDK 源码级兼容。
- D-02: 公共命名从 Phase 3 起向 `MRTC_*` / `mrtc_*` 收敛；AWS 公共头只作为能力集合、调用顺序和语义参考。
- D-03: 公共头不暴露 AWS/PIC 基础类型，例如 `STATUS`、`PCHAR`、`PVOID`、`BOOL`。继续使用 `MRTC_STATUS`、标准 C 类型和项目自有 opaque handle。
- D-04: Phase 3 应保留 PeerConnection、SDP、ICE candidate、配置和回调相关 API；不把 transceiver/media/DataChannel 正式可调用 API 纳入本阶段范围。
- D-05: PeerConnection 对外使用 opaque handle，公共头只暴露类似 `MRTC_PEER_CONNECTION_HANDLE` 的不透明句柄和 create/free 函数。
- D-06: create 流程一次性传入 config、callback table 和 `user_data`。
- D-07: PeerConnection 内部结构保持隐藏，不在 public header 暴露字段。
- D-08: Phase 3 应扩展 `MRTC_STATUS` 的最小状态集，至少考虑 `MRTC_STATUS_NOT_IMPLEMENTED`、`MRTC_STATUS_INVALID_STATE`、`MRTC_STATUS_PARSE_ERROR`。
- D-09: SDP 和 ICE candidate 使用纯字符串交换边界。
- D-10: 核心库不得内置 signaling adapter、signaling transport callback 或 KVS signaling client 依赖。
- D-11: SDP API 保留 AWS/WebRTC 使用模型的薄裁剪语义：`create_offer`、`create_answer`、`set_local_description`、`set_remote_description`，但使用 `mrtc_*` 命名和字符串 SDP 输入输出。
- D-12: ICE candidate API 支持 trickle candidate：提供 `on_ice_candidate` 回调和 `mrtc_peer_connection_add_ice_candidate()` 字符串输入接口。
- D-13: Phase 3 以 C 端 Answerer 路径可用为主：remote offer 字符串进入核心库，生成 local answer 字符串，并完成 set remote/local description 的基础调用链验证。
- D-14: Offerer API 在 Phase 3 需要存在并可编译；如果协议依赖尚未就绪，可明确返回 `MRTC_STATUS_NOT_IMPLEMENTED`。
- D-15: Phase 3 不要求完整 Chrome E2E、真实 ICE connected、DTLS/SRTP 或媒体流动。
- D-16: Phase 3 SDP 测试应覆盖基础 parse/serialize round-trip，以及 Answerer SDP 生成路径。
- D-17: 测试目标是证明 SDP 模块已脱离 AWS/KVS signaling client，并能支持 remote offer -> local answer 的基础 API 流程。
- D-18: Phase 3 不要求完整 Chrome SDP 兼容矩阵或浏览器自动化互通。

### the agent's Discretion

Planner 可以决定具体 public header 拆分方式、函数名细节、opaque handle typedef 写法、callback table 字段命名、config 结构的最小字段集合、SDP 字符串内存所有权规则和测试 fixture 组织方式。但不得改变以下约束：薄裁剪 AWS 使用模型、不暴露 AWS/PIC 基础类型、核心库 signaling-free、Answerer 先行、Offerer 接口预留、Phase 3 不正式暴露 transceiver/media/DataChannel 可调用 API。

### Deferred Ideas

- transceiver、media、`writeFrame` 类 API 留给 Phase 5。
- DataChannel 创建、打开、关闭和消息收发 API 留给 Phase 4。
- ICE/STUN/TURN 真实连接、DTLS/SRTP、安全通道和 TURN relay 验证留给 Phase 4/6。
- Chrome 自动化 E2E、DataChannel E2E、H264/Opus 双向媒体流动验收留给 Phase 6。
- 长期 libmicrortc 风格 API 清理和 AWS 兼容层是否存在，留给 v2 或后续 API 演进阶段。

## Project Constraints (from AGENTS.md)

- 所有 Markdown 文档尽可能使用中文编写。[VERIFIED: ./AGENTS.md]
- 剥离基线必须以本地 `reflib/kvs-webrtc-sdk` 为准，不默认使用 GitHub 上游最新版本。[VERIFIED: ./AGENTS.md]
- v1 优先 Linux x86_64、Chrome、H264、Opus、TURN relay、双向音视频和自动化 E2E 验收。[VERIFIED: ./AGENTS.md]
- 核心库不内置应用层 signaling，不做媒体采集和编码，只处理编码后媒体帧。[VERIFIED: ./AGENTS.md]

## Baseline Availability Finding

- 当前工作区未找到 `reflib/kvs-webrtc-sdk`、`.gitmodules` 或任何可替代的本地 KVS WebRTC SDK 路径。[VERIFIED: `test -d reflib/kvs-webrtc-sdk`, `find . -maxdepth 3`, `git ls-files`]
- Phase 1 文档记录的基线是 `reflib/kvs-webrtc-sdk`，tag/version `v1.18.1`，origin commit `9eebcc4`，且后续裁剪必须从该本地路径出发。[VERIFIED: `.planning/phases/01-/BASELINE.md`]
- 因为 Phase 3 的 API/SDP/PeerConnection 裁剪都依赖 AWS 公共头和本地 `Sdp` / `PeerConnection` 源码，恢复本地基线目录是执行前置条件，不是普通参考资料缺失。[HIGH]

## Standard Stack

- 语言与 ABI：继续使用 C99、公开 C ABI、`extern "C"` guards 和 `MRTC_STATUS` 风格状态码。[VERIFIED: `CMakeLists.txt`, `include/micrortc/micrortc.h`]
- 构建系统：继续扩展当前根目录 `CMakeLists.txt` 的 `micrortc` 静态库 target，不引入参考库 target 或 `add_subdirectory(reflib/...)`。[VERIFIED: Phase 2 summary]
- 测试框架：继续使用 CTest 加小型 C 可执行测试；Phase 3 可新增 `tests/sdp/` 和 `tests/peer_connection/`，不需要引入 googletest。[VERIFIED: current `tests/smoke`, `tests/package-consumer`]
- 来源追溯：凡是复制、裁剪、改名或 rewritten-derived AWS 文件，必须追加 `.planning/phases/01-/SOURCE-MANIFEST.md` 记录，origin commit 默认 `9eebcc4`。[VERIFIED: `.planning/phases/01-/SOURCE-MANIFEST.md`]

## Architecture Patterns

- Public API 建议采用 umbrella header `include/micrortc/micrortc.h` include 专项头 `include/micrortc/peer_connection.h`，以保持 Phase 2 package consumer 的单入口使用体验。[MEDIUM: inferred from current header and package consumer]
- PeerConnection 对象建议实现为 private struct，例如 `struct MRTC_PEER_CONNECTION` 在 `src/peer_connection.c` 内部定义，public typedef 为 `typedef struct MRTC_PEER_CONNECTION *MRTC_PEER_CONNECTION_HANDLE;`。[MEDIUM: C opaque handle convention + D-05/D-07]
- SDP 字符串内存所有权建议采用调用者提供 buffer 的 C API：`char *buffer, size_t buffer_len, size_t *required_len`。这避免 Phase 3 过早设计跨 allocator 的 `free` 语义，也不引入 AWS/PIC allocator。[MEDIUM: derived from D-03 and current no-allocator codebase]
- Answerer 流程建议以状态机保护调用顺序：初始 -> remote description set -> answer created -> local description set；错误返回 `MRTC_STATUS_INVALID_STATE`。[MEDIUM]
- SDP parser 在 Phase 3 不需要完整 RFC 兼容；计划应实现足够的行级解析/序列化能力来保存 session lines、media sections、attributes，并生成代表性 answer。复杂 ICE/DTLS/SRTP 参数填充留给 Phase 4。[MEDIUM: constrained by D-15/D-18]

## Don't Hand-Roll

- 不手写 signaling transport、WebSocket/HTTP 交换、KVS signaling callback 或应用层 signaling adapter。[VERIFIED: D-09/D-10, PROTO-07]
- 不把 AWS/PIC 基础类型复制进 public header。[VERIFIED: D-03]
- 不提前实现 DataChannel、transceiver、`writeFrame`、RTP/RTCP、DTLS/SRTP、真实 ICE 连接。[VERIFIED: deferred ideas]
- 不用 GitHub upstream latest 补全缺失的本地参考库事实。[VERIFIED: AGENTS.md + BASELINE.md]

## Common Pitfalls

- **参考库缺失导致伪裁剪:** 如果 executor 直接凭记忆写 AWS 风格 API，会违反本地基线规则。计划必须先阻断并要求恢复 `reflib/kvs-webrtc-sdk`。[HIGH]
- **公共类型污染:** `STATUS`、`PCHAR`、`PVOID`、`BOOL`、`RtcPeerConnection` 等 AWS/PIC 类型如果出现在 `include/micrortc/*.h`，Phase 3 API 边界失败。[HIGH]
- **Signaling 间接耦合:** `src/source/Signaling`、`kvsWebrtcSignalingClient`、libwebsockets、AWS SDK C++、credential/storage 不能被 CMake 或 include 链路引用。[HIGH]
- **Offerer 过度承诺:** `mrtc_peer_connection_create_offer()` 需要存在并可编译，但可返回 `MRTC_STATUS_NOT_IMPLEMENTED`；不要为它提前接入真实 ICE/DTLS 媒体状态。[HIGH]
- **SDP 测试太弱:** 只测试字符串透传不足以覆盖 `PROTO-06`；必须至少有 parse/serialize round-trip 和 remote offer -> local answer API 流程测试。[HIGH]
- **Manifest 遗漏:** 如果 Phase 3 从 AWS `Include.h`、`Sdp` 或 `PeerConnection` 派生任何代码而未更新 manifest，会破坏 BASE-04 后续审计路径。[HIGH]

## Recommended File Layout

- `include/micrortc/micrortc.h` — umbrella header，继续导出基础 init/version/status，并 include PeerConnection API。
- `include/micrortc/peer_connection.h` — opaque handle、config、callback table、SDP/candidate API。
- `src/peer_connection.c` — PeerConnection lifecycle、状态机、callback storage、answerer flow。
- `src/sdp.c` 和 `src/sdp.h` — private SDP parse/serialize/answer helper。
- `tests/sdp/test_sdp_roundtrip.c` — SDP parser/serializer 行为测试。
- `tests/peer_connection/test_peer_connection_api.c` — public API 编译、Answerer flow、Offerer `NOT_IMPLEMENTED`、candidate API 边界测试。
- `tests/fixtures/minimal_offer.sdp` — 代表性 remote offer fixture。

## Validation Architecture

Phase 3 的验证应分三层：

1. **Source boundary checks**
   - `test -d reflib/kvs-webrtc-sdk`
   - `git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD` 输出 `9eebcc4`
   - `rg -n "kvsWebrtcSignalingClient|src/source/Signaling|libwebsockets|AWS SDK|PCHAR|PVOID|BOOL|\\bSTATUS\\b" CMakeLists.txt include src tests` 不匹配禁止项；其中 `MRTC_STATUS` 是允许项，grep 需避免误判。

2. **Build and package checks**
   - `cmake -S . -B build -DMRTC_BUILD_TESTS=ON`
   - `cmake --build build`
   - `ctest --test-dir build --output-on-failure`
   - `cmake --install build --prefix build/install`
   - `cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"`
   - `cmake --build build/package-consumer && ./build/package-consumer/package_consumer`

3. **Behavior checks**
   - SDP round-trip test exits 0 and asserts parsed/serialized output retains `v=0`, at least one `m=` section and selected `a=` attributes.
   - PeerConnection API test exits 0 and asserts invalid args return `MRTC_STATUS_INVALID_ARG`, invalid ordering returns `MRTC_STATUS_INVALID_STATE`, malformed SDP returns `MRTC_STATUS_PARSE_ERROR`, Offerer returns `MRTC_STATUS_NOT_IMPLEMENTED`, Answerer path writes a non-empty answer containing `v=0` and `m=`.
   - Candidate API accepts a syntactically representative candidate string without requiring actual ICE transport.

## Open Questions For Execution

- 恢复 `reflib/kvs-webrtc-sdk` 的具体机制不在当前仓库中；执行 Phase 3 前需要用户提供或恢复该本地目录。[VERIFIED: current workspace]
- 如果执行时发现 AWS SDP 模块大量依赖 PIC allocator/logging/list/string helpers，Phase 3 应优先 rewritten-derived 一个小型 SDP 边界，而不是迁移 PIC 公共库进入核心 target。[MEDIUM]

---
*Research created: 2026-05-12*
