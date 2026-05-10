# API 执行器与内存契约

本文档描述第 1 阶段已经固定的公共边界。后续 SDP、ICE、DTLS-SRTP、RTP/RTCP 实现必须遵守这些规则。

## 固定内存

- 用户通过 `rtc_peer_connection_config_t.arena` 提供总 arena。
- 用户通过 `rtc_peer_connection_config_t.limits` 提供按子系统分组的容量限制。
- 创建阶段从 arena 中分配 `PeerConnection` 和后续内部资源。
- 容量不足通过 `rtc_status_t` 与 `rtc_capacity_diagnostics_t` 返回，诊断中包含资源类别、required 和 used。
- 核心源码必须通过内部 arena/allocator 入口申请内存，不能绕过固定内存边界。

## 执行器亲和

`rtc_executors_t` 包含三类执行器：

| 执行器 | 归属 |
|--------|------|
| `RTC_EXECUTOR_SIGNALING` | `create`、`destroy`、offer/answer、description、candidate 和 counters 快照 |
| `RTC_EXECUTOR_MEDIA` | 后续媒体帧输入输出 |
| `RTC_EXECUTOR_NETWORK` | datagram 输入输出、ICE、DTLS、SRTP 和网络相关事件 |

执行器亲和违规时，公开 API 返回 `RTC_STATUS_AFFINITY_VIOLATION`。第 1 阶段的测试 executor 通过内部测试接口模拟当前执行器；生产集成应确保用户在正确执行器上下文调用 API。

## 可观测性

- `rtc_observer_vtable_t` 提供状态、错误、trace、本地候选、媒体帧和 datagram 回调。
- 错误事件包含稳定 `rtc_status_t`、子系统、操作和 detail code。
- trace 事件名和字段键从公共头 `rtc/trace.h` 导出，生命周期、容量失败、亲和违规和未支持 API 已有稳定常量。
- `rtc_peer_connection_get_counters` 在 signaling executor 上返回计数器快照。

## 当前占位 API

第 1 阶段只固定 API 形状，不实现 SDP、ICE、DTLS-SRTP 或 RTP/RTCP。以下 API 当前返回 `RTC_STATUS_UNSUPPORTED`：

- `rtc_peer_connection_create_offer`
- `rtc_peer_connection_create_answer`
- `rtc_peer_connection_set_local_description`
- `rtc_peer_connection_set_remote_description`
- `rtc_peer_connection_add_ice_candidate`
- `rtc_peer_connection_receive_datagram`
