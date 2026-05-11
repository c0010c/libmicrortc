# 第 6 阶段：Chrome 端到端验收 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-11T12:43:28+08:00
**Phase:** 6-Chrome 端到端验收
**Areas discussed:** 验收拓扑、媒体样本策略、失败诊断与自动化

---

## 验收拓扑

| Option | Description | Selected |
|--------|-------------|----------|
| Chrome 主叫，C 示例接听 | Chrome 负责 `createOffer`、采集浏览器媒体、发起 ICE；C 示例生成 answer 并收发 datagram。 | ✓ |
| C 示例主叫，Chrome 接听 | 优先验证库生成 offer 被 Chrome 接受；对 SDP writer 和本地 candidate 输出更敏感。 | |
| 两种都必须作为第 6 阶段首批验收 | 覆盖最完整，但计划会更大，调试时也更难隔离方向性问题。 | |

**User's choice:** Chrome 主叫，C 示例接听。
**Notes:** 首个 happy path 固定为 Chrome 主叫、C 示例接听。

| Option | Description | Selected |
|--------|-------------|----------|
| 本机 WebSocket 信令 + C 示例自管 UDP socket | 一个最小本机信令服务转发 SDP/candidate；C 示例进程自己用 UDP socket 收发，再把 datagram 喂给库。 | ✓ |
| 单一 C 示例同时承载信令和 UDP | C 示例内置 HTTP/WebSocket 或等价信令服务，Chrome 页面直接连它。 | |
| 手动复制 SDP/candidate，UDP 只做本机固定端口 | 最小实现成本，但不太能验证 trickle ICE 和真实验收体验。 | |

**User's choice:** 本机 WebSocket 信令 + C 示例自管 UDP socket。
**Notes:** 信令服务只转发 SDP/candidate，不隐藏用户负责 UDP/socket 收发的边界。

| Option | Description | Selected |
|--------|-------------|----------|
| 本机/局域网 host candidate 为主，STUN 可选 | 第 6 阶段首验收跑 localhost 或 LAN host candidate；配置 1 个 STUN IP:port 时也可走 srflx。 | ✓ |
| 必须验证 STUN srflx 路径 | 要求本阶段提供可重复的 STUN server 配置和 srflx 验收。 | |
| 只做 localhost host-only | 最稳定、最小，但对 Chrome 真实 ICE 路径覆盖偏窄。 | |

**User's choice:** 本机/局域网 host candidate 为主，STUN 可选。
**Notes:** 公网 NAT 穿透不作为 must-have。

| Option | Description | Selected |
|--------|-------------|----------|
| 双向音视频，但允许 C 侧使用测试媒体源 | Chrome 发真实或合成媒体给 C；C 侧发送可识别的 Opus/H264 测试帧回 Chrome。 | ✓ |
| 先只验证 Chrome -> C 接收路径 | 更容易定位问题，但不能证明 C 侧发送被 Chrome 正常播放。 | |
| 先只验证 C -> Chrome 发送路径 | 聚焦库发送链路和 Chrome 播放，但无法验证 Chrome 媒体输入到库。 | |

**User's choice:** 双向音视频，但允许 C 侧使用测试媒体源。
**Notes:** 第 6 阶段首验收需要覆盖双向音视频。

---

## 媒体样本策略

| Option | Description | Selected |
|--------|-------------|----------|
| 内置小型预编码测试媒体 | 示例携带或生成短周期 Opus/H264 测试帧，循环发送给 Chrome。 | |
| 要求用户提供外部 H264/Opus 文件 | 示例读取用户给的 media fixture。 | ✓ |
| 接入真实编码器作为示例依赖 | 最接近真实产品，但会引入依赖和许可/平台复杂度。 | |

**User's choice:** 直接使用项目目录下的 `sample1.opus` 和 `test-25fps.h264`。
**Notes:** 这两个文件位于仓库根目录；`sample1.opus` 当前识别为 Ogg Opus，`test-25fps.h264` 是裸 H264 数据。

| Option | Description | Selected |
|--------|-------------|----------|
| Chrome 使用真实摄像头/麦克风 | 最贴近用户验收，但需要浏览器授权设备。 | |
| Chrome 使用浏览器合成媒体 / canvas + oscillator | 更可重复，适合无摄像头/麦克风环境。 | ✓ |
| 两者都支持，默认合成媒体 | 自动化/无设备环境更稳，同时保留真实设备手工验收。 | |

**User's choice:** Chrome 使用浏览器合成媒体 / canvas + oscillator。
**Notes:** 页面应生成可重复音视频源。

