# Feature Research

**Domain:** 纯 C、资源受限设备上的 WebRTC 媒体传输库  
**Researched:** 2026-05-12  
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (v1 必备)

缺这些能力，库即使能跑 demo，也很难稳定与 Chrome 做生产级 1v1 H264/Opus 通话。

| Feature | 分类 | Why Expected | Complexity | Notes | Confidence |
|---------|------|--------------|------------|-------|------------|
| SDP offer/answer 的 JSEP 子集 | Chrome 互通 | Chrome 互通必须通过 SDP 协商媒体、ICE、DTLS fingerprint、RTP/RTCP 选项和 codec 参数。 | HIGH | 支持生成/解析 audio/video m-line、`a=mid`、`a=group:BUNDLE`、`a=rtcp-mux`、`UDP/TLS/RTP/SAVPF`、`a=setup`、`a=fingerprint`、`a=ice-ufrag/pwd`、`a=msid`、方向属性、payload type/fmtp/rtcp-fb。只承诺本库实际支持的能力。 | HIGH |
| BUNDLE + RTP/RTCP mux | Chrome 互通 | 现代 WebRTC 默认复用传输流；减少 NAT 绑定也符合资源受限目标。 | MEDIUM | v1 建议只支持 bundle-only/rtcp-mux，不支持非 mux 的双端口 RTP/RTCP。必须实现 STUN/DTLS/RTP/RTCP demux。 | HIGH |
| ICE full agent + STUN + TURN/UDP | Chrome 互通 | 真实公网 NAT 穿透不能只靠 host candidate；TURN relay 是连接兜底。 | HIGH | 支持 host、srflx、relay candidate；controlling/controlled role；priority/foundation；connectivity checks；nomination；ICE restart 的最小状态处理；TURN v1 先 UDP relay。 | HIGH |
| Trickle ICE | Chrome 互通 | Chrome 常规调用流会边收集边通过 signaling 发送 candidate；等待完整 gathering 会显著拖慢建连。 | MEDIUM | 支持 `ice-options:trickle`、增量 local/remote candidate、end-of-candidates、按 ufrag 归属 ICE generation。 | HIGH |
| DTLS-SRTP 接入与证书指纹校验 | Chrome 互通 | WebRTC 媒体强制走 DTLS-SRTP/SRTP，不能明文 RTP 互通生产 Chrome。 | HIGH | DTLS 通过 vtable；默认 mbedTLS；支持 DTLS role、fingerprint 校验、SRTP key export、常用 SRTP protection profile；错误必须可诊断。 | HIGH |
| SRTP/SRTCP protect/unprotect | Chrome 互通 | RTP/SAVPF 只是协商层，实际媒体必须加密、认证、抗重放。 | HIGH | SRTP 通过 vtable；默认 libsrtp；覆盖 RTP 和 RTCP；区分 auth fail、replay、bad ROC、buffer too small。 | HIGH |
| RTP/RTCP 核心 | Chrome 互通 / H264/Opus | H264/Opus 只解决 payload；WebRTC 还需要 RTP 序号、时间戳、SSRC、RTCP SR/RR/SDES/feedback。 | HIGH | 支持 SSRC/CNAME、sequence wrap、jitter/loss 统计、SR/RR RTT、NACK 接收/发送、PLI/FIR 接收并回调应用请求关键帧。RTX 可先不做，但不能在 SDP 宣称支持。 | HIGH |
| H264 RTP packetize/depacketize | H264/Opus 媒体传输 | Chrome 侧 H264 互通依赖 RFC 6184 payload 格式和 SDP fmtp 正确匹配。 | HIGH | v1 支持 packetization-mode=1、single NAL、STAP-A、FU-A；解析/生成 `profile-level-id`、`level-asymmetry-allowed`；应用输入/输出 H264 access unit；处理 SPS/PPS/IDR 边界；MTU 分片。 | HIGH |
| Opus RTP packetize/depacketize | H264/Opus 媒体传输 | Opus 是 WebRTC mandatory audio codec；库不编码但必须正确承载 Opus frame。 | MEDIUM | SDP 使用 `opus/48000/2`；处理 `ptime`、`maxptime`、`maxaveragebitrate`、`stereo`、`useinbandfec`、`usedtx`；应用输入/输出 Opus packet/frame；不做 resample/AEC/NS。 | HIGH |
| Encoded media API | H264/Opus 媒体传输 | 项目明确不做编解码，因此 API 必须面向已编码帧而不是 PCM/YUV。 | MEDIUM | 发送侧输入 H264 access unit / Opus frame、timestamp、marker/keyframe metadata；接收侧回调完整 access unit/frame；支持 bounded copy 或 caller-owned buffer 生命周期约束。 | HIGH |
| 生产状态机事件 | 生产可观测性 | WebRTC 失败通常发生在 ICE、DTLS、SRTP、SDP 协商边界；只返回 generic failure 无法运维。 | MEDIUM | 暴露 PeerConnection、ICE gathering、ICE connection、DTLS、SRTP、media track 状态；事件包含 reason code、phase、peer id、candidate pair、track/ssrc。 | HIGH |
| W3C-like stats 子集 | 生产可观测性 | Chrome、Pion、Kinesis C SDK 都围绕 getStats/网络媒体统计诊断连接质量。 | MEDIUM | 至少提供 peer-connection、transport、candidate-pair、local/remote candidate、inbound/outbound RTP、codec/track 统计；包含 bytes/packets/lost/jitter/RTT/NACK/PLI/FIR/keyframes/selected pair。 | HIGH |
| 包级诊断钩子 | 生产可观测性 | 嵌入式现场问题常需要定位丢包、乱序、SRTP 解密失败、RTCP feedback 和 MTU 分片。 | MEDIUM | 可配置采样；支持 RTP/RTCP/STUN/DTLS 方向、长度、SSRC/PT/seq/timestamp、错误码；默认关闭 payload dump，避免泄露媒体内容。 | MEDIUM |
| 固定内存预算与水位 | 固定内存 | 资源受限设备不能接受隐式动态增长；OOM 必须是可预期状态而不是随机崩溃。 | HIGH | `rtc_context` 初始化时声明最大 PC、track、candidate、candidate pair、timer、packet buffer、reassembly buffer、RTCP queue；运行时只从 allocator/arena 取；暴露当前/峰值/失败次数。 | HIGH |
| 有界队列和背压 | 固定内存 / 多 PeerConnection | RTP burst、TURN relay、多个 PC 同时发送会快速耗尽内存。 | MEDIUM | 每 PC、每 track、全局队列都有容量；满时返回明确错误或丢弃策略事件；发送 API 区分 accepted、would-block、dropped。 | HIGH |
| 单线程 context 驱动多 PeerConnection | 多 PeerConnection | 项目目标是一个 `rtc_context` 驱动多个 PC，避免内部锁，同时支持 mesh 预留。 | HIGH | v1 至少支持多个 PC 生命周期、定时器、candidate check、DTLS/SRTP I/O 复用；提供公平调度，避免一个 PC 的视频 burst 饿死其他 PC 的 ICE/RTCP。 | HIGH |
| Chrome 互通测试夹具 | Chrome 互通 / 生产可观测性 | 没有自动化互通测试，SDP/ICE/H264 fmtp 的小差异会反复回归。 | HIGH | Playwright/Chrome headless 或可复现实机测试：offerer/answerer 双向、audio-only、video-only、AV、TURN relay、packet loss、ICE restart、multi-PC smoke。 | HIGH |

