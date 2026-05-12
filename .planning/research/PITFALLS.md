# Pitfalls Research

**Domain:** 纯 C、Sans-I/O、资源受限设备上的 WebRTC 媒体传输库  
**Researched:** 2026-05-12  
**Confidence:** HIGH  

## Critical Pitfalls

### Pitfall 1: 把 Chrome 互通误判成“SDP 能解析、ICE connected 就完成”

**What goes wrong:**
Demo 中能连上，但 Chrome 侧无画面、几秒后断流、`chrome://webrtc-internals` 只显示 transport 成功而 RTP/RTCP 指标异常。常见根因是只满足了连接层，遗漏 BUNDLE、rtcp-mux、MID/SSRC demux、动态 payload type、RTCP feedback、H264 fmtp、DTLS role/fingerprint 等组合约束。

**Why it happens:**
WebRTC 互通是多协议组合问题。RFC 8834 要求 WebRTC endpoint 实现 RTP 且使用 RTCP；RFC 8843 要求 bundled RTP media 包含并启用 `rtcp-mux`；Chromium 文档还指出 payload type 没有稳定性保证，BUNDLE 下需要唯一可识别。

**How to avoid:**
第一阶段就建立“Chrome 互通矩阵”，不要等媒体链路完成后再补。矩阵至少覆盖：Chrome offer -> lib answer、lib offer -> Chrome answer、音频 only、视频 only、音视频 BUNDLE、rtcp-mux、trickle/non-trickle、relay-only、IPv4/IPv6、ICE restart、DTLS active/passive。每条互通用例必须导出 Chrome stats 和库内 trace 双份证据。

**Warning signs:**
- 只有 ICE/DTLS 状态，没有 per-SSRC RTP/RTCP 统计。
- SDP answer 固定 payload type 或忽略 `mid`/`rtcp-mux`/`rtcp-fb`。
- Chrome 侧 selected candidate pair 正常，但 `framesDecoded`、`packetsReceived`、`nackCount`、`pliCount` 不符合预期。
- 通过本地 loopback 测试后才开始接 Chrome。

**Phase to address:**
Phase 0 “互通契约与测试基线”必须定义矩阵和验收指标；Phase 4 “Chrome 端到端互通”只负责扩展覆盖，不应首次发现协议契约。

**Confidence:** HIGH

---

### Pitfall 2: SDP/H264 fmtp 处理过窄，导致 Chrome 接受 SDP 但无法稳定解码

**What goes wrong:**
Chrome 接受 answer，但首帧黑屏、关键帧后才偶尔恢复、分辨率/码率变化后花屏，或者 profile/level 协商看似成功但远端丢弃视频。最危险的是把 H264 `profile-level-id`、`packetization-mode`、SPS/PPS 的 in-band 规则当成字符串透传。

**Why it happens:**
WebRTC H264 不是任意 Annex B/AVCC 都能直接发。RFC 7742 要求 H264 支持 RFC 6184 payload format，必须支持 `packetization-mode=1`，SDP 必须包含并解释 `profile-level-id`，并且 WebRTC 生成的 SDP 不应包含 `sprop-parameter-sets`，因为 SPS/PPS 必须 in-band。MDN 的 WebRTC codec 文档也确认浏览器端 AVC/H264 互通依赖 Constrained Baseline、mode 1、in-band parameter sets。

**How to avoid:**
实现一个显式 H264 capability/fmtp 模块，而不是散落字符串逻辑。v1 推荐只声明和接受 H264 Constrained Baseline 可兼容子集，明确支持 `packetization-mode=1`，解析并校验 `profile-level-id`，发送侧保证关键帧前或 IDR AU 中带 SPS/PPS，接收侧缓存并随 AU 回调上交完整参数集状态。拒绝不支持的 packetization mode，不静默降级。

**Warning signs:**
- `a=fmtp` 只是原样复制，没有结构化解析和单元测试。
- packetizer 只支持单 NAL 或只支持 FU-A，没有 STAP-A/聚合策略边界说明。
- 没有 Annex B start code 与 AVCC length-prefixed 输入格式的 API 区分。
- Chrome 解码统计中 `keyFramesDecoded` 增长但 `framesDecoded` 卡住，或 PLI 频繁增长。

**Phase to address:**
Phase 1 “SDP/RTP/H264/Opus 媒体核心”必须完成结构化 SDP/H264 fmtp、packetization/depacketization 和 Chrome SDP 样本回归；Phase 4 再做真实浏览器互通。

**Confidence:** HIGH

---

### Pitfall 3: H264 packetization 与固定内存模型冲突

**What goes wrong:**
大 IDR 帧、SPS/PPS 聚合、FU-A 重组或乱序包导致 arena 爆掉；为了跑通测试临时 malloc，破坏“初始化后固定资源上限”；或者接收侧等待完整帧时无限缓存，丢包后无法释放。

