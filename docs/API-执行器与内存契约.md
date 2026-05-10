# API 执行器与内存契约

本文档描述已经固定的公共边界。后续 ICE、DTLS-SRTP、RTP/RTCP 实现必须遵守这些规则。

## 固定内存

- 用户通过 `rtc_peer_connection_config_t.arena` 提供总 arena。
- 用户通过 `rtc_peer_connection_config_t.limits` 提供按子系统分组的容量限制。
- 用户通过 `rtc_peer_connection_config_t.sdp` 提供 create-time SDP 参数，包括 ICE ufrag/pwd、DTLS setup 和 session id/version；第 4 阶段起本地 DTLS fingerprint 来自 `security_backend` 的 `sha-256` 本地证书 fingerprint，不再信任 create-time 临时 fingerprint 字符串。
- 用户通过 `rtc_peer_connection_config_t.local_host_ip`、`local_host_ip_len` 和 `local_host_port` 提供 host candidate 来源。库不枚举网卡、不绑定端口，也不探测本机地址。
- 创建阶段从 arena 中分配 `PeerConnection`、local/remote SDP buffer、远端 candidate 原始字符串固定槽、candidate 摘要、candidate pair、STUN transaction 固定槽、DTLS session storage、SRTP/SRTCP context 和 key material buffer。
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

`createOffer` 和 `createAnswer` 输出固定 Chrome 1v1 SDP profile：BUNDLE、rtcp-mux、trickle ICE capability、ICE 参数、DTLS fingerprint/setup、Opus 111、H264 103 和 `sendrecv`/`recvonly` 方向。SDP writer 消费 `rtc_peer_connection_config_t.sdp` 中的 ICE/setup/session 参数，并从 `security_backend` 查询本地 `sha-256` DTLS fingerprint；它不枚举本机网络接口，不创建 socket，也不输出本地 candidate。

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
- 畸形 STUN datagram 映射为 `malformed_stun`。非 STUN/未知 datagram 在 demux 入口中映射为 `unknown_datagram`。

## 第 3 阶段 Datagram 输入输出

第 3 阶段已经实现 ICE/STUN 与 datagram demux 的公共边界，所有网络入口都保持用户负责 UDP/socket I/O 的模型。

- `rtc_peer_connection_gather_candidates` 在 `RTC_EXECUTOR_NETWORK` 上显式启动 gathering；它不会由 offer/answer 或 description API 隐式触发。
- `rtc_peer_connection_start_connectivity_checks` 在 `RTC_EXECUTOR_NETWORK` 上显式启动 pair checks；它不会创建 socket，也不会隐式发送 UDP。
- `rtc_peer_connection_receive_datagram` 在 `RTC_EXECUTOR_NETWORK` 上输入用户收到的 UDP payload。库只在调用期间读取用户 `data` buffer，不保存指针，也不在返回后访问该 buffer。
- `observer.on_datagram` 输出待发送 UDP payload，用户负责 socket 发送；回调返回后库不要求用户继续保留该回调中的 payload 指针。
- STUN 被真实处理：srflx gathering response、pair check response、regular nomination response 和 controlled 角色的 nominated Binding request 都会路由到 ICE/STUN handler。
- DTLS 在第 4 阶段进入 security 层；RTP/RTCP 仍留给第 5 阶段媒体平面。库不直接操作 socket，也不把 DTLS/SRTP 绑定到默认第三方库。
- unknown datagram 返回 `RTC_STATUS_PROTOCOL_ERROR`，observer error 的 subsystem 为 `net`，operation 为 `receive_datagram`，trace reason 为 `unknown_datagram`。
- 首字节像 STUN 但 magic cookie、header length 或 attribute framing 不合法的 datagram 返回 `RTC_STATUS_PROTOCOL_ERROR`，trace reason 为 `malformed_stun`，与 `unknown_datagram` 区分。

