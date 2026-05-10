# 第 3 阶段：ICE/STUN 与 Datagram 网络层 - 讨论记录

> **仅用于审计。** 不要作为规划、研究或执行智能体的输入。
> 决策已捕获在 `03-CONTEXT.md`；本记录保留曾考虑过的替代方案。

**日期：** 2026-05-10T18:45:00+08:00
**阶段：** 3-ICE/STUN 与 Datagram 网络层
**讨论区域：** ICE 角色与提名策略、Candidate gathering 入口、STUN server 与网络配置、Datagram demux 边界、ICE 状态与失败可观测性

---

## ICE 角色确定

| 选项 | 描述 | 已选 |
|------|------|------|
| 由 SDP/JSEP 推导 | 本地发起 offer 时默认 `controlling`，接收远端 offer 时默认 `controlled`；符合当前最小 JSEP 流程。 | ✓ |
| 用户显式配置 | 用户在 config 中指定角色，适合嵌入式强控制，但更容易与 JSEP 状态冲突。 | |
| 先固定单角色 | 只实现一种角色来降低复杂度，但不能完整覆盖 Chrome 互通场景。 | |

**用户选择：** 由 SDP/JSEP 推导。
**备注：** Role conflict 后续由 ICE/STUN 规则处理并通过 trace/error 暴露，不要求用户手动配置常规角色。

---

## ICE 提名策略

| 选项 | 描述 | 已选 |
|------|------|------|
| Regular nomination | 先完成 candidate pair 检查，再发送带 `USE-CANDIDATE` 的 nominated check；行为稳，贴近 RFC 8445。 | ✓ |
| Aggressive nomination | 更快选中 pair，但状态和失败解释更复杂；首版 Chrome 互通不是必须。 | |
| 先 regular，内部预留 aggressive 扩展点 | 公共 API 和文档只承诺 regular，内部结构不把 aggressive 路堵死。 | |

**用户选择：** Regular nomination。
**备注：** 公共契约不承诺 aggressive nomination；planner 可自行决定是否在内部结构中保留未来扩展余地。

---

## Candidate Gathering 启动时机

| 选项 | 描述 | 已选 |
|------|------|------|
| 显式启动 gathering | 新增或明确显式入口；用户在准备好 UDP 端口、STUN server 和收发循环后调用。 | ✓ |
| `setLocalDescription` 后自动开始 | 更接近浏览器体验，但会让信令 API 隐式启动 network executor 行为。 | |
| `createOffer/createAnswer` 时开始 | 最早产出候选，但会让 SDP 生成 API 带网络副作用。 | |

**用户选择：** 显式启动 gathering。
**备注：** 用户要求推荐理由；推荐理由是避免 SDP/JSEP API 带网络副作用，保持 network executor 边界清晰，并让用户负责 UDP/socket 的项目边界更可控。

---

## Gathering 与 Checks 入口粒度

| 选项 | 描述 | 已选 |
|------|------|------|
| 只启动 gathering | API 只负责收集本地候选；checks 由状态机或另一个入口触发。 | |
| 启动完整 ICE 流程 | 一个入口同时开始 gathering、pair checks 和 nomination；用户 API 简单但内部时序复杂。 | |
| 两个入口分开 | 一个 `gather_candidates`，一个 `start_connectivity_checks`；公共 API 更显式，排障更清楚。 | ✓ |

**用户选择：** 两个入口分开。
**备注：** 用户先询问推荐，随后确认两个入口分开。推荐理由包括 trickle ICE 时序清楚、固定内存资源更好分配、gathering 与 checks 失败原因更容易定位。

---

## STUN Server 配置形状

| 选项 | 描述 | 已选 |
|------|------|------|
| 创建时固定，STUN server 只接受 IP:port | 固定 arena 下资源可在 create 阶段切分，不引入 DNS/hostname 平台差异。 | ✓ |
| 创建时固定，但允许 hostname | 用户配置更友好，但会引入 DNS 解析边界。 | |
| 创建后可配置/更新 STUN server | 灵活，但会和正在进行的 gathering/checks 状态冲突。 | |

