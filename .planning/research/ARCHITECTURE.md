# Architecture Research

**Domain:** 纯 C、Sans-I/O、资源受限设备 WebRTC 媒体传输库  
**Researched:** 2026-05-12  
**Confidence:** HIGH（协议分层和核心边界来自 RFC/W3C/官方实现文档；具体内存预算与 Chrome 互通细节需后续实测）

## Standard Architecture

### System Overview

建议采用四层结构：公共控制 API、Sans-I/O 协议核心、平台适配外壳、第三方安全/网络实现。`rtc_context` 是唯一全局运行时对象，拥有 allocator、时间源、随机数、trace、timer wheel、socket handle 映射和多个 `rtc_pc`。每个 `rtc_pc` 拥有 JSEP/SDP 状态、一个 BUNDLE transport、ICE/DTLS/SRTP/RTP/RTCP 子状态机和媒体轨道。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Public C API / App Boundary                      │
│  rtc_context_*  rtc_pc_*  rtc_track_*  rtc_sdp_*  callbacks/stats/trace     │
├─────────────────────────────────────────────────────────────────────────────┤
│                              Sans-I/O Core                                  │
│                                                                             │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐ │
│  │ SDP/JSEP     │  │ ICE Agent   │  │ DTLS Driver  │  │ RTP/RTCP Session │ │
│  │ parser/model │  │ full/STUN   │  │ vtable pump  │  │ mux + payloaders │ │
│  └──────┬───────┘  └──────┬──────┘  └──────┬───────┘  └────────┬─────────┘ │
│         │                 │                │                   │           │
│  ┌──────▼─────────────────▼────────────────▼───────────────────▼─────────┐ │
│  │ pc_state / transport_state / timer_state / event_queue / stats sinks   │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐ │
│  │ STUN codec   │  │ TURN client │  │ SRTP vtable  │  │ H264/Opus RTP    │ │
│  │ tx/rx attrs  │  │ UDP relay   │  │ protect I/O  │  │ packetizers      │ │
│  └──────────────┘  └─────────────┘  └──────────────┘  └──────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│                         Platform Shell / Integration Layer                  │
│  Linux UDP sockets  monotonic clock  entropy  logging  user allocator       │
│  callback dispatch  poll/epoll wrapper  default mbedTLS/libsrtp adapters    │
├─────────────────────────────────────────────────────────────────────────────┤
│                            External Dependencies                            │
│  mbedTLS DTLS  libsrtp SRTP/SRTCP  OS network stack  app signaling channel   │
└─────────────────────────────────────────────────────────────────────────────┘
```

核心结论：不要做“一个巨大 PeerConnection 黑盒”。WebRTC 媒体栈天然由 SDP/JSEP、ICE、DTLS-SRTP、SRTP/SRTCP、RTP/RTCP、payload format 组成；工程结构也应按这些协议边界拆分，但由 `rtc_pc` 聚合成一个可诊断的连接状态。

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| `rtc_context` | 固定资源预算、对象池、timer wheel、trace/stats、平台 vtable、多个 PeerConnection 调度 | 单线程 owner；初始化时预分配 PC、candidate、packet buffer、timer、event 槽位 |
| `rtc_pc` | 单个浏览器对端的协商、状态聚合、BUNDLE transport、轨道和统计 | 明确状态机；不直接发 socket；只产生命令/事件 |
| `sdp` / `jsep` | SDP 解析/生成、offer/answer、ICE 参数、DTLS fingerprint、codec/PT、BUNDLE/mid、rtcp-mux | 解析为 typed model；生成只覆盖 Chrome 1v1 H264/Opus 子集 |
| `ice` | ICE full agent、候选收集、STUN binding、TURN allocation、candidate pair checklist、提名、keepalive、restart | Sans-I/O datagram state machine；平台层只负责 UDP send/recv |
| `stun` | STUN/TURN 消息编解码、transaction id、MESSAGE-INTEGRITY、FINGERPRINT、error code | 纯 codec + transaction table；不拥有 socket |
| `turn` | TURN UDP relay allocation/refresh/permission/channel bind/send/data | 作为 ICE candidate provider 和 selected path wrapper，不泄露到 RTP 层 |
| `dtls` | DTLS 握手驱动、fingerprint 校验、SRTP key export、rehandshake/rekey 预留 | vtable pump；默认 mbedTLS；核心只知道 `input_datagram`/`poll_output`/`deadline` |
| `srtp` | SRTP/SRTCP protect/unprotect、replay/error mapping、key installation | vtable；默认 libsrtp；必须预留 auth tag 尾部空间 |
| `rtp` | RTP header、SSRC/PT/sequence/timestamp、header extension、RTX/NACK 扩展点 | 与 payload format 分离；一条 BUNDLE RTP session 内 PT 全局唯一 |
| `rtcp` | SR/RR/SDES/BYE、PLI/NACK 基础反馈、RTCP interval、stats | v1 至少实现 WebRTC 必需 RTCP 和可观测统计；拥塞控制只暴露扩展点 |
| `payload_h264` | H264 access unit 与 RTP 包互转；STAP-A/FU-A；SPS/PPS/fmtp | 不编解码；接收侧重组 access unit 后回调应用 |
| `payload_opus` | Opus RTP payload、48 kHz RTP clock、DTX/duplicate handling | 不编解码；payload 透传给应用 |
| `packet_mux` | 单 UDP 5-tuple 上 STUN/TURN Channel/DTLS/RTP/RTCP 分流 | 按 RFC 7983 first-byte demux；再由 SRTP/RTCP/RTP 解析 |
| `mem` | arena、fixed pool、packet slabs、水位和 OOM 事件 | 所有核心模块只能从 context pool 获取内存；无隐式 malloc |
| `platform_linux` | UDP sockets、epoll/poll、clock、entropy、logging、默认 callbacks | 第一版参考实现；不是核心依赖 |

## Recommended Project Structure

```text
include/
├── micrortc.h              # 稳定公共 C API
├── micrortc_config.h       # 编译期能力、limits、feature flags
└── micrortc_types.h        # ABI-safe 公共类型、错误码、stats 类型

