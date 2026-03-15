# rtc_signal_cli 无命令化改造（贴 Offer 直出 Answer + 持续推流）实施计划

## Summary
- 将 [`tools/rtc_signal_cli.c`](/home/qshl/dev/rtc/s1/tools/rtc_signal_cli.c) 从“命令协议”改为“单次 stdin Offer 输入”模式：不再需要 `set-offer-begin/start/run/get-answer` 等命令。
- 成功路径：stdin 读到 EOF 后自动生成并输出一次 Answer（stdout 仅 SDP 本体），随后继续跑连接与媒体发送；默认 `Ctrl-C` 退出，可选 `--run-ms` 自动退出。
- 参考对齐：
  - `libdatachannel`：保留回调驱动产出本地描述思路；
  - `libpeer reader.c`：采用 Annex-B 起始码切 NAL、IDR 前补 SPS/PPS；
  - `amazon-kinesis StaticMedia.c`：媒体文件 EOF 循环；
  - `metaRTC`：沿用 `setRemoteDescription -> createAnswer` 的职责边界。
- 与参考差异（已定稿）：不做命令交互，不做重型 AU 解析；按 NAL 发送并保留可观测降级路径。

## Key Changes
- CLI 对外接口（无核心库 API 变更）：
  - 输入：stdin 粘贴 Offer，EOF 结束。
  - 输出：成功时 stdout 仅一次 Answer SDP；失败时 stderr 输出上下文并返回非 0。
  - 新参数：`--video-file <path>`（可选）、`--run-ms <ms>`（可选）、`-h/--help`。
- 内部状态推进：
  - `set_remote_description + start` 后自动轮询，500ms 内等待 `on_local_description` 产出 Answer；超时明确报错。
  - Answer 输出后进入持续运行循环（poll + media tick），直到 `Ctrl-C` 或 `--run-ms` 到期。
- 媒体发送策略：
  - 音频继续用现有 PCMA 合成静音。
  - `--video-file` 启用 Annex-B NAL 级读取：缓存 SPS/PPS，IDR 前拼接 SPS/PPS；文件 EOF 自动回环。
  - 文件异常/解析异常：stderr WARN 并回退到内置 H264 合成样本（不静默失败）。
- 资源与实时性约束：
  - 仅固定上限缓冲（无无界容器、无后台新线程、无递归）。
  - 轮询步长与媒体发送节奏保持现有有界策略，行为可预测。
- 文档与脚本同步：
  - 更新 [`docs/runbook/step11-cli.md`](/home/qshl/dev/rtc/s1/docs/runbook/step11-cli.md)、[`docs/runbook/step12-acceptance.md`](/home/qshl/dev/rtc/s1/docs/runbook/step12-acceptance.md)、[`docs/runbook/chrome-step11-offer.html`](/home/qshl/dev/rtc/s1/docs/runbook/chrome-step11-offer.html) 为“贴 Offer -> 自动出 Answer”流程。
  - 调整 [`test/run_step12_matrix.sh`](/home/qshl/dev/rtc/s1/test/run_step12_matrix.sh) 的 manual-evidence 逻辑为仅校验 web evidence（不再依赖 CLI client_evidence）。

## Public Interface Changes
- `rtc_signal_cli` 协议级行为变更（breaking）：
  - 移除命令式 stdin 协议；
  - 新增参数式控制（`--video-file`、`--run-ms`）；
  - stdout/stderr 契约固定为“stdout 仅 Answer，运行日志与错误走 stderr”。
- `run_step12_matrix.sh --manual-evidence` 从“web+client 双证据必填”改为“web 证据必填，client 证据不再必需”。

## Test Plan
- 改写 [`test/test_rtc_signal_cli.sh`](/home/qshl/dev/rtc/s1/test/test_rtc_signal_cli.sh)：
  - 正向：管道输入 Offer，断言退出码 0、stdout 含完整 Answer 关键字段。
  - 负向：空 Offer/非法 Offer，断言非 0 + stderr 有上下文错误。
  - 持续运行：用 `--run-ms` 验证不会挂死测试。
- 改写 [`test/test_rtc_signal_cli_long_offer.sh`](/home/qshl/dev/rtc/s1/test/test_rtc_signal_cli_long_offer.sh)：
  - 长 Offer 边界不溢出，仍可产出 Answer。
- 增补 `--video-file` 场景：
  - 文件可读时正常发送路径；
  - 文件异常时触发 WARN 并回退合成视频，不影响 Answer 产出。
- 回归执行：现有 `ctest` 中 `rtc_signal_cli_*` 与 `step12_matrix_auto` 全量通过。

## Assumptions & Defaults
- 已确认：彻底移除命令模式；stdin 以 EOF 结束；stdout 仅输出 Answer；错误走 stderr+非 0。
- 已确认：Answer 生成窗口 500ms；输出后继续运行连接与推流；默认 Ctrl-C 退出，可选 `--run-ms`。
- 已确认：媒体文件本次仅 `--video-file`；H264 按 NAL 发送并在 IDR 前补 SPS/PPS；解析失败回退合成视频。
- 已确认：文档仅更新当前操作文档（runbook/页面提示），不改历史 plan 文档。
