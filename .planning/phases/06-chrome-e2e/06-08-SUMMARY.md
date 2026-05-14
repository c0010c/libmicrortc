---
phase: 06-chrome-e2e
plan: "08"
subsystem: chrome-e2e-strict-transport
tags: [dtls, srtp, sctp, datachannel, chrome, transport-smoke, dependency-gate]
requires:
  - phase: 06-chrome-e2e
    provides: 06-07 STUN/ICE polling 和 transport false-positive guard
provides:
  - strict transport system dependency gate re-verified with OpenSSL/libsrtp/usrsctp present
  - OpenSSL DTLS 1.2 memory BIO handshake with SHA-256 certificate fingerprint verification
  - DTLS-SRTP exporter coverage using libsrtp protect/unprotect in strict build
  - SCTP/DataChannel packet APIs with DCEP/PPID dispatch coverage
  - PeerConnection packet pump path from selected ICE pair into DTLS and SCTP/DataChannel
affects: [phase-06, chrome-e2e, dtls, srtp, sctp, datachannel, peer-connection, transport]
tech-stack:
  added: []
  patterns: [strict-dependency-gate-before-transport-claim, browser-state-is-final-gate, no-addIceCandidate-fake-ready]
key-files:
  created:
    - .planning/phases/06-chrome-e2e/06-08-SUMMARY.md
  modified:
    - src/dtls/dtls_session.c
    - src/dtls/dtls_session.h
    - src/sctp/sctp_session.c
    - src/sctp/sctp_session.h
    - src/ice/ice_agent.c
    - src/ice/ice_agent.h
    - src/peer_connection.c
    - tests/transport/test_dtls_srtp.c
    - tests/transport/test_sctp_data_channel.c
    - .planning/debug/chrome-transport-smoke-blocks.md
key-decisions:
  - "06-08: 严格依赖门槛已复核通过，但 strict Chrome smoke 仍是唯一完成判据。"
  - "06-08: addIceCandidate 不再设置 selected_pair_ready，不再启动 DTLS/SCTP。"
  - "06-08: Self-Check 维持 FAILED；06-04/05/06 继续阻塞，直到 browser connection/datachannel passed。"
requirements-addressed: [E2E-03, E2E-04, E2E-05, TEST-02, TEST-04]
requirements-completed: []
duration: 20 min
completed: 2026-05-14
---

# Phase 06 Plan 08: 真实 DTLS/SCTP Transport 依赖解锁与严格 Smoke 收口 Summary

**OpenSSL DTLS packet BIO、DTLS-SRTP exporter、SCTP/DataChannel packet API 和 PeerConnection packet pump 已落地到 C 测试层，但严格 Chromium smoke 仍阻塞在 browser connection stage。**

## Performance

- **Duration:** 20 min
- **Started:** 2026-05-14T02:27:14Z
- **Completed:** 2026-05-14T02:47:28Z
- **Tasks:** 5/5 attempted；06-08-05 未通过 strict smoke
- **Files modified:** 11 tracked files plus this summary

## Accomplishments

- 复核 06-08-01：严格 configure 已通过，`MRTC_SRTP_LIBRARY=/usr/lib/x86_64-linux-gnu/libsrtp2.so`，`MRTC_USRSCTP_LIBRARY=/usr/lib/x86_64-linux-gnu/libusrsctp.so`。
- 实现 DTLS session packet API：`start`、`handle_inbound_packet`、`drain_outbound_packet`、`is_connected`、application data callback 和 SRTP exporter。
- DTLS connected 现在依赖 OpenSSL handshake 完成和 peer certificate SHA-256 fingerprint 匹配；handshake 前 exporter 返回 invalid state。
- SCTP/DataChannel 层新增 inbound packet API、DCEP OPEN/ACK 处理、text/binary/empty PPID 选择和全局 init/deinit refcount 覆盖。
- PeerConnection 不再由 `addIceCandidate()` 伪造 selected transport；非 STUN DTLS packet 进入 DTLS session，DTLS application data 进入 SCTP/DataChannel。
- 严格 Chrome smoke 仍失败，已更新 debug 文档并保留 06-04 blocked。

## Task Commits

1. **Task 06-08-02: OpenSSL DTLS packet handshake** - `5cf1c47` (feat)
2. **Task 06-08-03: SCTP/DataChannel packet path** - `11149e2` (feat)
3. **Task 06-08-04: PeerConnection DTLS/SCTP pump** - `7f5ee08` (feat)
4. **Task 06-08-05: strict smoke evidence** - `1a90b79` (docs)

Prior 06-08 dependency-gate commits still apply: `56b22bb` and `18d99db`.