`rtc_peer_connection_config_t.local_host_ip` 必须是 IP 字面量字符，`local_host_port` 必须大于 0。`rtc_peer_connection_config_t.stun_server_count` 只接受 `0` 或 `1`。为 `1` 时，`stun_server.ip` 必须是 IPv4/IPv6 字面量字符，`stun_server.port` 必须大于 0；hostname/DNS 和多个 STUN server 不属于首版边界。

首版 STUN server 只支持 0/1 个 IP:port，不支持 hostname、DNS、TURN。`0` 表示只生成 host candidate；`1` 表示 host + srflx gathering。TURN relay candidate、DNS 解析和多个 STUN server fallback 属于后续扩展。

`addIceCandidate` 仅接受 `candidate:` 或 `a=candidate:` 开头的远端 candidate 字符串，并复制到创建阶段分配的固定槽。第 3 阶段会解析 foundation、component、transport、priority、address、port 和 type，当前只支持 `host/srflx`，要求 component 为 `1`、transport 为 UDP、port 在 `1..65535`。原始字符串和结构化摘要都会复制到内部 arena，不保存调用方 buffer；调用返回后用户可以立即复用或释放输入 buffer。槽数量受 `limits.ice.max_candidates` 限制，超限返回 `RTC_STATUS_CAPACITY_ICE_CANDIDATES`。如果 checks 已经启动，`addIceCandidate` 会为新增远端 candidate 与现有本地 candidates 增量创建 pair；pair table 超限返回 `RTC_STATUS_CAPACITY_ICE_PAIRS`。该增量路径仍不创建 socket，不触发 `on_local_candidate`。

## 第 4 阶段 DTLS-SRTP 安全传输

第 4 阶段使用单一 `security_backend` vtable 覆盖 DTLS、fingerprint、key export、SRTP/SRTCP protect/unprotect 和相关 crypto 能力。核心 API 不拆分多个 public DTLS/SRTP/crypto backend，也不要求用户直接编排 DTLS handshake 或 SRTP context。

安全 backend 的运行边界如下：

- `rtc_peer_connection_config_t.security_backend` 指向固定的 backend 配置和 `rtc_security_backend_vtable_t`；backend session storage 在 `PeerConnection` 创建阶段从用户 arena 中切分。
- 默认构建不要求 OpenSSL/libsrtp 开发包。真实 OpenSSL/libsrtp 或同类宽松许可适配只能作为可选编译开关启用，并且必须说明如何满足固定 storage 契约；deterministic backend 是第 4 阶段默认验证入口。
- backend 不拥有 socket，不直接发送 UDP datagram，不保存用户输入 datagram 指针，也不得在核心运行期偷偷依赖动态内存增长。
- backend 产生的 `RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM` 由核心转成既有 `observer.on_datagram` 回调输出；回调返回后 payload 指针失效，用户负责实际 UDP/socket 发送。

DTLS 启动和 datagram 输入语义：

- DTLS handshake 在 ICE selected pair / `ice.connected` 后由核心自动启动，用户不需要公共 `start_dtls` API。
- `rtc_peer_connection_receive_datagram` 在 `RTC_EXECUTOR_NETWORK` 上接收 DTLS payload。若 ICE 尚未 connected/selected，DTLS datagram 被拒绝且不缓存，返回 `RTC_STATUS_INVALID_STATE`，trace reason 为 `ice_not_connected`。
- ICE connected 后，DTLS datagram 进入 `security_backend` 输入函数；backend 可以通过 event callback 产生 outgoing datagram、handshake complete 或 backend error。
- DTLS close/error 只改变安全状态，不回滚 ICE selected pair；失败通过 `dtls.failed`、observer error、trace reason 和 counter 暴露。

Fingerprint 和 key export 语义：

