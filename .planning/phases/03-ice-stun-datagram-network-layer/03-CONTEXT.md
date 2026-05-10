# 第 3 阶段：ICE/STUN 与 Datagram 网络层 - 上下文

**Gathered:** 2026-05-10T18:45:00+08:00
**Status:** 准备进入规划

<domain>
## 阶段边界

本阶段交付 WebRTC 网络连通性基础：在用户负责 UDP/socket 收发、库内部不创建线程的边界内，实现 host/srflx candidate gathering、Full ICE connectivity checks、trickle ICE 本地/远端候选增量处理、STUN transaction、ICE 状态事件，以及 STUN/DTLS/RTP/RTCP datagram demux。

本阶段真实处理 STUN 和 ICE 状态机，但不实现 DTLS 握手、SRTP/SRTCP protect/unprotect、RTP/RTCP 媒体解析或 Chrome 端到端页面。DTLS/RTP/RTCP datagram 只在 demux 层完成分类、trace、计数和占位路由，具体协议内容留给第 4、5 阶段。

</domain>

<decisions>
## 实现决策

### ICE 角色与提名

- **D-01:** ICE controlling/controlled 角色由 SDP/JSEP 流程推导，不由用户在常规路径手动配置。本地发起 offer 的路径默认 `controlling`，接收远端 offer 的路径默认 `controlled`。
- **D-02:** 如果后续 connectivity checks 中出现 role conflict，应按 ICE/STUN 规则处理并通过 trace/error 暴露原因，而不是要求用户重新配置角色。
- **D-03:** 第 3 阶段采用 regular nomination。公共契约不承诺 aggressive nomination；planner 可以在内部结构上保留未来扩展余地，但首版行为和测试按 regular nomination 锁定。

### Candidate Gathering 与 Connectivity Checks 入口

- **D-04:** 第 3 阶段新增显式 network executor API 来启动 candidate gathering。`createOffer`、`createAnswer`、`setLocalDescription` 和 `setRemoteDescription` 不隐式启动网络行为。
- **D-05:** Candidate gathering 与 connectivity checks 使用两个入口分开表达。一个入口只负责 host/srflx candidate gathering，并通过 `observer.on_local_candidate` 输出本地候选；另一个入口在本地和远端 candidate 具备后启动 candidate pair checks、regular nomination 和 ICE 状态推进。
- **D-06:** 这两个入口都属于 network executor 亲和。planner 可以决定具体 API 名称，例如 `rtc_peer_connection_gather_candidates` 和 `rtc_peer_connection_start_connectivity_checks`，但必须保持职责边界清晰。
- **D-07:** `addIceCandidate` 从第 2 阶段的“解析/保存远端 candidate 字符串”演进为 trickle ICE 远端候选增量输入的一部分；它仍受固定 `limits.ice` 容量约束，不应触发隐式 socket 操作。

### STUN Server 与网络配置

- **D-08:** STUN server 配置在 `rtc_peer_connection_config_t` 或等价 create-time 配置中固定。创建后不支持修改 STUN server 列表。
- **D-09:** 首版 STUN server 只接受 IP:port，不做 hostname/DNS 解析。DNS、异步解析和 hostname 配置属于后续扩展。
- **D-10:** STUN server 数量为 `0` 或 `1`。`0` 表示 host-only gathering；`1` 表示 host + srflx gathering；超过 1 个 server 在第 3 阶段返回稳定配置错误或不支持错误。
- **D-11:** STUN server、candidate 槽、candidate pair、transaction、timer 等资源必须在 create 阶段按 arena/limits 切分；运行期不得动态增长。

### Datagram Demux 边界

