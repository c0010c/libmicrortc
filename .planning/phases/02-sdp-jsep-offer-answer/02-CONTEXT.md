# 第 2 阶段：SDP/JSEP 与 Offer/Answer - 上下文

**Gathered:** 2026-05-10T16:45:00+08:00
**Status:** 准备进入规划

<domain>
## 阶段边界

本阶段交付 Chrome 1v1 最小 SDP/JSEP 与 Offer/Answer 能力：生成本地 offer/answer SDP，解析 Chrome offer/answer，保存本地和远端 description，推进最小 offer/answer 状态机，并让 `addIceCandidate` 能解析、校验和保存远端 trickle candidate 字符串。

本阶段不实现 ICE connectivity checks、STUN、candidate gathering、DTLS 握手、SRTP、RTP/RTCP 或 Chrome 端到端通话。ICE/DTLS 相关字段只作为 SDP/JSEP 层所需的参数被存储、校验和序列化，真实生成和网络行为由后续阶段接入。

</domain>

<decisions>
## 实现决策

### SDP 画像与生成

- **D-01:** 本阶段采用固定 Chrome 1v1 SDP 模板，不做通用 SDP builder。模板聚焦 1 路 audio、1 路 video、BUNDLE、rtcp-mux、trickle capability、Opus、H264、`sendrecv` 和 `recvonly`。
- **D-02:** ICE/DTLS 参数由用户 API 或测试夹具提供，包括 `ice-ufrag`、`ice-pwd`、DTLS fingerprint 和 setup role。本阶段只负责存储、校验和 SDP 序列化，不提前实现随机数、crypto、证书或 DTLS backend 生成逻辑。
- **D-03:** codec 画像固定为 Opus + H264 baseline 最小 Chrome 互通集合。`rtpmap`、`fmtp`、`rtcp-fb`、payload type 等字段由 golden tests 锁定；本阶段不做通用 codec 协商。
- **D-04:** 媒体方向只支持 `sendrecv` 和 `recvonly`。解析、保存和输出这两个方向；遇到 `sendonly` 或 `inactive` 返回稳定的不支持或协议错误。

### Offer/Answer 状态机

- **D-05:** 内部实现最小 offer/answer 状态机，状态包括 `stable`、`have-local-offer`、`have-remote-offer`。它是 JSEP-compatible 的最小状态契约，不实现完整浏览器 JSEP。
- **D-06:** 正常发起流程为 `stable -> createOffer -> setLocalDescription(offer) -> have-local-offer -> setRemoteDescription(answer) -> stable`。
- **D-07:** 正常接听流程为 `stable -> setRemoteDescription(offer) -> have-remote-offer -> createAnswer -> setLocalDescription(answer) -> stable`。
- **D-08:** 本阶段不支持 rollback、pranswer、renegotiation 或多轮协商。非法顺序必须被拒绝。
- **D-09:** 非法状态转换返回稳定 `rtc_status_t`，并通过 observer error 与 trace 输出细节。trace/detail 至少包含 operation、当前状态、目标状态或拒绝原因；文本消息只能作为辅助，不能成为稳定契约。

### ICE/trickle 边界

- **D-10:** `addIceCandidate` 在第 2 阶段解析、校验并保存远端 candidate 字符串，受 `limits.ice.max_candidates` 约束。
- **D-11:** `addIceCandidate` 不启动 ICE connectivity checks，不计算 candidate pair，不执行 STUN transaction，不计算 ICE priority/foundation。
- **D-12:** 第 2 阶段不生成本地 candidate，不触发 observer `on_local_candidate`。本地 candidate gathering、host/srflx candidate 和 ICE 状态事件属于第 3 阶段。
- **D-13:** 本阶段可以在 SDP 中表达 ICE 参数和 trickle capability，但不得依赖本机网络接口、socket、UDP 端口或平台网络枚举。

### 测试与 fixture

- **D-14:** SDP golden tests 使用仓库内固定 Chrome SDP fixture 和本地 offer/answer expected，不依赖运行时启动 Chrome 或抓取 SDP。
- **D-15:** 动态字段通过测试夹具固定或规范化，例如 session id、ICE ufrag/pwd、DTLS fingerprint。
- **D-16:** 错误路径测试必须覆盖 SDP 结构错误、不支持字段和状态错误，包括缺失 BUNDLE/rtcp-mux、缺失 ICE/DTLS 字段、未知 codec、`sendonly`/`inactive`、非法 offer/answer 顺序、以及超过 `limits.sdp.max_description_bytes`。

### 智能体的自由裁量

用户未要求锁定内部文件名、parser/tokenizer 具体实现、fixture 目录命名、payload type 数值或 trace detail code 枚举值。planner 可以在不突破上述阶段边界的前提下决定这些实现细节，但必须保持纯 C、固定内存、无线程、无 GPL/LGPL 依赖，并遵守第 1 阶段已建立的 API、executor、observer、trace、counter 和 allocator 契约。

