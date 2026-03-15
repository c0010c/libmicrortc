## Step 4 方案A：ICE-Lite 连通路径落地（Transport 分流 + ICE 候选对状态机）

### Summary
- 目标：在不引入新线程、保持有界内存和可观测性的前提下，完成步骤4要求：`STUN Binding Request/Response`、候选对状态 `WAITING/INPROGRESS/SUCCEEDED/FAILED`、超时/失败路径。
- 已锁定偏好：`先打通`、`严格 STUN 校验`、`按 priority 选路`、`未知来源动态加入`、`主动+被动混合检查`、`统计先做内部计数`、`API 集成测试优先`。
- 参考对齐：行为对齐 `libpeer`（候选对+周期检查）、`amazon-kinesis`（显式状态推进与超时失败）、`metaRTC`（动态候选/连通轮询）、`libdatachannel`（状态与超时可观测）。

### Key Changes
- `transport` 层（主改动入口：`src/transport/rtc_transport.*`）
  - 新增“控制面 STUN 收包分流”：在 `pump_io` 收包时按 STUN 头粗判（magic cookie + 类型位）分流到固定容量 STUN RX 队列，保留源地址。
  - RTP 路径保持原有有界队列；STUN 与 RTP 生命周期分离，避免 session/ice 直接碰 socket。
  - 新增有界 API：`dequeue_stun(...)`、`send_stun(...)`（或等价内部接口），均为非阻塞、显式返回码、无隐藏分配。
  - 队列满、协议异常、would-block 均计数并可日志定位。

- `ice` 层（主改动入口：`src/ice/rtc_ice.*`）
  - 新增候选对对象（固定容量）与状态机：`WAITING -> INPROGRESS -> SUCCEEDED/FAILED`，按 remote candidate `priority` 排序尝试。
  - 新增严格 STUN 编解码/校验（最小必需集合）：
    - 处理 Binding Request/Response；
    - 校验 Header/Length/Cookie、`USERNAME`、`MESSAGE-INTEGRITY`、`FINGERPRINT`、`transaction-id`；
    - 解析 `PRIORITY`，用于未知来源动态加入时的排序。
  - 主动+被动混合检查：
    - 主动：按 `retry_interval_ms/max_retries` 发送 Binding Request；
    - 被动：接收有效 Request 后回 Binding Response。
  - 未知来源策略：有效且可解析的 STUN Request 触发动态远端候选加入（受上限约束）；上限满则丢弃并告警。
  - 新增内部计数：`checks_sent/checks_ok/checks_failed/checks_drop`（保留在 ICE/session 内部，不改公开结构体）。

- `session` 层（主改动入口：`src/session/rtc_session.c`）
  - `ICE_CHECKING` 阶段每次 poll：
    - 先 `transport_pump_io`；
    - 消费 STUN RX 队列并喂给 ICE；
    - 发送 ICE 产出的待发 STUN。
  - 仅在 ICE 候选对 `SUCCEEDED` 后触发 `DTLS_HANDSHAKE`，失败保持显式错误码和状态迁移。
  - 日志补齐：检查发送、响应成功、候选切换、动态加入、超时失败、资源耗尽等关键路径。

- 可观测性与资源边界
  - 所有新增队列/列表均固定上限（复用现有 `RTC_CFG_MAX_REMOTE_CANDIDATES/RTC_CFG_MAX_CANDIDATE_PAIRS`）。
  - 热路径日志默认 DEBUG/WARN 分级，避免不可关闭高频 INFO。
  - 失败原因区分：校验失败、超时、资源耗尽、非法输入。

### Public Interfaces / Types
- `include/rtc/rtc.h`：本步骤不改公开 API 与 `rtc_peer_stats_t`（按你的选择，`ice_checks_sent/ok` 先不外露）。
- 内部类型扩展：
  - `rtc_transport_ctx_t` 增加 STUN RX 有界队列与计数字段；
  - `rtc_ice_ctx_t` 增加候选对数组、事务跟踪、内部计数字段；
  - `rtc_ice_event_t` 仅补内部需要的状态推进信息，不引入破坏性公开变更。

### Test Plan
- 以 `API 集成测试` 为主，补充 transport 行为回归：
  1. 注入合法 Binding Request（含 MI/Fingerprint）可驱动候选对 `INPROGRESS->SUCCEEDED` 并推进到 DTLS 阶段。
  2. 无远端候选/无有效 STUN 响应时按重试上限进入 FAILED（`RTC_ERR_TIMEOUT`）。
  3. 多候选按 priority 选择：高优先级失败后可回退到下一候选并成功。
  4. 未知来源合法 STUN Request 触发动态候选加入并可连通。
  5. 非法 STUN（坏 MI/Fingerprint/USERNAME）被拒绝，不得误推进状态。
  6. 队列满与资源上限触发时：返回码、日志、计数一致且无崩溃。
- `transport` 回归：STUN 分流后不破坏 RTP 既有收发/高水位/有界行为。

### Assumptions
- 首版仅覆盖 IPv4 host candidate；动态加入按 peer-reflexive 语义落地，但不扩展 TURN/STUN server 发现流程。
- ICE-Lite 仍保持 Answer 端定位（`a=ice-lite`），但在实现上采用“主动+被动混合”以对齐参考实现和当前重试框架。
- 本步骤不引入新线程，不引入无界容器，不增加不可控堆增长；仅增加固定大小结构体内存。
