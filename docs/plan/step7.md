# Step7：SRTP Role 激活与 RTP/RTCP 路径收敛设计

## Summary
- 目标：完成步骤7，修复 DTLS->SRTP key role 映射错误，补齐 SRTP/SRTCP protect/unprotect 基础路径，增强认证失败可观测性，保持会话销毁/重建安全。
- 执行策略：采用“正确性优先”路线，关闭默认本地镜像回环兼容开关，避免自发包伪接收掩盖 key 映射问题。
- 边界：不在本步实现 RTCP 业务闭环解析与反馈（保留到步骤9）。

## 参考对齐
- `libpeer/src/dtls_srtp.c::dtls_srtp_key_derivation`：按 DTLS 角色选择 local/remote key，并分别创建 inbound/outbound policy。
- `amazon-kinesis.../PeerConnection.c::allocateSrtp` + `SrtpSession.c`：receive/transmit key 明确分离，RTP 与 RTCP 分别走 protect/unprotect 接口。
- `libdatachannel/src/impl/dtlssrtptransport.cpp`：inbound/outbound key 与 client/server 角色绑定，SRTP/SRTCP 路径分离。
- `metaRTC/libmetartccore8/src/yangutil/sys/YangSRtp.c`：`enc/dec rtp` 与 `enc/dec rtcp` 分函数、独立可观测错误路径。

## Implementation Changes
- SRTP 模块：新增 `rtc_srtp_protect_rtcp()` 与 `rtc_srtp_unprotect_rtcp()`，沿用现有会话所有权与错误映射；RTP 路径继续用 `rtc_srtp_protect/unprotect`，不引入动态分配。
- Session 安全激活：DTLS 连接后按角色映射 key；`is_server=1` 时 `outbound=server_write_key`、`inbound=client_write_key`，`is_server=0` 时反向；移除当前“入出都取 client_write_key”的行为。
- 可观测性：`rtc_peer_stats_t` 末尾追加 `srtp_unprotect_fail`、`rtcp_rx_pkts`；认证失败时必增计数并保留错误码，日志按“首个 + 每64次一次”限频 `WARN`，避免热路径刷屏。
- Transport 兼容开关：`mirror_loopback` 默认改为关闭；仅测试或明确场景下显式开启，不再作为默认媒体回环路径。
- 生命周期：保持现有 `stop/destroy` 的 `rtc_srtp_deinit/init` 序列；补齐重启路径下 SRTP 上下文重建验证，不改线程模型与模块边界。

## Test Plan
- 新增 SRTP 单测（新增独立测试目标）：验证 role 分离后“本端保护包不能被本端 inbound 解密”；验证“对端交换 key 后可解密”；覆盖 RTP 与 RTCP protect/unprotect；覆盖认证失败返回码与计数。
- 调整现有 API 测试：去除对默认自回环接收帧的依赖，改为验证连接成功、发送路径与 stats；新增“注入非法 SRTP/SRTCP 数据包后计数+日志可见且连接不崩溃”场景。
- 生命周期测试：新增“连接->stop->start->再连接”场景，确认 SRTP 上下文可安全销毁重建。
- 回归：`ctest --output-on-failure` 全量通过，重点确认 `rtc_api_test`、`rtc_transport_test`、`rtc_interop_smoke_test` 与新增 SRTP 测试全绿。

## Assumptions
- 本步默认设备侧 DTLS 角色仍为 server（来自 `setup:passive`），但实现保留双角色映射逻辑以兼容后续扩展。
- 公开 stats 采用“结构体尾部追加”策略，保持 size 兼容调用方式。
- 内存开销增量为每 peer 固定字段（两个 `uint32_t`）与测试代码；无无界容器、无新增后台线程。
- 实时性影响可控：新增分支与计数为 O(1)，日志限频后不会引入高频不可关闭输出。

