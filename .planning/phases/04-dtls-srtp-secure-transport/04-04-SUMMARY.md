---
phase: 04-dtls-srtp-secure-transport
plan: 04
subsystem: security
tags: [c, dtls-srtp, srtp, security-backend, tdd]

requires:
  - phase: 04-dtls-srtp-secure-transport
    provides: backend session runtime、DTLS handshake complete event、peer fingerprint verification gate
provides:
  - DTLS handshake complete 后自动导出 `EXTRACTOR-dtls_srtp` keying material
  - key export 成功后初始化单一 BUNDLE SRTP/SRTCP context 并进入 `srtp.ready`
  - key export / SRTP init 失败时进入安全失败并阻断 `srtp.ready`
  - 内部 RTP/RTCP SRTP/SRTCP protect/unprotect wrapper
affects: [phase-04, phase-05, security, srtp, media]

tech-stack:
  added: []
  patterns: [dtls-srtp-exporter-gate, internal-srtp-wrapper, deterministic-security-backend-fixture]

key-files:
  created: [src/srtp/srtp.h, src/srtp/srtp.c]
  modified: [CMakeLists.txt, src/security/security.h, src/security/security.c, tests/test_security.c]

key-decisions:
  - "DTLS-SRTP key block 长度固定为 `RTC_DTLS_SRTP_KEY_MATERIAL_BYTES`，首版按单一 BUNDLE transport 初始化一套 SRTP/SRTCP context。"
  - "protect/unprotect 仅作为 `src/srtp` 内部接口暴露，不新增 `include/rtc/peer_connection.h` public API。"
  - "unprotect 返回 `RTC_STATUS_PROTOCOL_ERROR` 时映射为 `srtp_replay_failed`，同时记录 replay 与 unprotect counter。"

patterns-established:
  - "handshake complete 事件顺序固定为 peer fingerprint verification -> key export -> SRTP init -> `dtls.connected`/`srtp.ready`。"
  - "SRTP wrapper 先检查 `pc->srtp_ready` 和输出 capacity，再调用 backend；失败路径只返回 status/counter/trace/error，不输出 datagram。"

requirements-completed: [SEC-03, SEC-04]

duration: 5min
completed: 2026-05-10
---

# Phase 4 Plan 04: DTLS-SRTP Key Export 与内部 SRTP Wrapper Summary

**DTLS handshake complete 经 fingerprint gate 后自动导出 SRTP keying material，初始化单一 BUNDLE SRTP/SRTCP context，并提供内部 protect/unprotect wrapper。**

## Performance

- **Duration:** 5 分钟
- **Started:** 2026-05-10T13:52:20Z
- **Completed:** 2026-05-10T13:56:08Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- `RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE` 现在先校验 peer fingerprint，再使用精确 label `EXTRACTOR-dtls_srtp` 导出固定长度 key block。
- key export 成功后调用 backend `init_srtp_context(session, key_block, RTC_DTLS_SRTP_KEY_MATERIAL_BYTES, pc->dtls_role)`，成功才输出 `dtls.connected` 与 `srtp.ready`。
- key export 失败和 SRTP init 失败都会进入 `dtls.failed`，递增对应 counter，并通过 observer error 与 trace reason `key_export_failed` / `srtp_init_failed` 诊断。
- 新增 `src/srtp` 内部 wrapper：`rtc_srtp_protect_rtp`、`rtc_srtp_unprotect_rtp`、`rtc_srtp_protect_rtcp`、`rtc_srtp_unprotect_rtcp`。
- deterministic backend 测试覆盖成功链路、fingerprint mismatch 阻断、key export/init 失败、not-ready、protect 失败和 unprotect replay/auth 失败。

## Task Commits

1. **Task 1 RED:** `cc1077c` test(04-04): add failing SRTP key export tests
2. **Task 1 GREEN:** `2913bd1` feat(04-04): export DTLS SRTP key material
3. **Task 2 RED:** `90bdaff` test(04-04): add failing SRTP wrapper tests
4. **Task 2 GREEN:** `c424a40` feat(04-04): add internal SRTP wrappers

