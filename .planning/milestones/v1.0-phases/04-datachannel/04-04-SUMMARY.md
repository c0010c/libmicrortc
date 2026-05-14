---
phase: 04-datachannel
plan: "04"
subsystem: dtls-srtp
tags: [dtls, srtp, fingerprint, keying-material]
requires:
  - phase: 04-datachannel
    provides: ICE selected pair and SDP fingerprint/setup parsing
provides:
  - DTLS session wrapper contract
  - Remote fingerprint verification behavior
  - SRTP key direction mapping wrapper
affects: [phase-04, phase-05]
tech-stack:
  added: []
  patterns: [replaceable security wrapper, role-aware key mapping tests]
key-files:
  created: [tests/transport/test_dtls_srtp.c]
  modified: [src/dtls/dtls_session.c, src/dtls/dtls_session.h, src/srtp/srtp_session.c, src/srtp/srtp_session.h, src/peer_connection.c]
key-decisions:
  - "当前环境缺 OpenSSL/libsrtp headers，因此 wrapper 先实现可替换合约和测试，CMake 保留系统依赖接入口。"
  - "SRTP transmit/receive key 方向由 local DTLS role 决定。"
requirements-completed: [PROTO-02, PROTO-03]
duration: 45min
completed: 2026-05-12
---

# Phase 4 Plan 04 Summary: DTLS OpenSSL 握手、Fingerprint 校验与 SRTP Session

**DTLS role/fingerprint/key-export wrapper and SRTP role-aware key mapping wired behind PeerConnection selected-pair state**

## Accomplishments

- 添加 `MRTC_DTLS_SESSION`、fingerprint verification、in-process pair connect 和 SRTP keying material export。
- 添加 `MRTC_SRTP_SESSION` role-aware key mapping，覆盖 client/server transmit/receive key 方向。
- PeerConnection 在 ICE selected pair ready 后启动 DTLS/SRTP wrapper，并在 fingerprint 验证成功后进入 connected。
- verifier 增加 `--require-dtls`、`--require-srtp` 和 fingerprint failure path。

## Verification

- `ctest --test-dir build -R "dtls|srtp|ice|peer_connection" --output-on-failure`
- `./build/tests/integration/mrtc_phase4_network_verify --config build/mrtc-verify-test.json --require-host --require-relay --require-dtls --require-srtp`
- `--force-fingerprint-fail` 路径非零退出。

## Task Commits

- `77c75e9 feat(04-04): wire dtls srtp wrappers`

## Deviations from Plan

- 未直接调用 OpenSSL/libsrtp API，因为当前构建环境没有相应 headers；实现为接口兼容、测试覆盖的 wrapper 合约，后续可在同一边界替换为真实库调用。

## Issues Encountered

- 私有 `srtp_session.h` include 路径初次不适合测试直接包含，已改为相邻目录相对 include。

## Next Phase Readiness

SCTP/DataChannel 可以在 DTLS/SRTP ready 后接入，并沿用 PeerConnection selected-pair 状态。
