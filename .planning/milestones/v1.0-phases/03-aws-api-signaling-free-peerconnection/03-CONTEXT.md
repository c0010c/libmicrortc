# Phase 3: AWS 风格 API 与 signaling-free PeerConnection - Context

**Gathered:** 2026-05-12T21:54:08+08:00
**Status:** Ready for planning

<domain>
## Phase Boundary

本阶段从 AWS KVS WebRTC C SDK 公共头文件和 PeerConnection/SDP 模块中裁剪 v1 API，建立 `libmicrortc` 的 signaling-free PeerConnection 公共边界。交付重点是：PeerConnection 生命周期 API、配置与回调模型、SDP offer/answer 字符串接口、ICE candidate 字符串接口、Answerer 先行路径、Offerer 接口预留，以及 SDP 模块脱离 KVS signaling client 的基础验证。

本阶段不搬迁完整 ICE/STUN/TURN、DTLS、SRTP、SCTP/DataChannel、RTP/RTCP 或 H264/Opus 媒体路径；不把 AWS/KVS signaling client、credential/storage、libwebsockets signaling adapter 或应用层 signaling 传输放入核心库；不正式暴露 transceiver、media、DataChannel 可调用 API。

**Planning 前置风险:** 当前工作区未找到 `reflib/kvs-webrtc-sdk` 目录。Phase 3 研究、规划和实现必须先恢复本地基线目录，并以该本地基线为准；不得用 GitHub upstream latest 替代。

</domain>

<decisions>
## Implementation Decisions

### 公共 API 裁剪边界

- **D-01:** Phase 3 采用“薄裁剪 AWS API”策略：保留 AWS/WebRTC 的主要使用模型和函数语义，但不追求 AWS SDK 源码级兼容。
- **D-02:** 公共命名从 Phase 3 起向 `MRTC_*` / `mrtc_*` 收敛；AWS 公共头只作为能力集合、调用顺序和语义参考。
- **D-03:** 公共头不暴露 AWS/PIC 基础类型，例如 `STATUS`、`PCHAR`、`PVOID`、`BOOL`。继续使用 `MRTC_STATUS`、标准 C 类型和项目自有 opaque handle。
- **D-04:** Phase 3 应保留 PeerConnection、SDP、ICE candidate、配置和回调相关 API；不把 transceiver/media/DataChannel 正式可调用 API 纳入本阶段范围。

### PeerConnection 生命周期与回调模型

- **D-05:** PeerConnection 对外使用 opaque handle，公共头只暴露类似 `MRTC_PEER_CONNECTION_HANDLE` 的不透明句柄和 create/free 函数。
- **D-06:** create 流程一次性传入 config、callback table 和 `user_data`，例如 `mrtc_peer_connection_create(const MRTC_PEER_CONNECTION_CONFIG*, const MRTC_PEER_CONNECTION_CALLBACKS*, void* user_data, MRTC_PEER_CONNECTION_HANDLE*)` 这一类模型。
- **D-07:** PeerConnection 内部结构保持隐藏，不在 public header 暴露字段；planner 可决定内部文件拆分和 private struct 命名。
- **D-08:** Phase 3 应扩展 `MRTC_STATUS` 的最小状态集，至少考虑 `MRTC_STATUS_NOT_IMPLEMENTED`、`MRTC_STATUS_INVALID_STATE`、`MRTC_STATUS_PARSE_ERROR`，用于占位 API、调用顺序错误和 SDP/ICE 字符串解析失败。

### SDP 与 ICE Candidate 交换边界

- **D-09:** SDP 和 ICE candidate 使用纯字符串交换边界。核心库只生成和消费 SDP/candidate 字符串，不关心应用层通过 WebSocket、HTTP、文件、stdio 或测试 harness 如何交换。
- **D-10:** 核心库不得内置 signaling adapter、signaling transport callback 或 KVS signaling client 依赖。
- **D-11:** SDP API 保留 AWS/WebRTC 使用模型的薄裁剪语义：`create_offer`、`create_answer`、`set_local_description`、`set_remote_description`，但使用 `mrtc_*` 命名和字符串 SDP 输入输出。
- **D-12:** ICE candidate API 支持 trickle candidate：提供 `on_ice_candidate` 回调和 `mrtc_peer_connection_add_ice_candidate()` 这一类字符串输入接口。Phase 3 可先锁定 API 和解析边界，真实 ICE 行为主要由 Phase 4 接协议栈完成。