**Why it happens:**
H264 AU 大小不可由 WebRTC 传输层完全控制，而本项目又不做编解码和拥塞控制。RFC 8835 明确指出媒体发送不适合深队列，更应丢弃无依赖的中间帧来降低码率/延迟。若 packetizer/reassembler 没有硬上限，固定内存承诺会在第一条真实视频流上失效。

**How to avoid:**
在 API 和配置中显式声明 `max_encoded_au_bytes`、`max_fu_reassembly_bytes`、`max_packets_per_au`、`max_pending_aus`。发送侧超限返回可诊断错误，不拆成不可控队列。接收侧按 SSRC/PT/timestamp 建 AU 重组槽，乱序窗口和超时可配置；缺片 AU 到期必须丢弃并上报 `RTC_ERR_FRAME_INCOMPLETE`，必要时触发 PLI。

**Warning signs:**
- `rtc_send_h264()` 接受任意大小 buffer。
- 重组表按 packet 动态增长。
- 没有“半帧占用水位”和“因内存预算丢弃帧”统计。
- FU-A 丢失中间片后，后续相同 timestamp 的片段一直留在缓存中。

**Phase to address:**
Phase 0 定义内存预算模型；Phase 1 在 H264 RTP 模块中落实硬上限和错误码；Phase 5 多 PeerConnection 前做压力验证。

**Confidence:** HIGH

---

### Pitfall 4: ICE full 实现只覆盖 happy path，真实公网 NAT 下失败

**What goes wrong:**
局域网互通正常，跨运营商、企业网络、蜂窝网络或双 NAT 下连接失败；candidate pair 长时间 `in-progress`/`waiting`；relay candidate 能 gather 但无法被选中；ICE restart 后旧候选或旧 ufrag 导致误路由。

**Why it happens:**
ICE full 要维护 checklist、foundation、priority、角色冲突、triggered checks、nomination、peer reflexive candidate、trickle candidate、ICE restart 和定时器节奏。RFC 8445 建议默认 Ta 约 50ms，并对 checklist/nomination/triggered checks 有严格状态语义。只用“发 STUN binding 成功就 connected”的模型无法覆盖真实 NAT。

**How to avoid:**
把 ICE 作为独立 Sans-I/O 子系统实现，有完整状态表、定时器驱动、事务表和事件 trace。v1 支持 regular nomination；controlling/controlled role、tie-breaker、USE-CANDIDATE、peer reflexive candidate 和 ICE restart 必须有协议测试。真实网络测试至少包含：host-host、srflx-srflx、relay-relay、relay-srflx、双 NAT、候选延迟到达、候选重复、网络切换后 restart。

**Warning signs:**
- ICE 状态只有 `new/checking/connected/failed`，没有 candidate pair 级状态和失败原因。
- 无法输出 checklist、selected pair、nomination 时间线。
- 代码假设候选在 SDP 中一次性到齐。
- ICE restart 后旧 STUN transaction 或旧 ufrag 仍能影响新 transport。

**Phase to address:**
Phase 2 “ICE/STUN/TURN 与公网 NAT”必须完整处理；Phase 4 只验证 Chrome 互通，不应再补核心 ICE 状态机。

**Confidence:** HIGH

---

### Pitfall 5: TURN 边界处理不完整，relay-only 和企业网络场景不可用

**What goes wrong:**
TURN allocation 成功但媒体不通；5 或 10 分钟后突然断流；relay candidate 只在低流量测试中有效；ChannelData 与 STUN/TURN/Data Indication demux 混淆；服务器 nonce 刷新或 permission 过期后未恢复。

**Why it happens:**
TURN 不是“拿到 relay 地址”就结束。RFC 8656 规定 allocation 默认由 Allocate/Refresh 维护，发送数据不会刷新 allocation；permission 有独立生命周期，TURN channel 是带 4 字节前缀的优化路径。RFC 7983 还特别修正了 DTLS-SRTP 同 socket 下 TURN Channel demux 范围。

**How to avoid:**
TURN client 必须显式建模 allocation、permission、channel binding 和 refresh timer。v1 若只支持 UDP relay，要清楚拒绝 TURN/TCP/TLS URL 并在错误码中区分“协议未支持”和“鉴权/网络失败”。同一个 packet demux 必须按 RFC 7983 处理 STUN、DTLS、TURN Channel、RTP/RTCP。relay-only 模式应成为 Chrome 互通必测路径。

**Warning signs:**
- 只实现 Allocate，没有 Refresh/CreatePermission/ChannelBind 或权限刷新。
- TURN 成功日志只有 relayed address，没有 allocation lifetime、permission lifetime、nonce/realm。
- 运行数分钟后 relay 流量停止，但 ICE state 没有及时失败。
- demux 代码把首字节 64..79 当作 RTP 或未知包丢弃。

