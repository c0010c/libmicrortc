# Phase 1 Source Manifest: 来源追溯规范

## 目的

`SOURCE-MANIFEST.md` 是后续 Phase 2+ 搬迁、裁剪、改名或重写派生 AWS KVS WebRTC C SDK 文件时的文件级来源追溯记录。它不预先列出 `reflib/kvs-webrtc-sdk` 的全量源码，只记录实际进入 `libmicrortc` 或被明确作为参考、排除依据的条目。

所有来源路径默认以本地基线 `reflib/kvs-webrtc-sdk` 为准，origin commit 默认使用 `9eebcc4`。GitHub upstream latest 不是默认来源。

## 更新规则

1. 任何从 `reflib/kvs-webrtc-sdk` 复制、裁剪、改名或重写派生进入本项目的文件，都必须追加 manifest 记录。
2. 每条记录必须能让 reviewer 从 `source_path` 找到本地来源，从 `target_path` 找到目标文件。
3. 如果一个目标文件由多个来源文件合并而来，使用多行记录同一个 `target_path`，并在 `notes` 中说明合并关系。
4. 如果一个来源文件只作为设计参考，没有派生代码进入目标文件，使用 `reference-only`。
5. 如果某个来源模块被明确排除，使用 `excluded`，并说明排除原因。
6. 不在 Phase 1 预填全量源文件，避免 manifest 变成不可维护的静态索引。

## 必填字段

| 字段 | 含义 | 规则 |
|------|------|------|
| `source_path` | 来源文件或目录在本地参考库中的路径 | 必须以 `reflib/kvs-webrtc-sdk/` 开头 |
| `target_path` | 目标文件或目标目录路径 | 尚无目标文件时填 `n/a` |
| `origin_commit` | 来源提交 | Phase 1 基线为 `9eebcc4` |
| `derivation` | 派生方式 | 必须使用下方允许值 |
| `notes` | 裁剪、保留、排除或后续确认说明 | 必须写明边界或原因 |

## 可选字段

| 字段 | 用途 |
|------|------|
| `module_class` | `core`、`excluded`、`supporting/reference-only` |
| `requirement` | 关联需求 ID，例如 `BASE-04`、`PROTO-01` |
| `phase_added` | 实际新增或派生进入项目的阶段 |
| `review_status` | `draft`、`reviewed`、`needs-update` |

## 允许的 derivation 值

| derivation | 含义 |
|------------|------|
| `copied` | 基本按来源文件复制，仅做路径或 include 调整 |
| `trimmed` | 从来源文件裁剪掉 AWS/KVS、sample 或平台无关部分 |
| `renamed` | 以来源文件为基础改名或移动，语义基本保留 |
| `rewritten-derived` | 参考来源实现重新组织或重写，仍属于派生实现 |
| `reference-only` | 只作为行为、测试或设计参考，没有直接派生代码进入目标 |
| `excluded` | 明确不进入核心库或目标交付 |

## 初始模块边界

