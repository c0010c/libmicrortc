# API 执行器与内存契约

本文档描述已经固定的公共边界。后续 ICE、DTLS-SRTP、RTP/RTCP 实现必须遵守这些规则。

## 固定内存

- 用户通过 `rtc_peer_connection_config_t.arena` 提供总 arena。
- 用户通过 `rtc_peer_connection_config_t.limits` 提供按子系统分组的容量限制。
- 用户通过 `rtc_peer_connection_config_t.sdp` 提供 create-time SDP 参数，包括 ICE ufrag/pwd、DTLS fingerprint/setup 和 session id/version；库不会生成随机 ICE 参数或证书。
- 用户通过 `rtc_peer_connection_config_t.local_host_ip`、`local_host_ip_len` 和 `local_host_port` 提供 host candidate 来源。库不枚举网卡、不绑定端口，也不探测本机地址。
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

第 3 阶段新增 `rtc_peer_connection_gather_candidates` 和 `rtc_peer_connection_start_connectivity_checks`，二者必须在 `RTC_EXECUTOR_NETWORK` 上调用。`rtc_peer_connection_gather_candidates` 负责显式启动 host/srflx gathering；`rtc_peer_connection_start_connectivity_checks` 负责 Full ICE candidate pair checks、regular nomination 和 selected pair 推进。`createOffer`、`createAnswer`、`setLocalDescription`、`setRemoteDescription` 和 `addIceCandidate` 不会隐式启动 socket，也不会隐式启动 gathering/checks；只有已显式启动 checks 后的 trickle `addIceCandidate` 会在固定 pair table 内增量创建 candidate pair。

`rtc_peer_connection_gather_candidates` 的当前行为：

- host-only：当 `stun_server_count == 0` 时，使用 create-time 本地 IP:port 生成 `typ host` candidate，通过 `observer.on_local_candidate` trickle 输出，并进入 `ice.gathering_complete`。
- host + srflx：当 `stun_server_count == 1` 时，先输出 host candidate，再从固定 STUN transaction table 取槽，写入 STUN Binding request，并通过 `observer.on_datagram` 输出待发送 UDP payload。用户负责把该 datagram 发往 create-time STUN server；库不保存回调中的 datagram 指针。
- STUN success response：内部 ICE/STUN 处理只接受 pending transaction id 匹配的 Binding success response，解析 IPv4 `XOR-MAPPED-ADDRESS` 后输出 `typ srflx` candidate，并进入 `ice.gathering_complete`。
- STUN timeout：network executor 的 timer 回调会释放 transaction 槽并递增 timeout counter；如果 host candidate 已输出，则 gathering 仍以 host-only 结果完成。
- 重复调用：gathering 已开始或已完成后再次调用 `rtc_peer_connection_gather_candidates` 返回 `RTC_STATUS_INVALID_STATE`。

`rtc_peer_connection_start_connectivity_checks` 的当前行为：

- 启动前要求至少 1 个本地 candidate 和至少 1 个远端 candidate。缺少本地候选返回 `RTC_STATUS_INVALID_STATE`，状态进入 `ice.failed`，trace reason 为 `no_local_candidates`；缺少远端候选同理，reason 为 `no_remote_candidates`。
- ICE role 由 JSEP 路径推导：本地 offer 路径为 `controlling`，远端 offer 路径为 `controlled`；测试路径未设置 description 时默认按 `controlling` 处理。
- 启动时为每个 local/remote candidate 组合创建 candidate pair，受 `limits.ice.max_candidate_pairs` 约束。容量不足返回 `RTC_STATUS_CAPACITY_ICE_PAIRS`，状态进入 `ice.failed`，reason 为 `capacity_exhausted`。
- pair 创建通过 `RTC_TRACE_ICE_PAIR_CREATED` 输出，字段包含 pair id、本地/远端地址、candidate type、port 和 deterministic priority。
- checks 启动后进入 `ice.checking`，并通过 `observer.on_datagram` 输出最高优先级 pair 的 STUN Binding request。用户负责把 datagram 发往远端 candidate；库不创建 socket，不保存回调 buffer。
- controlling 角色采用 regular nomination：普通 Binding success 只把 pair 标记为 succeeded，随后发送带 `USE-CANDIDATE` 的 nominated Binding request；nominated success 后设置 selected pair，状态进入 `ice.connected`。
- controlled 角色收到带 `USE-CANDIDATE` 的 nominated Binding request 后标记 nominated，并设置 selected pair。
- selected pair 通过 `RTC_TRACE_ICE_SELECTED_PAIR` 输出，字段包含 `pair_id`、`role`、`local_address`、`remote_address`、`candidate_type` 和 `port`。
- pair check timeout 会释放 STUN transaction 槽、递增 timeout counter，并尝试下一个 pair；所有 pair exhausted 后进入 `ice.failed`，reason 为 `pair_check_exhausted`，`checks_failed` counter 递增。
- STUN role conflict error response 映射为 `role_conflict`，通过 observer error 和 `RTC_TRACE_ICE_STATE` reason 暴露。当前实现不能安全调整 role 时进入 `ice.failed`。
- 畸形 STUN datagram 映射为 `malformed_stun`。非 STUN/未知 datagram 在后续 demux 入口中应映射为 `unknown_datagram`。

`rtc_peer_connection_config_t.local_host_ip` 必须是 IP 字面量字符，`local_host_port` 必须大于 0。`rtc_peer_connection_config_t.stun_server_count` 只接受 `0` 或 `1`。为 `1` 时，`stun_server.ip` 必须是 IPv4/IPv6 字面量字符，`stun_server.port` 必须大于 0；hostname/DNS 和多个 STUN server 不属于首版边界。

`addIceCandidate` 仅接受 `candidate:` 或 `a=candidate:` 开头的远端 candidate 字符串，并复制到创建阶段分配的固定槽。第 3 阶段会解析 foundation、component、transport、priority、address、port 和 type，当前只支持 `host/srflx`，要求 component 为 `1`、transport 为 UDP、port 在 `1..65535`。原始字符串和结构化摘要都会复制到内部 arena，不保存调用方 buffer；调用返回后用户可以立即复用或释放输入 buffer。槽数量受 `limits.ice.max_candidates` 限制，超限返回 `RTC_STATUS_CAPACITY_ICE_CANDIDATES`。如果 checks 已经启动，`addIceCandidate` 会为新增远端 candidate 与现有本地 candidates 增量创建 pair；pair table 超限返回 `RTC_STATUS_CAPACITY_ICE_PAIRS`。该增量路径仍不创建 socket，不触发 `on_local_candidate`。

## 当前占位 API

第 3 阶段已经实现 host/srflx gathering 和 ICE connectivity checks，但尚未实现 DTLS-SRTP 或 RTP/RTCP。以下 API 当前返回 `RTC_STATUS_UNSUPPORTED`：

- `rtc_peer_connection_receive_datagram`
