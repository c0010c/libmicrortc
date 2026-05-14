# Milestones: libmicrortc

## v1.0 libmicrortc v1 (Shipped: 2026-05-14)

**Delivered:** 从本地 AWS KVS WebRTC C SDK 基线剥离出独立 C WebRTC 协议栈 v1，完成 Linux x86_64、Chrome、H264、Opus、DataChannel、TURN relay 和双向音视频自动化验收。

**Phases completed:** 1-6 (21 plans total)

**Key accomplishments:**

- 锁定本地 `reflib/kvs-webrtc-sdk` 基线，建立来源追溯、Apache-2.0/NOTICE 和第三方依赖合规策略。
- 建立独立 CMake 静态库、install/export package、package consumer 和中文构建/依赖边界文档。
- 剥离 PeerConnection、SDP、ICE/STUN/TURN、DTLS、SRTP、SCTP/DataChannel、RTP/RTCP、H264/Opus 等 v1 核心协议能力。
- 明确核心库边界：不内置应用层 signaling，不做媒体采集和编码，只处理编码后媒体帧。
- 建立 Chrome/Chromium 自动化 E2E：真实连接、DataChannel ping/pong、C→Chrome 和 Chrome→C H264/Opus 媒体流动均有断言。
- 建立显式 TURN relay E2E，并通过 selected relay candidate evidence 和 secret redaction 验证真实 relay 路径。
- 收口 `scripts/verify-v1.sh` 作为 v1 总验收入口，输出 `build/reports/mrtc-v1-summary.json` 机器可读证据。

**Stats:**

- 6 phases
- 21 plans
- 66 tasks
- 45/45 v1 requirements complete
- Milestone audit: passed

**Acceptance:**

```bash
scripts/verify-v1.sh
scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json
```

**Archive:**

- [v1.0 roadmap](./milestones/v1.0-ROADMAP.md)
- [v1.0 requirements](./milestones/v1.0-REQUIREMENTS.md)
- [v1.0 audit](./milestones/v1.0-MILESTONE-AUDIT.md)
- [v1.0 phase artifacts](./milestones/v1.0-phases/)

**What's next:** Run `$gsd-new-milestone` to define the next requirements and roadmap.

---
