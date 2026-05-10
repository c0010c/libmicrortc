# 第 1 阶段：核心骨架与边界契约 - 研究

## 研究完成

本研究回答一个问题：怎样规划第 1 阶段，才能把纯 C、固定内存、无线程、执行器亲和和生产可观测性变成后续协议层可依赖的硬边界。

## 研究结论

### 推荐实现切分

第 1 阶段应拆成四个递进计划：

1. **构建与公共骨架**：建立 CMake 静态库、公共头导出、目录结构、最小测试 runner 和示例目标。
2. **固定内存与错误契约**：实现 arena、limits、容量诊断、`rtc_status_t` 和 allocator 包装测试防线。
3. **执行器与生命周期**：实现三执行器 vtable、亲和检查、`PeerConnection` 不透明句柄、create/destroy 和未支持 API 占位。
4. **可观测性、文档与示例**：实现 observer、trace hook、计数器快照、最小 create/destroy 示例和执行器/内存契约文档。

这个顺序让后续 SDP、ICE、DTLS-SRTP 和 RTP/RTCP 不需要重新发明内存、错误、调度或诊断规则。

### API 形状

- 公共头建议以 `include/rtc/rtc.h` 为聚合入口，拆出 `status.h`、`config.h`、`limits.h`、`executor.h`、`observer.h`、`peer_connection.h`。
- `rtc_peer_connection_t` 必须是不透明类型，真实结构留在 `src/api` 或内部头中。
- `rtc_peer_connection_config_t` 使用分组结构，至少包含 arena、limits、platform、executors、observer、security_backend。
- `createOffer`、`createAnswer`、`setLocalDescription`、`setRemoteDescription`、`addIceCandidate` 等接口可以在第 1 阶段声明为 C 风格名称，并返回 `RTC_STATUS_UNSUPPORTED` 或等价稳定状态，避免第 2 阶段再大改公共形状。

### 固定内存策略

- 固定内存不能只靠“不要 malloc”的文档约束。计划必须要求核心代码只通过项目 allocator 包装使用内存，测试替身能统计创建后分配次数。
- arena 切分应先实现对齐、容量检查、诊断输出和已用/所需字节统计，再把 `PeerConnection` 对象和基础子系统状态挂上去。
- `limits` 必须按子系统分组，即使第 1 阶段部分字段只是为后续预留，也应在默认值和容量错误里可见。
- 容量不足需要稳定、可诊断的状态码或错误类别，能指出是 SDP buffer、ICE candidates、timer slots、packet cache、trace buffer 等资源不足。

### 执行器亲和策略

- 三执行器都是用户提供的 `post + timer` 抽象，库不创建线程，也不拥有事件循环。
- 第 1 阶段的重点不是实现复杂异步队列，而是锁定每类 API 和 observer 回调的归属 executor。
- 亲和违规要有双层行为：debug 构建断言，release 构建返回稳定错误码。
- 测试 executor 应能模拟当前执行器身份、记录 post/timer 调用，并验证 create/destroy、未支持 API 和 observer 回调的亲和规则。

### 可观测性策略

- observer 应以细粒度 vtable 为核心，事件 adapter 可作为辅助层，避免用户直接处理复杂 union。
- trace hook 从第 1 阶段就要定义稳定事件名和字段键，至少覆盖生命周期、容量失败、亲和违规和未支持 API。
- 计数器使用快照结构，按子系统分组。第 1 阶段至少覆盖生命周期、容量错误、亲和错误、未支持 API、trace 丢弃或写入次数。
- 错误事件应包含稳定错误码、子系统、操作和可选 detail code；文本消息只能作为辅助，不应成为稳定契约。

## 需要避免的规划陷阱

- 不要把第 1 阶段变成 SDP、ICE 或 RTP 的提前实现；只能声明接口形状和返回未支持状态。
- 不要把固定内存测试推迟到后续阶段；否则后续协议层会自然滑向隐式动态内存。
- 不要只建 CMake 和空头文件；第 1 阶段成功标准要求 create/destroy、limits 诊断、执行器亲和、observer、trace、计数器和文档都能被验证。
- 不要引入第三方测试框架；纯 C 极小 test runner 足够，且更符合宽松许可和嵌入式边界。

## Validation Architecture

### 自动化验证维度

- **构建验证**：`cmake -S . -B build` 和 `cmake --build build` 必须生成静态库、测试目标和最小示例目标。
- **公共头验证**：测试目标和示例只包含 `include/rtc` 下的公共头，不直接包含 `src` 内部头。
- **固定内存验证**：容量不足测试能得到具体资源类别；创建成功后测试替身统计不到运行期动态增长。
- **执行器验证**：测试 executor 能验证 signaling、media、network API 的亲和规则，release 路径返回稳定错误。
- **可观测性验证**：observer、trace 和 counters 能记录生命周期、错误和基础状态事件。
- **文档验证**：Markdown 文档为中文，并明确 API、observer、datagram 和 timer 的执行器亲和。

### 推荐命令

- 快速验证：`cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`
- 文档检查：`rg -n "执行器|arena|limits|rtc_status_t|trace|计数器" docs include tests examples`
- 边界检查：`rg -n "malloc|calloc|realloc|free" src include tests examples`

## 规划输入

- `.planning/phases/01-core-skeleton-boundary-contract/01-CONTEXT.md`
- `.planning/REQUIREMENTS.md`
- `.planning/ROADMAP.md`
- `.planning/STATE.md`
- `docs/000-设计边界记录.md`
- `.planning/research/ARCHITECTURE.md`
- `.planning/research/PITFALLS.md`
