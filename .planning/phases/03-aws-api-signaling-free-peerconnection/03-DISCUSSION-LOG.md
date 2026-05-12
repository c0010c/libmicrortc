# Phase 3: AWS 风格 API 与 signaling-free PeerConnection - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-12T21:54:08+08:00
**Phase:** 3-AWS 风格 API 与 signaling-free PeerConnection
**Areas discussed:** 公共头文件裁剪边界, PeerConnection 生命周期与状态语义, SDP/ICE candidate 应用层交换模型, Answerer 先行与 Offerer 预留, 补充 API 细节

---

## 公共头文件裁剪边界

| Option | Description | Selected |
|--------|-------------|----------|
| 薄裁剪 AWS API | 保留 AWS 的主要使用模型和函数语义，但类型和命名可逐步收敛到 `MRTC_*` / `mrtc_*`。 | ✓ |
| 强兼容 AWS 头文件子集 | 尽量保留 AWS 公共类型、函数名和调用方式，只删除 KVS/signaling/storage 相关内容。 | |
| 立即改成 libmicrortc 风格 API | 不追求 AWS 头文件形态，只参考能力集合，直接设计全新 API。 | |

**User's choice:** 薄裁剪 AWS API。
**Notes:** 用户选择保留 AWS/WebRTC 使用模型，但不追求源码级兼容。

| Option | Description | Selected |
|--------|-------------|----------|
| 只保留语义，不保留 AWS 基础类型 | 不把 `STATUS`、`PCHAR`、`PVOID`、`BOOL` 等 AWS/PIC 基础类型暴露到新公共头。 | ✓ |
| 短期保留 AWS 基础类型别名 | 为搬迁速度保留一批 AWS 风格 typedef。 | |
| 双层头文件 | 默认暴露 `micrortc` 风格头，另给兼容/迁移头承接 AWS 风格别名。 | |

**User's choice:** 只保留语义，不保留 AWS 基础类型。
**Notes:** 延续 Phase 2 的 `MRTC_STATUS` 和 `mrtc_*` 边界。

---

## PeerConnection 生命周期与状态语义

| Option | Description | Selected |
|--------|-------------|----------|
| Opaque handle + create/free | 公共头只暴露不透明句柄，以及 create/free 函数。 | ✓ |
| 公开结构体配置 + 半透明对象 | 配置结构公开，对象结构也暴露部分字段。 | |
| 接近 AWS 对象模型 | 尽量保留 AWS 的 PeerConnection 结构和调用方式。 | |

**User's choice:** Opaque handle + create/free。
**Notes:** PeerConnection 内部结构应隐藏，作为稳定 C ABI 起点。

| Option | Description | Selected |
|--------|-------------|----------|
| create 时传 config + callback table | create 一次性传入配置、回调表和 `user_data`。 | ✓ |
| create 后逐项 setter 注册 | 先 create 空对象，再逐项注册回调。 | |
| 混合模式 | create 传 config，callbacks 后续 setter 注册。 | |

**User's choice:** create 时传 config + callback table。
**Notes:** 沿用 AWS 回调思路，但命名和类型使用 `MRTC_*` / `mrtc_*`。

---

## SDP/ICE candidate 应用层交换模型

| Option | Description | Selected |
|--------|-------------|----------|
| 纯字符串交换边界 | 核心库通过回调产出 SDP/candidate 字符串，应用层调用 setter/add 函数喂回远端字符串。 | ✓ |
| 结构化对象 + 字符串序列化接口 | 公共 API 暴露 SDP/candidate 结构体，同时提供 parse/serialize。 | |
| 内置一个极简 signaling adapter 接口 | 核心库定义 signaling transport callback。 | |

**User's choice:** 纯字符串交换边界。
**Notes:** 核心库不关心 WebSocket/HTTP/文件/stdio 等交换方式。

| Option | Description | Selected |
|--------|-------------|----------|
| AWS 使用模型的薄裁剪 | 保留 `createOffer` / `createAnswer` / `setLocalDescription` / `setRemoteDescription` 语义，并以字符串 SDP 输入输出。 | ✓ |
| 合并成更少的高层函数 | 例如 accept offer 并 create answer 的高层函数。 | |
| 只做 Answerer 最小函数集 | Phase 3 暂时只支持 remote offer -> local answer，不提供 create offer。 | |

