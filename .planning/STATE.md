---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 06
status: executing_phase_6
last_updated: "2026-05-13T11:16:11Z"
progress:
  total_phases: 6
  completed_phases: 5
  total_plans: 19
  completed_plans: 15
  percent: 79
---

# State: libmicrortc

**Initialized:** 2026-05-12
**Current Phase:** 06
**Workflow Mode:** yolo
**Granularity:** standard
**Parallelization:** true

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-05-12)

**Core value:** 把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。
**Current focus:** Phase 06 — chrome-e2e

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
| 5. H264/Opus 媒体路径 | Complete | 9 | 5/5 plans; C fixture media verifier passed |
| 6. Chrome 自动化 E2E 与测试收口 | In Progress | 13 | 2/6 plans executed; Chrome host transport repair inserted before media E2E |

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

Continue Phase 6:

```bash
$gsd-execute-phase 6 --wave 3
```

---
*Last updated: 2026-05-13 after inserting Phase 6 Plan 03 transport repair*

## Performance Metrics

| Phase | Plan | Duration | Notes |
|-------|------|----------|-------|
| Phase 05-h264-opus P01 | 8min | 4 tasks | 10 files |
| Phase 05-h264-opus P02 | 8min | 5 tasks | 14 files |
| Phase 05-h264-opus P03 | 10min | 4 tasks | 9 files |
| Phase 05-h264-opus P04 | 13min | 4 tasks | 13 files |
| Phase 05-h264-opus P05 | 11min | 4 tasks | 13 files |
| Phase 06-chrome-e2e P01 | 11min | 4 tasks | 9 files |
| Phase 06-chrome-e2e P02 | 16min | 4 tasks | 6 files |

## Decisions

- [Phase 05]: 05-01: 媒体 public API 采用 MRTC 命名的 AWS 薄裁剪模型，不暴露 AWS/PIC 类型。
- [Phase 05]: 05-01: write_frame 当前只完成校验并在 media transport 未就绪时返回 MRTC_STATUS_INVALID_STATE，真实发送留给后续 Phase 5 计划。
- [Phase 05]: 05-01: SDP media m-line 由 PeerConnection transceiver list 驱动，同时保留 DataChannel application m-line。
- [Phase 05]: 05-02: H264 helper 仅接受 Annex-B bytestream；无 start code 或 AVCC-like length-prefixed 输入返回 MRTC_STATUS_PARSE_ERROR。
- [Phase 05]: 05-02: Opus timestamp 明确按 MRTC_FRAME.presentation_ts 的 100ns 单位转换为 48kHz RTP timestamp。
- [Phase 05]: 05-02: RTCP helpers 先提供 SR/RR/NACK/PLI packet primitive，不在本计划接入 PeerConnection 行为编排。
- [Phase 05]: 05-03: SRTP wrapper 在缺少系统 libsrtp 时保持 ready 但以 mrtc_srtp_session_is_passthrough() 明确标记非加密 fallback。
- [Phase 05]: 05-03: media send hook 保持 private，不把 application signaling、采集或编码引入核心库。
- [Phase 05]: 05-03: 接收侧先实现顺序 H264 FU-A accumulation 和 Opus per-packet delivery，不搬迁完整 KVS jitter buffer。
- [Phase 05]: 05-04: RTP rolling buffer 保存已 protected RTP packet bytes，NACK 重发不重新 SRTP protect 旧 sequence。
- [Phase 05]: 05-04: 未知 SSRC 和缺失 sequence 走确定性非崩溃路径，缺包通过 missing_count/counter 暴露。
- [Phase 05]: 05-04: RTCP Plan 04 仅实现 SR/RR/NACK/PLI 最小行为，不实现 REMB/TWCC/FIR/SLI 或拥塞控制。
- [Phase 05]: 05-05: Phase 5 完成标准采用固定 H264/Opus fixture C harness；Chrome 自动化 browser media E2E 明确保留在 Phase 6。
- [Phase 05]: 05-05: H264 fixture 采用 Annex-B SPS/PPS/IDR bytestream，Opus fixture 采用 big-endian 16-bit length-prefixed packet 序列，不引入解码器或容器解析。
- [Phase 05]: 05-05: installed static package 在导出 OpenSSL link dependency 时同步生成 find_dependency(OpenSSL)。
- [Phase 06]: 06-01: Playwright/Node 依赖只固定在 tests/e2e，根目录不创建 package.json。
- [Phase 06]: 06-01: WebSocket signaling server 只存在于 tests/e2e，C answerer 通过 stdio JSON line 接入，核心库不新增 signaling 依赖。
- [Phase 06]: 06-01: 默认 CTest 不注册 Chrome/Playwright E2E；浏览器验收保持显式 npm 命令。
- [Phase 06]: 06-02: C answerer 主路径采用 stdin/stdout JSON-line 协议，继续由 tests/e2e signaling bridge 持有应用层 signaling。
- [Phase 06]: 06-02: DataChannel label/id 以 read-only public accessor 暴露，避免 demo 直接读取 private struct。
- [Phase 06]: 06-02: 媒体 fixture 发送在 example target 内使用现有 private media send hook 统计 RTP 包，不把该 hook 暴露到 public API。
- [Phase 06]: 06-02: --self-test-media-callbacks 通过 protected RTP loopback 证明 on_frame 回调层统计，不只停留在 write_frame 调用。
- [Phase 06]: 06-03 前置修复: Chrome host E2E 不能把 C demo 内部 connected/datachannel.open 当作浏览器互通成功；先补真实 host transport，再执行媒体 E2E。
