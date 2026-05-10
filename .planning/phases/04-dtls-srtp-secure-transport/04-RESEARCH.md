# 第 4 阶段：DTLS-SRTP 安全传输 - Research

**Researched:** 2026-05-10  
**Domain:** WebRTC DTLS-SRTP、security backend vtable、固定内存安全状态机  
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
## 实现决策

### 安全 backend 边界

- **D-01:** 第 4 阶段采用单一 `security_backend` vtable，而不是拆成 DTLS、SRTP、crypto 多个公开 backend。该 vtable 覆盖 DTLS session、fingerprint 查询、DTLS datagram 输入输出、key export、SRTP/SRTCP protect/unprotect 等安全层职责。
- **D-02:** `rtc_peer_connection_config_t.security_backend` 从当前 `void *` 占位演进为指向稳定 backend vtable/config 的入口。planner 可以决定具体命名和结构拆分，但公共形状应保持“一个安全 backend”。
- **D-03:** 核心库在 create-time 从用户 arena 中为 backend session/storage 切固定内存。backend 不得在核心运行期偷偷依赖动态增长；如真实第三方库需要内部内存，适配层必须在第 4 阶段文档中明确它如何满足固定 storage 契约。
- **D-04:** 第 4 阶段必须提供 deterministic 测试后端，用于验证核心安全状态机和错误语义。真实库参考适配只要求接口骨架、文档或可选示例，不作为本阶段成功标准。
- **D-05:** DTLS/SRTP 产生的 UDP datagram 继续通过既有 `observer.on_datagram` 输出。用户仍负责 socket/UDP 发送；库和 backend 都不得直接发送网络包，也不新增 `on_dtls_datagram` 分叉路径。

### 握手启动与角色语义

- **D-06:** DTLS handshake 在 ICE selected pair / ICE connected 后由核心自动启动。用户不需要也不应该在常规 `PeerConnection` 路径中显式调用 `start_dtls`。
- **D-07:** DTLS role 由 SDP `setup` 属性和既有 JSEP/ICE 角色推导。planner 应研究并实现 Chrome 1v1 最小画像下 `actpass`、`active`、`passive` 到 DTLS client/server 的映射，不能新增用户手动配置 role 的常规路径。
- **D-08:** 收到 DTLS datagram 但 ICE 尚未 connected/selected 时，核心应拒绝该输入并输出 trace/error，不缓存早到 DTLS 包。第 4 阶段不引入 DTLS packet queue 或额外早到包容量。
- **D-09:** DTLS close/error 只影响 DTLS/SRTP 安全状态，不回滚 ICE 状态。ICE selected pair 仍作为网络层事实存在；安全失败通过 `dtls.failed` 或等价状态、observer error、trace 和 counter 表达。

### Fingerprint 与证书策略

- **D-10:** 第 4 阶段开始，本地证书由 backend 生成或持有，核心通过 backend 查询本地 DTLS fingerprint。`createOffer` 和 `createAnswer` 应使用 backend fingerprint 生成 SDP，而不是继续信任 create-time 临时 fingerprint 字符串。
- **D-11:** 首版 fingerprint 算法只支持 `sha-256`。其他算法属于后续兼容扩展。
- **D-12:** 远端 SDP fingerprint 与 DTLS peer certificate 不一致时是安全硬失败。核心必须使 DTLS 失败，不得继续 key export 或初始化 SRTP/SRTCP。
- **D-13:** Fingerprint mismatch 应输出稳定错误和 trace，reason 至少能表达 `fingerprint_mismatch`；observer error 的 subsystem 应为 `dtls`、`security` 或 planner 选择的一致安全子系统名称。
- **D-14:** 如果 security backend 无法在 create/offer/answer 所需时间提供本地 SHA-256 fingerprint，应返回稳定配置错误或 backend 错误，并通过 observer/trace 暴露原因。

### SRTP key export 与 protect API

- **D-15:** DTLS handshake 完成后，核心自动触发 SRTP keying material export，并初始化 SRTP/SRTCP 上下文。用户不需要显式调用 export 或 init。
- **D-16:** Key export 或 SRTP/SRTCP 初始化失败后，安全状态进入失败；不得进入 `srtp.ready`，也不得让第 5 阶段媒体路径发送未保护 RTP/RTCP。
- **D-17:** 第 4 阶段只建立内部 SRTP/SRTCP protect/unprotect 子系统接口，供第 5 阶段 RTP/RTCP 媒体平面调用；不新增公共 `protect_rtp` / `unprotect_rtp` 用户 API。
- **D-18:** 首版固定为 1 个 BUNDLE transport context，覆盖该 PeerConnection 的 RTP/RTCP audio/video。不要按 m-line 动态创建多套 SRTP context，也不要拆成 audio/video 各自 transport context。
- **D-19:** SRTP protect 失败时不得输出未保护 RTP/RTCP datagram；SRTP/SRTCP unprotect 失败时不得向媒体层或用户输出原始包。失败必须记录 observer error、trace reason 和 counter。

