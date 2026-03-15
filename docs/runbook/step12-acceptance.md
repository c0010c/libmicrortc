# Step12 手工验收 Runbook（非 Trickle）

## 前置条件
- 已构建：`build/rtc_signal_cli`。
- Chrome 与设备在同网段。
- 使用 `docs/runbook/chrome-step12-acceptance.html` 页面。
- 当前优先 `IPv4 host candidate`。若页面提示 mDNS-only，请调整 Chrome 隐私设置后重试。

## 1. 浏览器侧生成 Offer
1. 打开 `docs/runbook/chrome-step12-acceptance.html`。
2. 点击 `Start Capture`。
3. 点击 `Create Offer (Wait ICE Complete)`。
4. 将 Offer 保存为 `/tmp/step12-offer.sdp`。

## 2. 启动客户端并自动产出 Answer
```bash
cat /tmp/step12-offer.sdp | ./build/rtc_signal_cli --run-ms 320000 > /tmp/step12-answer.sdp 2>/tmp/step12-cli.log
```

说明：
- `stdout` 仅输出一次 Answer SDP 到 `/tmp/step12-answer.sdp`。
- 进程随后继续运行连接与推流，直到 `--run-ms` 到期（约 320 秒）。
- 如浏览器粘贴流程容易污染换行，可改用 `--answer-b64` 输出单行 base64。

## 3. Offer/Answer 交换
1. 将 `/tmp/step12-answer.sdp` 内容粘贴到网页 `Answer`。
2. 点击 `Apply Answer`。

## 4. 5 分钟验收
1. 页面点击 `Start 5min Acceptance`。
2. 等待页面达到 300 秒（CLI 在后台持续运行）。

期望：
- 页面显示本地和远端预览均有流动；
- 页面统计中双向音视频字节持续增长；
- 连接状态无 `failed/disconnected`。

## 5. 导出证据并校验
1. 页面点击 `Export Web Evidence`，保存为 `step12-web-evidence.json`。
2. 执行矩阵与证据校验：
```bash
./test/run_step12_matrix.sh --manual-evidence /path/to/step12-web-evidence.json
```

通过标准（严格）：
- `duration_sec >= 300`
- `connection_failed == false`
- `bidirectional_growth == true`
