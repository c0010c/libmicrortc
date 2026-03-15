# Step11 CLI Runbook

## 目标
- 用 `rtc_signal_cli` 完成最小信令联调：`Offer -> Answer` 与双向 candidate 交换。
- 验证 `restart-peer` 后可重建 peer 并再次产出 Answer/Candidate。

## 前置条件
- 已构建二进制：`rtc_signal_cli`。
- Chrome 与设备在同一网段。
- 当前仅支持 `host IPv4` candidate。
- 如 Chrome 输出 mDNS candidate（`.local`），需关闭 mDNS 隐藏或手工选择 IPv4 host candidate。

## 启动 CLI
```bash
./build/rtc_signal_cli
```

## Chrome 页面
- 辅助页面路径：`docs/runbook/chrome-step11-offer.html`
- 直接用 Chrome 打开该文件即可（`Ctrl+O` 选择文件）。
- 页面功能：
  - 生成 Offer SDP
  - 粘贴并应用 CLI 返回的 Answer SDP
  - 导出 Chrome 本地 IPv4 host candidate
  - 注入 CLI 输出的 `EVENT LOCAL_CANDIDATE ...`

## 命令协议
- 成功：`OK <cmd>`
- 失败：`ERR <code> <reason>`
- 状态事件：`EVENT STATE <old> <new>`
- 本地 SDP：`EVENT LOCAL_DESCRIPTION_BEGIN answer` ... `EVENT LOCAL_DESCRIPTION_END`
- 本地 candidate：`EVENT LOCAL_CANDIDATE <candidate>`

## Chrome -> CLI（Offer）
1. 在 `chrome-step11-offer.html` 中点击 `Create Offer`，等待状态区提示 gathering 完成后复制 `Offer SDP`（页面会输出包含 candidate 的 Offer）。
2. 在 CLI 输入：
```text
set-offer-begin
<粘贴 Offer，每行原样输入>
set-offer-end
start
run 500 10 500
```
3. 期望输出：
- `OK set-offer-end`
- `OK start`
- `EVENT LOCAL_DESCRIPTION_BEGIN answer` / `EVENT LOCAL_DESCRIPTION_END`
- 至少一条 `EVENT LOCAL_CANDIDATE ...`

## CLI -> Chrome（Answer）
1. 执行：
```text
get-answer
```
2. 将 `EVENT LOCAL_DESCRIPTION_BEGIN answer` 与 `EVENT LOCAL_DESCRIPTION_END` 之间的 SDP 粘贴到页面 `Paste Answer From CLI` 区域，点击 `Apply Answer SDP`。

## Candidate 交换
1. 在页面里复制 `Local IPv4 Host Candidates`，把其中一条注入 CLI：
```text
add-remote-candidate candidate:... typ host
```
2. 把 CLI 输出的 `EVENT LOCAL_CANDIDATE ...` 粘贴到页面 `Add Remote Candidate To Chrome` 输入框后点击按钮注入。
3. 可用 `state`、`stats` 观察状态与计数。

## restart-peer 重连验证
1. 执行：
```text
restart-peer
```
2. 期望输出 `OK restart-peer`。
3. 重复一轮 `set-offer-begin -> set-offer-end -> start -> run -> get-answer`。
4. 期望再次出现：
- `EVENT LOCAL_DESCRIPTION_BEGIN answer` / `EVENT LOCAL_DESCRIPTION_END`
- `EVENT LOCAL_CANDIDATE ...`

## 异常判定
- 注入不支持 candidate（非 UDP/非 host/非 IPv4）应返回 `ERR`，且进程不崩溃。
- Offer 解析失败应返回 `ERR` 并可在 stderr 日志定位模块、peer、错误码。