| source_path | target_path | origin_commit | derivation | notes | module_class | requirement | phase_added | review_status |
|-------------|-------------|---------------|------------|-------|--------------|-------------|-------------|---------------|
| `reflib/kvs-webrtc-sdk/src/source/Ice/` | `src/...` | `9eebcc4` | `rewritten-derived` | 模板行：后续实际搬迁 Ice 文件时替换为具体文件路径和目标路径。 | `core` | `PROTO-01` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Stun/` | `src/...` | `9eebcc4` | `rewritten-derived` | STUN/TURN 消息处理核心候选，后续按实际文件追加。 | `core` | `PROTO-01` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/` | `src/...` | `9eebcc4` | `rewritten-derived` | DTLS/TLS 相关实现，v1 优先 OpenSSL 路径。 | `core` | `PROTO-02` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Srtp/` | `src/...` | `9eebcc4` | `rewritten-derived` | SRTP 会话核心候选。 | `core` | `PROTO-03` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sctp/` | `src/...` | `9eebcc4` | `rewritten-derived` | DataChannel 所需 SCTP 核心候选。 | `core` | `PROTO-05` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sdp/` | `src/...` | `9eebcc4` | `rewritten-derived` | SDP 序列化和反序列化核心候选。 | `core` | `PROTO-06` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/` | `src/...` | `9eebcc4` | `rewritten-derived` | RTP 和 codec packetization 候选。 | `core` | `PROTO-04` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/` | `src/...` | `9eebcc4` | `rewritten-derived` | RTCP、rolling buffer、NACK 相关候选。 | `core` | `PROTO-04` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/` | `src/...` | `9eebcc4` | `rewritten-derived` | PeerConnection、DataChannel、JitterBuffer、SessionDescription 等编排候选。 | `core` | `API-02` | future | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Signaling/` | `n/a` | `9eebcc4` | `excluded` | AWS/KVS signaling client，不能成为核心库依赖。 | `excluded` | `PROTO-07` | `01` | `reviewed` |
| `reflib/kvs-webrtc-sdk/src/source/Threadpool/` | `n/a` | `9eebcc4` | `reference-only` | 可参考 AWS 线程模型，但长期不属于核心边界。 | `supporting/reference-only` | `API-06` | `01` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Metrics/` | `n/a` | `9eebcc4` | `reference-only` | 可参考统计能力，是否进入核心由后续需求驱动。 | `supporting/reference-only` | `BASE-04` | `01` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sdp/Sdp.h` | `src/sdp.h` | `9eebcc4` | `rewritten-derived` | 参考 AWS SDP line marker、parse/serialize 模型和 session/media/attribute 边界，重写为 Phase 3 signaling-free 最小 private API。 | `core` | `PROTO-06` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sdp/Deserialize.c` | `src/sdp.c` | `9eebcc4` | `rewritten-derived` | 参考 AWS 逐行 SDP 解析方式，重写为只覆盖 Phase 3 round-trip 和 answer helper 的 C99 实现。 | `core` | `PROTO-06` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sdp/Serialize.c` | `src/sdp.c` | `9eebcc4` | `reference-only` | 作为 SDP 输出格式参考；Phase 3 仅生成最小 answer，不复制序列化实现。 | `core` | `PROTO-06` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/tst/` | `tests/fixtures/minimal_offer.sdp` | `9eebcc4` | `rewritten-derived` | 参考本地基线测试/样例 SDP 形态，重写为最小 offer fixture 用于 Phase 3 单元测试。 | `core` | `PROTO-06` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | `include/micrortc/peer_connection.h` | `9eebcc4` | `rewritten-derived` | 参考 AWS PeerConnection、SDP 和 ICE candidate 公共使用模型，裁剪为 MRTC 命名且不暴露 AWS/PIC 类型的 public API。 | `core` | `API-01` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/PeerConnection.c` | `src/peer_connection.c` | `9eebcc4` | `rewritten-derived` | 参考 create/free、callback/user data 和 candidate 边界，重写为 Phase 3 signaling-free opaque handle 状态机。 | `core` | `API-02` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.c` | `src/peer_connection.c` | `9eebcc4` | `reference-only` | 作为 set remote/local description 与 offer/answer 调用顺序参考；Phase 3 不复制 JSON signaling wrapper。 | `core` | `API-03` | `03` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Ice/IceAgent.h` | `src/ice/ice_agent.h` | `9eebcc4` | `rewritten-derived` | 参考 ICE candidate 表达和 agent 边界，Phase 4 Wave 1 先重写为 MRTC 私有 candidate parse/format 骨架。 | `core` | `PROTO-01` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Ice/IceAgent.c` | `src/ice/ice_agent.c` | `9eebcc4` | `rewritten-derived` | 参考 candidate 语义和 host path 验证入口，未复制 AWS/PIC agent 状态机。 | `core` | `PROTO-01` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Stun/Stun.h` | `src/stun/stun_message.h` | `9eebcc4` | `rewritten-derived` | 参考 STUN magic cookie、transaction id 和 message header 结构，重写为 MRTC 私有 STUN helper。 | `core` | `PROTO-01` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Stun/Stun.c` | `src/stun/stun_message.c` | `9eebcc4` | `rewritten-derived` | 参考 binding request/header 行为，Wave 1 只落基础序列化和 header 校验骨架。 | `core` | `PROTO-01` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Ice/TurnConnection.h` | `src/turn/turn_client.h` | `9eebcc4` | `rewritten-derived` | 参考 TURN server URL/relay 边界，重写为 MRTC 私有 URL 解析骨架。 | `core` | `NET-03` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Ice/TurnConnection.c` | `src/turn/turn_client.c` | `9eebcc4` | `rewritten-derived` | 参考 TURN relay 配置语义，未复制 AWS TURN allocation/channel-bind 状态机。 | `core` | `NET-03` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls.h` | `src/dtls/dtls_session.h` | `9eebcc4` | `rewritten-derived` | 参考 DTLS role/fingerprint 边界，Wave 1 先提供 MRTC 私有 role 与 fingerprint 骨架。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls_openssl.c` | `src/dtls/dtls_session.c` | `9eebcc4` | `reference-only` | OpenSSL 握手实现作为后续 Wave 3 参考；Wave 1 未复制 OpenSSL 代码。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.h` | `src/srtp/srtp_session.h` | `9eebcc4` | `rewritten-derived` | 参考 SRTP session 生命周期，Wave 1 先提供 MRTC 私有 session 骨架。 | `core` | `PROTO-03` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.c` | `src/srtp/srtp_session.c` | `9eebcc4` | `reference-only` | libsrtp key mapping 行为留给后续 Wave 3；Wave 1 未复制实现。 | `core` | `PROTO-03` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sctp/Sctp.h` | `src/sctp/sctp_session.h` | `9eebcc4` | `rewritten-derived` | 参考 usrsctp 生命周期和 association 边界，Wave 1 先提供全局 init/session 骨架。 | `core` | `PROTO-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sctp/Sctp.c` | `src/sctp/sctp_session.c` | `9eebcc4` | `reference-only` | usrsctp association 和 PPID 行为留给后续 Wave 4；Wave 1 未复制实现。 | `core` | `PROTO-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/DataChannel.h` | `src/data_channel/data_channel.h` | `9eebcc4` | `rewritten-derived` | 参考 DataChannel handle、label 和 callback ownership，重写为 MRTC opaque private 结构。 | `core` | `API-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/DataChannel.c` | `src/data_channel/data_channel.c` | `9eebcc4` | `reference-only` | DCEP open/send/close 行为留给后续 Wave 4；Wave 1 只建立对象生命周期骨架。 | `core` | `PROTO-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | `include/micrortc/peer_connection.h` | `9eebcc4` | `rewritten-derived` | Phase 4 参考 AWS ICE server、connection state 和 DataChannel public API 形态，继续裁剪为 MRTC 命名和 opaque handle。 | `core` | `API-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.h` | `src/sdp.h` | `9eebcc4` | `rewritten-derived` | 参考 DTLS setup、fingerprint、ICE attributes 与 SCTP/DataChannel SDP 属性边界，扩展 MRTC private SDP API。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.c` | `src/sdp.c` | `9eebcc4` | `rewritten-derived` | 参考 answer 生成和属性解析模型，重写为 Phase 4 ICE/fingerprint/setup/SCTP answer helper。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls_openssl.c` | `src/dtls/dtls_session.c` | `9eebcc4` | `rewritten-derived` | Phase 4 参考 OpenSSL fingerprint 格式和 DTLS role 边界，当前以 MRTC 私有 wrapper 生成本地 fingerprint 并供 SDP 使用。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/Crypto.c` | `src/dtls/dtls_session.c` | `9eebcc4` | `reference-only` | 参考 KVS crypto helper 的 keying material 边界；v1 目标仍是 OpenSSL 路径，当前环境缺 OpenSSL headers 时 wrapper 保持可替换合约。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/IOBuffer.h` | `src/dtls/dtls_session.h` | `9eebcc4` | `reference-only` | 参考 DTLS packet buffering 边界；Phase 4 未复制 IOBuffer 实现。 | `core` | `PROTO-02` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.c` | `src/srtp/srtp_session.c` | `9eebcc4` | `rewritten-derived` | 参考 libsrtp session/key direction 映射，重写为 MRTC SRTP wrapper；v1 依赖发现保留 system libsrtp 接入口。 | `core` | `PROTO-03` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sctp/Sctp.h` | `src/sctp/sctp_session.h` | `9eebcc4` | `rewritten-derived` | 参考 SCTP port、DCEP 和 PPID 边界，扩展 MRTC SCTP wrapper callback 合约。 | `core` | `PROTO-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sctp/Sctp.c` | `src/sctp/sctp_session.c` | `9eebcc4` | `rewritten-derived` | 参考 usrsctp association、DCEP open 和 string/binary PPID，重写为可替换的 MRTC SCTP session wrapper。 | `core` | `PROTO-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/DataChannel.h` | `include/micrortc/peer_connection.h` | `9eebcc4` | `rewritten-derived` | 参考 AWS DataChannel handle/init/callback API，公开为 MRTC create/send/close 和 message type。 | `core` | `API-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/DataChannel.c` | `src/peer_connection.c` | `9eebcc4` | `rewritten-derived` | 参考 createDataChannel、send、close 和 callback lifecycle，将 DataChannel 挂入 PeerConnection transport ready 路径。 | `core` | `PROTO-05` | `04` | `draft` |
| `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | `include/micrortc/peer_connection.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 AWS Frame、transceiver callbacks、addTransceiver 和 writeFrame 使用模型，改写为 MRTC media kind/codec/direction/frame API，不暴露 AWS/PIC 类型且不加入采集或编码入口。 | `core` | `API-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-05` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.h` | `src/media/media_transceiver.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS RTP transceiver 私有状态字段，重写为 MRTC private transceiver owner、MID、SSRC、payload type、sequence、callback 和 next-list 合约。 | `core` | `API-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-05` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.c` | `src/media/media_transceiver.c` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS transceiver 创建、callback setter 和 writeFrame 校验路径，当前仅落对象生命周期与 media transport 未就绪的确定性状态。 | `core` | `API-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-05` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.h` | `src/sdp.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS H264/Opus SDP helper 边界，扩展 private SDP API 以接收 transceiver list 生成媒体 offer/answer。 | `core` | `API-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-05` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.c` | `src/sdp.c` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS H264/Opus rtpmap、fmtp、rtcp-fb、direction、mid、SSRC 和 DataChannel m-line 语义，重写为 deterministic MRTC SDP 输出。 | `core` | `API-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-05` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/RtpPacket.h` | `src/rtp/rtp_packet.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS RTP v2 header、payload/raw packet 边界和 sequence rollover 语义，重写为 MRTC private RTP packet helper。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/RtpPacket.c` | `src/rtp/rtp_packet.c` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS RTP parse/serialize/build 行为，改写为不依赖 AWS 宏的 C99 network-byte-order helper。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpH264Payloader.h` | `src/rtp/codecs/h264.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS H264 Annex-B packetizer/depacketizer API 边界，重写为 MRTC private H264 helper。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpH264Payloader.c` | `src/rtp/codecs/h264.c` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS Annex-B start code scanning、Single NALU、FU-A 和 Annex-B depay 输出，改写为 MRTC C99 实现。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpOpusPayloader.h` | `src/rtp/codecs/opus.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS Opus direct payload/depayload 边界，新增 MRTC_FRAME 100ns 到 48kHz RTP timestamp helper。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpOpusPayloader.c` | `src/rtp/codecs/opus.c` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS Opus 直拷贝语义，改写为不包含采集、编码或容器解析的 private codec primitive。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtcpPacket.h` | `src/rtcp/rtcp_packet.h` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS RTCP header、SR/RR、NACK 和 PLI packet 类型边界，重写为 MRTC private RTCP helper。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtcpPacket.c` | `src/rtcp/rtcp_packet.c` | `9eebcc4` | `rewritten-derived` | Phase 5 参考 KVS RTCP parse 和 NACK sequence extraction 行为，补充 SR/RR/PLI generation basics。 | `core` | `PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.c` | `src/srtp/srtp_session.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 03 参考 KVS RTP/RTCP protect/unprotect 与 inbound/outbound SRTP session 分离，扩展 MRTC wrapper；无 libsrtp 时保留可测试 passthrough fallback。 | `core` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.h` | `src/srtp/srtp_session.h` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 03 参考 KVS private SRTP session contract，新增 RTP/RTCP protect/unprotect 私有入口和 DTLS key/salt 方向保存。 | `core` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.c` | `src/peer_connection.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 03 参考 KVS writeFrame payloadize -> RTP -> SRTP protect -> transport send 顺序，接入 MRTC transceiver write_frame 和 private media send hook。 | `core` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.c` | `tests/media/test_media_send.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 03 以 KVS send path 为行为参考，测试 H264/Opus RTP packet count、payload type、SSRC、sequence 和 SRTP hook 可观察性。 | `core` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/PeerConnection.c` | `src/peer_connection.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 03 参考 KVS inbound demux、SRTP unprotect、RTP receiver routing 和 onFrameReady callback，重写为 MRTC private protected-RTP receive entry。 | `core` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/JitterBuffer.c` | `src/peer_connection.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 03 参考 KVS jitter buffer frame-ready/depay ordering；当前实现采用最小顺序接收和 H264 FU-A accumulation，不搬迁完整 jitter buffer。 | `supporting/reference-only` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtcp.c` | `src/srtp/srtp_session.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 03 参考 KVS RTCP protect/unprotect 调用边界，当前计划仅扩展 SRTP wrapper RTCP packet API，RTCP 编排留给后续计划。 | `supporting/reference-only` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtcpPacket.h` | `src/rtcp/rtcp_packet.h` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 继续参考 KVS RTCP packet 类型、SR/RR/NACK/PLI 字段布局和反馈格式，补齐行为层接入。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtcpPacket.c` | `src/rtcp/rtcp_packet.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 复用既有 MRTC RTCP primitive 作为 NACK/PLI/SR/RR 行为入口，不引入 REMB/TWCC/FIR/SLI。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RollingBuffer.h` | `src/rtcp/rtp_rolling_buffer.h` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS fixed-capacity rolling buffer 思路，重写为按 RTP sequence number 保存 protected packet bytes 的 MRTC 私有 API。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RollingBuffer.c` | `src/rtcp/rtp_rolling_buffer.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS overwrite/free lifecycle，重写为无 AWS 宏依赖的 C99 packet byte store。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtpRollingBuffer.h` | `src/rtcp/rtp_rolling_buffer.h` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS RTP sequence lookup 语义，目标实现改为直接 lookup/copy/remove stored packet bytes。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtpRollingBuffer.c` | `src/rtcp/rtp_rolling_buffer.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS NACK 可重发包缓存，当前选择保存已 protected RTP bytes 并由测试覆盖 overwrite/missing。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Retransmitter.h` | `src/rtcp/retransmitter.h` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS NACK sequence list 到 sender packet buffer 的编排，重写为 MRTC retransmit result counters。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Retransmitter.c` | `src/rtcp/retransmitter.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS resendPacketOnNack 行为，改为通过 private media send hook 重发 protected packet bytes，未知 SSRC/缺包为确定性状态。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtcp.c` | `src/peer_connection.c` | `9eebcc4` | `rewritten-derived` | Phase 5 Plan 04 参考 KVS PLI callback、NACK retransmit、SR/RR parse handling，裁剪为 MRTC protected RTCP ingress，不实现 REMB/TWCC/FIR/SLI/拥塞控制。 | `core` | `PROTO-04, API-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/samples/h264SampleFrames/` | `tests/fixtures/h264_annexb_sample.h264` | `9eebcc4` | `reference-only` | Phase 5 Plan 05 参考本地 KVS Annex-B H264 sample frame 形态，新增固定最小 SPS/PPS/IDR bytestream fixture；不复制浏览器 E2E 或编码器逻辑。 | `core` | `MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-07` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpOpusPayloader.c` | `tests/fixtures/opus_packets.bin` | `9eebcc4` | `reference-only` | Phase 5 Plan 05 参考 KVS Opus direct packet payload 语义，新增 big-endian 16-bit length-prefixed Opus packet fixture；不引入 Ogg/MP4/container parser。 | `core` | `MEDIA-03, MEDIA-04, MEDIA-06, MEDIA-07` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/samples/h264SampleFrames/` | `tests/media/test_media_fixtures.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 05 以 KVS H264 sample 目录为 Annex-B fixture 参考，测试固定 fixture 含 SPS/PPS before IDR 并可被 H264 packetizer 读取。 | `core` | `MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-07` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpOpusPayloader.c` | `tests/media/test_media_fixtures.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 05 以 KVS Opus payloader direct payload 行为为参考，测试 length-prefixed fixture 至少含两个非空 Opus packet。 | `core` | `MEDIA-03, MEDIA-04, MEDIA-06, MEDIA-07` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.c` | `tests/integration/mrtc_phase5_media_verify.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 05 参考 KVS writeFrame 到 RTP/SRTP/send 和 receive onFrame 行为，新增 C fixture verifier 覆盖 H264/Opus send/receive 和 SRTP media path。 | `core` | `API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06, MEDIA-07` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtcp.c` | `tests/integration/mrtc_phase5_media_verify.c` | `9eebcc4` | `reference-only` | Phase 5 Plan 05 参考 KVS NACK/PLI 行为入口，verifier 断言 RTCP NACK retransmit 和 PLI callback；完整 Chrome 自动化仍在 Phase 6。 | `core` | `PROTO-04, MEDIA-04` | `05` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Stun/Stun.c` | `src/stun/stun_message.c` | `9eebcc4` | `rewritten-derived` | Phase 6 Plan 07 参考 STUN Binding request/response、XOR-MAPPED-ADDRESS、MESSAGE-INTEGRITY 和 FINGERPRINT 边界，重写为 MRTC host-lite ICE 所需 helper；未引入 KVS signaling。 | `core` | `E2E-03, E2E-04, TEST-02` | `06` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Ice/IceAgent.c` | `src/ice/ice_agent.c` | `9eebcc4` | `rewritten-derived` | Phase 6 Plan 07 参考 ICE host candidate 和 connectivity-check 处理边界，扩展 MRTC host endpoint 为可轮询 UDP/STUN path；当前仍未完成 Chrome DTLS/SCTP 互通。 | `core` | `E2E-03, E2E-04, TEST-02` | `06` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls_openssl.c` | `src/dtls/dtls_session.c` | `9eebcc4` | `rewritten-derived` | Phase 6 Plan 07 参考 OpenSSL certificate fingerprint 语义，将本地 fingerprint 改为 self-signed certificate SHA-256 digest；packet BIO handshake 尚未完成。 | `core` | `E2E-03, E2E-04, TEST-02` | `06` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/Sctp/Sctp.c` | `src/sctp/sctp_session.c` | `9eebcc4` | `rewritten-derived` | Phase 6 Plan 07 参考 usrsctp/DCEP 状态边界，移除 connect-time in-process open/message 假阳性；系统 usrsctp 缺失时严格 transport 仍阻塞。 | `core` | `E2E-04, E2E-05, TEST-02` | `06` | `draft` |
| `reflib/kvs-webrtc-sdk/src/source/PeerConnection/DataChannel.c` | `src/peer_connection.c` | `9eebcc4` | `rewritten-derived` | Phase 6 Plan 07 参考 DataChannel 只能由真实 transport/DCEP 驱动 open/message 的边界，停止把 addIceCandidate 或本地 send loopback 当作 Chrome 互通成功。 | `core` | `E2E-04, E2E-05, TEST-04` | `06` | `draft` |

## 后续记录模板

| source_path | target_path | origin_commit | derivation | notes | module_class | requirement | phase_added | review_status |
|-------------|-------------|---------------|------------|-------|--------------|-------------|-------------|---------------|
| `reflib/kvs-webrtc-sdk/src/source/Ice/SomeFile.c` | `src/ice/some_file.c` | `9eebcc4` | `trimmed` | 删除 AWS/KVS 耦合 include，保留 ICE 行为；实际路径以搬迁 PR 为准。 | `core` | `PROTO-01` | `02` | `draft` |

## 审阅清单

- [ ] 每个实际派生文件都有 `source_path`、`target_path`、`origin_commit`、`derivation` 和 `notes`。
- [ ] `origin_commit` 与当前基线一致，除非另有显式基线更新记录。
- [ ] `Signaling` 保持 `excluded`，不被核心库 target 间接依赖。
- [ ] `reference-only` 条目没有把来源代码直接复制进目标文件。
- [ ] 表格只记录实际派生、参考或排除决策，不变成全量 reflib 文件列表。
