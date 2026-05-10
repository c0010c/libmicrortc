---
phase: 05-rtp-rtcp-media-plane
plan: 04
subsystem: media
tags: [c, rtcp, srtcp, sender-report, receiver-report, sdes, fixed-memory]

requires:
  - phase: 05-rtp-rtcp-media-plane
    provides: 05-03 RTP 接收、SRTP unprotect、Opus/H264 depacketize 和固定 media slot 模式
provides:
  - RTCP SR/RR/SDES 固定 buffer codec 与 compound packet parser
  - per-kind RTCP sender/receiver stats 和 SR/RR/SDES counters
  - RTCP datagram receive 的 SRTCP unprotect 安全门
  - SR/SDES report send 的 SRTCP protect 与 observer.on_datagram 输出路径
affects: [05-05-feedback, 05-06-docs, phase-6-chrome-e2e]

tech-stack:
  added: []
  patterns:
    - 纯 C 固定 buffer RTCP codec
    - network executor 上 SRTCP unprotect/protect gate
    - TDD RED/GREEN 原子提交

key-files:
  created:
    - src/rtcp/rtcp.h
    - src/rtcp/rtcp.c
    - tests/test_rtcp.c
  modified:
    - CMakeLists.txt
    - include/rtc/counters.h
    - src/api/peer_connection.h
    - src/api/peer_connection.c
    - src/media/media.h
    - src/media/media.c
    - src/observability/counters.c
    - tests/test_main.c
    - tests/test_peer_connection.c
    - tests/test_datagram.c

key-decisions:
  - "RTCP SR/RR/SDES codec 保持 internal API，公共 PeerConnection API 不暴露手动组包入口。"
  - "RTCP receive 与 RTP receive 一样，必须先复制到固定 slot 并完成 SRTCP unprotect，成功后才 parse。"
  - "RTCP report send 首版生成 audio SSRC 的 SR + SDES compound packet，PLI/NACK 和更完整调度留给 05-05。"

patterns-established:
  - "RTCP receive gate: `rtc_peer_connection_receive_datagram` 的 RTCP 分支路由到 `rtc_media_handle_rtcp_datagram`，该函数只在 `rtc_srtp_unprotect_rtcp` 成功后调用 parser。"
  - "RTCP send gate: `rtc_media_send_rtcp_reports` 先写 SR/SDES compound，再调用 `rtc_srtp_protect_rtcp`，成功后才复用 `observer.on_datagram`。"

requirements-completed: [RTCP-01, RTCP-02]

duration: 8min
completed: 2026-05-11
---

# Phase 05 Plan 04: RTCP SR/RR/SDES 与 SRTCP 集成 Summary

**RTCP SR/RR/SDES 现在有固定 buffer codec、基础统计维护，并且收发路径都受 SRTCP wrapper 保护。**

## Performance

- **Duration:** 8min
- **Started:** 2026-05-10T16:24:29Z
- **Completed:** 2026-05-10T16:31:46Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments

- 新增 `src/rtcp`，实现 RTCP common header、Sender Report、Receiver Report、SDES CNAME 写入和 compound packet parse。
- 在 `PeerConnection` 内维护 audio/video RTCP stats，RTP send/receive 路径同步更新 packets、octets、last sequence 和 SSRC 基础信息。
- 扩展 RTCP counters，覆盖 `rtcp_sr_sent`、`rtcp_sr_received`、`rtcp_rr_sent`、`rtcp_rr_received`、`rtcp_sdes_sent` 和 `rtcp_sdes_received`。
- 将 `RTC_NET_PROTOCOL_RTCP` 接入 `rtc_media_handle_rtcp_datagram`，确保 RTCP datagram 只有 SRTCP unprotect 成功后才 parse。
- 新增 `rtc_media_send_rtcp_reports`，生成 SR/SDES compound packet，并且只有 SRTCP protect 成功后才通过 `observer.on_datagram` 输出。

## Task Commits

1. **Task 1 RED: RTCP codec 失败测试** - `bf9cb5d` (test)
2. **Task 1 GREEN: RTCP SR/RR/SDES codec** - `cd331e2` (feat)
3. **Task 2 RED: RTCP/SRTCP 集成失败测试** - `90926ae` (test)
4. **Task 2 GREEN: RTCP receive/send SRTCP 集成** - `cb3a0e0` (feat)

**Plan metadata:** 见最终 docs commit。

## Files Created/Modified