| Option | Description | Selected |
|--------|-------------|----------|
| 写入最小 dump 文件 + 输出 counters/trace | 把收到的 typed frames 写到输出文件或统计摘要，同时打印 frame count、bytes、timestamp。 | ✓ |
| 只靠实时日志/counters 判断 | 实现最小，但不方便复现或检查媒体内容。 | |
| C 示例尝试回环收到的媒体给 Chrome | 可以证明接收和发送串联，但更容易混合问题。 | |

**User's choice:** 将音视频文件写入磁盘，后面用 VLC 播放。
**Notes:** C 侧接收媒体不能只停在日志计数。

| Option | Description | Selected |
|--------|-------------|----------|
| 按媒体固有节奏发送 | H264 按 25fps；Opus 按解析出的 Opus packet/granule 或固定 20ms packet 节奏。 | ✓ |
| 尽快发送全部样本 | 适合压力测试，但 Chrome 播放体验和 RTP timing 不真实。 | |
| 手动步进/按键发送 | 方便调试单帧问题，但不适合端到端 happy path。 | |

**User's choice:** 按媒体固有节奏发送。
**Notes:** H264 按 `test-25fps.h264` 的 25fps；Opus 按解析出的时序或稳定 20ms packet 节奏。

---

## 失败诊断与自动化

| Option | Description | Selected |
|--------|-------------|----------|
| 自动化覆盖到脚本启动服务 + Chrome 页面状态检查，媒体播放人工确认 | 自动脚本启动信令/C 示例/Chrome，检查 SDP、ICE、DTLS/SRTP、RTP/RTCP counters 和输出文件；VLC 播放人工确认。 | ✓ |
| 只提供手动 UAT 步骤 | 实现更轻，但回归成本高，失败难稳定复现。 | |
| 全自动媒体内容校验 | 尝试自动验证音视频内容可播放/可解码；覆盖强，但引入播放器/解码器/平台依赖。 | |

**User's choice:** 自动化覆盖到脚本启动服务 + Chrome 页面状态检查，媒体播放人工确认。
**Notes:** VLC 播放结果由用户人工验收。

| Option | Description | Selected |
|--------|-------------|----------|
| C 示例输出结构化 JSONL 事件 + 最终 summary | 每行记录阶段、状态、错误 detail、counter snapshot 或关键 trace 字段；结束时输出 summary。 | ✓ |
| 纯文本日志即可 | 更快做，但自动脚本难以稳定判断失败阶段。 | |
| 只依赖现有 observer/trace 回调，不新增示例层格式 | 保持核心最小，但第 6 阶段验收体验会比较散。 | |

**User's choice:** C 示例输出结构化 JSONL 事件 + 最终 summary。
**Notes:** 自动脚本和人工都要能读。

| Option | Description | Selected |
|--------|-------------|----------|
| 分层识别 ICE / DTLS / SRTP / RTP / RTCP / media file | 脚本至少能区分信令失败、ICE 未连接、DTLS 未完成、SRTP 未 ready、RTP/RTCP counter 不增长、媒体输出文件未产生或为空。 | ✓ |
| 只识别端到端成功/失败 | 最简单，但调试体验较差。 | |
| 识别到更细粒度协议原因 | 覆盖更强，但计划更大。 | |

**User's choice:** 分层识别 ICE / DTLS / SRTP / RTP / RTCP / media file。
**Notes:** 细粒度协议原因继续通过 JSONL、trace、counter 和 observer detail 提供。

| Option | Description | Selected |
|--------|-------------|----------|
| 显示紧凑状态面板 | 页面显示 signaling、ICE、connection、media tracks、candidate count、bytes/frames 等核心状态。 | ✓ |
| 页面只显示视频/audio 控件，诊断都在 C 示例日志 | 页面更干净，但浏览器侧问题不容易看。 | |
| 页面显示详细 debug console | 所有 candidate、SDP、stats 都铺出来；调试强，但页面复杂度更高。 | |

**User's choice:** 显示紧凑状态面板。
**Notes:** 页面状态用于手工验收和自动化读取。

## 智能体的自由裁量

- WebSocket 消息字段全集、信令服务语言、C 示例 CLI 参数名、JSONL 字段精确命名、输出媒体文件扩展名、Chrome 页面目录结构、验收脚本工具和 CMake target 命名由 planner 决定。

## Deferred Ideas

- Chrome 接听、C 示例主叫方向。
- 必须通过公网 NAT / 外部 STUN srflx 的验收。
- 全自动音视频内容解码校验。
- 真实摄像头/麦克风采集验收。
