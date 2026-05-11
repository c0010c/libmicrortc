# 第 6 阶段：Chrome 端到端验收 - 上下文

**Gathered:** 2026-05-11T12:43:28+08:00
**Status:** 准备进入规划

<domain>
## 阶段边界

本阶段交付真实 Chrome 1v1 音视频端到端验收：用本地 Chrome 页面、WebSocket 信令服务和 C 示例进程，把已实现的 SDP/JSEP、ICE/STUN、DTLS-SRTP、RTP/RTCP、typed media API、observer、trace 和 counters 串成可运行、可诊断、可人工确认媒体播放的验收路径。

本阶段不改变核心项目边界：库仍保持纯 C、固定内存、无线程，不创建 socket，不处理音视频编解码，不引入 GPL/LGPL 依赖。用户侧示例可以负责本机 UDP/socket 收发、WebSocket 信令、样本文件读取、媒体输出落盘和验收脚本编排，但这些能力不得变成核心库运行期动态依赖。

</domain>

<decisions>
## 实现决策

### 验收拓扑

- **D-01:** 第 6 阶段首个 happy path 固定为 **Chrome 主叫，C 示例接听**。Chrome 页面生成 offer、采集或生成媒体并发起 ICE；C 示例进程解析 offer、生成 answer，并驱动 ICE、DTLS/SRTP、RTP/RTCP 和 typed media API。
- **D-02:** Chrome 页面与 C 示例之间使用 **本机 WebSocket 信令 + C 示例自管 UDP socket**。WebSocket 信令只转发 SDP 和 trickle ICE candidate，不替核心库隐藏“用户负责 UDP/socket 收发”的边界。
- **D-03:** 首验收以本机或局域网 **host candidate** 为主；`0/1` 个 STUN IP:port 保持可选能力，但公网 NAT 穿透和外部 STUN 依赖不是第 6 阶段 must-have。
- **D-04:** 首验收要求 **双向音视频**。Chrome 向 C 示例发送合成音视频；C 示例向 Chrome 发送项目固定样本媒体。允许 C 侧使用测试媒体源，不要求接入真实编码器。

### 媒体样本策略

- **D-05:** C 示例发送给 Chrome 的媒体直接使用项目根目录的 `sample1.opus` 和 `test-25fps.h264`。Planner 必须把这两个文件作为阶段样本输入处理，而不是另造临时媒体 fixture。
- **D-06:** `sample1.opus` 当前是 Ogg Opus 文件；`test-25fps.h264` 是裸 H264 数据。示例层需要读取、切帧或解析到现有 encoded media API 可接受的 Opus frame / H264 access unit；不得把通用编解码器实现放入核心库。
- **D-07:** Chrome 侧媒体源使用浏览器合成媒体，而不是依赖真实摄像头/麦克风。页面应使用 canvas 产生可见视频图案，并使用 Web Audio oscillator 或等价浏览器能力产生音频，以保证验收可重复。
- **D-08:** C 示例收到 Chrome 媒体后，必须把接收到的音视频写入磁盘，供用户后续用 VLC 播放检查。不能只依赖日志或 counters 判断媒体内容。
- **D-09:** C 示例发送样本时按媒体固有节奏发送。H264 按 `test-25fps.h264` 的 25fps 节奏；Opus 按解析出的 packet/granule 时序，或在 parser 无法安全推导时使用稳定的 20ms packet 节奏。不得为 happy path 尽快倾倒全部样本。

### 失败诊断与自动化

- **D-10:** 自动化边界固定为：脚本启动信令服务、C 示例和 Chrome 页面，检查 SDP 交换、ICE connected、DTLS/SRTP ready、RTP/RTCP counters 增长、输出文件产生和关键页面状态；媒体播放质量由用户后续用 VLC 人工确认。
- **D-11:** C 示例必须输出结构化 **JSONL 事件** 和最终 summary。事件至少能记录阶段、状态、错误 detail、counter snapshot 或关键 trace 字段；summary 应给出 pass/fail 和失败层级。
- **D-12:** 验收脚本至少要分层识别 signaling、ICE、DTLS、SRTP、RTP、RTCP、media file 失败。更细粒度协议原因仍通过 observer error、trace reason、counter 和 JSONL 字段呈现，不要求全部变成脚本一级分类。
- **D-13:** Chrome 页面应显示紧凑状态面板，包括 signaling、ICE/connection 状态、media tracks、candidate 数量、bytes/frames 或等价 WebRTC stats 摘要。页面不应只显示 video/audio 控件，也不需要首版铺满全部 SDP/candidate debug 详情。