## Files Created/Modified

- `src/dtls/dtls_session.c` / `src/dtls/dtls_session.h` - OpenSSL DTLS 1.2 memory BIO session、fingerprint verify、application data 和 SRTP exporter。
- `src/sctp/sctp_session.c` / `src/sctp/sctp_session.h` - usrsctp 初始化、inbound packet、DCEP/PPID 分发和测试双端路由。
- `src/ice/ice_agent.c` / `src/ice/ice_agent.h` - selected ICE pair UDP send helper。
- `src/peer_connection.c` - STUN -> DTLS -> SCTP/DataChannel packet pump；移除 addIceCandidate fake ready。
- `tests/transport/test_dtls_srtp.c` - 覆盖 memory BIO handshake、fingerprint mismatch、DTLS exporter 和 libsrtp。
- `tests/transport/test_sctp_data_channel.c` - 覆盖 DCEP、PPID、pre-connect failure 和 refcount lifecycle。
- `.planning/debug/chrome-transport-smoke-blocks.md` - 记录依赖解除后的 strict smoke failure evidence。

## Decisions Made

- 不把 C 单测通过等同于 06-08 完成；最终完成条件仍是 browser `connection: "passed"` 和 `datachannel: "passed"`。
- SCTP 双端 C 单测使用内部 test-frame 路由来稳定覆盖 PPID/DCEP 分发；该路径不作为 Chrome strict success 证据。
- 不标记任何 Phase 6 requirement complete，不更新 06-04/05/06 为可执行完成态。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] usrsctp 本地双端单测无法直接从最小 AF_CONN wrapper 产出可控双端 packet**
- **Found during:** Task 06-08-03
- **Issue:** 最小 usrsctp wrapper 在无真实远端时不能稳定驱动两个本地 session 的 association packet exchange。
- **Fix:** 保留 usrsctp-backed socket/inbound API，同时为单元测试加仅在显式 remote session 下使用的 test-frame packet 路由，覆盖 DCEP/PPID 行为；Chrome strict 不信任该路径。
- **Files modified:** `src/sctp/sctp_session.c`, `src/sctp/sctp_session.h`, `tests/transport/test_sctp_data_channel.c`
- **Verification:** `ctest --test-dir build-transport -R "sctp|data_channel" --output-on-failure`
- **Committed in:** `11149e2`

---

**Total deviations:** 1 auto-fixed (Rule 3)
**Impact on plan:** C 层覆盖可以前进，但 strict Chrome smoke 仍未通过，因此不声明真实 Chrome transport 完成。

## Issues Encountered

- 严格 Chromium smoke 失败：`tests/e2e/artifacts/summary.json` 为 `signaling: "passed"`、`connection: "blocked"`、`datachannel: "blocked"`、`failure_stage: "connection"`。
- browser artifact 显示 answer/candidate 已交换，browser 进入 `ice.checking` / `pc.connecting` 后超时；没有 `pc.connected`、没有 `pong:<nonce>`。
- 剩余 blocker 已从系统依赖缺口收窄到真实 Chrome ICE/DTLS/SCTP 互通路径。

## Verification

- `cmake -S . -B build-transport -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` - passed。
- `cmake --build build-transport -j2` - passed。
- `ctest --test-dir build-transport -R "dtls|srtp|sctp|data_channel|peer_connection|transport" --output-on-failure` - passed，4/4 tests。
- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON && cmake --build build -j2` - passed；默认构建没有引入 Node/Chrome/TURN 强制依赖。
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` - failed，connection blocked。
- `rg -n '"connection": ?"passed"|"datachannel": ?"passed"|pong:' tests/e2e/artifacts` - 未找到 passed/pong 证据。

## Known Stubs

- `src/sctp/sctp_session.c` 的 `MRTC_SCTP_TEST_FRAME_MAGIC` 路由只服务本地 C 单测，不是 Chrome 互通路径；strict smoke 已证明不能把它当成功证据。

## User Setup Required

None - 本机已具备 OpenSSL/libsrtp/usrsctp strict transport dependencies。

## Next Phase Readiness

06-04 继续阻塞。下一步应直接针对 strict smoke 的 connection stage 做 packet-level 调试：确认 Chrome STUN selected pair 后 C 端是否实际收到 DTLS ClientHello、DTLS outbound 是否回到 browser、以及 browser 是否接受 answer `a=setup:active` 与当前 DTLS role。

## Self-Check: FAILED

- 严格依赖、构建和 C transport tests 均通过。
- Plan-level success criteria 未通过：browser connection/datachannel 没有 passed，未出现 `pong:<nonce>`。
- 06-04/05/06 不应标记完成。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-14*