**Phase to address:**
Phase 2 必须覆盖 TURN UDP relay 全生命周期；Phase 4 加入 Chrome `iceTransportPolicy: "relay"` 验收；TURN/TCP/TLS 放入后续研究，不能暗示已支持。

**Confidence:** HIGH

---

### Pitfall 6: 忽略 consent freshness，变成“连接还在但不能合法发送”

**What goes wrong:**
网络切换、NAT 映射变化或远端关闭后，本端继续发 RTP，造成黑洞流量、误判连接健康，甚至违反 WebRTC consent 语义。Chrome 侧可能显示 consent failed 或 candidate pair cancelled。

**Why it happens:**
ICE connectivity check 只获得初始 consent。RFC 7675 要求持续用 STUN binding request/response 维护 consent；若 30 秒内没有收到有效响应，endpoint 必须停止在该 5-tuple 上发送。

**How to avoid:**
selected candidate pair 进入可发送状态后，启动 consent timer，使用当前 ICE short-term credentials 发 authenticated STUN binding request。把 consent expiry 与 ICE state、发送门控、错误上报绑定：过期后停止 RTP/RTCP/SRTP 输出，触发连接失败或 ICE restart 建议事件。

**Warning signs:**
- ICE connected 后不再发送 STUN binding request。
- RTP 发送路径不检查 selected pair consent 状态。
- 只有 socket send 成功指标，没有 consent request/response/expiry 指标。
- 断网后仍持续消耗上行带宽。

**Phase to address:**
Phase 2 和 ICE selected pair 同时实现，不应推迟到生产硬化阶段。

**Confidence:** HIGH

---

### Pitfall 7: DTLS-SRTP key export、role、fingerprint 或 profile 搞错

**What goes wrong:**
DTLS handshake 成功但 SRTP 解密失败；Chrome 侧报 `srtp unprotect failed`、无媒体；或安全上接受了错误证书指纹，造成 DTLS-SRTP 认证绕过风险。

**Why it happens:**
WebRTC 的 DTLS 证书不是 CA 信任链模型，而是必须和 SDP fingerprint 绑定。JSEP 规定 answer 中 DTLS setup role 必须 active/passive，面对 JSEP offer 的 `actpass`，answerer 通常应使用 `active`。RFC 5764 通过 `use_srtp` 扩展协商 SRTP protection profile，并从 DTLS master secret 派生 client/server SRTP master key 和 salt。方向、label、profile、key/salt 顺序任何一个错都会表现为 SRTP auth failure。

**How to avoid:**
DTLS vtable 不能只暴露“握手成功”。必须暴露：本地/远端 role、selected SRTP profile、remote certificate fingerprint 校验结果、exported keying material、DTLS retransmit timer、alert/error。与 libsrtp 初始化之间加一层 `dtls_srtp_material` 结构，单测覆盖 client/server 方向 key 分配。拒绝 fingerprint 不匹配，且让错误可观察。

**Warning signs:**
- DTLS 完成后立即创建同一把 inbound/outbound SRTP key。
- SDP fingerprint 只解析不验证。
- `setup:actpass`、`active`、`passive` 被当成普通字符串存储，没有驱动 DTLS client/server role。
- libsrtp `auth fail` 被合并成普通丢包计数。

**Phase to address:**
Phase 3 “DTLS-SRTP/libsrtp 集成”必须完成；Phase 0 应先定义 vtable 合同和错误码。

**Confidence:** HIGH

---

### Pitfall 8: libsrtp buffer、SSRC、ROC/replay 语义误用

**What goes wrong:**
SRTP 偶发认证失败、内存越界、SRTCP 失败、乱序后永久解密失败，或者多个 SSRC/PeerConnection 共享状态互相污染。

**Why it happens:**
libsrtp 文档明确 `srtp_protect()` 假设 RTP buffer 尾部有足够空间写认证 tag，否则会内存破坏。SRTP stream 状态包含 SSRC、sequence number、rollover counter、anti-replay 数据；每个 source 需要独立 stream context。RFC 3711 也说明 SRTP 使用隐式 packet index/ROC 并推荐 RTP/RTCP replay protection。

**How to avoid:**
SRTP adapter 必须拥有 packet buffer contract：每个 RTP/RTCP buffer 有明确 headroom/tailroom，protect 前检查容量。按 direction 和 SSRC 管理 libsrtp policy，不跨 PeerConnection 共享 session。对 inbound unknown SSRC、ROC jump、replay rejection、auth failure、buffer too small 分开计数。SRTCP 使用独立 protect/unprotect 路径。

**Warning signs:**
- RTP packet buffer 刚好等于明文长度，没有 SRTP tag tailroom。
- 使用 `SSRC_ANY_*` 后没有在多 SSRC 或多 PC 中跟踪实际绑定。
- RTP 和 RTCP 共用同一错误码路径。
- 高丢包/乱序测试中 auth failure 随后持续增长。

