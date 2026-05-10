---
status: complete
phase: 01-core-skeleton-boundary-contract
source:
  - 01-01-SUMMARY.md
  - 01-02-SUMMARY.md
  - 01-03-SUMMARY.md
  - 01-04-SUMMARY.md
started: "2026-05-10T17:10:51+08:00"
updated: "2026-05-10T17:13:25+08:00"
---

# 第 1 阶段 UAT：核心骨架与边界契约

## Current Test

[testing complete]

## Tests

### 1. 构建静态库与测试入口
expected: 从干净构建目录运行 `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 时，项目可以构建静态库目标 `rtc`，CTest 可以发现并运行测试，公共头文件可以被测试目标包含。
result: pass

### 2. 公共状态码与头文件入口
expected: 集成者可以包含 `include/rtc/rtc.h` 和 `include/rtc/status.h`，看到稳定的 `rtc_status_t` 返回码，并以纯 C API 方式使用这些类型。
result: pass

### 3. 固定 arena 与容量诊断
expected: 集成者可以通过 `rtc_peer_connection_config_t` 提供 arena 和 limits；容量不足时会得到可诊断的资源类别、required 和 used 信息，核心源码不直接使用 `malloc`、`calloc` 或 `realloc`。
result: pass

### 4. PeerConnection 生命周期与执行器亲和
expected: 集成者可以创建和销毁不透明 `rtc_peer_connection_t`；在错误 executor 上调用受限 API 会返回 `RTC_STATUS_AFFINITY_VIOLATION`，尚未实现的协议 API 会稳定返回 `RTC_STATUS_UNSUPPORTED`。
result: pass

### 5. 可观测性、计数器和最小示例
expected: 集成者可以配置 observer、trace 和 counters；create/destroy、容量失败、亲和违规和未支持 API 路径会产生可观测信号；`examples/create_destroy.c` 展示最小创建销毁流程。
result: pass

### 6. 中文 API 契约文档
expected: `docs/API-执行器与内存契约.md` 用中文说明公共 API、observer/datagram 回调、执行器亲和和固定内存契约，能指导后续协议层遵守项目边界。
result: pass

## Summary

total: 6
passed: 6
issues: 0
pending: 0
skipped: 0
blocked: 0

## Evidence

- `cmake -S . -B build -DRTC_BUILD_TESTS=ON -DRTC_BUILD_EXAMPLES=ON`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `rg -n "\bmalloc\b|\bcalloc\b|\brealloc\b" src include` returned no matches.
- `./build/rtc_create_destroy`
- Inspected public headers: `include/rtc/rtc.h`, `include/rtc/status.h`, `include/rtc/config.h`, `include/rtc/limits.h`, `include/rtc/observer.h`, `include/rtc/trace.h`, `include/rtc/counters.h`.
- Inspected implementation and tests for `RTC_STATUS_AFFINITY_VIOLATION`, `RTC_STATUS_UNSUPPORTED`, `rtc_peer_connection_get_counters`, capacity diagnostics, trace events and counters.
- Inspected `docs/API-执行器与内存契约.md` for fixed memory, executor affinity, observer/trace/counter and datagram callback contract coverage.

## Gaps

[none yet]
