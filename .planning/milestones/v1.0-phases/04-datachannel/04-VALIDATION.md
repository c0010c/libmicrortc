---
phase: 04
slug: datachannel
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-12
---

# Phase 04 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + C99 executable tests |
| **Config file** | `CMakeLists.txt`; real network config `./mrtc-ice-servers.local.json` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure && ./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json` |
| **Estimated runtime** | Unit suite < 60s; real network verification depends on STUN/TURN latency |

## Sampling Rate

- **After every task commit:** Run `cmake --build build && ctest --test-dir build --output-on-failure` when the build directory exists; otherwise run the configure step first.
- **After every plan wave:** Run the full unit suite and any integration harness available for that wave.
- **Before `$gsd-verify-work`:** Full suite plus real network verification must be green in the developer environment.
- **Max feedback latency:** No more than one task may land without a runnable CTest or harness assertion.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 1 | PROTO-01/02/03/05 | T-04-01 | Core target links only allowed protocol dependencies, not AWS signaling or credential/storage | build + grep | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && rg -n "kvsWebrtcSignalingClient\|src/source/Signaling\|libwebsockets\|AWSSDK\|credential\|storage" CMakeLists.txt include src tests` | W0 | pending |
| 04-02-01 | 02 | 2 | API-05 | T-04-02 | Public API exposes MRTC DataChannel handles/callbacks without AWS/PIC types | unit + grep | `ctest --test-dir build -R mrtc_peer_connection_api_test --output-on-failure && rg -n "PCHAR\|PVOID\|BOOL\|(^|[^A-Z_])STATUS([^A-Z_]|$)" include/micrortc` | W0 | pending |
| 04-02-02 | 02 | 2 | PROTO-02 | T-04-03 | SDP role and fingerprint are generated from local cert and parsed from remote SDP | unit | `ctest --test-dir build -R "sdp|dtls" --output-on-failure` | W0 | pending |
| 04-03-01 | 03 | 2 | PROTO-01/NET-01/NET-02/NET-03 | T-04-04 | Missing real config fails; configured host/STUN/TURN paths produce host/srflx/relay candidates | integration | `./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json` | W0 | pending |
| 04-04-01 | 04 | 3 | PROTO-02/PROTO-03 | T-04-05 | DTLS role comes from SDP `setup`, remote fingerprint is verified, SRTP session is created from exported keys | integration | `ctest --test-dir build -R "dtls|srtp" --output-on-failure` | W0 | pending |
| 04-05-01 | 05 | 4 | API-05/PROTO-05 | T-04-06 | DataChannel open/send/receive/close succeeds over real ICE/DTLS/SCTP for text and binary messages | integration | `./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json --require-datachannel` | W0 | pending |

## Threat Model Summary

| Threat Ref | Threat | Mitigation Required In Plans |
|------------|--------|------------------------------|
| T-04-01 | Accidental AWS/KVS signaling or credential/storage dependency enters core target | Boundary grep and CMake target review in every dependency/migration task. |
| T-04-02 | Public header leaks AWS/PIC ABI types | Public API grep plus package consumer compile. |
| T-04-03 | DTLS role or fingerprint validation is hardcoded or skipped | SDP-driven role tests and remote fingerprint negative test. |
| T-04-04 | TURN credential is committed in docs/config | Only placeholder example config; local config ignored; docs avoid real secrets. |
| T-04-05 | SRTP session initialized with wrong key direction | DTLS role-aware key mapping test. |
| T-04-06 | DataChannel tests pass via loopback only, not real transport | Final harness must route DataChannel over ICE/DTLS/SCTP selected pair. |

## Wave 0 Requirements

- [ ] `tests/transport/` or equivalent integration test directory exists.
- [ ] `mrtc-ice-servers.example.json` exists with placeholder values only.
- [ ] `mrtc-ice-servers.local.json` is ignored by git.
- [ ] `mrtc_phase4_network_verify` has a missing-config test that exits non-zero.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real STUN/TURN relay availability | NET-02, NET-03 | Requires developer-provided server and secret credential | Put real values in `./mrtc-ice-servers.local.json`, then run `./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json` and confirm host/srflx/relay/DataChannel assertions pass. |

## Validation Sign-Off

- [ ] All tasks have automated verify or Wave 0 dependencies.
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify.
- [ ] Wave 0 covers all MISSING references.
- [ ] No watch-mode flags.
- [ ] Feedback latency < 60s for unit suite.
- [ ] `nyquist_compliant: true` set in frontmatter.

**Approval:** pending

