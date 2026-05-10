---
phase: 04-dtls-srtp-secure-transport
plan: 02
subsystem: security
tags: [c, dtls, security-runtime, ice-integration, fixed-memory]

requires:
  - phase: 04-dtls-srtp-secure-transport
    provides: 单一 security backend vtable、backend event/callback 契约、DTLS counters 与 trace 常量
provides:
  - PeerConnection create-time 固定 DTLS session storage 切分与 backend create/destroy 生命周期
  - ICE connected 后自动启动 DTLS handshake
  - DTLS datagram 输入 gate、早到 DTLS 拒绝和 backend outgoing datagram 转发
affects: [phase-04, phase-05, security, dtls, srtp, ice]

tech-stack:
  added: []
  patterns: [security-runtime-state-machine, backend-event-dispatch, ice-connected-dtls-autostart]

key-files:
  created: [src/security/security.h, src/security/security.c]
  modified: [CMakeLists.txt, src/api/peer_connection.h, src/api/peer_connection.c, src/ice/ice.c, tests/test_security.c, tests/test_datagram.c]

key-decisions:
  - "security runtime 只保存 backend session/storage 指针，不拥有 malloc/free 或 socket。"
  - "DTLS outgoing datagram 统一复用 observer.on_datagram，不新增 on_dtls_datagram。"
  - "ICE 未 connected 时 DTLS datagram 由 security 层拒绝并记录 ice_not_connected。"

patterns-established:
  - "create-time security storage 由 PeerConnection 从 arena 切分，src/security 只消费已分配 storage 并调用 backend create_session。"
  - "backend event callback 在 src/security/security.c 内部集中分发到 observer、counter、trace 和安全状态。"

requirements-completed: [SEC-01]

duration: 9min
completed: 2026-05-10
---

# Phase 4 Plan 02: Security Runtime 接入 Summary

**固定 DTLS session storage、ICE connected 自动启动 DTLS、早到 DTLS 拒绝和 backend outgoing datagram 转发到既有 UDP 输出回调。**

## Performance

- **Duration:** 9 分钟
- **Started:** 2026-05-10T13:29:26Z
- **Completed:** 2026-05-10T13:37:40Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- 新增 `src/security/security.h` 与 `src/security/security.c`，实现 `rtc_security_init`、`rtc_security_shutdown`、`rtc_security_on_ice_connected` 和 `rtc_security_handle_dtls_datagram`。
- `rtc_peer_connection_create` 从 arena 切分 `limits.dtls.max_session_storage_bytes`，并把 storage、callback 和 `pc` 传给 backend `create_session`。
- `rtc_peer_connection_destroy` 调用 backend `destroy_session`，不释放用户不拥有的内存。
- `src/ice/ice.c` 在 selected pair 进入 `ice.connected` 后调用 `rtc_security_on_ice_connected` 自动启动 DTLS。
- `rtc_peer_connection_receive_datagram` 的 DTLS demux 分支改为进入 security 层；ICE 未 connected 时返回 `RTC_STATUS_INVALID_STATE`，trace reason 为 `ice_not_connected`。
- backend `RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM` 通过既有 `observer.on_datagram` 输出同一 bytes，并递增 `dtls.outgoing_datagrams`。

## Task Commits

1. **Task 1 RED:** `0e1237e` test(04-02): add failing security runtime storage tests
2. **Task 1 GREEN:** `fb438c3` feat(04-02): initialize security backend storage
3. **Task 2 RED:** `28674e0` test(04-02): add failing security datagram runtime tests
4. **Task 2 GREEN:** `c860e20` feat(04-02): route DTLS through security runtime

## Files Created/Modified

- `src/security/security.h` - 新增 security runtime 内部入口声明。
- `src/security/security.c` - 实现 security 状态机、backend create/destroy、DTLS start/input 和 backend event 分发。
- `src/api/peer_connection.h` - 保存 security backend/session/storage、DTLS role/state 和 `srtp_ready`。
- `src/api/peer_connection.c` - create-time 切分 DTLS session storage，destroy 调用 security shutdown，DTLS datagram 路由到 security。
- `src/ice/ice.c` - ICE selected pair 进入 connected 后自动调用 `rtc_security_on_ice_connected`。
- `tests/test_security.c` - 扩展 deterministic backend runtime 测试，覆盖 storage、create failure、early DTLS、auto-start 和 outgoing datagram。
- `tests/test_datagram.c` - 更新 DTLS demux 旧预期，反映 04-02 后早到 DTLS 拒绝语义。
- `CMakeLists.txt` - 将 `src/security/security.c` 加入静态库构建。

