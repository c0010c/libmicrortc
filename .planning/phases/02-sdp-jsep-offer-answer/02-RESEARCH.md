# Phase 2: SDP/JSEP 与 Offer/Answer - Research

**Researched:** 2026-05-10  
**Domain:** WebRTC SDP/JSEP、Chrome 1v1 offer/answer、纯 C 固定内存协议层  
**Confidence:** HIGH

## User Constraints

### 阶段边界

本阶段交付 Chrome 1v1 最小 SDP/JSEP 与 Offer/Answer 能力：生成本地 offer/answer SDP，解析 Chrome offer/answer，保存本地和远端 description，推进最小 offer/answer 状态机，并让 `addIceCandidate` 能解析、校验和保存远端 trickle candidate 字符串。

本阶段不实现 ICE connectivity checks、STUN、candidate gathering、DTLS 握手、SRTP、RTP/RTCP 或 Chrome 端到端通话。ICE/DTLS 相关字段只作为 SDP/JSEP 层所需的参数被存储、校验和序列化，真实生成和网络行为由后续阶段接入。

### 锁定决策

- **D-01:** 本阶段采用固定 Chrome 1v1 SDP 模板，不做通用 SDP builder。模板聚焦 1 路 audio、1 路 video、BUNDLE、rtcp-mux、trickle capability、Opus、H264、`sendrecv` 和 `recvonly`。
- **D-02:** ICE/DTLS 参数由用户 API 或测试夹具提供，包括 `ice-ufrag`、`ice-pwd`、DTLS fingerprint 和 setup role。本阶段只负责存储、校验和 SDP 序列化，不提前实现随机数、crypto、证书或 DTLS backend 生成逻辑。
- **D-03:** codec 画像固定为 Opus + H264 baseline 最小 Chrome 互通集合。`rtpmap`、`fmtp`、`rtcp-fb`、payload type 等字段由 golden tests 锁定；本阶段不做通用 codec 协商。
- **D-04:** 媒体方向只支持 `sendrecv` 和 `recvonly`。解析、保存和输出这两个方向；遇到 `sendonly` 或 `inactive` 返回稳定的不支持或协议错误。
- **D-05:** 内部实现最小 offer/answer 状态机，状态包括 `stable`、`have-local-offer`、`have-remote-offer`。它是 JSEP-compatible 的最小状态契约，不实现完整浏览器 JSEP。
- **D-06:** 正常发起流程为 `stable -> createOffer -> setLocalDescription(offer) -> have-local-offer -> setRemoteDescription(answer) -> stable`。
- **D-07:** 正常接听流程为 `stable -> setRemoteDescription(offer) -> have-remote-offer -> createAnswer -> setLocalDescription(answer) -> stable`。
- **D-08:** 本阶段不支持 rollback、pranswer、renegotiation 或多轮协商。非法顺序必须被拒绝。
- **D-09:** 非法状态转换返回稳定 `rtc_status_t`，并通过 observer error 与 trace 输出细节。trace/detail 至少包含 operation、当前状态、目标状态或拒绝原因；文本消息只能作为辅助，不能成为稳定契约。
- **D-10:** `addIceCandidate` 在第 2 阶段解析、校验并保存远端 candidate 字符串，受 `limits.ice.max_candidates` 约束。
- **D-11:** `addIceCandidate` 不启动 ICE connectivity checks，不计算 candidate pair，不执行 STUN transaction，不计算 ICE priority/foundation。
- **D-12:** 第 2 阶段不生成本地 candidate，不触发 observer `on_local_candidate`。本地 candidate gathering、host/srflx candidate 和 ICE 状态事件属于第 3 阶段。
- **D-13:** 本阶段可以在 SDP 中表达 ICE 参数和 trickle capability，但不得依赖本机网络接口、socket、UDP 端口或平台网络枚举。
- **D-14:** SDP golden tests 使用仓库内固定 Chrome SDP fixture 和本地 offer/answer expected，不依赖运行时启动 Chrome 或抓取 SDP。
- **D-15:** 动态字段通过测试夹具固定或规范化，例如 session id、ICE ufrag/pwd、DTLS fingerprint。
- **D-16:** 错误路径测试必须覆盖 SDP 结构错误、不支持字段和状态错误，包括缺失 BUNDLE/rtcp-mux、缺失 ICE/DTLS 字段、未知 codec、`sendonly`/`inactive`、非法 offer/answer 顺序、以及超过 `limits.sdp.max_description_bytes`。