### 错误、trace 与测试后端

- **D-20:** 公共 `rtc_status_t` 保持粗粒度稳定：安全后端失败主要映射为 `RTC_STATUS_BACKEND_ERROR`，协议/安全校验失败映射为 `RTC_STATUS_PROTOCOL_ERROR`，状态顺序错误映射为 `RTC_STATUS_INVALID_STATE`。不要为每种 backend 失败扩展 public status 枚举。
- **D-21:** 细粒度原因通过 observer detail code、trace reason 和 counter 表达，例如 `handshake_failed`、`fingerprint_mismatch`、`key_export_failed`、`srtp_protect_failed`、`srtp_unprotect_failed`、`srtp_replay_failed`。
- **D-22:** Observer 状态保持阶段级，例如 `dtls.connecting`、`dtls.connected`、`dtls.failed`、`srtp.ready`。证书、role、key export、protect/unprotect 等细节进入 trace/counter，不把 observer vtable 做成安全协议细节接口。
- **D-23:** Deterministic 测试后端必须覆盖：成功 handshake + key export + `srtp.ready`、fingerprint mismatch、handshake backend error、key export failure、SRTP protect failure、SRTP/SRTCP unprotect failure。
- **D-24:** 第 4 阶段不要求 Chrome 真实 DTLS 握手验收。Chrome 端到端验收留到第 6 阶段；第 4 阶段用 deterministic backend 测试保证核心契约可重复验证。

### the agent's Discretion

用户未锁定具体 vtable 函数名、backend 结构体字段名、internal `src/dtls` / `src/srtp` 文件拆分、trace event 名称全集、detail code 枚举值、测试 fixture 命名或真实库适配对象。planner 可以在不突破上述决策的前提下决定这些实现细节，但必须保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发，并避免 GPL/LGPL 依赖。

### Deferred Ideas (OUT OF SCOPE)