src/
├── core/
│   ├── context.c           # rtc_context 生命周期、调度入口、对象池 owner
│   ├── pc.c                # PeerConnection 聚合状态机
│   ├── event.c             # core event/output command queue
│   ├── timer.c             # deadline/timer wheel
│   ├── stats.c             # counters, gauges, snapshots
│   └── trace.c             # packet/state trace hooks
├── mem/
│   ├── arena.c             # 用户 allocator/arena 包装
│   ├── pool.c              # fixed-size pools
│   └── packet.c            # packet slab/ref model
├── sdp/
│   ├── sdp_parse.c
│   ├── sdp_write.c
│   ├── jsep.c              # offer/answer/signaling state subset
│   └── chrome_profile.c    # v1 Chrome H264/Opus SDP profile
├── ice/
│   ├── ice_agent.c
│   ├── ice_candidate.c
│   ├── ice_checklist.c
│   ├── ice_nomination.c
│   ├── ice_restart.c
│   ├── stun.c
│   └── turn_udp.c
├── transport/
│   ├── mux.c               # RFC 7983 demux
│   ├── dtls.c              # DTLS vtable integration
│   ├── srtp.c              # SRTP vtable integration
│   └── path.c              # selected ICE path abstraction
├── rtp/
│   ├── rtp_packet.c
│   ├── rtp_session.c
│   ├── rtp_sender.c
│   ├── rtp_receiver.c
│   ├── rtcp.c
│   ├── rtcp_scheduler.c
│   └── rtcp_feedback.c
├── media/
│   ├── h264_packetizer.c
│   ├── h264_depacketizer.c
│   ├── opus_packetizer.c
│   └── opus_depacketizer.c
├── crypto/
│   ├── dtls_vtable.h       # internal adapter contract
│   ├── srtp_vtable.h
│   ├── mbedtls_adapter.c
│   └── libsrtp_adapter.c
├── platform/
│   ├── platform_vtable.h
│   └── linux/
│       ├── linux_context.c
│       ├── udp_socket.c
│       └── poll_loop.c
└── util/
    ├── endian.c
    ├── rand.c
    ├── crc32.c
    └── base64.c

