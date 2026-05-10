---
phase: 3
plan: 03-04
subsystem: net
tags: [c, ice, stun, datagram, demux, observability]
requires:
  - phase: 03-02
    provides: STUN 编解码、transaction table、host/srflx gathering 和 `observer.on_datagram`
  - phase: 03-03
    provides: Full ICE checks、regular nomination、selected pair 和 ICE 状态事件
provides:
  - STUN/DTLS/RTP/RTCP datagram demux
  - `rtc_peer_connection_receive_datagram` 的 STUN 路由和占位协议路由
  - 第 3 阶段 API 文档、需求追踪和阶段收口记录
affects: [phase-4-dtls-srtp, phase-5-rtp-rtcp, observability]
tech-stack:
  added: []
  patterns:
    - 纯 C 固定内存 demux 子系统
    - network executor 上的 datagram 输入和 trace/counter 诊断
key-files:
  created:
    - src/net/demux.h
    - src/net/demux.c
    - tests/test_datagram.c
    - .planning/phases/03-ice-stun-datagram-network-layer/03-04-SUMMARY.md
  modified:
    - CMakeLists.txt
    - src/api/peer_connection.c
    - tests/test_main.c
    - tests/test_peer_connection.c
    - tests/test_ice.c
    - docs/API-执行器与内存契约.md
    - .planning/REQUIREMENTS.md
    - .planning/PROJECT.md
key-decisions:
  - "DTLS/RTP/RTCP 在第 3 阶段只 demux、trace、counter 和占位路由，不解析 payload。"
  - "unknown_datagram 与 malformed_stun 使用不同 trace reason，均返回 RTC_STATUS_PROTOCOL_ERROR。"
requirements-completed: []
duration: 6m18s
completed: 2026-05-10
---

# Phase 3 Plan 03-04: Datagram demux、占位路由、文档与阶段收口 Summary

STUN/DTLS/RTP/RTCP datagram demux 接入 `receive_datagram`，STUN 进入 ICE/STUN handler，其他协议保持第 3 阶段可观测占位路由。

## 执行概况

- **开始时间：** 2026-05-10T11:33:46Z
- **完成时间：** 2026-05-10T11:40:04Z
- **耗时：** 6m18s
- **任务：** 2/2
- **验证：** `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。

## 完成内容

- 新增 `src/net/demux` 子系统，分类 STUN、DTLS、RTP、RTCP 和 unknown datagram。
- `rtc_peer_connection_receive_datagram` 不再返回 unsupported：它保持 `RTC_EXECUTOR_NETWORK` 亲和，输出 `RTC_TRACE_NET_DEMUX`，递增 `counters.net.demux_*`，并把 STUN datagram 路由到 ICE/STUN handler。
- DTLS/RTP/RTCP 在第 3 阶段只做 trace、counter 和占位返回 `RTC_STATUS_OK`，不解析 payload，不调用 security backend 或 media observer。
- unknown datagram 返回 `RTC_STATUS_PROTOCOL_ERROR`，observer error 使用 subsystem `net` / operation `receive_datagram`，trace reason 为 `unknown_datagram`。
- 首字节像 STUN 但 header、magic cookie 或长度不合法的 datagram 返回 `RTC_STATUS_PROTOCOL_ERROR`，trace reason 为 `malformed_stun`。
- 新增 `tests/test_datagram.c`，覆盖 demux 分类、空输入、unknown、malformed STUN、network executor 亲和、srflx response 和 pair check response。
- 更新中文 API 契约，明确 `receive_datagram` 只在调用期间读取用户 buffer、`observer.on_datagram` 由用户负责 socket 发送、STUN server 首版只支持 0/1 个 IP:port 且不支持 hostname/DNS/TURN。
- `.planning/REQUIREMENTS.md` 将第 3 阶段需求追踪状态保持为“已规划”，未标记完成；`.planning/PROJECT.md` 只记录“第 3 阶段已规划”，未宣称已验证。

## 任务提交

1. **03-04-01 Datagram demux 与 receive_datagram 路由** — `901fe18`
2. **03-04-02 文档、需求追踪与测试矩阵收口** — `2a41b1a`

## 验收结果

- `src/net/demux.h` 和 `src/net/demux.c` 存在。
- `src/net/demux.c` 包含 `RTC_NET_PROTOCOL_STUN`、`RTC_NET_PROTOCOL_DTLS`、`RTC_NET_PROTOCOL_RTP`、`RTC_NET_PROTOCOL_RTCP`。
- `rtc_peer_connection_receive_datagram` 函数体不再返回 `RTC_STATUS_UNSUPPORTED`，并包含 `RTC_TRACE_NET_DEMUX`。
- `tests/test_datagram.c` 包含 `unknown_datagram` 和 `RTC_NET_PROTOCOL_DTLS` 覆盖。
- `tests/test_peer_connection.c` 不再断言 `rtc_peer_connection_receive_datagram` 返回 `RTC_STATUS_UNSUPPORTED`。
- `.planning/REQUIREMENTS.md` 包含 `ICE-01 | 第 3 阶段 | 已规划` 和 `NET-03 | 第 3 阶段 | 已规划`。
- `tests/test_ice.c` 包含 `regular nomination` 覆盖标记。
- Full suite 通过。

## 偏离计划

None - plan executed exactly as written.

## Known Stubs

无阻塞目标的 stub。DTLS/RTP/RTCP 的“占位路由”是本计划明确范围：只 demux、trace、counter，不解析协议内容；后续第 4、5 阶段接管真实协议处理。

## Threat Flags

无计划外新增威胁面。新增 network datagram 输入、unknown/malformed 错误语义、STUN 路由和 DTLS/RTP/RTCP 占位边界均已在本计划 threat model 中覆盖。

## 后续衔接

- 第 4 阶段可以在 `RTC_NET_PROTOCOL_DTLS` 路由点接入 DTLS backend，但必须继续保持用户负责 UDP/socket I/O。
- 第 5 阶段可以在 RTP/RTCP 路由点接入 SRTP/SRTCP 与媒体解析，当前 tests 已锁定第 3 阶段不越界解析 payload。

## Self-Check: PASSED

- 文件存在：`src/net/demux.h`、`src/net/demux.c`、`tests/test_datagram.c`、`.planning/phases/03-ice-stun-datagram-network-layer/03-04-SUMMARY.md`。
- 提交存在：`901fe18`、`2a41b1a`。
- 最终验证通过：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`，`rtc_tests` 1/1 passed。