**Phase to address:**
Phase 3 集成 libsrtp 前必须完成 buffer ABI 和 SRTP state model；Phase 5 多 PeerConnection 前做 SSRC/session 隔离测试。

**Confidence:** HIGH

---

### Pitfall 9: RTCP 被当成可选附属品，导致媒体质量和恢复能力不足

**What goes wrong:**
音视频能出首帧，但丢包后无法恢复；Chrome 不发或不响应 PLI/NACK；统计缺失 RTT/loss/jitter；关键帧请求不触达应用层编码器；没有拥塞控制时更难判断何时该降码率或丢帧。

**Why it happens:**
RFC 8834 明确 RTCP 是 RTP 的基础组成部分，WebRTC endpoint 必须实现和使用；还要求支持 non-compound RTCP feedback，以便频繁反馈。即使 v1 不做拥塞控制，NACK/PLI/FIR、SR/RR、SDES CNAME/MID、BYE、reduced-size RTCP 仍是 Chrome 互通和可观测性的基础。

**How to avoid:**
把 RTCP 作为 Phase 1 核心，不是 Phase 4 修补。最小集合：SR、RR、SDES CNAME、BYE、RTPFB NACK、PSFB PLI/FIR、reduced-size RTCP、RTCP mux、SRTCP。库不编码时，PLI/FIR 必须回调应用请求关键帧；NACK 可先只统计或提供重传缓存扩展点，但不能吞掉。

**Warning signs:**
- 只实现 RTP，RTCP parser 返回 TODO。
- SDP 里声明了 `rtcp-fb`，但收包后无事件。
- Chrome stats 中 `pliCount`/`nackCount` 增长，应用层没有关键帧请求回调。
- RTT 只能从 ICE/STUN 推测，没有 SR/RR 关联指标。

**Phase to address:**
Phase 1 完成 RTCP 基础；Phase 4 验证 Chrome 丢包恢复和 PLI/NACK 事件；拥塞控制扩展留给后续。

**Confidence:** HIGH

---

### Pitfall 10: 单线程无锁状态机没有事件序列契约，后期被回调重入打穿

**What goes wrong:**
看似单线程无锁，但应用回调中调用库 API 造成 reentrancy；定时器、网络输入和用户发送交错导致状态反转；多 PeerConnection 共享 context 时一个 PC 的回调释放资源，另一个 PC 仍持有指针。

**Why it happens:**
Sans-I/O 消除了平台 I/O 依赖，但没有自动消除事件序列复杂度。WebRTC transport 的状态来自网络包、定时器、应用调用、DTLS/SRTP vtable 回调和媒体回调。若没有 run-to-completion 和 deferred action 规则，无锁实现会比加锁实现更脆弱。

**How to avoid:**
定义 `rtc_context_step()` 的 run-to-completion 语义：库内处理事件时禁止直接重入状态机；用户回调只允许查询和投递 command，真正状态迁移在下一轮 drain。所有对象使用 generation/id 验证，避免 callback 中 destroy 后悬挂。状态机转移表、非法转移计数和 trace 必须内建。

**Warning signs:**
- 回调文档没有说明是否允许调用 `rtc_*` API。
- 状态变量在多个函数中随意赋值，没有中心 transition 函数。
- 定时器回调直接释放 PeerConnection。
- 测试只有线性脚本，没有乱序事件/fuzz。

**Phase to address:**
Phase 0 必须确定 Sans-I/O 事件模型和回调重入规则；Phase 2/3/5 每个协议状态机都按同一模型实现。

**Confidence:** MEDIUM-HIGH

---

### Pitfall 11: 多 PeerConnection 共享 context 时资源与调度不公平

**What goes wrong:**
一个高码率视频 PC 占满发送队列、arena 或定时器预算，导致其他 PC ICE 超时、RTCP 延迟、音频卡顿。mesh 场景中线性扫描所有 PC，连接数上来后 tick 延迟扩大。

**Why it happens:**
项目要求单个 `rtc_context` 单线程驱动多个 PeerConnection。RFC 8835 讨论了同一拥塞控制 regime 下多流调度和优先级，且指出媒体不适合深队列。即便 v1 不实现拥塞控制，仍需要公平调度、预算隔离和 backpressure。

**How to avoid:**
资源模型按 context 和 PeerConnection 两级预算：全局 arena 上限、每 PC packet/RTCP/ICE transaction 上限、每 tick 最大处理包数、音频/控制优先级。发送调度采用显式队列和 bounded round-robin，控制包、ICE consent、RTCP 不被视频帧饿死。多 PC 压测作为 v1 验收，不等 mesh 功能完整后再补。

