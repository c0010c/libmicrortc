# Requirements: WebRTC 纯 C 库

**Defined:** 2026-05-10
**Core Value:** 在固定内存、无线程、跨平台约束下，稳定完成与 Chrome 的 1v1 音视频 `PeerConnection` 互通。

## v1 Requirements

### 公共 API 与生命周期

- [ ] **API-01**: 用户可以通过纯 C 不透明句柄创建和销毁 `PeerConnection`。
- [ ] **API-02**: 用户可以通过 `rtc_peer_connection_config_t` 提供 arena、limits、platform vtable、executor vtable、observer vtable 和安全 backend。
- [ ] **API-03**: 所有公开 API 使用 `rtc_status_t` 返回码表达成功、可恢复错误、配置错误、容量不足和协议错误。
- [ ] **API-04**: 用户可以调用 `createOffer`、`createAnswer`、`setLocalDescription`、`setRemoteDescription` 和 `addIceCandidate` 完成发起和接听流程。
- [ ] **API-05**: API 文档明确每个函数、observer 回调和 datagram 回调的执行器亲和规则。

### 固定内存与执行器

- [ ] **MEM-01**: 用户传入总 arena 和 limits 后，库在创建阶段完成对象、队列、候选、包缓存和 SDP 缓冲区切分。
- [ ] **MEM-02**: 创建成功后，核心库运行期不依赖动态内存增长。
- [ ] **MEM-03**: 当 limits 不足时，库返回可诊断的容量错误，指出不足的资源类别。
- [ ] **EXE-01**: 用户可以提供 `signaling`、`media`、`network` 三类 `post + timer` 执行器。
- [ ] **EXE-02**: 库内部不创建线程，不拥有平台事件循环。
- [ ] **EXE-03**: 用户可以把三个执行器映射到三个线程、同一线程或 superloop。

### SDP/JSEP

- [ ] **SDP-01**: 库可以生成 Chrome 可接受的 1v1 音视频 offer SDP。
- [ ] **SDP-02**: 库可以生成 Chrome 可接受的 1v1 音视频 answer SDP。
- [ ] **SDP-03**: 库可以解析 Chrome 1v1 音视频 offer/answer 中的 BUNDLE、rtcp-mux、DTLS fingerprint/setup、ICE 参数、H264、Opus 和媒体方向。
- [ ] **SDP-04**: 库支持 `sendrecv` 和 `recvonly` 媒体方向。
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

- [ ] **SEC-01**: 库通过 backend vtable 驱动 DTLS 握手，不在核心 API 中绑定特定 TLS 库。
- [ ] **SEC-02**: 库校验远端 SDP fingerprint 与 DTLS 证书一致。
- [ ] **SEC-03**: 库从 DTLS 握手导出 SRTP keying material。
- [ ] **SEC-04**: 库通过 backend vtable 对 RTP/RTCP 执行 SRTP/SRTCP protect 和 unprotect。
- [ ] **SEC-05**: 安全后端错误被映射为稳定的 `rtc_status_t` 和 observer 错误事件。

### RTP/RTCP 与媒体帧

- [ ] **RTP-01**: 用户可以提交 Opus frame，库负责 RTP packetize、时间戳、序列号和 SRTP 保护。
- [ ] **RTP-02**: 用户可以接收从 RTP depacketize 得到的 Opus frame。
- [ ] **RTP-03**: 用户可以提交 H264 access unit，库支持 single NALU 和 FU-A 发送。
- [ ] **RTP-04**: 用户可以接收从 RTP depacketize 得到的 H264 access unit，接收方向有限支持 STAP-A。
- [ ] **RTP-05**: 单个 `PeerConnection` 最多支持 1 路音频和 1 路视频。
- [ ] **RTCP-01**: 库支持 RTCP Sender Report 和 Receiver Report。
- [ ] **RTCP-02**: 库支持 RTCP SDES。
- [ ] **RTCP-03**: 库支持发送和接收 PLI。
- [ ] **RTCP-04**: 库解析 NACK 并通过 observer 上报，但不执行重传。