## Decisions Made

- 沿用 04-01 的单一 `rtc_security_backend_vtable_t`；04-02 只建立运行期编排，不引入 OpenSSL/libsrtp。
- DTLS role 推导集中在 `rtc_security_on_ice_connected`，当前覆盖计划要求的 Chrome 1v1 最小 setup 表。
- `RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE` 先进入统一 `rtc_security_handle_handshake_complete` 入口；fingerprint verification 和 key export 留给 04-03/04-04。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：PASS，1/1 tests passed。
- Task 1 acceptance grep：`rtc_security_init`、`security_session_storage`、`max_session_storage_bytes`、`RTC_CAPACITY_RESOURCE_DTLS_SESSION`、`create_session`、`backend_create_failed` 全部命中。
- Task 2 acceptance grep：`rtc_security_on_ice_connected`、`RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM`、`observer.on_datagram`、`ice_not_connected`、`early_datagrams_rejected`、`RTC_NET_PROTOCOL_DTLS` 全部命中。
- Plan-level 验证：`! grep -R "on_dtls_datagram" include src tests` PASS。
- 动态分配防线：`! grep -R "malloc\\|calloc\\|realloc\\|pthread_create" src/security src/api/peer_connection.c` PASS。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 更新旧 datagram 测试的 DTLS 占位预期**
- **Found during:** Task 2
- **Issue:** 04-02 前 `tests/test_datagram.c` 期望 DTLS demux 后返回 `RTC_STATUS_OK`，但新安全需求要求 ICE 未 connected 时早到 DTLS 返回 `RTC_STATUS_INVALID_STATE`。
- **Fix:** 将该测试的 DTLS 输入预期改为 `RTC_STATUS_INVALID_STATE`，并断言 trace reason 为 `ice_not_connected`。
- **Files modified:** `tests/test_datagram.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `c860e20`

---

**Total deviations:** 1 auto-fixed（Rule 1）
**Impact on plan:** 只更新由本计划行为变化直接影响的旧测试，未扩大功能范围。

### Workflow Deviations

**1. GSD SDK 不在 PATH**
- **Found during:** 执行初始化与状态更新
- **Issue:** `gsd-sdk query ...` 返回 `command not found`，无法使用 SDK 自动更新 STATE/ROADMAP/REQUIREMENTS。
- **Fix:** 按 AGENTS.md 的规划文档同步要求，手工更新 `.planning/STATE.md`、`.planning/ROADMAP.md` 和 `.planning/REQUIREMENTS.md`。
- **Files modified:** `.planning/STATE.md`, `.planning/ROADMAP.md`, `.planning/REQUIREMENTS.md`

## Known Stubs

None - 未发现 TODO/FIXME/placeholder/coming soon/not available 或会阻塞本计划目标的空数据 stub。

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: udp-to-security-input | `src/api/peer_connection.c`, `src/security/security.c` | 用户 UDP datagram 的 DTLS 分支进入 security 层；计划 threat model 已覆盖 T-04-05。 |
| threat_flag: backend-event-output | `src/security/security.c` | security backend event 通过 `observer.on_datagram` 回到用户 UDP/socket 边界；计划 threat model 已覆盖 T-04-07。 |
| threat_flag: dtls-role-derivation | `src/security/security.c` | DTLS role 由 SDP setup/JSEP 状态推导；计划 threat model 已覆盖 T-04-06。 |

## Issues Encountered

- RED 阶段按预期分别因缺少 `security_session_storage` 字段和早到 DTLS 未拒绝失败。
- C 语言 typedef 与函数名共用普通标识符命名空间，不能把函数命名为 `rtc_security_backend_event_cb`；实现使用 `rtc_security_backend_event_cb_dispatch`，仍消费 04-01 的 `rtc_security_backend_event_cb` 类型。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

04-03 可以基于统一 `rtc_security_handle_handshake_complete` 入口增加 fingerprint verification，并把本地 SDP fingerprint 来源切换为 backend `get_local_fingerprint`。

## Self-Check: PASSED

- `src/security/security.h`：FOUND
- `src/security/security.c`：FOUND
- `.planning/phases/04-dtls-srtp-secure-transport/04-02-SUMMARY.md`：FOUND
- `0e1237e`：FOUND
- `fb438c3`：FOUND
- `28674e0`：FOUND
- `c860e20`：FOUND

---
*Phase: 04-dtls-srtp-secure-transport*
*Completed: 2026-05-10*