### Differentiators (竞争优势)

这些不是 WebRTC 标准本身要求，但与 libmicrortc 的核心价值强相关，应在 v1 或 v1.x 中重点保留设计空间。

| Feature | 分类 | Value Proposition | Complexity | Notes | Confidence |
|---------|------|-------------------|------------|-------|------------|
| Sans-I/O 可重放协议核心 | 固定内存 / 生产可观测性 | 同一段网络输入、时钟和随机数可在测试中重放，显著降低 ICE/DTLS/RTP 状态机调试成本。 | HIGH | 核心不直接调用 socket/thread/time；所有输出以 action/event 形式交给外壳；trace 可保存后重放。 | HIGH |
| 内存预算规划器 | 固定内存 / 多 PeerConnection | 用户能在启动时知道“4 个 PC、每个 2 track、TURN relay”需要多少内存。 | MEDIUM | 提供 `rtc_estimate_memory(config)` 和运行时拒绝原因；按组件拆分预算，避免只给一个总数。 | MEDIUM |
| W3C stats 命名对齐但 C-friendly | 生产可观测性 | 运维人员可把本库指标与 Chrome `chrome://webrtc-internals`、Pion/Kinesis 指标对照。 | MEDIUM | 不复制 JS API；用快照结构或 iterator；字段命名尽量贴近 inbound-rtp/outbound-rtp/candidate-pair。 | HIGH |
| 多 PC 公平调度指标 | 多 PeerConnection | mesh 或多路预览场景下，能看到每个 PC 被调度、丢包、排队和发包情况。 | MEDIUM | 暴露 per-PC send budget、queue depth、timer lag、starvation counter；为后续拥塞控制接入留接口。 | MEDIUM |
| 可替换安全后端契约 | Chrome 互通 / 固定内存 | 用户可替换 mbedTLS/libsrtp，适配芯片安全模块或已有 FIPS/厂商栈。 | MEDIUM | vtable 必须包含 size/arena 预估、session lifecycle、export key、protect/unprotect 错误映射。 | HIGH |
| mDNS ICE candidate 支持 | Chrome 互通 | Chrome host candidate 可能使用 `.local` 名称；支持 mDNS 可提升局域网无 STUN 场景互通。 | MEDIUM | v1 可先正确解析并忽略不支持的 mDNS candidate；v1.x 加 mDNS query/response 或平台 resolver 钩子。 | MEDIUM |
| 互通故障报告包 | 生产可观测性 | 用户遇到 Chrome 失败时可导出最小诊断包，而不暴露媒体 payload。 | MEDIUM | 包含本地/远端 SDP、状态时间线、selected pair、错误码、stats 摘要、内存水位、协议 trace 摘要。 | MEDIUM |

