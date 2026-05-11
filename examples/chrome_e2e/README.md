# Chrome E2E 验收 Harness

本目录提供第 6 阶段本地 Chrome 端到端验收的浏览器侧 harness：

- `signaling.mjs` 同时提供静态页面 HTTP 服务和 `/ws` WebSocket 信令服务。
- `run_e2e.mjs` 提供 dry-run、页面 smoke 和后续完整 E2E 编排入口。
- `rtc_chrome_e2e` 是 C 侧示例进程，负责示例层 WebSocket client、UDP socket pump、`PeerConnection` public API 串联和 JSONL observer 输出。
- `page/` 放置原生 HTML/CSS/JS 页面；页面只使用 canvas 与 Web Audio 合成媒体，不请求摄像头或麦克风权限。

## 信令边界

WebSocket 信令只转发同一 `runId` 内的 JSON 消息，允许类型为 `hello`、`offer`、`answer`、`candidate`、`status`、`summary` 和 `error`。服务拒绝非 JSON、未知 `type`、二进制帧，以及包含 `media`、`payload` 或 `binary` 字段的消息。

信令服务不代理 UDP，不承载 RTP/RTCP/DTLS/SRTP 媒体 payload，也不替代用户侧 socket 收发责任。

## C 示例边界

`rtc_chrome_e2e` 的 socket、WebSocket、JSONL 和文件输出都位于 `examples/chrome_e2e/` 示例层。核心库仍不创建 socket、不拥有线程或事件循环；UDP datagram 通过 `rtc_peer_connection_receive_datagram` 输入库，待发送 datagram 通过 `observer.on_datagram` 输出后由示例层立即复制到发送队列，再由 `sendto` 发送。

当前计划只完成 C 示例运行时骨架和 smoke 检查；真实 Chrome 互通仍依赖后续样本媒体解析、真实 DTLS/SRTP backend gate 和 full E2E 编排。

## 样本媒体与落盘文件

C 示例直接读取项目根目录的 `sample1.opus` 与 `test-25fps.h264`。`sample1.opus` 只在示例层解析 Ogg page/lacing，跳过 `OpusHead` 和 `OpusTags` 后按 Opus packet 发送；`test-25fps.h264` 只在示例层按 Annex B start code 切分，按 25fps 的 `40000us` 节奏发送。parser 不进入 `src/` 核心库。

收到 Chrome 侧 typed media frame 后，示例写入：

- `received-opus.packets`：每个 Opus packet 前写 4 字节 big-endian 长度，便于后续工具解析。
- `received-h264.264`：每帧前写 Annex B `00 00 00 01` start code，可用 `ffplay -f h264 received-h264.264` 或 VLC 打开检查。

JSONL `summary` 会报告 `audio_frames_received`、`video_frames_received`、`audio_bytes_received` 和 `video_bytes_received`。媒体文件打开或写入失败时，summary/error 的 `layer` 使用 `media_file`。

## 运行

```bash
npm run e2e:chrome:dry-run
node examples/chrome_e2e/run_e2e.mjs --c-example-smoke
node examples/chrome_e2e/run_e2e.mjs --media-file-smoke
```

后续计划会把 C 示例、样本媒体解析和完整 E2E 编排接入同一入口。

## 依赖许可证

- `ws`: MIT
- `@playwright/test`: Apache-2.0
