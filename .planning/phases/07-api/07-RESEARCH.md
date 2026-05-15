# Phase 07: API 盘点与命名契约冻结 - Research

**Researched:** 2026-05-15  
**Domain:** C public API contract, naming migration, residual scanning  
**Confidence:** HIGH

## User Constraints

- Markdown 文档尽可能使用中文编写。[VERIFIED: AGENTS.md]
- 本项目是 `libmicrortc`，目标是从 AWS KVS WebRTC C SDK 中剥离通用 C WebRTC 协议栈。[VERIFIED: AGENTS.md; .planning/PROJECT.md]
- 需求、路线图和当前状态以 `.planning/PROJECT.md`、`.planning/REQUIREMENTS.md`、`.planning/ROADMAP.md`、`.planning/STATE.md` 为准。[VERIFIED: AGENTS.md]
- AWS KVS WebRTC SDK 的剥离基线必须以本地 `reflib/kvs-webrtc-sdk` 为准，不默认使用 GitHub 上游最新版本。[VERIFIED: AGENTS.md; .planning/PROJECT.md]
- v1 优先 Linux x86_64、Chrome、H264、Opus、TURN relay、双向音视频和自动化 E2E 验收。[VERIFIED: AGENTS.md; .planning/PROJECT.md]
- 核心库不内置应用层 signaling，不做媒体采集和编码，只处理编码后媒体帧。[VERIFIED: AGENTS.md; .planning/PROJECT.md]
- Phase 7 必须覆盖 API-01、API-02、API-04、STYLE-04。[VERIFIED: user prompt; .planning/ROADMAP.md]
- Phase 7 不应实现旧 `mrtc_*`、`MRTC_*`、AWS/KVS 风格 public API 的兼容 wrapper 层。[VERIFIED: user prompt; .planning/REQUIREMENTS.md; .planning/STATE.md]

## Project Constraints (from AGENTS.md)