### Anti-Features (明确不做 / 反功能)

这些功能看起来有吸引力，但会冲击 v1 的纯 C、固定内存、Chrome 1v1 H264/Opus 媒体传输目标。

| Feature | 分类 | Why Requested | Why Problematic | Alternative | Confidence |
|---------|------|---------------|-----------------|-------------|------------|
| 内置 H264/Opus 编解码 | H264/Opus 媒体传输 | 用户希望“一站式”采集到播放。 | 编解码带来平台硬件差异、许可证、线程、缓冲、A/V sync 和内存不可控；与项目“只传输已编码媒体”冲突。 | 提供 encoded media API 和清晰 codec 参数协商；应用接入硬编/软编。 | HIGH |
| W3C JavaScript API 克隆 | Chrome 互通 | 看起来便于迁移浏览器代码。 | C ABI 下复制 RTCPeerConnection/Transceiver/Promise/MediaStreamTrack 会引入大量非目标复杂度。 | 提供小而明确的 C API：context、peer、track、encoded frame、event/stats。 | HIGH |
| DataChannel/SCTP | Chrome 互通 | WebRTC 常被等同于媒体 + DataChannel。 | SCTP/DTLS data stack 是另一套可靠传输和背压系统，v1 会分散媒体互通与固定内存验证。 | v1 不协商 `m=application`；v2 另立阶段。 | HIGH |
| 内置信令服务器/客户端 | Chrome 互通 | demo 需要 WebSocket/HTTP/MQTT 交换 SDP/candidate。 | 信令不是 WebRTC 标准协议栈的一部分，场景差异大；内置会扩大依赖和安全面。 | 只生成/解析 SDP 与 candidate；提供示例 signaling，不进核心库。 | HIGH |
| 拥塞控制闭环（TWCC/REMB/GCC） | 生产可观测性 / H264/Opus | 用户希望自动码率自适应。 | 正确实现需要带宽估计、pacing、encoder bitrate 控制和复杂验证；项目 v1 明确不做。 | v1 采集 RTCP/stats、NACK/PLI/keyframe callback、send queue 指标；保留后续 congestion controller vtable。 | HIGH |
| Simulcast/SVC/多编码器协商 | H264/Opus 媒体传输 | 多清晰度和多人场景常见。 | SDP/RID/SSRC/FEC/RTX/带宽控制复杂度高，且与 1v1 H264/Opus v1 目标不匹配。 | v1 单 audio track + 单 video track 为主；多 track 数据结构预留但不承诺 simulcast/SVC。 | HIGH |
| TURN over TCP/TLS | Chrome 互通 | 企业网络和严苛防火墙需要 443/TLS relay。 | TCP/TLS relay 增加连接管理、队头阻塞、证书/代理边界和测试矩阵；项目已将其列为后续评估。 | v1 支持 TURN/UDP；接口保留 relay transport 类型和错误码。 | HIGH |
| 动态无限缓冲“保证不丢帧” | 固定内存 | 用户希望弱网下视频不丢。 | 实时媒体中无限排队会导致延迟爆炸和 OOM，破坏固定内存承诺。 | 有界队列、明确丢弃策略、关键帧请求、统计暴露。 | HIGH |
| SDP munging 容错器 | Chrome 互通 | 试图自动修正各种浏览器/服务端 SDP。 | 容易生成“看似可协商但实际不支持”的 SDP，导致隐蔽互通故障。 | 严格 parser + capability-driven generator；未知/不支持字段保留或拒绝，但不伪装支持。 | HIGH |
| 内置 SFU/多人会议/mesh 管理 | 多 PeerConnection | 多 PC 让人自然想做会议。 | 调度、带宽、公平性、拓扑管理、媒体转发都超出传输库边界。 | 库只支持多个独立 PeerConnection；应用层决定拓扑和转发。 | HIGH |

