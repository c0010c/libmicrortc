---
phase: 3
plan: 03-03
title: Full ICE checks、regular nomination 与 ICE 状态事件
completed_at: "2026-05-10T11:32:18Z"
requirements:
  - ICE-01
  - ICE-04
  - ICE-05
  - NET-02
key_files:
  created:
    - .planning/phases/03-ice-stun-datagram-network-layer/03-03-SUMMARY.md
  modified:
    - src/ice/ice.h
    - src/ice/ice.c
    - src/stun/stun.h
    - src/stun/stun.c
    - src/api/peer_connection.c
    - src/api/peer_connection.h
    - tests/test_ice.c
    - tests/test_observability.c
    - tests/test_peer_connection.c
    - docs/API-执行器与内存契约.md
commits:
  - 437c70e
  - cacd643
---

# Phase 3 Plan 03-03: Full ICE checks、regular nomination 与 ICE 状态事件 Summary

## 一句话总结

在固定 candidate pair 和 STUN transaction 容量内实现 Full ICE checks、controlling/controlled role、regular nomination、selected pair 和可诊断 ICE 失败原因。

## 完成内容

- 新增 ICE pair 状态：`FROZEN`、`WAITING`、`IN_PROGRESS`、`SUCCEEDED`、`FAILED`、`NOMINATED`、`SELECTED`。
- `rtc_peer_connection_start_connectivity_checks` 不再返回 unsupported；它在 network executor 上校验本地/远端 candidates，创建固定容量 pair table，进入 `ice.checking` 并输出 STUN Binding request datagram。
- ICE role 由 JSEP offer 路径推导：本地 offer 为 `controlling`，远端 offer 为 `controlled`；未设置 description 的测试路径默认 `controlling`。
- controlling 角色采用 regular nomination：普通 Binding success 后发送带 `USE-CANDIDATE` 的 nominated Binding request；nominated success 后设置 selected pair 并进入 `ice.connected`。
- controlled 角色收到 nominated Binding request 后标记 nominated，设置 selected pair，并输出 `RTC_TRACE_ICE_SELECTED_PAIR`。
- checks 已启动后，`addIceCandidate` 会为新增远端 candidate 增量创建 pair；pair table 超限返回 `RTC_STATUS_CAPACITY_ICE_PAIRS`。
- 增加 ICE 失败 reason：`no_local_candidates`、`no_remote_candidates`、`stun_timeout`、`role_conflict`、`pair_check_exhausted`、`capacity_exhausted`、`malformed_stun`、`unknown_datagram`。
- STUN role conflict error response 映射为 `role_conflict`；pair check timeout 会尝试下一个 pair，全部 exhausted 后进入 `ice.failed` 并递增 `checks_failed`。
- 更新 ICE 与 observability 测试，覆盖 no local/no remote、pair 容量、regular nomination、controlled selected pair、trickle pair、timeout exhausted、malformed STUN、role conflict，以及 observer 收到 `ice.checking`、`ice.connected`、`ice.failed`。
- 更新中文 API 契约文档，说明 checks 入口、regular nomination、selected pair trace、trickle pair 和失败原因。

## 验证结果

- Task 03-03-01 verification：`cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Task 03-03-02 verification：`cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Plan-level verification：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Acceptance grep 均通过：`RTC_ICE_CHECKING`、`RTC_ICE_CONNECTED`、`RTC_TRACE_ICE_PAIR_CREATED`、`RTC_TRACE_ICE_SELECTED_PAIR`、`start_connectivity_checks`、`RTC_STATUS_CAPACITY_ICE_PAIRS`、`ice.connected`、`no_local_candidates`、`no_remote_candidates`、`stun_timeout`、`pair_check_exhausted`、`role_conflict`、`ice.failed`、`ice.checking`、文档 `regular nomination` 和 `selected pair` 均存在。

## 偏离计划

### 自动修复的问题

**1. [Rule 3 - Blocking] 同步既有 peer_connection 测试的 start checks 预期**

- **Found during:** Task 03-03-01
- **Issue:** 03-01/03-02 留下的 `tests/test_peer_connection.c` 仍断言 `rtc_peer_connection_start_connectivity_checks` 返回 `RTC_STATUS_UNSUPPORTED`，与本计划真实实现该 API 的目标冲突。
- **Fix:** 将旧断言更新为当前没有远端 candidate 时的 `RTC_STATUS_INVALID_STATE`。
- **Files modified:** `tests/test_peer_connection.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Commit:** `437c70e`

**Total deviations:** 1 auto-fixed。**Impact:** 与 03-03 的公共 API 行为保持一致，不改变项目边界。

## Known Stubs

- `rtc_peer_connection_receive_datagram` 仍返回 `RTC_STATUS_UNSUPPORTED`，这是 03-04 datagram demux 计划范围。本计划已在 ICE/STUN handler 内覆盖 `malformed_stun`，并保留 `unknown_datagram` reason 常量供 demux 入口接入。

## Threat Flags

无新增计划外安全边界。新增网络输出仍只通过 `observer.on_datagram` 交给用户发送；库不创建 socket、不持有调用方 datagram buffer、不引入动态内存或线程。

## 后续衔接

- 03-04 可以把 `rtc_peer_connection_receive_datagram` 的 STUN demux 接入本计划的 ICE handler，并将未知 datagram 映射到 `unknown_datagram`。
- 后续 DTLS/SRTP/RTP 阶段可以读取 selected pair trace/counter 作为连接建立前置诊断。

## Self-Check: PASSED

- Summary 文件存在：`FOUND_SUMMARY`
- 任务提交存在：`437c70e`、`cacd643`
- 最终验证通过：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`，`rtc_tests` 1/1 passed。
