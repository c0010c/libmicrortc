# 第 2 阶段：SDP/JSEP 与 Offer/Answer - 讨论日志

> **审计记录。** 不要把本文档作为 planning、research 或 execution agent 的输入。
> 决策已记录在 `02-CONTEXT.md`，本文档只保留讨论过的备选项。

**Date:** 2026-05-10T16:45:00+08:00
**Phase:** 2-SDP/JSEP 与 Offer/Answer
**Areas discussed:** SDP 画像与生成、JSEP 状态与错误、ICE/trickle 边界、SDP golden tests 策略

---

## 讨论范围选择

| Option | Description | Selected |
|--------|-------------|----------|
| 全部讨论 | 覆盖 SDP 画像、JSEP 状态、ICE 占位边界、golden 测试。 | ✓ |
| SDP 画像与生成 | 锁定 Chrome 最小 SDP 的 codec、m-line、direction、fingerprint/ICE 参数如何表达。 | |
| JSEP 状态与错误 | 锁定 setLocal/setRemote 的状态转换、非法顺序、错误与 observer/trace 表达。 | |

**User's choice:** 全部讨论。
**Notes:** 用户希望一次性锁定第 2 阶段主要灰区。

---

## SDP 生成固定程度

| Option | Description | Selected |
|--------|-------------|----------|
| 固定 Chrome 1v1 模板 | 只生成 1 路 audio + 1 路 video、BUNDLE、rtcp-mux、Opus、H264、`sendrecv`/`recvonly` 所需字段。 | ✓ |
| 固定模板 + 少量配置项 | 允许用户配置是否启用 audio/video、媒体方向、codec payload type 等。 | |
| 偏通用 SDP builder | 做成可扩展 builder，为以后多路媒体和更多 codec 预留。 | |

**User's choice:** 固定 Chrome 1v1 模板。
**Notes:** 本阶段聚焦 Chrome 最小互通画像，不做通用 SDP builder。

---

## ICE/DTLS 字段来源

| Option | Description | Selected |
|--------|-------------|----------|
| 由用户/API 配置提供占位值 | 生成 SDP 所需 `ice-ufrag`、`ice-pwd`、fingerprint、setup role 由用户 API 或测试夹具提供。 | ✓ |
| 本阶段内部生成测试用值 | 库生成临时 ICE 参数和 fingerprint 字符串。 | |
| 输出空占位或固定假值 | 最省实现，但 Chrome 互通意义弱。 | |

**User's choice:** 同意推荐选项：由用户/API 配置提供占位值。
**Notes:** 用户要求先说明推荐和理由。推荐理由是避免提前引入 crypto/random/backend，保持第 2 阶段只负责 SDP/JSEP 层。

---

## Codec 画像

| Option | Description | Selected |
|--------|-------------|----------|
| 固定 Opus + H264 baseline 画像 | audio 固定 Opus；video 固定 H264，包含 Chrome 常见 `rtpmap/fmtp/rtcp-fb` 最小集合。 | ✓ |
| Opus + H264，但 payload type 可配置 | codec 集合固定，payload type、profile-level-id 等少量字段允许配置。 | |
| 解析远端 codec 后再协商输出 | answer 根据 Chrome offer 做较完整的 codec intersection。 | |

**User's choice:** 固定 Opus + H264 baseline 画像。
**Notes:** 不做通用 codec 协商。

---

## 媒体方向

| Option | Description | Selected |
|--------|-------------|----------|
| 只支持 `sendrecv` 和 `recvonly` | 与需求 `SDP-04` 一致，遇到 `sendonly/inactive` 返回错误。 | ✓ |
| 解析四种方向，但只生成 `sendrecv/recvonly` | 对 Chrome SDP 更宽容，内部状态更多。 | |
| 完整支持四种方向 | `sendrecv/sendonly/recvonly/inactive` 都解析和生成。 | |

**User's choice:** 只支持 `sendrecv` 和 `recvonly`。
**Notes:** 用户选择保持需求内最小方向集合。

---

## 最小 JSEP/状态机

| Option | Description | Selected |
|--------|-------------|----------|
| 实现最小稳定状态机 | 支持 `stable`、`have-local-offer`、`have-remote-offer`，拒绝非法顺序。 | ✓ |
| 实现浏览器风格更多状态 | 加上 rollback/pranswer 等状态。 | |
| 只做顺序标记，不暴露完整状态 | 更快，但非法转换和排障较弱。 | |

