---
phase: 6
slug: chrome-end-to-end-acceptance
status: complete
created: 2026-05-11
---

# 第 6 阶段：Chrome 端到端验收研究

## User Constraints

### 阶段边界

本阶段交付真实 Chrome 1v1 音视频端到端验收：用本地 Chrome 页面、WebSocket 信令服务和 C 示例进程，把已实现的 SDP/JSEP、ICE/STUN、DTLS-SRTP、RTP/RTCP、typed media API、observer、trace 和 counters 串成可运行、可诊断、可人工确认媒体播放的验收路径。

本阶段不改变核心项目边界：库仍保持纯 C、固定内存、无线程，不创建 socket，不处理音视频编解码，不引入 GPL/LGPL 依赖。用户侧示例可以负责本机 UDP/socket 收发、WebSocket 信令、样本文件读取、媒体输出落盘和验收脚本编排，但这些能力不得变成核心库运行期动态依赖。

### 锁定决策

- **D-01:** 第 6 阶段首个 happy path 固定为 **Chrome 主叫，C 示例接听**。Chrome 页面生成 offer、采集或生成媒体并发起 ICE；C 示例进程解析 offer、生成 answer，并驱动 ICE、DTLS/SRTP、RTP/RTCP 和 typed media API。
- **D-02:** Chrome 页面与 C 示例之间使用 **本机 WebSocket 信令 + C 示例自管 UDP socket**。WebSocket 信令只转发 SDP 和 trickle ICE candidate，不替核心库隐藏“用户负责 UDP/socket 收发”的边界。
- **D-03:** 首验收以本机或局域网 **host candidate** 为主；`0/1` 个 STUN IP:port 保持可选能力，但公网 NAT 穿透和外部 STUN 依赖不是第 6 阶段 must-have。
- **D-04:** 首验收要求 **双向音视频**。Chrome 向 C 示例发送合成音视频；C 示例向 Chrome 发送项目固定样本媒体。允许 C 侧使用测试媒体源，不要求接入真实编码器。
- **D-05:** C 示例发送给 Chrome 的媒体直接使用项目根目录的 `sample1.opus` 和 `test-25fps.h264`。Planner 必须把这两个文件作为阶段样本输入处理，而不是另造临时媒体 fixture。
- **D-06:** `sample1.opus` 当前是 Ogg Opus 文件；`test-25fps.h264` 是裸 H264 数据。示例层需要读取、切帧或解析到现有 encoded media API 可接受的 Opus frame / H264 access unit；不得把通用编解码器实现放入核心库。
- **D-07:** Chrome 侧媒体源使用浏览器合成媒体，而不是依赖真实摄像头/麦克风。页面应使用 canvas 产生可见视频图案，并使用 Web Audio oscillator 或等价浏览器能力产生音频，以保证验收可重复。
- **D-08:** C 示例收到 Chrome 媒体后，必须把接收到的音视频写入磁盘，供用户后续用 VLC 播放检查。不能只依赖日志或 counters 判断媒体内容。
- **D-09:** C 示例发送样本时按媒体固有节奏发送。H264 按 `test-25fps.h264` 的 25fps 节奏；Opus 按解析出的 packet/granule 时序，或在 parser 无法安全推导时使用稳定的 20ms packet 节奏。不得为 happy path 尽快倾倒全部样本。
- **D-10:** 自动化边界固定为：脚本启动信令服务、C 示例和 Chrome 页面，检查 SDP 交换、ICE connected、DTLS/SRTP ready、RTP/RTCP counters 增长、输出文件产生和关键页面状态；媒体播放质量由用户后续用 VLC 人工确认。
- **D-11:** C 示例必须输出结构化 **JSONL 事件** 和最终 summary。事件至少能记录阶段、状态、错误 detail、counter snapshot 或关键 trace 字段；summary 应给出 pass/fail 和失败层级。
- **D-12:** 验收脚本至少要分层识别 signaling、ICE、DTLS、SRTP、RTP、RTCP、media file 失败。更细粒度协议原因仍通过 observer error、trace reason、counter 和 JSONL 字段呈现，不要求全部变成脚本一级分类。
- **D-13:** Chrome 页面应显示紧凑状态面板，包括 signaling、ICE/connection 状态、media tracks、candidate 数量、bytes/frames 或等价 WebRTC stats 摘要。页面不应只显示 video/audio 控件，也不需要首版铺满全部 SDP/candidate debug 详情。