</decisions>

<canonical_refs>
## 权威引用

**下游智能体在规划或实现前必须阅读这些文档。**

### 项目范围与需求

- `.planning/PROJECT.md`：项目价值、首版边界、关键决策和不在范围内事项。
- `.planning/REQUIREMENTS.md`：第 2 阶段覆盖的 `SDP-01`、`SDP-02`、`SDP-03`、`SDP-04`、`API-04` 需求及需求追踪。
- `.planning/ROADMAP.md`：第 2 阶段目标、成功标准和后续阶段边界。
- `.planning/STATE.md`：当前项目状态和工作流配置。

### 已锁定的上游边界

- `.planning/phases/01-core-skeleton-boundary-contract/01-CONTEXT.md`：第 1 阶段 D-01 到 D-20 的公共 API、固定内存、executor 亲和、observer、trace、counter 和测试策略决策。
- `docs/000-设计边界记录.md`：纯 C、固定内存、无线程、用户负责 UDP/socket、Chrome 最小 SDP/JSEP、可观测性等设计边界。
- `docs/API-执行器与内存契约.md`：公开 API 的 executor 归属、固定 arena/limits、observer/trace/counter 契约和当前占位 API。

### 现有代码入口

- `include/rtc/peer_connection.h`：已声明的 `create_offer`、`create_answer`、`set_local_description`、`set_remote_description`、`add_ice_candidate` API 形状。
- `include/rtc/limits.h`：`limits.sdp.max_description_bytes` 与 `limits.ice.max_candidates` 等容量入口。
- `include/rtc/observer.h`：observer error、trace、本地 candidate 回调契约。
- `src/api/peer_connection.c`：当前占位 API、executor 亲和检查、unsupported/error/trace 模式。

</canonical_refs>

<code_context>
## 既有代码洞察

### 可复用资产

- `include/rtc/peer_connection.h`：第 2 阶段应实现已有占位 API，而不是新增另一套 offer/answer 入口。
- `src/api/peer_connection.c`：已有 `rtc_unsupported_signaling`、`rtc_pc_affinity_violation`、`rtc_pc_trace` 模式，可替换为真实 SDP/JSEP 行为并复用错误和 trace 风格。
- `include/rtc/limits.h`：已有 `sdp.max_description_bytes` 和 `ice.max_candidates`，可作为 SDP buffer 与 candidate 保存容量边界。
- `tests/test_runner.h` 与现有 `tests/test_*.c`：继续使用纯 C 自研测试 runner 和 CTest 调度，不引入第三方测试框架。

### 既有模式

- signaling API 必须在 `RTC_EXECUTOR_SIGNALING` 上调用，违规返回 `RTC_STATUS_AFFINITY_VIOLATION` 并输出 observer/trace。
- 公共 API 使用 `rtc_status_t` 返回稳定错误；observer error 使用 subsystem、operation、detail code 表达诊断信息。
- 创建成功后运行期不得动态增长；新增 SDP/JSEP 对象、description buffer、candidate storage 需要在 arena/limits 模型内完成。
- Markdown 文档使用中文；技术标识符、协议字段、API 名称和需求 ID 可以保留英文。

### 集成点

- 新增代码应围绕 `src/sdp`、`src/jsep` 或等价子系统拆分，避免把 SDP parser/generator 和状态机全部塞进 `src/api/peer_connection.c`。
- `rtc_peer_connection_t` 内部需要保存最小 signaling state、本地/远端 description 摘要、ICE/DTLS SDP 参数和远端 candidate 列表。
- golden fixture 可放在 `tests/fixtures` 或 planner 选择的等价目录，并由纯 C 测试读取或内嵌为静态字符串。

</code_context>

<specifics>
## 具体想法

- 用户明确询问“必须引入 JSEP 吗？”，最终决策是不引入完整浏览器 JSEP，只实现最小稳定 offer/answer 状态机。
- 本阶段的 SDP 行为应服务 Chrome 1v1 最小互通画像，而不是泛 SDP 兼容。
- 本地 candidate generation 和 `on_local_candidate` 主动触发留给第 3 阶段，以保持第 2 阶段测试不依赖机器网络环境。

</specifics>

<deferred>
## 延后事项

- 完整 JSEP 能力，例如 rollback、pranswer、renegotiation：不属于第 2 阶段。
- ICE connectivity checks、host/srflx candidate、STUN、candidate pair、gathering 状态事件：第 3 阶段处理。
- DTLS 证书、fingerprint 生成、DTLS setup 执行和 SRTP key export：第 4 阶段处理。
- 通用 SDP builder、广泛 SDP 兼容、多 codec 协商、多路媒体：v1 首版范围外或后续扩展处理。

</deferred>

---

*阶段：2-SDP/JSEP 与 Offer/Answer*
*上下文收集时间：2026-05-10T16:45:00+08:00*