**用户选择：** 创建时固定，STUN server 只接受 IP:port。
**备注：** DNS、hostname 和创建后动态修改留给后续扩展。

---

## STUN Server 数量

| 选项 | 描述 | 已选 |
|------|------|------|
| 首版 1 个 STUN server | 最小可验收 srflx；状态、失败原因、测试简单。 | |
| 创建时支持固定上限多个 STUN server | 更接近生产容错，但增加 server fallback 和优先级策略。 | |
| 允许 0 或 1 个 STUN server | `0` 表示 host-only，`1` 表示 host + srflx；超过 1 暂不支持。 | ✓ |

**用户选择：** 确认 0 或 1 个 STUN server。
**备注：** 这是推荐组合：host-only 可作为合法模式；配置 1 个 server 时额外生成 srflx candidate。

---

## Datagram Demux 范围

| 选项 | 描述 | 已选 |
|------|------|------|
| 分类全部，真实处理 STUN，其他只 trace/占位 | STUN/DTLS/RTP/RTCP 都能分类；本阶段只真实处理 STUN。 | ✓ |
| 本阶段只接受 STUN，其他一律 unsupported | 范围更小，但不能提前建立后续层共同入口。 | |
| 分类并尝试初步解析 DTLS/RTP/RTCP header | 更深入，但会进入第 4、5 阶段范围。 | |

**用户选择：** 分类全部，真实处理 STUN，其他只 trace/占位。
**备注：** 避免提前实现 DTLS/RTP/RTCP，同时固定后续安全层和媒体层的共同 datagram 入口。

---

## 未知或畸形 Datagram

| 选项 | 描述 | 已选 |
|------|------|------|
| 返回 `RTC_STATUS_PROTOCOL_ERROR` 并输出 trace/error | API 语义清楚，排障价值高，测试稳定。 | ✓ |
| 静默丢弃但计数 | 更像网络栈容错，但用户不容易发现接错包。 | |
| 返回 OK，只记录 unknown counter/trace | 不打断收包循环，但 API 语义容易变松。 | |

**用户选择：** 返回 `RTC_STATUS_PROTOCOL_ERROR` 并输出 trace/error。
**备注：** 用户要求推荐理由；推荐理由是 `receive_datagram` 是显式处理输入，无效包应被清楚报告，用户仍可选择忽略错误继续收包。

---

## ICE 状态与失败可观测性

| 选项 | 描述 | 已选 |
|------|------|------|
| 细粒度 trace + 简洁 observer 状态 | Observer 输出阶段级事件；trace/counter 记录 transaction、pair、原因等细节。 | ✓ |
| observer 和 trace 都很细 | 用户可直接在回调拿到所有细节，但公共 observer 契约变重。 | |
| 只做粗粒度状态 | 简单，但 ICE 失败排障能力不足。 | |

**用户选择：** 细粒度 trace + 简洁 observer 状态。
**备注：** 用户询问推荐选项后选择 1。推荐理由是保持公共 API 轻量，同时通过 trace/counter 提供 candidate pair、STUN transaction、selected pair 和失败原因等生产排障信息。

---

## 智能体的自由裁量

- 具体 API 函数名由 planner 决定，但必须表达两个 network executor 入口：gathering 和 connectivity checks。
- 内部文件拆分、helper 命名、trace detail code、candidate/pair/transaction 结构布局和 fixture 命名由 planner 决定。
- Planner 可以在内部为 aggressive nomination、多个 STUN server、DNS 等未来扩展保留结构余地，但第 3 阶段公共行为不得承诺这些能力。

## 延后事项

- Aggressive nomination。
- 创建后动态修改 STUN server。
- Hostname/DNS 解析。
- 多个 STUN server fallback。
- TURN relay candidate。
- DTLS 握手、SRTP/SRTCP、RTP/RTCP 协议解析和媒体处理。
