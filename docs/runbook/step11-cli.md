# Step11 CLI Runbook

## 目标
- 用 `rtc_signal_cli` 完成最小信令联调：`Offer -> Answer`（无命令模式）。
- 验证 `stdin` 贴 Offer 后自动输出 Answer，并在输出后持续运行连接与推流。

## 前置条件
- 已构建二进制：`rtc_signal_cli`。
- Chrome 与设备在同一网段。
- 推荐 Offer 中包含可用 candidate（当前优先 `host IPv4`）。

## 启动 CLI
```bash
./build/rtc_signal_cli --run-ms 30000
```

## Chrome 页面
- 辅助页面路径：`docs/runbook/chrome-step11-offer.html`
- 直接用 Chrome 打开该文件即可（`Ctrl+O` 选择文件）。
- 页面功能：
  - 生成 Offer SDP
  - 粘贴并应用 CLI 返回的 Answer SDP

## I/O 契约
- 输入：`stdin` Offer 文本，`EOF` 结束（Linux/macOS: `Ctrl-D`）。
- 成功：`stdout` 仅输出 Answer SDP 本体（无 `OK/ERR/EVENT` 前缀）。
- 可选：加 `--answer-b64` 时，`stdout` 输出单行 base64（浏览器可 `atob(...)` 后直接 `setRemoteDescription`）。
- 日志/错误：走 `stderr`；错误返回非 0。

## Chrome -> CLI（Offer）
1. 在 `chrome-step11-offer.html` 中点击 `Create Offer`，等待状态区提示 gathering 完成后复制 `Offer SDP`（页面会输出包含 candidate 的 Offer）。
2. 在另一个终端执行：
```bash
cat /path/to/offer.sdp | ./build/rtc_signal_cli --run-ms 30000 > /tmp/answer.sdp
```
或直接运行 CLI 后粘贴 Offer，再按 `Ctrl-D` 结束输入。
3. 期望：
- 进程返回码为 `0`；
- `/tmp/answer.sdp` 是完整 Answer SDP；
- `stderr` 可看到状态推进日志。

## CLI -> Chrome（Answer）
1. 将 `/tmp/answer.sdp` 内容粘贴到页面 `Paste Answer From CLI` 区域。
2. 点击 `Apply Answer SDP`。

## 异常判定
- 空 Offer/非法 Offer：CLI 非 0 退出，`stderr` 可定位失败原因。
- Offer 超长：CLI 非 0 退出并提示 `buffer_too_small` 相关错误。
