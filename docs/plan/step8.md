### Step8: RTP H264 FU-A + Negotiated PT/SSRC (Kinesis-aligned)

**Summary**
- 采用 `amazon-kinesis` 的 H264 打包/解包思路，并引入 `libdatachannel` 的分片连续性丢弃原则。  
- 本步范围锁定为 `single NAL + FU-A`，`STAP-A` 暂不纳入。  
- 对已接受媒体启用 SDP 严格 SSRC：缺失 `a=ssrc` 直接按协议错误失败。  
- 重组缓存采用固定上限 `32KB`，不引入堆分配。  

**Implementation Changes**
- 在 [rtc_rtp.c](/home/qshl/dev/rtc/s1/src/rtp/rtc_rtp.c) 增加协商 PT 映射并用于收发，去掉运行时固定 `96/8/0` 假设。  
- 在 [rtc_rtp.c](/home/qshl/dev/rtc/s1/src/rtp/rtc_rtp.c) 增加有界 H264 发送打包器：检测到起始码时按 Annex-B 扫描 NAL，未检测到起始码时回退为单 NAL 兼容路径，大 NAL 按 FU-A 分片并正确设置 `S/E/marker`。  
- 在 [rtc_rtp.c](/home/qshl/dev/rtc/s1/src/rtp/rtc_rtp.c) 增加有界 H264 接收重组器：支持 single NAL + FU-A，校验 seq/timestamp/ssrc 连续性，链路断裂时丢弃当前重组链。  
- 在 [rtc_ice.c](/home/qshl/dev/rtc/s1/src/ice/rtc_ice.c) 解析音视频 m-line 的 `a=ssrc:<uint32>` 并保存到 ICE 上下文；已接受媒体若无 SSRC 则返回 `RTC_ERR_PROTOCOL`。  
- 在 [rtc_session.c](/home/qshl/dev/rtc/s1/src/session/rtc_session.c) 把协商 PT 和远端 SSRC 下发到 RTP 上下文，并在发送前先计算分片数与 TX 队列容量，容量不足时在入队前直接返回 `RTC_ERR_OVERFLOW`。  
- 在 [rtc_session.c](/home/qshl/dev/rtc/s1/src/session/rtc_session.c) 保持统计语义为“按完整媒体帧计数”，即 `tx/rx_*_frames` 不按 RTP 包计。  
- 在 [rtc_session.c](/home/qshl/dev/rtc/s1/src/session/rtc_session.c) 增强可观测性日志：记录 PT/SSRC 映射生效、FU-A 链中断、重组溢出、分片容量不足。  

**Public APIs / Interfaces / Types**
- 对外 `rtc.h` API 函数签名保持不变。  
- 在 [rtc_config.h](/home/qshl/dev/rtc/s1/include/rtc/rtc_config.h) 新增 `RTC_CFG_H264_REASSEMBLY_MAX`，默认 `32768`。  
- 扩展内部类型：`rtc_ice_ctx_t` 增加远端音视频 SSRC 字段；`rtc_rtp_ctx_t` 增加协商 PT、期望 SSRC、FU-A 重组状态。  
- 更新 [rtc_rtp.h](/home/qshl/dev/rtc/s1/src/rtp/rtc_rtp.h) 内部接口以支持 PT/SSRC 配置与帧级 H264 收发流程。  

**Test Plan**
- 新增 [test_rtc_rtp.c](/home/qshl/dev/rtc/s1/test/test_rtc_rtp.c) 单测：Annex-B 分片计数、FU-A 头字段正确性、>MTU 重组成功、分片丢失/乱序丢链、>32KB 重组溢出、动态 PT 映射、SSRC 严格匹配。  
- 更新 [test_sdp_fixtures.h](/home/qshl/dev/rtc/s1/test/test_sdp_fixtures.h)：为可接受媒体 offer 补充 `a=ssrc` 行。  
- 增补 [test_rtc_api.c](/home/qshl/dev/rtc/s1/test/test_rtc_api.c) 场景：缺失 `a=ssrc` 的可接受媒体应失败；动态 PT 协商后媒体路径统计保持正确。  
- 执行回归：`ctest --output-on-failure`。  

**Assumptions**
- 发送输入默认优先 Annex-B，同时保留“单 NAL 直接发送”兼容以避免破坏现有调用。  
- `STAP-A` 延后到后续步骤，不在本步扩范围。  
- 内存增量为每 peer 固定约 32KB + 少量状态字段，保持有界。  
- 实时性影响为线性扫描与常量状态更新，无递归、无隐藏分配。  
- 参考对齐来源：`amazon-kinesis` H264 payloader/depayloader、`libdatachannel` 分片连续性处理、`libpeer/metaRTC` 的 SSRC 识别思路。  

