# Phase 4: 传输、安全与 DataChannel 协议核心 - Context

**Gathered:** 2026-05-12T22:59:16+08:00
**Status:** Ready for planning

<domain>
## Phase Boundary

本阶段把 Phase 3 的 signaling-free PeerConnection API 接入真实传输、安全和 DataChannel 协议核心。交付重点是：ICE/STUN/TURN 模块进入 `micrortc` 核心库并被 PeerConnection 使用；DTLS OpenSSL 路径完成握手并导出 SRTP keying material；SRTP session 可以创建；SCTP/DataChannel 支持创建、打开、关闭、文本和二进制消息收发；host、STUN srflx、TURN relay 路径都有基于真实配置的可运行验证。

本阶段不实现 H264/Opus 媒体 RTP/RTCP 收发，不要求真实媒体包走 SRTP protect/unprotect，不引入应用层 signaling 服务，不提前做 Phase 6 的完整 Chrome 自动化 E2E 和双向媒体验收。

</domain>

<decisions>
## Implementation Decisions

### ICE/STUN/TURN 候选与验证

- **D-01:** Phase 4 采用“本地/受控网络验证优先”边界：需要有可运行的小型 harness 或测试流程验证真实连接路径，而不是只做协议单元测试。
- **D-02:** Phase 4 接受真实 STUN/TURN server 作为验收依赖；不要求实现本地 mock STUN/TURN server。
- **D-03:** ICE server 配置通过项目根目录的本地配置文件读取，测试直接从该文件加载 `urls`、`username`、`credential` 等字段。
- **D-04:** 真实 STUN/TURN 验证是硬前置：根目录配置文件缺失或不可读时，对应 Phase 4 真实网络验证必须失败，而不是静默跳过。
- **D-05:** 规划和提交文档不得写入真实 TURN credential。仓库中可以提供不含秘密的示例配置或模板；真实配置应保持本地化并避免提交。
- **D-06:** host candidate、STUN server-reflexive candidate、TURN relay candidate 都要进入 Phase 4 可运行验证范围。Phase 6 可以继续扩展为完整浏览器自动化验收，但不能替代 Phase 4 的传输验证。

### DTLS/SRTP 安全通道

- **D-07:** Phase 4 不止迁入 DTLS/SRTP 模块；必须完成 DTLS OpenSSL 路径握手，并导出 SRTP keying material。
- **D-08:** DTLS 角色必须从 SDP `setup` 属性推导，按 WebRTC SDP 语义处理 `actpass`、`active`、`passive`，不通过测试配置硬指定角色。
- **D-09:** PeerConnection/SDP 流程需要生成自签名证书并写入 SDP `a=fingerprint`，DTLS 握手时校验远端 fingerprint。
- **D-10:** Phase 4 需要用导出的 keying material 创建 SRTP session，证明安全媒体通道初始化路径成立。
- **D-11:** 真实 RTP 包的 SRTP protect/unprotect 和 H264/Opus 媒体流动属于 Phase 5；Phase 4 不扩大到完整媒体路径。

### DataChannel API 与 SCTP 路径

- **D-12:** DataChannel 公共 API 采用 AWS/WebRTC 薄裁剪风格，延续 Phase 3 的 `mrtc_*`、opaque handle、callback table 方向。
- **D-13:** Public API 需要提供 DataChannel handle，以及类似 `mrtc_peer_connection_create_data_channel()`、send、close、open/message/close 回调的能力。
- **D-14:** Phase 4 先支持 `negotiated=false` 的常规 DCEP 打开流程，与浏览器默认 `createDataChannel()` 行为对齐；`negotiated=true` 静态 channel 可留作后续扩展，除非 planner 证明低成本可同时支持。
- **D-15:** DataChannel 消息需要支持文本和二进制。测试至少覆盖文本 ping/pong，并包含一个基础 binary buffer 用例。
- **D-16:** DataChannel 验证必须跑在真实 ICE/DTLS/SCTP 路径上，证明 open、send、receive、close 不只是 API 或 loopback 单元测试可用。

### the agent's Discretion