tests/
├── unit/                   # codec/state/pool 单元测试
├── fuzz/                   # SDP/STUN/RTP/RTCP/H264 payload fuzz
├── interop/                # Chrome/Puppeteer 或 headless browser 互通
└── vectors/                # RFC/test packet fixtures
```

### Structure Rationale

- **`core/` 只做编排，不做协议细节。** `rtc_pc` 可以聚合状态，但 ICE、DTLS、SRTP、RTP/RTCP 必须各自可单测，否则后续 NAT、重传、rekey、RTCP 反馈会把连接逻辑拖成不可维护的全局状态。
- **`ice/` 同时包含 STUN/TURN codec，但 TURN 作为 ICE 的候选和路径来源。** 应用层不应该看到 TURN send indication/channel data；RTP 层只看到 selected path 可发 datagram。
- **`transport/` 是安全传输边界。** `mux.c` 先按 first byte 分发 STUN/DTLS/TURN/RTP；`dtls.c` 完成握手并把 key material 安装到 `srtp.c`；RTP 层不直接依赖 mbedTLS/libsrtp。
- **`rtp/` 与 `media/` 分离。** RTP session 管 SSRC/PT/sequence/timestamp/RTCP；H264/Opus payload format 管 access unit 或 frame 到 RTP payload 的映射。
- **`platform/` 是可替换外壳。** 第一版 Linux 可以自带回调驱动 loop，但核心 API 应保持 `rtc_context_step(now)` / `rtc_context_input_datagram(...)` / `rtc_context_pop_output(...)` 这种可嵌入形态。

## Architectural Patterns

### Pattern 1: Sans-I/O Step Function

**What:** 所有协议状态机只消费输入事件、当前时间和配置，输出“待发送 datagram / 回调事件 / 下次 deadline”。不调用 socket、sleep、thread、malloc。  
**When to use:** ICE/STUN/TURN、DTLS pump、RTCP scheduler、RTP sender/receiver、SDP/JSEP 状态推进。  
**Trade-offs:** 测试和嵌入更容易；代价是需要显式 output queue、timer queue 和较严格的所有权设计。

```c
rtc_status rtc_context_step(rtc_context *ctx, rtc_time_ms now);
rtc_status rtc_context_input_datagram(
    rtc_context *ctx,
    rtc_socket_id sock,
    const rtc_addr *src,
    const uint8_t *data,
    size_t len,
    rtc_time_ms now);