- **D-12:** `rtc_peer_connection_receive_datagram` 在第 3 阶段必须能分类 STUN、DTLS、RTP 和 RTCP。STUN 被真实处理；DTLS/RTP/RTCP 只进行 trace、计数和占位路由，不解析协议内容。
- **D-13:** 未知或畸形 datagram 返回 `RTC_STATUS_PROTOCOL_ERROR`，并通过 observer error、trace 和 counter 暴露分类失败原因。用户可以选择忽略返回码继续收包，但库不能把无效输入当作成功处理。
- **D-14:** Datagram 输入仍必须在 `RTC_EXECUTOR_NETWORK` 上调用，且 buffer 所有权和生命周期需要在公共文档中明确：库只在调用期间读取用户传入的 datagram，不持有调用方 buffer。
- **D-15:** 通过 `observer.on_datagram` 输出的待发送 UDP datagram 由用户负责发送；本阶段不得创建 socket、绑定端口或拥有平台事件循环。

### ICE 状态、Trace 与计数器

- **D-16:** Observer 输出简洁 ICE 状态事件，例如 gathering、checking、connected、failed 等阶段级状态，避免把公共 observer vtable 做成过重的协议细节接口。
- **D-17:** Trace 和 counter 记录细粒度诊断信息，包括 host candidate、srflx request/response、STUN transaction sent/received/timeout/error、candidate pair checked、nominated、selected pair、role conflict、容量不足和失败原因。
- **D-18:** Selected pair 的 trace 至少应能表达本地/远端 candidate 类型、地址、端口，以及 foundation/priority 或内部 pair id 中的可诊断标识。
- **D-19:** ICE 失败必须能区分没有本地候选、没有远端候选、STUN timeout、role conflict、pair check exhausted、容量不足、畸形 STUN 或未知 datagram 等原因。

### 智能体的自由裁量

用户未锁定具体 API 函数名、内部 ICE/STUN 文件拆分、candidate priority/foundation 计算 helper 名称、trace detail code 枚举值或测试 fixture 命名。planner 可以在不突破上述决策的前提下决定这些实现细节，但必须保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发，并避免 GPL/LGPL 依赖。

</decisions>

<canonical_refs>
## 权威引用

**下游智能体在规划或实现前必须阅读这些文档。**

### 项目范围与需求

- `.planning/PROJECT.md`：项目价值、首版边界、关键决策和不在范围内事项。
- `.planning/REQUIREMENTS.md`：第 3 阶段覆盖的 `SDP-05`、`ICE-01`、`ICE-02`、`ICE-03`、`ICE-04`、`ICE-05`、`NET-01`、`NET-02`、`NET-03` 需求及需求追踪。
- `.planning/ROADMAP.md`：第 3 阶段目标、成功标准和后续第 4、5 阶段边界。
- `.planning/STATE.md`：当前项目状态和工作流配置。

### 已锁定的上游边界

- `.planning/phases/01-core-skeleton-boundary-contract/01-CONTEXT.md`：公共 API、固定 arena、executor 亲和、observer、trace、counter 和测试策略决策。
- `.planning/phases/02-sdp-jsep-offer-answer/02-CONTEXT.md`：Chrome 1v1 SDP/JSEP、最小 offer/answer 状态机、远端 candidate 保存和第 3 阶段 ICE 边界。
- `docs/000-设计边界记录.md`：纯 C、固定内存、无线程、用户负责 UDP/socket、Full ICE + STUN、host/srflx、不支持 TURN、trickle ICE 和 datagram 边界。
- `docs/API-执行器与内存契约.md`：公开 API 的 executor 归属、固定 arena/limits、observer/trace/counter、datagram 和第 2 阶段 SDP/JSEP 契约。

### 现有代码入口

- `include/rtc/peer_connection.h`：已有 `rtc_peer_connection_receive_datagram`、`rtc_peer_connection_add_ice_candidate` 和 offer/answer/description API 形状。
- `include/rtc/limits.h`：已有 `limits.ice.max_candidates`、`limits.ice.max_timer_slots` 等容量入口，需要扩展以覆盖 pair/transaction/STUN 资源。
- `include/rtc/observer.h`：已有 `on_local_candidate`、`on_datagram`、`on_state`、`on_error` 和 `on_trace` 回调入口。
- `include/rtc/trace.h`：已有稳定 trace 命名和字段键模式，需要扩展 ICE/STUN/demux 事件。
- `src/api/peer_connection.c`：当前 `receive_datagram` 返回 unsupported，`addIceCandidate` 已保存远端候选；第 3 阶段需要替换为真实 ICE/STUN/demux 接入。

