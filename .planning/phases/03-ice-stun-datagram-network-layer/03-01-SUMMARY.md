---
phase: 3
plan: 03-01
title: ICE/STUN 公共契约、固定容量与候选模型
completed_at: "2026-05-10T11:10:42Z"
requirements:
  - SDP-05
  - ICE-02
  - ICE-03
  - ICE-04
  - ICE-05
  - NET-03
key_files:
  created:
    - .planning/phases/03-ice-stun-datagram-network-layer/03-01-SUMMARY.md
  modified:
    - include/rtc/config.h
    - include/rtc/limits.h
    - include/rtc/peer_connection.h
    - include/rtc/status.h
    - include/rtc/counters.h
    - include/rtc/trace.h
    - src/api/peer_connection.h
    - src/api/peer_connection.c
    - src/observability/counters.c
    - tests/test_peer_connection.c
    - tests/test_jsep.c
    - tests/test_observability.c
    - docs/API-执行器与内存契约.md
    - examples/create_destroy.c
commits:
  - 692f3ae
  - cacc08b
---

# Phase 3 Plan 03-01: ICE/STUN 公共契约、固定容量与候选模型 Summary

## 一句话总结

建立第 3 阶段 ICE/STUN 的公共 API、create-time STUN 配置、固定容量槽位和远端 trickle candidate 结构化输入。

## 完成内容

- 新增 `rtc_stun_server_t` 与 `rtc_peer_connection_config_t.stun_server_count`，创建阶段只接受 0/1 个 IP 字面量 STUN server，拒绝 hostname、数量 2 和 port 0。
- 扩展 `rtc_ice_limits_t`，加入 `max_candidate_pairs` 和 `max_transactions`；创建阶段从 arena 固定切分本地/远端 candidate 摘要、candidate pair 和 STUN transaction 槽。
- 新增 `RTC_STATUS_CAPACITY_ICE_PAIRS`、`RTC_STATUS_CAPACITY_STUN_TRANSACTIONS`、ICE/STUN/net counters 与第 3 阶段 trace event/field 常量。
- 新增 `rtc_peer_connection_gather_candidates` 与 `rtc_peer_connection_start_connectivity_checks`，保持 network executor 亲和；当前按计划返回 `RTC_STATUS_UNSUPPORTED`。
- `rtc_peer_connection_add_ice_candidate` 升级为远端 trickle candidate 输入：复制原始字符串，解析 foundation、component、transport、priority、address、port 和 type；支持 `host/srflx`，拒绝 relay、非 UDP、component 2 和非法 port。
- 文档更新了第 3 阶段 `addIceCandidate` 语义、固定容量、STUN 配置边界、不会隐式启动 checks、不会保存调用方 buffer。

## 验证结果

- Task 03-01-01 verification：`cmake --build build && ctest --test-dir build --output-on-failure` 通过。
- Task 03-01-02 verification：`cmake --build build && ctest --test-dir build --output-on-failure` 通过。
- Plan-level verification：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Acceptance grep 均通过：公共 API、STUN 配置、limits/status/trace 常量、测试覆盖和中文文档关键语义均存在。

## 偏离计划

### 自动修复的问题

**1. [Rule 3 - Blocking] 同步示例和既有测试配置的新 ICE limits**

- **Found during:** Task 03-01-01
- **Issue:** 新增 create-time 校验要求 `max_candidate_pairs` 和 `max_transactions` 非 0；既有 `tests/test_jsep.c`、`tests/test_observability.c` 和 `examples/create_destroy.c` 的配置未设置新字段，会导致构建/测试路径被 `RTC_STATUS_INVALID_ARGUMENT` 阻断。
- **Fix:** 在这些配置 helper 中设置固定 pair/transaction 容量。
- **Files modified:** `tests/test_jsep.c`、`tests/test_observability.c`、`examples/create_destroy.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Commit:** `692f3ae`

**Total deviations:** 1 auto-fixed。**Impact:** 与新增公共契约保持一致，不改变项目边界。

## Known Stubs

无。`rtc_peer_connection_gather_candidates` 和 `rtc_peer_connection_start_connectivity_checks` 当前返回 `RTC_STATUS_UNSUPPORTED` 是本计划明确要求的第 3 阶段占位，不阻塞本计划目标。

## Threat Flags

无新增计划外安全边界。STUN 配置、network API 和远端 candidate 输入均已在本计划 threat model 中覆盖。

## 后续衔接

- 03-02 可以在不修改公共 API 的情况下实现 STUN 编解码、transaction 和 host/srflx gathering。
- 03-03 可以复用 candidate summary、pair 和 pending pair 标记实现 connectivity checks。

## Self-Check: PASSED

- Summary 文件存在：`FOUND_SUMMARY`
- 任务提交存在：`692f3ae`、`cacc08b`
- 最终验证通过：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`，`rtc_tests` 1/1 passed。
