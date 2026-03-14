# Chrome 音视频互通（首版）实施计划

## 摘要
基于当前代码，首版目标定为：**Chrome Offer / 设备 Answer、局域网直连、双向 H264 + G711、手工/脚本信令、核心库保持 tick/poll 驱动**。  
当前实现是“内存内环回模型”（fake ICE + 内部 DTLS 对打 + loopback transport），与真实互通差异在于：**没有真实 UDP/ICE/STUN/DTLS 报文路径、没有可用 SDP 协商子集、没有与 Chrome 实际收发链路对齐**。  
计划按“最小可通 + 每步可验收”推进，先打通再增强。

## 参考实现对齐（已采样）
- `libpeer`：`src/agent.c`、`src/peer_connection.c`、`src/dtls_srtp.c`、`src/sdp.c`  
  对齐点：候选对选择、STUN 连通检查、DTLS 角色与指纹校验、SDP 生成/解析最小闭环。
- `amazon-kinesis-video-streams-webrtc-sdk-c`：`src/source/Ice/IceAgentStateMachine.c`、`.../IceAgent.c`、`.../Crypto/Dtls_mbedtls.c`、`.../Srtp/SrtpSession.c`、`.../PeerConnection/SessionDescription.c`  
  对齐点：显式状态机、超时/重试、DTLS 非阻塞计时驱动、SRTP key 派生与会话分离、SDP codec/payload 映射。
- `libdatachannel`：`src/impl/icetransport.cpp`、`.../peerconnection.cpp`、`.../dtlssrtptransport.cpp`  
  对齐点：RTP/DTLS 报文 demux、DTLS-SRTP 连接后置初始化、互通行为边界。
- `metaRTC`：`libmetartccore8/src/yangice/YangIceAgent.c`、`.../yangssl/YangDtlsMbedtls.c`、`.../yangsdp/YangRtcSdp.c`  
  对齐点：工程化分层与 C 实现里的 ICE/DTLS/SDP 路径组织方式。

## 公共接口/类型变更（最小且向后兼容）
1. `rtc_peer_config_t` 末尾追加（通过 `size` 字段兼容旧调用方）：`local_udp_port`（0=自动）、`ice_lite_enabled`（默认1）。
2. `rtc_peer_stats_t` 末尾追加观测字段：`ice_checks_sent`、`ice_checks_ok`、`dtls_rx_pkts`、`dtls_tx_pkts`、`srtp_unprotect_fail`、`rtcp_rx_pkts`。
3. 不新增线程模型 API；核心仍由 `rtc_engine_poll()` 驱动。

## 实施步骤（每步可验证）
1. **建立差异基线与回归护栏**：固化当前单测与日志基线，新增“互通空跑”测试骨架（不改行为）。  
   验证：`ctest --output-on-failure` 全绿；新增测试仅做框架断言且可重复运行。

2. **transport 从 loopback 改为真实 UDP 收发（保留有界队列）**：在 `transport` 层引入非阻塞 socket、固定容量 RX/TX ring、远端地址绑定与生命周期管理。  
   验证：本地 UDP 回环测试（同进程）可收发；队列满时返回 `RTC_ERR_OVERFLOW` 且计数增长；无堆分配增长。

3. **实现最小 SDP 解析/应答器（Answer-only）**：解析 Chrome Offer 的 `ice-ufrag/pwd`、`fingerprint`、`setup`、`m=audio/video`、`rtpmap/fmtp`、`candidate`；生成 Answer（`a=ice-lite`、`setup:passive`、`rtcp-mux`、BUNDLE）。  
   验证：Offer fixture 解析成功；生成 Answer 包含必需字段；不支持 codec 时返回 `RTC_ERR_NOT_SUPPORTED` 并给出上下文日志。

