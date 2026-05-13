---
phase: 06-chrome-e2e
plan: "07"
subsystem: chrome-e2e-transport-gap
tags: [stun, ice, dtls, sctp, datachannel, chrome, transport-smoke]
requires:
  - phase: 06-chrome-e2e
    provides: 06-03 SDP mid/order、真实 host candidate 和分层 transport smoke
provides:
  - STUN Binding request/response helper 覆盖 USERNAME、XOR-MAPPED-ADDRESS、MESSAGE-INTEGRITY 和 FINGERPRINT
  - answerer 主循环中的 PeerConnection UDP/STUN polling 入口
  - OpenSSL self-signed certificate SHA-256 fingerprint 生成
  - DataChannel/SCTP connect-time loopback open/message 假阳性移除
affects: [phase-06, chrome-e2e, ice, dtls, sctp, datachannel, transport]
tech-stack:
  added: []
  patterns: [strict-smoke-does-not-trust-remote-diagnostics, transport-stage-classification, no-addIceCandidate-datachannel-open]
key-files:
  created:
    - .planning/phases/06-chrome-e2e/06-07-SUMMARY.md
  modified:
    - src/stun/stun_message.c
    - src/stun/stun_message.h
    - src/ice/ice_agent.c
    - src/ice/ice_agent.h
    - src/dtls/dtls_session.c
    - src/sctp/sctp_session.c
    - src/sctp/sctp_session.h
    - src/data_channel/data_channel.h
    - src/peer_connection.c
    - include/micrortc/peer_connection.h
    - examples/chrome-e2e/mrtc_chrome_answerer.c
    - tests/transport/test_stun_message.c
    - tests/transport/test_sctp_data_channel.c
    - tests/peer_connection/test_peer_connection_api.c
    - .planning/phases/01-/SOURCE-MANIFEST.md
    - .planning/debug/chrome-transport-smoke-blocks.md
key-decisions:
  - "06-07: addIceCandidate 不能再触发 DataChannel open/message；browser 互通只由本地浏览器 connectionState 和 pong 判定。"
  - "06-07: classification smoke 可以记录 blocked；MRTC_E2E_REQUIRE_TRANSPORT=1 仍必须因 connection/datachannel blocked 非零失败。"
  - "06-07: 当前环境缺 libsrtp/usrsctp，strict system transport configure 不能通过，不能宣称真实 Chrome transport 完成。"
patterns-established:
  - "answerer stdin loop 必须同时轮询 PeerConnection transport，避免 signaling 阻塞 UDP/STUN 收包。"
  - "SCTP/DataChannel fallback 不得本地 loopback 伪造 browser pong。"
requirements-addressed: [E2E-03, E2E-04, E2E-05, TEST-02, TEST-04]
requirements-completed: []
duration: partial
completed: 2026-05-13
---

# Phase 06 Plan 07: Chrome Transport Gap Closure Summary

**STUN/ICE polling and transport false-positive guards were hardened, but strict Chrome connected/pong remains blocked at the browser connection stage.**

## Performance

- **Duration:** partial execution
- **Started:** 2026-05-13T12:34:00Z
- **Completed:** 2026-05-13T12:52:18Z
- **Tasks:** 5 planned tasks, partially completed
- **Files modified:** 16 tracked files plus this summary

## Accomplishments

- Added STUN Binding request parsing and Binding success response generation with transaction-id preservation, USERNAME, XOR-MAPPED-ADDRESS, MESSAGE-INTEGRITY and FINGERPRINT coverage.
- Extended the host ICE endpoint with UDP polling and selected remote address tracking, and added `mrtc_peer_connection_poll_transport()` for answerer-driven transport pumping.
- Updated `mrtc_chrome_answerer` to select over stdin while polling PeerConnection transport, so UDP/STUN activity no longer requires a new signaling line.
- Changed DTLS fingerprint generation to hash an OpenSSL self-signed certificate instead of returning random bytes.
- Removed the most dangerous SCTP/DataChannel false positive: `mrtc_sctp_session_connect()` no longer immediately emits DCEP open/message, and `addIceCandidate` no longer opens the DataChannel.
- Updated transport tests so local C unit coverage matches the stricter “no fake browser pong” behavior.

## Task Commits

1. **Task 06-07 partial: transport gap probes and false-positive hardening** - `ba022b9` (feat)

## Files Created/Modified

