# Phase 2: 独立构建与库骨架 - Context

**Gathered:** 2026-05-12T19:34:59+08:00
**Status:** Ready for planning

<domain>
## Phase Boundary

本阶段建立 `libmicrortc` 的独立 CMake 静态库骨架，使项目根目录可以独立配置、构建、安装并被下游 CMake 工程消费。交付重点是构建边界、目录布局、最小公共头、最小可链接源码、smoke test、install/export 和开发者构建说明。

本阶段不搬迁 ICE、STUN、TURN、DTLS、SRTP、RTP/RTCP、SCTP/DataChannel、SDP 或 PeerConnection 等协议源码，不设计 Phase 3 的完整 PeerConnection API，不引入 AWS/KVS signaling、credential/storage、GStreamer sample，也不临时链接 `kvsCommonLws` / PIC 公共库能力。

</domain>

<decisions>
## Implementation Decisions

### 骨架目录与 Target 命名

- **D-01:** Phase 2 采用清爽 `libmicrortc` 布局：`include/micrortc/`、`src/`、`cmake/`、`tests/`。
- **D-02:** 核心 CMake target 命名为 `micrortc`，静态库产物为 `libmicrortc.a`。
- **D-03:** 安装和导出的 CMake namespace 使用 `micrortc::`，下游消费方式以 `target_link_libraries(app PRIVATE micrortc::micrortc)` 为准。
- **D-04:** Phase 2 不提供 AWS/KVS 命名 alias，也不把 `kvsWebrtcClient` 作为过渡 target 名称，避免旧命名扩散到新工程边界。
- **D-05:** Phase 2 的公共 API 只放最小头文件 `include/micrortc/micrortc.h`，用于证明 public include、install/export 和消费端编译链路可用。
- **D-06:** `PeerConnection`、SDP、ICE candidate、transceiver、DataChannel 等 AWS 风格 API 裁剪属于 Phase 3+，不得在 Phase 2 提前承诺公共头结构。

### 首版静态库内容边界

- **D-07:** `libmicrortc.a` 在 Phase 2 采用最小可链接壳，只新增自有极小源码，例如版本、初始化或能力查询类函数，让 smoke test 能证明真实链接符号存在。
- **D-08:** 最小壳 API 使用纯 C 和项目自有状态码风格，例如 `MRTC_STATUS` 与 `mrtc_*` 函数；不使用 AWS 的 `STATUS`、`PCHAR` 等公共类型命名。
- **D-09:** Phase 2 不搬迁 AWS 协议源码，也不迁入 allocator、logging、state、utils 等 PIC/kvsCommonLws 基础能力。
- **D-10:** PIC/kvsCommonLws 基础能力在 Phase 2 明确留空并记录为后续协议搬迁风险；本阶段不预留 `src/internal/allocator.h`、`log.h`、`state.h` 等内部接口头。
- **D-11:** 后续 planner 应把 AWS 协议源码对 allocator/logging/state/utils 等能力的依赖作为 Phase 3/4/5 搬迁前置风险处理，而不是在本阶段提前定型。

### 第三方依赖入口与排除边界

- **D-12:** Phase 2 不强制 `find_package`、pkg-config 查找或 ExternalProject 构建 OpenSSL、libsrtp、usrsctp，因为最小壳 target 不需要这些库。
- **D-13:** OpenSSL、libsrtp、usrsctp 仍记录为 v1 `runtime/core` 依赖边界；真正的发现、链接、系统包或 vendored/ExternalProject 策略留到协议模块实际引入时决定。
- **D-14:** `micrortc` target 的 link list 必须保持极小，不链接 `libwebsockets`、AWS SDK、`kvsWebrtcSignalingClient`、AWS credential/storage、GStreamer sample、`kvsCommonLws`、`kvspicUtils`、`kvspicState` 或其他 PIC 公共库 target。
- **D-15:** Phase 2 需要在 CMake 和开发者文档中明确 excluded/reference-only 依赖不得进入核心 target；先不写复杂强校验脚本。
- **D-16:** 第三方依赖边界既要写入本 CONTEXT，也要落到开发者可见构建说明中。Phase 2 可新增或更新根目录 `README.md` 或 `docs/build.md`，说明当前最小骨架不强制 OpenSSL/libsrtp/usrsctp，并说明 excluded 依赖边界。

### 构建验收与溯源更新

- **D-17:** Phase 2 验收必须覆盖三件事：CMake 可生成 `libmicrortc.a`；一个小测试程序可 include `micrortc/micrortc.h` 并链接 `micrortc::micrortc`；安装后的 CMake package/export 可被下游消费。
- **D-18:** 验收不能只检查 `.a` 文件存在；必须证明 public header、真实链接符号、install/export 路径都可用。
- **D-19:** Phase 2 可添加 smoke test 或等价消费端验证目标，但不需要建立完整 CI 脚本骨架。
- **D-20:** `.planning/phases/01-/SOURCE-MANIFEST.md` 继续只记录 AWS 派生、reference-only 或 excluded 条目，不把 Phase 2 自有骨架文件混入派生 manifest。
- **D-21:** Phase 2 新增的自有文件，例如根目录 `CMakeLists.txt`、`include/micrortc/micrortc.h`、最小源码、smoke test、README/build 文档，应在 Phase 2 summary 或相关文档中标为 `original`。
- **D-22:** 如果 Phase 2 实际引用或复制 `reflib/kvs-webrtc-sdk` 的 CMake 片段、源码、头文件或测试逻辑，不再视为纯自有文件，必须按 Phase 1 manifest 规则追加来源追溯记录。