### 可观测性

- [ ] **OBS-01**: 库提供统一 observer vtable 输出状态事件、错误事件、本地候选、远端媒体帧和待发送 datagram。
- [ ] **OBS-02**: 库提供关键计数器，覆盖 ICE、DTLS、SRTP、RTP、RTCP、媒体帧和容量错误。
- [ ] **OBS-03**: 库提供 trace hook，能关联 PeerConnection、子系统、状态转换、错误码和关键字段。
- [ ] **OBS-04**: NACK、PLI、ICE 失败、DTLS 失败、SRTP 解保护失败等未自动修复事件都有可观测输出。

### 构建、示例与验收

- [ ] **BLD-01**: 项目提供 CMake 静态库构建目标。
- [ ] **BLD-02**: 项目提供公共头文件安装或导出规则。
- [ ] **TST-01**: 项目包含 SDP/JSEP、ICE/STUN、RTP/RTCP、固定内存和错误路径的自动化测试。
- [ ] **EXM-01**: 项目提供本地 Chrome 页面用于发起或接听 1v1 音视频通话。
- [ ] **EXM-02**: 项目提供最小信令示例，用于交换 SDP 和 trickle ICE candidate。
- [ ] **ACC-01**: 用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收。

## v2 Requirements

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

## Out of Scope

| Feature | Reason |
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

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| API-01 | Phase 1 | Pending |
| API-02 | Phase 1 | Pending |
| API-03 | Phase 1 | Pending |
| API-05 | Phase 1 | Pending |
| MEM-01 | Phase 1 | Pending |
| MEM-02 | Phase 1 | Pending |
| MEM-03 | Phase 1 | Pending |
| EXE-01 | Phase 1 | Pending |
| EXE-02 | Phase 1 | Pending |
| EXE-03 | Phase 1 | Pending |
| OBS-01 | Phase 1 | Pending |
| OBS-02 | Phase 1 | Pending |
| OBS-03 | Phase 1 | Pending |
| BLD-01 | Phase 1 | Pending |
| BLD-02 | Phase 1 | Pending |
| SDP-01 | Phase 2 | Pending |
| SDP-02 | Phase 2 | Pending |
| SDP-03 | Phase 2 | Pending |
| SDP-04 | Phase 2 | Pending |
| API-04 | Phase 2 | Pending |
| SDP-05 | Phase 3 | Pending |
| ICE-01 | Phase 3 | Pending |
| ICE-02 | Phase 3 | Pending |
| ICE-03 | Phase 3 | Pending |
| ICE-04 | Phase 3 | Pending |
| ICE-05 | Phase 3 | Pending |
| NET-01 | Phase 3 | Pending |
| NET-02 | Phase 3 | Pending |
| NET-03 | Phase 3 | Pending |
| SEC-01 | Phase 4 | Pending |
| SEC-02 | Phase 4 | Pending |
| SEC-03 | Phase 4 | Pending |
| SEC-04 | Phase 4 | Pending |
| SEC-05 | Phase 4 | Pending |
| RTP-01 | Phase 5 | Pending |
| RTP-02 | Phase 5 | Pending |
| RTP-03 | Phase 5 | Pending |
| RTP-04 | Phase 5 | Pending |
| RTP-05 | Phase 5 | Pending |
| RTCP-01 | Phase 5 | Pending |
| RTCP-02 | Phase 5 | Pending |
| RTCP-03 | Phase 5 | Pending |
| RTCP-04 | Phase 5 | Pending |
| OBS-04 | Phase 5 | Pending |
| TST-01 | Phase 6 | Pending |
| EXM-01 | Phase 6 | Pending |
| EXM-02 | Phase 6 | Pending |
| ACC-01 | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 48 total
- Mapped to phases: 48
- Unmapped: 0

---
*Requirements defined: 2026-05-10*
*Last updated: 2026-05-10 after initial definition*
