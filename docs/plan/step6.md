# 步骤6实施方案：DTLS 真网络握手替换内部对打

## Summary
- 目标：将当前“内存内 client/server 对打 DTLS”替换为“真实网络 DTLS datagram 握手”，保持单线程 `poll/tick` 驱动、无无界内存、失败可观测。
- 对齐路线：主对齐 `amazon-kinesis` 的显式 DTLS 状态与非阻塞计时驱动；借鉴 `libdatachannel` 的 DTLS/RTP 一字节 demux 规则；保留 `libpeer` 的指纹核验与 keying material 导出模式。
- 本步范围：完成握手链路与失败闭环，包含 DTLS 收发计数字段；不做 Step7 的 SRTP role-key 映射修正。

## Implementation Changes
- `transport`（[rtc_transport.c](/home/qshl/dev/rtc/s1/src/transport/rtc_transport.c)）：
  - 增加固定容量 DTLS RX 队列（有界、无堆分配）与对应高水位/丢包计数。
  - `pump_io` 收包 demux 顺序固定为：`STUN` -> `DTLS(首字节20~63)` -> `RTP/RTCP`，避免把 DTLS 误判为协议错误。
  - 新增 DTLS 出站发送接口（与 STUN 发送风格一致，非阻塞、显式 would-block/error）。
- `dtls`（[rtc_dtls.c](/home/qshl/dev/rtc/s1/src/dtls/rtc_dtls.c)）：
  - 移除内部双端点对打，改为单端点（server/passive）握手状态机。
  - 保留并复用 mailbox 思路：入站 mailbox 由 session 喂入网络 DTLS 包，出站 mailbox 暂存待发送握手包。
  - `tick` 内驱动 `mbedtls_ssl_handshake` + 计时器回调重传；完成后执行远端证书 SHA-256 指纹校验（与 SDP `remote_fingerprint` 对比）。
  - 握手成功后导出 `EXTRACTOR-dtls_srtp` keying material；失败统一落 `RTC_ERR_DTLS_HANDSHAKE_FAILED` 并记录可观测原因。
- `session`（[rtc_session.c](/home/qshl/dev/rtc/s1/src/session/rtc_session.c)）：
  - `DTLS_HANDSHAKE` 状态每次 poll 固定顺序：
    1) pump transport I/O；
    2) dequeue DTLS 入站包并喂给 dtls ctx；
    3) `rtc_dtls_tick`；
    4) flush DTLS 出站 mailbox 到 transport；
    5) 根据 `dtls_event` 决定 `CONNECTED/FAILED`。
  - ICE 成功后仅启动 DTLS server 握手（与 Answer `setup:passive` 一致）。
  - 失败日志保持模块化上下文：`module=dtls/session.peer` + `peer_id` + `code` + 阶段文案。
- Public API / types：
  - 在 `rtc_peer_stats_t` 末尾追加：`dtls_rx_pkts`、`dtls_tx_pkts`（保持尾部追加兼容策略）。
  - 不新增线程 API，不改外部调用模型。

## Test Plan
- API 集成（[test_rtc_api.c](/home/qshl/dev/rtc/s1/test/test_rtc_api.c)）：
  - 正向：远端测试桩同时处理 STUN + 真实 DTLS client 握手，断言 `STARTING -> ICE_CHECKING -> DTLS_HANDSHAKE -> CONNECTED`，并断言 `dtls_state=CONNECTED`、`dtls_rx_pkts>0`、`dtls_tx_pkts>0`。
  - 负向指纹：构造“offer 指纹与远端测试证书不匹配”场景，断言进入 `FAILED`，`dtls_last_error == RTC_ERR_DTLS_HANDSHAKE_FAILED`，日志含 dtls fail 上下文。
- transport 回归：
  - 新增 DTLS demux 用例：DTLS 报文进入 DTLS 队列，不触发 RTP 解析错误路径。
  - 新增 DTLS 队列满用例：丢包计数增长且行为有界。
- 全量回归：
  - `ctest --output-on-failure` 在 `build-default` 与 `build` 通过。
- 手工验收：
  - 保留 Chrome LAN 握手 runbook，验证握手可达 `CONNECTED` 且 DTLS 计数增长。

## Assumptions
- 已确认采用 `Kinesis` 风格非阻塞握手驱动（非 `libpeer` 阻塞式）。
- 本步一并落地 DTLS 收发计数；`srtp` role-key 映射修正在 Step7 处理。
- 继续遵守：无新增后台线程、无隐藏分配、所有队列硬上限、错误路径显式日志与计数。

