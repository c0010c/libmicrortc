# 路线图：WebRTC 纯 C 库

**创建日期：** 2026-05-10
**最近更新：** 2026-05-11
**项目模式：** 水平层
**粒度：** 标准
**核心价值：** 在固定内存、无线程、跨平台约束下，稳定完成与 Chrome 的 1v1 音视频 `PeerConnection` 互通。

## 里程碑

- ✅ **v1.0 Chrome 1v1 MVP** — 阶段 1-6，35/35 个计划完成，2026-05-11 发货。完整归档见 [v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md)，需求归档见 [v1.0-REQUIREMENTS.md](milestones/v1.0-REQUIREMENTS.md)，审计记录见 [v1.0-MILESTONE-AUDIT.md](milestones/v1.0-MILESTONE-AUDIT.md)。
- 📋 **下一里程碑** — 待 `$gsd-new-milestone` 定义需求、研究和阶段路线图。

## 已发货范围

<details>
<summary>✅ v1.0 Chrome 1v1 MVP（阶段 1-6）— SHIPPED 2026-05-11</summary>

- [x] 第 1 阶段：核心骨架与边界契约，4/4 plans。
- [x] 第 2 阶段：SDP/JSEP 与 Offer/Answer，4/4 plans。
- [x] 第 3 阶段：ICE/STUN 与 Datagram 网络层，4/4 plans。
- [x] 第 4 阶段：DTLS-SRTP 安全传输，6/6 plans。
- [x] 第 5 阶段：RTP/RTCP 媒体平面，6/6 plans。
- [x] 第 6 阶段：Chrome 端到端验收，11/11 plans。

v1.0 完成纯 C 静态库、固定 arena/limits、三执行器模型、SDP/JSEP、Full ICE + STUN、可插拔 DTLS/SRTP backend、Opus/H264 RTP/RTCP 媒体平面、本地 Chrome 页面、WebSocket 信令示例和 secure full E2E 验收。库仍保持不创建线程、不直接操作 socket、不处理编解码、不引入 GPL/LGPL 依赖的项目边界。

已知审计技术债：v1.0 审计状态为 `gaps_found`，原因是第 1、3、4 阶段缺少严格工作流要求的 `*-VERIFICATION.md`；现有 UAT、SUMMARY、源码测试和 secure full E2E 证据支持产品目标已达成。该 gap 已由用户选择作为技术债接受后继续归档。

</details>

## 进度

| 里程碑 | 阶段 | 计划 | 需求 | 状态 | 完成日期 |
|---|---:|---:|---:|---|---|
| v1.0 Chrome 1v1 MVP | 6/6 | 35/35 | 48/48 | 已发货 | 2026-05-11 |

## 下一步

运行 `$gsd-new-milestone` 开始下一轮里程碑：重新定义 fresh `.planning/REQUIREMENTS.md`、研究目标范围并生成新的路线图阶段。
