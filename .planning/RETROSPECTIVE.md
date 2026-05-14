# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.0 — libmicrortc v1

**Shipped:** 2026-05-14
**Phases:** 6 | **Plans:** 21 | **Tasks:** 66

### What Was Built

- 独立 CMake 静态库 `micrortc`，包含 install/export package、package consumer 和 CTest 骨架。
- AWS 风格裁剪后的 PeerConnection/SDP/ICE/DataChannel/transceiver public API。
- ICE/STUN/TURN、DTLS、SRTP、SCTP/DataChannel、RTP/RTCP、H264、Opus 的 v1 协议路径。
- Chrome/Chromium E2E 测试层：本地 WebSocket signaling bridge、C answerer demo、browser client 和 Playwright runner。
- `scripts/verify-v1.sh` 总验收入口，覆盖 build、CTest、host Chrome E2E 和显式 TURN relay E2E。

### What Worked

- 先锁定本地 `reflib/kvs-webrtc-sdk` 基线和来源规则，后续协议搬迁时边界更稳。
- 把 signaling 和 Playwright 固定在 `tests/e2e`，避免核心库被应用层职责污染。
- 严格区分 C 内部诊断和浏览器真实状态，避免把 fake connected / fake DataChannel open 当成互通成功。
- 最终 summary 使用结构化 JSON，归档和 milestone audit 可以直接引用机器可读证据。

### What Was Inefficient

- Phase 6 transport 曾经过早依赖内部状态，导致 06-03/06-07 出现 self-check failed 后再补严格 gate。
- milestone close 前缺 Phase 2/6 `VERIFICATION.md`，需要回填证据链；后续阶段完成时应同步生成 verification。
- `audit-open` 对 quick task summary 文件名有兼容坑，后续 quick task 最好同时保留 `SUMMARY.md` 或确保工具识别。

### Patterns Established

- Public API 保持 `MRTC_*` / `mrtc_*` 和 opaque handle，避免泄漏 AWS/PIC 基础类型。
- 核心库只处理编码后媒体帧；fixture、browser 页面、Node runner 和 signaling bridge 都属于 demo/test 层。
- 对真实网络能力使用 opt-in 本地配置：`mrtc-ice-servers.local.json` 不提交，缺配置时必须硬失败。
- 浏览器互通验收必须从 browser `connectionState`、DataChannel pong、RTCStats/DOM/WebAudio 和 C `on_frame` 多源取证。

### Key Lessons

1. 任何 “connected” 状态都要先问它来自谁：C 内部状态只能做诊断，不能替代浏览器本地状态。
2. TURN relay 验收不能只强制浏览器 relay；C 端也必须产生公网可达 relay candidate，并证明 selected pair 是 relay。
3. 媒体 E2E 要避免一次性 burst 被 stats 采样错过；短流和并发观察更稳定。
4. 归档前审计需要 SUMMARY、VERIFICATION、REQUIREMENTS 三源一致；少一个文件就会变成流程债。

### Cost Observations

- Model mix: not recorded.
- Sessions: multi-session milestone.
- Notable: 严格 transport gate 虽然增加了一轮返工，但防止了 browser E2E 假阳性，是本里程碑最值的一次收紧。

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | multi-session | 6 | 从规划文档驱动的协议剥离，演进到以 Chrome E2E 和 TURN relay 为最终验收证据。 |

### Cumulative Quality

| Milestone | Tests | Coverage | Notable Gate |
|-----------|-------|----------|--------------|
| v1.0 | CTest + Playwright E2E | 45/45 v1 requirements | `scripts/verify-v1.sh` and TURN relay opt-in |

### Top Lessons (Verified Across Milestones)

1. 协议栈剥离项目要把“构建边界”“协议行为”“真实互通”分层验证，单层通过不代表里程碑完成。
2. 文档归档和 evidence hygiene 不是事后杂务；它直接决定下一轮 agent 能不能稳定接续。