- 所有 Markdown 文档尽可能中文。[VERIFIED: AGENTS.md]
- 本地 `reflib/kvs-webrtc-sdk` 是 AWS KVS WebRTC SDK 剥离基线；研究和计划不能默认转向远端上游。[VERIFIED: AGENTS.md]
- 核心库边界必须保持 signaling-free、无媒体采集、无编码器，仅处理编码后媒体帧。[VERIFIED: AGENTS.md]
- v1 优先平台和验收组合是 Linux x86_64、Chrome、H264、Opus、TURN relay、双向音视频和自动化 E2E。[VERIFIED: AGENTS.md]

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| API-01 | 开发者可以在文档中看到冻结的 v1.1 public、private 和 test-only API 边界。 | 本研究将 installed headers、`src/` private headers、CTest/private-include tests 和 E2E/demo 边界分开列出。[VERIFIED: include/micrortc/*.h; cmake/MicroRtcTests.cmake; rg include scan] |
| API-02 | 开发者可以查到完整旧名到新名映射，覆盖 `include/micrortc/*.h` 的 public symbols。 | 本研究给出 `include/micrortc/*.h` 的 public symbol mapping 表，建议 Phase 7 将该表冻结到开发者文档。[VERIFIED: include/micrortc/micrortc.h; include/micrortc/peer_connection.h] |
| API-04 | 公共 API 默认不保留旧 `mrtc_*`、`MRTC_*`、AWS/KVS 风格 typedef、handle 或 wrapper 兼容层。 | 本研究将删除策略列为 Phase 7 的契约，后续 Phase 8/9 直接迁移调用点而不是增加旧名 wrapper。[VERIFIED: .planning/REQUIREMENTS.md; .planning/STATE.md] |
| STYLE-04 | 旧 AWS/KVS、`mrtc_*` 和 `MRTC_*` public API 残留扫描可以区分禁止残留、来源合规记录和迁移文档白名单。 | 本研究定义残留扫描分类：禁止残留、合规/溯源白名单、迁移文档白名单、测试 harness 非 public C API 记录、构建产物忽略。[VERIFIED: rg residual scan; .planning/milestones/v1.0-phases/01-/COMPLIANCE.md; .planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md] |

</phase_requirements>

## Summary

Phase 7 应是“冻结契约”阶段，而不是机械改名阶段。[VERIFIED: .planning/ROADMAP.md] 计划应产出可被 Phase 8-11 消费的文档和扫描规则：public/private/test-only 边界、`include/micrortc/*.h` 旧名到新名映射、旧名删除策略、残留扫描分类和验证入口。[VERIFIED: .planning/REQUIREMENTS.md; .planning/ROADMAP.md]

当前 installed public headers 只有 `include/micrortc/micrortc.h` 和 `include/micrortc/peer_connection.h`，且 public C API 仍使用 `mrtc_*` 函数、`MRTC_*` 类型/枚举/宏。[VERIFIED: include/micrortc/*.h] Private implementation headers 位于 `src/`，大量测试通过 `WITH_PRIVATE_INCLUDES` 或相对路径包含 private headers；这些应作为 private/test-only API 明确记录，不能在 Phase 7 误升为 public API。[VERIFIED: cmake/MicroRtcTests.cmake; rg "#include" scan]

**Primary recommendation:** Phase 7 建立 `docs/api-v1.1-boundary.md`、`docs/api-v1.1-symbol-map.md` 和 `scripts/scan-api-residuals.sh`，并让扫描输出机器可读报告，禁止在 public headers 和公开示例中残留旧 public API 名称，同时对白名单文档和合规溯源记录分类放行。[VERIFIED: .planning/ROADMAP.md; rg residual scan; scripts/verify-v1.sh pattern]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|--------------|----------------|-----------|
| Public API boundary freeze | Documentation / API contract | Public C headers | Public surface 的事实来源是 installed `include/micrortc/*.h`，但 Phase 7 的交付物应是冻结文档。[VERIFIED: include/micrortc/*.h; .planning/ROADMAP.md] |
| Old-to-new symbol mapping | Documentation / API contract | Public C headers | Phase 8 才改 header；Phase 7 应先冻结映射表，避免边改边重新命名。[VERIFIED: .planning/ROADMAP.md] |
| Private API boundary | Private C implementation | Tests | `src/*.h` 和 `src/**/*.h` 暴露 SDP/ICE/DTLS/SRTP/SCTP/RTP/RTCP/media helpers，当前测试直接包含这些 header。[VERIFIED: rg "#include" scan; src/**/*.h scan] |
| Test-only API boundary | Test harness / examples | Private C implementation | `tests/*` 和 `examples/chrome-e2e/mrtc_chrome_answerer.c` 使用 private media/send hook 与 internal headers 验证行为。[VERIFIED: cmake/MicroRtcTests.cmake; examples/chrome-e2e/mrtc_chrome_answerer.c] |
| Residual scanning | Scripts / CI-style verification | Documentation whitelist | STYLE-04 要求扫描能区分 forbidden residual、source compliance record、migration docs whitelist。[VERIFIED: .planning/REQUIREMENTS.md] |
| v1 regression evidence | Verification script | CTest + Node/Playwright E2E | `scripts/verify-v1.sh` 串起 build、CTest、Chrome host E2E，并生成 `build/reports/mrtc-v1-summary.json`。[VERIFIED: scripts/verify-v1.sh; build/reports/mrtc-v1-summary.json] |

## Standard Stack

### Core

| Library / Tool | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| C public headers | n/a | Public API source of truth | Installed headers are copied by `install(DIRECTORY include/)`, so Phase 7 inventory must start from `include/micrortc/*.h`。[VERIFIED: cmake/MicroRtcInstall.cmake; include/micrortc/*.h] |
| Markdown docs | n/a | Freeze API contract and migration map | User and AGENTS.md require Markdown docs to be Chinese when possible, and Phase 7 success criteria are developer-readable documentation.[VERIFIED: AGENTS.md; .planning/ROADMAP.md] |
| Bash | 5.1.16 | Verification and residual scan shell entrypoints | Existing `scripts/verify-v1.sh` is Bash and uses staged summaries, so a Phase 7 scan script should follow this repo pattern.[VERIFIED: bash --version; scripts/verify-v1.sh] |
| ripgrep (`rg`) | 15.1.0 | Fast residual scanning | `rg` is available and suitable for classifying text residuals across include/src/tests/examples/docs while excluding build/reflib paths.[VERIFIED: rg --version; rg residual scan] |
| CMake / CTest | 3.22.1 | Existing build/test discovery | CMake config registers library, install package, examples and 18 CTest tests.[VERIFIED: cmake --version; ctest -N; CMakeLists.txt] |

### Supporting

| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| Node.js / npm | Node 24.14.0 / npm 11.9.0 | Existing E2E and summary tooling | Use only if Phase 7 extends JSON summary integration; do not make API scanning depend on Node unless needed.[VERIFIED: node --version; npm --version; tests/e2e/package.json] |
| `@playwright/test` | 1.60.0 in lockfile/package | Existing Chrome E2E validation | Phase 7 should reference, not modify, Playwright E2E unless validation docs need to mention v1 regression commands.[VERIFIED: tests/e2e/package.json] |
| `clang-tidy` | missing in PATH | Future STYLE-01/STYLE-02 enforcement | Phase 7 can document `.clang-tidy` rules, but cannot assume `clang-tidy` is locally available unless the plan installs or probes it.[VERIFIED: command -v clang-tidy; .clang-tidy] |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `rg` residual scan | libclang/ctags parser | A parser would classify C symbols more precisely, but Phase 7 only needs a contract scan over known names and whitelist categories; parser setup adds dependency risk.[VERIFIED: include/micrortc/*.h; environment audit] |
| Markdown mapping table | Generated Doxygen/API ref | FUT-02 defers Doxygen/API reference until public header stabilizes further, so Phase 7 should not block on generated API docs.[VERIFIED: .planning/REQUIREMENTS.md] |
| Compatibility wrapper headers | `#define mrtc_* rtc_*` or inline wrappers | API-04 explicitly rejects old public API wrapper compatibility by default.[VERIFIED: .planning/REQUIREMENTS.md; .planning/STATE.md] |

**Installation:**

```bash
# Phase 7 不需要新增依赖；复用仓库已有 Bash、rg、CMake/CTest。
```

**Version verification:** No npm package is recommended for Phase 7 implementation; existing Node dependencies were inspected from `tests/e2e/package.json` rather than newly selected.[VERIFIED: tests/e2e/package.json]

## Public Symbol Mapping To Freeze

Planner should create a developer-facing mapping doc that covers these symbols exactly before Phase 8 edits headers.[VERIFIED: include/micrortc/micrortc.h; include/micrortc/peer_connection.h]

### Core / Status

| Old public symbol | New v1.1 symbol | Category | Action |
|-------------------|-----------------|----------|--------|
| `MRTC_STATUS` | `RtcStatus` | enum type | Rename; no old typedef wrapper.[VERIFIED: include/micrortc/micrortc.h:10] |
| `MRTC_STATUS_OK` | `RTC_STATUS_OK` | enum constant | Rename; no old macro alias.[VERIFIED: include/micrortc/micrortc.h:11] |
| `MRTC_STATUS_INVALID_ARG` | `RTC_STATUS_INVALID_ARG` | enum constant | Rename; no old macro alias.[VERIFIED: include/micrortc/micrortc.h:12] |
| `MRTC_STATUS_NOT_IMPLEMENTED` | `RTC_STATUS_NOT_IMPLEMENTED` | enum constant | Rename; no old macro alias.[VERIFIED: include/micrortc/micrortc.h:13] |
| `MRTC_STATUS_INVALID_STATE` | `RTC_STATUS_INVALID_STATE` | enum constant | Rename; no old macro alias.[VERIFIED: include/micrortc/micrortc.h:14] |
| `MRTC_STATUS_PARSE_ERROR` | `RTC_STATUS_PARSE_ERROR` | enum constant | Rename; no old macro alias.[VERIFIED: include/micrortc/micrortc.h:15] |
| `mrtc_version_string` | `rtc_version_string` | function | Rename; no old wrapper.[VERIFIED: include/micrortc/micrortc.h:19] |
| `mrtc_initialize` | `rtc_initialize` | function | Rename; no old wrapper.[VERIFIED: include/micrortc/micrortc.h:20] |
| `mrtc_shutdown` | `rtc_shutdown` | function | Rename; no old wrapper.[VERIFIED: include/micrortc/micrortc.h:21] |

### Handles / Types / Structs

| Old public symbol | New v1.1 symbol | Category | Action |
|-------------------|-----------------|----------|--------|
| `MRTC_PEER_CONNECTION_HANDLE` | `RtcPeerConnection` | opaque handle typedef | Rename to `Rtc*`; avoid `_HANDLE` public suffix.[VERIFIED: include/micrortc/peer_connection.h:22; .planning/REQUIREMENTS.md] |
| `MRTC_DATA_CHANNEL_HANDLE` | `RtcDataChannel` | opaque handle typedef | Rename to `Rtc*`; avoid `_HANDLE` public suffix.[VERIFIED: include/micrortc/peer_connection.h:23; .planning/REQUIREMENTS.md] |
| `MRTC_RTP_TRANSCEIVER_HANDLE` | `RtcRtpTransceiver` | opaque handle typedef | Rename to `Rtc*`; avoid `_HANDLE` public suffix.[VERIFIED: include/micrortc/peer_connection.h:24; .planning/REQUIREMENTS.md] |
| `MRTC_ICE_SERVER` | `RtcIceServer` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:26] |
| `MRTC_PEER_CONNECTION_STATE` | `RtcPeerConnectionState` | enum type | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:32] |
| `MRTC_DATA_CHANNEL_MESSAGE_TYPE` | `RtcDataChannelMessageType` | enum type | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:41] |
| `MRTC_MEDIA_KIND` | `RtcMediaKind` | enum type | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:46] |
| `MRTC_CODEC` | `RtcCodec` | enum type | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:51] |
| `MRTC_RTP_TRANSCEIVER_DIRECTION` | `RtcRtpTransceiverDirection` | enum type | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:56] |
| `MRTC_FRAME_FLAG` | `RtcFrameFlag` | enum type | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:63] |
| `MRTC_FRAME` | `RtcFrame` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:68] |
| `MRTC_DATA_CHANNEL_INIT` | `RtcDataChannelInit` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:78] |
| `MRTC_DATA_CHANNEL_CALLBACKS` | `RtcDataChannelCallbacks` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:84] |
| `MRTC_TRANSCEIVER_CALLBACKS` | `RtcTransceiverCallbacks` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:94] |
| `MRTC_TRANSCEIVER_INIT` | `RtcTransceiverInit` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:101] |
| `MRTC_PEER_CONNECTION_CONFIG` | `RtcPeerConnectionConfig` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:108] |
| `MRTC_SELECTED_CANDIDATE_PAIR_INFO` | `RtcSelectedCandidatePairInfo` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:114] |
| `MRTC_PEER_CONNECTION_CALLBACKS` | `RtcPeerConnectionCallbacks` | struct typedef | Rename to `Rtc*`.[VERIFIED: include/micrortc/peer_connection.h:120] |

### Enum Constants

| Old public symbol pattern | New v1.1 symbol pattern | Action |
|---------------------------|-------------------------|--------|
| `MRTC_PEER_CONNECTION_STATE_*` | `RTC_PEER_CONNECTION_STATE_*` | Rename all six state constants.[VERIFIED: include/micrortc/peer_connection.h:33-38] |
| `MRTC_DATA_CHANNEL_MESSAGE_TYPE_*` | `RTC_DATA_CHANNEL_MESSAGE_TYPE_*` | Rename text/binary constants.[VERIFIED: include/micrortc/peer_connection.h:42-43] |
| `MRTC_MEDIA_KIND_*` | `RTC_MEDIA_KIND_*` | Rename audio/video constants.[VERIFIED: include/micrortc/peer_connection.h:47-48] |
| `MRTC_CODEC_*` | `RTC_CODEC_*` | Rename H264 and Opus constants.[VERIFIED: include/micrortc/peer_connection.h:52-53] |
| `MRTC_RTP_TRANSCEIVER_DIRECTION_*` | `RTC_RTP_TRANSCEIVER_DIRECTION_*` | Rename sendrecv/sendonly/recvonly/inactive constants.[VERIFIED: include/micrortc/peer_connection.h:57-60] |
| `MRTC_FRAME_FLAG_*` | `RTC_FRAME_FLAG_*` | Rename none/key-frame constants.[VERIFIED: include/micrortc/peer_connection.h:64-65] |

### Functions

| Old public symbol | New v1.1 symbol | Action |
|-------------------|-----------------|--------|
| `mrtc_peer_connection_create` | `rtc_peer_connection_create` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:126] |
| `mrtc_peer_connection_free` | `rtc_peer_connection_free` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:131] |
| `mrtc_peer_connection_set_remote_description` | `rtc_peer_connection_set_remote_description` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:133] |
| `mrtc_peer_connection_create_answer` | `rtc_peer_connection_create_answer` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:137] |
| `mrtc_peer_connection_set_local_description` | `rtc_peer_connection_set_local_description` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:142] |
| `mrtc_peer_connection_create_offer` | `rtc_peer_connection_create_offer` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:146] |
| `mrtc_peer_connection_add_ice_candidate` | `rtc_peer_connection_add_ice_candidate` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:151] |
| `mrtc_peer_connection_poll_transport` | `rtc_peer_connection_poll_transport` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:154] |
| `mrtc_peer_connection_get_selected_candidate_pair_info` | `rtc_peer_connection_get_selected_candidate_pair_info` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:157] |
| `mrtc_peer_connection_create_data_channel` | `rtc_peer_connection_create_data_channel` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:161] |
| `mrtc_peer_connection_add_transceiver` | `rtc_peer_connection_add_transceiver` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:168] |
| `mrtc_transceiver_set_callbacks` | `rtc_transceiver_set_callbacks` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:173] |
| `mrtc_transceiver_on_frame` | `rtc_transceiver_on_frame` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:177] |
| `mrtc_transceiver_on_picture_loss` | `rtc_transceiver_on_picture_loss` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:183] |
| `mrtc_transceiver_write_frame` | `rtc_transceiver_write_frame` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:188] |
| `mrtc_transceiver_free` | `rtc_transceiver_free` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:191] |
| `mrtc_data_channel_set_callbacks` | `rtc_data_channel_set_callbacks` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:193] |
| `mrtc_data_channel_send` | `rtc_data_channel_send` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:197] |
| `mrtc_data_channel_label` | `rtc_data_channel_label` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:202] |
| `mrtc_data_channel_id` | `rtc_data_channel_id` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:204] |
| `mrtc_data_channel_close` | `rtc_data_channel_close` | Rename; no old wrapper.[VERIFIED: include/micrortc/peer_connection.h:206] |

### Header Implementation Macros

| Old macro | New macro | Classification |
|-----------|-----------|----------------|
| `MRTC_MICRORTC_H` | `RTC_MICRORTC_H` | Header guard; not user-facing API, but scan should not leave old `MRTC_` in installed headers.[VERIFIED: include/micrortc/micrortc.h:1-2] |
| `MRTC_PEER_CONNECTION_H` | `RTC_PEER_CONNECTION_H` | Header guard; not user-facing API, but scan should not leave old `MRTC_` in installed headers.[VERIFIED: include/micrortc/peer_connection.h:1-2] |
| `MRTC_STATUS_DEFINED` | `RTC_STATUS_DEFINED` | Include guard helper; duplicate status definition should be resolved or renamed consistently.[VERIFIED: include/micrortc/*.h] |

## Architecture Patterns

### System Architecture Diagram

```text
include/micrortc/*.h
  -> public symbol inventory
  -> v1.1 symbol mapping document
  -> Phase 8 header/source rename plan

src/**/*.h + tests/**/*.c + examples/**/*.c
  -> private/test-only boundary inventory
  -> boundary document
  -> Phase 10 private boundary cleanup plan

repo-wide rg residual scan
  -> classify findings:
       forbidden public residual
       migration-doc whitelist
       source/provenance whitelist
       test-harness/env-var record
       ignored build/reflib artifact
  -> machine-readable report
  -> Phase 8-11 validation gates
```

### Recommended Project Structure

```text
docs/
├── api-v1.1-boundary.md       # public/private/test-only boundary contract
└── api-v1.1-symbol-map.md     # complete old->new public symbol map
scripts/
└── scan-api-residuals.sh      # classifies old naming residuals
build/reports/
└── api-residuals.json         # generated scan output
```

This is a recommended new structure because no `docs/` directory currently exists.[VERIFIED: find docs] It keeps developer-facing API docs outside `.planning/`, while `.planning/phases/07-api/07-RESEARCH.md` remains workflow research.[VERIFIED: find docs; GSD output path]

### Pattern 1: Installed Headers Are The Public API Source

**What:** Public API means identifiers visible through installed `include/micrortc/*.h`.[VERIFIED: cmake/MicroRtcInstall.cmake]  
**When to use:** Use this definition for API-01/API-02 mapping and residual scan forbidden zones.[VERIFIED: .planning/REQUIREMENTS.md]  
**Example:**

```bash
rg -n '\b(mrtc_[A-Za-z0-9_]+|MRTC_[A-Za-z0-9_]+)\b' include/micrortc
```

### Pattern 2: Private Headers Stay Private Even When Tests Include Them

**What:** `src/**/*.h` exposes internal SDP/ICE/STUN/TURN/DTLS/SRTP/SCTP/RTP/RTCP/media helpers, and tests may include them for white-box coverage.[VERIFIED: rg "#include" scan; src/**/*.h scan]  
**When to use:** Phase 7 docs should list these as private or test-only, not promote them to public API.[VERIFIED: .planning/ROADMAP.md]  
**Example:** `tests/sdp/test_sdp_roundtrip.c` includes `../../src/sdp.h` and `../../src/media/media_transceiver.h`, so SDP parser helpers are test-visible private APIs, not installed public APIs.[VERIFIED: tests/sdp/test_sdp_roundtrip.c]

### Pattern 3: Residual Scan Must Classify, Not Just Count

**What:** STYLE-04 requires distinguishing forbidden residuals from allowed source compliance records and migration docs.[VERIFIED: .planning/REQUIREMENTS.md]  
**When to use:** Every scan finding should carry `path`, `line`, `token`, `classification`, and `reason`.[VERIFIED: scripts/verify-v1.sh summary pattern]  
**Example:**

```json
{
  "path": "include/micrortc/peer_connection.h",
  "line": 126,
  "token": "mrtc_peer_connection_create",
  "classification": "forbidden_public_residual",
  "reason": "installed public header must use rtc_* in v1.1"
}
```

### Anti-Patterns to Avoid

- **Adding compatibility aliases:** `#define mrtc_* rtc_*` or inline wrapper functions directly violates API-04.[VERIFIED: .planning/REQUIREMENTS.md]
- **Treating all `MRTC_*` hits as failures:** Compliance docs and migration docs must remain allowed to mention old names.[VERIFIED: .planning/REQUIREMENTS.md; .planning/milestones/v1.0-phases/01-/COMPLIANCE.md]
- **Promoting `src/` helper APIs to installed headers for convenience:** Current private helpers include transport/media internals and should remain outside public API unless a future requirement explicitly asks for them.[VERIFIED: src/**/*.h scan; .planning/REQUIREMENTS.md HDR-04]
- **Relying on build artifacts for API inventory:** `build/`, `build-transport/`, `build-debug/`, `cmake-build-debug/` contain stale old-name binaries and generated files.[VERIFIED: runtime state inventory find]

## Boundary Inventory

### Public API Boundary

| Boundary | Items | Evidence |
|----------|-------|----------|
| Installed headers | `include/micrortc/micrortc.h`, `include/micrortc/peer_connection.h` | Only these two headers exist under `include/micrortc/`.[VERIFIED: find include/micrortc] |
| CMake install surface | `install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})` | Install copies the entire `include/` tree.[VERIFIED: cmake/MicroRtcInstall.cmake] |
| Package target | `micrortc::micrortc` | CMake alias/export namespace exists and v1.1 requirements keep package namespace unchanged.[VERIFIED: CMakeLists.txt; .planning/ROADMAP.md] |

### Private API Boundary

| Area | Representative private headers | Evidence |
|------|-------------------------------|----------|
| SDP | `src/sdp.h` | Test includes private SDP header directly.[VERIFIED: tests/sdp/test_sdp_roundtrip.c] |
| ICE/TURN/STUN | `src/ice/ice_agent.h`, `src/ice/ice_config.h`, `src/turn/turn_client.h`, `src/stun/stun_message.h` | Transport tests and network verifier include these private headers.[VERIFIED: tests/transport/*.c; tests/integration/mrtc_phase4_network_verify.c] |
| DTLS/SRTP/SCTP | `src/dtls/dtls_session.h`, `src/srtp/srtp_session.h`, `src/sctp/sctp_session.h` | Transport tests include these private headers.[VERIFIED: tests/transport/test_dtls_srtp.c; tests/transport/test_sctp_data_channel.c] |
| RTP/RTCP/media | `src/rtp/**`, `src/rtcp/**`, `src/media/media_transceiver.h` | Media tests and integration verifier use private media/RTP/RTCP helpers.[VERIFIED: tests/media/*.c; tests/integration/mrtc_phase5_media_verify.c] |
| Common internals | `src/common/mrtc_common.h`, `src/common/mrtc_mutex.h`, `src/common/mrtc_socket.h` | Internal modules include these private helpers.[VERIFIED: src/**/*.c scan] |

### Test-only / Demo-only Boundary

| Area | Items | Evidence |
|------|-------|----------|
| C white-box tests | Tests with `WITH_PRIVATE_INCLUDES` in CMake | CMake grants private `src` include dirs to media tests and integration media verifier.[VERIFIED: cmake/MicroRtcTests.cmake] |
| Chrome answerer demo | `examples/chrome-e2e/mrtc_chrome_answerer.c` includes `media/media_transceiver.h` | Demo currently uses private media send hook for E2E media packet capture.[VERIFIED: examples/chrome-e2e/mrtc_chrome_answerer.c; Phase 6 summaries] |
| Node/Playwright harness | `tests/e2e/*` | E2E harness is test tooling and not core library dependency.[VERIFIED: tests/e2e/package.json; .planning/milestones/v1.0-phases/06-chrome-e2e/06-CONTEXT.md] |

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Old API compatibility | Wrapper layer, macro aliases, duplicated old typedefs | Direct breaking rename plus migration map | API-04 explicitly rejects compatibility wrappers by default.[VERIFIED: .planning/REQUIREMENTS.md] |
| Full C parser for Phase 7 | Custom AST parser | Explicit public symbol table from installed headers plus `rg` verification | Public header surface is small and already enumerated; parser dependency is unnecessary for contract freeze.[VERIFIED: include/micrortc/*.h] |
| Style enforcement in Phase 7 | Reimplement `.clang-tidy` naming checker | Residual scan for old public names; leave full `.clang-tidy` loop to Phase 10 | STYLE-01/STYLE-02 are mapped to Phase 10, while STYLE-04 is residual classification in Phase 7.[VERIFIED: .planning/REQUIREMENTS.md Traceability] |
| API governance baseline | ABI diff or Doxygen reference | Markdown contract docs | FUT-01/FUT-02/FUT-03 defer API snapshot/Doxygen/ABI diff beyond current Phase 7 scope.[VERIFIED: .planning/REQUIREMENTS.md] |

**Key insight:** Phase 7 should freeze decisions and scanning semantics; implementation phases should consume that contract instead of renegotiating names while editing C headers.[VERIFIED: .planning/ROADMAP.md]

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None found as project-owned database/datastore; scan found JSON config/artifact files but no SQLite/db files in repo root depth 3.[VERIFIED: find *.db/*.sqlite/*.json scan] | No data migration for Phase 7; future rename phases should ignore generated reports or regenerate them.[VERIFIED: find scan] |
| Live service config | `mrtc-ice-servers.local.json` exists locally and is gitignored-style local TURN config; its filename contains `mrtc` but contents are runtime ICE/TURN server config, not public C API.[VERIFIED: find local scan; Phase 6 context] | Do not edit or commit local secret config in Phase 7; record as local runtime config unaffected by C symbol rename.[VERIFIED: .planning/milestones/v1.0-phases/06-chrome-e2e/06-CONTEXT.md] |
| OS-registered state | None found in repo for systemd/launchd/pm2/task scheduler style registrations.[VERIFIED: find *.service/*.plist/ecosystem config scan] | No OS re-registration task for Phase 7.[VERIFIED: find scan] |
| Secrets/env vars | No live `MRTC_*`, `MICRORTC_*`, `KVS_*`, or `AWS_*` environment variables were present in the shell.[VERIFIED: env scan] Existing code uses test env names such as `MRTC_E2E_BROWSER_CHANNEL` and `MRTC_E2E_FIXTURES` in scripts.[VERIFIED: scripts/verify-v1.sh] | Classify test harness env vars separately from public C API residuals; decide in Phase 10 whether to rename env vars under style cleanup.[VERIFIED: .planning/REQUIREMENTS.md Traceability] |
| Build artifacts | `build/`, `build-transport/`, `build-debug/`, and `cmake-build-debug/` contain old `mrtc_*` executables, generated CMake files, installed headers and static libraries.[VERIFIED: find build artifacts scan] | Residual scan must exclude build artifact directories by default and require clean rebuild after actual Phase 8-11 renames.[VERIFIED: find build artifacts scan] |

## Common Pitfalls

### Pitfall 1: Public Header Tunnel Vision

**What goes wrong:** Planner inventories only `include/micrortc/*.h` and misses tests/examples that currently rely on private headers.[VERIFIED: rg "#include" scan]  
**Why it happens:** CMake selectively grants `src` include dirs to tests and `mrtc_chrome_answerer`.[VERIFIED: cmake/MicroRtcTests.cmake]  
**How to avoid:** Document public/private/test-only boundaries in separate tables and do not promote test-only helpers to public API.[VERIFIED: .planning/ROADMAP.md]  
**Warning signs:** New mapping doc contains `mrtc_sdp_*`, `mrtc_stun_*`, `mrtc_rtp_*`, `mrtc_rtcp_*`, `mrtc_srtp_*`, `mrtc_sctp_*`, or `mrtc_ice_config_*` as public APIs.[VERIFIED: src/**/*.h scan]

### Pitfall 2: Accidental Compatibility Layer

**What goes wrong:** Phase 8 adds `#define mrtc_initialize rtc_initialize` or old typedef aliases to keep tests compiling.[VERIFIED: API-04 constraint]  
**Why it happens:** Mechanical rename can break many call sites, and wrapper aliases look like a quick fix.[VERIFIED: rg residual scan]  
**How to avoid:** Phase 7 docs must explicitly say old public names are deleted and all call sites migrate to new names.[VERIFIED: .planning/REQUIREMENTS.md; .planning/STATE.md]  
**Warning signs:** Public headers contain both `rtc_*` and `mrtc_*`, or both `Rtc*` and `MRTC_*` typedef names.[VERIFIED: include/micrortc/*.h current scan]

### Pitfall 3: False Positive Residual Failures

**What goes wrong:** A repo-wide scan fails because compliance docs, migration docs, v1.0 archives, or `reflib/` mention AWS/KVS or old names.[VERIFIED: rg residual scan; .planning/milestones/v1.0-phases/01-/COMPLIANCE.md]  
**Why it happens:** STYLE-04 asks for classification, not a single grep count.[VERIFIED: .planning/REQUIREMENTS.md]  
**How to avoid:** Build an explicit whitelist with categories and reasons.[VERIFIED: .planning/REQUIREMENTS.md]  
**Warning signs:** Scan has no category field, or treats `.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md` as a failure.[VERIFIED: .planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md]

### Pitfall 4: Stale Build Artifacts Pollute Results

**What goes wrong:** `build/` contains `mrtc_*` executables and installed old headers, causing residual scan failures after source changes.[VERIFIED: find build artifacts scan]  
**Why it happens:** Generated build/install trees persist old names until cleaned or rebuilt.[VERIFIED: find build artifacts scan]  
**How to avoid:** Default scan excludes build directories; verification rebuilds before inspecting generated install output.[VERIFIED: scripts/verify-v1.sh build stage]  
**Warning signs:** Scan findings point under `build/`, `build-transport/`, `build-debug/`, or `cmake-build-debug/`.[VERIFIED: find build artifacts scan]

## Code Examples

Verified patterns from local sources:

### Residual Scan Skeleton

```bash
#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPORT_DIR="$ROOT_DIR/build/reports"
REPORT="$REPORT_DIR/api-residuals.json"

mkdir -p "$REPORT_DIR"

rg -n --json \
  --glob '!reflib/**' \
  --glob '!build/**' \
  --glob '!build-*/**' \
  --glob '!cmake-build-debug/**' \
  '\b(mrtc_[A-Za-z0-9_]+|MRTC_[A-Z0-9_]+|AWS|KVS)\b' \
  "$ROOT_DIR/include" "$ROOT_DIR/src" "$ROOT_DIR/tests" "$ROOT_DIR/examples" "$ROOT_DIR/docs" \
  > "$REPORT"
```

Source: existing Bash verification style and available `rg` tool.[VERIFIED: scripts/verify-v1.sh; rg --version]

### Public API Inventory Command

```bash
rg -n '^(typedef (struct|enum)|\} [A-Za-z_][A-Za-z0-9_]*;|[A-Z][A-Za-z0-9_]* [a-z_][A-Za-z0-9_]*\(|const char \*[a-z_][A-Za-z0-9_]*\(|void [a-z_][A-Za-z0-9_]*\(|unsigned short [a-z_][A-Za-z0-9_]*\(|#define [A-Z][A-Z0-9_]+|    [A-Z][A-Z0-9_]+)' include/micrortc/*.h
```

Source: command used during this research to enumerate current public header symbols.[VERIFIED: rg public symbol scan]

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| v1 public API uses AWS/KVS-adjacent `mrtc_*`/`MRTC_*` style | v1.1 target uses `rtc_*`, `Rtc*`, `RTC_*` | v1.1 roadmap created 2026-05-15 | Phase 7 must freeze mapping before Phase 8 edits.[VERIFIED: .planning/ROADMAP.md; .planning/STATE.md] |
| v1 acceptance uses `scripts/verify-v1.sh` and summary JSON | v1.1 should preserve v1 regression gates while adding API/style categories | Phase 11 roadmap | Phase 7 scan output should be compatible with later summary/gate separation.[VERIFIED: .planning/ROADMAP.md; scripts/verify-v1.sh] |
| Default CTest excludes Chrome E2E | Chrome E2E remains explicit through scripts/npm | Phase 6 | Phase 7 should not add browser dependency to API scan.[VERIFIED: .planning/milestones/v1.0-phases/06-chrome-e2e/06-CONTEXT.md; scripts/verify-v1.sh] |

**Deprecated/outdated:**

- Old public `mrtc_*` / `MRTC_*` names are migration/removal targets, not compatibility targets.[VERIFIED: .planning/REQUIREMENTS.md; .planning/STATE.md]
- AWS/KVS product public API names are not present in installed headers now, but AWS/KVS provenance references remain valid in compliance/source records.[VERIFIED: include/micrortc/*.h scan; .planning/milestones/v1.0-phases/01-/COMPLIANCE.md]

## Assumptions Log

All claims in this research were verified or cited from local project files and command output; no `[ASSUMED]` claims are used.

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| — | — | — | — |

## Open Questions (RESOLVED)

1. **RESOLVED: Should test harness env vars keep `MRTC_E2E_*` until Phase 10?**
   - What we know: `scripts/verify-v1.sh` uses `MRTC_E2E_BROWSER_CHANNEL` and `MRTC_E2E_FIXTURES` for E2E configuration.[VERIFIED: scripts/verify-v1.sh]
   - What's unclear: Requirements focus on public C API residuals in Phase 7, while STYLE-01/STYLE-02 broader style convergence belongs to Phase 10.[VERIFIED: .planning/REQUIREMENTS.md]
   - RESOLVED recommendation: Phase 7 should classify these as `test_harness_non_public_api` and leave rename decision to Phase 10.[VERIFIED: .planning/REQUIREMENTS.md Traceability]

2. **RESOLVED: Should `RtcPeerConnection` opaque handle names be pointer typedefs or explicit `RtcPeerConnectionHandle`?**
   - What we know: Requirements require public types/typedefs/enums to converge to `Rtc*`, and API-04 rejects old handle style as compatibility target.[VERIFIED: .planning/REQUIREMENTS.md]
   - What's unclear: The exact handle suffix policy is not explicitly stated beyond removing old `MRTC_*`/AWS style.[VERIFIED: .planning/STATE.md]
   - RESOLVED recommendation: Use `RtcPeerConnection`, `RtcDataChannel`, `RtcRtpTransceiver` as opaque handle typedef names and document the choice in Phase 7 mapping.[VERIFIED: .planning/REQUIREMENTS.md]

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| Bash | `scripts/scan-api-residuals.sh` | ✓ | 5.1.16 | POSIX shell with reduced features, but repo scripts already use Bash.[VERIFIED: bash --version; scripts/verify-v1.sh] |
| ripgrep | residual scanning | ✓ | 15.1.0 | `grep -R`, lower quality JSON/report support.[VERIFIED: rg --version] |
| CMake | build/package context | ✓ | 3.22.1 | None needed for Phase 7 docs-only tasks.[VERIFIED: cmake --version] |
| CTest | validation architecture | ✓ | 3.22.1 | None needed for Phase 7 docs-only tasks.[VERIFIED: ctest --version] |
| Node.js | existing E2E summary tooling | ✓ | 24.14.0 | Avoid Node dependency for Phase 7 scan unless integrating summaries.[VERIFIED: node --version] |
| npm | existing E2E dependency install | ✓ | 11.9.0 | Avoid npm dependency for Phase 7 scan.[VERIFIED: npm --version] |
| OpenSSL | v1 regression transport tests | ✓ | 1.1.1w | Existing build can use fallback depending on CMake options.[VERIFIED: pkg-config/openssl version; CMakeLists.txt] |
| libsrtp2 | v1 regression transport tests | ✓ | 2.4.2 | Existing non-strict build can fallback when not required.[VERIFIED: pkg-config libsrtp2] |
| usrsctp | v1 regression transport tests | ✓ | 0.9.5.0 | Existing non-strict build can fallback when not required.[VERIFIED: pkg-config usrsctp] |
| clang-tidy | future style convergence | ✗ | — | Phase 7 uses residual scan; Phase 10 should install/probe clang-tidy before enforcing `.clang-tidy`.[VERIFIED: command -v clang-tidy; .clang-tidy] |

**Missing dependencies with no fallback:**

- None for Phase 7 documentation and residual scan contract.[VERIFIED: environment audit]

**Missing dependencies with fallback:**

- `clang-tidy` is missing; Phase 7 can document `.clang-tidy` rules and use `rg`-based residual scan, while Phase 10 handles full style enforcement.[VERIFIED: command -v clang-tidy; .planning/REQUIREMENTS.md]

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | CTest 3.22.1 for C tests; Bash scan script for API residual classification; existing Node/Playwright is separate E2E tooling.[VERIFIED: ctest --version; scripts/verify-v1.sh; tests/e2e/package.json] |
| Config file | `CMakeLists.txt`, `cmake/MicroRtcTests.cmake`, `.clang-tidy` for naming policy documentation.[VERIFIED: CMakeLists.txt; cmake/MicroRtcTests.cmake; .clang-tidy] |
| Quick run command | `scripts/scan-api-residuals.sh` after Phase 7 creates it.[VERIFIED: Phase 7 requirement STYLE-04] |
| Full suite command | `scripts/verify-v1.sh` remains v1 regression entrypoint.[VERIFIED: scripts/verify-v1.sh; .planning/ROADMAP.md] |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|--------------|
| API-01 | Boundary doc lists public/private/test-only API boundaries | docs validation | `test -f docs/api-v1.1-boundary.md && rg -n 'Public|Private|test-only|include/micrortc|src/' docs/api-v1.1-boundary.md` | ❌ Wave 0 |
| API-02 | Symbol map covers every current `include/micrortc/*.h` public symbol | scan/docs validation | `scripts/scan-api-residuals.sh --check-symbol-map docs/api-v1.1-symbol-map.md` | ❌ Wave 0 |
| API-04 | Deletion policy states no old wrapper layer | docs validation | `rg -n '不提供兼容|no compatibility|wrapper' docs/api-v1.1-boundary.md docs/api-v1.1-symbol-map.md` | ❌ Wave 0 |
| STYLE-04 | Residual scan emits categorized findings | script validation | `scripts/scan-api-residuals.sh --format json --output build/reports/api-residuals.json` | ❌ Wave 0 |

### Sampling Rate

- **Per task commit:** Run the relevant docs validation command plus `scripts/scan-api-residuals.sh` once it exists.[VERIFIED: Phase 7 requirements]
- **Per wave merge:** Run `ctest --test-dir build --output-on-failure` if CMake files or scripts touch build/test behavior.[VERIFIED: ctest -N]
- **Phase gate:** Run `scripts/scan-api-residuals.sh` and verify its report distinguishes forbidden residuals, compliance records, migration docs whitelist and ignored generated artifacts.[VERIFIED: STYLE-04]

### Wave 0 Gaps

- [ ] `docs/api-v1.1-boundary.md` — covers API-01.[VERIFIED: no docs directory found]
- [ ] `docs/api-v1.1-symbol-map.md` — covers API-02/API-04.[VERIFIED: no docs directory found]
- [ ] `scripts/scan-api-residuals.sh` — covers STYLE-04.[VERIFIED: find scripts scan]
- [ ] `build/reports/api-residuals.json` schema expectation — needed by planner if machine-readable validation is required.[VERIFIED: scripts/verify-v1.sh summary pattern]

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|------------------|
| V2 Authentication | no | Phase 7 docs/scripts do not add authentication paths.[VERIFIED: Phase 7 roadmap scope] |
| V3 Session Management | no | Phase 7 docs/scripts do not add sessions.[VERIFIED: Phase 7 roadmap scope] |
| V4 Access Control | no | Phase 7 docs/scripts do not add authorization boundaries.[VERIFIED: Phase 7 roadmap scope] |
| V5 Input Validation | yes | Shell scripts should quote variables, validate paths/options, and avoid `eval`; existing `verify-v1.sh` has explicit option parsing pattern.[VERIFIED: scripts/verify-v1.sh] |
| V6 Cryptography | no new crypto | Phase 7 does not change DTLS/SRTP/STUN crypto code.[VERIFIED: Phase 7 roadmap scope] |

### Known Threat Patterns for Phase 7 Scripts

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Shell option/path injection | Tampering | Quote variables, reject unknown options, normalize repo-relative paths like `verify-v1.sh` does.[VERIFIED: scripts/verify-v1.sh] |
| Secret leakage from local TURN config | Information Disclosure | Do not print `mrtc-ice-servers.local.json` contents; Phase 6 already treats TURN credentials as local secret input.[VERIFIED: .planning/milestones/v1.0-phases/06-chrome-e2e/06-CONTEXT.md] |
| False compliance from ignored findings | Repudiation | Residual report must include classification reason and whitelist source path.[VERIFIED: STYLE-04 requirement] |

## Sources

### Primary (HIGH confidence)

- `AGENTS.md` - project-specific Markdown language, baseline and core boundary constraints.
- `.planning/PROJECT.md` - project state, v1.1 goal, constraints, key decisions.
- `.planning/REQUIREMENTS.md` - API-01, API-02, API-04, STYLE-04, deferred items and traceability.
- `.planning/ROADMAP.md` - Phase 7 goal, dependencies and success criteria.
- `.planning/STATE.md` - locked v1.1 naming decisions and no-wrapper decision.
- `include/micrortc/micrortc.h`, `include/micrortc/peer_connection.h` - current public installed headers and symbol inventory.
- `cmake/MicroRtcInstall.cmake`, `cmake/MicroRtcTests.cmake`, `CMakeLists.txt` - install/test/package boundaries.
- `scripts/verify-v1.sh` - existing verification script pattern and v1 summary path.
- `.clang-tidy` - naming policy planned for v1.1 style convergence.
- `.planning/milestones/v1.0-phases/06-chrome-e2e/06-CONTEXT.md`, `06-VERIFICATION.md` - Phase 6 E2E/test boundary and v1 verification status.

### Secondary (MEDIUM confidence)

- Local command output: `rg` scans over `include`, `src`, `tests`, `examples`; `find` scans for runtime state; `ctest -N`; environment version probes.

### Tertiary (LOW confidence)

- None.

## Metadata

**Confidence breakdown:**

- Standard stack: HIGH - based on local tool probes and existing repo scripts.[VERIFIED: environment audit; scripts/verify-v1.sh]
- Architecture: HIGH - based on actual installed headers, CMake install/test rules, and Phase 7 roadmap.[VERIFIED: include/micrortc/*.h; cmake/*.cmake; .planning/ROADMAP.md]
- Pitfalls: HIGH - based on observed private header usage, build artifacts and locked no-wrapper decision.[VERIFIED: rg scans; find build artifacts scan; .planning/STATE.md]

**Research date:** 2026-05-15  
**Valid until:** 2026-06-14 for local codebase findings; re-run header and residual scans after any Phase 8+ API edits.[VERIFIED: current_date; local scans]