- `src/rtcp/rtcp.h` / `src/rtcp/rtcp.c` - RTCP SR/RR/SDES codec、compound parser 和 stats 更新。
- `src/media/media.h` / `src/media/media.c` - RTCP receive handler、RTCP report send helper、SRTCP protect/unprotect gate 和 RTP stats 同步。
- `src/api/peer_connection.h` / `src/api/peer_connection.c` - `PeerConnection` RTCP stats 固定状态和 RTCP datagram demux 路由。
- `include/rtc/counters.h` / `src/observability/counters.c` - RTCP SR/RR/SDES counters 扩展和初始化。
- `tests/test_rtcp.c` - SR/RR/SDES round trip、长度/类型错误、SRTCP protect/unprotect 成功和失败路径测试。
- `tests/test_peer_connection.c` / `tests/test_datagram.c` - 固定 arena 边界和 RTCP demux 安全门预期更新。
- `CMakeLists.txt` / `tests/test_main.c` - 注册 RTCP 源文件和测试。

## Decisions Made

- SDES 首版只写 CNAME item，固定 CNAME 为 `rtc-cname`，长度受 `limits.rtcp.max_sdes_cname_bytes` 限制。
- Unknown RTCP packet type 返回 `RTC_STATUS_UNSUPPORTED`；短包、版本错误和长度越界返回 protocol/argument/capacity 类错误。
- RTCP report 发送保持 internal helper，便于 05-05/05-06 再接入 PLI、NACK 和文档化调度语义。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 新增 RTCP stats 后旧固定 arena 容量测试提前命中错误资源**
- **Found during:** Task 1
- **Issue:** `rtc_peer_connection_t` 增加 per-kind RTCP stats 后，旧 `test_peer_connection` 中精确大小 arena 不再在 STUN transaction 分配点失败，而是提前或延后触发其他容量路径。
- **Fix:** 调整该测试的 arena 和 `max_transactions`，继续稳定验证 STUN transaction 容量诊断。
- **Files modified:** `tests/test_peer_connection.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `cd331e2`

**2. [Rule 1 - Bug] 旧 datagram 测试仍假设 RTCP demux 是 no-op**
- **Found during:** Task 2
- **Issue:** 05-04 接入 RTCP receive 后，未 ready 的 RTCP datagram 会进入 SRTCP ready gate 并返回 `RTC_STATUS_INVALID_STATE`，旧测试仍期待 `RTC_STATUS_OK`。
- **Fix:** 更新旧测试预期，锁定 RTCP 与 RTP 一样必须通过 SRTP/SRTCP ready gate。
- **Files modified:** `tests/test_datagram.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `cb3a0e0`

---

**Total deviations:** 2 auto-fixed (Rule 1: 2)
**Impact on plan:** 均为本计划新增 RTCP 状态和安全路由导致的旧测试预期修正；没有扩大到线程、socket、动态内存或新依赖。

## Issues Encountered

- TDD RED 阶段均按预期失败：Task 1 失败于 `src/rtcp/rtcp.c` 不存在；Task 2 失败于 `rtc_media_send_rtcp_reports` 未实现。
- `rtc_rtcp_write_receiver_report` 当前已实现并测试 codec 基线；自动报告发送首版选择 SR/SDES，后续反馈和更完整 RR 调度可在 05-05/05-06 收口。

## Known Stubs

None - 本计划目标内没有保留阻塞性 stub。PLI/NACK 和自动 RTCP 定时调度仍按阶段计划留给后续计划。

## Threat Flags

无。新增 RTCP receive/send 信任边界已覆盖在计划 threat model T-05-10、T-05-11、T-05-12：unprotect 成功后才 parse，protect 成功后才输出，SDES CNAME 受固定 limit 限制。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure` - PASS
- Task 1 acceptance grep - PASS
- Task 2 acceptance grep - PASS
- Stub scan - PASS，无 TODO/FIXME/placeholder 或阻塞性空数据源。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

05-05 可以复用 `src/rtcp` common header 和 `rtc_media_handle_rtcp_datagram` 的 SRTCP unprotect gate，接入 PLI/NACK parse/write、typed feedback observer 和 NACK no-retransmit 可观测性。05-06 可以文档化 SR/SDES internal report helper、SRTCP 安全门和 CNAME limit。

## Self-Check: PASSED

- 文件存在性检查通过：`src/rtcp/rtcp.h`、`src/rtcp/rtcp.c`、`tests/test_rtcp.c`、`05-04-SUMMARY.md` 均存在。
- 提交存在性检查通过：`bf9cb5d`、`cd331e2`、`90926ae`、`cb3a0e0` 均可在 git log 中找到。
- 计划级验证通过：`cmake --build build && ctest --test-dir build --output-on-failure`。
- 验收 grep 通过：RTCP writer/parser、SRTCP protect/unprotect、`rtc_test_rtcp` 和 `srtcp_*_calls` 均已注册。

---
*Phase: 05-rtp-rtcp-media-plane*
*Completed: 2026-05-11*