### 延后事项

- Chrome 接听、C 示例主叫方向：第 6 阶段首个 happy path 不要求首批覆盖，可在基础路径稳定后扩展。
- 必须通过公网 NAT / 外部 STUN srflx 验收：不作为第 6 阶段 must-have，STUN 只保持可选路径。
- 全自动音视频内容解码校验：不作为首版成功标准，避免引入播放器/解码器/平台依赖。
- 真实摄像头/麦克风采集验收：当前选择浏览器合成媒体作为默认可重复路径，真实设备可作为后续手工增强。

## Project Constraints (from AGENTS.md)

- 所有 Markdown 文档尽可能使用中文编写；技术标识符、API 名称、协议名和需求 ID 可以保留英文。[VERIFIED: AGENTS.md]
- 优先读取 `.planning/PROJECT.md`、`.planning/REQUIREMENTS.md`、`.planning/ROADMAP.md`、`.planning/STATE.md` 和当前阶段文档。[VERIFIED: AGENTS.md]
- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发的项目边界。[VERIFIED: AGENTS.md]
- 不要引入 GPL/LGPL 依赖。[VERIFIED: AGENTS.md]
- 修改规划文档时同步维护需求追踪和项目状态。[VERIFIED: AGENTS.md]

## Standard Stack

### 核心库与 C 示例

- 继续使用现有 CMake 静态库目标 `rtc`，新增 E2E C 示例应接入 `RTC_BUILD_EXAMPLES`，不要改成动态库或引入核心库运行期外部依赖。[VERIFIED: CMakeLists.txt]
- C 示例可以使用 POSIX socket、文件 I/O 和进程级循环实现用户侧 UDP/socket 收发；这些属于示例层，不能进入 `src/` 核心库。[VERIFIED: 06-CONTEXT.md]
- WebSocket 信令建议放在 Node 脚本中，使用 `ws` 包；`npm view ws` 显示当前版本 `8.20.0`，许可证 `MIT`，符合非 GPL/LGPL 约束。[VERIFIED: npm registry]
- 浏览器自动化建议使用 Playwright；`npm view @playwright/test` 显示当前版本 `1.59.1`，许可证 `Apache-2.0`，可用于启动 Chrome、读取页面状态和收集日志。[VERIFIED: npm registry]
- 静态页面服务可使用 Node 内置 `http`/`fs`，或在脚本内提供最小静态响应；如使用 `http-server`，`npm view http-server` 显示当前版本 `14.1.1`，许可证 `MIT`。[VERIFIED: npm registry]

### Chrome 页面媒体源

- `HTMLCanvasElement.captureStream()` 可以从 canvas 内容创建实时视频 `MediaStream`，适合作为可重复的合成视频源。[CITED: https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement/captureStream]
- `RTCPeerConnection.addTrack()` 可把 `MediaStreamTrack` 添加到连接中；页面应在 `createOffer()` 之前添加音视频 track，确保 offer 包含媒体 m-line。[CITED: https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/addTrack]
- `AudioContext.createMediaStreamDestination()` / `MediaStreamAudioDestinationNode` 可把 Web Audio 图输出成 `MediaStream`，适合作为 oscillator 合成音频源。[CITED: https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamAudioDestinationNode]
- `RTCPeerConnection.getStats()` 和连接状态事件可用于页面状态面板和自动化脚本的 bytes/frames/candidate 状态采样。[CITED: https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection]

## Architecture Patterns

### 分层拓扑

