---
phase: 2
slug: sdp-jsep-offer-answer
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-10
---

# Phase 2 — Validation Strategy

> 第 2 阶段执行期间使用的验证契约。所有自动化命令必须在本地 CMake/CTest 下运行，不依赖 Chrome 运行时。

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | 纯 C 自研 test runner + CTest |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~2 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **After every plan wave:** Run `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | API-04, SDP-01, SDP-02, SDP-03, SDP-04 | T-02-01 | SDP/JSEP buffers allocated from arena; no dynamic allocation | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W1 | ⬜ pending |
| 02-01-02 | 01 | 1 | SDP-01, SDP-02, SDP-03, SDP-04 | T-02-02 | Public config validates ICE/DTLS parameter lengths before use | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W1 | ⬜ pending |
| 02-02-01 | 02 | 2 | SDP-01, SDP-02, SDP-04 | T-02-03 | Serializer refuses undersized output buffers | golden/unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W2 | ⬜ pending |
| 02-02-02 | 02 | 2 | SDP-01, SDP-02 | T-02-03 | Generated SDP contains BUNDLE, rtcp-mux, trickle, Opus, H264 | golden | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W2 | ⬜ pending |
| 02-03-01 | 03 | 2 | SDP-03, SDP-04 | T-02-04 | Parser rejects malformed or unsupported SDP before state transition | unit/golden | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W2 | ⬜ pending |
| 02-03-02 | 03 | 2 | SDP-03, SDP-04 | T-02-04 | Chrome fixture normalizes to supported Opus/H264 profile summary | golden | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W2 | ⬜ pending |
| 02-04-01 | 04 | 3 | API-04 | T-02-05 | Illegal offer/answer ordering returns stable errors and trace | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W3 | ⬜ pending |
| 02-04-02 | 04 | 3 | API-04, SDP-01, SDP-02, SDP-03, SDP-04 | T-02-05 | Full caller/callee flows complete without ICE/DTLS execution | integration | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W3 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_sdp.c` — stubs and assertions for SDP-01, SDP-02, SDP-03, SDP-04.
- [ ] `tests/test_jsep.c` — stubs and assertions for API-04 state transitions.
- [ ] `tests/fixtures/chrome-offer-audio-video.sdp` — normalized Chrome SDP fixture.
- [ ] `tests/fixtures/expected-local-offer.sdp` — generated local offer golden.
- [ ] `tests/fixtures/expected-local-answer.sdp` — generated local answer golden.

---

## Manual-Only Verifications

All phase behaviors have automated verification.

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