## Feature Dependencies

```text
Chrome 1v1 H264/Opus 互通
    ├──requires──> SDP/JSEP 子集
    │                  ├──requires──> BUNDLE + rtcp-mux
    │                  ├──requires──> H264/Opus payload capability
    │                  └──requires──> DTLS fingerprint/setup 协商
    ├──requires──> ICE full + STUN + TURN/UDP
    │                  └──enhanced-by──> Trickle ICE
    ├──requires──> DTLS-SRTP
    │                  └──requires──> SRTP/SRTCP protect/unprotect
    └──requires──> RTP/RTCP core
                       ├──requires──> H264 packetize/depacketize
                       ├──requires──> Opus packetize/depacketize
                       └──enhanced-by──> NACK/PLI/FIR + keyframe callback

固定内存承诺
    ├──requires──> 初始化期资源预算
    ├──requires──> 有界队列和背压
    └──requires──> Sans-I/O 驱动模型

生产可观测性
    ├──requires──> 状态机事件
    ├──requires──> W3C-like stats 子集
    ├──requires──> 内存水位
    └──enhanced-by──> 包级诊断钩子 / 互通故障报告包

多 PeerConnection
    ├──requires──> rtc_context 单线程调度
    ├──requires──> per-PC 资源预算
    └──requires──> 公平定时器和发送队列

DataChannel/SCTP
    └──conflicts-in-v1──> 固定内存 + 媒体互通 MVP 聚焦

拥塞控制闭环
    └──requires-later──> stats + RTCP feedback + pacing + encoder bitrate control
```