- 本地 SDP fingerprint 必须来自 backend local certificate fingerprint，首版只接受 `sha-256`。
- backend 通过 `RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE` 通知核心握手完成后，核心先查询 peer certificate fingerprint 并与远端 SDP fingerprint 比对。
- `fingerprint_mismatch` 是安全硬失败：核心进入 `dtls.failed`，返回或记录 `RTC_STATUS_PROTOCOL_ERROR`，不得执行 key export，也不得进入 `srtp.ready`。
- fingerprint 验证通过后，核心调用 backend 使用 label `EXTRACTOR-dtls_srtp` 导出固定长度 keying material，并初始化单一 BUNDLE SRTP/SRTCP context。
- key export 或 SRTP/SRTCP 初始化失败会进入安全失败状态；不得让后续媒体路径发送未保护 RTP/RTCP。

SRTP/SRTCP wrapper 语义：

- 第 4 阶段只提供内部 `src/srtp` wrapper，供第 5 阶段 RTP/RTCP 媒体平面调用；不新增 public `PeerConnection` protect/unprotect API。
- SRTP/SRTCP protect 成功后才允许输出受保护 datagram；protect 失败不得调用 `observer.on_datagram` 输出明文 RTP/RTCP。
- SRTP/SRTCP unprotect 成功后才允许把包交给媒体层；unprotect、认证或 replay 失败不得输出未认证媒体数据。
- 安全失败 public status 保持粗粒度：backend 失败主要映射为 `RTC_STATUS_BACKEND_ERROR`，fingerprint/replay 等协议安全失败映射为 `RTC_STATUS_PROTOCOL_ERROR`，状态顺序错误映射为 `RTC_STATUS_INVALID_STATE`。细节通过 `RTC_SECURITY_DETAIL_*`、trace reason（例如 `handshake_failed`、`fingerprint_mismatch`、`key_export_failed`、`srtp_protect_failed`、`srtp_unprotect_failed`、`srtp_replay_failed`）和 counters 表达。

## 第 5 阶段：RTP/RTCP 媒体平面契约

第 5 阶段交付 typed media API、Opus/H264 RTP payload format、RTCP SR/RR/SDES、PLI/NACK feedback 和媒体可观测性。该阶段只覆盖本地 deterministic tests 验证的媒体平面行为；Chrome 页面、信令示例和真实 1v1 音视频端到端验收仍属于第 6 阶段。

### Public API 与 executor 亲和

- `rtc_peer_connection_send_media_frame` 必须在 `RTC_EXECUTOR_MEDIA` 上调用。该入口接收 `rtc_media_frame_t`，v1 只接受 `RTC_MEDIA_KIND_AUDIO_OPUS` 和 `RTC_MEDIA_KIND_VIDEO_H264`。
- `rtc_peer_connection_request_keyframe` 必须在 `RTC_EXECUTOR_MEDIA` 上调用。v1 只支持对 `RTC_MEDIA_KIND_VIDEO_H264` 显式触发 PLI；对 Opus 请求 keyframe 返回不支持。
- `rtc_peer_connection_receive_datagram` 必须在 `RTC_EXECUTOR_NETWORK` 上调用。RTP/RTCP datagram 在 network executor 上完成 demux、SRTP/SRTCP unprotect 和安全门检查后，才会投递到 media executor 做 depacketize、reassembly 或 feedback 上报。
- `observer.on_datagram` 在 `RTC_EXECUTOR_NETWORK` 上回调，并且只输出已经通过 SRTP/SRTCP protect 的 RTP/RTCP datagram。库不会通过该回调输出明文 RTP/RTCP。
- `observer.on_media_frame_typed` 在 `RTC_EXECUTOR_MEDIA` 上回调，用于输出 typed Opus frame 或 H264 access unit。旧 `observer.on_media_frame(data,len)` 仅作为兼容回调，不是第 5 阶段主要媒体出口。
- `observer.on_media_feedback` 在 `RTC_EXECUTOR_MEDIA` 上回调，用于输出远端 PLI/NACK feedback。用户负责据此驱动编码器，例如收到 PLI 后产生 H264 关键帧。

### Buffer 生命周期