**Warning signs:**
- 所有 PC 共用一个无限发送队列。
- 每次 tick 全量扫描所有 candidate pair/rtp streams，没有 deadline 或 work budget。
- 无 per-PC 内存水位和丢弃原因。
- 两路以上视频时 ICE consent 或 RTCP report 延迟明显增加。

**Phase to address:**
Phase 0 定义预算；Phase 5 “多 PeerConnection 与调度”实现和压测；Phase 2/3 的定时器设计要为多 PC 预留。

**Confidence:** MEDIUM-HIGH

---

### Pitfall 12: 固定内存只约束自研代码，第三方依赖仍然动态分配

**What goes wrong:**
项目 API 声称固定内存，但 mbedTLS DTLS handshake、certificate parse、libsrtp session、日志缓冲或测试路径仍在运行时 malloc；资源受限设备上表现为偶发握手失败或内存碎片。

**Why it happens:**
mbedTLS 文档显示 DTLS handshake reassembly/future message buffering 有独立堆分配预算，TLS I/O buffer 也有 `MBEDTLS_SSL_IN_CONTENT_LEN`/`OUT_CONTENT_LEN` 等配置。libsrtp 创建 session/stream/policy 也有内部状态。若 vtable 不显式约束 allocator 和最大对象数，固定内存预算只覆盖了 Sans-I/O 核心的一部分。

**How to avoid:**
Phase 0 做“全栈内存账本”：核心对象、packet pools、ICE transaction、TURN allocation、DTLS contexts、mbedTLS buffers、libsrtp sessions、日志 ring buffer 都必须有预算项。默认适配层应支持接入用户 allocator 或静态 arena；无法固定的依赖分配必须在初始化/连接建立前完成，并在文档中标注峰值。

**Warning signs:**
- 只统计自研 arena，不统计 mbedTLS/libsrtp。
- DTLS handshake 时才首次申请大 buffer。
- `RTC_ERR_NOMEM` 没有携带预算域和水位。
- 单 PC 通过，多 PC 握手并发时内存峰值不可预测。

**Phase to address:**
Phase 0 必须完成预算分类和 allocator/vtable contract；Phase 3 集成 mbedTLS/libsrtp 时验证峰值；Phase 5 做并发握手水位测试。

**Confidence:** HIGH

---

### Pitfall 13: RTP/RTCP/DTLS/STUN/TURN 同 socket demux 顺序错误

**What goes wrong:**
STUN consent 包被送进 DTLS，TURN ChannelData 被当 RTP，RTCP feedback 被当 RTP payload，导致间歇性连接失败或媒体异常。问题通常只在 relay、rtcp-mux、BUNDLE、ICE restart 或 Chrome 动态 payload type 组合下出现。

**Why it happens:**
WebRTC 在同一 5-tuple 上 multiplex STUN/TURN、DTLS、RTP、RTCP。RFC 7983 更新了 RFC 5764 的 demux 规则：首字节 0..3 是 STUN，20..63 是 DTLS，64..79 是 TURN Channel，128..191 是 RTP/RTCP，其他应丢弃并可记录。RFC 5761 又要求 RTP payload type 避免 64..95，以免与 RTCP packet type 冲突。

**How to avoid:**
实现一个独立 packet classifier，所有网络输入先经过 classifier，再分发到 ICE/TURN/DTLS/SRTP/RTCP/RTP。classifier 单测覆盖所有边界首字节、短包、伪造包、SRTP/SRTCP、TURN ChannelData。SDP payload type allocator/validator 禁用 64..95，并在 BUNDLE 下检查 PT 唯一性。

**Warning signs:**
- demux 代码分散在 socket adapter、ICE 和 RTP 模块。
- RTP 判断只检查版本位 `0x80`。
- 允许动态 payload type 72、73、77、78、79。
- relay 模式下才出现不可解释的 auth failure 或 unknown packet。

**Phase to address:**
Phase 1 建 RTP/RTCP mux 和 payload type 规则；Phase 2 加 STUN/TURN；Phase 3 加 DTLS/SRTP 后做全组合 classifier 回归。

**Confidence:** HIGH

---

### Pitfall 14: 可观测性后补，导致互通和公网问题无法定位

**What goes wrong:**
真实用户报告“连不上/黑屏/卡顿”，库只能输出一个 failed 状态。无法判断是 SDP、ICE candidate、TURN auth、consent、DTLS fingerprint、SRTP auth、H264 SPS/PPS、RTCP feedback、内存预算还是应用未给关键帧。

**Why it happens:**
WebRTC 故障横跨多个层级。W3C WebRTC Stats 定义了 candidate-pair、transport、inbound/outbound RTP、remote RTP 等统计，Chrome 也依赖这些视角定位问题。自研库如果没有同构统计和 packet-level trace，和 Chrome 对照时会失去时间线。

