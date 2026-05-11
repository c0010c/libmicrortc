# 需求：WebRTC 纯 C 库

**定义日期：** 2026-05-10
**核心价值：** 在固定内存、无线程、跨平台约束下，稳定完成与 Chrome 的 1v1 音视频 `PeerConnection` 互通。

## v1 需求

### 公共 API 与生命周期

- [x] **API-01**: 用户可以通过纯 C 不透明句柄创建和销毁 `PeerConnection`。
- [x] **API-02**: 用户可以通过 `rtc_peer_connection_config_t` 提供 arena、limits、platform vtable、executor vtable、observer vtable 和安全 backend。
- [x] **API-03**: 所有公开 API 使用 `rtc_status_t` 返回码表达成功、可恢复错误、配置错误、容量不足和协议错误。
- [x] **API-04**: 用户可以调用 `createOffer`、`createAnswer`、`setLocalDescription`、`setRemoteDescription` 和 `addIceCandidate` 完成发起和接听流程。
- [x] **API-05**: API 文档明确每个函数、observer 回调和 datagram 回调的执行器亲和规则。

### 固定内存与执行器

- [x] **MEM-01**: 用户传入总 arena 和 limits 后，库在创建阶段完成对象、队列、候选、包缓存和 SDP 缓冲区切分。
- [x] **MEM-02**: 创建成功后，核心库运行期不依赖动态内存增长。
- [x] **MEM-03**: 当 limits 不足时，库返回可诊断的容量错误，指出不足的资源类别。
- [x] **EXE-01**: 用户可以提供 `signaling`、`media`、`network` 三类 `post + timer` 执行器。
- [x] **EXE-02**: 库内部不创建线程，不拥有平台事件循环。
- [x] **EXE-03**: 用户可以把三个执行器映射到三个线程、同一线程或 superloop。

### SDP/JSEP

- [x] **SDP-01**: 库可以生成 Chrome 可接受的 1v1 音视频 offer SDP。
- [x] **SDP-02**: 库可以生成 Chrome 可接受的 1v1 音视频 answer SDP。
- [x] **SDP-03**: 库可以解析 Chrome 1v1 音视频 offer/answer 中的 BUNDLE、rtcp-mux、DTLS fingerprint/setup、ICE 参数、H264、Opus 和媒体方向。
- [x] **SDP-04**: 库支持 `sendrecv` 和 `recvonly` 媒体方向。
- [ ] **SDP-05**: 库支持 trickle ICE 所需的本地候选输出和远端候选输入。

### ICE 与网络 datagram

- [ ] **ICE-01**: 库实现 Full ICE connectivity checks，支持 controlling 和 controlled 角色。
- [ ] **ICE-02**: 库支持 host candidate。
- [ ] **ICE-03**: 库通过 STUN 支持 server reflexive candidate。
- [ ] **ICE-04**: 库支持 trickle ICE candidate 的增量处理。
- [ ] **ICE-05**: 库通过 observer 输出 ICE gathering、checking、connected、failed 等状态事件。
- [ ] **NET-01**: 用户可以把收到的 UDP datagram 输入给库，库完成 STUN、DTLS、RTP、RTCP 分类。
- [ ] **NET-02**: 库通过回调输出待发送 UDP datagram，由用户负责 socket 发送。
- [ ] **NET-03**: datagram 输入输出 API 明确 buffer 所有权和生命周期。

### DTLS/SRTP 与安全后端

- [x] **SEC-01**: 库通过 backend vtable 驱动 DTLS 握手，不在核心 API 中绑定特定 TLS 库。
- [x] **SEC-02**: 库校验远端 SDP fingerprint 与 DTLS 证书一致。
- [x] **SEC-03**: 库从 DTLS 握手导出 SRTP keying material。
- [x] **SEC-04**: 库通过 backend vtable 对 RTP/RTCP 执行 SRTP/SRTCP protect 和 unprotect。
- [x] **SEC-05**: 安全后端错误被映射为稳定的 `rtc_status_t` 和 observer 错误事件。

### RTP/RTCP 与媒体帧

- [x] **RTP-01**: 用户可以提交 Opus frame，库负责 RTP packetize、时间戳、序列号和 SRTP 保护。
- [x] **RTP-02**: 用户可以接收从 RTP depacketize 得到的 Opus frame。
- [x] **RTP-03**: 用户可以提交 H264 access unit，库支持 single NALU 和 FU-A 发送。
- [x] **RTP-04**: 用户可以接收从 RTP depacketize 得到的 H264 access unit，接收方向有限支持 STAP-A。
- [x] **RTP-05**: 单个 `PeerConnection` 最多支持 1 路音频和 1 路视频。
- [x] **RTCP-01**: 库支持 RTCP Sender Report 和 Receiver Report。
- [x] **RTCP-02**: 库支持 RTCP SDES。
- [x] **RTCP-03**: 库支持发送和接收 PLI。
- [x] **RTCP-04**: 库解析 NACK 并通过 observer 上报，但不执行重传。

