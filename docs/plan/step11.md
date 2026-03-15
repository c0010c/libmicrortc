# Step11：终端信令联调 CLI（最小实现，核心不耦合）

## Summary
- 目标：落地“终端输入输出”的最小信令工具，完成 `读取 Offer -> 输出 Answer`、`接收/输出 candidate`，并可按 runbook 复现联调。
- 参考库对齐：
  - `libdatachannel`：手工 copy/paste SDP 的最小流程。
  - `amazon-kinesis`、`libpeer`：Answer/Candidate 分事件输出，回调驱动。
  - `metaRTC`：信令放在工具层，不塞进核心 RTC 层。
- 已锁定方案：命令式文本协议、支持 `tick` 和自动 `run` 双模式、`tools` 独立可执行、内置 `restart-peer`，并在文档中明确 IPv4-only 限制。

## Key Changes
- 新增工具可执行（建议路径：`tools/rtc_signal_cli.c`）并接入构建（`CMakeLists.txt` 增加 target，不改核心库链接关系）。
- CLI 命令接口（稳定文本协议）：
  - `set-offer-begin` / `set-offer-end`：多行粘贴 Offer。
  - `add-remote-candidate <candidate>`：注入远端 candidate。
  - `start`：启动 peer。
  - `tick <now_ms> [budget_us]`：手动推进状态机。
  - `run <duration_ms> [step_ms] [budget_us]`：自动循环推进（工具层实现）。
  - `get-answer`：输出当前 Answer。
  - `state`、`stats`：输出状态与关键统计。
  - `restart-peer`：销毁并重建 peer，保留引擎与日志配置，验证清理/重连。
  - `quit`：退出。
- CLI 输出协议（stdout，脚本友好）：
  - `OK <cmd>` / `ERR <code> <reason>`
  - `EVENT STATE <old> <new>`
  - `EVENT LOCAL_DESCRIPTION_BEGIN answer` + SDP + `EVENT LOCAL_DESCRIPTION_END`
  - `EVENT LOCAL_CANDIDATE <candidate>`
- 日志与可观测性：
  - 复用现有 `rtc_log_callback_t`，日志走 stderr，保留模块/peer/code/context。
  - 关键失败路径（Offer 解析失败、candidate 不支持、restart 失败）统一 `ERR` 可定位输出。
- 资源边界：
  - Offer 缓冲上限 `RTC_CFG_MAX_SDP_LEN`，candidate 上限 `RTC_CFG_MAX_CANDIDATE_LEN`，命令行固定上限（例如 4KB），不引入无界容器，不改核心内存模型。

## Public APIs / Interfaces
- `include/rtc/rtc.h` 与核心库对外 C API：不变。
- 新增“工具层命令协议”作为外部接口（上述命令与事件文本格式）。

## Test Plan
- 自动化 smoke（新增一个 CLI 测试用例，here-doc 驱动）：
  - `set-offer -> start -> tick/run` 后必须出现 `EVENT LOCAL_DESCRIPTION...` 与 `EVENT LOCAL_CANDIDATE`。
  - 注入非法/不支持 candidate，必须返回明确 `ERR` 且不崩溃。
  - 执行 `restart-peer` 后再次走 `set-offer/start`，应能再次产出 Answer/Candidate。
- 文档化手工 runbook（新增 `docs/runbook/step11-cli.md`）：
  - Chrome 与 CLI 的 Offer/Answer/candidate 交换步骤。
  - 中途 `restart-peer` 的重连步骤与预期输出。
- 回归：
  - `ctest --output-on-failure` 全量通过，确认核心行为不回归。

## Assumptions & Defaults
- 先保持当前核心能力：仅支持 `host IPv4` candidate；runbook 明确要求可解析 IPv4 candidate（必要时关闭 Chrome mDNS 隐藏）。
- 自动 `run` 仅在工具层主线程循环中实现，不新增核心后台线程。
- 内存开销仅在工具可执行内增加固定缓冲；核心库内存与实时性保持现状（常量级影响）。