**User's choice:** AWS 使用模型的薄裁剪。
**Notes:** 使用 `mrtc_*` 命名，但保留标准 PeerConnection 流程语义。

---

## Answerer 先行与 Offerer 预留

| Option | Description | Selected |
|--------|-------------|----------|
| Answerer 可用，Offerer API 编译可用但可返回未实现 | 测试重点放在 remote offer -> local answer，`create_offer` 先存在并可编译。 | ✓ |
| Answerer 和 Offerer 都要基础可用 | 范围更完整，但会提前拉入 Phase 4 风险。 | |
| 只做 Answerer，不暴露 Offerer 接口 | 实现最窄，但弱化同一 v1 路线内 Offerer 预留。 | |

**User's choice:** Answerer 可用，Offerer API 编译可用但可返回未实现。
**Notes:** `create_offer` 可返回 `MRTC_STATUS_NOT_IMPLEMENTED` 或等价状态。

---

## 补充 API 细节

| Option | Description | Selected |
|--------|-------------|----------|
| 支持 trickle candidate | 提供 `on_ice_candidate` 回调和 `mrtc_peer_connection_add_ice_candidate()`。 | ✓ |
| 只支持 SDP 内嵌 candidate | Phase 3 API 更少，但后续互通更别扭。 | |
| 两者都支持，但 Phase 3 只测 SDP 内嵌 | 公共面完整，但测试语义容易含糊。 | |

**User's choice:** 支持 trickle candidate。
**Notes:** Phase 3 锁定 API 和字符串边界，真实 ICE 行为留给 Phase 4。

| Option | Description | Selected |
|--------|-------------|----------|
| 只放占位所需的最小声明 | Phase 3 聚焦 PeerConnection、SDP、ICE candidate；transceiver/media/DataChannel 只做后续预留。 | ✓ |
| 提前暴露 DataChannel 创建 API | 方便 Phase 4 接入，但超出 Phase 3 requirements。 | |
| 提前暴露 transceiver/writeFrame API | 方便 Phase 5 媒体路径，但明显超出 Phase 3。 | |

**User's choice:** 只放占位所需的最小声明。
**Notes:** 不正式暴露 transceiver/media/DataChannel 可调用 API。

| Option | Description | Selected |
|--------|-------------|----------|
| 解析/序列化基础 round-trip + Answerer SDP 生成 | 证明 SDP 模块脱离 signaling client，可消费 remote offer 并生成 local answer。 | ✓ |
| 只测 API 编译和字符串传递 | 范围最窄，但 PROTO-06 覆盖偏弱。 | |
| 要求用 Chrome SDP 样本做较完整兼容测试 | 更可信，但可能提前引入 Phase 6 E2E 风险。 | |

**User's choice:** 解析/序列化基础 round-trip + Answerer SDP 生成。
**Notes:** 不要求完整 Chrome E2E。

| Option | Description | Selected |
|--------|-------------|----------|
| 扩展 MRTC_STATUS 最小状态集 | 加入 `NOT_IMPLEMENTED`、`INVALID_STATE`、`PARSE_ERROR` 等 Phase 3 所需状态。 | ✓ |
| 沿用 AWS STATUS 数值/宏体系 | 更接近参考实现，但会把 AWS/PIC 状态体系带进公共 API。 | |
| 只返回 OK/INVALID_ARG，细节靠日志 | API 太贫血，调试困难。 | |

**User's choice:** 扩展 MRTC_STATUS 最小状态集。
**Notes:** 不引入 AWS `STATUS`。

---

## the agent's Discretion

- 具体 public header 拆分、函数名细节、callback 字段命名和 config 最小字段集合由 planner 决定。
- SDP 字符串内存所有权规则由 planner 决定，但必须在 public API 文档和测试中明确。

## Deferred Ideas

- DataChannel API 留给 Phase 4。
- transceiver、media、`writeFrame` 类 API 留给 Phase 5。
- Chrome 自动化 E2E、TURN relay 和双向 H264/Opus 媒体流动验收留给 Phase 6。
