---
phase: 05-rtp-rtcp-media-plane
plan: 05
subsystem: media
tags: [c, rtcp, pli, nack, srtcp, fixed-memory, observability]

requires:
  - phase: 05-rtp-rtcp-media-plane
    provides: 05-04 RTCP SR/RR/SDES codec、SRTCP protect/unprotect gate 和 RTCP counters
provides:
  - 显式 `rtc_peer_connection_request_keyframe(video)` 到受保护 RTCP PLI datagram 输出路径
  - 远端 PLI 解析后 typed media feedback observer 上报
  - Generic NACK PID/BLP 固定数组展开和 observer 上报
  - NACK parse/report only 的 `nack_no_retransmit` counter 与 trace reason
affects: [05-06-docs, phase-6-chrome-e2e, media-feedback-api]

tech-stack:
  added: []
  patterns:
    - 纯 C 固定 buffer RTCP feedback codec
    - media executor API 投递 network executor SRTCP protect
    - NACK parse/report only 可观测性锁定

key-files:
  created:
    - .planning/phases/05-rtp-rtcp-media-plane/05-05-SUMMARY.md
  modified:
    - src/rtcp/rtcp.h
    - src/rtcp/rtcp.c
    - src/media/media.h
    - src/media/media.c
    - src/api/peer_connection.c
    - tests/test_rtcp.c
    - tests/test_media_api.c

key-decisions:
  - "PLI 发送只通过 `request_keyframe(video)` 显式触发；Opus keyframe 请求返回 `RTC_STATUS_UNSUPPORTED`。"
  - "收到远端 PLI/NACK 只通过 `observer.on_media_feedback`、counter 和 trace 上报，库不控制编码器也不执行重传。"
  - "Generic NACK 的 PID/BLP 展开写入固定 17 项数组，超出部分截断在固定容量内。"

patterns-established:
  - "RTCP feedback receive gate: RTCP datagram 必须先通过 `rtc_srtp_unprotect_rtcp`，成功后才解析 PLI/NACK 并上报。"
  - "RTCP PLI send gate: media executor 写入固定 slot，network executor 进行 SRTCP protect，成功后复用 `observer.on_datagram`。"
  - "NACK no-retransmit: `retransmit_performed = 0`、`nack_received`、`nack_no_retransmit` 和 trace reason `nack_no_retransmit` 同步输出。"

requirements-completed: [RTCP-03, RTCP-04, OBS-04]

duration: 6min32s
completed: 2026-05-11
---

# Phase 05 Plan 05: PLI/NACK 媒体反馈 Summary

**PLI 现在可显式发送和接收上报，NACK 现在可解析 PID/BLP 并以 no-retransmit 语义上报。**

## Performance

- **Duration:** 6min32s
- **Started:** 2026-05-10T16:35:16Z
- **Completed:** 2026-05-10T16:41:48Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments

- 实现 `rtc_peer_connection_request_keyframe(pc, RTC_MEDIA_KIND_VIDEO_H264)`，在 media executor 入口校验后投递 network executor，生成 RTCP PSFB PLI，完成 SRTCP protect 后通过 `observer.on_datagram` 输出。
- 收到远端 PLI 时，在 SRTCP unprotect 成功后解析并上报 `RTC_MEDIA_FEEDBACK_PLI`，`kind = RTC_MEDIA_KIND_VIDEO_H264`，`retransmit_performed = 0`，并更新 `pli_received`。
- 实现 `rtc_rtcp_parse_nack`，把 Generic NACK 的 PID/BLP 展开到固定 17 项 `lost_sequence_numbers` 数组。
- 收到远端 NACK 时上报 `RTC_MEDIA_FEEDBACK_NACK`，更新 `nack_received` 和 `nack_no_retransmit`，trace reason 固定为 `nack_no_retransmit`。
- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发边界；未新增依赖、重传缓存、RTX、resend 或发送节奏逻辑。

## Task Commits

1. **Task 1 RED: PLI 失败测试** - `4d36cc3` (test)
2. **Task 1 GREEN: PLI feedback path** - `277a31c` (feat)
3. **Task 2 RED: NACK no-retransmit 失败测试** - `e73abda` (test)
4. **Task 2 GREEN: NACK parse-only feedback** - `abac30d` (feat)

