# Phase 4: 传输、安全与 DataChannel 协议核心 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-12T22:59:16+08:00
**Phase:** 4-传输、安全与 DataChannel 协议核心
**Areas discussed:** 候选与验证深度, 安全通道接入边界, DataChannel API 与 SCTP 路径

---

## 候选与验证深度

| Option | Description | Selected |
|--------|-------------|----------|
| 协议级/单元验证优先 | 只证明 ICE/STUN/TURN 模块能独立构建和跑协议测试；PeerConnection 先接接口，不要求真实网络连通。 | |
| 本地/受控网络验证优先 | 有可运行的小型 harness，至少验证 host candidate 真实连通，STUN/TURN 通过配置的真实 server 做验证。 | ✓ |
| 直接真实 Chrome 互通验证 | Phase 4 就要求 Chrome + C 端完成 ICE connected/DataChannel，Phase 6 再加媒体和完整自动化。 | |

**User's choice:** 本地/受控网络验证优先。
**Notes:** 用户进一步明确：真实 STUN/TURN server 作为验收依赖；配置文件保存在项目根目录，测试时直接读取；如果根目录配置文件缺失，真实 STUN/TURN 验证应失败，不应跳过。用户提供过真实 TURN 配置样例，但文档不保存 credential 原文。

### STUN/TURN 外部依赖策略

| Option | Description | Selected |
|--------|-------------|----------|
| 本地优先，外部可选 | 默认不依赖公网；存在 server 配置时才跑外部验证。 | |
| 必须内置本地 mock server | 实现或迁入足够的 STUN/TURN 测试服务，让验证完全自包含。 | |
| 直接依赖真实 STUN/TURN server | 验证默认要求可访问的 STUN/TURN server。 | ✓ |

**User's choice:** 直接依赖真实 STUN/TURN server。
**Notes:** 该 TURN 地址同时支持 STUN；规划时要记录配置字段和安全边界。

### 配置缺失策略

| Option | Description | Selected |
|--------|-------------|----------|
| 没有配置文件就跳过真实测试 | 默认构建和单元测试可跑；有配置才启用真实连通验证。 | |
| 没有配置文件就失败 | 强制 Phase 4 验证环境必须提供真实 server 配置。 | ✓ |
| 拆成两个命令 | `ctest` 跑默认测试，另一个显式命令跑真实 STUN/TURN 验证。 | |

**User's choice:** 没有配置文件就失败。
**Notes:** Phase 4 真实网络验证必须把根目录配置文件作为硬前置。

---

## 安全通道接入边界

| Option | Description | Selected |
|--------|-------------|----------|
| 只迁入并独立构建模块 | DTLS OpenSSL 和 SRTP session 能编译、初始化、单元测试通过，但不要求跟 PeerConnection/ICE 状态连起来。 | |
| 完成握手与 SRTP key export | ICE 选中 candidate pair 后，DTLS OpenSSL 路径能完成 client/server 角色握手，并导出 SRTP keying material，创建 SRTP session。 | ✓ |
| 完整保护媒体包路径 | Phase 4 就要求真实 RTP 包走 SRTP protect/unprotect。 | |

**User's choice:** 完成握手与 SRTP key export。
**Notes:** 真实 RTP 媒体包保护留给 Phase 5。

### DTLS 角色

| Option | Description | Selected |
|--------|-------------|----------|
| 从 SDP `setup` 属性推导 | 按 WebRTC SDP 语义处理 `actpass`、`active`、`passive`。 | ✓ |
| 测试配置显式指定 | 测试和 demo 直接告诉库当前是 client/server。 | |
| Phase 4 先固定 C 端角色 | 为 Answerer 路径先硬编码一个角色，后续再补 SDP 推导。 | |

**User's choice:** 从 SDP `setup` 属性推导。
**Notes:** 不靠测试配置硬指定 DTLS 角色。

### 证书与 Fingerprint