### Answerer 先行与 Offerer 预留

- **D-13:** Phase 3 以 C 端 Answerer 路径可用为主：remote offer 字符串进入核心库，生成 local answer 字符串，并完成 set remote/local description 的基础调用链验证。
- **D-14:** Offerer API 在 Phase 3 需要存在并可编译；如果协议依赖尚未就绪，可明确返回 `MRTC_STATUS_NOT_IMPLEMENTED` 或等价状态。
- **D-15:** Phase 3 不要求完整 Chrome E2E、真实 ICE connected、DTLS/SRTP 或媒体流动；这些属于后续 Phase 4-6。

### SDP 模块验证深度

- **D-16:** Phase 3 SDP 测试应覆盖基础 parse/serialize round-trip，以及 Answerer SDP 生成路径。
- **D-17:** 测试目标是证明 SDP 模块已脱离 AWS/KVS signaling client，并能支持 remote offer -> local answer 的基础 API 流程。
- **D-18:** Phase 3 不要求完整 Chrome SDP 兼容矩阵或浏览器自动化互通；可使用代表性 SDP 样本，但不得把 Phase 6 E2E 验收提前扩大到本阶段。

### the agent's Discretion

Planner 可以决定具体 public header 拆分方式、函数名细节、opaque handle typedef 写法、callback table 字段命名、config 结构的最小字段集合、SDP 字符串内存所有权规则和测试 fixture 组织方式。但不得改变以下约束：薄裁剪 AWS 使用模型、不暴露 AWS/PIC 基础类型、核心库 signaling-free、Answerer 先行、Offerer 接口预留、Phase 3 不正式暴露 transceiver/media/DataChannel 可调用 API。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目规划

- `.planning/PROJECT.md` — 项目定位、v1 范围、signaling/media 边界、API 形态和线程/回调模型约束。
- `.planning/REQUIREMENTS.md` — Phase 3 对应 `API-01`、`API-02`、`API-03`、`API-06`、`PROTO-06`、`PROTO-07`、`NET-05`。
- `.planning/ROADMAP.md` — Phase 3 目标和成功标准。
- `.planning/STATE.md` — 当前阶段、项目状态和需要继承的决策。

### 前置阶段决策

- `.planning/phases/01-/01-CONTEXT.md` — Phase 1 来源基线、核心/非核心边界、Signaling excluded 决策。
- `.planning/phases/01-/BASELINE.md` — 本地 KVS WebRTC SDK 基线、公开头文件位置、PeerConnection/SDP/Signaling 模块分类和 CMake/link 观察。
- `.planning/phases/01-/SOURCE-MANIFEST.md` — 后续派生文件必须遵守的来源追溯规则；Phase 3 若复制、裁剪、改名或 rewritten-derived AWS 头/源码/测试，必须更新 manifest。
- `.planning/phases/01-/COMPLIANCE.md` — Apache-2.0、NOTICE、第三方依赖和 excluded 组件策略。
- `.planning/phases/02-/02-CONTEXT.md` — Phase 2 建立的 `micrortc` target、`MRTC_STATUS`、`mrtc_*` 最小 API、public include 和 install/export 边界。
- `.planning/phases/02-/02-01-SUMMARY.md` — Phase 2 实际完成的构建骨架、测试和验证信息。

### 当前代码与本地参考库

