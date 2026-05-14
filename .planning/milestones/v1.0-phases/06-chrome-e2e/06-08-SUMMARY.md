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
  - strict Chromium transport smoke passed with browser connection and DataChannel ping/pong
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
  - "06-08: 严格依赖门槛已复核通过，strict Chrome smoke 作为最终完成判据已通过。"
  - "06-08: addIceCandidate 不再设置 selected_pair_ready，不再启动 DTLS/SCTP。"
  - "06-08: STUN HMAC、ICE username、SCTP AF_CONN port 和远端 DataChannel stream id 均按 Chrome 实际互通修正。"
requirements-addressed: [E2E-03, E2E-04, E2E-05, TEST-02, TEST-04]
requirements-completed: [E2E-03, E2E-04, E2E-05, TEST-02, TEST-04]
duration: 45 min
completed: 2026-05-14
---

# Phase 06 Plan 08: 真实 DTLS/SCTP Transport 依赖解锁与严格 Smoke 收口 Summary

**OpenSSL DTLS packet BIO、DTLS-SRTP exporter、SCTP/DataChannel packet API 和 PeerConnection packet pump 已接通到真实 Chromium transport；严格 smoke 的 browser connection 与 DataChannel ping/pong 均通过。**

## Performance

- **Duration:** 45 min
- **Started:** 2026-05-14T02:27:14Z
- **Completed:** 2026-05-14T03:02:11Z
- **Tasks:** 5/5 completed；06-08-05 strict smoke passed
- **Files modified:** transport core, smoke spec, debug docs and this summary

## Accomplishments

- 复核 06-08-01：严格 configure 已通过，`MRTC_SRTP_LIBRARY=/usr/lib/x86_64-linux-gnu/libsrtp2.so`，`MRTC_USRSCTP_LIBRARY=/usr/lib/x86_64-linux-gnu/libusrsctp.so`。
- 实现 DTLS session packet API：`start`、`handle_inbound_packet`、`drain_outbound_packet`、`is_connected`、application data callback 和 SRTP exporter。
- DTLS connected 现在依赖 OpenSSL handshake 完成和 peer certificate SHA-256 fingerprint 匹配；handshake 前 exporter 返回 invalid state。
- SCTP/DataChannel 层新增 inbound packet API、DCEP OPEN/ACK 处理、text/binary/empty PPID 选择和全局 init/deinit refcount 覆盖。
- PeerConnection 不再由 `addIceCandidate()` 伪造 selected transport；非 STUN DTLS packet 进入 DTLS session，DTLS application data 进入 SCTP/DataChannel。
- 追加修复严格 Chrome 互通缺口：STUN `MESSAGE-INTEGRITY` HMAC 覆盖长度修正、ICE username `local:remote` 匹配、SCTP `sockaddr_conn` 端口使用网络字节序、远端 DCEP stream id 绑定到复用 channel。
- 严格 Chrome smoke 已通过，browser 本地 `pc.connected`、`datachannel.open` 和 `pong:<nonce>` 均出现；06-04 可以继续执行媒体断言计划。

## Task Commits

1. **Task 06-08-02: OpenSSL DTLS packet handshake** - `5cf1c47` (feat)
2. **Task 06-08-03: SCTP/DataChannel packet path** - `11149e2` (feat)
3. **Task 06-08-04: PeerConnection DTLS/SCTP pump** - `7f5ee08` (feat)
4. **Task 06-08-05: strict smoke evidence** - `1a90b79` (docs)
5. **Task 06-08-05 follow-up: strict Chromium transport fixes** - current docs/code follow-up commit

Prior 06-08 dependency-gate commits still apply: `56b22bb` and `18d99db`.

## Files Created/Modified

- `src/dtls/dtls_session.c` / `src/dtls/dtls_session.h` - OpenSSL DTLS 1.2 memory BIO session、fingerprint verify、application data 和 SRTP exporter。
- `src/sctp/sctp_session.c` / `src/sctp/sctp_session.h` - usrsctp 初始化、inbound packet、DCEP/PPID 分发和测试双端路由。
- `src/ice/ice_agent.c` / `src/ice/ice_agent.h` - selected ICE pair UDP send helper。
- `src/peer_connection.c` - STUN -> DTLS -> SCTP/DataChannel packet pump；移除 addIceCandidate fake ready。
- `src/stun/stun_message.c` - 修正 STUN `MESSAGE-INTEGRITY` HMAC 输入范围，与 Chrome connectivity check 互通。
- `tests/e2e/chrome-transport.spec.js` - 严格 smoke 在发送 ping 前等待 browser DataChannel 真正 open。
- `tests/transport/test_dtls_srtp.c` - 覆盖 memory BIO handshake、fingerprint mismatch、DTLS exporter 和 libsrtp。
- `tests/transport/test_sctp_data_channel.c` - 覆盖 DCEP、PPID、pre-connect failure 和 refcount lifecycle。
- `.planning/debug/chrome-transport-smoke-blocks.md` - 记录依赖解除后的 strict smoke failure evidence。

## Decisions Made

- 不把 C 单测通过等同于 06-08 完成；最终完成条件是 browser `connection: "passed"` 和 `datachannel: "passed"`，本次已满足。
- SCTP 双端 C 单测使用内部 test-frame 路由来稳定覆盖 PPID/DCEP 分发；该路径不作为 Chrome strict success 证据。
- 06-08 只解除 strict transport gate；06-04/05/06 仍需按各自计划执行媒体、TURN 和 v1 总验收。

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
**Impact on plan:** C 层覆盖可以前进；后续 follow-up 修复 strict Chrome smoke 后，真实 Chrome transport gate 完成。

## Issues Encountered

- 初次严格 Chromium smoke 在 browser connection stage 阻塞；加诊断后定位到 STUN HMAC 与 ICE username 匹配问题。
- 连接通过后，DataChannel 先后暴露两个问题：SCTP `sockaddr_conn.sconn_port` 未使用网络字节序导致 usrsctp 对 Chrome INIT 回 ABORT；修复后 pong 又发在本地默认 stream 0，而 Chrome 创建的 channel 在 stream 1。
- 以上问题均已修正；最终严格 smoke 出现 browser `datachannel.message`，内容为 `pong:<nonce>`。

## Verification

- `cmake -S . -B build-transport -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` - passed。
- `cmake --build build-transport -j2` - passed。
- `ctest --test-dir build-transport -R "stun|ice|dtls|srtp|sctp|data_channel|peer_connection|transport" --output-on-failure` - passed，6/6 tests。
- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON && cmake --build build -j2` - passed；默认构建没有引入 Node/Chrome/TURN 强制依赖。
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` - passed，1/1 tests。
- `git diff --check` - passed。

## Known Stubs

- `src/sctp/sctp_session.c` 的 `MRTC_SCTP_TEST_FRAME_MAGIC` 路由只服务本地 C 单测，不是 Chrome 互通路径；strict smoke 成功来自真实 usrsctp over DTLS packet path。

## User Setup Required

None - 本机已具备 OpenSSL/libsrtp/usrsctp strict transport dependencies。

## Next Phase Readiness

06-04 可以继续执行。下一步在已通过的真实 Chrome transport 上补浏览器媒体 stats、DOM/WebAudio 观测和 C 端 on_frame/媒体事件断言。

## Self-Check: PASSED

- 严格依赖、构建和 C transport tests 均通过。
- Plan-level success criteria 已通过：browser connection/datachannel 均为 passed，出现 `pong:<nonce>`。
- 06-04/05/06 尚未完成，但 strict transport gate 已解除。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-14*
