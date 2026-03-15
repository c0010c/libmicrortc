# Step12 手工验收 Runbook（非 Trickle）

## 前置条件
- 已构建：`build/rtc_signal_cli`。
- Chrome 与设备在同网段。
- 使用 `docs/runbook/chrome-step12-acceptance.html` 页面。
- 当前仅支持 `IPv4 host candidate`。若页面提示 mDNS-only，请调整 Chrome 隐私设置后重试。

## 1. 启动客户端测试程序
```bash
./build/rtc_signal_cli
```

可选命令：
- `media-start`：开启合成流发送（默认开启）。
- `media-stop`：停止合成流发送。

## 2. 浏览器侧生成 Offer
1. 打开 `docs/runbook/chrome-step12-acceptance.html`。
2. 点击 `Start Capture`。
3. 点击 `Create Offer (Wait ICE Complete)`。
4. 复制 `Offer SDP`。

## 3. Offer/Answer 交换
在 CLI 输入：
```text
set-offer-begin
<粘贴 Offer SDP>
set-offer-end
start
run 2000 10 500
get-answer
```

将 `get-answer` 输出的 SDP（`LOCAL_DESCRIPTION_BEGIN/END` 之间内容）粘贴到网页 `Answer`，点击 `Apply Answer`。

## 4. 5 分钟验收
1. 页面点击 `Start 5min Acceptance`。
2. CLI 持续推进状态机（示例每次 10 秒）：
```text
run 10000 10 500
```
重复执行直到页面达到 300 秒。

期望：
- 页面显示本地和远端预览均有流动；
- 页面统计中双向音视频字节持续增长；
- 连接状态无 `failed/disconnected`。

## 5. 导出证据并校验
1. 页面点击 `Export Web Evidence`，保存为 `step12-web-evidence.json`。
2. CLI 导出客户端证据：
```text
evidence-dump /tmp/step12-client-evidence.json
```
3. 执行矩阵与证据校验：
```bash
./test/run_step12_matrix.sh --manual-evidence /path/to/step12-web-evidence.json /tmp/step12-client-evidence.json
```

通过标准（严格）：
- `duration_sec >= 300`
- `connection_failed == false`
- `bidirectional_growth == true`
- `peer_state == connected`
- `rx_audio_frames > 0 && rx_video_frames > 0`
- `dtls_last_error == 0`
- `queue_overflow_count == 0`