- 真实 Chrome DTLS 握手验收：留到第 6 阶段端到端验收。
- 多 fingerprint 算法，例如 SHA-384/SHA-512：首版第 4 阶段不支持，后续兼容扩展再评估。
- 多 transport / 多 m-line SRTP context：首版固定 Chrome 1v1 BUNDLE/rtcp-mux，不做动态扩展。
- 公共 `protect_rtp` / `unprotect_rtp` 用户 API：第 4 阶段不暴露；第 5 阶段如需要再围绕媒体 API 设计。
- 强制真实第三方 TLS/SRTP 库适配作为阶段验收：第 4 阶段只要求 deterministic 测试后端和可选适配骨架/文档。
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SEC-01 | 库通过 backend vtable 驱动 DTLS 握手，不在核心 API 中绑定特定 TLS 库。 | 单一 `security_backend` vtable 是已锁定决策；DTLS datagram 应从现有 demux DTLS 分支进入安全状态机。[VERIFIED: .planning/phases/04-dtls-srtp-secure-transport/04-CONTEXT.md][VERIFIED: src/api/peer_connection.c] |
| SEC-02 | 库校验远端 SDP fingerprint 与 DTLS 证书一致。 | RFC 8842/8122 定义 SDP `fingerprint` 用于 DTLS 关联；本阶段只支持 `sha-256` 且 mismatch 必须硬失败。[CITED: https://www.rfc-editor.org/rfc/rfc8842][CITED: https://www.rfc-editor.org/rfc/rfc8122.html][VERIFIED: 04-CONTEXT.md] |
| SEC-03 | 库从 DTLS 握手导出 SRTP keying material。 | RFC 5764 和 OpenSSL 文档均指定 DTLS-SRTP exporter label `EXTRACTOR-dtls_srtp` 及 key/salt 顺序。[CITED: https://www.rfc-editor.org/rfc/rfc5764.html][CITED: https://docs.openssl.org/3.3/man3/SSL_CTX_set_tlsext_use_srtp/] |
| SEC-04 | 库通过 backend vtable 对 RTP/RTCP 执行 SRTP/SRTCP protect 和 unprotect。 | 第 4 阶段只提供内部 wrapper；WebRTC RTP/SAVPF 必须保护 RTP 和 RTCP，libSRTP 的公开模式也区分 RTP/RTCP protect/unprotect。[CITED: https://www.ietf.org/rfc/rfc8834.html][CITED: https://github.com/cisco/libsrtp][VERIFIED: 04-CONTEXT.md] |
| SEC-05 | 安全后端错误被映射为稳定 `rtc_status_t` 和 observer 错误事件。 | 既有 `RTC_STATUS_BACKEND_ERROR`、`RTC_STATUS_PROTOCOL_ERROR`、observer error、trace/counter 模式已存在；本阶段应扩展原因枚举和 counters，而不是扩展大量 public status。[VERIFIED: include/rtc/status.h][VERIFIED: include/rtc/observer.h][VERIFIED: include/rtc/counters.h][VERIFIED: 04-CONTEXT.md] |
</phase_requirements>

## Summary

第 4 阶段应规划为“核心安全编排 + deterministic backend 契约”阶段，而不是“接入真实 OpenSSL/libsrtp 并完成 Chrome E2E”阶段。[VERIFIED: 04-CONTEXT.md] 现有代码已经具备 network executor 的 `receive_datagram`、RFC 7983 风格 STUN/DTLS/RTP/RTCP demux、`observer.on_datagram` 输出、粗粒度 status、trace/counter 和固定 arena 切分模式，可直接接入安全状态机。[VERIFIED: src/api/peer_connection.c][VERIFIED: src/net/demux.c][VERIFIED: include/rtc/observer.h]

规划重点应放在四条链路：create-time backend/storage 分配、SDP 本地 fingerprint 来源从临时字符串切到 backend、ICE connected 后自动启动 DTLS、DTLS 完成后做 fingerprint 校验与 SRTP key export，再暴露内部 SRTP/SRTCP protect/unprotect wrapper。[VERIFIED: 04-CONTEXT.md][VERIFIED: docs/API-执行器与内存契约.md] 所有失败必须在不发送明文 RTP/RTCP、不输出未认证数据的前提下，通过稳定 status、observer error、trace reason 和 counter 解释。[VERIFIED: 04-CONTEXT.md][CITED: https://www.ietf.org/rfc/rfc8834.html]

**Primary recommendation:** 使用单一 `rtc_security_backend_vtable_t` + 内部 `src/security` 编排层 + deterministic backend 测试夹具；真实 OpenSSL/libsrtp 适配只作为可选骨架/文档，不作为本阶段 gate。[VERIFIED: 04-CONTEXT.md][VERIFIED: ctest]

## Project Constraints (from AGENTS.md)

- 所有 Markdown 文档尽可能使用中文；技术标识符、API 名称、协议名和需求 ID 可保留英文。[VERIFIED: AGENTS.md]
- 优先阅读 `.planning/PROJECT.md`、`.planning/REQUIREMENTS.md`、`.planning/ROADMAP.md`、`.planning/STATE.md` 和当前阶段文档。[VERIFIED: AGENTS.md]
- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发的项目边界。[VERIFIED: AGENTS.md]
- 不要引入 GPL/LGPL 依赖。[VERIFIED: AGENTS.md]
- 修改规划文档时同步维护需求追踪和项目状态。[VERIFIED: AGENTS.md]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|--------------|----------------|-----------|
| DTLS handshake 编排 | API / Backend | Network executor | `receive_datagram` 在 network executor 输入 DTLS；核心自动启动和推进状态，用户不直接调用 start。[VERIFIED: docs/API-执行器与内存契约.md][VERIFIED: 04-CONTEXT.md] |
| SDP fingerprint 写入 | API / Backend | SDP/JSEP | `createOffer/createAnswer` 当前走 SDP writer；本阶段必须改为查询 backend local fingerprint。[VERIFIED: src/sdp/sdp_writer.c][VERIFIED: 04-CONTEXT.md] |
| Fingerprint 校验 | API / Backend | Security backend | 后端提供 peer cert fingerprint，核心比对远端 SDP fingerprint 并决定安全状态。[VERIFIED: 04-CONTEXT.md][CITED: https://www.rfc-editor.org/rfc/rfc8122.html] |
| SRTP key export | Security backend | API / Backend | RFC 5764 定义 DTLS exporter 材料；核心在 handshake 完成后自动触发并初始化 SRTP。[CITED: https://www.rfc-editor.org/rfc/rfc5764.html][VERIFIED: 04-CONTEXT.md] |
| SRTP/SRTCP protect/unprotect | Security backend | Media plane | 第 4 阶段只提供内部接口，第 5 阶段 RTP/RTCP 调用；失败不得输出未保护或未认证数据。[VERIFIED: 04-CONTEXT.md] |
| UDP 发送 | Browser / Client integration | Observer callback | 库只通过 `observer.on_datagram` 输出 datagram，用户负责 socket/UDP 发送。[VERIFIED: include/rtc/observer.h][VERIFIED: docs/API-执行器与内存契约.md] |

## Standard Stack

### Core

| Library / 标准 | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| 项目内 `security_backend` vtable | Phase 4 新增 | 单一入口覆盖 DTLS、fingerprint、key export、SRTP/SRTCP protect/unprotect。 | 已由用户决策锁定，保持核心 API 不绑定 TLS/SRTP 实现。[VERIFIED: 04-CONTEXT.md] |
| Deterministic test backend | Phase 4 新增 | 可重复驱动成功、mismatch、handshake error、key export error、protect/unprotect error。 | 本阶段验收核心，不依赖 Chrome 或真实 TLS 库。[VERIFIED: 04-CONTEXT.md] |
| RFC 8842 + RFC 8122 | 8842 / 8122 | SDP `setup`/`fingerprint` 与 DTLS offer/answer 规则。 | RFC 8842 更新 RFC 5763 的 DTLS offer/answer 过程；RFC 8122 注册 `sha-256` fingerprint 算法。[CITED: https://www.rfc-editor.org/rfc/rfc8842][CITED: https://www.rfc-editor.org/rfc/rfc8122.html] |
| RFC 5764 + RFC 3711 | 5764 / 3711 | DTLS-SRTP exporter、SRTP/SRTCP crypto context、replay/integrity 概念。 | WebRTC media security 依赖 DTLS-SRTP keying 与 SRTP/SRTCP 保护。[CITED: https://www.rfc-editor.org/rfc/rfc5764.html][CITED: https://www.rfc-editor.org/rfc/rfc3711] |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| OpenSSL | 本机 CLI `3.0.2`；开发包未由 `pkg-config` 验证 | 可选真实 DTLS-SRTP 参考适配。 | 仅作为后续/可选骨架；OpenSSL 文档支持 `use_srtp` 扩展和 `SSL_export_keying_material`。[VERIFIED: openssl version][CITED: https://docs.openssl.org/3.3/man3/SSL_CTX_set_tlsext_use_srtp/] |
| libSRTP | 本机未发现 `pkg-config` 包 | 可选真实 SRTP/SRTCP protect/unprotect 参考适配。 | 不作为 Phase 4 成功标准；若后续使用，必须处理输出 buffer 预留认证 tag 的坑。[VERIFIED: pkg-config][CITED: https://github.com/cisco/libsrtp] |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| 单一 `security_backend` | 拆成 DTLS/SRTP/crypto 多 vtable | 用户已锁定单一 vtable；拆分会扩大公共 API 面并破坏下游决策。[VERIFIED: 04-CONTEXT.md] |
| Deterministic backend gate | 真实 Chrome DTLS gate | Chrome E2E 已明确延后到第 6 阶段；真实网络握手会让 Phase 4 测试不可重复且扩大范围。[VERIFIED: 04-CONTEXT.md] |
| 内部 protect wrapper | 公共 `protect_rtp` API | 用户已锁定第 4 阶段不新增公共 protect/unprotect API；第 5 阶段媒体层再调用内部接口。[VERIFIED: 04-CONTEXT.md] |

**Installation:**
```bash
# Phase 4 核心不新增必需第三方依赖。
# 可选真实适配若开启，应先由 planner 增加显式 feature flag 和依赖探测。
```

**Version verification:** C 项目无 npm 包；本机验证为 `cc 11.4.0`、`cmake 3.22.1`、`ctest 3.22.1`、`OpenSSL 3.0.2` CLI；`pkg-config` 未发现 `openssl`/`libsrtp2` 开发包。[VERIFIED: command -v/version/pkg-config]

## Architecture Patterns

### System Architecture Diagram

```text
用户 UDP receive
    |
    v
rtc_peer_connection_receive_datagram (network executor)
    |
    v
RFC 7983 demux: STUN / DTLS / RTP / RTCP / unknown
    |
    +--> STUN -> existing ICE/STUN handler
    |
    +--> DTLS -> security transport state
              |
              +-- if ICE not connected/selected -> reject + trace/error
              |
              +-- backend.dtls_input()
                    |
                    +-- outgoing DTLS bytes -> observer.on_datagram
                    |
                    +-- handshake complete
                          |
                          +-- backend peer fingerprint -> compare remote SDP fingerprint
                          |      |
                          |      +-- mismatch -> dtls.failed + no key export
                          |
                          +-- backend.export_keying_material("EXTRACTOR-dtls_srtp")
                          |
                          +-- backend.srtp_init()
                          |
                          +-- state: srtp.ready

第 5 阶段内部媒体路径
    |
    +--> security_protect_rtp/rtcp -> backend.srtp_protect_* -> observer.on_datagram
    +--> backend.srtp_unprotect_* -> media parser only after success
```

### Recommended Project Structure

```text
include/rtc/
├── security.h          # public-ish backend vtable/config types, included by config.h
├── counters.h          # add dtls/srtp counters
└── trace.h             # add security trace events/fields
src/
├── security/           # orchestration: state, role, fingerprint, key export, wrappers
├── dtls/               # optional boundary helpers if planner separates DTLS concerns
├── srtp/               # internal protect/unprotect wrappers
└── api/                # thin integration in peer_connection.c only
tests/
├── test_security.c     # deterministic backend behavior matrix
└── test_sdp_writer.c   # backend fingerprint replaces create-time string
```

### Pattern 1: 单一 backend vtable + 核心编排

**What:** backend vtable 只做后端能力调用，核心负责状态顺序、ICE gating、fingerprint 比对、错误映射和 observer/trace/counter。[VERIFIED: 04-CONTEXT.md]

**When to use:** 所有 DTLS/SRTP 相关输入输出都走此模式，避免把 backend 错误或第三方 API 直接暴露到 public API。[VERIFIED: include/rtc/status.h][VERIFIED: include/rtc/observer.h]

**Example:**
```c
/* Source: RFC 5764 + project CONTEXT; illustrative shape for planner. */
typedef struct rtc_security_backend_vtable_t {
    rtc_status_t (*get_local_fingerprint)(void *ctx,
                                          char *out, size_t *inout_len);
    rtc_status_t (*start_dtls)(void *ctx, int role);
    rtc_status_t (*dtls_input)(void *ctx, const uint8_t *data, size_t len);
    rtc_status_t (*export_keying_material)(void *ctx,
                                           uint8_t *out, size_t out_len);
    rtc_status_t (*srtp_protect_rtp)(void *ctx, uint8_t *pkt,
                                     size_t *inout_len, size_t cap);
    rtc_status_t (*srtp_unprotect_rtp)(void *ctx, uint8_t *pkt,
                                       size_t *inout_len);
} rtc_security_backend_vtable_t;
```

### Pattern 2: ICE connected 后自动启动 DTLS

**What:** `rtc_ice` 设置 selected pair / `ice.connected` 后调用内部 `rtc_security_on_ice_connected(pc)`，该函数根据 SDP/JSEP setup 推导 DTLS client/server 并启动 backend。[VERIFIED: 04-CONTEXT.md][CITED: https://www.rfc-editor.org/rfc/rfc8842]

**When to use:** `rtc_peer_connection_start_connectivity_checks` 的成功路径或 ICE handler 收到 nominated success 后。[VERIFIED: docs/API-执行器与内存契约.md]

### Pattern 3: SDP writer 只接受 backend fingerprint 快照

**What:** `createOffer/createAnswer` 前从 backend 查询 `sha-256 XX:...` 字符串，写入临时/内部 `rtc_sdp_parameters_t`，不要继续信任 `config.sdp.dtls_fingerprint`。[VERIFIED: 04-CONTEXT.md][VERIFIED: src/sdp/sdp_writer.c]

**When to use:** 所有本地 offer/answer 生成路径。[VERIFIED: src/api/peer_connection.c]

### Anti-Patterns to Avoid

- **把 DTLS datagram 在 ICE connected 前缓存:** 用户已锁定不缓存早到 DTLS；应拒绝并输出 trace/error。[VERIFIED: 04-CONTEXT.md]
- **fingerprint mismatch 后继续 export key:** mismatch 是安全硬失败，不得初始化 SRTP。[VERIFIED: 04-CONTEXT.md]
- **protect 失败后发送明文 RTP/RTCP:** 失败必须阻断输出并计数/trace。[VERIFIED: 04-CONTEXT.md][CITED: https://www.ietf.org/rfc/rfc8834.html]
- **把真实 TLS 库内存行为藏在 backend 内:** 固定内存契约要求真实适配文档说明 storage/allocator 策略。[VERIFIED: 04-CONTEXT.md][VERIFIED: AGENTS.md]

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| 真实 DTLS 协议 | 自写 DTLS record/handshake/cert/crypto | backend vtable + 后续 OpenSSL/同类宽松许可适配 | DTLS 握手、证书和重传复杂且高风险；Phase 4 只测核心契约。[CITED: https://www.rfc-editor.org/rfc/rfc8842][VERIFIED: 04-CONTEXT.md] |
| 真实 SRTP 加密与 replay | 自写 AES/HMAC/replay window | backend vtable + 后续 libSRTP/同类宽松许可适配 | SRTP 需要加密、认证、replay 和 key derivation；libSRTP 已覆盖 protect/unprotect 和 replay，但需注意 buffer 容量。[CITED: https://www.rfc-editor.org/rfc/rfc3711][CITED: https://github.com/cisco/libsrtp] |
| 测试用伪随机握手 | 依赖 wall clock、真实证书、真实网络 | deterministic backend 脚本化状态 | Phase 4 要可重复覆盖所有错误语义。[VERIFIED: 04-CONTEXT.md] |

**Key insight:** 本阶段要 hand-roll 的是“固定内存状态编排和后端契约”，不能 hand-roll 密码协议本身。[VERIFIED: 04-CONTEXT.md][CITED: https://www.rfc-editor.org/rfc/rfc3711]

## Common Pitfalls

### Pitfall 1: SDP setup 到 DTLS role 映射反了

**What goes wrong:** answer 中 `a=setup:active` 表示 answerer 发 ClientHello；offerer 收到该 answer 后作为 DTLS server 等待。[CITED: https://www.rfc-editor.org/rfc/rfc8842]  
**Why it happens:** `active/passive` 容易被误解成 offerer/answerer 固定角色。[CITED: https://www.rfc-editor.org/rfc/rfc8842]  
**How to avoid:** 用 table-driven helper 测 `local offer + remote active answer`、`remote offer actpass + local active answer` 等路径。[VERIFIED: 04-CONTEXT.md]  
**Warning signs:** 双方都等待或双方都发 ClientHello。[ASSUMED]

### Pitfall 2: Key material 顺序或长度错误

**What goes wrong:** RFC 5764 导出的材料按 client write key、server write key、client write salt、server write salt 排列；方向搞错会导致 SRTP 解保护失败。[CITED: https://www.rfc-editor.org/rfc/rfc5764.html][CITED: https://docs.openssl.org/3.3/man3/SSL_CTX_set_tlsext_use_srtp/]  
**How to avoid:** deterministic backend 固定 client/server role 和导出字节，测试本地/远端方向分配。[VERIFIED: 04-CONTEXT.md]

### Pitfall 3: libSRTP protect 输出 buffer 没有预留 tag 空间

**What goes wrong:** `srtp_protect()` 会把认证 tag 写到 packet 末尾；buffer 容量不足会导致内存破坏。[CITED: https://github.com/cisco/libsrtp]  
**How to avoid:** 内部 wrapper 签名必须传入 `capacity`，protect 前检查 `len + max_auth_tag <= capacity`。[CITED: https://github.com/cisco/libsrtp]

### Pitfall 4: public status 过度细分

**What goes wrong:** 为每个 backend 失败扩展 `rtc_status_t` 会污染稳定 API。[VERIFIED: 04-CONTEXT.md]  
**How to avoid:** status 保持 `BACKEND_ERROR` / `PROTOCOL_ERROR` / `INVALID_STATE`，细节进 detail code、trace reason 和 counters。[VERIFIED: include/rtc/status.h][VERIFIED: 04-CONTEXT.md]

## Code Examples

### DTLS 输入 gate

```c
/* Source: 04-CONTEXT D-08/D-20/D-21, existing receive_datagram pattern. */
if (protocol == RTC_NET_PROTOCOL_DTLS) {
    if (pc->ice_state != RTC_ICE_CONNECTED) {
        rtc_security_fail(pc, RTC_STATUS_INVALID_STATE,
                          "dtls", "receive_datagram", "ice_not_connected");
        return RTC_STATUS_INVALID_STATE;
    }
    return rtc_security_handle_dtls_datagram(pc, data, data_len);
}
```

### Fingerprint 校验顺序

```c
/* Source: RFC 8122 / 04-CONTEXT D-12. */
status = backend->get_peer_fingerprint(ctx, fp, &fp_len);
if (status != RTC_STATUS_OK) {
    return rtc_security_backend_error(pc, "get_peer_fingerprint");
}
if (!rtc_security_fingerprint_equals(pc->remote_summary.dtls_fingerprint, fp)) {
    return rtc_security_protocol_error(pc, "fingerprint_mismatch");
}
```

### Key export label

```c
/* Source: RFC 5764 and OpenSSL SSL_CTX_set_tlsext_use_srtp docs. */
static const char k_dtls_srtp_label[] = "EXTRACTOR-dtls_srtp";
status = backend->export_keying_material(ctx, k_dtls_srtp_label,
                                         sizeof(k_dtls_srtp_label) - 1u,
                                         key_block, key_block_len);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| RFC 5763 自带通用 DTLS-SRTP offer/answer 过程 | RFC 8842 统一并更新 DTLS SDP offer/answer 过程 | RFC 8842, January 2021 | role、ICE interaction 和新 DTLS association 规划应引用 RFC 8842。[CITED: https://www.rfc-editor.org/rfc/rfc8842] |
| 只按 RFC 5764 老 demux 表理解 STUN/DTLS/RTP | RFC 7983 更新单端口多协议 demux | RFC 7983, September 2016 | 现有 `src/net/demux.c` 的 DTLS 首字节 20..63 路由符合该方向。[CITED: https://www.rfc-editor.org/rfc/rfc7983][VERIFIED: src/net/demux.c] |
| Phase 2 create-time fingerprint 字符串 | Phase 4 backend certificate fingerprint | Phase 4 决策 | SDP writer 需要改源头，否则无法保证证书与 SDP 一致。[VERIFIED: src/sdp/sdp_writer.c][VERIFIED: 04-CONTEXT.md] |

**Deprecated/outdated:**
- 第 2 阶段 `config.sdp.dtls_fingerprint` 作为本地 fingerprint 来源在 Phase 4 后过时；可保留为测试兼容输入或移除，但本地 SDP 必须来自 backend。[VERIFIED: 04-CONTEXT.md][VERIFIED: include/rtc/config.h]

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | 双方都等待或双方都发 ClientHello 是 role 映射错误的典型 warning sign。 | Common Pitfalls | 需要用 deterministic backend 测试确认具体 failure trace。 |

## Open Questions

1. **是否保留 `rtc_sdp_parameters_t.dtls_fingerprint` 字段？**  
   - What we know: CONTEXT 锁定本地 fingerprint 必须来自 backend。[VERIFIED: 04-CONTEXT.md]  
   - What's unclear: public struct 兼容性是否要求字段暂时保留。[ASSUMED]  
   - Recommendation: planner 保留字段但标记 Phase 4 后不作为本地 SDP 来源，避免破坏测试/示例；后续 API 清理再移除。[ASSUMED]

2. **真实参考 backend 到什么程度？**  
   - What we know: deterministic backend 是成功标准，真实库只要求骨架/文档/可选示例。[VERIFIED: 04-CONTEXT.md]  
   - What's unclear: 是否要在本阶段加入 CMake option stub。[ASSUMED]  
   - Recommendation: 只加 `RTC_ENABLE_OPENSSL_SRTP_BACKEND` 之类可选开关的设计文档或空适配目录，不让默认 build 依赖外部库。[VERIFIED: pkg-config]

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| C compiler | build/tests | ✓ | `cc 11.4.0` | — |
| CMake | build/tests | ✓ | `3.22.1` | — |
| CTest | validation | ✓ | `3.22.1` | — |
| OpenSSL CLI | optional reference docs/smoke | ✓ | `3.0.2` | deterministic backend |
| OpenSSL dev pkg-config | optional real backend compile | ✗ | — | do not compile real backend in Phase 4 |
| libSRTP pkg-config | optional real SRTP backend compile | ✗ | — | deterministic backend + wrapper tests |

**Missing dependencies with no fallback:**  
- 无；Phase 4 核心不应依赖真实 OpenSSL/libsrtp 构建通过。[VERIFIED: 04-CONTEXT.md][VERIFIED: pkg-config]

**Missing dependencies with fallback:**  
- OpenSSL/libsrtp 开发包缺失；使用 deterministic backend 完成本阶段，真实适配留可选骨架/文档。[VERIFIED: pkg-config][VERIFIED: 04-CONTEXT.md]

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | 纯 C 自研 test runner + CTest。[VERIFIED: tests/test_runner.h][VERIFIED: ctest] |
| Config file | `CMakeLists.txt`。[VERIFIED: CMakeLists.txt] |
| Quick run command | `cmake --build build && ctest --test-dir build --output-on-failure` |
| Full suite command | `cmake --build build && ctest --test-dir build --output-on-failure` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|--------------|
| SEC-01 | DTLS handshake 由 backend vtable 驱动，ICE connected 后自动启动。 | unit/integration | `ctest --test-dir build --output-on-failure` | ❌ Wave 0: `tests/test_security.c` |
| SEC-02 | fingerprint mismatch 进入 `dtls.failed`，不 export key。 | unit | `ctest --test-dir build --output-on-failure` | ❌ Wave 0 |
| SEC-03 | handshake complete 后 export key material 并初始化 SRTP。 | unit | `ctest --test-dir build --output-on-failure` | ❌ Wave 0 |
| SEC-04 | internal SRTP/SRTCP protect/unprotect 成功与失败路径。 | unit | `ctest --test-dir build --output-on-failure` | ❌ Wave 0 |
| SEC-05 | backend errors 映射 status、observer error、trace reason、counter。 | unit | `ctest --test-dir build --output-on-failure` | ❌ Wave 0 |

### Sampling Rate

- **Per task commit:** `cmake --build build && ctest --test-dir build --output-on-failure`。[VERIFIED: ctest]
- **Per wave merge:** 同上；当前 suite 运行约 0.01s，适合作为每次变更 gate。[VERIFIED: ctest]
- **Phase gate:** 全 suite 绿色，并用 `rg -n "security_backend|dtls.connected|srtp.ready|fingerprint_mismatch|EXTRACTOR-dtls_srtp" include src tests docs` 做契约 grep。[ASSUMED]

### Wave 0 Gaps

- [ ] `tests/test_security.c` — 覆盖 SEC-01 到 SEC-05 deterministic backend matrix。[VERIFIED: rg --files tests]
- [ ] `include/rtc/security.h` — 定义 backend vtable/config 类型。[VERIFIED: rg --files include]
- [ ] `src/security/` — 安全状态机、role helper、fingerprint/key export/protect wrapper。[VERIFIED: rg --files src]
- [ ] `include/rtc/counters.h` / `include/rtc/trace.h` — 增加 DTLS/SRTP counters 和 trace events。[VERIFIED: include/rtc/counters.h][VERIFIED: include/rtc/trace.h]

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|------------------|
| V2 Authentication | yes | DTLS certificate fingerprint 必须与 SDP fingerprint 匹配；不匹配硬失败。[CITED: https://www.rfc-editor.org/rfc/rfc8122.html][VERIFIED: 04-CONTEXT.md] |
| V3 Session Management | yes | 单个 BUNDLE transport 对应一个安全状态机；DTLS failure 不回滚 ICE，但阻断 SRTP ready。[VERIFIED: 04-CONTEXT.md] |
| V4 Access Control | no | 本阶段没有用户/权限模型。[VERIFIED: .planning/REQUIREMENTS.md] |
| V5 Input Validation | yes | datagram demux、DTLS early packet gate、fingerprint 格式和 backend 返回长度必须校验。[VERIFIED: src/net/demux.c][VERIFIED: 04-CONTEXT.md] |
| V6 Cryptography | yes | 不 hand-roll 真实 DTLS/SRTP crypto；通过 backend 接入标准实现或 deterministic test backend。[VERIFIED: 04-CONTEXT.md][CITED: https://www.rfc-editor.org/rfc/rfc3711] |

### Known Threat Patterns for DTLS-SRTP

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| SDP fingerprint mismatch / MITM | Spoofing | 比对 remote SDP fingerprint 与 DTLS peer cert fingerprint；失败不 export key。[CITED: https://www.rfc-editor.org/rfc/rfc8122.html][VERIFIED: 04-CONTEXT.md] |
| 未保护 RTP/RTCP 泄漏 | Information Disclosure | `srtp.ready` 前媒体发送必须阻断；protect 失败不得调用 `on_datagram`。[VERIFIED: 04-CONTEXT.md][CITED: https://www.ietf.org/rfc/rfc8834.html] |
| SRTP replay / auth failure | Tampering / Replay | unprotect 失败不得向媒体层输出；记录 `srtp_replay_failed`/`srtp_unprotect_failed`。[VERIFIED: 04-CONTEXT.md][CITED: https://www.rfc-editor.org/rfc/rfc3711] |
| 早到 DTLS 绕过 ICE selected pair | Spoofing | ICE 未 connected/selected 时拒绝 DTLS 输入，不缓存。[VERIFIED: 04-CONTEXT.md][CITED: https://www.rfc-editor.org/rfc/rfc8842] |

## Sources

### Primary (HIGH confidence)

- `.planning/phases/04-dtls-srtp-secure-transport/04-CONTEXT.md` — 用户锁定决策 D-01 到 D-24、自由裁量和延后事项。[VERIFIED]
- `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`, `.planning/STATE.md` — 项目范围、SEC-01..SEC-05、路线图和当前状态。[VERIFIED]
- `docs/000-设计边界记录.md`, `docs/API-执行器与内存契约.md` — 纯 C/固定内存/无线程/UDP 边界、executor/datagram 契约。[VERIFIED]
- `include/rtc/*.h`, `src/api/peer_connection.c`, `src/net/demux.c`, `src/sdp/*` — 现有 API、demux、SDP fingerprint 接入点。[VERIFIED]
- RFC 8842 — DTLS SDP offer/answer 和 ICE interaction。[CITED: https://www.rfc-editor.org/rfc/rfc8842]
- RFC 5764 — DTLS-SRTP exporter 和 keying material。[CITED: https://www.rfc-editor.org/rfc/rfc5764.html]
- RFC 8122 — SDP fingerprint attribute 和 `sha-256` 注册。[CITED: https://www.rfc-editor.org/rfc/rfc8122.html]
- RFC 3711 — SRTP/SRTCP crypto context、replay、key derivation。[CITED: https://www.rfc-editor.org/rfc/rfc3711]
- RFC 8834 / RFC 8835 — WebRTC 使用 SRTP/SRTCP 与 DTLS-SRTP key exchange。[CITED: https://www.ietf.org/rfc/rfc8834.html][CITED: https://www.ietf.org/rfc/rfc8835.html]

### Secondary (MEDIUM confidence)

- OpenSSL docs `SSL_CTX_set_tlsext_use_srtp` — 可选真实 DTLS-SRTP 适配 API、export label 和 key block 顺序。[CITED: https://docs.openssl.org/3.3/man3/SSL_CTX_set_tlsext_use_srtp/]
- Cisco libSRTP README — 可选 SRTP/SRTCP API 模式、buffer capacity pitfall、replay support note。[CITED: https://github.com/cisco/libsrtp]

### Tertiary (LOW confidence)

- 无；未使用未验证社区资料作为规划依据。

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — 核心栈来自用户锁定决策和现有代码，外部真实库明确降级为可选。[VERIFIED: 04-CONTEXT.md][VERIFIED: pkg-config]
- Architecture: HIGH — 入口、executor、observer、demux、SDP writer 都已在代码中验证。[VERIFIED: src/api/peer_connection.c][VERIFIED: src/net/demux.c][VERIFIED: src/sdp/sdp_writer.c]
- Pitfalls: MEDIUM — 关键协议坑来自 RFC/OpenSSL/libSRTP 文档；role warning sign 有一条为工程推断并已列入 assumptions。[CITED: RFC/OpenSSL/libSRTP][ASSUMED]

**Research date:** 2026-05-10  
**Valid until:** 2026-06-09
