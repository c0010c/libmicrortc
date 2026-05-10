# 第 4 阶段：DTLS-SRTP 安全传输 - 讨论日志

> **仅用于审计。** 不要把本文作为规划、研究或执行智能体输入。
> 决策已记录在 `04-CONTEXT.md`；本文只保留讨论过的备选项。

**Date:** 2026-05-10T20:20:50+08:00
**Phase:** 04-DTLS-SRTP 安全传输
**Areas discussed:** 安全 backend 边界, 握手启动与角色语义, Fingerprint 与证书策略, SRTP key export 与 protect API, 错误、trace 与测试后端

---

## 安全 backend 边界

| 问题 | 备选项 | 选择 |
|------|--------|------|
| 第 4 阶段的安全 backend vtable 应该怎么组织？ | 统一 security backend / 拆成 DTLS-SRTP-crypto / 你决定 | 统一 security backend |
| 参考后端在第 4 阶段应该做到什么级别？ | 测试后端 + 可选适配骨架 / 必须接入真实库 / 只做测试后端 | 测试后端 + 可选适配骨架 |
| backend 的内存和句柄所有权应如何约束？ | 核心提供固定 storage / backend 自己管理内存 / 用户自带 backend 实例 | 核心提供固定 storage |
| DTLS outgoing datagram 应该如何输出？ | 复用 `observer.on_datagram` / 新增 `on_dtls_datagram` / backend 自己发送 | 复用 `observer.on_datagram` |

**用户选择：** `1.1=A, 1.2=A, 1.3=A, 1.4=A`

**备注：** 用户先要求说明推荐理由，随后确认全部推荐项。

---

## 握手启动与角色语义

| 问题 | 备选项 | 选择 |
|------|--------|------|
| DTLS 握手什么时候启动？ | ICE connected 后自动启动 / 用户显式调用 `start_dtls` / 你决定 | ICE connected 后自动启动 |
| DTLS role 如何决定？ | 由 SDP setup + ICE/JSEP 角色推导 / 用户配置 local DTLS role / 只固定一种 role | 由 SDP setup + ICE/JSEP 角色推导 |
| 收到 DTLS datagram 但 ICE 还没 connected 怎么处理？ | 拒绝并 trace，不缓存 / 暂存到固定小队列 / 直接丢弃不报错 | 拒绝并 trace，不缓存 |
| DTLS close/error 是否自动影响 ICE？ | 只影响 DTLS/SRTP 状态，不回滚 ICE / DTLS 失败自动 ICE failed / 用户决定是否关闭 ICE | 只影响 DTLS/SRTP 状态，不回滚 ICE |

**用户选择：** `2.1=A, 2.2=A, 2.3=A, 2.4=A`

---

## Fingerprint 与证书策略

| 问题 | 备选项 | 选择 |
|------|--------|------|
| 本地证书和 fingerprint 来源？ | backend 生成/持有证书，核心向 backend 查询 fingerprint / 用户继续 create-time 提供 fingerprint / 两者都支持 | backend 生成/持有证书，核心向 backend 查询 fingerprint |
| Fingerprint 算法范围？ | 首版只支持 SHA-256 / 支持 SHA-256-SHA-384-SHA-512 / backend 自报任意算法 | 首版只支持 SHA-256 |
| 远端 fingerprint mismatch 怎么处理？ | DTLS 失败，输出稳定错误和 trace，不继续 SRTP / 只 warning，继续连接 / 交给 backend 决定 | DTLS 失败，输出稳定错误和 trace，不继续 SRTP |
| 本地 SDP 何时使用真实 fingerprint？ | createOffer/createAnswer 使用 backend fingerprint / 仍使用 config.sdp.dtls_fingerprint / 直到 DTLS 启动才填充 | createOffer/createAnswer 使用 backend fingerprint |

**用户选择：** `3.1=A, 3.2=A, 3.3=A, 3.4=A`

---

## SRTP key export 与 protect API

| 问题 | 备选项 | 选择 |
|------|--------|------|
| DTLS 握手完成后 key export 由谁触发？ | 核心自动触发 key export 并初始化 SRTP/SRTCP / 用户显式调用 export/init / 等第 5 阶段首次发媒体时懒初始化 | 核心自动触发 key export 并初始化 SRTP/SRTCP |
| SRTP protect/unprotect 在第 4 阶段暴露成什么接口？ | 先做内部子系统接口，不新增公共媒体 API / 暴露公共 protect_rtp-unprotect_rtp / 只在 backend vtable 暴露，不建内部 wrapper | 先做内部子系统接口，不新增公共媒体 API |
| SRTP context 数量怎么限定？ | 固定 1 个 BUNDLE transport context，覆盖 RTP/RTCP audio/video / audio-video 各自 context / 按 m-line 动态创建 | 固定 1 个 BUNDLE transport context，覆盖 RTP/RTCP audio/video |
| protect/unprotect 失败如何处理 packet？ | 失败包丢弃/不输出，记录错误、trace、counter / unprotect 失败仍上报原始包 / 忽略失败继续 | 失败包丢弃/不输出，记录错误、trace、counter |

**用户选择：** `4.1=A, 4.2=A, 4.3=A, 4.4=A`

---

## 错误、trace 与测试后端

| 问题 | 备选项 | 选择 |
|------|--------|------|
| backend 错误映射要多细？ | 公共 status 粗粒度 + detail/trace 细粒度 / 为每种安全失败新增 public status / 只返回 BACKEND_ERROR | 公共 status 粗粒度 + detail/trace 细粒度 |
| DTLS/SRTP observer 状态怎么表达？ | 阶段级状态 + 细节 trace / observer 暴露所有安全细节事件 / 只 trace，不发状态 | 阶段级状态 + 细节 trace |
| 测试后端必须覆盖哪些路径？ | 成功 + 四类关键失败 / 只覆盖成功和 fingerprint mismatch / 覆盖完整真实 DTLS 状态机 | 成功 + 四类关键失败 |
| 第 4 阶段是否要做 Chrome 真实握手验收？ | 不要求，留到第 6 阶段；第 4 阶段做 deterministic backend 测试 / 必须和 Chrome 完成真实 DTLS 握手 / 手工 smoke test 即可 | 不要求，留到第 6 阶段；第 4 阶段做 deterministic backend 测试 |

**用户选择：** `5.1=A, 5.2=A, 5.3=A, 5.4=A`

---

## 智能体的自由裁量

- 具体 vtable 函数名、结构体字段名、内部文件拆分、trace event 名称全集、detail code 枚举值、测试 fixture 命名和真实库适配对象留给后续 researcher/planner。

## 延后事项

- Chrome 真实 DTLS 握手验收留到第 6 阶段。
- 多 fingerprint 算法、多 transport / 多 m-line SRTP context、公共 protect/unprotect 用户 API、强制真实第三方库适配均不纳入第 4 阶段上下文。
