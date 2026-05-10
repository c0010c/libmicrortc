---
phase: 1
slug: core-skeleton-boundary-contract
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-10
---

# 第 1 阶段：验证策略

## 测试基础设施

| 属性 | 值 |
|------|----|
| **框架** | 纯 C 极小 test runner + CTest 调度 |
| **配置文件** | `CMakeLists.txt` |
| **快速命令** | `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` |
| **完整命令** | `cmake -S . -B build -DRTC_BUILD_TESTS=ON -DRTC_BUILD_EXAMPLES=ON && cmake --build build && ctest --test-dir build --output-on-failure` |
| **预计耗时** | 10 到 30 秒 |

## 采样频率

- **每个任务提交后**：运行快速命令。
- **每个 wave 后**：运行完整命令。
- **进入 `$gsd-verify-work` 前**：完整命令必须通过。
- **最大反馈延迟**：30 秒。

## 每任务验证映射

| 任务 ID | 计划 | Wave | 需求 | 威胁引用 | 安全行为 | 测试类型 | 自动命令 | 文件存在 | 状态 |
|---------|------|------|------|----------|----------|----------|----------|----------|------|
| 01-01-01 | 01 | 1 | BLD-01, BLD-02 | T-01-01 | 不引入第三方依赖或 GPL/LGPL 代码 | build | `cmake -S . -B build && cmake --build build` | ✅ | ⬜ pending |
| 01-01-02 | 01 | 1 | API-03 | T-01-02 | 公共状态码稳定可测 | unit | `ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 01-02-01 | 02 | 1 | API-02, MEM-01, MEM-02, MEM-03 | T-02-01 | 固定 arena 失败可诊断，创建后无动态增长 | unit | `ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 01-03-01 | 03 | 2 | API-01, API-05, EXE-01, EXE-02, EXE-03 | T-03-01 | 亲和违规 release 返回错误，debug 可断言 | unit | `ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 01-04-01 | 04 | 3 | OBS-01, OBS-02, OBS-03 | T-04-01 | 诊断不泄露第三方 backend 类型或拥有外部资源 | unit | `ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |

## Wave 0 要求

现有仓库没有测试基础设施，因此第 1 个计划必须创建以下基础：

- [ ] `tests/test_runner.h`：纯 C 极小断言和测试注册宏。
- [ ] `tests/test_main.c`：测试入口。
- [ ] `CMakeLists.txt`：启用 CTest，并提供 `RTC_BUILD_TESTS`、`RTC_BUILD_EXAMPLES` 选项。

## 仅人工验证

| 行为 | 需求 | 为什么人工 | 验证说明 |
|------|------|------------|----------|
| 文档可读性 | API-05 | 自动化只能检查关键词，不能判断说明是否足够清楚 | 阅读 `docs/API-执行器与内存契约.md`，确认每个公开 API 和 observer/datagram 回调都有执行器亲和说明 |

## 验证签署

- [x] 所有任务包含自动验证或 Wave 0 依赖。
- [x] 没有连续 3 个任务缺少自动验证。
- [x] Wave 0 覆盖缺失的测试基础设施。
- [x] 命令不使用 watch 模式。
- [x] 反馈延迟小于 30 秒。
- [x] `nyquist_compliant: true` 已设置。

**审批：** pending