### Dependency Notes

- **不要先做媒体 API 再补 SDP 能力。** H264/Opus 的 fmtp、payload type、rtcp-fb、SSRC、方向属性会反向决定 API 需要携带的 metadata。
- **ICE/TURN 早于 DTLS/SRTP 验证。** 没有稳定 selected candidate pair，DTLS 问题会被 NAT/relay 问题掩盖。
- **固定内存要早于多 PC。** 多 PC 不是“循环创建多个对象”，而是全局 timer、candidate pair、packet buffer 和发送公平性的资源模型。
- **可观测性必须贯穿每个阶段。** ICE、DTLS、SRTP、RTP 错误如果后补，很难从状态机边界恢复足够上下文。
- **NACK/PLI/FIR 是 v1，RTX 是 v1.x 候选。** Chrome SDP 常出现 RTX，但 v1 可以选择不协商；一旦协商就必须完整处理 apt/RTX SSRC 和重传缓存。
- **mDNS candidate 是兼容性增强，不是 v1 硬门槛。** v1 至少不能因 `.local` candidate 解析失败而中断协商；是否解析 mDNS 可作为 v1.x 或平台适配项。

## MVP Definition

### Launch With (v1)

- [ ] SDP/JSEP 子集：能与 Chrome 完成 audio/video offer-answer，严格只声明支持的 codec/rtcp/transport 能力。
- [ ] ICE full + STUN + TURN/UDP + Trickle ICE：覆盖真实 NAT 和 relay 路径。
- [ ] DTLS-SRTP + SRTP/SRTCP：通过 vtable 接入 mbedTLS/libsrtp 默认实现。
- [ ] RTP/RTCP core：SR/RR/SDES、NACK、PLI/FIR、loss/jitter/RTT、SSRC/CNAME、rtcp-mux。
- [ ] H264 packetization-mode=1：single NAL、STAP-A、FU-A、profile-level-id、keyframe request 回调。
- [ ] Opus RTP：`opus/48000/2`、ptime/maxptime/fmtp、encoded frame 输入输出。
- [ ] 固定内存 budget：初始化期声明容量，运行期无不可控增长，暴露水位和 OOM reason。
- [ ] 单线程 `rtc_context` 多 PC：至少验证多个 PC 并发建连、保活、发送和关闭。
- [ ] 生产可观测性：状态事件、stats 快照、错误码、包级诊断钩子、互通测试日志。
- [ ] Chrome 互通测试：Chrome offer/answer、TURN relay、audio-only、video-only、AV、弱网丢包 smoke。

### Add After Validation (v1.x)

- [ ] RTX：当 NACK/PLI 已稳定且有有界重传缓存设计后加入。
- [ ] mDNS candidate resolver：当局域网无 STUN 互通成为真实需求时加入。
- [ ] ICE restart 完整回归矩阵：v1 可做最小状态处理，v1.x 强化异常恢复。
- [ ] TURN over TCP/TLS：当目标部署环境证明 UDP relay 不足时加入。
- [ ] 更完整的 RTCP XR / 带宽估计字段：先收集 stats，再决定拥塞控制接入点。
- [ ] poll/event-loop 风格 API：回调外壳稳定后，为不同嵌入式调度模型补充。

### Future Consideration (v2+)

