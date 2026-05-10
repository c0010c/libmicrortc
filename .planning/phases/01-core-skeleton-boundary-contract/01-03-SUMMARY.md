---
phase: 1
plan: 03
subsystem: executor-api
tags: [executor, affinity, peer-connection]
key-files:
  created:
    - include/rtc/executor.h
    - include/rtc/peer_connection.h
    - src/api/peer_connection.c
    - src/executor/executor.c
  modified:
    - include/rtc/config.h
    - include/rtc/rtc.h
metrics:
  tests: 4
  commits: 1
---

# 计划 03 总结：执行器亲和、PeerConnection 生命周期与未支持 API 占位

## 完成内容

- 定义 `RTC_EXECUTOR_SIGNALING`、`RTC_EXECUTOR_MEDIA`、`RTC_EXECUTOR_NETWORK` 三类 executor vtable。
- 将 `rtc_peer_connection_config_t` 中的 executor 占位替换为 `rtc_executors_t`。
- 实现内部执行器亲和检查，错误执行器返回 `RTC_STATUS_AFFINITY_VIOLATION`。
- 定义公共不透明 `rtc_peer_connection_t`，真实布局仅位于内部头。
- 实现 `rtc_peer_connection_create`、`rtc_peer_connection_destroy` 和 offer/answer/description/candidate/datagram 占位 API；未实现协议能力返回 `RTC_STATUS_UNSUPPORTED`。

## 提交

| 提交 | 内容 |
|------|------|
| `229bc7a` | `feat(01-03): add peer connection lifecycle` |

## 验证

- `cmake -S . -B build`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- 已确认 `struct rtc_peer_connection_t` 的真实字段只存在于 `src/api/peer_connection.h`。

## 偏差

- 计划中的负向 grep 会匹配公共头里的合法不透明 typedef；实现仍满足公共头不暴露真实布局的目标。

## Self-Check: PASSED

执行器 vtable、亲和检查、不透明生命周期和未支持 API 占位均已实现并通过测试。
