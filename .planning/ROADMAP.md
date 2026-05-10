# 路线图：WebRTC 纯 C 库

**创建日期：** 2026-05-10
**项目模式：** 水平层
**粒度：** 标准
**核心价值：** 在固定内存、无线程、跨平台约束下，稳定完成与 Chrome 的 1v1 音视频 `PeerConnection` 互通。

## 概览

| 阶段 | 名称 | 目标 | 需求 | 状态 |
|------|------|------|------|------|
| 1 | 核心骨架与边界契约 | 建立纯 C API、固定内存、执行器、observer、构建和基础可观测性 | API-01, API-02, API-03, API-05, MEM-01, MEM-02, MEM-03, EXE-01, EXE-02, EXE-03, OBS-01, OBS-02, OBS-03, BLD-01, BLD-02 | 已完成 |
| 2 | SDP/JSEP 与 Offer/Answer | 生成和解析 Chrome 1v1 最小 SDP，并完成发起/接听信令 API | SDP-01, SDP-02, SDP-03, SDP-04, API-04 | 待开始 |
| 3 | ICE/STUN 与 Datagram 网络层 | 实现 host/srflx、Full ICE、trickle ICE、状态事件和 datagram demux | SDP-05, ICE-01, ICE-02, ICE-03, ICE-04, ICE-05, NET-01, NET-02, NET-03 | 待开始 |
| 4 | DTLS-SRTP 安全传输 | 建立安全 backend vtable，完成 DTLS fingerprint、key export 和 SRTP/SRTCP 保护 | SEC-01, SEC-02, SEC-03, SEC-04, SEC-05 | 待开始 |
| 5 | RTP/RTCP 媒体平面 | 支持 Opus、H264、SR/RR、SDES、PLI、NACK 上报和媒体可观测性 | RTP-01, RTP-02, RTP-03, RTP-04, RTP-05, RTCP-01, RTCP-02, RTCP-03, RTCP-04, OBS-04 | 待开始 |
| 6 | Chrome 端到端验收 | 完成测试、Chrome 页面、信令示例和 1v1 音视频通话验收 | TST-01, EXM-01, EXM-02, ACC-01 | 待开始 |

## 阶段详情

### 第 1 阶段：核心骨架与边界契约

**目标：** 建立所有后续协议层必须遵守的公共边界：纯 C API、固定内存、执行器亲和、observer、构建目标和基础 trace/计数器。

**需求：** API-01, API-02, API-03, API-05, MEM-01, MEM-02, MEM-03, EXE-01, EXE-02, EXE-03, OBS-01, OBS-02, OBS-03, BLD-01, BLD-02

**成功标准：**
1. 用户可以用 CMake 构建静态库，并包含公共头文件创建/销毁 `PeerConnection`。
2. 创建阶段根据 arena 和 limits 完成固定内存切分，容量不足时返回可诊断错误。
3. 三类 executor vtable 可以被测试 executor 驱动，库内部没有线程创建行为。
4. observer、计数器和 trace hook 能输出生命周期、错误和基础状态事件。
5. 公共 API 文档明确所有函数和回调的执行器亲和规则。

**计划：**

**Wave 1**
- `01-01-PLAN.md`：构建骨架、公共头入口与测试基础。已完成，见 `01-01-SUMMARY.md`。
- `01-02-PLAN.md`：固定 arena、limits、容量诊断与 allocator 防线。已完成，见 `01-02-SUMMARY.md`。

**Wave 2** *(blocked on Wave 1 completion)*
- `01-03-PLAN.md`：执行器亲和、`PeerConnection` 生命周期与未支持 API 占位。已完成，见 `01-03-SUMMARY.md`。

**Wave 3** *(blocked on Wave 2 completion)*
- `01-04-PLAN.md`：observer、trace、计数器、文档与最小示例。已完成，见 `01-04-SUMMARY.md`。

**跨计划约束：**
- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发。
- 不引入 GPL/LGPL 依赖或第三方测试框架。
- 第 1 阶段不得提前实现 SDP、ICE、DTLS-SRTP、RTP/RTCP 或 Chrome 页面。
- 所有执行计划必须保留 CONTEXT.md 中 D-01 到 D-20 的决策可追踪性。

### 第 2 阶段：SDP/JSEP 与 Offer/Answer

**目标：** 完成 Chrome 1v1 最小 SDP/JSEP 模型，让用户可以通过接近浏览器 WebRTC 的 API 完成 offer/answer 描述生成和设置。

**需求：** SDP-01, SDP-02, SDP-03, SDP-04, API-04

**成功标准：**
1. `createOffer` 可以生成包含 BUNDLE、rtcp-mux、ICE 参数、DTLS fingerprint/setup、H264、Opus 的 Chrome 可接受 SDP。
2. `createAnswer` 可以基于 Chrome offer 生成 Chrome 可接受 answer。
3. `setLocalDescription` 和 `setRemoteDescription` 能推进 JSEP 状态并拒绝非法状态转换。
4. SDP golden tests 覆盖 Chrome offer、Chrome answer、本地 offer、本地 answer 和错误字段。
5. `sendrecv` 和 `recvonly` 方向被正确解析、保存和输出。

