# API 执行器与内存契约

本文档描述已经固定的公共边界。后续 ICE、DTLS-SRTP、RTP/RTCP 实现必须遵守这些规则。

## 固定内存

- 用户通过 `rtc_peer_connection_config_t.arena` 提供总 arena。
- 用户通过 `rtc_peer_connection_config_t.limits` 提供按子系统分组的容量限制。
- 用户通过 `rtc_peer_connection_config_t.sdp` 提供 create-time SDP 参数，包括 ICE ufrag/pwd、DTLS fingerprint/setup 和 session id/version；库不会生成随机 ICE 参数或证书。
- 创建阶段从 arena 中分配 `PeerConnection`、local/remote SDP buffer、远端 candidate 原始字符串固定槽、candidate 摘要、candidate pair 和 STUN transaction 固定槽。
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
- trace 事件名和字段键从公共头 `rtc/trace.h` 导出，生命周期、容量失败、亲和违规、SDP parse/write、JSEP transition/reject 和 candidate 保存已有稳定常量。
- `rtc_peer_connection_get_counters` 在 signaling executor 上返回计数器快照。

## Offer/Answer 与 SDP/JSEP

第 2 阶段已经实现以下 signaling API：

- `rtc_peer_connection_create_offer`
- `rtc_peer_connection_create_answer`
- `rtc_peer_connection_set_local_description`
- `rtc_peer_connection_set_remote_description`
- `rtc_peer_connection_add_ice_candidate`

`createOffer` 和 `createAnswer` 输出固定 Chrome 1v1 SDP profile：BUNDLE、rtcp-mux、trickle ICE capability、ICE 参数、DTLS fingerprint/setup、Opus 111、H264 103 和 `sendrecv`/`recvonly` 方向。SDP writer 只消费 `rtc_peer_connection_config_t.sdp` 中的参数，不枚举本机网络接口，不创建 socket，也不输出本地 candidate。

`setLocalDescription` 和 `setRemoteDescription` 会解析 SDP、校验 Chrome 1v1 profile，并推进最小 JSEP 状态机：

| 流程 | 合法状态 |
|------|----------|
| 发起 | `stable -> setLocalDescription(offer) -> have-local-offer -> setRemoteDescription(answer) -> stable` |
| 接听 | `stable -> setRemoteDescription(offer) -> have-remote-offer -> setLocalDescription(answer) -> stable` |

非法状态转换返回 `RTC_STATUS_INVALID_STATE`，结构缺失返回 `RTC_STATUS_PROTOCOL_ERROR`，不支持的方向或 codec 返回 `RTC_STATUS_UNSUPPORTED`。失败路径不会保存 description，也不会推进 JSEP 状态。

第 3 阶段新增 `rtc_peer_connection_gather_candidates` 和 `rtc_peer_connection_start_connectivity_checks`，二者必须在 `RTC_EXECUTOR_NETWORK` 上调用。前者后续负责 host/srflx gathering，后者后续负责 candidate pair checks；`createOffer`、`createAnswer`、`setLocalDescription`、`setRemoteDescription` 和 `addIceCandidate` 不会隐式启动 checks、STUN transaction、socket 或 datagram 输出。

`rtc_peer_connection_config_t.stun_server_count` 只接受 `0` 或 `1`。为 `1` 时，`stun_server.ip` 必须是 IPv4/IPv6 字面量字符，`stun_server.port` 必须大于 0；hostname/DNS 和多个 STUN server 不属于首版边界。

`addIceCandidate` 仅接受 `candidate:` 或 `a=candidate:` 开头的远端 candidate 字符串，并复制到创建阶段分配的固定槽。第 3 阶段会解析 foundation、component、transport、priority、address、port 和 type，当前只支持 `host/srflx`，要求 component 为 `1`、transport 为 UDP、port 在 `1..65535`。原始字符串和结构化摘要都会复制到内部 arena，不保存调用方 buffer；调用返回后用户可以立即复用或释放输入 buffer。槽数量受 `limits.ice.max_candidates` 限制，超限返回 `RTC_STATUS_CAPACITY_ICE_CANDIDATES`。本计划中 `addIceCandidate` 不创建 candidate pair、不执行 STUN、不触发 `on_local_candidate`，也不会产生任何 socket 或 datagram 副作用。

## 当前占位 API

第 2 阶段仍不实现 ICE、DTLS-SRTP 或 RTP/RTCP。以下 API 当前返回 `RTC_STATUS_UNSUPPORTED`：

- `rtc_peer_connection_receive_datagram`
- `rtc_peer_connection_gather_candidates`
- `rtc_peer_connection_start_connectivity_checks`