1. `examples/chrome_e2e/page/`：Chrome 验收页面，负责合成音视频、创建 offer、交换 trickle candidate、显示 compact 状态面板、播放/渲染 C 侧返回媒体。[VERIFIED: 06-CONTEXT.md]
2. `examples/chrome_e2e/signaling.mjs`：本机 WebSocket 信令服务，只转发 `offer`、`answer`、`candidate`、`status`、`summary` 等 JSON 消息，不做媒体或 UDP 代理。[VERIFIED: 06-CONTEXT.md]
3. `examples/chrome_e2e/rtc_chrome_e2e.c`：C 示例进程，负责 WebSocket 信令客户端、UDP socket pump、调用 public `PeerConnection` API、发送样本媒体、落盘接收媒体、输出 JSONL。[VERIFIED: include/rtc/peer_connection.h]
4. `examples/chrome_e2e/run_e2e.mjs`：自动化编排脚本，启动构建产物、信令服务和 Chrome，读取 JSONL 与页面状态，按层级输出 pass/fail。[VERIFIED: 06-CONTEXT.md]

### C 侧状态机建议

- `signaling.connected` 后等待 Chrome `offer`，调用 `rtc_peer_connection_set_remote_description()`、`rtc_peer_connection_create_answer()`、`rtc_peer_connection_set_local_description()`，再通过信令发 `answer`。[VERIFIED: include/rtc/peer_connection.h]
- 通过 `rtc_peer_connection_gather_candidates()` 触发本地 candidate；`observer.on_local_candidate` 负责发信令 candidate。[VERIFIED: include/rtc/observer.h]
- 收到远端 candidate 后调用 `rtc_peer_connection_add_ice_candidate()`，在本地 candidate/remote candidate 都可用后调用 `rtc_peer_connection_start_connectivity_checks()`。[VERIFIED: include/rtc/peer_connection.h]
- UDP socket 收到 datagram 后调用 `rtc_peer_connection_receive_datagram()`；`observer.on_datagram` 输出后，示例复制到自己的发送 buffer 并通过 UDP socket 发往当前远端候选地址或 selected pair 地址。[VERIFIED: docs/API-执行器与内存契约.md]
- `observer.on_state` 监听 `ice.connected`、`dtls.connected`、`srtp.ready` 等状态，作为启动样本发送和 E2E pass/fail 判断的依据。[VERIFIED: src/ice/ice.c, src/security/security.c]
- `rtc_peer_connection_send_media_frame()` 用于 C 到 Chrome 的样本发送；`observer.on_media_frame_typed` 用于 Chrome 到 C 的接收落盘；`observer.on_media_feedback` 记录 PLI/NACK，不执行重传。[VERIFIED: include/rtc/media.h, src/media/media.c]

### 样本文件处理

- `test-25fps.h264` 是裸 H264 数据；示例层应实现 Annex B start code access unit 切分，按 25fps 节奏提交 `RTC_MEDIA_KIND_VIDEO_H264` frame。[VERIFIED: file command, 06-CONTEXT.md]
- `sample1.opus` 是 Ogg Opus；示例层应实现足够窄的 Ogg page / Opus packet 提取，输出 Opus packet 给 `RTC_MEDIA_KIND_AUDIO_OPUS`。如 granule 时序不足以安全推导，按 D-09 使用 20ms packet 节奏。[VERIFIED: file command, 06-CONTEXT.md]
- 接收落盘建议分为 `received-opus.ogg` 或 `received-opus.packets` 与 `received-h264.264`。如果第一个计划无法安全写出完整容器，至少要写出明确扩展名和 README 中可复现的 VLC/ffplay 播放说明，且 JSONL summary 要报告媒体文件字节数和 frame 数。[VERIFIED: 06-CONTEXT.md]

## Don't Hand-Roll

- 不要在核心库实现 WebSocket、HTTP server、socket loop、文件播放器或浏览器自动化。[VERIFIED: AGENTS.md, 06-CONTEXT.md]
- 不要在核心库实现 Opus/H264 编解码器；示例层只做容器/帧边界解析和节奏调度。[VERIFIED: docs/000-设计边界记录.md]
- 不要实现 TURN、公网 NAT 成功判定、NACK 重传、RTX、拥塞控制、多路音视频或 DataChannel。[VERIFIED: REQUIREMENTS.md]
- 不要把自动化媒体质量判断做成 must-have；第 6 阶段自动化检查连接、状态、counter 和文件产生，VLC 播放由人工完成。[VERIFIED: 06-CONTEXT.md]
- 不要新增 GPL/LGPL 依赖；新增 npm 依赖必须记录许可证，新增 C 依赖必须可选且许可证宽松。[VERIFIED: AGENTS.md]