**计划：**

**Wave 1**
- `02-01-PLAN.md`：SDP/JSEP 公共契约、create-time SDP 参数、固定 arena 切分、CMake/test 骨架。

**Wave 2** *(blocked on Wave 1 completion)*
- `02-02-PLAN.md`：固定 Chrome 1v1 SDP writer、本地 offer/answer golden 输出和 buffer 错误路径。
- `02-03-PLAN.md`：Chrome SDP parser、profile validator、成功 fixture 和错误字段 fixture。

**Wave 3** *(blocked on Wave 2 completion)*
- `02-04-PLAN.md`：Offer/Answer API 接入、最小 JSEP 状态机、`addIceCandidate` 保存和文档更新。

**跨计划约束：**
- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发。
- 不引入 GPL/LGPL 依赖或第三方测试框架。
- 第 2 阶段不得实现 ICE connectivity checks、STUN、candidate gathering、DTLS 握手、SRTP、RTP/RTCP 或 Chrome 端到端通话。
- 所有执行计划必须保留 `02-CONTEXT.md` 中 D-01 到 D-16 的决策可追踪性。

### 第 3 阶段：ICE/STUN 与 Datagram 网络层

**目标：** 实现网络连通性基础，让库可以通过用户提供的 UDP datagram 完成 ICE gathering、connectivity checks、trickle candidate 和协议 demux。

**需求：** SDP-05, ICE-01, ICE-02, ICE-03, ICE-04, ICE-05, NET-01, NET-02, NET-03

**成功标准：**
1. 库可以生成 host candidate，并通过 STUN server 发现 server reflexive candidate。
2. Full ICE 检查列表、角色、提名和失败超时在固定 limits 内工作。
3. trickle ICE 本地候选通过 observer 输出，远端候选可增量输入。
4. 用户输入 UDP datagram 后，库能区分 STUN、DTLS、RTP 和 RTCP 并路由到对应子系统。
5. ICE 状态、candidate pair、STUN transaction 和失败原因都有 trace 或计数器输出。

### 第 4 阶段：DTLS-SRTP 安全传输

**目标：** 通过可插拔 backend vtable 建立 DTLS-SRTP 安全通道，不让核心 API 绑定特定第三方 TLS/SRTP 实现。

**需求：** SEC-01, SEC-02, SEC-03, SEC-04, SEC-05

**成功标准：**
1. DTLS backend vtable 可以由测试后端和至少一个参考后端驱动握手。
2. 远端 fingerprint 与 DTLS 证书不一致时，连接失败并输出稳定错误事件。
3. DTLS 握手完成后可以导出 SRTP keying material，并初始化 SRTP/SRTCP 上下文。
4. RTP/RTCP protect 和 unprotect 通过 SRTP backend vtable 完成。
5. backend 错误被映射为稳定 `rtc_status_t`、observer 错误和 trace 字段。

### 第 5 阶段：RTP/RTCP 媒体平面

**目标：** 完成 1 路 Opus 音频和 1 路 H264 视频的 RTP/RTCP 媒体平面，并把关键反馈事件暴露给用户。

**需求：** RTP-01, RTP-02, RTP-03, RTP-04, RTP-05, RTCP-01, RTCP-02, RTCP-03, RTCP-04, OBS-04

**成功标准：**
1. 用户提交 Opus frame 后，库可以生成受 SRTP 保护的 RTP datagram；接收方向可以输出 Opus frame。
2. 用户提交 H264 access unit 后，库支持 single NALU 和 FU-A 发送；接收方向支持 access unit 重组和有限 STAP-A。
3. 单个 `PeerConnection` 的媒体规模被限制为最多 1 路音频和 1 路视频，超限时返回可诊断错误。
4. SR/RR、SDES 和 PLI 能正确发送、接收并更新计数器。
5. NACK 被解析并通过 observer 上报，但不会触发重传；文档和 trace 明确这一语义。

### 第 6 阶段：Chrome 端到端验收

**目标：** 把所有层集成成可验收的 `PeerConnection`，通过本地 Chrome 页面和信令示例完成 1v1 音视频通话。

**需求：** TST-01, EXM-01, EXM-02, ACC-01

**成功标准：**
1. 自动化测试覆盖 SDP/JSEP、ICE/STUN、DTLS/SRTP、RTP/RTCP、固定内存和关键错误路径。
2. 本地 Chrome 页面可以作为发起方或接听方交换 SDP 和 trickle ICE candidate。
3. 最小信令示例可以完成本地页面与 C 库示例进程之间的消息交换。
4. 用户可以完成 1v1 音视频通话，并观察到 ICE、DTLS、SRTP、RTP/RTCP 和媒体事件。
5. 端到端失败时，trace、计数器和错误事件足以定位失败阶段。

## 覆盖验证

| 指标 | 数量 |
|------|------|
| v1 需求总数 | 48 |
| 已映射需求 | 48 |
| 未映射需求 | 0 |
| 阶段总数 | 6 |

---
*最后更新：2026-05-10，第 2 阶段规划后*