rtc_status rtc_context_pop_output(rtc_context *ctx, rtc_output *out);
```

### Pattern 2: One BUNDLE Transport Per PeerConnection

**What:** v1 对 Chrome 1v1 使用 BUNDLE + rtcp-mux，音频、视频、RTCP、DTLS 和 ICE 共享一个 ICE selected pair/5-tuple；RTP session 内通过 SSRC、payload type、MID/RID 扩展区分流。  
**When to use:** v1 默认且建议唯一模式。RFC 8834 要求 WebRTC 支持单 RTP session/单 transport flow，RFC 8843 定义 BUNDLE，同一 BUNDLE group 内 PT 必须避免冲突。  
**Trade-offs:** NAT/端口/内存成本最低；但 payload type、SSRC、RTCP CNAME、MID 绑定必须做严格表驱动，不能按 m-line 独立假设。

### Pattern 3: Explicit State Ownership

**What:** 每个层级维护自己的细粒度状态，并由 `rtc_pc` 聚合成应用可见状态。

```text
signaling: stable / have-local-offer / have-remote-offer / closed
ice_gathering: new / gathering / complete
ice: new / checking / connected / completed / disconnected / failed / closed
dtls: new / connecting / connected / failed / closed
srtp: empty / keying / ready / failed
pc: new / connecting / connected / disconnected / failed / closed
```

**When to use:** 所有跨协议状态联动，例如 remote SDP 到达后启动 ICE，ICE selected pair 可写后启动 DTLS，DTLS key export 后启用 SRTP。  
**Trade-offs:** 状态数量更多，但错误定位清晰。W3C 的 `RTCPeerConnectionState` 是聚合状态，不能替代内部 ICE/DTLS/SRTP 状态。

### Pattern 4: Fixed Pools With Handles, Not Owning Pointers

**What:** `rtc_context` 初始化时按配置预分配对象池，内部引用使用 index/generation handle，避免悬垂指针和碎片化。

```text
pc_pool[max_peer_connections]
candidate_pool[max_pcs * max_candidates_per_pc]
candidate_pair_pool[max_pcs * max_candidate_pairs_per_pc]
packet_pool[max_packets]
timer_pool[max_timers]
event_pool[max_events]
track_pool[max_pcs * max_tracks_per_pc]
```

**When to use:** 所有长期对象和 packet buffers。  
**Trade-offs:** 配置不合理时会显式 OOM；这是资源受限设备的正确失败模式。必须提供水位统计和“需要多少槽位”的错误诊断。

### Pattern 5: External Security Implementations Behind Narrow Vtables

**What:** DTLS 和 SRTP 不进入核心协议实现，只通过 narrow vtable 接入。

```c
typedef struct rtc_dtls_vtable {
    rtc_status (*create)(void *user, rtc_dtls **out);
    rtc_status (*set_role)(rtc_dtls *dtls, rtc_dtls_role role);
    rtc_status (*input)(rtc_dtls *dtls, const uint8_t *pkt, size_t len);
    rtc_status (*step)(rtc_dtls *dtls, rtc_time_ms now);
    rtc_status (*pop_datagram)(rtc_dtls *dtls, rtc_buf *out);
    rtc_status (*export_srtp_keys)(rtc_dtls *dtls, rtc_srtp_keys *out);
    void (*destroy)(rtc_dtls *dtls);
} rtc_dtls_vtable;
```

**When to use:** mbedTLS/libsrtp 默认实现和未来替换实现。  
**Trade-offs:** vtable 增加少量 glue code，但避免把第三方库 ABI、allocator、timer、socket 模型泄露到核心。

## Data Flow

### Outbound Media Flow

```text
app encoded H264 access unit / Opus frame
    ↓ rtc_track_send_*
media packetizer
    ↓ RTP payload(s)
rtp_sender assigns SSRC/PT/seq/timestamp/extensions
    ↓ RTP packet
srtp.protect() / srtcp.protect()
    ↓ SRTP/SRTCP packet
selected ICE path
    ↓ datagram output command
platform UDP sendto()
```

### Inbound Packet Flow

```text
platform UDP recvfrom()
    ↓ rtc_context_input_datagram(sock, src, bytes)
packet_mux first-byte demux
    ├── STUN [0..3] → ICE transaction/checklist/TURN
    ├── TURN Channel [64..79] → TURN unwrap → inner data → mux again
    ├── DTLS [20..63] → DTLS input/pump → SRTP key export
    └── RTP/RTCP [128..191] → SRTP/SRTCP unprotect
            ├── RTCP → rtcp parser/stats/feedback callbacks
            └── RTP  → rtp_receiver → payload depacketizer → app frame callback
```

### Offer/Answer and Transport Startup Flow

```text
create_pc
  ↓
add audio/video track descriptors
  ↓
create_offer()
  ├── allocate ICE ufrag/pwd
  ├── generate DTLS certificate fingerprint
  ├── assign MID/PT/SSRC/CNAME
  └── write SDP with BUNDLE + rtcp-mux + UDP/TLS/RTP/SAVPF
  ↓
set_local_description()
  ↓
ICE gathering starts; trickle candidates emitted
  ↓
set_remote_description(answer)
  ├── parse ICE params/candidates
  ├── verify codec/PT/fmtp/mid/BUNDLE
  ├── store DTLS setup role + fingerprint
  └── form ICE checklists
  ↓
ICE selected pair nominated
  ↓
DTLS handshake over selected ICE path
  ↓
fingerprint verified; SRTP keys exported
  ↓
SRTP/SRTCP sessions installed
  ↓
media send/receive enabled
```

### State Management

```text
[app API call / inbound packet / timer]
        ↓
rtc_context_step(now)
        ↓
pc state reducer
        ↓
module state reducers: jsep, ice, dtls, srtp, rtp, rtcp
        ↓
event queue + output datagram queue + next deadlines
        ↓