- `include/micrortc/micrortc.h` — Phase 2 当前最小 public header，包含 `MRTC_STATUS`、`mrtc_version_string()`、`mrtc_initialize()`、`mrtc_shutdown()`。
- `src/micrortc.c` — Phase 2 当前最小实现源文件，Phase 3 需要在不破坏 smoke/package-consumer 的前提下扩展。
- `CMakeLists.txt` — 当前 `micrortc` 静态库 target、public include、install/export 和 smoke test 入口。
- `README.md` — Phase 2 API 边界、构建命令、excluded 依赖和溯源说明。
- `reflib/kvs-webrtc-sdk` — 唯一本地剥离基线；当前工作区缺失，Phase 3 planning/implementation 前必须恢复。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` — Phase 3 AWS 风格 API 裁剪的主要参考。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/NullableDefs.h` — nullable/type 语义参考；不得直接把 AWS/PIC 基础类型暴露到新 public API。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Stats.h` — 统计结构参考；Phase 3 默认不纳入公共 API，除非 planner 证明 API-01 必须有最小占位。
- `reflib/kvs-webrtc-sdk/src/source/Sdp/` — SDP parse/serialize 和 answer/offer 相关实现参考。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/` — PeerConnection 生命周期、SessionDescription、ICE candidate 回调和调用顺序参考。
- `reflib/kvs-webrtc-sdk/src/source/Signaling/` — excluded 参考；不得成为核心库依赖。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `include/micrortc/micrortc.h`：已建立 `MRTC_STATUS` 和 `mrtc_*` public API 风格，Phase 3 应延续该命名和基础类型边界。
- `CMakeLists.txt`：已建立 `micrortc` 静态库 target 和 `micrortc::micrortc` install/export 消费路径，Phase 3 新增 API/SDP 源码应接入该 target。
- `tests/smoke/test_link.c` 和 `tests/package-consumer/package_consumer.c`：可扩展或新增测试来证明 public header、链接符号和安装后消费仍可用。
- `.planning/phases/01-/SOURCE-MANIFEST.md`：可复用其表格规则记录 Phase 3 派生自 AWS 的 public API、SDP、PeerConnection 文件。

### Established Patterns

- Phase 2 已选择清爽 `include/micrortc/`、`src/`、`tests/` 布局；Phase 3 不应引入 `com/amazonaws/...` 作为新的 public include 路径。
- Phase 1 已将 `src/source/Signaling`、`kvsWebrtcSignalingClient`、AWS credential/storage/KVS service paths 标为 `excluded`。
- Phase 1/2 均强调本地 `reflib/kvs-webrtc-sdk` 是默认剥离基线，GitHub upstream latest 不作为默认参考。

### Integration Points

- Public API 入口应继续落在 `include/micrortc/` 下，可由 planner 决定是扩展 `micrortc.h` 还是拆出 `peer_connection.h`/`sdp.h` 并由 umbrella header include。
- PeerConnection、SDP 和 ICE candidate 实现应接入 `micrortc` target，不得引入 `kvsWebrtcSignalingClient`、libwebsockets 或 AWS SDK 依赖。
- 测试应覆盖 API 编译、链接、SDP parse/serialize、remote offer -> local answer 和 Offerer 占位返回状态；不应把完整浏览器 E2E 作为本阶段完成条件。

</code_context>

<specifics>
## Specific Ideas

- 推荐 API 形态接近：`MRTC_PEER_CONNECTION_HANDLE`、`MRTC_PEER_CONNECTION_CONFIG`、`MRTC_PEER_CONNECTION_CALLBACKS`、`mrtc_peer_connection_create()`、`mrtc_peer_connection_free()`。
- 推荐 SDP API 语义包括：`mrtc_peer_connection_set_remote_description()`、`mrtc_peer_connection_create_answer()`、`mrtc_peer_connection_set_local_description()`、`mrtc_peer_connection_create_offer()`。
- 推荐 ICE candidate API 语义包括：`on_ice_candidate` callback 和 `mrtc_peer_connection_add_ice_candidate()`。
- 推荐状态码至少包括：`MRTC_STATUS_NOT_IMPLEMENTED`、`MRTC_STATUS_INVALID_STATE`、`MRTC_STATUS_PARSE_ERROR`。
- 字符串内存所有权规则需要 planner 明确，例如由库分配并提供 free 函数，或由调用者提供 buffer；用户未指定，留给 planner 在 API 一致性和 C 易用性之间取舍。

</specifics>

<deferred>
## Deferred Ideas

- transceiver、media、`writeFrame` 类 API 留给 Phase 5。
- DataChannel 创建、打开、关闭和消息收发 API 留给 Phase 4。
- ICE/STUN/TURN 真实连接、DTLS/SRTP、安全通道和 TURN relay 验证留给 Phase 4/6。
- Chrome 自动化 E2E、DataChannel E2E、H264/Opus 双向媒体流动验收留给 Phase 6。
- 长期 libmicrortc 风格 API 清理和 AWS 兼容层是否存在，留给 v2 或后续 API 演进阶段。

</deferred>

---

*Phase: 3-AWS 风格 API 与 signaling-free PeerConnection*
*Context gathered: 2026-05-12T21:54:08+08:00*
