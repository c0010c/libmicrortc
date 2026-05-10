---
phase: 04-dtls-srtp-secure-transport
plan: 05
subsystem: security
tags: [c, dtls-srtp, diagnostics, security-backend, tdd]

requires:
  - phase: 04-dtls-srtp-secure-transport
    provides: DTLS/SRTP backend runtime、fingerprint verification、SRTP wrapper
provides:
  - public `RTC_SECURITY_DETAIL_*` detail code 枚举
  - 安全 backend 失败到稳定 `rtc_status_t` 的统一映射
  - deterministic backend 全错误矩阵，覆盖 status、observer detail、trace reason 和 counter
affects: [phase-04, security, srtp, observability]

tech-stack:
  added: []
  patterns: [stable-status-detail-diagnostics, deterministic-security-error-matrix]

key-files:
  modified: [include/rtc/security.h, src/security/security.c, src/srtp/srtp.c, tests/test_security.c]

key-decisions:
  - "不扩展 `rtc_status_t`；DTLS/SRTP/fingerprint 细节通过 `RTC_SECURITY_DETAIL_*`、trace reason 和 counters 表达。"
  - "backend handshake/create/start/input 失败统一映射为 `RTC_STATUS_BACKEND_ERROR` + `RTC_SECURITY_DETAIL_HANDSHAKE_FAILED` + `handshake_failed`。"
  - "SRTP/SRTCP unprotect 的认证/重放失败保持 `RTC_STATUS_PROTOCOL_ERROR`，detail 使用 `RTC_SECURITY_DETAIL_SRTP_REPLAY_FAILED`。"

patterns-established:
  - "安全失败测试必须同时断言 public status、observer subsystem/operation/detail、trace reason 和 counter。"
  - "SRTP/SRTCP protect/unprotect wrapper 不把 backend 细节泄漏到 public status。"

requirements-completed: [SEC-05]

duration: 5min
completed: 2026-05-10
---

# Phase 4 Plan 05: 安全错误映射与 Deterministic Backend 矩阵 Summary

**安全 backend 失败现在被收口到稳定 public status，并通过 detail code、trace reason 和 counters 提供细粒度诊断。**

## Performance

- **Duration:** 5 分钟
- **Started:** 2026-05-10T13:59:20Z
- **Completed:** 2026-05-10T14:04:18Z
- **Tasks:** 1
- **Files modified:** 4

## Accomplishments

- 在 `include/rtc/security.h` 新增 `rtc_security_detail_code_t`，覆盖 `HANDSHAKE_FAILED`、`FINGERPRINT_MISMATCH`、`KEY_EXPORT_FAILED`、`SRTP_INIT_FAILED`、`SRTP_PROTECT_FAILED`、`SRTP_UNPROTECT_FAILED` 和 `SRTP_REPLAY_FAILED`。
- 统一 DTLS backend create/start/input/event 失败诊断：public status 保持 `RTC_STATUS_BACKEND_ERROR`，observer detail 使用 `RTC_SECURITY_DETAIL_HANDSHAKE_FAILED`，trace reason 使用 `handshake_failed`。
- 保留 fingerprint mismatch 的 `RTC_STATUS_PROTOCOL_ERROR` 映射，并用 `RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH`、`fingerprint_mismatch` 和 counter 锁定行为。
- SRTP/SRTCP protect/unprotect wrapper 现在输出对应 detail code；backend 失败映射为 `RTC_STATUS_BACKEND_ERROR`，认证/重放失败映射为 `RTC_STATUS_PROTOCOL_ERROR`。
- deterministic backend 测试矩阵扩展到成功 handshake + key export + `srtp.ready`、fingerprint mismatch、handshake backend error、key export failure、SRTP init failure、SRTP protect failure、SRTP/SRTCP unprotect failure 和 replay failure。

## Task Commits

1. **Task 1 RED:** `f3b3b74` test(04-05): add failing security error matrix tests
2. **Task 1 GREEN:** `c5ee7d2` feat(04-05): map security failures to stable diagnostics

## Files Created/Modified

- `include/rtc/security.h` - 新增 public security detail code 枚举。
- `src/security/security.c` - 统一 DTLS/backend 失败 reason、detail 和 status 映射。
- `src/srtp/srtp.c` - 为 protect/unprotect/replay 失败输出 observer detail code，并统一 SRTCP unprotect reason。
- `tests/test_security.c` - 扩展 deterministic backend 状态，新增/更新全错误矩阵断言。

## Decisions Made

- 没有给 `rtc_status_t` 新增 DTLS/SRTP/fingerprint 细粒度 public status。
- detail code 暴露在 `include/rtc/security.h`，因为 observer `on_error` 已经有 `detail_code` 参数，SEC-05 要求用户能稳定诊断安全失败细节。
- SRTCP unprotect backend failure 使用同一个 reason `srtp_unprotect_failed`，避免把 SRTCP 作为新的 public 失败类别扩张。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：PASS，1/1 tests passed。
- 计划 automated grep：`RTC_SECURITY_DETAIL_HANDSHAKE_FAILED`、`handshake_failed`、`srtp_unprotect_failed` 全部命中。
- acceptance grep：`RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH`、`handshake_failed`、`key_export_failed`、`RTC_SECURITY_DETAIL_SRTP_INIT_FAILED` / `srtp_init_failed`、`srtp_protect_failed`、`srtp_unprotect_failed`、`srtp_replay_failed` 全部命中。
- `! grep -R "RTC_STATUS_.*FINGERPRINT\\|RTC_STATUS_.*SRTP\\|RTC_STATUS_.*DTLS" include/rtc/status.h`：PASS，未新增细粒度 public status。

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- RED 阶段按预期失败：测试引用的 `RTC_SECURITY_DETAIL_*` 尚未在 public header 中声明。
- GREEN 阶段发现 deterministic SRTCP fixture 复用了 RTP unprotect status，导致 SRTCP backend failure 未被正确模拟；作为 Rule 1 测试夹具 bug 已在同一 GREEN 提交中修正。
- `gsd-sdk` 不在 PATH，无法用 SDK 自动更新 STATE/ROADMAP/REQUIREMENTS；按 AGENTS.md 要求手工同步规划文档。

## User Setup Required

None - no external service configuration required.

## Known Stubs

None - 未发现 TODO/FIXME/placeholder/coming soon/not available 或阻塞本计划目标的空数据 stub。

## Threat Flags

无新增网络端点、认证路径、文件访问或 schema trust boundary。新增 public detail code 属于计划内安全诊断面。

## Next Phase Readiness

04-06 可以基于已完成的 SEC-01..SEC-05 做 Phase 4 文档、需求追踪和项目状态收口。第 5 阶段可以依赖 SRTP wrapper 的稳定错误语义接入 RTP/RTCP 媒体平面。

## TDD Gate Compliance

- RED commits: `f3b3b74`
- GREEN commits: `c5ee7d2`
- Gate sequence: PASSED

## Self-Check: PASSED

- `include/rtc/security.h`：FOUND
- `src/security/security.c`：FOUND
- `src/srtp/srtp.c`：FOUND
- `tests/test_security.c`：FOUND
- `.planning/phases/04-dtls-srtp-secure-transport/04-05-SUMMARY.md`：FOUND
- `f3b3b74`：FOUND
- `c5ee7d2`：FOUND

---
*Phase: 04-dtls-srtp-secure-transport*
*Completed: 2026-05-10*