- `rtc_peer_connection_send_media_frame` 只在调用期间读取 `frame` 和 `frame->data`；跨 executor 发送前会复制到创建阶段分配的固定 media/RTP slot，不保存调用方 buffer 指针。
- `rtc_peer_connection_receive_datagram` 只在调用期间读取用户传入的 datagram buffer；RTP/RTCP 数据进入内部固定 slot 后再处理，不保存调用方 buffer 指针。
- `observer.on_datagram` 回调中的 datagram buffer 只在回调期间有效；用户需要在回调内完成 socket 发送或自行复制。
- `observer.on_media_frame_typed` 和 `observer.on_media_feedback` 回调中的结构体及其内部 buffer 只在回调期间有效；用户如需异步解码、编码器控制或统计，需要自行复制。

### RTP 媒体语义

- Opus 发送方向按一个 Opus frame 到一个 RTP packet 的首版路径实现，payload type 固定匹配 Chrome profile 的 Opus PT 111。
- Opus 接收方向在 SRTP unprotect 成功后把 RTP payload 作为 `RTC_MEDIA_KIND_AUDIO_OPUS` typed frame 输出。
- H264 发送方向要求用户提交 H264 access unit。当前发送支持 Annex B single NALU 和 FU-A；超过固定 RTP payload 上限的 NALU 使用 FU-A 分片。
- H264 接收方向支持 single NALU、FU-A 和有限 STAP-A。接收侧会把重组结果作为 `RTC_MEDIA_KIND_VIDEO_H264` typed frame 输出；H264 access unit 输出为重组后的 NALU bytes。
- H264 FU-A sequence gap、missing start、STAP-A length 越界或 reassembly capacity 不足时，当前 access unit 被丢弃，并通过 counter/trace 暴露；不会输出损坏媒体帧。
- 单个 `PeerConnection` v1 最多支持 1 路 Opus audio 和 1 路 H264 video。非法 media kind、空 frame、payload 超限或固定 slot 不足返回稳定错误，并通过 observer error、trace 和 counters 诊断。

### RTCP 与 feedback 语义

- RTCP SR/RR/SDES 由库维护 internal codec 和基础 stats；用户不需要也不能通过 public API 手动组 SR/RR/SDES。
- RTCP receive 必须先通过 SRTCP unprotect；只有 unprotect 成功后才解析 SR/RR/SDES/PLI/NACK 并更新 counters 或输出 feedback。
- RTCP send 必须先写入固定 buffer，再通过 SRTCP protect；只有 protect 成功后才通过 `observer.on_datagram` 输出受保护 RTCP datagram。
- PLI 发送由 `rtc_peer_connection_request_keyframe(pc, RTC_MEDIA_KIND_VIDEO_H264)` 显式触发；库负责生成 RTCP PSFB PLI 并走 SRTCP protect/output gate。
- 收到远端 PLI 后，库通过 `observer.on_media_feedback` 上报 `RTC_MEDIA_FEEDBACK_PLI`。库不控制编码器；用户负责让 H264 编码器产生关键帧。
- 收到远端 NACK 后，库解析 Generic NACK PID/BLP，并把丢包序号展开到 `rtc_media_feedback_t.lost_sequence_numbers` 的固定 17 项容量内。
- NACK 不触发重传。NACK 上报时 `retransmit_performed = 0`，并通过 `nack_no_retransmit` counter/trace reason 明确表达 “NACK 不触发重传”。

### 安全失败语义

- SRTP protect 失败不输出明文：RTP 发送路径只有 `rtc_srtp_protect_rtp` 成功后才调用 `observer.on_datagram`。
- SRTCP protect 失败不输出明文：RTCP SR/RR/SDES/PLI 发送路径只有 `rtc_srtp_protect_rtcp` 成功后才调用 `observer.on_datagram`。
- SRTP unprotect、认证或 replay 失败不输出媒体帧：RTP 接收路径失败时不会调用 `observer.on_media_frame_typed`。
- SRTCP unprotect、认证或 replay 失败不输出 feedback：RTCP 接收路径失败时不会调用 `observer.on_media_feedback`，也不会把未认证 RTCP 当作有效 SR/RR/SDES/PLI/NACK。
