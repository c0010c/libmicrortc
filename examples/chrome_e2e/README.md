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

默认构建不会启用真实 Chrome DTLS/SRTP backend。`rtc_chrome_e2e` 在非 dry-run 路径检测到未启用可选 backend 时会输出 JSONL summary：`pass:false`、`layer:"dtls"`、`reason:"optional_security_backend_disabled"`，防止把 deterministic backend 或无加密路径误报为真实互通成功。

安全相关 observer error/trace 会保留分层诊断：fingerprint mismatch 归入 `dtls`，`key_export` 与 `srtp_init` 归入 `srtp`，RTP protect/unprotect 归入 `rtp`，RTCP/SRTCP protect/unprotect 归入 `rtcp`，同时在 JSONL detail 中保留 `detail_code`。

## 样本媒体与落盘文件

C 示例直接读取项目根目录的 `sample1.opus` 与 `chrome-25fps-42001f.h264`。`sample1.opus` 只在示例层解析 Ogg page/lacing，跳过 `OpusHead` 和 `OpusTags` 后按 Opus packet 发送；`chrome-25fps-42001f.h264` 从 `chrome.mp4` 转码而来，使用 Annex B H264、25fps、重复 SPS/PPS，并让 SPS `profile-level-id` 匹配当前 SDP 的 `42001f`；示例层按 Annex B start code 切分，按 25fps 的 `40000us` 节奏发送。parser 不进入 `src/` 核心库。

收到 Chrome 侧 typed media frame 后，示例写入：

- `received-opus.packets`：每个 Opus packet 前写 4 字节 big-endian 长度，便于后续工具解析。
- `received-h264-network.264`：Chrome -> C 方向收到的原始 H264 access unit dump；用于证明 C 侧收到视频字节。Chrome RTP 可能不携带可播放裸流所需的 SPS/PPS，因此该原始 dump 不作为 VLC 稳定播放验收物。
- `received-h264.264`：E2E 编排在 C 侧确认收到视频后，从同一次浏览器页面录屏中裁剪 local synthetic 区域并重新编码成 Annex B H264；它的画面对应 Chrome -> C 方向的 synthetic 输入，文件名固定为人工 VLC 检查目标。
- `received-h264-playable.mp4`：E2E 编排从浏览器页面录屏中裁剪远端视频区域后用 ffmpeg 重新编码生成的可播放 MP4；带容器时间戳，用于检查 Chrome 是否看到 C 端 sample 视频。旁路输出 `received-h264-playable.264` 仅用于裸 H264 诊断。

JSONL `summary` 会报告 `audio_frames_received`、`video_frames_received`、`audio_bytes_received` 和 `video_bytes_received`。媒体文件打开或写入失败时，summary/error 的 `layer` 使用 `media_file`。

## 运行

首次运行前安装 Node 依赖，并构建 C 示例：

```bash
npm install
cmake -S . -B build && cmake --build build --target rtc_chrome_e2e
```

常用验收命令：

```bash
node examples/chrome_e2e/run_e2e.mjs --dry-run
node examples/chrome_e2e/run_e2e.mjs --page-smoke
node examples/chrome_e2e/run_e2e.mjs --c-example-smoke
node examples/chrome_e2e/run_e2e.mjs --media-file-smoke
node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke
node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000
RTC_CHROME_E2E_BINARY=build-secure/rtc_chrome_e2e node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000 --output-dir examples/chrome_e2e/out/full-secure
node examples/chrome_e2e/run_e2e.mjs --binary build-secure/rtc_chrome_e2e --timeout-ms 30000 --output-dir examples/chrome_e2e/out/full-secure
node examples/chrome_e2e/run_e2e.mjs --binary build-secure/rtc_chrome_e2e --timeout-ms 60000 --min-media-ms 10000 --output-dir examples/chrome_e2e/out/full-secure-long
```

完整编排会启动本机信令服务、`rtc_chrome_e2e` C 示例和 Chrome 页面。编排脚本会生成一个共享 `runId`，通过页面 URL query 注入给 Chrome 页面，并通过 `rtc_chrome_e2e --run-id` 注入给 C 示例，确保双方加入同一个信令 run。随后脚本输出一行 compact JSON summary。summary 字段包含 `pass`、`layer`、`reason`、`duration_ms`、`page`、`c_example`、`media_files`、`manual_vlc_required` 和最近 5 条 `latestEvents`。`manual_vlc_required` 当前始终为 `true`；自动化只验证 SDP/ICE/DTLS/SRTP/RTP/RTCP 状态、counter 和媒体文件产物，不替代 VLC 人工播放验收。页面成功但未人工确认时，`summary-layer` 会显示 `manual_vlc_pending`。

默认 C 示例 binary 为 `build/rtc_chrome_e2e`。secure ON 构建位于 `build-secure/` 时，必须通过 `--binary build-secure/rtc_chrome_e2e` 或 `RTC_CHROME_E2E_BINARY=build-secure/rtc_chrome_e2e` 明确选择，否则 full E2E 会继续使用默认 OFF binary。

