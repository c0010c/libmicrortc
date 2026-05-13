---
phase: 06-chrome-e2e
plan: "08"
subsystem: chrome-e2e-strict-transport
tags: [dtls, srtp, sctp, datachannel, chrome, transport-smoke, dependency-gate]
requires:
  - phase: 06-chrome-e2e
    provides: 06-07 STUN/ICE polling 和 transport false-positive guard
provides:
  - strict transport system dependency gate with aggregated missing dependency reporting
  - local KVS protocol source references for the next DTLS/SCTP implementation pass
affects: [phase-06, chrome-e2e, cmake, dtls, srtp, sctp, datachannel, transport]
tech-stack:
  added: []
  patterns: [strict-dependency-gate-before-transport-claim, no-fake-transport-success]
key-files:
  created:
    - .planning/phases/06-chrome-e2e/06-08-SUMMARY.md
  modified:
    - CMakeLists.txt
    - .planning/phases/01-/SOURCE-MANIFEST.md
    - .planning/debug/chrome-transport-smoke-blocks.md
key-decisions:
  - "06-08: 缺 libsrtp/usrsctp 时停止后续 DTLS/SCTP/PeerConnection packet pump 任务，不写入假 transport 实现。"
  - "06-08: 严格依赖门槛必须一次性汇总缺失项，避免只报告第一个缺失库后误导后续执行。"
requirements-addressed: [E2E-03, E2E-04, E2E-05, TEST-02, TEST-04]
requirements-completed: []
duration: 5 min
completed: 2026-05-13
---

# Phase 06 Plan 08: 真实 DTLS/SCTP Transport 依赖门槛 Summary

**严格 transport 依赖门槛已加固，但本机仍缺 `libsrtp` 和 `usrsctp`，因此 06-08 按计划停止在 06-08-01，后续真实 DTLS/SCTP/PeerConnection packet pump 未执行。**

## Performance

- **Duration:** 5 min
- **Started:** 2026-05-13T13:16:38Z
- **Completed:** 2026-05-13T13:21:00Z
- **Tasks:** 1 executed, 4 blocked by dependency gate
- **Files modified:** 4 tracked files including this summary

## Accomplishments

- Updated strict CMake dependency gate so `MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` aggregates all missing required transport dependencies before failing.
- Verified the current machine has OpenSSL available, but is missing `libsrtp` and `usrsctp`.
- Preserved the default low-dependency build path: normal configure/build still succeeds without Node, Chrome, TURN, libsrtp or usrsctp.
- Recorded Phase 06 Plan 08 local KVS reference sources in `SOURCE-MANIFEST.md` without claiming any direct implementation migration.
- Updated the Chrome transport debug note with the latest 06-08 dependency-gate evidence.

## Task Commits

1. **Task 06-08-01: strict transport dependency gate** - `56b22bb` (fix)

## Files Created/Modified

- `CMakeLists.txt` - Aggregates missing strict transport dependencies and prints Ubuntu/Debian development package guidance.
- `.planning/phases/01-/SOURCE-MANIFEST.md` - Adds Phase 06 Plan 08 reference-only records for local KVS DTLS, SRTP, SCTP and DataChannel sources.
- `.planning/debug/chrome-transport-smoke-blocks.md` - Records that 06-08 stopped at missing `libsrtp` / `usrsctp`.
- `.planning/phases/06-chrome-e2e/06-08-SUMMARY.md` - Captures this blocked execution result.

## Decisions Made

- Did not proceed to 06-08-02 through 06-08-05 because the plan explicitly requires stopping when strict system transport dependencies are missing.
- Did not mark any Phase 6 requirements complete; strict Chrome connection and DataChannel pong evidence still do not exist.
- Kept `06-04` blocked until 06-08 strict transport smoke can pass.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] strict configure reported only the first missing transport dependency**
- **Found during:** Task 06-08-01 strict configure gate
- **Issue:** CMake failed at `libsrtp` before also reporting missing `usrsctp`, making the local setup blocker less actionable.
- **Fix:** Accumulated `OpenSSL`, `libsrtp` and `usrsctp` missing states and emitted one fatal error listing all missing dependencies.
- **Files modified:** `CMakeLists.txt`
- **Verification:** strict configure now reports `Missing required system transport dependencies: libsrtp, usrsctp`.
- **Committed in:** `56b22bb`

---

**Total deviations:** 1 auto-fixed (Rule 3)
**Impact on plan:** The dependency gate is clearer and safer. It does not complete real DTLS/SCTP transport.

## Issues Encountered

- `libsrtp` is not installed or not discoverable by CMake on this machine.
- `usrsctp` is not installed or not discoverable by CMake on this machine.
- Because strict configure failed, these planned tasks were not executed:
  - 06-08-02 OpenSSL DTLS 1.2 memory BIO handshake and SRTP exporter
  - 06-08-03 usrsctp association, DCEP OPEN/ACK and PPID packet path
  - 06-08-04 PeerConnection packet pump to DTLS/SCTP/SRTP
  - 06-08-05 strict Chrome transport smoke pass and resolved evidence

## Verification

- `cmake -S . -B build-transport -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` - failed as expected, now reporting `libsrtp` and `usrsctp` together.
- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON` - passed; default configure does not require strict transport dependencies.
- `cmake --build build -j2` - passed.
- `git diff --check` - passed.
- Not run: `cmake --build build-transport`, transport CTest filter, strict Chrome transport smoke. These are blocked until `libsrtp` and `usrsctp` development dependencies are available.

## User Setup Required

Install system development packages for OpenSSL, libsrtp and usrsctp before rerunning strict transport verification. On Ubuntu/Debian, the expected package names are `libssl-dev`, `libsrtp2-dev` and `libusrsctp-dev`.

## Next Phase Readiness

06-04 remains blocked. The next 06-08 execution should start after system dependencies are installed, then continue with real OpenSSL DTLS packet BIO, DTLS-SRTP key export, usrsctp/DCEP packet IO and the PeerConnection packet pump before attempting strict Chrome media E2E.

## Self-Check: FAILED

- Strict transport dependency gate behaved correctly and did not allow a false pass.
- Default configure/build still works.
- Plan-level success criteria did not pass: strict Chrome connection and DataChannel `pong:<nonce>` evidence are still absent.

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-13*
