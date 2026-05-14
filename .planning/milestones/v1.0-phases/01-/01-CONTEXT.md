# Phase 1: 基线、范围与合规边界 - Context

**Gathered:** 2026-05-12T18:49:01+08:00
**Status:** Ready for planning（可进入规划）

<domain>
## Phase Boundary

本阶段只锁定本地 `reflib/kvs-webrtc-sdk` 作为剥离基线，并建立后续代码搬迁必须遵守的来源追溯、模块边界和合规文档规则。

本阶段不搬迁协议栈代码，不设计最终 API，不改造构建系统，也不引入新的 WebRTC 能力。下游规划应产出基线、来源清单和合规策略文档，为 Phase 2 之后的实际剥离提供约束。

</domain>

<decisions>
## Implementation Decisions

### 来源清单粒度

- **D-01:** 来源追溯采用文件级清单。后续每个从 `reflib/kvs-webrtc-sdk` 复制、裁剪、改名或重写派生进入 `libmicrortc` 的文件，都必须能追到原始路径。
- **D-02:** 文件级清单只覆盖实际派生进入 `libmicrortc` 的文件，不预先列出 `reflib` 全量源码。
- **D-03:** 每条 manifest 记录的最低字段为 `source_path`、`target_path`、`origin_commit`、`derivation`、`notes`。
- **D-04:** manifest 使用 Markdown 表格，优先保证人工审阅和 PR review 友好。许可证字段和 hash 不作为每条记录必填项。

### 核心/非核心边界

- **D-05:** 模块边界采用三类：`core`、`excluded`、`supporting/reference-only`。
- **D-06:** 初始 `core` 候选只包含协议栈硬核心：`Ice`、`Stun`、`Crypto`/DTLS、`Srtp`、`Sctp`、`Sdp`、`Rtp`、`Rtcp`、`PeerConnection`。
- **D-07:** `src/source/Signaling` 明确归为 `excluded`。它是 AWS/KVS signaling client，不能成为核心库依赖，只能必要时作为参考材料。
- **D-08:** `Threadpool`、`Metrics`、`samples/common/StaticMedia.c`、固定 H264/Opus 样本归为 `supporting/reference-only`。
- **D-09:** `kvsCommonLws` / PIC 公共库依赖整体归为 `supporting/reference-only`。可参考或临时迁移 allocator、logging、state、utils 等能力，但它们不是 libmicrortc 长期核心边界。
- **D-10:** 参考库 `tst/` 下协议相关测试归为 `supporting/reference-only`。`Signaling*Test` 仅作为 excluded 模块参考，不进入核心验收。

### 合规文档形态

- **D-11:** Phase 1 应产出三份文档：`BASELINE.md`、`SOURCE-MANIFEST.md`、`COMPLIANCE.md`。
- **D-12:** 三份文档先放在 Phase 1 阶段目录。根目录只在需要时建立引用或后续整理，避免规划阶段提前扩张根目录文档结构。
- **D-13:** 合规策略采用保留 Apache-2.0 兼容路径，并建立独立 NOTICE 衍生说明。根/发布包必须包含 LICENSE；NOTICE 应保留 AWS 原 NOTICE，并添加 libmicrortc 对 AWS KVS WebRTC SDK 的派生说明。
- **D-14:** 第三方依赖按依赖名、来源、用途、v1 状态记录，状态至少包括 `runtime/core`、`reference-only`、`test-only`、`excluded`。

### 基线快照策略

- **D-15:** baseline 采用可审计快照，记录 `reflib` 路径、tag/version、commit、dirty 状态、关键目录结构、核心候选模块、排除模块、关键构建选项和依赖清单；不要求全量文件 hash。
- **D-16:** 依赖清单记录 CMake 声明来源和已知 tag/URL。读取 `CMake/Dependencies/*` 中能直接看到的 `GIT_REPOSITORY`、`GIT_TAG`、用途；无法确认的项标注“需 Phase 2 构建确认”。
- **D-17:** `reflib` 工作区干净是后续来源追溯和搬迁的硬性前置条件。若 `reflib` dirty，必须先记录补丁或停止搬迁。
- **D-18:** `BASELINE.md` 和本 `CONTEXT.md` 都必须明确约束：研究、搬迁和差异讨论以本地 `reflib/kvs-webrtc-sdk` 为准，不默认跟随 GitHub 上游最新。

### 执行裁量范围