## Files Created/Modified

- `src/security/security.h` - 定义内部 `RTC_DTLS_SRTP_KEY_MATERIAL_BYTES` 常量。
- `src/security/security.c` - 实现 handshake complete 后的 key export、SRTP context init、`srtp.ready` 和失败诊断。
- `src/srtp/srtp.h` - 新增内部 SRTP/SRTCP wrapper 声明。
- `src/srtp/srtp.c` - 新增 RTP/RTCP protect/unprotect wrapper、ready/capacity 检查和失败映射。
- `CMakeLists.txt` - 将 `src/srtp/srtp.c` 纳入静态库。
- `tests/test_security.c` - 扩展 deterministic backend 和安全错误矩阵测试。

## Decisions Made

- 没有新增 public `protect_rtp` / `unprotect_rtp` 用户 API；第 5 阶段媒体层将通过内部 `src/srtp` 接口调用。
- 缺失 backend key export/init/protect/unprotect 函数不在 create-time 直接拒绝，而是在对应安全操作发生时返回 backend error；这样不破坏只依赖 fingerprint/start DTLS 的既有测试 fixture。
- protect wrapper 本身不调用 `observer.on_datagram`，只返回受保护后的 packet 给调用方；这保证 protect 失败不会从 wrapper 泄漏明文 datagram。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：PASS，1/1 tests passed。
- Task 1 acceptance grep：`RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE`、`rtc_security_verify_peer_fingerprint`、`EXTRACTOR-dtls_srtp`、`init_srtp_context`、`srtp_init_failed`、`RTC_DTLS_SRTP_KEY_MATERIAL_BYTES`、`key_export_failed`、`srtp.ready`、`export_keying_material_calls` 全部命中。
- Task 2 acceptance grep：`rtc_srtp_protect_rtp`、`rtc_srtp_unprotect_rtp`、`rtc_srtp_protect_rtcp`、`rtc_srtp_unprotect_rtcp`、`size_t capacity`、`srtp_not_ready`、`srtp_protect_failed`、`srtp_unprotect_failed` / `srtcp_unprotect_failed`、`srtp_replay_failed` 全部命中。
- `! grep -R "protect_rtp\\|unprotect_rtp" include/rtc/peer_connection.h`：PASS，未新增 public PeerConnection protect/unprotect API。

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- RED 阶段按预期失败：Task 1 中 handshake complete 后未调用 key export；Task 2 中缺少 `src/srtp/srtp.h`。
- `gsd-sdk` 不在 PATH，无法用 SDK 自动更新 STATE/ROADMAP/REQUIREMENTS；按 AGENTS.md 要求手工同步规划文档。

## User Setup Required

None - no external service configuration required.

## Known Stubs

None - 未发现 TODO/FIXME/placeholder/coming soon/not available 或阻塞本计划目标的空数据 stub。

## Next Phase Readiness

04-05 可以在现有 security backend 错误矩阵基础上继续统一 backend error 映射。第 5 阶段 RTP/RTCP 媒体平面可以直接调用 `src/srtp` 内部 wrapper；wrapper 已保证未 ready、protect 失败和 unprotect 失败不会输出未保护或未认证数据。

## TDD Gate Compliance

- RED commits: `cc1077c`, `90bdaff`
- GREEN commits: `2913bd1`, `c424a40`
- Gate sequence: PASSED

## Self-Check: PASSED

- `src/security/security.h`：FOUND
- `src/security/security.c`：FOUND
- `src/srtp/srtp.h`：FOUND
- `src/srtp/srtp.c`：FOUND
- `tests/test_security.c`：FOUND
- `.planning/phases/04-dtls-srtp-secure-transport/04-04-SUMMARY.md`：FOUND
- `cc1077c`：FOUND
- `2913bd1`：FOUND
- `90bdaff`：FOUND
- `c424a40`：FOUND

---
*Phase: 04-dtls-srtp-secure-transport*
*Completed: 2026-05-10*
