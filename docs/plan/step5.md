# 步骤5实施计划：Session 状态机接入真实 ICE 事件并显式上报失败原因

## Summary
- 目标：完成 `STARTING -> ICE_CHECKING -> DTLS_HANDSHAKE` 的真实事件驱动推进，并把 ICE 失败原因明确区分为 `TIMEOUT / PROTOCOL / RESOURCE_EXHAUSTED`。
- 已锁定决策：
1. 失败策略采用“仅致命失败”。
2. 对齐 `amazon-kinesis` 的显式状态机+sticky error 风格。
3. 本步扩展公开 stats，但字段范围限定为步骤5最小集。

## Implementation Changes
- 在 [rtc_session.c](/home/qshl/dev/rtc/s1/src/session/rtc_session.c) 重构 ICE 轮询路径为“事件+致命错误分类”：
1. `rtc_peer_drive_ice_transport_io(...)` 改为返回“是否致命+错误码”。
2. 致命错误判定规则固定为：
- `rtc_ice_handle_incoming_stun()` 返回 `RTC_ERR_RESOURCE_EXHAUSTED` 或 `RTC_ERR_OVERFLOW`：立即失败，统一映射为 `RTC_ERR_RESOURCE_EXHAUSTED`。
- `rtc_ice_handle_incoming_stun()` 返回 `RTC_ERR_PROTOCOL` 或 `RTC_ERR_AUTH_FAILED` 且来源地址属于已配置 remote candidate：立即失败，映射为 `RTC_ERR_PROTOCOL`。
- `rtc_transport_send_stun()` / `rtc_ice_dequeue_outgoing_stun()` 非 `RTC_ERR_TIMEOUT` 的内部异常：立即失败，映射为 `RTC_ERR_PROTOCOL`（资源类保持资源错误）。
- 其它异常（未知来源畸形包、would-block、噪声）只计数+日志，不触发失败。
3. `ICE_CHECKING` 分支执行顺序固定：
- 先处理 ICE/transport I/O；
- 若出现致命错误，直接 `RTC_PEER_STATE_FAILED` 并上报对应 code；
- 否则继续 `rtc_ice_tick()`；
- `ice_event.connected` 才进入 `DTLS_HANDSHAKE`；
- `ice_event.failed` 保持 `RTC_ERR_TIMEOUT` 路径。
4. 状态失败日志文案固定区分：
- `ice timeout`
- `ice protocol failure`
- `ice resource exhausted`

## Public API / Type Changes
- 在 [rtc.h](/home/qshl/dev/rtc/s1/include/rtc/rtc.h) 的 `rtc_peer_stats_t` 末尾追加：
1. `uint32_t ice_checks_sent`
2. `uint32_t ice_checks_ok`
3. `uint32_t ice_checks_failed`
4. `uint32_t ice_checks_drop`
5. `int16_t ice_last_error`
- 在 session 轮询中同步填充上述字段；进入 ICE 失败态时写入 `ice_last_error`。

## Test Plan
- 在 [test_rtc_api.c](/home/qshl/dev/rtc/s1/test/test_rtc_api.c) 增强并新增用例：
1. 成功路径状态序列断言：至少出现 `STARTING -> ICE_CHECKING -> DTLS_HANDSHAKE`（最终到 `CONNECTED`）。
2. `ICE timeout`：不给有效候选/响应，进入 `FAILED`，断言 `ice_last_error == RTC_ERR_TIMEOUT`，日志 code 一致。
3. `ICE protocol`：从已知 remote candidate 地址注入错误事务/认证 STUN 响应，断言进入 `FAILED` 且 `ice_last_error == RTC_ERR_PROTOCOL`。
4. `ICE resource exhausted`：填满 remote candidate 容量后，从新地址注入合法 STUN request 触发动态候选添加失败，断言 `ice_last_error == RTC_ERR_RESOURCE_EXHAUSTED`。
5. 回归：`ctest --output-on-failure`（`build` 与 `build-default`）保持全绿。

## Assumptions
- 不引入新线程；保持单线程 `poll/tick`。
- 不引入无界容器或隐藏分配；仅固定结构体字段增加。
- 预计内存增量仅来自 `rtc_peer_stats_t` 固定字段（每 peer 常量级增长），实时性影响为常量级分支与计数更新。
- 步骤5不触碰 DTLS 真网络握手逻辑（步骤6范围）。