### 智能体的自由裁量

用户未要求锁定内部文件名、parser/tokenizer 具体实现、fixture 目录命名、payload type 数值或 trace detail code 枚举值。planner 可以在不突破上述阶段边界的前提下决定这些实现细节，但必须保持纯 C、固定内存、无线程、无 GPL/LGPL 依赖，并遵守第 1 阶段已建立的 API、executor、observer、trace、counter 和 allocator 契约。

### 延后事项

- 完整 JSEP 能力，例如 rollback、pranswer、renegotiation：不属于第 2 阶段。
- ICE connectivity checks、host/srflx candidate、STUN、candidate pair、gathering 状态事件：第 3 阶段处理。
- DTLS 证书、fingerprint 生成、DTLS setup 执行和 SRTP key export：第 4 阶段处理。
- 通用 SDP builder、广泛 SDP 兼容、多 codec 协商、多路媒体：v1 首版范围外或后续扩展处理。

## Summary

第 2 阶段应把 SDP/JSEP 作为 signaling executor 上的纯数据层实现：创建时从 arena 切出 description 缓冲区、解析结果结构和远端 candidate 表；运行期只做输入字符串解析、固定模板序列化、状态转换和可观测输出。[VERIFIED: include/rtc/peer_connection.h, src/api/peer_connection.c, docs/API-执行器与内存契约.md]