platform shell dispatches callbacks and socket writes
```

### Key Data Flows

1. **ICE path selection:** STUN/TURN candidate gathering produces local candidates；remote SDP/candidates produce remote candidates；ICE checklist produces nominated pair；`transport/path.c` exposes a stable selected path to DTLS and SRTP.
2. **DTLS-SRTP keying:** DTLS starts only after ICE has a writable nominated pair；DTLS validates remote fingerprint from SDP；after handshake it exports SRTP keying material；SRTP adapter creates inbound/outbound SRTP/SRTCP contexts.
3. **RTP/RTCP mux:** BUNDLE + rtcp-mux means RTP and RTCP share the same transport flow；first-byte demux only separates RTP/RTCP as a family，之后根据 RTCP packet type / RTP PT / SSRC / MID 继续分发。
4. **Stats/observability:** 每个状态迁移、STUN transaction、candidate pair result、DTLS alert、SRTP auth fail、RTP loss/jitter、RTCP SR/RR 都要产出结构化事件；不要只做 printf。

## State Flow

### PeerConnection Aggregate State

```text
new
 ↓ create offer/answer or set remote description
connecting
 ├── ice failed        → failed
 ├── dtls failed       → failed
 ├── close            → closed
 └── ice connected + dtls connected + srtp ready
        ↓
connected
 ├── consent/keepalive timeout → disconnected
 ├── ice restart              → connecting
 ├── fatal protocol error     → failed
 └── close                    → closed
```

### ICE Full Agent State

```text
new
 ↓ local credentials allocated
gathering
 ↓ host/srflx/relay candidates available
checking
 ↓ successful nominated pair
connected
 ↓ checklist complete and no better checks pending
completed
 ├── consent or keepalive failures → disconnected → failed
 ├── ICE restart                   → gathering/checking
 └── close                         → closed
```

ICE 内部必须显式维护：

- local/remote ICE role：controlling/controlled，处理 role conflict。
- candidate foundations、priorities、components。v1 使用 rtcp-mux/BUNDLE 后建议只支持 component 1。
- candidate pair checklist states：Frozen、Waiting、In-Progress、Succeeded、Failed。
- STUN transaction retransmission deadline、RTO、transaction id、integrity credentials。
- TURN allocation/permission/channel refresh timers。

### DTLS/SRTP State

```text
dtls new
 ↓ ICE selected path writable
connecting
 ↓ handshake complete + fingerprint ok
connected
 ↓ export keying material
srtp ready
 ├── DTLS alert / fingerprint mismatch / key export fail → failed
 ├── rehandshake/rekey                               → keying/ready
 └── close                                           → closed