**How to avoid:**
Phase 0 就定义 stats schema 和 trace taxonomy。至少包括：SDP parse/answer decisions、candidate gathering/check/nomination、selected pair、STUN transaction RTT、TURN allocation/permission/channel lifetimes、consent、DTLS role/profile/fingerprint、SRTP protect/unprotect failure reason、RTP seq/timestamp/jitter/loss、RTCP SR/RR/NACK/PLI/FIR、H264 AU reassembly、arena 水位和丢弃原因。trace 必须可按 PC/SSRC/candidate pair 过滤。

**Warning signs:**
- 日志只有字符串，没有结构化 event id 和对象 id。
- 错误码不带协议层和子原因。
- 无法把 Chrome `getStats()` 的 candidate pair id/SSRC/PT 和库内对象对应起来。
- release build 默认无法打开低开销 ring trace。

**Phase to address:**
Phase 0 必须先建观测骨架；所有后续 phase 的完成标准都应包含 stats/trace 字段。

**Confidence:** HIGH

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| SDP 用字符串拼接/正则处理 | 快速生成 Chrome 能读的 answer | fmtp、BUNDLE、ICE restart、payload type、方向属性很快失控 | 仅允许测试 fixture；生产路径必须结构化 AST/模型 |
| H264 只测小帧 single NAL | packetizer 快速可见画面 | IDR/FU-A/STAP-A/SPS/PPS 和内存上限问题后期集中爆发 | 不可作为 MVP 完成标准 |
| ICE 只实现 host/srflx happy path | 局域网 demo 快 | 公网 NAT、relay-only、企业网络不可用 | 仅用于最早期协议 spike |
| TURN 只 Allocate 不 Refresh/Permission | relay candidate 可出现 | 数分钟后断流，难定位 | never |
| DTLS vtable 只暴露 encrypted I/O | 抽象简单 | 无法安全完成 SRTP profile/key/fingerprint/role 集成 | never |
| RTCP 延后 | RTP demo 更快 | 无法恢复丢包、无法请求关键帧、无质量统计 | never，最小 RTCP 必须进媒体核心 |
| 临时 malloc 绕过 arena | 避免早期预算设计 | 固定内存承诺失效，嵌入式不可预测 | 仅限测试工具，不进入库 |
| 单线程下允许回调重入 | API 看起来方便 | 状态机难以证明，destroy/reentrant bug 难查 | never，改用 deferred command |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Chrome SDP | 固定 payload type、忽略 BUNDLE/rtcp-mux/MID | 解析远端 SDP，动态建立 PT/MID/SSRC 映射，并校验 BUNDLE 下唯一性 |
| Chrome H264 | SDP 放 `sprop-parameter-sets` 或不发 in-band SPS/PPS | WebRTC SDP 不生成 `sprop-parameter-sets`；关键帧路径携带 SPS/PPS |
| Chrome Opus | 把采样率当实际输入采样率协商 | SDP `opus/48000/2` 是 RTP signaling 要求；应用层采样率/声道通过编解码器处理 |
| mbedTLS DTLS | 忽略 DTLS retransmit timer 和 buffer 峰值 | vtable 暴露 timer、WANT_READ/WANT_WRITE、profile、key export、allocator/预算 |
| libsrtp | 明文 buffer 无 tailroom 调 `srtp_protect()` | packet pool 固定预留 SRTP/SRTCP tag tailroom，protect 前检查 |
| TURN server | 只保存 relay address | 保存 allocation lifetime、permission lifetime、nonce/realm、channel binding 状态并刷新 |
| Chrome stats 对照 | 只看 ICE connected | 对照 candidate pair、transport、inbound/outbound RTP、RTCP feedback、selected pair RTT |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| 深媒体队列 | 延迟不断增长，旧帧还在发送 | bounded queue，丢弃过期非关键帧，暴露 backpressure | 一个高码率视频流在弱网下即可触发 |
| 每 tick 全量扫描所有对象 | 多 PC 后 CPU 抖动、ICE/RTCP timer 延迟 | timer wheel/min-heap + per tick work budget | 3-5 个 PC 或大量 candidate pair |
| 每包 malloc/free | 抖动、碎片、资源受限设备偶发失败 | 固定 packet pool 和 slab/arena | 高 pps 音视频或 NACK/RTCP burst |
| 无 packet pacing | UDP burst、丢包、Chrome NACK/PLI 增长 | 发送预算、简单 pacing hook，为未来拥塞控制预留 | 大 IDR 帧、relay 路径、蜂窝网络 |
| TURN Send Indication 全程发送 | relay 带宽开销和 CPU 增加 | 需要时使用 ChannelBind/ChannelData，并正确 demux | relay-only 长通话 |
| 日志同步输出 | 弱设备卡顿、时序改变 | ring buffer + 采样 + 二进制/结构化 trace | 高丢包诊断开启时 |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| 不校验 SDP fingerprint 与 DTLS 证书 | DTLS-SRTP 认证绕过，可能被中间人解密媒体 | fingerprint 校验失败即终止，错误可观测 |
| 未实现 consent freshness | 远端不同意或地址复用后仍发送媒体 | RFC 7675 consent timer，30 秒无有效响应停止发送 |
| SRTP replay/auth failure 被当普通丢包 | 重放攻击或 key/ROC 错误被掩盖 | 区分 replay、auth、ROC、unknown SSRC 计数和告警 |
| TURN 凭据/nonce 处理粗糙 | relay 滥用、鉴权失败不可恢复 | 实现 long-term credential、nonce stale 重试、权限刷新 |
| 随机数质量不足 | ICE tie-breaker、SSRC、DTLS key/cert 不安全 | 平台 RNG vtable 必须有失败语义和启动自检 |
| 过度暴露包级 trace | 生产日志泄漏 IP、候选、指纹、媒体元信息 | trace 分级、脱敏、默认不记录 payload |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| 错误码只有 `FAILED` | 集成方不知道重试、换 TURN、请求关键帧还是降码率 | 分层错误码：SDP/ICE/TURN/DTLS/SRTP/RTP/RTCP/MEM |
| 不暴露关键帧请求事件 | 丢包后长时间黑屏 | PLI/FIR 回调应用编码器，并统计请求次数 |
| 不暴露内存水位 | 设备上线后才发现预算不足 | 初始化预算报告 + 运行水位 + 丢弃原因 |
| Chrome 互通需要抓包才能定位 | 集成成本高 | 提供 Chrome stats 对照指南和库内 trace 导出 |

