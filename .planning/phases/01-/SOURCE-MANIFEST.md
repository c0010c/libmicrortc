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