</canonical_refs>

<code_context>
## 既有代码洞察

### 可复用资产

- `rtc_peer_connection_receive_datagram`：已声明并验证 network executor 亲和，是第 3 阶段 datagram demux 的公共入口。
- `rtc_peer_connection_add_ice_candidate`：已解析 `candidate:` / `a=candidate:` 前缀并保存固定槽远端候选，可演进为远端 trickle candidate 输入。
- `observer.on_local_candidate` 与 `observer.on_datagram`：第 1 阶段已预留本地候选输出和待发送 datagram 输出回调。
- `rtc_peer_connection_limits_t.ice`：已有 `max_candidates` 和 `max_timer_slots`，可以作为扩展 candidate、pair、transaction、timer 容量模型的入口。
- `rtc_pc_trace`、`rtc_observer_emit_error`、counter helper 模式：现有 API、JSEP、SDP 错误和 trace 风格可复用于 ICE/STUN/demux。

### 既有模式

- Signaling API 在 `RTC_EXECUTOR_SIGNALING` 上调用；network API 在 `RTC_EXECUTOR_NETWORK` 上调用，亲和违规返回 `RTC_STATUS_AFFINITY_VIOLATION` 并输出 observer/trace。
- 创建成功后运行期不得动态增长；第 3 阶段新增的 ICE/STUN 内部对象、队列、candidate pair、transaction 和 timer slot 必须来自 create 阶段 arena 切分。
- 公共错误使用稳定 `rtc_status_t`，诊断细节通过 observer error、trace field 和 counter 表达，文本只能作为辅助。
- 测试继续使用纯 C 自研 test runner 和 CTest，不引入第三方测试框架。
- Markdown 文档使用中文；技术标识符、API 名称、协议字段、trace event 和需求 ID 可以保留英文。

### 集成点

- 新增代码应围绕 `src/ice`、`src/stun`、`src/net` 或等价子系统拆分，避免把 ICE/STUN 状态机和 demux 全塞进 `src/api/peer_connection.c`。
- `rtc_peer_connection_t` 内部需要保存本地 candidate、远端 candidate 解析摘要、candidate pair、ICE role、nomination 状态、STUN transaction、selected pair 和 ICE 状态。
- SDP/JSEP 层保存的 ICE ufrag/pwd、远端 description 摘要和远端 candidate 列表是第 3 阶段 ICE 启动的上游输入。
- `docs/API-执行器与内存契约.md` 需要更新第 3 阶段 API、buffer 生命周期、gather/checks 入口和 datagram demux 语义。

</code_context>

<specifics>
## 具体想法

- 用户选择了全部灰区进行讨论，并要求在若干关键选择上给出推荐理由。
- 推荐并锁定的总方向是：网络行为显式启动、gathering 与 connectivity checks 分开、STUN 配置创建时固定、首版 0/1 个 IP:port STUN server、demux 只真实处理 STUN、observer 简洁而 trace/counter 细。
- 本阶段应优先让失败可解释，而不是把浏览器内部 ICE 行为黑盒化。

</specifics>

<deferred>
## 延后事项

- Aggressive nomination：公共契约不承诺，未来如需要可作为扩展。
- 创建后动态修改 STUN server 列表：第 3 阶段不支持。
- Hostname/DNS 解析和多个 STUN server fallback：留给后续扩展。
- TURN relay candidate：v1 不支持。
- DTLS 握手、SRTP/SRTCP、RTP/RTCP 协议解析和媒体处理：分别由第 4、5 阶段处理。

</deferred>

---

*阶段：3-ICE/STUN 与 Datagram 网络层*
*上下文收集时间：2026-05-10T18:45:00+08:00*
