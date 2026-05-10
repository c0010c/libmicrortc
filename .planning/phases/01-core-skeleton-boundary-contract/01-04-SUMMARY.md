---
phase: 1
plan: 04
subsystem: observability
tags: [observer, trace, counters, docs, examples]
key-files:
  created:
    - include/rtc/observer.h
    - include/rtc/trace.h
    - include/rtc/counters.h
    - tests/test_observability.c
    - examples/create_destroy.c
    - docs/API-执行器与内存契约.md
  modified:
    - src/api/peer_connection.c
    - include/rtc/config.h
    - include/rtc/rtc.h
metrics:
  tests: 5
  commits: 1
---

# 计划 04 总结：observer、trace、计数器、文档与最小示例

## 完成内容

- 新增公共 observer、trace 和 counters 结构，固定生命周期、容量失败、亲和违规和未支持 API 的 trace 常量。
- 将 observer 和 counters 接入 `PeerConnection` 生命周期、未支持 API 和亲和违规路径。
- 新增 `rtc_peer_connection_get_counters`，在 signaling executor 上返回计数器快照。
- 新增可观测性测试，覆盖 create/destroy trace、未支持 API counters 和亲和违规 counters。
- 新增最小 create/destroy 示例和中文 API 执行器/内存契约文档。

## 提交

| 提交 | 内容 |
|------|------|
| `039ce2d` | `feat(01-04): add observability contract` |

## 验证

- `cmake -S . -B build`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `! rg -n "\\bmalloc\\b|\\bcalloc\\b|\\brealloc\\b" src include`

## 偏差

无。

## Self-Check: PASSED

observer、trace、counters、示例和中文契约文档已完成，测试通过，并保持纯 C、固定内存、无线程和无第三方依赖边界。