### 可观测性

- [x] **OBS-01**: 库提供统一 observer vtable 输出状态事件、错误事件、本地候选、远端媒体帧和待发送 datagram。
- [x] **OBS-02**: 库提供关键计数器，覆盖 ICE、DTLS、SRTP、RTP、RTCP、媒体帧和容量错误。
- [x] **OBS-03**: 库提供 trace hook，能关联 PeerConnection、子系统、状态转换、错误码和关键字段。
- [x] **OBS-04**: NACK、PLI、ICE 失败、DTLS 失败、SRTP 解保护失败等未自动修复事件都有可观测输出。

### 构建、示例与验收

- [x] **BLD-01**: 项目提供 CMake 静态库构建目标。
- [x] **BLD-02**: 项目提供公共头文件安装或导出规则。
- [ ] **TST-01**: 项目包含 SDP/JSEP、ICE/STUN、RTP/RTCP、固定内存和错误路径的自动化测试。
- [x] **EXM-01**: 项目提供本地 Chrome 页面用于发起或接听 1v1 音视频通话。
- [x] **EXM-02**: 项目提供最小信令示例，用于交换 SDP 和 trickle ICE candidate。
- [ ] **ACC-01**: 用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收。

## v2 需求

### 网络与媒体增强

- **V2-NET-01**: 支持 TURN relay candidate。
- **V2-RTP-01**: 实现 NACK 触发的 RTP 重传。
- **V2-RTP-02**: 实现基础拥塞控制或发送节奏控制。
- **V2-MED-01**: 支持多路音频或多路视频。
- **V2-SDP-01**: 扩展 SDP 兼容性，覆盖更多浏览器和非浏览器端实现。

### 功能扩展

- **V2-DATA-01**: 支持 DataChannel/SCTP。
- **V2-SEC-01**: 提供更多安全 backend 官方适配。
- **V2-OBS-01**: 提供结构化 trace 导出工具或 Wireshark 辅助脚本。

## 不在范围内

| 功能 | 原因 |
|---------|--------|
| 音视频编解码 | 用户负责 H264/Opus 编解码，库只处理编码帧与 RTP/RTCP |
| 内部线程 | 执行器由用户提供，库不拥有线程和事件循环 |
| socket 创建和网络 I/O | 用户负责 UDP/socket 收发，库只处理 datagram |
| 运行期动态内存增长 | 固定 arena 是核心约束 |
| TURN | 首版聚焦 host/srflx + STUN |
| DataChannel/SCTP | 与首版 Chrome 1v1 音视频目标无关 |
| 多路音视频 | 首版限制为 1 路音频 + 1 路视频 |
| 完整拥塞控制 | 首版只解析/统计相关扩展，不做闭环控制 |
| NACK 重传 | 首版解析并上报，不重传 |
| 泛 SDP 兼容 | 首版只追求 Chrome 1v1 最小画像 |
| GPL/LGPL 依赖 | 依赖策略优先宽松许可 |

## 需求追踪

