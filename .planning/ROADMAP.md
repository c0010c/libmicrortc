# Roadmap: libmicrortc

**Created:** 2026-05-12
**Last updated:** 2026-05-14 after v1.0 milestone archive
**Mode:** standard
**Granularity:** standard

## Milestones

- ✅ **v1.0 libmicrortc v1** — Phases 1-6 shipped on 2026-05-14. Full archive: [v1.0-ROADMAP.md](./milestones/v1.0-ROADMAP.md), [v1.0-REQUIREMENTS.md](./milestones/v1.0-REQUIREMENTS.md), [v1.0-MILESTONE-AUDIT.md](./milestones/v1.0-MILESTONE-AUDIT.md), [v1.0-phases/](./milestones/v1.0-phases/).
- 📋 **Next milestone** — not planned yet. Start with `$gsd-new-milestone`.

## Completed Milestone Summary

<details>
<summary>✅ v1.0 libmicrortc v1 (Phases 1-6) — SHIPPED 2026-05-14</summary>

| Phase | Name | Plans | Status |
|-------|------|-------|--------|
| 1 | 基线、范围与合规边界 | 1/1 | Complete |
| 2 | 独立构建与库骨架 | 1/1 | Complete |
| 3 | AWS 风格 API 与 signaling-free PeerConnection | 1/1 | Complete |
| 4 | 传输、安全与 DataChannel 协议核心 | 5/5 | Complete |
| 5 | H264/Opus 媒体路径 | 5/5 | Complete |
| 6 | Chrome 自动化 E2E 与测试收口 | 8/8 | Complete |

**Delivered:** 从本地 AWS KVS WebRTC C SDK 基线剥离出可独立构建、可自动验证的 C WebRTC 协议栈 v1，覆盖 Linux x86_64、Chrome、H264、Opus、DataChannel、TURN relay 和双向音视频 E2E。

**Acceptance entrypoints:**

```bash
scripts/verify-v1.sh
scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json
```

**Canonical evidence:** `build/reports/mrtc-v1-summary.json`

</details>

## Progress

| Milestone | Phases | Plans | Requirements | Status | Shipped |
|-----------|--------|-------|--------------|--------|---------|
| v1.0 libmicrortc v1 | 6/6 | 21/21 | 45/45 | Complete | 2026-05-14 |

## Next Step

Run `$gsd-new-milestone` to define the next milestone requirements and roadmap.

---
*For detailed v1.0 phase history, see `.planning/milestones/v1.0-phases/`.*