## "Looks Done But Isn't" Checklist

- [ ] **Chrome connected:** ICE/DTLS connected 不等于媒体互通；验证 Chrome `getStats()` 中 RTP packets/frames/RTCP feedback 随时间正常增长。
- [ ] **H264 video visible:** 验证 IDR 前 SPS/PPS in-band、`profile-level-id` 解析、`packetization-mode=1`、FU-A 丢片恢复。
- [ ] **TURN supported:** 验证 relay-only、allocation refresh、permission refresh、ChannelData demux、nonce stale 重试。
- [ ] **DTLS-SRTP integrated:** 验证 fingerprint、setup role、selected SRTP profile、key/salt 方向、SRTCP。
- [ ] **RTCP supported:** 验证 SR/RR/SDES/BYE、NACK、PLI/FIR、reduced-size RTCP、rtcp-mux。
- [ ] **Fixed memory:** 验证 mbedTLS/libsrtp 也在预算账本内，运行时无不可控 malloc。
- [ ] **Sans-I/O lock-free:** 验证 callback reentrancy 被禁止或 deferred，乱序事件 fuzz 不破坏状态机。
- [ ] **Multi PC:** 验证每 PC 预算、调度公平、控制包不被视频发送饿死。
- [ ] **Observability:** 验证每个失败路径都有结构化事件、错误码和对象 id，可与 Chrome stats 对齐。

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| SDP/H264 fmtp 后期返工 | HIGH | 抽出 SDP AST，建立 capability model，重写 offer/answer 和 H264 packetizer contract |
| ICE happy path 过窄 | HIGH | 补 checklist 状态机、transaction table、timer、nomination、restart；新增公网 NAT lab |
| TURN 生命周期缺失 | MEDIUM-HIGH | 增加 allocation/permission/channel 状态和 refresh timer，补 relay-only 回归 |
| DTLS-SRTP key 方向错 | MEDIUM | 增加 key material 单测、Wireshark/known-answer vectors、区分 inbound/outbound policy |
| libsrtp buffer tailroom 缺失 | MEDIUM | 改 packet pool ABI，所有 protect 前容量检查，补 ASAN/valgrind |
| RTCP 延迟实现 | HIGH | 重排路线图，把 RTCP 基础前置到 RTP phase，SDP `rtcp-fb` 与事件模型联动 |
| 固定内存漏算依赖 | HIGH | 建全栈预算账本，依赖初始化集中化，无法固定的路径从 v1 scope 剔除或显式标注 |
| 可观测性缺失 | HIGH | 先冻结 stats/trace schema，再逐协议补事件；否则 debug 成本持续上升 |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Chrome 互通误判 | Phase 0 + Phase 4 | Chrome stats + 库 trace 对照矩阵 |
| SDP/H264 fmtp 过窄 | Phase 1 | Chrome SDP corpus、RFC 7742/RFC 6184 单测、真实 H264 AU 互通 |
| H264 固定内存冲突 | Phase 0 + Phase 1 | AU/FU-A 上限、丢包重组、内存水位压测 |
| ICE happy path | Phase 2 | checklist/nomination/restart/peer-reflexive 单测 + NAT lab |
| TURN 边界 | Phase 2 + Phase 4 | relay-only Chrome 互通，运行超过 allocation/permission refresh 周期 |
| Consent freshness | Phase 2 | 断网/远端停止响应后 30 秒内停止发送并上报 |
| DTLS-SRTP key/role | Phase 3 | fingerprint mismatch、role matrix、SRTP profile/key direction 测试 |
| libsrtp buffer/SSRC/ROC | Phase 3 + Phase 5 | tailroom 检查、乱序/replay、unknown SSRC、多 PC 隔离 |
| RTCP 缺失 | Phase 1 + Phase 4 | PLI/NACK/SR/RR/SDES/BYE 与 Chrome stats 验证 |
| 无锁状态机重入 | Phase 0 | reentrancy policy 单测、event fuzz、destroy-in-callback 测试 |
| 多 PC 调度不公平 | Phase 5 | 多 PC soak test、per-PC 水位、timer latency |
| 依赖内存漏算 | Phase 0 + Phase 3 | mbedTLS/libsrtp 峰值账本、并发握手预算测试 |
| 同 socket demux 错误 | Phase 1 + Phase 2 + Phase 3 | RFC 7983 classifier 边界测试、TURN Channel/RTP/RTCP/STUN/DTLS 组合测试 |
| 可观测性后补 | Phase 0 | 每个 phase 的验收包含 stats/trace 字段 |

