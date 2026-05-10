---
phase: 3
plan: 03-02
title: STUN 编解码、transaction 与 host/srflx gathering
completed_at: "2026-05-10T11:20:43Z"
duration_seconds: 476
requirements:
  - SDP-05
  - ICE-02
  - ICE-03
  - ICE-05
  - NET-02
  - NET-03
key_files:
  created:
    - src/stun/stun.h
    - src/stun/stun.c
    - src/ice/ice.h
    - src/ice/ice.c
    - tests/test_stun.c
    - tests/test_ice.c
    - .planning/phases/03-ice-stun-datagram-network-layer/03-02-SUMMARY.md
  modified:
    - CMakeLists.txt
    - include/rtc/config.h
    - src/api/peer_connection.h
    - src/api/peer_connection.c
    - tests/test_main.c
    - tests/test_peer_connection.c
    - tests/test_jsep.c
    - tests/test_observability.c
    - examples/create_destroy.c
    - docs/API-执行器与内存契约.md
commits:
  - 70d964a
  - 4dcb910
---

# Phase 3 Plan 03-02: STUN 编解码、transaction 与 host/srflx gathering Summary

## 一句话总结

实现最小 STUN Binding 编解码、固定 transaction table，以及通过 observer 输出 host/srflx candidate 和 STUN UDP datagram 的 ICE gathering。

## 完成内容

- 新增 `src/stun` 子系统，支持 STUN datagram 识别、Binding request 写入、header 解析和 IPv4 `XOR-MAPPED-ADDRESS` 解析；IPv6 XOR-MAPPED-ADDRESS 按计划保留结构并返回 `RTC_STATUS_UNSUPPORTED`。
- 新增 `src/ice` gathering 子系统，维护 `RTC_ICE_NEW/GATHERING/GATHERING_COMPLETE/CHECKING/CONNECTED/FAILED` 内部状态。
- `rtc_peer_connection_config_t` 增加 create-time `local_host_ip/local_host_ip_len/local_host_port`，创建阶段校验 IP 字面量字符和端口。
- `rtc_peer_connection_gather_candidates` 不再返回 unsupported：在 network executor 上输出 `typ host` candidate；配置 1 个 STUN server 时发送 Binding request，并通过 `observer.on_datagram` 输出待发送 UDP payload。
- STUN success response 只接受 pending transaction id 匹配的响应，解析 srflx 地址后输出 `typ srflx` candidate；STUN timeout 会释放 transaction 槽并在已有 host candidate 时完成 gathering。
- 更新 counters、trace、observer 状态测试，覆盖 host-only、host+srflx、transaction timeout、transaction 容量不足和 `ice.gathering_complete` 状态。
- 更新中文 API 契约文档，明确 gathering API、host-only、host+srflx、`on_datagram` buffer 生命周期和用户负责 UDP/socket 发送。

## 验证结果

- Task 03-02-01 verification：`cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Task 03-02-02 verification：`cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Plan-level verification：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- Acceptance grep 均通过：`src/stun`、`src/ice`、`RTC_STUN_MAGIC_COOKIE`、`rtc_stun_write_binding_request`、`rtc_stun_parse_xor_mapped_address`、`RTC_ICE_GATHERING`、`RTC_TRACE_ICE_CANDIDATE_LOCAL`、`RTC_TRACE_STUN_TRANSACTION`、`typ host`、`typ srflx` 和文档 `gather_candidates` 均存在；`rtc_peer_connection_gather_candidates` 函数体不再返回 `RTC_STATUS_UNSUPPORTED`。

## 偏离计划

### 自动修复的问题

**1. [Rule 1 - Bug] 清零固定 ICE/STUN 运行期槽位**

- **Found during:** Task 03-02-02
- **Issue:** create 阶段从 arena 分配出的 candidate summary、candidate pair 和 STUN transaction 槽位未显式清零，导致 transaction timeout 测试可能把未初始化槽识别为 pending transaction。
- **Fix:** create 成功分配各固定数组后立即 `memset` 清零，保持固定内存模型不变。
- **Files modified:** `src/api/peer_connection.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Commit:** `4dcb910`

**2. [Rule 3 - Blocking] 同步既有测试和示例的本地 host 配置**

- **Found during:** Task 03-02-02
- **Issue:** 本计划新增 create-time `local_host_ip/local_host_port` 校验后，既有测试 helper 和示例缺少该必填配置，会阻断构建测试路径。
- **Fix:** 在既有 PeerConnection 测试、JSEP 测试、observability 测试和 create/destroy 示例中设置固定本地 host IP:port。
- **Files modified:** `tests/test_peer_connection.c`、`tests/test_jsep.c`、`tests/test_observability.c`、`examples/create_destroy.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Commit:** `4dcb910`

**Total deviations:** 2 auto-fixed。**Impact:** 不改变项目边界；仍为纯 C、固定内存、无线程、用户负责 UDP/socket I/O、无新增第三方依赖。

## Known Stubs

- `rtc_peer_connection_receive_datagram` 和 `rtc_peer_connection_start_connectivity_checks` 仍返回 `RTC_STATUS_UNSUPPORTED`，属于后续 03-03/03-04 计划范围，不阻塞本计划的 STUN 编解码与 gathering 目标。
- IPv6 `XOR-MAPPED-ADDRESS` 返回 `RTC_STATUS_UNSUPPORTED`，这是本计划允许的最小实现范围；IPv4 srflx 已完成。

## Threat Flags

无新增计划外安全边界。新增 STUN response 匹配、transaction table、candidate 字符串容量检查和 `on_datagram` 生命周期均在本计划 threat model 中覆盖。

## 后续衔接

- 03-03 可以复用 `rtc_ice_handle_stun_response`、transaction table 和 candidate summary，实现 connectivity checks、pair check 和 nomination。
- 03-04 可以把 `rtc_peer_connection_receive_datagram` 的 STUN demux 接入本计划的 STUN/ICE response 处理。

## Self-Check: PASSED

- Summary 文件存在：`FOUND .planning/phases/03-ice-stun-datagram-network-layer/03-02-SUMMARY.md`
- 任务提交存在：`70d964a`、`4dcb910`
- 最终验证通过：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`，`rtc_tests` 1/1 passed。