## Common Pitfalls

- **Chrome offer 中缺 m-line：** 页面必须先添加 canvas video track 和 oscillator audio track，再调用 `createOffer()`。[CITED: https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/addTrack]
- **mDNS host candidate：** 现代 Chrome 可能输出 `.local` host candidate；现有 C parser/ICE 层只接受 IP 字符串和 `0/1` 个 STUN IP:port。计划需要显式处理本机验收配置、Chrome flags 或候选过滤策略，避免把 DNS/mDNS 支持塞进核心库。[ASSUMED]
- **DTLS 真实 backend 缺口：** 第 4 阶段实现了 backend vtable 和 deterministic backend；真实 Chrome E2E 需要一个真实或可互通的 DTLS/SRTP backend 适配，否则 `dtls.connected` / `srtp.ready` 无法达成。Planner 必须把“参考 backend 或可跳过的依赖 gate”作为早期计划，而不是把它埋到最后。[VERIFIED: .planning/phases/04-dtls-srtp-secure-transport/04-06-SUMMARY.md]
- **异步 executor 语义：** 第 5 阶段验证保留了异步 executor advisory；E2E 示例若使用真实事件循环，应明确 API 返回值是“入队/调用成功”还是“协议动作完成”，并通过 JSONL 状态避免误判。[VERIFIED: .planning/phases/05-rtp-rtcp-media-plane/05-VERIFICATION.md]
- **`on_datagram` 生命周期：** observer 回调中的 datagram buffer 不能在回调后继续借用；示例必须复制到自己的发送 buffer 后再异步发送。[VERIFIED: docs/API-执行器与内存契约.md]
- **Ogg Opus 与 RTP Opus packet 混淆：** `sample1.opus` 不是裸 RTP payload 序列，必须先解析 Ogg page/lacing value 得到 Opus packet。[VERIFIED: file command]
- **自动化自证过强：** Playwright 能确认页面状态和 stats，但不应把“文件可播放”完全自动化为 must-have，避免引入系统播放器/解码器依赖。[VERIFIED: 06-CONTEXT.md]

## Code Examples

### public API 串联骨架

```c
rtc_peer_connection_set_remote_description(pc, offer, offer_len);
rtc_peer_connection_create_answer(pc, answer, &answer_len);
rtc_peer_connection_set_local_description(pc, answer, answer_len);
rtc_peer_connection_gather_candidates(pc);
rtc_peer_connection_add_ice_candidate(pc, candidate, candidate_len);
rtc_peer_connection_start_connectivity_checks(pc);
rtc_peer_connection_receive_datagram(pc, udp_buf, udp_len);
rtc_peer_connection_send_media_frame(pc, &frame);
```

### observer 映射

```c
static void on_datagram(void *user_data, const uint8_t *data, size_t len)
{
    /* 示例层必须复制 data；核心库只保证回调期间有效。 */
}

static void on_state(void *user_data, const char *state)
{
    /* JSONL: {"type":"state","state":"ice.connected"} */
}

static void on_media_frame_typed(void *user_data, const rtc_media_frame_t *frame)
{
    /* 按 frame->kind 写入接收媒体文件，并更新 summary counters。 */
}
```

### Chrome 页面媒体源

```js
const canvasStream = canvas.captureStream(25);
canvasStream.getVideoTracks().forEach((track) => pc.addTrack(track, canvasStream));

const audioContext = new AudioContext();
const oscillator = audioContext.createOscillator();
const destination = audioContext.createMediaStreamDestination();
oscillator.connect(destination);
oscillator.start();
destination.stream.getAudioTracks().forEach((track) => pc.addTrack(track, destination.stream));
```

## Validation Architecture

### 自动化层

