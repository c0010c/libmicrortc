# WebRTC 纯 C 库

## 这是什么

这是一个面向性能受限设备的生产级 WebRTC 纯 C 库。首版提供完整 `PeerConnection` 抽象，负责 SDP/JSEP、ICE、DTLS-SRTP、RTP/RTCP，并以接近浏览器 WebRTC API 的方式与 Chrome 完成 1v1 音视频通话互通。

库本身不处理音视频编解码、不创建线程、不直接操作 socket。用户负责提供固定 arena、执行器、平台能力、UDP 收发和 H264/Opus 编解码输入输出。

## 核心价值

在固定内存、无线程、跨平台约束下，稳定完成与 Chrome 的 1v1 音视频 `PeerConnection` 互通。

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] 提供完整 `PeerConnection` 生命周期，支持发起和接听，即 `createOffer`、`createAnswer`、`setLocalDescription`、`setRemoteDescription`、`addIceCandidate` 等核心流程。
- [ ] 支持 Chrome 1v1 最小 SDP/JSEP 画像，包括 BUNDLE、rtcp-mux、DTLS fingerprint/setup、ICE 参数、H264、Opus 和 trickle ICE。
- [ ] 实现 Full ICE + STUN，支持 host/srflx candidate，不支持 TURN。
- [ ] 通过可插拔 backend vtable 集成 DTLS、SRTP 和 crypto 能力。
- [ ] 支持 RTP/RTCP 音视频传输，覆盖 H264 access unit、Opus frame、SR/RR、SDES、PLI，并解析上报 NACK。
- [ ] 使用固定 arena 和 limits，在创建阶段完成内存切分，创建成功后运行期不动态增长。
- [ ] 使用 `signaling`、`media`、`network` 三类 `post + timer` 执行器抽象，库内部不创建线程。
- [ ] 通过 datagram 输入输出 API 与用户提供的 UDP/socket 层集成。
- [ ] 提供事件、计数器和 trace hook，首版即可支撑生产排障。
- [ ] 使用 CMake 交付静态库，并提供本地 Chrome 页面和信令示例完成端到端验收。

### Out of Scope

- 音视频编解码 — 用户输入和接收 H264 access unit 与 Opus frame，库只负责 RTP payload format、时间戳、序列号和 SRTP。
- 内部线程和事件循环 — 库通过执行器抽象投递任务和定时器，线程模型由用户决定。
- socket 创建和网络 I/O — 用户负责 UDP/socket 收发，库只处理 datagram。
- 运行期动态内存增长 — 所有对象、队列、候选、包缓存和 SDP 缓冲区在创建阶段按 limits 固定。
- TURN — 首版只支持 host/srflx 和 STUN。
- 拥塞控制闭环 — 首版可解析、统计或 trace 相关 RTP header extension，但不做码率控制、发送节奏控制或带宽估计。
- RTP NACK 重传 — 首版解析并上报 NACK，不执行重传。
- 广泛 SDP 兼容 — 首版聚焦 Chrome 1v1 最小互通画像。
- GPL/LGPL 依赖 — 依赖策略优先选择宽松许可。

## Context

项目的首版边界来自 `docs/000-设计边界记录.md`。目标不是构建浏览器级完整 WebRTC 栈，而是在嵌入式或性能受限环境中提供一个可集成、可排障、可控内存和线程行为的 WebRTC 连接库。

核心对象应保持不透明句柄风格，公共 API 使用 `rtc_status_t` 返回码表达错误。内部需要按 SDP/JSEP、ICE、DTLS-SRTP、RTP/RTCP、媒体帧和可观测性等子系统拆分，避免 `PeerConnection` 演化成不可维护的巨大状态机。

发送方向用户提交 H264 access unit 或 Opus frame；接收方向库输出 H264 access unit 或 Opus frame。H264 发送仅支持 single NALU / FU-A，接收有限支持 STAP-A。媒体规模为单个 `PeerConnection` 最多 1 路音频和 1 路视频，媒体方向重点支持 `sendrecv` / `recvonly`。

验收方式是本地 Chrome 页面加信令示例完成 1v1 音视频通话。首版需要能通过事件、计数器和 trace hook 解释 ICE、DTLS、SRTP、RTP/RTCP、SDP/JSEP 等关键状态与失败原因。

## Constraints

- **语言**: 纯 C — 目标平台和集成场景要求低运行时依赖。
- **交付形态**: CMake 静态库 — 便于跨平台和嵌入式集成。
- **浏览器互通**: Chrome 1v1 音视频通话 — 首版成功标准聚焦单一明确互通目标。
- **内存模型**: 用户传入总 arena 和 limits — 创建阶段一次性切分，运行期不得动态增长。
- **执行模型**: 库内部不创建线程 — 用户通过 `signaling`、`media`、`network` 执行器决定调度。
- **网络边界**: 不直接操作 socket — 用户负责 UDP 收发，库负责 datagram 解析和输出。
- **媒体边界**: 不处理 H264/Opus 编解码 — 库只在编码帧和 RTP/RTCP 之间转换。
- **加密边界**: DTLS/SRTP/crypto 使用 backend vtable — 避免核心 API 绑定特定第三方库。
- **依赖策略**: 宽松许可优先，避免 GPL/LGPL — 降低商用和嵌入式集成风险。
- **可观测性**: 首版必须包含事件、计数器和 trace hook — 生产排障不是后补功能。

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| 首版提供完整 `PeerConnection` | 用户不需要直接编排 ICE、DTLS、SRTP、RTP/RTCP 和 SDP/JSEP 状态机 | — Pending |
| 库自身不处理编解码 | 降低复杂度和依赖，保持对嵌入式平台友好 | — Pending |
| 固定 arena + limits 内存模型 | 运行期内存可预测，适合性能受限设备 | — Pending |
| 三执行器亲和模型 | 保留无线程库边界，同时允许用户映射到多线程、单线程或 superloop | — Pending |
| 用户负责 UDP/socket 收发 | 避免平台 I/O 假设，保持跨平台边界清晰 | — Pending |
| Chrome 最小 SDP/JSEP 画像 | 首版聚焦可验收互通，不追求泛化 SDP 兼容 | — Pending |
| Full ICE + STUN，暂不支持 TURN | 控制首版复杂度，同时满足基础 NAT 场景 | — Pending |
| DTLS/SRTP/crypto 使用 backend vtable | 保持依赖和许可策略可替换 | — Pending |
| 首版不做拥塞控制闭环 | 避免引入复杂反馈控制，先保证基本互通与可观测性 | — Pending |
| NACK 解析并上报但不重传 | 暴露网络质量信息，同时不扩大首版发送缓存和调度复杂度 | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `$gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `$gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-10 after initialization*
