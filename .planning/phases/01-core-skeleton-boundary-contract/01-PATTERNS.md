# 第 1 阶段：代码模式映射

## 结论

当前仓库尚无 C 源码、公共头、CMake target、测试 runner 或示例代码，因此没有可直接复用的实现模式。第 1 阶段应创建项目的第一套代码模式，供后续协议层复用。

## 计划创建的模式

| 角色 | 目标路径 | 模式说明 |
|------|----------|----------|
| 公共聚合头 | `include/rtc/rtc.h` | 用户只需包含一个入口头，也允许按模块包含细分头 |
| API 与生命周期 | `src/api` | 保存不透明 `rtc_peer_connection_t` 的真实结构和生命周期实现 |
| 固定内存 | `src/memory` | arena 对齐、切分、容量诊断和 allocator 包装 |
| 执行器 | `src/executor` | 三执行器 vtable、当前亲和标记、post/timer 辅助 |
| 可观测性 | `src/observability` | observer 调用、trace 事件、计数器快照 |
| 测试基础 | `tests` | 纯 C 极小 test runner 和单元测试 |
| 示例 | `examples` | 最小 create/destroy 示例 |
| 文档 | `docs` | 中文 API 亲和和固定内存边界文档 |

## 对执行者的约束

- 不要把协议实现提前塞进 `PeerConnection`；第 1 阶段只建立边界和占位。
- 不要在公共头暴露内部结构体布局。
- 不要引入第三方测试库或 GPL/LGPL 依赖。
- 不要绕过 allocator 包装直接在核心代码中使用动态内存。