**User's choice:** 实现最小稳定状态机。
**Notes:** 用户询问是否必须引入 JSEP。讨论结论是不引入完整浏览器 JSEP，但需要最小 JSEP-compatible offer/answer 状态契约。

---

## 非法状态转换反馈

| Option | Description | Selected |
|--------|-------------|----------|
| 稳定错误码 + observer/trace 细节 | API 返回稳定 `rtc_status_t`，observer error 和 trace 记录状态、operation 和原因。 | ✓ |
| 只返回错误码 | 实现简单，但排障弱。 | |
| 返回错误码 + 文本消息 | 文本友好但不适合作为稳定契约。 | |

**User's choice:** 稳定错误码 + observer/trace 细节。
**Notes:** 需要符合第 1 阶段可观测性契约。

---

## `addIceCandidate` 边界

| Option | Description | Selected |
|--------|-------------|----------|
| 只解析/保存 SDP candidate 字符串，不启动 ICE | 验证格式、受 `limits.ice.max_candidates` 约束、保存远端候选列表。 | ✓ |
| 继续返回 `RTC_STATUS_UNSUPPORTED` | 严格留给第 3 阶段，但 API-04 流程不完整。 | |
| 解析并做 priority/foundation 计算 | 更接近 ICE 实现，但进入第 3 阶段范围。 | |

**User's choice:** 只解析/保存 SDP candidate 字符串，不启动 ICE。
**Notes:** 保存远端 trickle candidate，但不做 connectivity checks、pair、STUN。

---

## 本地 candidate 输出

| Option | Description | Selected |
|--------|-------------|----------|
| 不生成本地 candidate，只保留 API/observer 契约 | 本地 gathering 属于第 3 阶段，`on_local_candidate` 不主动触发。 | ✓ |
| 允许用户注入本地 candidate 并序列化到 SDP | 便于非 trickle SDP，但需要新增本地 candidate 管理入口。 | |
| 生成 host candidate 占位 | 进入网络接口枚举/ICE 范围。 | |

**User's choice:** 不生成本地 candidate，只保留 API/observer 契约。
**Notes:** 用户要求说明推荐和理由。推荐理由是避免把网络接口、端口、candidate priority 和机器环境依赖拖入第 2 阶段。

---

## Golden tests 样本来源

| Option | Description | Selected |
|--------|-------------|----------|
| 固定仓库内 Chrome 样本 + 规范化比较 | 保存 Chrome offer/answer fixture、本地 expected；动态字段固定或规范化。 | ✓ |
| 运行时从 Chrome 抓 SDP | 更真实，但 CI 和本地环境复杂。 | |
| 只测关键字段，不做完整 golden | 测试更少，但容易漏掉行顺序和必需属性。 | |

**User's choice:** 固定仓库内 Chrome 样本 + 规范化比较。
**Notes:** 不依赖运行时 Chrome。

---

## 错误路径测试覆盖

| Option | Description | Selected |
|--------|-------------|----------|
| 覆盖结构错误 + 不支持字段 + 状态错误 | 覆盖缺失 BUNDLE/rtcp-mux、ICE/DTLS 字段、未知 codec、方向、非法顺序、容量限制。 | ✓ |
| 只覆盖解析失败和非法状态 | 测试量较少，但画像退化保护弱。 | |
| 主要测成功路径，错误路径留给后续阶段 | 快，但信令错误不可诊断。 | |

**User's choice:** 覆盖结构错误 + 不支持字段 + 状态错误。
**Notes:** 错误路径必须保护 SDP/JSEP 层边界和可诊断性。

---

## 智能体的自由裁量

- 内部文件名、parser/tokenizer 具体实现、fixture 目录命名、payload type 数值和 trace detail code 枚举值由 planner 在阶段边界内决定。

## 延后事项

- 完整 JSEP：rollback、pranswer、renegotiation。
- ICE/STUN：本地 candidate gathering、connectivity checks、candidate pair、STUN。
- DTLS/SRTP：fingerprint 真实生成、握手、key export。
- 通用 SDP builder、广泛 SDP 兼容、多路媒体、多 codec 协商。
