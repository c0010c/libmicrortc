# Chrome E2E C Answerer Demo

`mrtc_chrome_answerer` 是 Phase 6 Chrome E2E 使用的 C 端 answerer 示例。它通过 stdin/stdout 的 JSON line 协议接收浏览器侧 offer/candidate，返回 answer/candidate，并输出 PeerConnection、DataChannel 和媒体路径事件。

## 构建

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
npm --prefix tests/e2e ci
```

可执行文件位于：

```bash
./build/examples/chrome-e2e/mrtc_chrome_answerer
```

## 常用命令

```bash
./build/examples/chrome-e2e/mrtc_chrome_answerer --help
./build/examples/chrome-e2e/mrtc_chrome_answerer --fixtures ./tests/fixtures --self-test-media
./build/examples/chrome-e2e/mrtc_chrome_answerer --fixtures ./tests/fixtures --self-test-media-callbacks
```

## 浏览器手动验证

启动本地 signaling bridge 和 C answerer：

```bash
npm --prefix tests/e2e run manual
```

命令会打印一个 `Open:` 开头的 `file://...examples/chrome-e2e/index.html?manual=1&ws=...` 地址。用 Chrome 打开该地址，然后点击页面里的“开始拉流”。

期望结果：

- `Peer` 变为 `connected`。
- `DataChannel` 变为 `open`。
- `浏览器视频` 变为 `playing`，页面视频区域开始显示 C 端 H264 fixture。
- `浏览器音频` 变为 `playing`。
- `C 端媒体` 显示收到的浏览器侧 H264/Opus 帧计数。

停止时在启动命令所在终端按 `Ctrl-C`。

输入消息类型：

- `offer`
- `candidate`
- `start-media`
- `stop`

输出消息类型：

- `hello`
- `answer`
- `candidate`
- `event`
- `done`
- `error`

## ICE 配置

`--ice-config <path>` 可读取本地 ICE server JSON。真实 TURN hostname、username、password、token 或 credential 不应提交到仓库，也不应写入 stdout、stderr 或测试摘要。示例配置请使用仓库根目录的 `mrtc-ice-servers.example.json`，真实本地配置应放在已忽略的本地文件中。

## 媒体 fixture

`--fixtures ./tests/fixtures` 会读取：

- `h264_annexb_sample.h264`：Annex-B SPS/PPS/IDR 样本。
- `opus_packets.bin`：big-endian 16-bit length-prefixed Opus packet 序列。

demo 只读取编码后媒体帧，不做采集、编码、解码、GStreamer、FFmpeg、Ogg、MP4 或 AVCC 解析。
