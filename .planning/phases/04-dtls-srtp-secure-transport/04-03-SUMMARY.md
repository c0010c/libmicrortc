---
phase: 04-dtls-srtp-secure-transport
plan: 03
subsystem: security
tags: [c, dtls, sdp-fingerprint, security-backend, tdd]

requires:
  - phase: 04-dtls-srtp-secure-transport
    provides: 固定 DTLS session runtime、ICE connected 后自动启动 DTLS、backend event 分发入口
provides:
  - createOffer/createAnswer 写 SDP 前从 security backend 刷新本地 sha-256 fingerprint
  - backend local fingerprint 查询失败或非 sha-256 时返回稳定错误并阻断旧配置 fingerprint 输出
  - handshake complete event 后校验 DTLS peer certificate fingerprint 与远端 SDP fingerprint 完整匹配
  - fingerprint mismatch 进入 dtls.failed，记录 counter/trace/error，并阻断 key export
affects: [phase-04, phase-05, security, dtls, srtp, sdp]

tech-stack:
  added: []
  patterns: [backend-fingerprint-snapshot, handshake-complete-fingerprint-gate, deterministic-security-backend-fixture]

key-files:
  created: []
  modified: [src/security/security.h, src/security/security.c, src/api/peer_connection.h, src/api/peer_connection.c, tests/test_security.c, tests/test_sdp_writer.c, tests/test_peer_connection.c, tests/test_jsep.c, tests/test_observability.c]

key-decisions:
  - "本地 SDP fingerprint 由 backend session 的 get_local_fingerprint 提供，create-time config.sdp.dtls_fingerprint 只保留为兼容输入。"
  - "首版只接受完整字符串形式的 sha-256 fingerprint；非 sha-256 算法返回 RTC_STATUS_PROTOCOL_ERROR。"
  - "HANDSHAKE_COMPLETE 先做 peer fingerprint verification；mismatch 后不进入 key export 或 srtp.ready。"

patterns-established:
  - "createOffer/createAnswer 在 SDP writer 前调用 rtc_security_prepare_local_fingerprint，并把 PeerConnection 内部固定 buffer 作为 rtc_sdp_parameters_t 的 fingerprint 快照。"
  - "fingerprint mismatch 统一通过 dtls.failed、RTC_TRACE_DTLS_STATE reason=fingerprint_mismatch、dtls.fingerprint_mismatch counter 和 observer error 诊断。"

requirements-completed: [SEC-02]

duration: 6min
completed: 2026-05-10
---

# Phase 4 Plan 03: DTLS Fingerprint 绑定 Summary

**SDP fingerprint 绑定到 security backend 证书 fingerprint，并在 DTLS peer certificate mismatch 时硬失败。**

## Performance

- **Duration:** 6 分钟
- **Started:** 2026-05-10T13:41:55Z
- **Completed:** 2026-05-10T13:47:27Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- `rtc_security_prepare_local_fingerprint` 查询 backend local fingerprint，只接受 `sha-256 ` 前缀，并把结果写入 PeerConnection 固定 buffer 后供 SDP writer 使用。
- `rtc_peer_connection_create_offer` 和 `rtc_peer_connection_create_answer` 在写 SDP 前刷新 backend fingerprint，避免输出旧 `config.sdp.dtls_fingerprint`。
- deterministic security backend fixture 接入 `tests/test_peer_connection.c`、`tests/test_jsep.c` 和 `tests/test_observability.c`，使 createOffer/createAnswer 路径显式获得 backend fingerprint。
- `rtc_security_verify_peer_fingerprint` 接入 `RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE`，完整匹配 `pc->remote_summary.dtls_fingerprint` 与 backend peer fingerprint。
- mismatch 路径进入 `dtls.failed`，递增 `fingerprint_mismatch` 与 `handshake_failed`，输出 reason `fingerprint_mismatch`，并测试确认 `export_keying_material` 未调用。

## Task Commits

1. **Task 1 RED:** `6246132` test(04-03): add failing backend fingerprint SDP tests
2. **Task 1 GREEN:** `9f5ff22` feat(04-03): use backend fingerprint for local SDP
3. **Task 2 RED:** `65b8d40` test(04-03): add failing fingerprint mismatch test
4. **Task 2 GREEN:** `47a5029` feat(04-03): fail DTLS on fingerprint mismatch

## Files Created/Modified

- `src/security/security.h` - 暴露内部 `rtc_security_prepare_local_fingerprint` 与 `rtc_security_verify_peer_fingerprint`。
- `src/security/security.c` - 实现 local/peer fingerprint 查询、`sha-256` 校验、完整匹配和 mismatch 失败诊断。
- `src/api/peer_connection.h` - 新增固定 `local_dtls_fingerprint[128]` buffer。
- `src/api/peer_connection.c` - createOffer/createAnswer 写 SDP 前刷新 backend fingerprint。
- `tests/test_security.c` - 新增 backend fingerprint SDP 测试、非 `sha-256` 拒绝测试和 mismatch 阻断 key export 测试。
- `tests/test_sdp_writer.c` - 标注 writer 消费 backend fingerprint snapshot。
- `tests/test_peer_connection.c` - 接入 deterministic security backend fixture。
- `tests/test_jsep.c` - 接入 deterministic security backend fixture。
- `tests/test_observability.c` - 接入 deterministic security backend fixture。

