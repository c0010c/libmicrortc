---
phase: 05-rtp-rtcp-media-plane
reviewed: 2026-05-10T17:01:36Z
depth: quick
files_reviewed: 5
files_reviewed_list:
  - examples/create_destroy.c
  - src/media/media.c
  - src/rtp/rtp.c
  - tests/test_rtcp.c
  - tests/test_rtp.c
findings:
  critical: 0
  warning: 1
  info: 0
  total: 1
status: advisory
---

# Phase 5：代码复审报告

**Reviewed:** 2026-05-10T17:01:36Z
**Depth:** quick
**Files Reviewed:** 5
**Status:** advisory

## Summary

本次按指定 quick 深度复审先前报告的 `CR-01`、`CR-02`、`CR-03`、`WR-02`，并确认 `WR-01` 是否仍然存在。指定的快速扫描未发现新的 hardcoded secret、危险函数、调试残留或空 `catch` 模式。

先前 4 个阻塞/非阻塞修复点已处理：

- `CR-01` 已修复：`src/media/media.c:695` 在 H264 packetize 后释放 `packet_count..slot_count-1` 的未使用预占 slot；`tests/test_rtp.c:716-722` 连续发送两个小 H264 single NALU，覆盖了 slot 泄漏回归。
- `CR-02` 已修复：`src/rtp/rtp.c:462-493` 在 active FU-A 被 single NALU 或 STAP-A 打断时重置 `inout_active` / `inout_reassembly_len` 并设置 `h264_interrupted_au`；`tests/test_rtp.c:479-523` 覆盖中断 FU-A 后接 single NALU 的场景。
- `CR-03` 已修复：`src/media/media.c:344-360` 将 RTCP parse 派发到 media executor，PLI/NACK observer 因此在 media executor 上触发；`tests/test_rtcp.c:554-597` 和 `tests/test_rtcp.c:662-705` 分别断言 PLI/NACK feedback 的 executor 为 `RTC_EXECUTOR_MEDIA`。
- `WR-02` 已修复：`examples/create_destroy.c:55` 在逐字段赋值前对 `rtc_peer_connection_config_t config` 执行 `memset` 清零。

当前仅保留 `WR-01` 作为非阻塞设计警告：同步测试仍掩盖了真实异步 executor 下 `post()` 后立即读取任务结果的 API 语义风险。

## Warnings

### WR-01：跨 executor post 后立即读取任务结果，仍只被同步执行器语义覆盖

**分类：WARNING**
**File:** `src/media/media.c:205`
**Issue:** `rtc_media_dispatch_slot` 在调用 `pc->executors.network.post(...)` 后立即读取 `slot->dispatch_status`；同类模式还存在于 `src/media/media.c:335-341`、`src/media/media.c:359-365` 和 `src/media/media.c:567-578`。当前测试执行器会同步运行任务，所以测试能观察到 protect/unprotect/parse 的最终结果；但真实执行器如果合法地排队稍后执行，public API 会先返回旧的 `RTC_STATUS_OK`，调用者无法从返回值观察后续失败，slot 生命周期也依赖尚未明确定义的异步执行契约。
**Fix:** 明确媒体发送/接收跨 executor 后的 API 语义。如果 API 需要同步返回 protect/unprotect/parse 错误，就不能通过可异步的 `post()` 后读共享 slot 状态实现；如果 API 语义是“已入队”，则返回值应仅表示入队结果，后续失败通过 observer error、trace、counter 等异步通道报告，并补充延迟执行器测试覆盖任务未立即运行、失败稍后发生、以及销毁时仍有排队任务等场景。

---

_Reviewed: 2026-05-10T17:01:36Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: quick_