- [ ] DataChannel/SCTP：独立阶段，单独评估内存、背压和 API。
- [ ] 拥塞控制闭环：TWCC/REMB/GCC、pacing、encoder bitrate callback 作为完整系统设计。
- [ ] Simulcast/SVC/RID/FEC：在单流互通和 stats 稳定后再做。
- [ ] 非 Linux 平台适配：核心 Sans-I/O 稳定后扩展平台外壳。
- [ ] 多方会议/SFU 辅助：应用层或独立库，不进入 v1 媒体传输核心。

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority | Confidence |
|---------|------------|---------------------|----------|------------|
| SDP/JSEP 子集 | HIGH | HIGH | P1 | HIGH |
| ICE full + STUN + TURN/UDP | HIGH | HIGH | P1 | HIGH |
| Trickle ICE | HIGH | MEDIUM | P1 | HIGH |
| DTLS-SRTP vtable + 默认 mbedTLS | HIGH | HIGH | P1 | HIGH |
| SRTP/SRTCP vtable + 默认 libsrtp | HIGH | HIGH | P1 | HIGH |
| BUNDLE + rtcp-mux + packet demux | HIGH | MEDIUM | P1 | HIGH |
| RTP/RTCP core + NACK/PLI/FIR | HIGH | HIGH | P1 | HIGH |
| H264 RTP packetization-mode=1 | HIGH | HIGH | P1 | HIGH |
| Opus RTP | HIGH | MEDIUM | P1 | HIGH |
| Encoded media API | HIGH | MEDIUM | P1 | HIGH |
| 固定内存预算与水位 | HIGH | HIGH | P1 | HIGH |
| 有界队列和背压 | HIGH | MEDIUM | P1 | HIGH |
| 状态事件和错误码 | HIGH | MEDIUM | P1 | HIGH |
| W3C-like stats 子集 | HIGH | MEDIUM | P1 | HIGH |
| Chrome 互通测试夹具 | HIGH | HIGH | P1 | HIGH |
| 单线程 context 多 PC | MEDIUM | HIGH | P1 | HIGH |
| 包级诊断钩子 | MEDIUM | MEDIUM | P2 | MEDIUM |
| RTX | MEDIUM | HIGH | P2 | HIGH |
| mDNS candidate resolver | MEDIUM | MEDIUM | P2 | MEDIUM |
| TURN over TCP/TLS | MEDIUM | HIGH | P3 | HIGH |
| DataChannel/SCTP | LOW for v1 | HIGH | P3 | HIGH |
| 拥塞控制闭环 | MEDIUM | HIGH | P3 | HIGH |
| Simulcast/SVC | LOW for v1 | HIGH | P3 | HIGH |

**Priority key:**

- P1: v1 launch 必须有，或必须明确“不支持且不协商”。
- P2: v1.x 值得加入，但不阻塞 H264/Opus 1v1 基线。
- P3: 后续大阶段，当前只预留扩展点。

## Competitor Feature Analysis

| Feature | libdatachannel | Kinesis Video Streams WebRTC C SDK | GStreamer webrtcbin | Our Approach |
|---------|----------------|-------------------------------------|---------------------|--------------|
| 浏览器互通 | 声称兼容 Chromium/Firefox/Safari；支持 JSEP、ICE、STUN/TURN、DTLS、SRTP、BUNDLE、Trickle ICE。 | 面向嵌入式设备到 web/mobile 的实时音视频。 | 实现多数 W3C PeerConnection API，支持 offer/answer、media/data channel。 | 只聚焦 Chrome 1v1 H264/Opus，但把互通测试和严格 SDP 能力声明作为 v1 硬门槛。 |
| 语言/API | C++17，提供 C bindings；API 类似简化 JS。 | C SDK，面向 AWS 信令和云服务集成。 | GObject/GStreamer pipeline 模型。 | 纯 C ABI；Sans-I/O core + C callback shell；不绑定云服务或媒体 pipeline。 |
| 媒体范围 | 支持 WebRTC Media Transport，可选禁用；还有 DataChannel/WebSocket。 | 支持 H264、H265、VP8、Opus、G.711 等，含 signaling 生态。 | 与 GStreamer RTP/codec pipeline 深度集成。 | v1 只做 H264/Opus RTP 承载，不做 codec、pipeline、DataChannel、signaling。 |
| 可观测性 | C API 有状态/gathering/track 回调；Context7 示例显示 bytes/rtt/selected pair。 | 官方发布 client metrics，覆盖 W3C 标准相关网络/媒体/数据流指标。 | 文档暴露 WebRTC stats 相关字段。 | W3C-like stats + 内存水位 + 包级诊断是核心 API，而不是 demo 辅助。 |
| 固定内存 | 未以固定 arena/预算作为核心卖点。 | 面向嵌入式，但固定内存契约需逐项确认。 | GStreamer 生态更重，不适合固定小内存核心目标。 | 把固定内存预算作为差异化能力和验收标准。 |
| 多 PC | 支持 PeerConnection 对象模型。 | 支持设备/peer 场景，但依赖 SDK 结构。 | 支持多个 bin/pipeline，但资源模型较重。 | 单 `rtc_context` 单线程驱动多个 PC，显式资源预算和公平调度。 |