**Plan metadata:** 见最终 docs commit。

## Files Created/Modified

- `src/rtcp/rtcp.h` / `src/rtcp/rtcp.c` - 新增 PLI writer、Generic NACK parser 和 RTCP feedback packet 分发。
- `src/media/media.h` / `src/media/media.c` - 新增 PLI request 发送路径、PLI/NACK feedback observer 上报、counter 和 trace。
- `src/api/peer_connection.c` - 将 public `request_keyframe` 从 unsupported 占位接入 media 层实现。
- `tests/test_rtcp.c` - 覆盖 PLI 发送、PLI 接收上报、NACK PID/BLP 展开、NACK no-retransmit counter/trace。
- `tests/test_media_api.c` - 覆盖 Opus keyframe 请求拒绝语义。

## Decisions Made

- PLI 发送使用 RTCP Payload-Specific Feedback：FMT=1、PT=206；media SSRC 使用 video RTCP SSRC。
- NACK 解析使用 RTCP Transport-Layer Feedback：FMT=1、PT=205；首个 FCI 保留在 `pid` / `blp` 字段，所有 FCI 在固定容量内展开到 `lost_sequence_numbers`。
- NACK 不触发任何 RTP 重发行为；可观测输出明确声明 `nack_no_retransmit`。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] no-retransmit 扫描被测试函数名误触发**
- **Found during:** Task 2
- **Issue:** 计划验收命令允许 `nack_no_retransmit` 和 `retransmit_performed`，但测试函数名最初包含 `no_retransmit`，导致禁止重传扫描误报。
- **Fix:** 将测试函数名改为包含规范 reason `nack_no_retransmit`，不改变测试行为。
- **Files modified:** `tests/test_rtcp.c`
- **Verification:** `! grep -R "retransmit\\|RTX\\|rtx" src include tests | grep -v "retransmit_performed" | grep -v "nack_no_retransmit"` 通过。
- **Committed in:** `abac30d`

---

**Total deviations:** 1 auto-fixed (Rule 1: 1)
**Impact on plan:** 仅修正测试命名以匹配计划的禁止重传扫描规则，没有扩大功能范围。

## Issues Encountered

- TDD RED 阶段均按预期失败：Task 1 缺少 `rtc_rtcp_write_pli`；Task 2 缺少 `rtc_rtcp_parse_nack`。
- Task 2 初次实现后 `src/rtcp/rtcp.h` 需要包含 `rtc/media.h` 才能公开 `rtc_media_feedback_t` 参数；已在 GREEN 调试中修正并通过全量测试。

## Known Stubs

None - 本计划目标内没有保留阻塞性 stub。

## Threat Flags

无。计划 threat model 已覆盖本次新增信任边界：PLI/NACK 只在 SRTCP unprotect 成功后解析；NACK BLP 展开限制在固定 17 项数组；NACK 明确 no-retransmit。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure` - PASS
- Task 1 acceptance grep - PASS
- Task 2 acceptance grep - PASS
- `! grep -R "retransmit\\|RTX\\|rtx" src include tests | grep -v "retransmit_performed" | grep -v "nack_no_retransmit"` - PASS
- `! grep -R "rtx\\|retransmit_cache\\|resend" src include tests` - PASS
- Stub scan - PASS，无 TODO/FIXME/placeholder 或阻塞性空数据源。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

05-06 可以文档化 `request_keyframe(video)`、远端 PLI/NACK feedback observer、NACK no-retransmit 语义、counter/trace 名称，以及 Chrome 端到端验收仍在第 6 阶段的边界。

## Self-Check: PASSED

- 文件存在性检查通过：`src/rtcp/rtcp.h`、`src/rtcp/rtcp.c`、`src/media/media.c`、`tests/test_rtcp.c`、`tests/test_media_api.c`、`05-05-SUMMARY.md` 均存在。
- 提交存在性检查通过：`4d36cc3`、`277a31c`、`e73abda`、`abac30d` 均可在 git log 中找到。
- 计划级验证通过：`cmake --build build && ctest --test-dir build --output-on-failure`。
- 禁止重传扫描通过：没有新增 `rtx`、`retransmit_cache` 或 `resend`。

---
*Phase: 05-rtp-rtcp-media-plane*
*Completed: 2026-05-11*