| 需求 | 阶段 | 状态 |
|-------------|-------|--------|
| API-01 | 第 1 阶段 | 已完成 |
| API-02 | 第 1 阶段 | 已完成 |
| API-03 | 第 1 阶段 | 已完成 |
| API-05 | 第 1 阶段 | 已完成 |
| MEM-01 | 第 1 阶段 | 已完成 |
| MEM-02 | 第 1 阶段 | 已完成 |
| MEM-03 | 第 1 阶段 | 已完成 |
| EXE-01 | 第 1 阶段 | 已完成 |
| EXE-02 | 第 1 阶段 | 已完成 |
| EXE-03 | 第 1 阶段 | 已完成 |
| OBS-01 | 第 1 阶段 | 已完成 |
| OBS-02 | 第 1 阶段 | 已完成 |
| OBS-03 | 第 1 阶段 | 已完成 |
| BLD-01 | 第 1 阶段 | 已完成 |
| BLD-02 | 第 1 阶段 | 已完成 |
| SDP-01 | 第 2 阶段 | 已完成 |
| SDP-02 | 第 2 阶段 | 已完成 |
| SDP-03 | 第 2 阶段 | 已完成 |
| SDP-04 | 第 2 阶段 | 已完成 |
| API-04 | 第 2 阶段 | 已完成 |
| SDP-05 | 第 3 阶段 | 已规划 |
| ICE-01 | 第 3 阶段 | 已规划 |
| ICE-02 | 第 3 阶段 | 已规划 |
| ICE-03 | 第 3 阶段 | 已规划 |
| ICE-04 | 第 3 阶段 | 已规划 |
| ICE-05 | 第 3 阶段 | 已规划 |
| NET-01 | 第 3 阶段 | 已规划 |
| NET-02 | 第 3 阶段 | 已规划 |
| NET-03 | 第 3 阶段 | 已规划 |
| SEC-01 | 第 4 阶段 | 已完成：04-01 建立 backend 公共契约，04-02 完成固定 storage runtime、ICE connected 自动启动 DTLS 和 DTLS datagram/event 接入，04-06 文档收口确认单一 `security_backend` 边界 |
| SEC-02 | 第 4 阶段 | 已完成：04-03 将本地 SDP fingerprint 绑定到 backend sha-256 fingerprint，并在 DTLS peer fingerprint mismatch 时进入 dtls.failed、阻断 key export |
| SEC-03 | 第 4 阶段 | 已完成：04-04 在 handshake complete + fingerprint verification 后导出 `EXTRACTOR-dtls_srtp` keying material，并初始化单一 BUNDLE SRTP/SRTCP context |
| SEC-04 | 第 4 阶段 | 已完成：04-04 新增内部 SRTP/SRTCP protect/unprotect wrapper，失败路径阻断明文或未认证数据输出 |
| SEC-05 | 第 4 阶段 | 已完成：04-05 新增 `RTC_SECURITY_DETAIL_*` detail code，并用 deterministic backend 矩阵锁定 status、observer detail、trace reason 和 counter，04-06 文档收口确认默认构建不要求 OpenSSL/libsrtp |
| RTP-01 | 第 5 阶段 | 已完成：05-02 实现 Opus frame 到 RTP packetize、timestamp/sequence 更新、SRTP protect 和受保护 datagram 输出；05-06 在 API 契约与 UAT 中收口发送语义 |
| RTP-02 | 第 5 阶段 | 已完成：05-03 实现 RTP datagram SRTP unprotect 后 Opus depacketize，并通过 `on_media_frame_typed` 输出 `RTC_MEDIA_KIND_AUDIO_OPUS` frame；05-06 在 API 契约与 UAT 中收口接收语义 |
| RTP-03 | 第 5 阶段 | 已完成：05-02 实现 H264 Annex B access unit single NALU 与 FU-A 发送，覆盖 marker、容量和 protect 失败不输出明文；05-06 文档化 H264 access unit 发送边界 |
| RTP-04 | 第 5 阶段 | 已完成：05-03 实现 H264 single NALU、FU-A 和有限 STAP-A 接收重组，sequence gap / capacity 失败不输出媒体帧；05-06 文档化 H264 access unit 接收边界 |
| RTP-05 | 第 5 阶段 | 已完成：05-01 建立 typed media kind，v1 只公开 1 路 Opus audio 和 1 路 H264 video 的 public contract 与固定容量基线；05-06 在设计边界中确认 v1 不做多路媒体 |
| RTCP-01 | 第 5 阶段 | 已完成：05-04 实现 RTCP Sender Report / Receiver Report 固定 buffer codec、compound parser、基础 stats 和 SRTCP receive/send 集成；05-06 文档化 SR/RR 行为 |
| RTCP-02 | 第 5 阶段 | 已完成：05-04 实现 RTCP SDES CNAME 写入/解析，CNAME 长度受 `limits.rtcp.max_sdes_cname_bytes` 限制；05-06 文档化 SDES 行为 |
| RTCP-03 | 第 5 阶段 | 已完成：05-05 实现 `rtc_peer_connection_request_keyframe(video)` 生成受保护 PLI datagram，并在远端 PLI 后通过 `on_media_feedback` 上报；05-06 文档化显式 PLI 与用户编码器责任 |
| RTCP-04 | 第 5 阶段 | 已完成：05-05 实现 Generic NACK PID/BLP 固定数组展开和 observer 上报，`retransmit_performed = 0` 且不执行重传；05-06 文档和 UAT 明确 NACK 不触发重传 |
| OBS-04 | 第 5 阶段 | 已完成：05-05 为 PLI/NACK 输出 typed feedback、counter 和 trace；NACK reason 固定为 `nack_no_retransmit`；05-06 在 API 契约、UAT 和项目状态中收口可观测语义 |
| TST-01 | 第 6 阶段 | 已规划：06-05 端到端自动化编排与失败分层，06-06 文档/UAT 收口 |
| EXM-01 | 第 6 阶段 | 已完成：06-01 新增本地 Chrome 合成媒体页面、双视频预览、七阶段状态条、compact diagnostics 和 Playwright page smoke |
| EXM-02 | 第 6 阶段 | 已完成：06-01 新增本机 WebSocket 信令服务，支持 SDP offer/answer 与 trickle ICE candidate JSON 转发并拒绝媒体 payload |
| ACC-01 | 第 6 阶段 | 执行中：06-02 已完成 C 示例运行时、WebSocket/UDP 示例层 I/O 和 JSONL observer；06-03 已完成样本解析、按节奏发送、接收媒体落盘和 media-file smoke；06-04 可选真实安全 backend gate、06-05 full E2E、06-06 VLC 人工验收仍待完成 |

**覆盖情况：**
- v1 需求：48 项
- 已映射到阶段：48 项
- 未映射：0 项

---
*需求定义：2026-05-10*
*最后更新：2026-05-11，05-06 媒体 API 文档、UAT 和需求追踪收口后*
