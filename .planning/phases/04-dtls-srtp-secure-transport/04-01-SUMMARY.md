---
phase: 04-dtls-srtp-secure-transport
plan: 01
subsystem: security
tags: [c, dtls, srtp, backend-vtable, counters, trace]

requires:
  - phase: 03-ice-stun-datagram-network-layer
    provides: STUN/DTLS/RTP/RTCP datagram demux 与 observer.on_datagram 输出边界
provides:
  - 单一 rtc_security_backend_vtable_t 公共安全 backend 契约
  - security backend event/callback 输出契约
  - DTLS/SRTP counters 与 trace 常量
  - deterministic backend 测试入口
affects: [phase-04, phase-05, security, dtls, srtp]

tech-stack:
  added: []
  patterns: [typed-security-backend-config, backend-event-callback, explicit-security-counters]

key-files:
  created: [include/rtc/security.h, tests/test_security.c]
  modified: [include/rtc/config.h, include/rtc/limits.h, include/rtc/counters.h, include/rtc/trace.h, src/observability/counters.c, CMakeLists.txt, tests/test_main.c, tests/test_peer_connection.c]

key-decisions:
  - "第 4 阶段 security backend 以单一 rtc_security_backend_vtable_t 暴露，不拆分 DTLS/SRTP/crypto 多个 public backend。"
  - "backend outgoing DTLS datagram 与 handshake-complete 只通过 rtc_security_backend_event_cb 回到核心。"
  - "DTLS session storage 容量以 max_session_storage_bytes 和 RTC_CAPACITY_RESOURCE_DTLS_SESSION 表达。"

patterns-established:
  - "security backend 函数返回粗粒度 rtc_status_t，细节保留给 event detail code、trace 和 counters。"
  - "测试后端入口从 tests/test_security.c 开始，后续计划可在同一文件扩展 deterministic matrix。"

requirements-completed: [SEC-01]

duration: 7min
completed: 2026-05-10
---

# Phase 4 Plan 01: 安全 Backend 契约 Summary

**单一 DTLS-SRTP security backend vtable、事件回调通道、固定 storage 容量入口与安全可观测性测试入口。**

## Performance

- **Duration:** 7 分钟
- **Started:** 2026-05-10T13:19:17Z
- **Completed:** 2026-05-10T13:26:05Z
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments

- 新增 `include/rtc/security.h`，定义 `rtc_security_backend_config_t`、DTLS role、backend event、event callback 和单一 `rtc_security_backend_vtable_t`。
- 将 `rtc_peer_connection_config_t.security_backend` 从 `void *` 类型化为 `const rtc_security_backend_config_t *`。
- 扩展 DTLS limits 和 capacity resource，用于后续 create-time fixed storage 诊断。
- 新增 DTLS/SRTP counters、trace 常量与 `tests/test_security.c` 默认测试入口。

## Task Commits

1. **Task 1 RED:** `4ab8fe2` test(04-01): add failing security backend contract test
2. **Task 1 GREEN:** `6a5372b` feat(04-01): define security backend contract
3. **Task 2 RED:** `f34addd` test(04-01): add failing security observability test
4. **Task 2 GREEN:** `9d6dc52` feat(04-01): register security counters and traces

## Files Created/Modified

- `include/rtc/security.h` - 新增 security backend vtable/config/event/callback 公共契约。
- `include/rtc/config.h` - 类型化 `security_backend`，新增 `RTC_CAPACITY_RESOURCE_DTLS_SESSION`。
- `include/rtc/limits.h` - 新增 `dtls.max_session_storage_bytes`。
- `include/rtc/counters.h` - 新增 `rtc_dtls_counters_t` 与 `rtc_srtp_counters_t`。
- `include/rtc/trace.h` - 新增 DTLS/SRTP trace 常量。
- `src/observability/counters.c` - 显式清零 DTLS/SRTP counters。
- `tests/test_security.c` - 新增 deterministic backend 契约、event 构造和 counters/trace 初始化测试。
- `tests/test_main.c` - 注册 `rtc_test_security`。
- `CMakeLists.txt` - 将 `tests/test_security.c` 加入默认测试目标。
- `tests/test_peer_connection.c` - 调整容量边界测试 arena 大小，保持新增 counters 后仍验证 STUN transaction 容量错误。

