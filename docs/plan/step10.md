## Step10：可观测性与资源边界补齐（Kinesis 风格，核心扩展版）

### 摘要
- 对齐 `amazon-kinesis` 的“状态机可追踪 + 指标可诊断”风格，结合 `libdatachannel` 的“高频告警限频计数”模式。
- 保持现有 `rtc_log_callback_t` 签名，不新增线程，不引入无界容器，不引入隐藏分配。
- 允许统一日志文案（优先可观测一致性），并同步更新测试断言。

### 公共接口/类型变更
- 在 `rtc_peer_stats_t` 末尾追加以下字段（保持尾部追加兼容策略）：
  - `uint16_t stun_rx_queue_depth`
  - `uint16_t stun_rx_queue_high_watermark`
  - `uint16_t dtls_rx_queue_depth`
  - `uint16_t dtls_rx_queue_high_watermark`
  - `uint16_t ice_stun_tx_queue_depth`
  - `uint16_t ice_stun_tx_queue_high_watermark`
  - `uint32_t transport_rx_drop_pkts`
  - `uint32_t transport_stun_drop_pkts`
  - `uint32_t transport_dtls_drop_pkts`
  - `uint32_t transport_io_error_count`
  - `uint32_t queue_overflow_count`

### 关键实现变更（按顺序）
1. **补齐有界队列观测面**
- 在 ICE 的 `stun_out_queue` 增加 `high_watermark` 追踪，入队时更新，reset 时清零。
- 补 ICE getter：`stun_out_depth/high_watermark`，供 session 统一汇总到 peer stats。
- transport 侧补一个聚合 getter：`transport_io_error_count`（汇总 tx/rx/stun/dtls error 计数）。

2. **统一日志上下文（不改回调签名）**
- 新增统一日志辅助函数，格式固定为：`evt=<event> state=<peer_state> <k=v...>`。
- module/peer/code 继续走现有回调参数；state 与关键参数进入 message。
- 对 WARN/ERROR 和关键状态切换日志统一迁移到该格式。
- 高频日志统一“首个 + 每64次”限频策略（复用已有节流函数）。

3. **补错误计数与队列水位汇总**
- 在 session 汇总函数中统一刷新：
  - RTP/STUN/DTLS/ICE 队列 depth + high watermark
  - drop/error 细分计数
- 在所有 `RTC_ERR_OVERFLOW` 路径统一 `queue_overflow_count++`，避免遗漏。

4. **资源边界显式化**
- 对已有固定容量缓存/列表保持原上限，不改容量策略。
- 增加配置期校验（`#if/#error`）覆盖尚未显式校验的关键容量宏，确保“0 或非法组合”在编译期失败。

### 测试计划（确定性压力单测）
1. 扩展 API 测试
- 连接成功后断言新增 stats 字段可读且不越界（depth<=cap，high_watermark<=cap）。
- 构造队列满/协议异常路径，断言 `queue_overflow_count`、细分 drop/error 计数增长。
- 日志捕获改为断言包含 `evt=` 与 `state=` 关键片段（避免对完整文案过度脆弱）。

2. 扩展 transport 压力测试
- 固定循环压力注入 STUN/DTLS/RTP，断言所有队列深度与高水位始终受上限约束。
- 人为触发队列满，断言对应 drop/error 计数增长且无崩溃。

3. 全量回归
- 执行 `ctest --output-on-failure`，通过后作为步骤10验收基线。

### 假设与默认
- 参考对齐优先级：`amazon-kinesis`（主） > `libdatachannel`（限频计数） > `libpeer/metaRTC`（仅作对照）。
- 不新增结构化日志回调；现有回调签名保持不变。
- 本步采用“核心扩展”而非“全量扩展”，控制结构体增量与改动面。
- 预计内存增量为 `rtc_peer_stats_t` 与 ICE 上下文固定字段常量级增长；实时性影响为常量级计数与少量格式化，无新增线程。
