# 步骤2：Transport 迁移到真实 UDP（保留兼容）实施计划

## Summary
- 目标：把当前内存 `loopback` 传输迁移为“非阻塞 UDP + 有界 TX/RX ring”，保持单线程 `poll/tick` 驱动。
- 参考对齐：采用你确认的 `Kinesis` 风格（非阻塞、`EAGAIN` 语义、可观测重试/错误处理），并保留 `libpeer/metaRTC` 的轻量 socket 抽象习惯；不采用 `libdatachannel` 的线程/外部栈模式。
- 兼容策略：你确认采用“发送后镜像入本地 RX（临时）”，保证现有单 peer 媒体环回测试不中断。

## Key Changes
- 在 [rtc_platform.h](/home/qshl/dev/rtc/s1/src/platform/rtc_platform.h) 与 [rtc_platform.c](/home/qshl/dev/rtc/s1/src/platform/linux/rtc_platform.c) 增加最小 UDP 平台抽象：
  `create/bind(non-block)/sendto/recvfrom/close`，上层不直接依赖 Linux socket 细节。
- 在 [rtc_transport.c](/home/qshl/dev/rtc/s1/src/transport/rtc_transport.c) 将 `pump_loopback` 扩展为 `pump_io`：
  先发 TX ring（遇 `EAGAIN` 停止本轮），再收 socket 入 RX ring（满则丢弃并计数），返回本轮 `sent/recv/dropped`。
- `transport ctx` 扩展固定字段（无动态分配）：socket 句柄、本地端口、远端地址有效位、兼容镜像开关、I/O 统计计数。
- `session` 在 `rtc_peer_add_remote_candidate()` 做最小 candidate 解析（先支持 host IPv4:port），成功后下发 transport 远端地址；`CONNECTED` 态改调 `pump_io`。
- 临时兼容开关默认开启：UDP 发送成功后镜像一份入本地 RX ring，确保当前单 peer 测试连续通过（后续步骤再移除）。

## Public Interfaces / Types
- `include/rtc/rtc.h` 本步骤不改公开 API/结构体（按你的选择：`local_udp_port` 暂不引入）。
- 内部接口新增/调整（platform/transport/session 内部函数签名），保持调用方 API 向后兼容。
- candidate 解析失败策略：不静默吞错，记录 `WARN` 日志；保持现有 `rtc_peer_add_remote_candidate` 行为可预测（不引入破坏性行为变化）。

## Test Plan
- 新增 transport 级测试：
  1. UDP 同进程回环（TX->UDP->RX 可出队）。
  2. RX 队列满时丢弃与计数增长（`RTC_ERR_OVERFLOW` 语义一致）。
  3. 高频 pump 下队列深度/高水位不越界。
- 保持并回归现有测试：
  `ctest --output-on-failure` 需全绿（`rtc_api_test` + `rtc_interop_smoke_test`）。
- 验收对齐步骤2：
  本地 UDP 收发可用；队列满行为可观测；无无界内存增长；无新增线程。

## Assumptions
- 仍是单线程 `rtc_engine_poll()` 驱动，不引入后台任务。
- 步骤2优先落地 IPv4 host candidate 的最小解析；更完整 SDP/candidate 语义放在步骤3。
- 兼容镜像机制为阶段性措施，后续随真实互通链路完善逐步下线。