## Decisions Made

- 未新增第三方依赖或真实 TLS/SRTP 适配；继续使用 deterministic backend 验证核心安全语义。
- `security_backend` 缺失、local fingerprint 查询失败或输出 buffer 不足统一视为 `local_fingerprint_failed` backend error。
- peer fingerprint mismatch 使用内部 detail code `RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH`，公共 status 仍保持 `RTC_STATUS_PROTOCOL_ERROR`。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：PASS，1/1 tests passed。
- `grep -R "fingerprint_mismatch" src/security/security.c tests/test_security.c`：PASS。
- `grep -R "rtc_security_prepare_local_fingerprint" src/api/peer_connection.c src/security/security.h`：PASS。
- Task 1 acceptance grep：`local_fingerprint_failed`、`unsupported_fingerprint_algorithm`、`backend fingerprint`、`test_security_backend`、createOffer/createAnswer 路径全部命中。
- Task 2 acceptance grep：`fingerprint_mismatch`、`RTC_STATUS_PROTOCOL_ERROR`、`dtls.failed`、`export_keying_material_calls` 全部命中。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 调整旧容量测试的精确 arena 大小**
- **Found during:** Task 1
- **Issue:** PeerConnection 增加固定 `local_dtls_fingerprint` buffer 且相关测试接入 security backend storage 后，旧测试用精确 arena 大小卡容量边界时会先命中不同资源。
- **Fix:** 只调整 `tests/test_peer_connection.c` 中容量边界测试的 arena 大小，保持原测试仍验证 ICE pair 与 STUN transaction 容量诊断。
- **Files modified:** `tests/test_peer_connection.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `9f5ff22`

---

**Total deviations:** 1 auto-fixed（Rule 1）
**Impact on plan:** 只修正由本计划固定 buffer/backend fixture 直接影响的旧测试边界，未扩大功能范围。

### Workflow Deviations

**1. GSD SDK 不在 PATH**
- **Found during:** 状态读取与状态更新
- **Issue:** `gsd-sdk query ...` 无输出且 `command -v gsd-sdk` 未找到，无法使用 SDK 自动更新 STATE/ROADMAP/REQUIREMENTS。
- **Fix:** 按 AGENTS.md 的规划文档同步要求，手工更新 `.planning/STATE.md`、`.planning/ROADMAP.md` 和 `.planning/REQUIREMENTS.md`。

## Known Stubs

None - 未发现 TODO/FIXME/placeholder/coming soon/not available 或阻塞本计划目标的空数据 stub。

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: backend-local-fingerprint-to-sdp | `src/security/security.c`, `src/api/peer_connection.c` | backend local certificate fingerprint 进入本地 SDP；计划 threat model 已覆盖 T-04-07/T-04-08。 |
| threat_flag: dtls-peer-fingerprint-verification | `src/security/security.c` | DTLS peer certificate fingerprint 与远端 SDP fingerprint 完整匹配；计划 threat model 已覆盖 T-04-06/T-04-09/T-04-10。 |

## Issues Encountered

- RED 阶段按预期失败：Task 1 中 backend local fingerprint 查询次数为 0；Task 2 中 handshake complete 后 peer fingerprint 查询次数为 0。
- 04-04 会在 fingerprint verification 成功后继续实现 key export 与 `srtp.ready`；本计划只保证 mismatch 阻断该后续路径。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

04-04 可以在 `rtc_security_handle_handshake_complete` 中的 `rtc_security_verify_peer_fingerprint` 成功之后接入 `export_keying_material`、SRTP context init 和 `srtp.ready`，并沿用本计划的 mismatch 阻断测试模式。

## TDD Gate Compliance

- RED commits: `6246132`, `65b8d40`
- GREEN commits: `9f5ff22`, `47a5029`
- Gate sequence: PASSED

## Self-Check: PASSED

- `src/security/security.h`：FOUND
- `src/security/security.c`：FOUND
- `src/api/peer_connection.c`：FOUND
- `src/api/peer_connection.h`：FOUND
- `tests/test_security.c`：FOUND
- `.planning/phases/04-dtls-srtp-secure-transport/04-03-SUMMARY.md`：FOUND
- `6246132`：FOUND
- `9f5ff22`：FOUND
- `65b8d40`：FOUND
- `47a5029`：FOUND

---
*Phase: 04-dtls-srtp-secure-transport*
*Completed: 2026-05-10*