### 智能体的自由裁量

用户未锁定 WebSocket 消息字段全集、信令服务语言、C 示例 CLI 参数名、JSONL 字段精确命名、输出媒体文件扩展名、Chrome 页面目录结构、验收脚本使用的浏览器自动化工具、VLC 播放命令或 CMake target 命名。Planner 可以在不突破上述决策的前提下决定这些实现细节，但必须保持核心库纯 C、固定内存、无线程、用户负责 UDP/socket 收发，并避免 GPL/LGPL 依赖。

</decisions>

<canonical_refs>
## 权威引用

**下游智能体在规划或实现前必须阅读这些文档和样本。**

### 项目范围与需求

- `.planning/PROJECT.md`：项目价值、首版边界、Chrome 1v1 音视频互通目标、固定内存/无线程/用户 UDP/socket 边界和媒体编解码边界。
- `.planning/REQUIREMENTS.md`：第 6 阶段覆盖的 `TST-01`、`EXM-01`、`EXM-02`、`ACC-01` 需求及需求追踪。
- `.planning/ROADMAP.md`：第 6 阶段目标、成功标准和本阶段仍待开始的验收范围。
- `.planning/STATE.md`：当前项目状态、阶段验证状态和第 6 阶段准备信息。

### 已锁定的上游边界

- `.planning/phases/03-ice-stun-datagram-network-layer/03-CONTEXT.md`：显式 network API、host/srflx、trickle ICE、`observer.on_datagram`、datagram 输入生命周期和 STUN/DTLS/RTP/RTCP demux 边界。
- `.planning/phases/04-dtls-srtp-secure-transport/04-CONTEXT.md`：单一 `security_backend`、ICE connected 后自动 DTLS、fingerprint 校验、`srtp.ready`、SRTP/SRTCP protect/unprotect 失败语义。
- `.planning/phases/05-rtp-rtcp-media-plane/05-CONTEXT.md`：typed media API、Opus/H264 RTP payload、RTCP SR/RR/SDES、PLI/NACK、media/network executor 分层和 Chrome 验收留到第 6 阶段的边界。
- `docs/000-设计边界记录.md`：纯 C、固定内存、无线程、用户负责 UDP/socket、H264/Opus 编码帧级边界、RTCP/NACK 语义和依赖策略。
- `docs/API-执行器与内存契约.md`：executor 亲和、arena/limits、observer/trace/counter、datagram buffer 生命周期，以及第 3/4/5 阶段网络、安全和媒体契约。

### 媒体样本

- `sample1.opus`：C 示例发送给 Chrome 的固定 Opus 样本输入；当前文件识别为 Ogg Opus，stereo，48000 Hz。
- `test-25fps.h264`：C 示例发送给 Chrome 的固定 H264 样本输入；按 25fps 节奏发送。

### 现有代码入口

- `include/rtc/peer_connection.h`：已有 offer/answer、description、candidate、gather/checks、datagram、media frame、keyframe 和 counters API。
- `include/rtc/media.h`：已有 `rtc_media_frame_t`、`rtc_media_feedback_t`、Opus/H264 media kind 和 PLI/NACK feedback 类型。
- `include/rtc/observer.h`：已有 `on_local_candidate`、`on_datagram`、`on_media_frame_typed`、`on_media_feedback`、`on_state`、`on_error`、`on_trace` 回调。
- `examples/create_destroy.c`：当前唯一示例，只覆盖 create/destroy；第 6 阶段需要新增真实 Chrome 端到端示例，而不是扩展核心库边界。
- `CMakeLists.txt`：已有 `RTC_BUILD_EXAMPLES` 和测试 target；新增示例和验收 target 应接入现有构建风格。
- `tests/test_rtp.c`、`tests/test_rtcp.c`、`tests/test_security.c`、`tests/test_ice.c`：已有 deterministic 本地协议测试，可作为端到端失败分层和 counter 期望的参考。