Planner 可以决定配置文件名称、JSON/schema 细节、DataChannel 函数命名细节、callback 字段命名、内部模块拆分、测试 harness 组织、是否拆分默认 CTest 与真实网络验证命令。但不得改变以下约束：真实 STUN/TURN 配置为 Phase 4 验收硬前置；真实 credential 不写入提交文档；DTLS 角色从 SDP 推导；库内证书/fingerprint 路径必须存在；DataChannel 要有 public handle/API，并在真实 ICE/DTLS/SCTP 路径上验证。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目规划

- `.planning/PROJECT.md` — 项目定位、v1 范围、核心库不内置信aling、不做媒体采集编码、TURN relay 和 Chrome 优先约束。
- `.planning/REQUIREMENTS.md` — Phase 4 对应 `API-05`、`PROTO-01`、`PROTO-02`、`PROTO-03`、`PROTO-05`、`NET-01`、`NET-02`、`NET-03`。
- `.planning/ROADMAP.md` — Phase 4 目标和成功标准。
- `.planning/STATE.md` — 当前阶段、已完成阶段和需要继承的项目级决策。

### 前置阶段决策

- `.planning/phases/01-/01-CONTEXT.md` — 本地 KVS 基线、核心/非核心边界、signaling excluded 决策。
- `.planning/phases/01-/BASELINE.md` — 本地 `reflib/kvs-webrtc-sdk` 基线、模块分类和依赖观察。
- `.planning/phases/01-/SOURCE-MANIFEST.md` — AWS 派生文件来源追溯规则；Phase 4 搬迁或重写派生协议模块必须更新。
- `.planning/phases/01-/COMPLIANCE.md` — Apache-2.0、NOTICE、第三方依赖和 excluded 组件策略。
- `.planning/phases/02-/02-CONTEXT.md` — `micrortc` target、CMake/install/export、public include 和最小库边界。
- `.planning/phases/03-aws-api-signaling-free-peerconnection/03-CONTEXT.md` — Phase 3 API、SDP、ICE candidate 字符串边界、Answerer 先行和 signaling-free 约束。
- `.planning/phases/03-aws-api-signaling-free-peerconnection/03-01-SUMMARY.md` — Phase 3 实际交付的 public PeerConnection API、SDP helper、测试和 manifest 更新。
- `.planning/phases/03-aws-api-signaling-free-peerconnection/03-PATTERNS.md` — 当前 public API、CMake、CTest 和来源追溯模式。

### 当前代码与本地参考库

- `include/micrortc/micrortc.h` — 当前 umbrella public header 和 `MRTC_STATUS` 状态码集合。
- `include/micrortc/peer_connection.h` — Phase 3 PeerConnection public API；Phase 4 需要扩展 ICE server config、DataChannel API 和连接状态相关能力。
- `src/peer_connection.c` — 当前 opaque PeerConnection、Answerer 状态机、SDP/candidate 字符串存储实现。
- `src/sdp.c` — 当前最小 SDP parse/serialize/answer helper；Phase 4 需要扩展 `setup`、fingerprint、ICE attributes 等 SDP 语义。
- `CMakeLists.txt` — 当前 `micrortc` 静态库 target、CTest 测试入口和 install/export 路径。
- `tests/peer_connection/test_peer_connection_api.c` — 可扩展用于 PeerConnection、DataChannel public API 行为验证。
- `reflib/kvs-webrtc-sdk` — 唯一本地剥离基线；不要默认使用 GitHub upstream latest。
- `reflib/kvs-webrtc-sdk/src/source/Ice/` — ICE agent、candidate、socket、TURN connection 和网络路径参考。
- `reflib/kvs-webrtc-sdk/src/source/Stun/` — STUN message/parser/transaction 参考。
- `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls_openssl.c` 和 `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls.h` — DTLS OpenSSL 路径和角色/握手参考。
- `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.c` 和 `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.h` — SRTP session 创建和 keying material 使用参考。
- `reflib/kvs-webrtc-sdk/src/source/Sctp/` — SCTP/DataChannel 关联、DCEP 和消息收发参考。
- `reflib/kvs-webrtc-sdk/src/source/PeerConnection/` — AWS PeerConnection 将 ICE、DTLS、SRTP、SCTP/DataChannel 组合起来的参考。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `include/micrortc/peer_connection.h`：已建立 opaque PeerConnection handle、config、callbacks、SDP offer/answer、ICE candidate 字符串 API。Phase 4 应在此基础上扩展 ICE server 配置和 DataChannel API。
- `src/peer_connection.c`：已存储 config、callbacks、user_data、remote/local SDP 和 remote candidate；Phase 4 可替换或扩展其内部状态机，把真实 ICE/DTLS/SCTP 对象挂入 private struct。
- `src/sdp.c`：已有最小 SDP parse/answer helper，可作为扩展点加入 ICE ufrag/pwd、candidate、fingerprint、setup、sctp/datachannel m-line 等解析/生成能力。
- `CMakeLists.txt`：已有 `micrortc` 静态库 target 和 CTest 布局；协议模块、依赖链接和真实网络验证目标应接入同一工程边界。
- `reflib/kvs-webrtc-sdk/src/source/Ice`、`Stun`、`Crypto`、`Srtp`、`Sctp`：本阶段主要协议来源参考，必须以本地目录为准。

