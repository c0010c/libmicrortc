# 第 5 阶段：RTP/RTCP 媒体平面 - 讨论日志

> **仅供审计。** 不要作为规划、研究或执行智能体输入。
> 决策已写入 `05-CONTEXT.md`；本文只保留备选方案和讨论过程。

**Date:** 2026-05-10T23:00:00+08:00
**Phase:** 5-RTP/RTCP 媒体平面
**Areas discussed:** 媒体帧 API 形状、H264 与 Opus 打包边界、RTCP 与反馈语义、媒体执行器与 datagram 路由

---

## 媒体帧 API 形状

| Option | Description | Selected |
|--------|-------------|----------|
| Typed frame API | 新增独立 `send_opus_frame` / `send_h264_access_unit` API；接收方向新增 typed media 回调。 | |
| Single generic media API | 单一 `send_media_frame(kind, data, metadata)` 入口，音频/视频由参数区分。 | ✓ |
| 最小改动 API | 发送方向新增最少函数，接收方向复用 `observer.on_media_frame(data,len)`。 | |

**User's choice:** 用户询问 “api 能统一吗?”，随后接受统一 API 方案。

**Notes:** 最终锁定为 typed generic API：公共入口统一，但 frame 结构必须包含稳定 media kind；v1 只接受 Opus 和 H264，接收方向也应使用 typed media observer。

---

## H264 与 Opus 打包边界

| Option | Description | Selected |
|--------|-------------|----------|
| 用户提交完整编码帧，库负责 RTP payload format | Opus 一次一个 frame；H264 一次一个 access unit。库负责 single NALU / FU-A 发送、FU-A 重组、有限 STAP-A 接收。 | ✓ |
| 用户提交更底层的 payload 单元 | Opus frame 直接发；H264 由用户逐个提交 NALU，库只负责单 NALU 或 FU-A。 | |
| 用户预先 packetize，库只做 SRTP 和发送 | 用户传入已组好的 RTP payload 或 RTP packet，库少做媒体逻辑。 | |

**User's choice:** 1。

**Notes:** 用户要求先看选项，再选择推荐方案。锁定 access unit / frame 级输入，库承担 RTP payload format、timestamp、sequence、single NALU、FU-A 和有限 STAP-A 接收。

---

## RTCP 与反馈语义

| Option | Description | Selected |
|--------|-------------|----------|
| 库自动维护基础 RTCP，用户显式请求关键反馈 | 库自动生成 SR/RR/SDES；用户显式请求 PLI；NACK 只上报不重传。 | ✓ |
| RTCP 全部由用户显式驱动 | 用户调用 API 发送 SR/RR/SDES/PLI，库只负责编码和 SRTP。 | |
| RTCP 尽量自动化，不给用户显式 PLI API | 库自动发 SR/RR/SDES，也根据丢包或 H264 重组失败自动发 PLI。 | |

**User's choice:** 1。

**Notes:** 锁定基础 RTCP 由库维护，PLI 由用户显式触发；收到 NACK 只 observer/trace/counter 上报，不执行重传。

---

## 媒体执行器与 datagram 路由

| Option | Description | Selected |
|--------|-------------|----------|
| 严格 executor 分层 | media executor 处理媒体帧输入输出和 packetize/depacketize；network executor 处理 datagram、SRTP/SRTCP 和 `on_datagram`。 | ✓ |
| 媒体 API 也要求 network executor | 用户在 network executor 调媒体发送 API，库同步 packetize + protect + 输出 datagram。 | |
| 发送在 media，接收回调留在 network | 发送方向保持 media API；接收方向直接在 network 回调媒体帧。 | |

**User's choice:** 用户要求推荐理由，随后选择 1。

**Notes:** 推荐理由是项目核心边界已经要求 media 负责编码帧输入输出、network 负责 datagram/ICE/DTLS/SRTP。锁定跨 executor 投递和固定内存队列/槽语义由 planner 设计。

---

## 智能体的自由裁量

- 具体 API 函数名、frame 字段名、media kind 枚举名、timestamp metadata 类型由 planner 决定。
- RTP/RTCP 内部文件拆分、RTCP 周期、trace event 常量、counter 字段和测试 fixture 命名由 planner 决定。
- planner 必须保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发，并避免 GPL/LGPL 依赖。

## 延后事项

- NACK 触发 RTP 重传。
- 拥塞控制或发送节奏控制闭环。
- 多路音频或多路视频。
- Opus repacketization、多帧聚合、FEC/DTX 策略。
- 通用 codec 协商和更多 codec。
- Chrome 端到端页面和信令示例验收。