</canonical_refs>

<code_context>
## 既有代码洞察

### 可复用资产

- `rtc_peer_connection_gather_candidates`、`rtc_peer_connection_start_connectivity_checks`、`rtc_peer_connection_receive_datagram` 已形成用户自管 UDP/socket 的集成边界；C 示例应围绕这些 API 做 socket pump。
- `observer.on_datagram` 已作为所有待发送 UDP payload 的统一出口；C 示例应在回调内或自行复制后发送到 selected/目标 UDP 地址。
- `rtc_peer_connection_send_media_frame` 和 `rtc_peer_connection_request_keyframe` 已提供 C 侧发送 encoded media 与触发 PLI 的 public API。
- `observer.on_media_frame_typed` 和 `observer.on_media_feedback` 已提供 C 侧接收 Chrome media/feedback 的出口，适合落盘和 JSONL 事件输出。
- 现有 CTest 测试覆盖 SDP/JSEP、ICE/STUN、DTLS/SRTP wrapper、RTP/RTCP 和 media API，可作为第 6 阶段自动化 preflight。

### 既有模式

- Signaling API 在 `RTC_EXECUTOR_SIGNALING`，media API 在 `RTC_EXECUTOR_MEDIA`，network/datagram/ICE/DTLS/SRTP 在 `RTC_EXECUTOR_NETWORK`；示例必须保持这些亲和语义清楚可见。
- 创建成功后运行期不得动态增长；示例可以有自己的 I/O buffer 和文件 buffer，但核心库仍必须通过 create-time arena/limits 工作。
- 公共 status 保持粗粒度，细节通过 observer error、trace reason、detail code、counters 和示例层 JSONL 暴露。
- Markdown 文档使用中文；技术标识符、API 名称、协议字段、trace event、JSON 字段和需求 ID 可以保留英文。

### 集成点

- 新增 Chrome 页面、WebSocket 信令服务、C 端 E2E 示例和自动化脚本应放在 `examples/`、`tests/` 或 planner 选定的清晰目录中，并通过 CMake/npm/脚本入口让用户容易运行。
- C 示例需要把 WebSocket 信令消息转成 `setRemoteDescription`、`createAnswer`、`setLocalDescription`、`addIceCandidate` 等 API 调用，同时把本地 candidate 和 answer 通过信令发给 Chrome。
- C 示例需要实现 UDP socket 收发循环：收到 UDP datagram 后调用 `rtc_peer_connection_receive_datagram`；`observer.on_datagram` 输出后由示例发送到远端 candidate / selected pair 对应地址。
- Chrome 页面需要把 `RTCPeerConnection` 状态、candidate 数量、track 状态和 stats 摘要呈现在紧凑状态面板中，供自动化和人工验收读取。

</code_context>

<specifics>
## 具体想法

- 用户明确选择讨论验收拓扑、媒体样本策略、失败诊断与自动化，未选择继续深挖信令消息细节；信令 JSON 形状留给 planner 裁量。
- 用户明确要求 C 侧直接使用项目根目录下的 `sample1.opus` 和 `test-25fps.h264`，而不是另行生成或要求外部输入。
- 用户希望 C 示例把接收到的音视频文件写入磁盘，后续由用户用 VLC 播放确认。
- 自动化验收应服务于快速定位失败层级；媒体内容最终是否可播放由人工 VLC 检查闭环。

</specifics>

<deferred>
## 延后事项

- Chrome 接听、C 示例主叫方向：第 6 阶段首个 happy path 不要求首批覆盖，可在基础路径稳定后扩展。
- 必须通过公网 NAT / 外部 STUN srflx 验收：不作为第 6 阶段 must-have，STUN 只保持可选路径。
- 全自动音视频内容解码校验：不作为首版成功标准，避免引入播放器/解码器/平台依赖。
- 真实摄像头/麦克风采集验收：当前选择浏览器合成媒体作为默认可重复路径，真实设备可作为后续手工增强。

</deferred>

---

*阶段：6-Chrome 端到端验收*
*上下文收集时间：2026-05-11T12:43:28+08:00*
