---
status: complete
phase: 03-ice-stun-datagram-network-layer
source:
  - 03-01-SUMMARY.md
  - 03-02-SUMMARY.md
  - 03-03-SUMMARY.md
  - 03-04-SUMMARY.md
started: "2026-05-10T19:51:29+08:00"
updated: "2026-05-10T19:53:10+08:00"
---

# 第 3 阶段 UAT：ICE/STUN 与 Datagram 网络层

## Current Test

[testing complete]

## Tests

### 1. ICE/STUN 公共契约与固定容量
expected: 集成者可以在 create-time config 中设置 0/1 个 IP:port STUN server、本地 host IP:port、candidate/pair/transaction limits；创建阶段会从 arena 固定切分候选、pair 和 STUN transaction 槽，容量不足返回对应 `RTC_STATUS_CAPACITY_*`。
result: pass

### 2. 远端 Trickle Candidate 输入
expected: `rtc_peer_connection_add_ice_candidate` 接受 `candidate:` 和 `a=candidate:`，解析 host/srflx UDP component 1 candidate，复制原始字符串和结构化摘要，不保存调用方 buffer，不隐式启动 gathering/checks/socket，并稳定拒绝 relay、非 UDP、component 2、非法 port 和容量超限。
result: pass

### 3. STUN 编解码与 Host/Srflx Gathering
expected: `rtc_peer_connection_gather_candidates` 在 network executor 上显式启动 gathering；host-only 模式输出 `typ host` candidate，host+srflx 模式通过 `observer.on_datagram` 输出 STUN Binding request，匹配 transaction id 的 IPv4 `XOR-MAPPED-ADDRESS` response 生成 `typ srflx` candidate，timeout 和 transaction 容量不足可诊断。
result: pass

### 4. Full ICE Checks 与 Regular Nomination
expected: `rtc_peer_connection_start_connectivity_checks` 要求已有本地和远端 candidate；它在固定 pair table 内创建 candidate pairs，进入 `ice.checking`，通过 `observer.on_datagram` 输出 STUN check，controlling 角色执行 regular nomination，nominated success 后进入 `ice.connected` 并输出 selected pair trace。
result: pass

### 5. ICE 失败原因与可观测性
expected: 没有本地候选、没有远端候选、STUN timeout、pair exhausted、role conflict、capacity exhausted、malformed STUN 和 unknown datagram 都能通过 observer/trace/counter 区分；`ice.checking`、`ice.connected`、`ice.failed` 状态可观察。
result: pass

### 6. Datagram Demux 与占位路由
expected: `rtc_peer_connection_receive_datagram` 在 network executor 上分类 STUN、DTLS、RTP、RTCP 和 unknown；STUN 路由到 ICE/STUN handler，DTLS/RTP/RTCP 在第 3 阶段只 trace/counter/占位返回，不解析 payload；unknown 与 malformed STUN 都返回 `RTC_STATUS_PROTOCOL_ERROR` 且 reason 不同。
result: pass

### 7. 第 3 阶段构建、测试、文档与需求追踪
expected: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过；中文 API 文档明确 gathering、checks、receive_datagram、`observer.on_datagram`、STUN server 0/1 IP:port、用户负责 socket 发送和第 3 阶段不越界实现 DTLS/RTP/RTCP；需求追踪保持阶段执行后待正式验证/转场的状态。
result: pass

## Summary

total: 7
passed: 7
issues: 0
pending: 0
skipped: 0
blocked: 0

## Evidence

- `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- `rg -n "\bmalloc\b|\bcalloc\b|\brealloc\b" src include` 无输出，核心源码没有绕过固定内存边界。
- 检查 `include/rtc/config.h`、`include/rtc/limits.h`、`include/rtc/status.h`、`include/rtc/peer_connection.h`、`src/api/peer_connection.c` 和 `tests/test_peer_connection.c`：STUN server 0/1、candidate pair/transaction limits、容量错误、gather/check API 和 network executor 亲和有覆盖。
- 检查 `tests/test_jsep.c` 和 `docs/API-执行器与内存契约.md`：远端 candidate 支持 host/srflx，拒绝 relay、非 UDP、component 2、非法 port 和容量超限；文档明确不保存调用方 buffer、不隐式启动 checks/socket。
- 检查 `src/stun`、`src/ice`、`tests/test_stun.c`、`tests/test_ice.c`：STUN Binding request、header parse、IPv4 `XOR-MAPPED-ADDRESS`、host-only gathering、host+srflx gathering、STUN timeout 和 transaction 容量不足均有覆盖。
- 检查 `src/ice/ice.c`、`tests/test_ice.c`、`tests/test_observability.c`：candidate pair、`ice.checking`、regular nomination、`USE-CANDIDATE`、selected pair、`ice.connected`、trickle 后增量 pair 和 pair table 容量有覆盖。
- 检查 `src/ice/ice.c`、`tests/test_ice.c`、`tests/test_datagram.c`：`no_local_candidates`、`no_remote_candidates`、`stun_timeout`、`pair_check_exhausted`、`role_conflict`、`capacity_exhausted`、`malformed_stun`、`unknown_datagram` 可区分。
- 检查 `src/net/demux.c`、`src/api/peer_connection.c`、`tests/test_datagram.c`：STUN/DTLS/RTP/RTCP/UNKNOWN 分类、`RTC_TRACE_NET_DEMUX`、`demux_*` counters、STUN 路由、unknown/malformed 错误路径和 DTLS/RTP/RTCP 占位路由有覆盖。
- 检查 `docs/API-执行器与内存契约.md`：第 3 阶段 network API、buffer 生命周期、`observer.on_datagram` 用户负责 socket 发送、STUN server 0/1 IP:port、无 hostname/DNS/TURN、DTLS/RTP/RTCP 不越界解析均用中文说明。
- 检查 `.planning/REQUIREMENTS.md`、`.planning/PROJECT.md`、`.planning/ROADMAP.md`、`.planning/STATE.md`：第 3 阶段需求、计划 SUMMARY 和“执行完成，待验证/转场”状态可追踪。

## Gaps

[none yet]
