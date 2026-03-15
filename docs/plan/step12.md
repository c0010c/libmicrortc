# Step12：端到端验收与回归矩阵（网页 + 客户端）

## 目标
- 固化三组回归：`positive-auto`、`negative-auto`、`boundary-auto`。
- 提供手工 5 分钟验收闭环：Chrome 验收页 + `rtc_signal_cli` 客户端测试程序。
- 采用非 Trickle 主流程：仅复制 `Offer/Answer`，candidate 内嵌在 SDP。

## 实现摘要
- `tools/rtc_signal_cli.c`
  - 新增命令：`media-start`、`media-stop`、`evidence-dump <path>`。
  - `run`/`tick` 中按固定节奏发送合成流（无新线程）：
    - 音频：G711(PCMA) 每 20ms；
    - 视频：H264 固定节奏 IDR 样本。
  - 证据导出 JSON 包含：`rx_audio_frames/rx_video_frames/protocol_error_count/queue_overflow_count/dtls_last_error/state` 等关键指标。
- `docs/runbook/chrome-step12-acceptance.html`
  - 本地采集 + 远端预览；
  - 生成内嵌 candidate 的 Offer；
  - 粘贴并应用 Answer；
  - 每秒统计 `getStats()`；
  - 5 分钟计时与 web evidence 导出。
- `test/run_step12_matrix.sh`
  - 单入口执行三组自动化矩阵；
  - 支持 `--auto-only`；
  - 支持 `--manual-evidence <web.json> <client.json>` 做证据校验。
- `CMakeLists.txt`
  - 新增 `step12_matrix_auto` 测试入口。
- `test/test_rtc_api.c`
  - 新增分组开关：`RTC_API_GROUP=all|positive|negative|boundary`。

## 通过标准（严格）
- 自动化：`step12_matrix_auto` 全绿。
- 手工 5 分钟：
  - 双向字节/帧计数持续增长；
  - 连接无 `FAILED/DISCONNECTED`；
  - `dtls_last_error=0`；
  - `queue_overflow_count=0`；
  - web/client evidence 校验通过。