当前默认构建未启用可选安全 backend，因此 full run 会以非零退出并报告 `layer:"dtls"`、`reason:"optional_security_backend_disabled"`。开发 smoke 如需确认编排路径可使用 `--manual-security-ok`，但该选项只允许脚本以 0 退出，不表示真实 Chrome DTLS/SRTP 或 VLC 媒体验收通过。

## VLC/ffplay 人工检查

只有在启用并成功配置可选安全 backend 后，full run 才可能生成来自真实 Chrome 媒体流的 `received-opus.packets` 与 `received-h264.264`。自动化 summary 中的 `manual_vlc_required:true` 表示仍需要人工播放检查；不要把媒体文件存在或 counter 增长等同于人工验收完成。

推荐人工步骤：

1. 运行 `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000`，确认 summary 不再停在 `dtls/optional_security_backend_disabled`，且 `media_files` 中列出 `received-opus.packets` 与 `received-h264.264`。如果需要更长的双向媒体传输和更大的 H264 产物，可增加 `--min-media-ms 10000`，并把 `--timeout-ms` 提高到 `60000`。
2. 用 VLC 打开输出目录中的 `received-h264.264` 检查 Chrome -> C 方向画面是否可播放；应该看到 `Chrome synthetic media`。原始网络 dump 保存在 `received-h264-network.264`。
3. `received-opus.packets` 是长度前缀 Opus packet 文件，不是 Ogg 容器；如需播放，先用本地工具把 4 字节 big-endian 长度前缀 packet 重新封装为可播放容器，再用 VLC/ffplay 检查。
4. 记录 compact JSON summary、`rtc_chrome_e2e.jsonl` 最后 5 条事件、VLC/ffplay 结果和失败层级。人工确认前，`ACC-01` 仍视为待人工验收。

## 失败层级排查表

| layer | 下一步检查 |
|-------|------------|
| `signaling` | 检查 `signaling.mjs` 是否 ready、WebSocket `/ws` 是否可连接、页面是否发送 `offer`、C JSONL 是否包含 `offer.received` 与 `answer.sent`。 |
| `ice` | 检查 Chrome/C candidate 数量、本机 host candidate IP/port、UDP bind 是否成功，以及 JSONL 是否出现 `ice.connected`。 |
| `dtls` | 检查 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` 是否启用、fingerprint/handshake trace，以及默认 gate 是否为 `optional_security_backend_disabled`。 |
| `srtp` | 检查 `key_export`、`srtp_init` 和 `srtp.ready` 事件；SRTP 初始化失败不应继续宣称媒体可用。 |
| `rtp` | 检查 RTP protect/unprotect trace、发送/接收 counter 是否增长，以及页面 `stats-frames` / `stats-bytes` 是否变化。 |
| `rtcp` | 检查 SRTCP protect/unprotect、SR/RR/SDES 或 feedback trace，以及 RTCP counter 是否增长。 |
| `media_file` | 检查输出目录中的 `received-opus.packets` 与 `received-h264.264` 是否存在且字节数大于 0，然后再用 VLC/ffplay 做人工播放确认。 |

## 可选 Chrome 安全 backend

构建开关 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` 默认 `OFF`。默认构建不查找、不链接 OpenSSL、libsrtp 或其他系统安全依赖；full E2E 会停在 `optional_security_backend_disabled` gate。

如需启用可选安全依赖，可配置：

```bash
cmake -S . -B build -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON
cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON && cmake --build build-secure --target rtc_chrome_e2e
```

启用后 CMake 会执行 `cmake/FindRtcOptionalSecurity.cmake`，并只在 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 的 `rtc_chrome_e2e` 示例 target 上查找、包含和链接 OpenSSL/libsrtp；默认 OFF 构建不查找、不链接这些系统依赖。当前 gate 只接受宽松许可证依赖：OpenSSL 使用 Apache-2.0，libsrtp 使用 BSD-3-Clause，或等价宽松许可的替代实现；不得接受 GPL/LGPL 依赖。安装 OpenSSL/libsrtp 开发包、确认系统包许可证和维护安全更新是启用者的责任。如果本机缺少依赖，CMake 会失败并提示安装可选 Chrome E2E 安全依赖，或改回 `-DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=OFF`。

只有启用并成功配置可选安全 backend 后，full E2E 才可能到达 `dtls.connected` 和 `srtp.ready`。默认 OFF 的 smoke 只验证安全依赖 gate 与失败分层，不声明真实 Chrome DTLS/SRTP 互通已完成。

## 依赖许可证

- `ws`: MIT
- `@playwright/test`: Apache-2.0
- `OpenSSL`（仅 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 时可选）: Apache-2.0
- `libsrtp`（仅 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 时可选）: BSD-3-Clause
