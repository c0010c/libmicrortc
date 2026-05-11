# 项目复盘

## Milestone: v1.0 — Chrome 1v1 MVP

**Shipped:** 2026-05-11  
**Phases:** 6  
**Plans:** 35  
**Requirements:** 48/48 v1 requirements completed

### What Was Built

v1.0 建成了生产取向的 WebRTC 纯 C `PeerConnection` MVP：公共 API、固定内存模型、执行器亲和、SDP/JSEP、ICE/STUN、DTLS-SRTP、RTP/RTCP、Chrome 页面、信令示例、C E2E 示例和 secure full E2E 验收都已经贯通。

### What Worked

- 按层推进的水平阶段拆分有效降低了协议栈复杂度：SDP/JSEP、ICE、DTLS-SRTP、RTP/RTCP 和 E2E 验收各自形成了清晰边界。
- 固定 arena、执行器亲和和用户负责 UDP/socket 收发的约束在每个阶段持续回归，避免了后期边界漂移。
- 第 6 阶段的 failure layering、JSONL summary、共享 `runId` 和 secure binary selection 让 Chrome E2E 问题定位速度显著提升。
- 可选 OpenSSL/libsrtp backend 默认关闭的策略保住了默认构建的低依赖边界，同时允许真实 secure E2E 验收。

### What Was Inefficient

- 第 1、3、4 阶段早期没有同步生成 `*-VERIFICATION.md`，导致里程碑审计在产品目标已达成后仍出现 artifact gate。
- 部分 Nyquist validation 文档 frontmatter 和表格状态没有随执行回写，形成额外文档一致性成本。
- 第 6 阶段真实安全依赖和 Chrome ICE/STUN authenticated Binding 直到后期才暴露，产生了 06-07 到 06-11 的 gap closure 链。

### Patterns Established

- Chrome E2E 的 compact JSON summary 必须同时包含页面状态、C 示例 summary、失败 layer、媒体产物和人工 gate 状态。
- 默认构建不得强制真实安全依赖；真实安全 backend 必须通过显式 CMake option 和用户级依赖路径启用。
- 安全、媒体和网络失败应保持 public status 粗粒度，细节通过 detail code、trace reason、counter 和 JSONL 分层诊断输出。

### Key Lessons

- 每个阶段完成后同步生成 `*-VERIFICATION.md`，比里程碑末尾补审计证据更便宜。
- E2E harness 要尽早共享 run id、统一日志 schema，并避免 stale media files 影响 pass/fail 判定。
- 对浏览器互通而言，STUN authenticated Binding request/response 和 peer-reflexive 候选处理应作为 Chrome E2E 的早期风险项。

### Cost Observations

- Sessions: 多轮阶段规划、执行、验证和 gap closure。
- Notable: 第 6 阶段后半段的成本主要来自真实环境和浏览器互通，不是核心 C API 形状。

## Cross-Milestone Trends

| Milestone | Shipped | Main Outcome | Main Debt |
|---|---|---|---|
| v1.0 | 2026-05-11 | Chrome 1v1 WebRTC 纯 C MVP 完成 | 早期阶段 verification artifact 缺口 |