4. **ICE-Lite 连通路径落地**：实现 STUN Binding Request/Response 处理、候选对状态（WAITING/INPROGRESS/SUCCEEDED/FAILED）、超时与失败路径。  
   验证：注入 STUN 报文可触发状态推进；无远端候选超时进入 FAILED；`ice_checks_sent/ok` 与日志一致。

5. **session 层状态机接入真实 ICE 事件**：从 `STARTING -> ICE_CHECKING -> DTLS_HANDSHAKE` 的条件由真实连接事件驱动，失败原因显式上报。  
   验证：状态回调序列与预期一致；错误码可区分超时/协议错/资源耗尽。

6. **DTLS 改为真实网络握手（移除内部 client/server 对打）**：按 Answer 角色走 `passive/server`，通过 transport 收发 DTLS datagram，`poll` 内计时重传，握手完成后导出 keying material。  
   验证：与 Chrome 握手成功并进入 CONNECTED；故意改错 fingerprint 必须失败并返回 `RTC_ERR_DTLS_HANDSHAKE_FAILED`。

7. **SRTP 上下文按 role 正确激活**：修正入/出向 key 映射，分别建立 RTP/RTCP protect/unprotect 路径，保持会话所有权清晰。  
   验证：SRTP/RTCP 加解密单测通过；认证失败可观测（计数+日志）；连接中断后上下文可安全销毁重建。

8. **RTP 媒体最小互通能力补齐**：H264 增加 FU-A 分片/重组，G711 保持 PCMA/PCMU；按协商 payload type 与 SSRC 工作。  
   验证：大于 MTU 的 H264 可正确分片发送与重组接收；Chrome 端可见视频帧与音频播放；统计 `tx/rx_*_frames` 正常。

9. **RTCP 最小闭环**：支持接收并解析 RR/PLI/NACK（先统计与关键回调），不做复杂拥塞控制。  
   验证：收到 PLI 可触发关键帧请求回调（或统计）；RTCP 包解析异常可定位且不崩溃。

10. **可观测性与资源边界补齐**：统一模块日志上下文（模块/peer/state/code/关键参数），补错误计数与队列水位，确保所有缓存和列表有硬上限。  
    验证：压力发送下无无界增长；队列满、协议异常、超时都有明确日志和计数变化。

11. **手工/脚本信令联调工具**：提供最小 CLI + 文档（读取 Offer，输出 Answer；接收/输出 candidate），不把信令耦合进核心库。  
    验证：按 runbook 可在同网段完成一次完整呼叫；中途重启 peer 能稳定清理并重连。

12. **端到端验收与回归矩阵**：执行“正向互通 + 负向异常 + 资源边界”三组场景并固化。  
    验证：  
    - 正向：Chrome 与设备双向音视频连续 5 分钟稳定。  
    - 负向：错误 fingerprint、无候选、非法 RTP/RTCP 都可预期失败。  
    - 边界：队列满/重试上限触发时行为可预测、无崩溃、无泄漏。

## 测试场景清单
1. Chrome Offer（H264+PCMA）-> 设备 Answer，双向媒体成功。  
2. Offer 含不支持音频（仅 Opus）时，设备拒绝并给出明确错误。  
3. 仅给远端 SDP 不给 candidate，ICE 超时失败。  
4. 指纹不匹配，DTLS 失败并进入 FAILED。  
5. 高速发送触发 TX 队列满，丢包计数和日志正确。  
6. 注入畸形 RTP/RTCP，协议错误计数增长且不影响进程稳定。

## 假设与默认值
- 首版固定协商方向：**Chrome Offer / 设备 Answer**。  
- 首版网络范围：**局域网 host candidate 直连**（STUN/TURN 放到下一阶段）。  
- 首版编解码：**H264 + G711(PCMA/PCMU)**。  
- 核心库默认仍采用 **single-thread poll/tick**；如需线程，仅允许出现在示例/工具层，不进入核心状态机。  
- 所有新增结构保持上限可配置，禁止无界容器与隐式分配。