### the agent's Discretion

Planner 可以决定最小 API 的具体函数名、状态码枚举值、CMake 文件拆分方式、测试目录细节、install/export 文件名和构建文档落点，但不得改变以下约束：target/export 命名、清爽目录布局、最小壳范围、不搬协议源码、不链接 AWS/KVS/PIC 依赖、验收必须覆盖 install/export 消费。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目规划

- `.planning/PROJECT.md` — 项目定位、v1 范围、构建系统、依赖策略和核心库边界。
- `.planning/REQUIREMENTS.md` — Phase 2 对应 `BUILD-01`、`BUILD-02`、`BUILD-03`、`BUILD-04`。
- `.planning/ROADMAP.md` — Phase 2 目标和成功标准。
- `.planning/STATE.md` — 当前阶段、工作流状态和需要继承的项目级决策。

### Phase 1 已锁边界

- `.planning/phases/01-/01-CONTEXT.md` — Phase 1 用户决策，尤其是来源追溯、核心/非核心分类和本地基线约束。
- `.planning/phases/01-/BASELINE.md` — 本地 `reflib/kvs-webrtc-sdk` 基线、关键 CMake 观察、模块边界和依赖入口快照。
- `.planning/phases/01-/SOURCE-MANIFEST.md` — 派生文件来源追溯规则；Phase 2 必须遵守。
- `.planning/phases/01-/COMPLIANCE.md` — Apache-2.0、NOTICE、第三方依赖和 excluded 组件策略。

### 本地参考库

- `reflib/kvs-webrtc-sdk` — 唯一剥离基线；不要默认使用 GitHub upstream latest。
- `reflib/kvs-webrtc-sdk/CMakeLists.txt` — 参考库当前顶层构建选项、dependency 构建流程、`kvsWebrtcClient` 和 `kvsWebrtcSignalingClient` target 关系。
- `reflib/kvs-webrtc-sdk/src/CMakeLists.txt` — 参考库子目录构建方式和 PIC 公共库耦合参考。
- `reflib/kvs-webrtc-sdk/CMake/Dependencies` — OpenSSL、libsrtp、usrsctp、libwebsockets、AWS SDK、kvsCommonLws/PIC 等依赖声明来源。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` — Phase 3 API 裁剪参考；Phase 2 不应直接承诺其中的 PeerConnection API。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/NullableDefs.h` — Phase 3 类型语义参考；Phase 2 不沿用 AWS 公共类型命名。
- `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Stats.h` — Phase 3+ 统计结构参考；Phase 2 不纳入最小公共 API。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `reflib/kvs-webrtc-sdk/CMakeLists.txt`：可作为构建边界反例和参考，尤其是当前 `kvsWebrtcClient` 链接 `kvspicUtils`、`kvspicState`、OpenSSL、libsrtp、usrsctp 等依赖，而 `kvsWebrtcSignalingClient` 链接 `kvsCommonLws` 和 libwebsockets。
- `reflib/kvs-webrtc-sdk/CMake/Dependencies`：可作为后续依赖版本和来源的事实依据，但 Phase 2 不复用 ExternalProject 下载/构建流程。
- `.planning/phases/01-/SOURCE-MANIFEST.md`：可复用其表格规则和审阅清单来判断 Phase 2 是否意外引入派生内容。

### Established Patterns

- Phase 1 已将 `src/source/Signaling`、AWS SDK C++、credential/storage/KVS 服务路径标为 `excluded`，它们不得成为核心库依赖。
- Phase 1 已将 `kvsCommonLws` / PIC 公共能力标为 `supporting/reference-only`，Phase 2 不应把它们链接进 `micrortc`。
- 参考库 `src/source` 下的协议模块后续将需要基础设施适配，但 Phase 2 的目标是先建立可验证的独立构建壳，不提前搬迁协议实现。

### Integration Points

- 项目根目录当前没有正式 `CMakeLists.txt`、`include/`、`src/`、`tests/` 或根目录构建文档；Phase 2 planner 应从空骨架开始设计。
- Phase 2 的核心集成点是根目录 CMake 配置、`micrortc` 静态库 target、install/export package、最小 public header、最小 source 文件和 smoke test。
- 开发者文档应放在根目录 `README.md` 或 `docs/build.md`，用于说明构建命令、当前依赖边界和 excluded 组件。

</code_context>

<specifics>
## Specific Ideas

- 最小 public header 建议命名为 `include/micrortc/micrortc.h`。
- 最小 C API 应使用 `MRTC_STATUS` 和 `mrtc_*` 命名风格，并提供至少一个真实可链接函数。
- smoke test 应证明下游以 `micrortc::micrortc` 方式链接，而不是直接依赖 build-tree 内部 target 名称。
- 构建文档需要明确：当前 Phase 2 骨架不强制 OpenSSL/libsrtp/usrsctp；这些依赖会在对应协议模块进入时再激活。

</specifics>

<deferred>
## Deferred Ideas

- PIC/kvsCommonLws 中 allocator、logging、state、utils 等基础能力的替换、裁剪或临时迁移策略留给后续协议模块搬迁阶段。
- PeerConnection、SDP、ICE candidate、DataChannel 和媒体 API 公共头设计留给 Phase 3+。
- OpenSSL/libsrtp/usrsctp 的系统包、pkg-config、vendored 或 ExternalProject 具体策略留给协议模块真正引入时决定。

</deferred>

---

*Phase: 2-独立构建与库骨架*
*Context gathered: 2026-05-12T19:34:59+08:00*
