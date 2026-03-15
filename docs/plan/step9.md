### Step9：RTCP 闭环增强（Kinesis 风格，含 NACK 重传）

#### Summary
- 目标：在现有 `SRTCP unprotect` 基础上，完成 `RR/PLI/NACK` 解析闭环、PLI 按流回调、NACK 有界重传，并补齐端到端联测。
- 范围：不做拥塞控制、不做 TWCC/REMB、不改线程模型；保持 `poll/tick` 驱动与有界内存。
- 分层：新增独立 RTCP 解析模块，`session` 只编排（解析结果 -> 统计/回调/重传）。

#### 参考对齐
- `libpeer`：[`peer_connection.c`]( /home/qshl/dev/rtc/goodlib/libpeer/src/peer_connection.c )、[`rtcp.c`]( /home/qshl/dev/rtc/goodlib/libpeer/src/rtcp.c )  
  对齐点：轻量 RTCP 分发与 PLI/FIR 触发关键帧回调。
- `amazon-kinesis`：[`Rtcp.c`]( /home/qshl/dev/rtc/goodlib/amazon-kinesis-video-streams-webrtc-sdk-c/src/source/PeerConnection/Rtcp.c )  
  对齐点：compound RTCP 逐包解析、PLI 计数+回调、NACK 触发重发。
- `libdatachannel`：[`plihandler.cpp`]( /home/qshl/dev/rtc/goodlib/libdatachannel/src/plihandler.cpp )、[`rtcpnackresponder.cpp`]( /home/qshl/dev/rtc/goodlib/libdatachannel/src/rtcpnackresponder.cpp )  
  对齐点：PLI 处理链与有界 NACK 缓存重发模式。
- `metaRTC`：[`YangRtcpRR.c`]( /home/qshl/dev/rtc/goodlib/metaRTC/libmetartccore8/src/yangrtp/YangRtcpRR.c )、[`YangSRtp.c`]( /home/qshl/dev/rtc/goodlib/metaRTC/libmetartccore8/src/yangutil/sys/YangSRtp.c )  
  对齐点：RTCP 独立编解码与 RTP/RTCP 分路径安全处理。

#### Key Changes
- 公共接口（[`rtc.h`](/home/qshl/dev/rtc/s1/include/rtc/rtc.h)）  
  - 追加回调：`on_keyframe_request(peer, media_ssrc, user_data)`（`rtc_peer_config_t` 末尾）。  
  - 追加 stats：`rtcp_rr_rx`、`rtcp_pli_rx`、`rtcp_nack_rx`、`rtcp_nack_retx`（`rtc_peer_stats_t` 末尾）。
- RTCP 解析模块（新增 `src/rtp/rtc_rtcp.c/.h`，并接入 [`CMakeLists.txt`](/home/qshl/dev/rtc/s1/CMakeLists.txt)）  
  - 解析 `PT=201(RR)`、`PT=206/FMT=1(PLI)`、`PT=205/FMT=1(NACK)`。  
  - 按 compound RTCP 块做严格 `length/bounds/version` 校验。  
  - 输出固定大小事件结构（含 `media_ssrc` 与展开后的 NACK 序列列表，受上限配置约束）。
- Session 编排与重传（[`rtc_session.c`](/home/qshl/dev/rtc/s1/src/session/rtc_session.c)、[`rtc_session.h`](/home/qshl/dev/rtc/s1/src/session/rtc_session.h)）  
  - 在 `SRTCP unprotect` 成功后调用 RTCP 解析器。  
  - PLI：`rtcp_pli_rx++` 并触发 `on_keyframe_request`。  
  - NACK：按 `media_ssrc + seq` 查找发送缓存并重发；成功重发则 `rtcp_nack_retx++` 与 `retransmit_count++`。  
  - RR/PLI/NACK 分别计数；解析异常计入 `protocol_error_count`，不中断连接主循环。  
  - 新增有界 RTP 重传缓存（环形，容量 `RTC_CFG_RTX_CACHE`）：缓存“已 SRTP 保护且成功入 TX 队列”的包，避免重发时重新加密与状态歧义。
- 可观测性  
  - 新增 `rtcp` 模块日志上下文（pt/fmt/ssrc/seq/err）。  
  - 热路径告警按“首个 + 周期性”限频，保持可定位且不刷屏。  
  - 队列满、缓存未命中、畸形 RTCP 都有明确计数/日志。

#### Test Plan
1. 扩展 [`test_rtc_api.c`](/home/qshl/dev/rtc/s1/test/test_rtc_api.c) 远端 DTLS peer：握手后导出 `EXTRACTOR-dtls_srtp` keying material，建立远端 SRTCP 发送上下文。  
2. `RR/PLI/NACK` 正向端到端：发送合法加密反馈包，断言 `rtcp_rr_rx/rtcp_pli_rx/rtcp_nack_rx` 增长，PLI 回调触发且 `media_ssrc` 正确。  
3. `NACK 重传` 端到端：设备先发视频 RTP，远端按观测到的 `seq+ssrc` 发 NACK，断言出现同 `seq+ssrc` 重发，且 `rtcp_nack_retx/retransmit_count` 增长。  
4. `RTCP 解析异常` 端到端：发送“可解密但字段畸形”RTCP，断言连接保持 `CONNECTED`，`protocol_error_count` 增长且日志可定位。  
5. 保留现有非法 SRTP/SRTCP 观测测试，执行 `ctest --output-on-failure` 全量回归。

#### Assumptions
- NACK 重传采用“原包重发”（同 SSRC/SEQ，不引入 RTX SSRC/PT 机制），符合当前最小闭环目标。  
- 单次 NACK 展开与缓存查找均有硬上限；未知/不支持 RTCP 类型只记录并跳过。  
- 默认内存增量为每 peer 固定大小重传缓存（约 `RTC_CFG_RTX_CACHE * (RTC_CFG_MTU + RTC_CFG_SRTP_MAX_TRAILER + 元数据)`，默认约 40KB 量级），无无界增长。  
- 实时性影响为有界线性扫描（默认小常量），无新增线程、无递归、无隐藏分配。

