---
phase: 05
slug: h264-opus
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-13
---

# Phase 05 — Validation Strategy

> Phase 5 执行期间的反馈采样契约。目标是尽早发现媒体路径“只编译、未真正流动”的假阳性。

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + C99 executable tests |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure && ./build/tests/integration/mrtc_phase5_media_verify --fixtures ./tests/fixtures` |
| **Estimated runtime** | Unit suite < 60s; Phase 5 fixture verifier < 60s |

## Sampling Rate

- **After every task commit:** Run `cmake --build build && ctest --test-dir build --output-on-failure` when `build/` exists; otherwise configure first.
- **After every plan wave:** Run all available media unit tests plus the current `mrtc_phase5_media_verify` mode.
- **Before `$gsd-verify-work`:** Full suite and Phase 5 fixture verifier must be green.
- **Max feedback latency:** No more than one task may land without a runnable CTest or verifier assertion.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | API-04, MEDIA-01, MEDIA-02 | T-05-01 | Public media API exposes only MRTC/std C types and no capture/encoder API | unit + grep | `ctest --test-dir build -R "peer_connection_api" --output-on-failure && ! rg -n "PCHAR|PVOID|\\bBOOL\\b|\\bUINT64\\b|(^|[^A-Z_])STATUS([^A-Z_]|$)|capture|encoder|gstreamer" include/micrortc` | W0 | pending |
| 05-01-02 | 01 | 1 | API-04, MEDIA-03, MEDIA-05 | T-05-02 | SDP advertises H264/Opus transceivers with required fmtp/rtcp-fb | unit | `ctest --test-dir build -R "sdp|peer_connection" --output-on-failure` | W0 | pending |
| 05-02-01 | 02 | 1 | PROTO-04, MEDIA-03, MEDIA-05 | T-05-03 | RTP/H264/Opus primitives serialize valid packets and preserve timestamps | unit | `ctest --test-dir build -R "rtp|h264|opus" --output-on-failure` | W0 | pending |
| 05-03-01 | 03 | 2 | API-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06 | T-05-04 | SRTP media path sends encoded frames and invokes `on_frame` after receive | integration | `ctest --test-dir build -R "srtp|media" --output-on-failure` | W0 | pending |
| 05-04-01 | 04 | 3 | PROTO-04 | T-05-05 | NACK retransmits from rolling buffer and PLI invokes callback | unit + integration | `ctest --test-dir build -R "rtcp|nack|pli" --output-on-failure` | W0 | pending |
| 05-05-01 | 05 | 4 | MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06, MEDIA-07 | T-05-06 | Fixed H264/Opus fixture flow proves bidirectional encoded media movement | integration | `./build/tests/integration/mrtc_phase5_media_verify --fixtures ./tests/fixtures` | W0 | pending |

## Threat Model Summary

| Threat Ref | Threat | Mitigation Required In Plans |
|------------|--------|------------------------------|
| T-05-01 | Public API leaks AWS/PIC types or expands into capture/encoder responsibility | Header grep and package consumer compile. |
| T-05-02 | SDP remains minimal first-m-line echo and cannot negotiate H264/Opus | SDP unit tests assert exact H264/Opus rtpmap/fmtp/rtcp-fb lines. |
| T-05-03 | RTP packetizer passes trivial cases but fails FU-A/Annex-B boundaries | H264 fixture tests cover small NALU, large FU-A and invalid Annex-B input. |
| T-05-04 | SRTP wrapper reports ready without protecting media path | Integration test exercises RTP and RTCP protect/unprotect API and receive callback. |
| T-05-05 | NACK/PLI only parse but do not drive behavior | Tests assert retransmit callback count and `on_picture_loss` callback count. |
| T-05-06 | Phase 5 claims Chrome E2E completion prematurely | Final docs distinguish Phase 5 fixture harness from Phase 6 browser automation. |

## Wave 0 Requirements

- [ ] `tests/media/` exists with RTP/H264/Opus/RTCP unit tests.
- [ ] `tests/fixtures/h264_annexb_sample.h264` exists and contains Annex-B start code plus SPS/PPS before keyframe.
- [ ] `tests/fixtures/opus_packets.bin` exists and documents length-prefix format.
- [ ] `mrtc_phase5_media_verify` exists and fails clearly when fixture path is missing.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Optional Chrome smoke | MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06 | Full automated browser E2E belongs to Phase 6 | If a low-cost page/demo exists, use it only as supplemental evidence; do not replace fixture verifier. |

## Validation Sign-Off

- [ ] All tasks have automated verify or Wave 0 dependencies.
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify.
- [ ] Wave 0 covers all MISSING references.
- [ ] No watch-mode flags.
- [ ] Feedback latency < 60s for unit suite and fixture verifier.
- [ ] `nyquist_compliant: true` set in frontmatter.

**Approval:** pending