## Decisions Made

- 遵循计划使用单一 `rtc_security_backend_vtable_t`，覆盖 DTLS session/start/input、fingerprint、key export、单一 BUNDLE SRTP/SRTCP context 初始化和 SRTP/SRTCP protect/unprotect。
- `create_session` 接收固定 storage、`rtc_security_backend_event_cb` 和 callback user data，backend 不直接触达 observer 或 socket。
- 本计划只建立公共契约和测试入口，不新增 OpenSSL/libsrtp、socket、线程或运行期动态分配要求。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：PASS，1/1 tests passed。
- Task 1 grep 验收：`create_session`、`export_keying_material`、`init_srtp_context`、SRTP/SRTCP protect/unprotect 和 event 常量全部命中。
- Task 2 grep 验收：`rtc_dtls_counters_t`、`rtc_srtp_counters_t`、`handshake_started`、`fingerprint_mismatch`、`srtp_init_failed`、DTLS/SRTP trace 常量和 `rtc_test_security` 全部命中。
- Plan-level 验证：`! grep -R "on_dtls_datagram" include src tests` PASS，未新增 DTLS 专用 datagram observer 分叉。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 调整容量边界测试 arena 大小**
- **Found during:** Task 2
- **Issue:** 新增 DTLS/SRTP counters 后 `rtc_peer_connection_t` 变大，既有 `tests/test_peer_connection.c` 的 STUN transaction 容量测试先命中了 ICE candidate 容量错误。
- **Fix:** 将该测试的局部 arena 从 6500 调整为 6700，使它继续越过候选/配对容量并命中 STUN transaction 容量错误。
- **Files modified:** `tests/test_peer_connection.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `9d6dc52`

---

**Total deviations:** 1 auto-fixed（Rule 1）
**Impact on plan:** 只修正由本计划结构体扩展直接触发的测试边界漂移；未扩大功能范围。

### Workflow Deviations

**1. GSD SDK 不在 PATH**
- **Found during:** 状态更新
- **Issue:** `gsd-sdk query ...` 返回 `command not found`，无法使用 SDK 自动更新 STATE/ROADMAP/REQUIREMENTS。
- **Fix:** 按 AGENTS.md 的规划文档同步要求，手工更新 `.planning/STATE.md`、`.planning/ROADMAP.md` 和 `.planning/REQUIREMENTS.md`。
- **Files modified:** `.planning/STATE.md`, `.planning/ROADMAP.md`, `.planning/REQUIREMENTS.md`

## Known Stubs

None - 未发现 TODO/FIXME/placeholder/coming soon/not available 或会阻塞本计划目标的空数据 stub。

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: backend-event-boundary | `include/rtc/security.h` | 新增 security backend -> core event callback 信任边界；计划 threat model 已覆盖 T-04-01/T-04-02。 |
| threat_flag: fixed-storage-capacity | `include/rtc/security.h`, `include/rtc/limits.h`, `include/rtc/config.h` | 新增 backend session fixed storage 容量契约；计划 threat model 已覆盖 T-04-03。 |

## Issues Encountered

- TDD RED 阶段按预期分别因缺少 `rtc/security.h`、DTLS/SRTP counters 和 trace 常量失败。
- 初版测试使用了不存在的 `RTC_ASSERT`/`RTC_ASSERT_EQ` 宏，已改为项目既有 `RTC_TEST_ASSERT`/`RTC_TEST_EQ_INT`。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

04-02 可以基于 `rtc_security_backend_config_t` 和 `rtc_security_backend_vtable_t` 在 create-time 切分 fixed storage，并将 backend `RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM` 转发到既有 `observer.on_datagram`。

## Self-Check: PASSED

- `include/rtc/security.h`：FOUND
- `tests/test_security.c`：FOUND
- `.planning/phases/04-dtls-srtp-secure-transport/04-01-SUMMARY.md`：FOUND
- `4ab8fe2`：FOUND
- `6a5372b`：FOUND
- `f34addd`：FOUND
- `9d6dc52`：FOUND

---
*Phase: 04-dtls-srtp-secure-transport*
*Completed: 2026-05-10*