协议标准支持这个边界：SDP 是会话描述格式，不拥有传输；JSEP 定义浏览器侧 offer/answer 状态模型；ICE SDP Usage 和 Trickle ICE 定义 SDP 中的 ICE 参数和增量 candidate 表达，但 connectivity checks 和 candidate gathering 是 ICE 层职责。[CITED: https://www.rfc-editor.org/rfc/rfc8866.html] [CITED: https://www.rfc-editor.org/rfc/rfc8829.html] [CITED: https://www.rfc-editor.org/rfc/rfc8839] [CITED: https://www.rfc-editor.org/rfc/rfc8838]

**Primary recommendation:** 建立 `src/sdp` + `src/jsep` 两个内部子系统，先用固定 Chrome 1v1 profile 和 golden tests 锁定 SDP 行为，再把现有 `RTC_STATUS_UNSUPPORTED` 占位 API 替换为状态机驱动的真实实现。[VERIFIED: codebase grep]

## Project Constraints (from AGENTS.md)

- 所有 Markdown 文档尽可能使用中文编写。
- 优先读取 `.planning/PROJECT.md`、`.planning/REQUIREMENTS.md`、`.planning/ROADMAP.md`、`.planning/STATE.md` 和当前阶段文档。
- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发的项目边界。
- 不要引入 GPL/LGPL 依赖。
- Markdown 文档必须使用中文；技术标识符、API 名称、协议名和需求 ID 可以保留英文。
- 修改规划文档时同步维护需求追踪和项目状态。

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|--------------|----------------|-----------|
| SDP 固定 profile 与序列化 | `src/sdp` | `src/api` | SDP 是格式化和 profile 校验职责，API 只负责调用和状态推进。 |
| Chrome offer/answer 解析 | `src/sdp` | `src/jsep` | parser 应输出结构化 description，JSEP 消费 description type 和 media 参数。 |
| 最小 offer/answer 状态机 | `src/jsep` | `src/api` | 状态转换与非法顺序判断应独立于 API glue，便于单元测试。 |
| 远端 candidate 保存 | `src/sdp` / `src/jsep` | `src/api` | 第 2 阶段只解析、校验、容量约束和保存，不进入 ICE connectivity。 |
| 可观测性 | `src/api` | `src/observability` | 公开 API 统一触发 observer error、state 和 trace，沿用第 1 阶段模式。 |

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C99 标准库 | 项目已配置 C99 | 字符扫描、定长复制、整数解析 | 当前 CMake 已锁定 `CMAKE_C_STANDARD 99`，无需第三方依赖。[VERIFIED: CMakeLists.txt] |
| 自研纯 C 测试 runner | 已存在 | 单元测试和 golden test | 仓库已有 `tests/test_runner.h` 与 CTest；引入外部测试库会扩大依赖面。[VERIFIED: tests/test_runner.h, CMakeLists.txt] |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| 无新增外部库 | N/A | 固定模板 SDP、line scanner、状态机 | 本阶段不需要 parser generator、regex 或 WebRTC 第三方库；避免 license 和动态内存风险。 |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| 手写 line scanner | parser generator | generator 可能引入构建和许可复杂度，且本阶段 profile 固定，收益不足。 |
| 固定 profile serializer | 通用 SDP builder | 与 D-01 冲突，且容易把 v2 泛 SDP 兼容提前拖入当前阶段。 |
| C 测试 runner | 第三方测试框架 | 与现有仓库模式不一致，并增加依赖审计成本。 |

**Installation:** 无新增安装命令。

## Architecture Patterns

### System Architecture Diagram

```text
用户 API(signaling executor)
  |
  +-- createOffer/createAnswer
  |     -> jsep 状态检查
  |     -> sdp profile serializer
  |     -> out_sdp + trace
  |
  +-- setLocalDescription/setRemoteDescription
  |     -> sdp line parser
  |     -> Chrome 1v1 profile validator
  |     -> jsep 状态转换
  |     -> 保存 local/remote summary + observer state/trace
  |
  +-- addIceCandidate
        -> candidate attribute parser
        -> limits.ice.max_candidates 容量检查
        -> 保存 remote candidate 字符串与摘要
        -> 不启动 ICE/STUN
```

### Recommended Project Structure

```text
include/rtc/
├── peer_connection.h     # 既有公开 API；可扩展 SDP 参数配置类型
├── config.h              # create-time config 可承载 SDP/JSEP profile 输入
└── trace.h               # 增加 signaling/sdp/jsep trace 事件和字段
src/
├── api/peer_connection.c # API glue、executor 亲和、observer/trace
├── sdp/                  # SDP line parser、profile validator、serializer
└── jsep/                 # 最小 signaling state 和状态转换
tests/
├── fixtures/             # Chrome SDP 和 expected local SDP golden
├── test_sdp.c            # parser/serializer/profile tests
└── test_jsep.c           # 状态机和 API 顺序 tests
```

### Pattern 1: 固定 profile 数据结构先行

**What:** 用内部结构描述 `ice_ufrag`、`ice_pwd`、fingerprint、setup、audio/video direction、payload type、codec fmtp，而不是在状态机里直接拼字符串。  
**When to use:** serializer、parser 和 JSEP 都需要同一组 SDP 字段时。  
**Example:** `rtc_sdp_description_t` 保存 session-level BUNDLE 和两个 media section；serializer 从结构输出 CRLF SDP；parser 从 CRLF SDP 填回结构。

### Pattern 2: line scanner + profile validator 分离

**What:** scanner 只负责拆行、识别 `v=`/`a=`/`m=` 字段和基础值；validator 负责 Chrome 1v1 画像，例如 BUNDLE、rtcp-mux、Opus/H264、direction。  
**When to use:** 错误路径需要区分结构错误、容量错误、不支持字段和 profile 缺失时。  
**Why:** RFC 8866 定义 SDP 格式，WebRTC profile 约束来自 JSEP/ICE/BUNDLE/项目决策，分离后测试更直接。[CITED: https://www.rfc-editor.org/rfc/rfc8866.html]

### Pattern 3: API 层只做亲和和可观测性

**What:** `src/api/peer_connection.c` 调用 `sdp` 和 `jsep` 子系统，集中处理 `rtc_require_pc`、`rtc_executor_require`、observer error 和 trace。  
**When to use:** 替换第 1 阶段 unsupported API。  
**Why:** 保留第 1 阶段执行器和错误模式，避免 `PeerConnection` 变成 SDP parser。

### Anti-Patterns to Avoid

- **把 parser/generator 全塞进 `src/api/peer_connection.c`:** 会破坏子系统边界，后续 ICE/DTLS/RTP 接入时难维护。
- **用 `malloc`/`strdup` 保存 SDP 和 candidate:** 违反固定 arena 与运行期不动态增长。
- **直接接受 Chrome SDP 中所有 codec:** 与 D-03 冲突；本阶段应固定 Opus + H264 baseline，并对未知必选画像返回协议错误或不支持。
- **`addIceCandidate` 顺手启动 ICE:** 与 D-11 冲突，且会提前进入第 3 阶段。
- **用文本错误作为稳定契约:** 与 D-09 冲突；必须用 `rtc_status_t`、detail code 和 trace fields。

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| 通用 SDP 语法树 | 完整 AST、任意 attribute 保留和重排 | 固定 Chrome 1v1 profile 结构 | D-01 锁定固定模板，当前需求不需要泛化。 |
| 完整 JSEP | rollback/pranswer/renegotiation | `stable` / `have-local-offer` / `have-remote-offer` 状态机 | D-05 到 D-08 锁定最小状态契约。 |
| ICE agent | candidate pair、STUN transaction、priority 计算 | candidate 字符串解析和保存 | D-10 到 D-13 把 ICE 行为推迟到第 3 阶段。 |
| Crypto/fingerprint 生成 | 随机数、证书、hash backend | 用户/测试夹具提供 fingerprint 字符串 | D-02 避免提前接入第 4 阶段安全后端。 |

**Key insight:** 第 2 阶段的复杂度主要在边界控制：要让 Chrome SDP/JSEP 流程可测试，但不能让 ICE/DTLS/RTP 真实行为泄漏进来。

## Common Pitfalls

### Pitfall 1: `createOffer` 成功但没有可设置的本地 description

**What goes wrong:** 只把 SDP 写到 `out_sdp`，没有在 `setLocalDescription` 中重新解析并推进状态。  
**Why it happens:** 把 create 和 set 混为一谈。  
**How to avoid:** `createOffer`/`createAnswer` 只生成 SDP；状态推进发生在 `setLocalDescription`/`setRemoteDescription`。  
**Warning signs:** 测试没有覆盖 `createOffer -> setLocalDescription -> setRemoteDescription` 完整流程。

### Pitfall 2: Chrome fixture 太宽导致 profile 验证失焦

**What goes wrong:** Chrome offer 包含 VP8/VP9/AV1/H265/RTX/RED/ULPFEC 等字段，parser 误以为都要支持。  
**Why it happens:** 把“能解析输入 fixture”误读成“支持所有 codec”。  
**How to avoid:** 解析可以跳过非目标 codec，但 profile validator 必须确认 Opus + H264 baseline 存在，并拒绝目标画像缺失。  
**Warning signs:** 计划没有明确 unknown codec 策略。

### Pitfall 3: SDP buffer 容量只检查输出不检查输入

**What goes wrong:** `setRemoteDescription` 接受超过 `limits.sdp.max_description_bytes` 的 SDP。  
**Why it happens:** 只关注 serializer。  
**How to avoid:** 所有 description 输入和输出都先检查 `limits.sdp.max_description_bytes`。  
**Warning signs:** 错误测试只覆盖 `createOffer` buffer 太小。

### Pitfall 4: candidate 保存没有创建期切分容量

**What goes wrong:** `addIceCandidate` 运行期分配字符串数组或直接保存调用方指针。  
**Why it happens:** 忽略 candidate 字符串所有权。  
**How to avoid:** create 阶段从 arena 切出 `max_candidates` 个固定槽，每槽容量受 SDP/candidate limit 约束，复制 candidate 内容。  
**Warning signs:** candidate 测试没有在调用后修改输入 buffer。

### Pitfall 5: observer 和 trace 不带稳定 detail

**What goes wrong:** 只能看到 `PROTOCOL_ERROR`，不知道缺哪个 SDP 字段或哪个状态转换被拒绝。  
**Why it happens:** 只沿用 unsupported API trace。  
**How to avoid:** 增加 signaling/sdp/jsep detail code 和 trace fields：operation、current_state、target_state、description_type、reason。  
**Warning signs:** 错误路径测试只断言返回码。

## Code Examples

### 状态机表驱动建议

```c
/* 内部示例，供计划引用；非最终 API。 */
typedef enum rtc_jsep_state_t {
    RTC_JSEP_STABLE = 0,
    RTC_JSEP_HAVE_LOCAL_OFFER,
    RTC_JSEP_HAVE_REMOTE_OFFER
} rtc_jsep_state_t;

typedef enum rtc_sdp_type_t {
    RTC_SDP_TYPE_OFFER = 0,
    RTC_SDP_TYPE_ANSWER
} rtc_sdp_type_t;
```

### 输出缓冲区 API 约定建议

```c
/* 与现有 create_offer/create_answer 签名兼容。 */
if (out_sdp == 0 || inout_sdp_len == 0) {
    return RTC_STATUS_INVALID_ARGUMENT;
}
if (*inout_sdp_len < required_len) {
    *inout_sdp_len = required_len;
    return RTC_STATUS_CAPACITY_SDP_BUFFER;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| RFC 4566 SDP | RFC 8866 SDP | 2021 | 以 RFC 8866 作为 SDP 格式引用。[CITED: https://www.rfc-editor.org/rfc/rfc8866.html] |
| RFC 5245 ICE SDP 内容混在 ICE 主规范 | RFC 8445 + RFC 8839 分离 ICE core 和 SDP offer/answer | 2021 | 本阶段引用 RFC 8839 做 SDP ICE 参数，不实现 RFC 8445 connectivity。[CITED: https://www.rfc-editor.org/rfc/rfc8839] |
| 非 trickle 完整 candidate SDP | Trickle ICE 增量 candidate | 2021 RFC 8838 | 本阶段保留 `ice-options:trickle` 和 `addIceCandidate` 保存能力。[CITED: https://www.rfc-editor.org/rfc/rfc8838] |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | 计划建议使用 H264 payload type `103` 与 Opus payload type `111` 作为 golden 输出默认值。 | Architecture Patterns | 若未来希望不同 payload type，golden expected 需调整；不影响阶段边界。 |
| A2 | 远端 candidate 单条保存容量可复用 `limits.sdp.max_description_bytes` 的较小派生上限或固定内部上限。 | Common Pitfalls | 若 candidate 字符串上限过小，需在执行时补一个 public limit 字段。 |

## Open Questions

1. **公开 config 如何暴露 SDP 参数？**
   - What we know: D-02 要求用户 API 或测试夹具提供 ICE/DTLS 参数；当前 `rtc_peer_connection_config_t` 没有专门字段。
   - What's unclear: 是否直接扩展 `rtc_peer_connection_config_t`，还是增加后续 setter。
   - Recommendation: 本阶段计划采用 create-time config 扩展，保持固定内存和 deterministic golden tests。

2. **Chrome fixture 是否应保留 JSON 包装？**
   - What we know: 仓库已有 `chrome_offer.sdp`，内容是 JSON 包装的 `{type,sdp}`。
   - What's unclear: 测试读取时是否解析 JSON。
   - Recommendation: 新增纯 SDP fixture，避免在 C 测试中引入 JSON parser。

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| CMake | 构建和测试 | yes | 项目要求 3.16+ | 无 |
| CTest | 自动化测试 | yes | 随 CMake | 直接运行 `build/rtc_tests` |
| 外部 SDP/WebRTC 库 | 不需要 | N/A | N/A | 固定 profile 手写实现 |

**Missing dependencies with no fallback:** 无。

**Missing dependencies with fallback:** 无。

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | 纯 C 自研 test runner + CTest |
| Config file | `CMakeLists.txt` |
| Quick run command | `cmake --build build && ctest --test-dir build --output-on-failure` |
| Full suite command | `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|--------------|
| SDP-01 | 生成 Chrome 可接受本地 offer SDP | golden/unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ Wave 1 |
| SDP-02 | 基于 Chrome offer 生成 answer SDP | golden/unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ Wave 2 |
| SDP-03 | 解析 Chrome offer/answer 中 BUNDLE、rtcp-mux、ICE、DTLS、Opus、H264、方向 | unit/golden | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ Wave 2 |
| SDP-04 | 支持 `sendrecv` 和 `recvonly`，拒绝 `sendonly`/`inactive` | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ Wave 2 |
| API-04 | offer/answer/set description/add candidate 流程可用并拒绝非法状态 | integration/unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ Wave 3 |

### Sampling Rate

- **Per task commit:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Per wave merge:** `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`
- **Phase gate:** Full suite green before `$gsd-verify-work`

### Wave 0 Gaps

- [ ] `tests/test_sdp.c` — covers SDP-01, SDP-02, SDP-03, SDP-04.
- [ ] `tests/test_jsep.c` — covers API-04 and illegal state transitions.
- [ ] `tests/fixtures/*.sdp` — Chrome fixture and expected local SDP golden.

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|------------------|
| V2 Authentication | no | N/A |
| V3 Session Management | no | N/A |
| V4 Access Control | no | N/A |
| V5 Input Validation | yes | strict SDP/candidate length checks, field validation, unsupported field rejection |
| V6 Cryptography | limited | fingerprint is parsed and stored only; no crypto implementation in this phase |

### Known Threat Patterns for SDP/JSEP

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Oversized SDP or candidate input | Denial of Service | check `limits.sdp.max_description_bytes` and fixed candidate slots before copying |
| Malformed SDP line triggers out-of-bounds read | Tampering / DoS | length-bounded scanner, no NUL-termination assumptions |
| Unsupported direction/codec silently accepted | Tampering | profile validator returns `RTC_STATUS_UNSUPPORTED` or `RTC_STATUS_PROTOCOL_ERROR` |
| Fingerprint missing or malformed | Spoofing | reject description before state transition |

## Sources

### Primary (HIGH confidence)

- https://www.rfc-editor.org/rfc/rfc8866.html — SDP format and scope.
- https://www.rfc-editor.org/rfc/rfc8829.html — JSEP offer/answer model.
- https://www.rfc-editor.org/rfc/rfc8839 — SDP offer/answer procedures for ICE.
- https://www.rfc-editor.org/rfc/rfc8838 — Trickle ICE candidate exchange model.
- https://www.rfc-editor.org/rfc/rfc8843 — BUNDLE SDP grouping.
- https://www.rfc-editor.org/rfc/rfc6184.html — H264 RTP payload parameters such as `profile-level-id` and `packetization-mode`.
- https://www.w3.org/TR/webrtc/ — WebRTC signaling state names.
- Local code: `include/rtc/peer_connection.h`, `src/api/peer_connection.c`, `include/rtc/limits.h`, `docs/API-执行器与内存契约.md`.

### Secondary (MEDIUM confidence)

- `chrome_offer.sdp` — existing Chrome SDP capture in repository, useful as fixture seed but should be normalized into pure SDP test fixture.

### Tertiary (LOW confidence)

- 无。

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — 当前阶段无需新增依赖，已由仓库代码和 CMake 验证。
- Architecture: HIGH — 上游边界、CONTEXT.md 决策和 RFC 责任划分一致。
- Pitfalls: MEDIUM-HIGH — 来自本仓库边界和 WebRTC SDP/JSEP 常见分层风险。

**Research date:** 2026-05-10  
**Valid until:** 2026-06-09

## RESEARCH COMPLETE