- `src/stun/stun_message.c` / `src/stun/stun_message.h` - Expanded STUN helpers for credentialed Binding request parsing and Binding success response generation.
- `src/ice/ice_agent.c` / `src/ice/ice_agent.h` - Added host endpoint polling and STUN response path.
- `src/dtls/dtls_session.c` - Uses OpenSSL certificate digest for local fingerprint when OpenSSL is available.
- `src/sctp/sctp_session.c` / `src/sctp/sctp_session.h` - Stops connect-time DCEP/message loopback and adds explicit DCEP/message receive helpers.
- `src/data_channel/data_channel.h` / `src/peer_connection.c` - Tracks channel owner, adds PeerConnection transport polling, and keeps DataChannel closed until real transport-driven open.
- `include/micrortc/peer_connection.h` - Exposes `mrtc_peer_connection_poll_transport()`.
- `examples/chrome-e2e/mrtc_chrome_answerer.c` - Polls transport while waiting for JSON lines.
- `tests/transport/test_stun_message.c`, `tests/transport/test_sctp_data_channel.c`, `tests/peer_connection/test_peer_connection_api.c` - Updated coverage for STUN auth/fingerprint, DCEP gating and no fake open/send.
- `.planning/phases/01-/SOURCE-MANIFEST.md` - Records local KVS reference paths used by this plan.
- `.planning/debug/chrome-transport-smoke-blocks.md` - Records the current strict smoke blocker.

## Decisions Made

- Kept strict smoke honest: no classification pass is treated as plan success.
- Did not vendor or fake `libsrtp`/`usrsctp`; strict configure remains the dependency gate.
- Preserved default CTest behavior in non-strict environments, while keeping `MRTC_E2E_REQUIRE_TRANSPORT=1` as the real Chrome gate.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] answerer exited before offer because transport poll saw no bound UDP socket**
- **Found during:** classification Chrome transport smoke
- **Issue:** The answerer polled transport before local description created the host endpoint, causing an invalid-state exit and signaling failure.
- **Fix:** `mrtc_peer_connection_poll_transport()` now returns OK when no endpoint is bound yet.
- **Files modified:** `src/peer_connection.c`
- **Verification:** classification smoke advanced past signaling and wrote `signaling: "passed"`.
- **Committed in:** `ba022b9`

**2. [Rule 3 - Blocking] STUN parse failures killed the answerer**
- **Found during:** classification Chrome transport smoke
- **Issue:** Chrome STUN packets that did not pass the current minimal validator caused answerer termination.
- **Fix:** host endpoint polling now ignores non-accepted STUN checks instead of failing the process; strict success is still not claimed.
- **Files modified:** `src/ice/ice_agent.c`
- **Verification:** classification smoke passed; strict smoke remained blocked at connection.
- **Committed in:** `ba022b9`

---

**Total deviations:** 2 auto-fixed (Rule 3)
**Impact on plan:** Both fixes prevented false negatives/crashes during diagnostics. They did not complete true DTLS/SCTP Chrome interop.

## Issues Encountered

- Strict transport dependencies are missing on this machine: `cmake -S . -B build-transport -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` fails because `libsrtp` and `usrsctp` are not found.
- Strict Chrome smoke still fails as designed: `connection: "blocked"`, `datachannel: "blocked"`, `failure_stage: "connection"`.
- DTLS packet BIO handshake, DTLS-SRTP export from a real handshake and usrsctp-over-DTLS packet IO are not complete.
- Because Self-Check failed, no Phase 6 requirements are marked complete by this plan.

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON && cmake --build build -j2` - passed.
- `ctest --test-dir build -R "stun|ice|dtls|srtp|sctp|data_channel|peer_connection|transport" --output-on-failure` - passed, 6/6 tests.
- `ctest --test-dir build --output-on-failure` - passed, 18/18 tests.
- `cmake -S . -B build-transport -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` - failed as expected in this environment, missing `libsrtp` and `usrsctp`.
- `MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` - passed in classification mode.
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` - failed at strict connection gate.
- `rg -n '"connection": ?"passed"|"datachannel": ?"passed"|pong:' tests/e2e/artifacts` - no strict passed/pong evidence found.

## User Setup Required

Install system `libsrtp` and `usrsctp` development packages before rerunning strict transport configure. OpenSSL is present in this environment.

## Next Phase Readiness

06-04 remains blocked for its original media-over-real-browser-transport goal. The next closure needs real OpenSSL DTLS packet BIO handshake, DTLS-SRTP key export from the completed handshake, and usrsctp/DCEP packet IO over DTLS before browser media assertions should proceed.

## Self-Check: FAILED

- Local build and C tests passed.
- Classification Chrome smoke passed and cleanly reports the blocker.
- Plan-level success criteria did not pass: browser `RTCPeerConnection.connectionState` did not reach `connected`, and browser did not receive `pong:<nonce>`.
- Strict system transport configure also cannot run to completion on this machine because `libsrtp` and `usrsctp` are missing.

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-13*
