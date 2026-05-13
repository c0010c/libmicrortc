---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 05
status: ready_to_execute_phase_5
last_updated: "2026-05-13T05:34:40.669Z"
progress:
  total_phases: 6
  completed_phases: 4
  total_plans: 13
  completed_plans: 10
  percent: 77
---

# State: libmicrortc

**Initialized:** 2026-05-12
**Current Phase:** 05
**Workflow Mode:** yolo
**Granularity:** standard
**Parallelization:** true

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-05-12)

**Core value:** 把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。
**Current focus:** Phase 05 — h264-opus

## Artifacts

| Artifact | Path | Status |
|----------|------|--------|
| Project context | `.planning/PROJECT.md` | Created |
| Workflow config | `.planning/config.json` | Created |
| Requirements | `.planning/REQUIREMENTS.md` | Created |
| Roadmap | `.planning/ROADMAP.md` | Created |
| State | `.planning/STATE.md` | Created |

## Roadmap Progress

| Phase | Status | Requirements | Progress |
|-------|--------|--------------|----------|
| 1. 基线、范围与合规边界 | Complete | 4 | 100% |
| 2. 独立构建与库骨架 | Complete | 4 | 100% |
| 3. AWS 风格 API 与 signaling-free PeerConnection | Complete | 7 | 100% |
| 4. 传输、安全与 DataChannel 协议核心 | Complete | 8 | 5/5 plans; real-network verifier passed |
| 5. H264/Opus 媒体路径 | In Progress | 9 | 2/5 plans complete |
| 6. Chrome 自动化 E2E 与测试收口 | Pending | 13 | 0% |

## Decisions To Carry Forward

- 以本地 `reflib/kvs-webrtc-sdk` 作为剥离基线。
- v1 API 先从 AWS 公共头文件裁剪。
- 核心库只处理编码后媒体帧，不做采集和编码。
- 先沿用 AWS 线程和回调模型。
- v1 先保证 Chrome，H264 + Opus，双向音视频。
- v1 包含 TURN relay 验收。
- demo/验收层尽可能自动化，但 signaling 不进入核心库。
- E2E 浏览器互通是 v1 验收核心。

## Next Step

Execute Phase 5:

```bash
$gsd-execute-phase 5
```

---
*Last updated: 2026-05-13 after Phase 5 planning*

## Performance Metrics

| Phase | Plan | Duration | Notes |
|-------|------|----------|-------|
| Phase 05-h264-opus P01 | 8min | 4 tasks | 10 files |
| Phase 05-h264-opus P02 | 8min | 5 tasks | 14 files |

## Decisions

- [Phase 05]: 05-01: 媒体 public API 采用 MRTC 命名的 AWS 薄裁剪模型，不暴露 AWS/PIC 类型。
- [Phase 05]: 05-01: write_frame 当前只完成校验并在 media transport 未就绪时返回 MRTC_STATUS_INVALID_STATE，真实发送留给后续 Phase 5 计划。
- [Phase 05]: 05-01: SDP media m-line 由 PeerConnection transceiver list 驱动，同时保留 DataChannel application m-line。
- [Phase 05]: 05-02: H264 helper 仅接受 Annex-B bytestream；无 start code 或 AVCC-like length-prefixed 输入返回 MRTC_STATUS_PARSE_ERROR。
- [Phase 05]: 05-02: Opus timestamp 明确按 MRTC_FRAME.presentation_ts 的 100ns 单位转换为 48kHz RTP timestamp。
- [Phase 05]: 05-02: RTCP helpers 先提供 SR/RR/NACK/PLI packet primitive，不在本计划接入 PeerConnection 行为编排。
