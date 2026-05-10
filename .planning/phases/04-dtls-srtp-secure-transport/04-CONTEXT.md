# 第 4 阶段：DTLS-SRTP 安全传输 - 上下文

**Gathered:** 2026-05-10T20:20:50+08:00
**Status:** 准备进入规划

<domain>
## 阶段边界

本阶段交付 WebRTC 安全传输层：在第 3 阶段 ICE selected pair 和 datagram demux 已经可用的基础上，通过可插拔 `security_backend` vtable 驱动 DTLS handshake、远端 fingerprint 校验、SRTP key export，以及 SRTP/SRTCP protect/unprotect 的内部安全接口。

本阶段不把核心 API 绑定到特定 TLS/SRTP 第三方库，不创建 socket，不拥有线程或事件循环，不提前实现第 5 阶段的 RTP/RTCP 媒体 packetize/depacketize 公共 API，也不要求完成 Chrome 真实 DTLS 握手验收。第 4 阶段的验收重点是固定内存、backend 契约、安全状态机、错误映射、trace/counter 和 deterministic 测试后端。

</domain>

<decisions>
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

### 智能体的自由裁量

用户未锁定具体 vtable 函数名、backend 结构体字段名、internal `src/dtls` / `src/srtp` 文件拆分、trace event 名称全集、detail code 枚举值、测试 fixture 命名或真实库适配对象。planner 可以在不突破上述决策的前提下决定这些实现细节，但必须保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发，并避免 GPL/LGPL 依赖。

</decisions>

<canonical_refs>
## 权威引用

**下游智能体在规划或实现前必须阅读这些文档。**

### 项目范围与需求

- `.planning/PROJECT.md`：项目价值、首版边界、固定内存/无线程/用户 UDP/socket 边界、加密 backend 关键决策。
- `.planning/REQUIREMENTS.md`：第 4 阶段覆盖的 `SEC-01`、`SEC-02`、`SEC-03`、`SEC-04`、`SEC-05` 需求及需求追踪。
- `.planning/ROADMAP.md`：第 4 阶段目标、成功标准和第 5、6 阶段边界。
- `.planning/STATE.md`：当前项目状态和工作流配置。

### 已锁定的上游边界

- `.planning/phases/01-core-skeleton-boundary-contract/01-CONTEXT.md`：公共 API、固定 arena、executor 亲和、observer、trace、counter、测试策略和 allocator 防线。
- `.planning/phases/02-sdp-jsep-offer-answer/02-CONTEXT.md`：Chrome 1v1 SDP/JSEP、最小 offer/answer 状态机、DTLS fingerprint/setup 字段的 SDP 画像。
- `.planning/phases/03-ice-stun-datagram-network-layer/03-CONTEXT.md`：ICE selected pair、显式 network API、STUN/DTLS/RTP/RTCP datagram demux、`observer.on_datagram` 输出模型。
- `docs/000-设计边界记录.md`：纯 C、固定内存、无线程、用户负责 UDP/socket、DTLS/SRTP/crypto backend vtable、宽松许可依赖策略。
- `docs/API-执行器与内存契约.md`：executor 亲和、arena/limits、observer/trace/counter、datagram buffer 生命周期、第 3 阶段 demux 语义。

### 现有代码入口

- `include/rtc/config.h`：当前 `rtc_peer_connection_config_t.security_backend` 占位、`rtc_sdp_parameters_t.dtls_fingerprint` 临时 SDP 参数、arena/limits 配置入口。
- `include/rtc/limits.h`：已有 `limits.dtls.max_sessions`，需要扩展或解释 backend storage、SRTP context、packet/security 资源容量。
- `include/rtc/peer_connection.h`：现有 `receive_datagram`、offer/answer/description、gather/checks 和 counters API 形状。
- `include/rtc/observer.h`：已有 `on_state`、`on_error`、`on_trace`、`on_datagram`，第 4 阶段应复用这些出口。
- `include/rtc/status.h`：已有 `RTC_STATUS_BACKEND_ERROR`、`RTC_STATUS_PROTOCOL_ERROR`、`RTC_STATUS_INVALID_STATE` 等粗粒度公共错误。
- `include/rtc/counters.h`：已有 ICE/STUN/net/trace counters，需要扩展安全层 counters。
- `src/api/peer_connection.c`：`receive_datagram` 当前对 DTLS 只 demux/trace/counter 后返回，需接入安全状态机。
- `src/net/demux.c`：已按首字节分类 STUN、DTLS、RTP、RTCP，是第 4 阶段 DTLS 输入路由入口。
- `src/sdp/sdp_writer.c` 和 `src/sdp/sdp_parser.c`：已有 fingerprint/setup 写入与解析，第 4 阶段需把本地 fingerprint 来源切到 backend。

