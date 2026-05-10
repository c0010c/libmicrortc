# 功能研究

## 功能分层

本项目不是终端应用，而是底层 WebRTC 连接库。功能应按“用户可集成能力”和“互通验收能力”组织。

## 基础必备能力

| 功能 | 首版要求 | 复杂度 | 依赖 |
|------|----------|--------|------|
| PeerConnection 生命周期 | 创建、销毁、状态推进、observer 事件 | 高 | API、执行器、状态机 |
| Offer/Answer | `createOffer`、`createAnswer`、本地/远端描述设置 | 高 | SDP/JSEP |
| Trickle ICE | 本地候选事件、远端候选输入 | 中高 | ICE、STUN、执行器 |
| Full ICE + STUN | host/srflx candidate、检查列表、提名、状态事件 | 高 | 网络 datagram、定时器 |
| DTLS-SRTP | DTLS 握手、fingerprint 校验、SRTP key export | 高 | 安全后端、SDP |
| RTP/RTCP 音频 | Opus frame 输入输出、时间戳、SR/RR/SDES | 中高 | RTP、RTCP、SRTP |
| RTP/RTCP 视频 | H264 access unit 输入输出、single NALU、FU-A、有限 STAP-A | 高 | RTP payload、SRTP |
| PLI | 接收/发送关键帧请求事件 | 中 | RTCP |
| NACK 可观测 | 解析并上报 NACK，不重传 | 中 | RTCP、observer |
| 固定内存 | limits 驱动对象、队列、缓存和字符串容量 | 高 | allocator、API |
| 无线程执行器 | 三类 executor post/timer 契约 | 高 | API、状态机亲和 |
| UDP datagram 边界 | 输入 datagram、输出待发送 datagram | 中 | ICE、DTLS、RTP demux |
| 可观测性 | 状态事件、错误事件、计数器、trace hook | 中高 | 全子系统 |
| Chrome 验收示例 | 本地页面、信令示例、1v1 音视频通话 | 中 | 示例、文档 |

## 差异化能力

| 功能 | 价值 | 首版处理 |
|------|------|----------|
| 严格固定内存报告 | 用户可在创建前估算容量和失败原因 | 纳入 v1 |
| 统一 trace hook | 便于嵌入式生产排障 | 纳入 v1 |
| 后端无关 DTLS/SRTP | 适配不同许可和平台要求 | 纳入 v1 |
| 执行器亲和断言 | 提前暴露跨线程误用 | 纳入 v1 |
| 浏览器 API 近似命名 | 降低 WebRTC 使用者迁移成本 | 纳入 v1 |

## 反功能

| 功能 | 不做原因 |
|------|----------|
| TURN | 复杂度、认证、分配生命周期和中继数据面会显著扩大首版范围 |
| DataChannel/SCTP | 与首版音视频互通目标无关，会引入 SCTP 和可靠传输复杂度 |
| 多路音视频 | 固定内存和 SDP/JSEP 复杂度增加，首版只需 1 音频 + 1 视频 |
| 完整拥塞控制 | 需要带宽估计、发送节奏、码率闭环和媒体编码器协作 |
| NACK 重传 | 需要发送包缓存、重传调度和拥塞交互，首版只解析上报 |
| 泛 SDP 兼容 | 会稀释 Chrome 1v1 验收目标 |
| 内置编解码 | 依赖和平台差异巨大，不符合库边界 |

## 依赖关系

1. API、arena、executor、observer 是所有子系统的前置。
2. SDP/JSEP 决定 ICE、DTLS、RTP 参数，必须早于完整互通。
3. ICE datagram demux 与 DTLS/RTP demux 共享网络入口，需要统一分类规则。
4. DTLS-SRTP key export 是 RTP/SRTP 可用的前置。
5. 示例验收需要贯穿 SDP、ICE、DTLS、SRTP、RTP/RTCP 和媒体 API。

## 资料来源

- W3C WebRTC: https://www.w3.org/TR/webrtc/
- RFC 8829 JSEP: https://www.rfc-editor.org/rfc/rfc8829
- RFC 8834 WebRTC RTP: https://www.rfc-editor.org/rfc/rfc8834
- RFC 6184 H.264 RTP payload: https://www.rfc-editor.org/rfc/rfc6184
- RFC 7587 Opus RTP payload: https://www.rfc-editor.org/rfc/rfc7587
