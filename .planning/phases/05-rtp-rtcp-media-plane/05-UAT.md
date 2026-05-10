# 第 5 阶段 UAT 清单：RTP/RTCP 媒体平面

本文用于阶段级验收第 5 阶段交付的 typed media/RTP/RTCP 行为。第 5 阶段只覆盖本地 deterministic tests 可验证的媒体平面；Chrome 页面、信令示例和真实 1v1 音视频端到端验收仍属于第 6 阶段。

## 验收边界

- 库保持纯 C、固定内存、无线程，用户负责 UDP/socket 收发。
- 用户提交 Opus frame 或 H264 access unit；库负责 RTP payload format、timestamp、sequence、SRTP/SRTCP protect/unprotect 和 RTCP 基础反馈。
- 用户负责音视频编解码、H264 关键帧生成、socket 发送、socket 接收和执行器调度。
- v1 单个 `PeerConnection` 最多 1 路 Opus audio 和 1 路 H264 video。
- v1 不做 RTP NACK 重传、RTX、拥塞控制、发送节奏控制、多路媒体或 Chrome 真实 E2E。

## 必验项目

| 项目 | 用户可验收行为 | 期望结果 |
|------|----------------|----------|
| Opus 发送 | 在 media executor 调用 `rtc_peer_connection_send_media_frame`，传入 `RTC_MEDIA_KIND_AUDIO_OPUS` frame | 库生成 Opus RTP packet，调用 SRTP protect，成功后通过 network executor 的 `observer.on_datagram` 输出受保护 datagram |
| Opus 接收 | 在 network executor 调用 `rtc_peer_connection_receive_datagram` 输入受保护 Opus RTP datagram | SRTP unprotect 成功后，media executor 通过 `observer.on_media_frame_typed` 输出 `RTC_MEDIA_KIND_AUDIO_OPUS` frame |
| H264 single NALU 发送 | 提交可放入单个 RTP payload 的 H264 access unit | 库输出 single NALU RTP packet，marker/timestamp/sequence 可通过测试 observer 和 counters 验证 |
| H264 FU-A 发送 | 提交超过固定 RTP payload 上限但可分片的 H264 access unit | 库按 FU-A 分片输出多个受保护 RTP datagram，最后一片设置 marker |
| H264 single NALU 接收 | 输入受保护 H264 single NALU RTP datagram | SRTP unprotect 成功后，media executor 输出 `RTC_MEDIA_KIND_VIDEO_H264` H264 access unit |
| H264 FU-A 接收 | 输入连续 FU-A 分片 | 库在固定 reassembly buffer 中重组 H264 access unit 并输出 typed video frame |
| H264 STAP-A 接收 | 输入合法 STAP-A RTP payload | 库拆出其中 NALU 并纳入 H264 access unit 输出；STAP-A 长度越界时不输出媒体帧 |
| RTCP SR/RR/SDES | 触发或输入 SR/RR/SDES compound RTCP | 库维护 SR/RR/SDES codec、基础 stats 和 counters；SRTCP protect/unprotect 失败时不解析或输出 |
| PLI 发送 | 在 media executor 调用 `rtc_peer_connection_request_keyframe(pc, RTC_MEDIA_KIND_VIDEO_H264)` | 库生成 RTCP PLI，SRTCP protect 成功后通过 `observer.on_datagram` 输出受保护 datagram |
| PLI 接收 | 输入受保护远端 PLI | SRTCP unprotect 成功后，media executor 通过 `observer.on_media_feedback` 上报 `RTC_MEDIA_FEEDBACK_PLI`；用户负责让编码器产生关键帧 |
| NACK 接收 | 输入受保护 Generic NACK | 库解析 PID/BLP，展开固定 17 项丢包序号，通过 `observer.on_media_feedback` 上报 `RTC_MEDIA_FEEDBACK_NACK` |
| NACK 不重传 | 输入远端 NACK 后观察 outgoing RTP datagram 和 feedback | NACK 不触发重传；`retransmit_performed = 0`，counter/trace 包含 `nack_no_retransmit` |
| 容量错误 | 使用过小的 RTP payload、packet cache、media queue、H264 reassembly 或 RTCP SDES limit | API 返回容量相关错误，observer/trace/counter 可诊断，不动态扩容 |
| executor 亲和 | 在错误 executor 调用 media API 或 network API | 返回 `RTC_STATUS_AFFINITY_VIOLATION`；media frame callback 在 media executor，datagram callback 在 network executor |
| buffer 生命周期 | 在回调返回后不再使用库提供的 frame/datagram 指针 | 文档和 API 契约要求用户如需异步处理必须自行复制，库不保存调用方 buffer 指针 |
| SRTP protect 失败不泄漏 | 配置 deterministic backend 让 RTP/RTCP protect 失败 | 不调用 `observer.on_datagram` 输出明文 RTP/RTCP；错误通过 status、observer error、trace 和 counter 表达 |
| SRTP unprotect/replay 失败不输出 | 输入 unprotect、认证或 replay 失败的 RTP/RTCP datagram | 不调用 `observer.on_media_frame_typed`，不调用 `observer.on_media_feedback` 输出未认证媒体或 feedback |

## 自动化验收命令

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

阶段级验证还需要确认文档没有宣称 Chrome 端到端已完成：

```bash
! grep -R "Chrome.*端到端.*已完成" .planning docs
```

## 与第 6 阶段的分界

- 第 5 阶段完成 typed media API、RTP/RTCP 媒体平面、本地 deterministic tests 和中文文档收口。
- 第 6 阶段仍负责本地 Chrome 页面、最小信令示例、真实 1v1 音视频通话验收、端到端失败排障体验和 `ACC-01`。
