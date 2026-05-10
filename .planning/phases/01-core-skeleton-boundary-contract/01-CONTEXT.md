# 第 1 阶段：核心骨架与边界契约 - 上下文

**Gathered:** 2026-05-10T16:13:14+08:00
**Status:** 准备进入规划

<domain>
## 阶段边界

本阶段交付后续 SDP/JSEP、ICE/STUN、DTLS-SRTP、RTP/RTCP 必须遵守的公共边界：纯 C 公共 API、固定 arena 与 limits、执行器亲和、observer、trace、计数器、CMake 静态库构建、最小 create/destroy 示例和基础测试骨架。

本阶段不实现 SDP 生成解析、ICE、DTLS/SRTP、RTP/RTCP 媒体传输或 Chrome 端到端互通；这些能力只允许以 API 形状或占位返回码的方式预留。

</domain>

<decisions>
## 实现决策

### 公共 API 边界

- **D-01:** `rtc_peer_connection_config_t` 使用顶层总配置加子结构的组织方式，至少分出 `arena`、`limits`、`platform`、`executors`、`observer`、`security_backend` 等职责块。
- **D-02:** 公开 API 命名接近浏览器 WebRTC 语义，但采用 C 风格命名，例如 `rtc_peer_connection_create_offer`、`rtc_peer_connection_set_local_description`。
- **D-03:** `PeerConnection` 使用不透明句柄，用户传入总 arena，库在 arena 内分配对象和内部资源；公共头不暴露 `rtc_peer_connection_t` 的真实布局。
- **D-04:** 第 1 阶段应在公共头中占位核心生命周期 API 声明，包括 offer/answer/description/candidate 相关签名草案；暂未实现的协议能力可以返回稳定的未支持状态。

### 固定内存与 limits 模型

- **D-05:** `limits` 按子系统分组，例如 `sdp`、`ice`、`dtls`、`rtp`、`rtcp`、`trace`，避免后续所有容量字段堆在一个扁平结构中。
- **D-06:** 容量不足错误需要精确到资源类别，例如 ICE candidates、SDP buffer、timer slots、packet cache、trace buffer 等。
- **D-07:** arena 切分失败时，应通过可选诊断结构返回各资源类别的 `required`、`used` 或等价容量信息，不能只依赖文档公式或 observer。
- **D-08:** 第 1 阶段必须建立禁止隐式动态内存的测试或钩子。核心代码只走项目 allocator 包装，测试替身统计创建后分配次数。

### 执行器亲和契约

- **D-09:** 公开 API 按职责分散到对应 executor：signaling API 走 `signaling`，media API 走 `media`，network API 走 `network`。
- **D-10:** UDP datagram 输入 API 必须在 `network` executor 调用；第 1 阶段不提供任意线程输入后内部转投的队列语义。
- **D-11:** observer 回调按事件归属触发：信令状态和错误在 `signaling`，媒体帧在 `media`，datagram、ICE、DTLS、SRTP 相关事件在 `network`。
- **D-12:** 执行器亲和违规时，debug 构建触发断言；release 构建返回稳定错误码，便于生产环境恢复和排障。

### observer、trace 与计数器

- **D-13:** observer 采用两层设计：核心 API 提供细粒度 vtable 回调，可选提供统一事件 adapter 供日志、测试或示例复用。
- **D-14:** trace hook 从第 1 阶段开始定义稳定事件名和字段键；本阶段不必枚举所有协议事件，但必须锁定命名规则、字段稳定性策略和基础生命周期事件。
- **D-15:** 计数器通过快照结构读取，例如 `rtc_peer_connection_get_counters` 返回按子系统分组的 counters。
- **D-16:** 错误事件包含稳定错误码、子系统、操作和可选 detail code。文本消息只能作为辅助信息，不能成为主要稳定契约。

### 项目骨架与测试策略

- **D-17:** 首批目录按子系统提前分层，例如 `src/api`、`src/memory`、`src/executor`、`src/observability`，并配套 `include/rtc`、`tests`、`examples`、`cmake`。
- **D-18:** 测试使用纯 C 自研极小 test runner，保持零第三方测试依赖；CMake/CTest 可以只负责构建和调度。
- **D-19:** malloc/动态内存防线采用项目 allocator 包装和测试替身统计。源码扫描和 code review 可作为辅助，但不能替代行为测试。
- **D-20:** 示例和文档范围限定为最小 create/destroy 示例、API 执行器亲和文档和固定内存边界文档；不提前搭 Chrome 页面 skeleton。

### 智能体的自由裁量

用户未要求 planner 在上述决策之外扩展新能力。实现细节可以由 planner 在不突破本阶段边界的前提下决定，例如具体文件名、内部 helper 拆分、测试 runner 的宏名称和 CMake target 名称。

</decisions>

<canonical_refs>
## 权威引用

**下游智能体在规划或实现前必须阅读这些文档。**

### 项目范围与需求

- `.planning/PROJECT.md`：项目价值、首版边界、关键决策和不在范围内事项。
- `.planning/REQUIREMENTS.md`：第 1 阶段覆盖的 API、MEM、EXE、OBS、BLD 需求及需求追踪。
- `.planning/ROADMAP.md`：第 1 阶段目标、成功标准和后续阶段边界。
- `.planning/STATE.md`：当前项目状态和工作流配置。

### 设计边界与研究

- `docs/000-设计边界记录.md`：已确认的纯 C、固定内存、无线程、执行器、网络、媒体、加密和可观测性边界。
- `.planning/research/SUMMARY.md`：技术栈、必备能力、主要风险和构建顺序摘要。
- `.planning/research/ARCHITECTURE.md`：分层状态机架构、主要组件、数据流和架构风险。
- `.planning/research/PITFALLS.md`：固定内存、执行器亲和、可观测性等第 1 阶段风险和预防策略。

</canonical_refs>

<code_context>
## 既有代码洞察

### 可复用资产

- 当前仓库尚无 C 源码、公共头、CMake target 或测试 runner。第 1 阶段需要创建初始骨架。

### 既有模式

- 规划文档已经锁定纯 C、固定 arena、无线程、用户负责 UDP/socket、CMake 静态库和宽松许可依赖策略。
- Markdown 文档必须使用中文；技术标识符、API 名称、协议名和需求 ID 可以保留英文。

### 集成点

- 新增实现应围绕 `include/rtc`、`src/api`、`src/memory`、`src/executor`、`src/observability`、`tests`、`examples`、`cmake` 建立。
- 构建系统需要从一开始导出静态库目标和公共头文件，满足 BLD-01、BLD-02。

</code_context>

<specifics>
## 具体想法

- API 风格应接近浏览器 WebRTC 语义，但保持纯 C 命名和不透明句柄。
- 固定内存不是仅靠文档约束，必须在第 1 阶段变成可测试契约。
- 可观测性不是后补日志，trace 事件名、字段键、错误结构和计数器快照都属于公共边界的一部分。

</specifics>

<deferred>
## 延后事项

无。本次讨论保持在第 1 阶段范围内。

</deferred>

---

*阶段：1-核心骨架与边界契约*
*上下文收集时间：2026-05-10T16:13:14+08:00*