## Sources

- RFC 8834, **Media Transport and Use of RTP in WebRTC** — SRTP/SRTCP、RTP/RTCP mux、MID、SSRC、RTCP feedback 要求。https://www.rfc-editor.org/rfc/rfc8834
- RFC 8835, **Transports for WebRTC** — WebRTC transport、DTLS-SRTP、ICE、DTLS/RTP mux。https://www.ietf.org/rfc/rfc8835.html
- RFC 7742, **WebRTC Video Processing and Codec Requirements** — WebRTC H264/VP8 mandatory codec 背景，H264 packetization-mode=1。https://www.rfc-editor.org/rfc/rfc7742.html
- RFC 7874, **WebRTC Audio Codec and Processing Requirements** — Opus mandatory audio codec 和 G.711/CN/DTMF 背景。https://www.rfc-editor.org/rfc/rfc7874.html
- RFC 7587, **RTP Payload Format for Opus** — `opus/48000/2`、ptime/maxptime/fmtp 参数。https://www.rfc-editor.org/rfc/rfc7587.html
- RFC 6184, **RTP Payload Format for H.264 Video** — packetization-mode、FU-A、STAP-A、profile-level-id、SDP fmtp。https://www.rfc-editor.org/rfc/rfc6184.html
- RFC 8838, **Trickle ICE** — 增量 candidate、end-of-candidates 和 ICE session 归属。https://www.rfc-editor.org/rfc/rfc8838
- W3C, **Identifiers for WebRTC's Statistics API** — peer-connection、transport、candidate-pair、inbound/outbound RTP stats。https://www.w3.org/TR/webrtc-stats/
- WebRTC/Chromium, **SRTP in WebRTC** — Chrome/WebRTC SRTP 与 DTLS-SRTP 说明。https://webrtc.googlesource.com/src/+/main/pc/g3doc/srtp.md
- libdatachannel official site and README — 轻量 WebRTC C/C++ 库能力、C bindings、ICE/STUN/TURN、DTLS/SRTP、Trickle ICE、BUNDLE、RTX。https://libdatachannel.org/ and https://github.com/paullouisageneau/libdatachannel
- Context7 `/paullouisageneau/libdatachannel` docs lookup — PeerConnection callbacks、candidate/description/state、bytes/rtt/selected pair 示例。HIGH confidence.
- Context7 `/pion/webrtc` docs lookup — GetStats、ICE/RTP stats、transport component structure。MEDIUM confidence for ecosystem pattern.
- AWS, **Kinesis Video Streams WebRTC SDK in C client metrics** — 嵌入式 C WebRTC SDK 中 client metrics 的生产价值。https://aws.amazon.com/about-aws/whats-new/2020/11/amazon-kinesis-video-streams-webrtc-sdk-in-c-supports-client-metrics/
- AWS Docs, **Stream live media SDKs** — C SDK 面向 embedded devices、实时双向音视频。https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/webrtc-sdks.html
- GStreamer, **webrtcbin documentation** — offer/answer、media/data channel、stats 字段和 transport stream 模型。https://gstreamer.freedesktop.org/documentation/webrtc/

---
*Feature research for: libmicrortc WebRTC C media transport*
*Researched: 2026-05-12*
