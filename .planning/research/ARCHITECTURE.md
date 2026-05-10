# 架构研究

## 推荐架构

采用分层状态机架构，`PeerConnection` 作为编排门面，具体协议由独立子系统负责。所有子系统共享固定内存池、执行器亲和规则、事件/计数器/trace 基础设施。

## 主要组件

| 组件 | 职责 | 不负责 |
|------|------|--------|
| Public API | 不透明句柄、配置、limits、生命周期、错误码 | 具体协议细节 |
| Arena/Limits | 创建期容量计算、分区、固定对象池 | 运行期扩容 |
| Executors | `post`、`timer`、取消/触发语义、亲和检查 | 创建线程 |
| Observer | 状态、错误、媒体帧、候选、datagram、trace 回调 | 拥有回调数据生命周期之外的资源 |
| SDP/JSEP | Chrome 最小 SDP 生成/解析、offer/answer 状态 | 泛 SDP 兼容 |
| ICE/STUN | candidate、检查列表、connectivity checks、提名 | TURN |
| Transport Demux | STUN、DTLS、RTP/RTCP datagram 分类 | socket I/O |
| DTLS Backend | 握手驱动、fingerprint、SRTP key export | 绑定特定库 |
| SRTP Backend | RTP/RTCP protect/unprotect、重放保护 | 绑定特定库 |
| RTP Media | Opus/H264 packetize/depacketize、序列号、时间戳 | 编解码 |
| RTCP | SR/RR、SDES、PLI、NACK 解析上报 | NACK 重传 |
| Observability | 计数器、trace hook、关键状态快照 | 外部日志系统 |
| Examples | Chrome 页面、信令、平台 glue | 生产信令服务 |

## 数据流

### 信令方向

1. 用户调用 `createOffer` 或 `createAnswer`。
2. `PeerConnection` 在 signaling executor 上推进 JSEP 状态。
3. SDP/JSEP 生成本地描述，observer 输出给用户信令层。
4. 用户调用 `setRemoteDescription` 和 `addIceCandidate`。
5. ICE、DTLS 和媒体参数被分发到 network/media 子系统。

### 网络输入方向

1. 用户收到 UDP datagram 后调用库的 network 输入 API。
2. Transport demux 判断 STUN、DTLS、RTP 或 RTCP。
3. STUN 进入 ICE；DTLS 进入安全后端；RTP/RTCP 先经 SRTP 解保护。
4. RTP depacketize 后在 media executor 输出 Opus frame 或 H264 access unit。
5. RTCP 更新计数器、状态并触发 PLI/NACK 等 observer 事件。

### 媒体发送方向

1. 用户提交 Opus frame 或 H264 access unit。
2. media executor 完成 RTP packetize、时间戳和序列号处理。
3. network executor 经 SRTP protect 后输出 datagram 给用户发送。
4. RTCP sender report 和 SDES 由定时器驱动。

## 构建顺序建议

1. 项目骨架、公共 API、错误码、arena/limits、执行器和 observer。
2. SDP/JSEP 最小模型和 round-trip 测试。
3. STUN 与 ICE 状态机，先 host，再 srflx。
4. DTLS/SRTP backend vtable 和参考适配。
5. RTP/RTCP 基础包处理、Opus、H264。
6. `PeerConnection` 集成与 datagram demux。
7. Chrome 页面和信令示例。
8. 可观测性补强、压力和失败路径测试。

## 架构风险

- 执行器亲和如果后补，会导致 API 和状态机难以收敛。
- 固定内存如果不从第一阶段建立，后续很容易出现隐式 malloc。
- SDP/JSEP 如果一开始追求泛化，会拖慢 Chrome 最小互通。
- DTLS/SRTP 后端 vtable 如果职责过宽，会泄漏第三方库模型。
- 示例如果放到最后才做，互通风险会集中爆发。

## 资料来源

- RFC 8829 JSEP: https://www.rfc-editor.org/rfc/rfc8829
- RFC 8445 ICE: https://www.rfc-editor.org/rfc/rfc8445
- RFC 5764 DTLS-SRTP: https://www.rfc-editor.org/rfc/rfc5764
- RFC 8834 WebRTC RTP: https://www.rfc-editor.org/rfc/rfc8834