</canonical_refs>

<code_context>
## 既有代码洞察

### 可复用资产

- `rtc_peer_connection_receive_datagram` 已要求 `RTC_EXECUTOR_NETWORK` 亲和，并完成 STUN/DTLS/RTP/RTCP demux；第 4 阶段应把 DTLS 分支从占位路由替换为安全 backend 输入。
- `observer.on_datagram` 已用于 STUN request 输出，buffer 回调后失效；DTLS outgoing datagram 应复用这一模型。
- `rtc_peer_connection_config_t.security_backend` 已存在但未类型化，是演进为安全 backend vtable/config 的自然接入点。
- `limits.dtls.max_sessions` 已存在，可作为第 4 阶段固定 DTLS session/storage 容量的起点。
- `RTC_STATUS_BACKEND_ERROR`、observer error、trace field 和 counters 模式已经存在，可复用于安全层错误映射。

### 既有模式

- Network API 必须在 `RTC_EXECUTOR_NETWORK` 上调用；亲和违规返回 `RTC_STATUS_AFFINITY_VIOLATION` 并输出 observer/trace。
- 创建成功后运行期不得动态增长；第 4 阶段新增 backend session、SRTP context、keying material、security counters 和测试后端状态必须来自 create-time arena/limits。
- 公共 status 保持稳定粗粒度；细节通过 observer detail code、trace reason 和 counter 暴露。
- 测试继续使用纯 C 自研 test runner 和 CTest，不引入第三方测试框架。
- Markdown 文档使用中文；技术标识符、API 名称、协议字段、trace event 和需求 ID 可以保留英文。

### 集成点

- 新增代码应围绕 `src/dtls`、`src/srtp`、`src/security` 或等价子系统拆分，避免把 DTLS/SRTP 状态机塞进 `src/api/peer_connection.c`。
- `rtc_peer_connection_t` 内部需要保存安全状态、DTLS role、backend session/storage、远端 fingerprint、SRTP ready 状态、key export 结果和安全 counters。
- SDP writer 应在第 4 阶段从 security backend 取得本地 SHA-256 fingerprint；parser 保存的远端 fingerprint 是 DTLS 校验输入。
- 第 5 阶段 RTP/RTCP 媒体平面应调用本阶段内部 SRTP/SRTCP protect/unprotect wrapper，而不是直接碰 backend vtable。

</code_context>

<specifics>
## 具体想法

- 用户要求讨论全部五个灰区，并接受了每组推荐方案。
- 总方向是：保持完整 `PeerConnection` 体验，把 DTLS/SRTP 编排留在核心内部；用户只提供 backend、arena、executors、observer 和 UDP 收发。
- 本阶段优先把安全状态机和错误语义测稳，真实 Chrome 握手和媒体端到端验收不提前拉入第 4 阶段。
- Deterministic 测试后端是本阶段的关键设计工具，不是临时替代品；它负责把 fingerprint mismatch、handshake error、key export failure、SRTP protect/unprotect failure 做成可重复测试路径。

</specifics>

<deferred>
## 延后事项

- 真实 Chrome DTLS 握手验收：留到第 6 阶段端到端验收。
- 多 fingerprint 算法，例如 SHA-384/SHA-512：首版第 4 阶段不支持，后续兼容扩展再评估。
- 多 transport / 多 m-line SRTP context：首版固定 Chrome 1v1 BUNDLE/rtcp-mux，不做动态扩展。
- 公共 `protect_rtp` / `unprotect_rtp` 用户 API：第 4 阶段不暴露；第 5 阶段如需要再围绕媒体 API 设计。
- 强制真实第三方 TLS/SRTP 库适配作为阶段验收：第 4 阶段只要求 deterministic 测试后端和可选适配骨架/文档。

</deferred>

---

*阶段：4-DTLS-SRTP 安全传输*
*上下文收集时间：2026-05-10T20:20:50+08:00*
