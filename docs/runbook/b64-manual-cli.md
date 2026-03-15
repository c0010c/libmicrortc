# rtc_signal_b64_cli 手工联调 Runbook（Offer/Answer 纯 Base64）

## 目标
- 设备端 `rtc_signal_b64_cli` 持续推流，Chrome 端 `recvonly` 拉流。
- 信令只通过手工粘贴交换 `offer_b64` / `answer_b64`。

## 前置条件
- 已构建二进制：`build/rtc_signal_b64_cli`。
- Chrome 与设备在同一网段。
- Chrome 打开页面：`docs/runbook/chrome-b64-manual.html`。

## 1) Chrome 生成 Offer Base64
1. 打开 `chrome-b64-manual.html`。
2. 点击 `Create Offer (recvonly audio+video)`。
3. 复制 `Offer SDP base64` 文本。

## 2) 设备侧输入 Offer 并产出 Answer Base64
```bash
echo '<offer_b64>' | ./build/rtc_signal_b64_cli --run-ms 300000 > /tmp/rtc_signal_b64_cli.out
```

说明：
- `stdin`：offer base64。
- `stdout`：输出日志与 answer（answer 是单独一行 base64）。
- 输出 answer 后，CLI 会继续连接与推流，直到 `Ctrl-C` 或 `--run-ms` 到期。

## 3) Chrome 应用 Answer Base64
1. 从输出中提取 answer 行：
```bash
grep -E '^[A-Za-z0-9+/=]+$' /tmp/rtc_signal_b64_cli.out | head -n1 > /tmp/answer.b64
```
2. 将 `/tmp/answer.b64` 内容粘贴到页面 `Answer` 输入框。
3. 点击 `Apply Answer Base64`。
4. 观察页面远端视频区域与状态日志（`pc.connectionState=connected`，并可见远端媒体）。

## 常见问题定位
- `read offer b64 failed`：输入为空、非 base64 字符、padding 错误或长度超限。
- `set remote description failed`：Offer 解码后 SDP 内容不合法或不被当前能力支持。
- 页面 `Apply answer failed`：answer base64 不完整/包含污染字符。
- 无远端流：确认 CLI 进程仍在运行，且 Chrome 与设备网络可达。

## 快速验证命令
```bash
# 将 answer.b64 还原为 sdp 查看关键字段
cat /tmp/answer.b64 | tr -d '\r\n' | base64 -d | sed -n '1,80p'
```