| Option | Description | Selected |
|--------|-------------|----------|
| 库内生成自签名证书并写入 SDP 指纹 | PeerConnection 创建时生成或持有证书，SDP answer 带 `a=fingerprint`；DTLS 校验远端 fingerprint。 | ✓ |
| 从配置文件加载证书 | 适合可重复测试和固定证书，但增加配置项和密钥管理边界。 | |
| Phase 4 暂不校验 fingerprint | 更快打通握手，但 WebRTC 安全语义不完整。 | |

**User's choice:** 库内生成自签名证书并写入 SDP 指纹。
**Notes:** 需要同时校验远端 fingerprint。

---

## DataChannel API 与 SCTP 路径

| Option | Description | Selected |
|--------|-------------|----------|
| AWS/WebRTC 薄裁剪 DataChannel API | 提供 DataChannel handle、create、send、close、on_open、on_message、on_close 回调。 | ✓ |
| 只暴露 PeerConnection 级 send/receive | 不单独暴露 DataChannel handle，API 简单但多 channel 难扩展。 | |
| Phase 4 先内部跑通，不暴露 public API | 先让 SCTP/DataChannel 内部可测，公共 API 留后面。 | |

**User's choice:** AWS/WebRTC 薄裁剪 DataChannel API。
**Notes:** 延续 Phase 3 的薄裁剪 AWS 使用模型和 `mrtc_*` API 风格。

### DataChannel 创建范围

| Option | Description | Selected |
|--------|-------------|----------|
| 先支持 negotiated=false 的常规创建 | C 端通过 DCEP 打开 DataChannel，和浏览器常规 `createDataChannel()` 对齐。 | ✓ |
| 只支持 negotiated=true 静态 channel | 双方预先约定 id，不跑 DCEP，实现更简单。 | |
| 两者都支持 | 功能完整，但 Phase 4 工作量更大。 | |

**User's choice:** 先支持 `negotiated=false` 的常规 DCEP 打开流程。
**Notes:** `negotiated=true` 可留作后续扩展。

### 消息类型范围

| Option | Description | Selected |
|--------|-------------|----------|
| 文本 + 二进制都支持 | API 明确 message type 或 binary flag；测试覆盖 text，binary 至少有基础用例。 | ✓ |
| 只支持文本 | 更快满足 demo ping/pong，但消息能力偏窄。 | |
| 只支持二进制 buffer | C API 更统一，文本由应用层解释。 | |

**User's choice:** 文本 + 二进制都支持。
**Notes:** 测试至少覆盖文本 ping/pong 和基础 binary buffer。

### DataChannel 验证边界

| Option | Description | Selected |
|--------|-------------|----------|
| 真实 ICE/DTLS/SCTP 路径上验证 DataChannel | 证明 DataChannel 在选中的 candidate pair、DTLS、SCTP 上能 open/send/receive/close。 | ✓ |
| SCTP loopback 为主，真实路径只到握手 | DataChannel 行为先用 loopback 测，真实路径只证明 SCTP association 或初始化。 | |
| 只做 API + 单元测试 | 最省力，但后续 E2E 压力会变大。 | |

**User's choice:** 真实 ICE/DTLS/SCTP 路径上验证 DataChannel。
**Notes:** DataChannel 不应只在 loopback 或 API 层证明可用。

---

## the agent's Discretion

- 配置文件名称、JSON/schema 细节、示例模板路径由 planner 决定，但真实 credential 不写入提交文档。
- DataChannel 函数名和 callback 字段名可由 planner 按现有 `mrtc_*` 风格细化。
- 测试 harness、CTest 拆分、真实网络验证命令组织由 planner 决定，但真实配置缺失时 Phase 4 验证必须失败。

## Deferred Ideas

- RTP/RTCP、H264/Opus、`writeFrame` 和真实媒体 SRTP protect/unprotect 留给 Phase 5。
- 完整 Chrome 自动化 E2E、浏览器页面、自动 signaling 和双向媒体断言留给 Phase 6。
- `negotiated=true` DataChannel 静态 channel 留作后续扩展。