- **Preflight:** `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 必须保持通过，锁定已有 SDP/ICE/DTLS/RTP/RTCP deterministic tests。[VERIFIED: CMakeLists.txt]
- **示例构建:** `cmake --build build --target rtc_chrome_e2e` 或 planner 选择的等价 target 必须生成 C 示例进程。[ASSUMED]
- **信令和页面:** `node examples/chrome_e2e/signaling.mjs --port 8080` 必须能服务 WebSocket 和静态页面；脚本退出时释放端口。[ASSUMED]
- **编排脚本:** `node examples/chrome_e2e/run_e2e.mjs --chrome-channel chrome --timeout-ms 30000` 启动 Chrome 页面和 C 示例，检查 SDP/ICE/DTLS/SRTP/RTP/RTCP/media file 分层状态。[ASSUMED]
- **日志门禁:** JSONL 至少包含 `signaling.connected`、`offer.received`、`answer.sent`、`ice.connected`、`dtls.connected`、`srtp.ready`、`rtp.sent`、`rtp.received`、`summary`；失败时 `summary.layer` 属于 `signaling|ice|dtls|srtp|rtp|rtcp|media_file`。[VERIFIED: 06-CONTEXT.md]

### 人工验收层

- 用户运行脚本后用 VLC 打开 C 示例输出的接收音视频文件，确认 Chrome 合成视频/音频内容可播放或至少有可诊断的媒体文件输出。[VERIFIED: 06-CONTEXT.md]
- 页面状态面板需可见显示 signaling、ICE/connection、track、candidate 和 stats 摘要，便于用户截图或人工判断。[VERIFIED: 06-CONTEXT.md]

## Recommended Plan Decomposition

1. **06-01：E2E harness 契约、目录、信令服务和页面骨架。** 交付 WebSocket 消息协议、静态页面、状态面板和 Playwright smoke，不接入核心库。
2. **06-02：C 示例运行时、WebSocket client、UDP pump、JSONL observer。** 交付用户侧 I/O harness 和 public API 串联，但可先使用 deterministic/reference backend gate。
3. **06-03：样本解析与双向媒体文件。** 交付 H264 Annex B access unit 切分、Ogg Opus packet 提取、按节奏发送、接收落盘和 counters。
4. **06-04：真实 DTLS/SRTP backend 或可选参考适配 gate。** 交付 Chrome 可互通安全 backend 的构建开关、许可证说明、失败分层和 deterministic fallback。若外部依赖不可自动安装，计划必须把它标为明确 manual/action gate，而不是假装 E2E 已可跑。
5. **06-05：端到端自动化编排和失败分层。** 交付 `run_e2e.mjs`、页面状态断言、JSONL summary 解析、媒体文件存在性和字节/frame 检查。
6. **06-06：文档、UAT、需求追踪和状态收口。** 交付中文运行说明、VLC 人工验收步骤、`TST-01`/`EXM-01`/`EXM-02`/`ACC-01` 追踪更新和 Phase 6 UAT。

## Open Questions for Planning

- 是否允许第 6 阶段引入可选 OpenSSL/libsrtp 参考 backend 作为示例/适配层依赖？第 4 阶段只禁止默认构建强制依赖，未禁止可选参考 backend。[VERIFIED: .planning/phases/04-dtls-srtp-secure-transport/04-06-SUMMARY.md]
- Chrome mDNS candidate 是否通过运行脚本配置/过滤解决，还是需要在 C 示例层提示用户使用本机 IP/Chrome flag？不得在核心库里悄悄扩大到 DNS resolver。[ASSUMED]
- 输出媒体文件首版采用可直接 VLC 播放的容器，还是 raw/packet 文件加明确播放说明？D-08 要求供 VLC 播放检查，因此 planner 应优先计划可播放输出或明确包含容器化任务。[VERIFIED: 06-CONTEXT.md]

## Research Complete

本阶段规划应把“真实互通前置风险”放在早期计划：Chrome 页面和信令较直接，真正的关键路径是 C 示例事件循环、UDP/socket pump、真实 DTLS/SRTP backend、Ogg/H264 样本解析、JSONL 分层诊断和自动化编排。核心库边界已经足够明确，计划不应新增核心库 socket/thread/codec 责任。