未授权下游 agent 自行改变以上边界。planner 可以决定三份文档的章节组织、表格列顺序和生成命令，但不得降低追溯粒度、弱化 `Signaling` 排除边界，或把基线切换到远端上游。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目规划

- `.planning/PROJECT.md` — 项目定位、v1 范围、既定约束和关键决策。
- `.planning/REQUIREMENTS.md` — Phase 1 对应 `BASE-01`、`BASE-02`、`BASE-03`、`BASE-04`。
- `.planning/ROADMAP.md` — Phase 1 目标和成功标准。
- `.planning/STATE.md` — 当前阶段、项目状态和需要继承的决策。

### 本地剥离基线

- `reflib/kvs-webrtc-sdk` — 唯一剥离基线；当前检查到 tag/version 为 `v1.18.1`，commit 为 `9eebcc4`，工作区干净。
- `reflib/kvs-webrtc-sdk/LICENSE` — Apache-2.0 许可证文本。
- `reflib/kvs-webrtc-sdk/NOTICE` — AWS KVS WebRTC SDK 原 NOTICE。
- `reflib/kvs-webrtc-sdk/CMakeLists.txt` — 顶层构建选项、依赖声明入口、关键 feature flags。
- `reflib/kvs-webrtc-sdk/src/CMakeLists.txt` — 参考库当前源码 target 和链接关系。
- `reflib/kvs-webrtc-sdk/CMake/Dependencies` — 第三方依赖 ExternalProject 声明来源。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `reflib/kvs-webrtc-sdk/src/source/Ice`、`Stun`、`Crypto`、`Srtp`、`Sctp`、`Sdp`、`Rtp`、`Rtcp`、`PeerConnection`：初始核心协议栈候选模块。
- `reflib/kvs-webrtc-sdk/tst/*`：协议单元测试迁移参考，特别是 ICE/STUN/DTLS/SRTP/RTP/RTCP/SDP/DataChannel/PeerConnection 行为样例。
- `reflib/kvs-webrtc-sdk/samples/common/StaticMedia.c` 和 `samples/h264SampleFrames`、`samples/opusSampleFrames`：后续 demo/E2E 媒体输入参考，不属于核心库。

### Established Patterns

- 参考库顶层 CMake 当前默认 `USE_OPENSSL=ON`、`BUILD_DEPENDENCIES=ON`、`BUILD_SAMPLE=ON`、`ENABLE_DATA_CHANNEL=ON`，但 Phase 1 只记录这些作为基线，不在本阶段改造构建。
- 参考库 `src/source/Signaling` 与 AWS/KVS 服务耦合，必须被标注为 `excluded`，不能作为 libmicrortc 核心依赖。
- 当前参考库 `src/CMakeLists.txt` 链接 `client heap trace view mkvgen utils state jsmn crypto ssl` 等 PIC/公共库能力，这些整体视为 `supporting/reference-only`。

### Integration Points

- Phase 1 planner 应在 `.planning/phases/01-/` 设计并生成 `BASELINE.md`、`SOURCE-MANIFEST.md`、`COMPLIANCE.md`。
- `SOURCE-MANIFEST.md` 应定义 Markdown 表格格式，后续 Phase 2+ 逐步追加实际派生文件条目。
- `COMPLIANCE.md` 应列出第三方依赖的来源、用途和 v1 状态。已在 CMake 依赖声明中看到：OpenSSL `OpenSSL_1_1_1t`、libsrtp `bd0f27ec0e299ad101a396dde3f7c90d48efc8fc`、usrsctp `1ade45cbadfd19298d2c47dc538962d4425ad2dd`、libwebsockets `v4.3.5`、jsmn `v1.0.0`、kvsCommonLws/PIC `v1.6.1`、gtest `release-1.12.1`、benchmark `v1.5.1`。

</code_context>

<specifics>
## Specific Ideas

- baseline 文档必须重复强调：不要默认使用 GitHub 上游最新版本；任何研究、搬迁或差异讨论都以本地 `reflib/kvs-webrtc-sdk` 为准。
- 若后续需要机器校验，可从 Markdown manifest 派生工具，但 Phase 1 不要求双格式。
- LICENSE/NOTICE 策略应为后续根目录或发布包落地留出口，但 Phase 1 的正式规划产物先保留在阶段目录。

</specifics>

<deferred>
## Deferred Ideas

无。本次讨论保持在 Phase 1 范围内。

</deferred>

---

*Phase: 1-基线、范围与合规边界*
*Context gathered: 2026-05-12T18:49:01+08:00*