### Established Patterns

- Public API 使用 `MRTC_STATUS`、`mrtc_*` 命名和 opaque handle，不暴露 AWS/PIC 基础类型。
- 核心库 target 不引入 AWS/KVS signaling、credential/storage、libwebsockets 或 AWS SDK C++。
- 测试使用 CTest 和小型 C 可执行文件，不引入大型测试框架。
- 从 AWS 参考库搬迁、重写派生或 reference-only 使用都要更新 Phase 1 `SOURCE-MANIFEST.md`。

### Integration Points

- `MRTC_PEER_CONNECTION_CONFIG` 需要扩展以支持 ICE server 配置文件读入后的配置表达；planner 可决定是直接放结构数组，还是提供轻量 loader/helper，但真实配置来自项目根目录文件。
- PeerConnection SDP answer 生成需要加入 DTLS fingerprint、ICE credentials 和 DataChannel/SCTP 相关 SDP 属性。
- ICE selected pair 应触发 DTLS 握手；DTLS key export 应创建 SRTP session；SCTP/DataChannel 应在 DTLS 保护通道之上完成 open/send/receive/close。
- 测试应覆盖无配置文件失败、host/STUN/TURN 真实路径、DTLS/SRTP 初始化、DataChannel 文本和二进制消息。

</code_context>

<specifics>
## Specific Ideas

- 项目根目录需要一个真实 STUN/TURN 配置文件；用户提供过包含 `urls`、`username`、`credential` 字段的 TURN 配置形态。规划文档不得保存真实 credential。
- 推荐配置示例采用 JSON 形态，例如：

```json
{
  "ice_servers": [
    {
      "urls": "turn:<host>:3478?transport=udp",
      "username": "<username>",
      "credential": "<credential>"
    }
  ]
}
```

- DataChannel API 推荐至少包含 create、send text/binary、close，以及 on_open/on_message/on_close 回调。函数名可由 planner 按现有 `mrtc_*` 风格微调。
- Phase 4 验收应明确区分协议单元测试、真实网络 ICE/STUN/TURN 验证、安全握手验证和 DataChannel 端到端收发验证。

</specifics>

<deferred>
## Deferred Ideas

- RTP/RTCP、H264/Opus、`writeFrame` 类媒体 API 和真实媒体 SRTP protect/unprotect 留给 Phase 5。
- 完整 Chrome 自动化 E2E、浏览器页面、自动 signaling 交换和双向媒体流动断言留给 Phase 6。
- `negotiated=true` 静态 DataChannel 可作为后续扩展，不是 Phase 4 的首要用户决策。
- 本地 mock STUN/TURN server 不纳入 Phase 4 用户决策范围；当前选择是真实服务器依赖。

</deferred>

---

*Phase: 4-传输、安全与 DataChannel 协议核心*
*Context gathered: 2026-05-12T22:59:16+08:00*