## Sources

- RFC 8834, *Media Transport and Use of RTP in WebRTC* — RTP/RTCP 必需、RTCP feedback、WebRTC RTP 约束。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8834
- RFC 8835, *Transports for WebRTC* — 多流调度、媒体深队列风险、DSCP/transport 行为。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8835.html
- RFC 8843, *Negotiating Media Multiplexing Using SDP (BUNDLE)* — BUNDLE 与 `rtcp-mux` 约束。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8843
- RFC 8829, *JSEP* — SDP offer/answer、DTLS setup role、WebRTC profile。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8829.html
- RFC 8445, *ICE* — ICE full 状态机、checklist、Ta、nomination。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8445
- RFC 8489, *STUN* — STUN message integrity/fingerprint 与协议基础。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8489.html
- RFC 8656, *TURN* — allocation、permission、channel、refresh 生命周期。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc8656
- RFC 7675, *STUN Usage for Consent Freshness* — consent 获取/维护/30 秒过期。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc7675.html
- RFC 7983, *DTLS-SRTP Multiplexing Scheme Updates* — STUN/DTLS/TURN Channel/RTP/RTCP 同 socket demux。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc7983
- RFC 5761, *Multiplexing RTP and RTCP on a Single Port* — RTP payload type 64..95 冲突和 rtcp-mux。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc5761.html
- RFC 7742, *WebRTC Video Processing and Codec Requirements* — H264 Constrained Baseline、packetization-mode 1、profile-level-id、in-band parameter sets。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc7742
- RFC 6184, *RTP Payload Format for H.264 Video* — H264 packetization、FU-A/STAP-A、fmtp 参数。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc6184.html
- RFC 7587, *RTP Payload Format for Opus* — Opus SDP `opus/48000/2` 和 fmtp 参数。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc7587
- RFC 5764, *DTLS Extension to Establish Keys for SRTP* — `use_srtp`、SRTP profile、keying material。Confidence: HIGH. https://datatracker.ietf.org/doc/html/rfc5764
- RFC 3711, *SRTP* — ROC、packet index、replay protection。Confidence: HIGH. https://www.rfc-editor.org/rfc/rfc3711
- Chromium WebRTC RTP notes — payload type 分配、BUNDLE collision、Chrome/WebRTC RTP 行为。Confidence: HIGH. https://webrtc.googlesource.com/src/+/HEAD/pc/g3doc/rtp.md
- W3C WebRTC Stats — candidate-pair、transport、RTP stats、consent/STUN RTT 指标。Confidence: HIGH. https://www.w3.org/TR/webrtc-stats/
- Mbed TLS v3.6.1 API docs — DTLS buffer、I/O buffer、DTLS-SRTP API surface。Confidence: HIGH. https://mbed-tls.readthedocs.io/projects/api/en/v3.6.1/api/file/ssl_8h/
- Cisco libsrtp README/API notes — protect/unprotect、stream/SSRC state、tailroom、replay behavior。Confidence: HIGH. https://github.com/cisco/libsrtp
- MDN WebRTC codecs guide — 浏览器 H264/Opus 互通实践摘要。Confidence: MEDIUM. https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Formats/WebRTC_codecs
- USENIX Security 2026 prepublication, *Analyzing the WebRTC Ecosystem and Breaking Authentication in DTLS-SRTP* — DTLS-SRTP 认证绕过风险的生态研究。Confidence: MEDIUM. https://www.usenix.org/conference/usenixsecurity26/presentation/bach

---
*Pitfalls research for: libmicrortc WebRTC media transport core*
*Researched: 2026-05-12*