```

mbedTLS 的 DTLS 集成需要自定义 event-loop timer callbacks；官方文档明确 DTLS 需要 timer callbacks，event-based I/O 要写自己的回调。因此默认 adapter 不应使用阻塞 read timeout，而应把 mbedTLS WANT_READ/WANT_WRITE、retransmit deadline 和 output datagrams 映射回 Sans-I/O shell。

## Scaling Considerations

这里的“规模”不是服务器用户量，而是同一资源受限设备上的 PeerConnection/轨道/候选数量。

| Scale | Architecture Adjustments |
|-------|--------------------------|
| 1 PC, audio+video | 单 BUNDLE transport；固定 2 tracks；候选/pair/packet pool 取保守默认；完整 trace 开启 |
| 2-8 PCs mesh | `rtc_context` 做 round-robin 或 deadline-priority 调度；全局 packet pool 按 PC 配额隔离；限制每 PC candidate pair 数 |
| 8+ PCs 或弱设备 | 需要显式 admission control；禁用非必要 candidate 类型或限制 relay；按 PC 预算拒绝新连接；后续考虑 SFU/网关而非端侧 mesh |

### Scaling Priorities

1. **First bottleneck: packet buffer 和 candidate pair 爆炸。** ICE candidate pair 是 local×remote 组合，公网/NAT/TURN 场景很容易超过预期。需要 per-PC hard limit、foundation 剪枝、明确 `RTC_ERR_RESOURCE_LIMIT`。
2. **Second bottleneck: DTLS/SRTP per-PC state。** DTLS 握手和 SRTP replay window 都是每连接成本；vtable adapter 必须支持静态内存或受控 allocator。
3. **Third bottleneck: RTCP/RTP timing fairness。** 多 PC 单线程下不能让一个大视频发送队列饿死 ICE/DTLS timer；调度优先级应为 timers/control > DTLS/ICE > RTCP > RTP media。

## Anti-Patterns

### Anti-Pattern 1: 把 SDP 当字符串拼接

**What people do:** 用 ad hoc 字符串查找和拼接 `a=` 行。  
**Why it's wrong:** BUNDLE、payload type 唯一性、fmtp、mid、rtcp-mux、ice-ufrag/pwd、fingerprint、setup role 都有跨行关系；字符串拼接会导致 Chrome 互通问题难以定位。  
**Do this instead:** 解析成 typed SDP model，再由 Chrome profile 生成 v1 子集。

### Anti-Pattern 2: ICE 成功后忽略 STUN/consent/TURN refresh

**What people do:** nominated pair 成功后只发 RTP。  
**Why it's wrong:** NAT binding、TURN allocation、permissions、ICE consent 都依赖持续控制流；长通话会“开始成功，几分钟后黑屏/无声”。  
**Do this instead:** ICE/TURN timer 是 context 调度的一等任务，优先级高于媒体。

### Anti-Pattern 3: DTLS/SRTP 直接调用 socket

**What people do:** mbedTLS BIO callback 里直接 `sendto/recvfrom`。  
**Why it's wrong:** 破坏 Sans-I/O；难以固定内存、测试重传、与 ICE selected path 切换协作。  
**Do this instead:** BIO 读写 adapter 只读写内存队列，DTLS output 变成 core output datagram。

### Anti-Pattern 4: 以 m-line 划分 RTP session

**What people do:** audio/video 各自独立 RTP session 和 PT 表。  
**Why it's wrong:** WebRTC/Chrome 默认 BUNDLE 下 audio/video 在同一 RTP session，payload type 在 BUNDLE group 内必须唯一，SSRC/MID 才是正确分流关键。  
**Do this instead:** v1 内部建一个 RTP session，多个 RTP stream/track 挂在 session 下。

### Anti-Pattern 5: RTP payload 与编解码耦合

**What people do:** H264/Opus packetizer 调用 encoder/decoder 或假设 Annex B 输入永远一致。  
**Why it's wrong:** 项目明确不做编解码；H264 RTP payload 传输的是 NAL unit stream，应用输入格式需要清晰约定和转换边界。  
**Do this instead:** API 明确接受 H264 access unit/NALU view 和 Opus frame；必要时提供 Annex B/AVCC 辅助转换，但不进核心发送路径。

### Anti-Pattern 6: 运行时隐式扩容

**What people do:** candidate、packet、event、RTCP report 动态 malloc。  
**Why it's wrong:** 与固定内存预算冲突；OOM 会出现在最难复现的 NAT/抖动路径。  
**Do this instead:** 初始化时计算并分配 pools，所有失败带上 pool name、requested count、high water mark。

## Integration Points

### External Services / Libraries

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| Application signaling | 应用传入/取出 SDP 和 trickle ICE candidate | 库不内置 WebSocket/MQTT/HTTP；JSEP 状态仍由库校验 |
| STUN server | ICE candidate gathering + connectivity checks | STUN 是 ICE 的工具，不是独立 NAT 穿透方案 |
| TURN server | ICE relay candidate provider + selected path wrapper | v1 建议只做 UDP relay；ChannelData 支持需遵循 RFC 7983 demux 范围 |
| mbedTLS | `rtc_dtls_vtable` 默认实现 | 使用非阻塞/event-loop BIO 和 DTLS timer callbacks；不让 mbedTLS 拥有 socket |
| libsrtp | `rtc_srtp_vtable` 默认实现 | protect buffer 必须预留 auth tag；SRTP stream/session 按 SSRC/key policy 建模 |
| Chrome | 互通基准 | 用 headless/真实 Chrome 做 SDP、ICE、DTLS、SRTP、RTP/RTCP 回归 |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| App ↔ Public API | 函数调用 + callbacks | callbacks 不应重入修改 core；建议排队到下一次 `step` 或明确文档化 |
| Platform ↔ Core | datagram input/output command、time、entropy、log | 平台不理解 SDP/RTP；只搬运 bytes 和时间 |
| PC ↔ SDP/JSEP | typed model | `rtc_pc` 只读取协商结果，不解析字符串 |
| ICE ↔ DTLS/SRTP | selected path datagram service | DTLS/SRTP 不知道 host/srflx/relay 细节 |
| DTLS ↔ SRTP | exported key material + protection profile | SRTP 不参与 DTLS 握手；DTLS 不解析 RTP |
| RTP ↔ Payload | payload format function table | RTP 管 packet header；payload 管 codec-specific fragmentation/reassembly |
| Modules ↔ Memory | pool handles | 模块不得私自保存外部 buffer 指针超过调用期，除非 buffer 来自 packet pool 且引用计数明确 |

## Recommended Build Order

1. **Foundation: allocator、pool、errors、trace、timer、Sans-I/O harness。** 先建立固定内存和可观测框架，否则后面协议失败不可诊断。
2. **Packet codecs: STUN、SDP subset、RTP、RTCP、H264/Opus payload。** 这些可离线单测/fuzz，不依赖网络。
3. **ICE full without TURN: host candidates + STUN binding + checklist + nomination。** 先在局域网/受控 NAT 跑通 selected pair。
4. **TURN UDP relay: allocate/refresh/permission/channel/data。** 再覆盖真实公网 NAT；同时完善 RFC 7983 demux。
5. **DTLS vtable + mbedTLS adapter。** 用 fake ICE path 驱动握手，再接入 ICE selected path；实现 fingerprint 校验和 key export。
6. **SRTP vtable + libsrtp adapter。** 先对 RTP/RTCP fixtures protect/unprotect，再接到 DTLS key export。
7. **RTP/RTCP media session。** 发送 H264/Opus 到 Chrome；接收 Chrome RTP 并重组 frame；补 SR/RR/SDES/PLI/NACK 基础反馈。
8. **Full PeerConnection state aggregation。** 将 JSEP、ICE、DTLS、SRTP、RTP/RTCP 状态统一成公共事件、stats 和错误码。
9. **Linux callback shell。** UDP socket/poll loop、示例 signaling glue、Chrome 互通测试。
10. **Multi PeerConnection scheduling。** 在单 `rtc_context` 下跑多 PC，验证 pool 配额、timer fairness、trace 可读性。

构建顺序的关键依赖是：**固定内存/trace 先于协议，ICE selected path 先于 DTLS，DTLS key export 先于 SRTP，SRTP ready 先于媒体发送**。

## Confidence and Open Questions

| Area | Confidence | Notes |
|------|------------|-------|
| WebRTC 协议分层 | HIGH | RFC 8834、RFC 8843、RFC 8829、W3C WebRTC 与 libwebrtc 文档一致 |
| BUNDLE/rtcp-mux 单 transport 建议 | HIGH | Chrome 互通和资源受限目标都支持该方向 |
| ICE/STUN/TURN 边界 | HIGH | RFC 8445、RFC 8489、RFC 8656 明确角色；具体 NAT 失败策略需实测 |
| DTLS/SRTP vtable | HIGH | 项目已决策；mbedTLS/libsrtp 官方文档支持该集成模式 |
| H264/Opus payload 边界 | MEDIUM-HIGH | RFC 明确；但 Chrome SDP profile-level-id、packetization-mode、SPS/PPS 发送策略需互通验证 |
| 多 PeerConnection 调度参数 | MEDIUM | 架构清晰，但默认 pool size、timer 优先级和公平性需要基准测试确定 |
| 不做拥塞控制的 Chrome 体验 | MEDIUM | 协议上可先保留 RTCP/stats；真实弱网体验和 Chrome 反馈期望需后续研究 |

### Uncertain Points Requiring Phase Research

- **DTLS 版本与 cipher/profile 组合。** 需要确认 Chrome 当前默认 DTLS/SRTP protection profile 与 mbedTLS 配置矩阵，避免编译出无法协商的组合。
- **Chrome H264 SDP profile。** `profile-level-id`、`packetization-mode=1`、`level-asymmetry-allowed`、SPS/PPS out-of-band/in-band 策略需要互通测试锁定。
- **RTCP 最小集合。** v1 至少 SR/RR/SDES/BYE/PLI/NACK；是否必须支持 transport-cc 或 abs-send-time 才能让 Chrome 表现稳定，需要实测。
- **ICE consent freshness。** RFC 与浏览器行为之间的定时参数要通过 Chrome 互通和长通话测试确定。
- **TURN ChannelData vs Send/Data Indication 默认策略。** ChannelData 降低开销但增加 demux/ChannelBind 状态；v1 可先实现 Send/Data，再根据性能切 ChannelData。

## Sources

- RFC 8834, Media Transport and Use of RTP in WebRTC, January 2021 — RTP/RTCP、RTP/SAVPF、SRTP、单 RTP session、rtcp-mux 要求。https://www.rfc-editor.org/rfc/rfc8834
- RFC 8445, Interactive Connectivity Establishment (ICE), July 2018 — ICE full agent、candidate pair、checklist、nomination。https://www.rfc-editor.org/rfc/rfc8445
- RFC 8489, STUN, February 2020 — STUN 作为 NAT traversal 工具、connectivity check/keepalive 基础。https://www.rfc-editor.org/rfc/rfc8489
- RFC 8656, TURN, February 2020 — relay allocation、permission、channel、ICE 中继用途。https://www.rfc-editor.org/rfc/rfc8656
- RFC 5764, DTLS-SRTP, May 2010 — DTLS keying for SRTP、SSRC/SRTP association、rekey。https://www.rfc-editor.org/rfc/rfc5764
- RFC 7983, DTLS-SRTP multiplexing update, September 2016 — STUN/DTLS/TURN/RTP first-byte demux。https://www.rfc-editor.org/rfc/rfc7983
- RFC 8843, SDP BUNDLE, January 2021 — BUNDLE group、MID、bundle-only、单 RTP session。https://www.rfc-editor.org/rfc/rfc8843
- RFC 8829, JSEP, January 2021 — WebRTC offer/answer、signaling 与 media plane 分离。https://www.rfc-editor.org/rfc/rfc8829
- RFC 8839, SDP Offer/Answer for ICE, January 2021 — ICE SDP 属性和 offer/answer procedures。https://www.rfc-editor.org/rfc/rfc8839
- RFC 8866, SDP, January 2021 — SDP 基础语法和语义。https://www.rfc-editor.org/rfc/rfc8866
- RFC 6184, RTP Payload Format for H.264, May 2011 — H264 NAL/access unit、FU/STAP payload 边界。https://www.rfc-editor.org/rfc/rfc6184
- RFC 7587, RTP Payload Format for Opus, June 2015 — Opus RTP timestamp、DTX/FEC/duplicate handling。https://www.rfc-editor.org/rfc/rfc7587
- W3C WebRTC Recommendation, March 2025 snapshot — RTCPeerConnection/ICE/DTLS 状态聚合模型。https://www.w3.org/TR/webrtc/
- WebRTC project architecture page — WebRTC 官方分层：API、Transport/Session、RTP、STUN/ICE、Session Management。https://webrtc.github.io/webrtc-org/architecture/
- libwebrtc DTLS transport g3doc — ICE writable 后 DTLS、fingerprint 校验、DTLS/SRTP demux、key export/protect 边界。https://webrtc.googlesource.com/src/+/HEAD/pc/g3doc/dtls_transport.md
- libwebrtc RTP g3doc — payload type 动态分配、BUNDLE 下 PT collision、RTCP 避让范围。https://webrtc.googlesource.com/src/+/HEAD/pc/g3doc/rtp.md
- Mbed TLS DTLS tutorial / Context7 lookup — DTLS timer callbacks、event-based I/O 自定义回调要求。https://mbed-tls.readthedocs.io/en/latest/kb/how-to/dtls-tutorial/
- Cisco libsrtp README — SRTP session/stream/policy、protect/unprotect、buffer tag 空间、replay protection。https://github.com/cisco/libsrtp
- Sans-I/O architecture guide — I/O-free protocol core、同步输入输出、可测试性和平台外壳分离。https://sans-io.readthedocs.io/how-to-sans-io.html

---
*Architecture research for: libmicrortc WebRTC media transport stack*  
*Researched: 2026-05-12*
