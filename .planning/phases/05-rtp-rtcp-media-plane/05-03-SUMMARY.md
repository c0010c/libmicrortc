---
phase: 05-rtp-rtcp-media-plane
plan: 03
subsystem: media
tags: [c, rtp, srtp, opus, h264, fixed-memory]

requires:
  - phase: 05-rtp-rtcp-media-plane
    provides: 05-02 Opus/H264 RTP 发送 packetize、固定 media slots 和 SRTP protect/output gate
provides:
  - RTP datagram 到 SRTP unprotect 后 typed Opus frame 的接收路径
  - H264 single NALU、FU-A 和有限 STAP-A 接收重组
  - unprotect 失败、sequence gap 和 reassembly capacity 失败不输出媒体帧的测试锁定
affects: [05-04-rtcp, 05-05-feedback, 05-06-docs, phase-6-chrome-e2e]

tech-stack:
  added: []
  patterns:
    - network executor SRTP unprotect 后投递 media executor
    - 固定 media slot 复制 datagram，不保存调用方 buffer
    - H264 reassembly 使用 create-time 固定 buffer

key-files:
  created:
    - .planning/phases/05-rtp-rtcp-media-plane/05-03-SUMMARY.md
  modified:
    - src/api/peer_connection.c
    - src/api/peer_connection.h
    - src/media/media.h
    - src/media/media.c
    - src/rtp/rtp.h
    - src/rtp/rtp.c
    - tests/test_rtp.c
    - tests/test_datagram.c

key-decisions:
  - "RTP 接收路径必须先复制到固定 slot 并完成 `rtc_srtp_unprotect_rtp`，成功后才解析 RTP header 和输出媒体帧。"
  - "H264 接收 access unit 以重组后的 NALU bytes 输出；single NALU、FU-A 和 STAP-A 都写入同一个 create-time reassembly buffer。"
  - "sequence gap、missing start、STAP-A length 越界和 reassembly capacity 超限均丢弃当前 AU，不调用 media observer。"

patterns-established:
  - "RTP receive gate: `rtc_peer_connection_receive_datagram` 的 RTP 分支只路由到 `rtc_media_handle_rtp_datagram`，RTCP 仍留给 05-04。"
  - "H264 drop observability: media counter `h264_reassembly_drops` 配合 trace reason `h264_sequence_gap` / `h264_reassembly_capacity`。"

requirements-completed: [RTP-02, RTP-04]

duration: 7min
completed: 2026-05-11
---

# Phase 05 Plan 03: RTP 接收 depacketize 与 H264 重组 Summary

**受保护 RTP datagram 现在会先经 SRTP unprotect，再输出 typed Opus frame 或重组后的 H264 access unit。**

## Performance

- **Duration:** 7min
- **Started:** 2026-05-10T16:14:45Z
- **Completed:** 2026-05-10T16:21:34Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- 将 `RTC_NET_PROTOCOL_RTP` 接入 `rtc_media_handle_rtp_datagram`，RTP datagram 复制进固定 media slot 后才调用 `rtc_srtp_unprotect_rtp`。
- 新增 RTP header parse 和 Opus 接收输出，typed callback 收到 `RTC_MEDIA_KIND_AUDIO_OPUS`、payload length 和 RTP timestamp。
- 新增 `rtc_rtp_depacketize_h264`，支持 H264 single NALU、FU-A 和有限 STAP-A 接收重组。
- 用测试锁定 unprotect 失败、FU-A sequence gap、reassembly capacity 不足时不输出媒体帧，并递增对应 drop counter。

## Task Commits

1. **Task 1 RED: Opus RTP 接收失败测试** - `dd03447` (test)
2. **Task 1 GREEN: Opus RTP receive path** - `31ad63b` (feat)
3. **Task 2 RED: H264 RTP 接收失败测试** - `2736a95` (test)
4. **Task 2 GREEN: H264 depacketize/reassembly** - `b7550d8` (feat)

**Plan metadata:** 见最终 docs commit

## Files Created/Modified

- `src/api/peer_connection.c` - RTP demux 分支接入 media receive handler。
- `src/api/peer_connection.h` - media slot 增加 RTP metadata，PeerConnection 增加 H264 reassembly state。
- `src/media/media.h` / `src/media/media.c` - 新增 RTP datagram receive handler、SRTP unprotect gate、Opus typed 输出和 H264 reassembly 调度。
- `src/rtp/rtp.h` / `src/rtp/rtp.c` - 新增 RTP header parse 和 H264 depacketize/reassembly helper。
- `tests/test_rtp.c` - 覆盖 Opus/H264 接收、unprotect failure、FU-A gap 和 capacity drop。
- `tests/test_datagram.c` - 更新旧 demux 测试对 RTP 分支的期望：RTP 现在会进入 SRTP ready gate。

## Decisions Made

- H264 receive 输出重组后的连续 NALU bytes，不额外插入 Annex B start code；后续文档可在 05-06 明确该 callback 形态。
- STAP-A 首版只解析 16-bit NALU length 和 payload，长度越界立即 protocol error。
- RTCP 分支仍未接入 RTP handler，保留给 05-04 的 SRTCP/RTCP 路径。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 旧 datagram 测试仍假设 RTP demux 是 no-op**
- **Found during:** Task 1
- **Issue:** 05-03 接入 RTP receive 后，旧 `tests/test_datagram.c` 仍期待 RTP datagram 返回 `RTC_STATUS_OK`；实际现在会进入 SRTP ready gate，未 ready 时返回 `RTC_STATUS_INVALID_STATE`。
- **Fix:** 更新该测试只验证 demux counter 和新的 RTP 路由行为，RTCP 分支仍保持未误路由。
- **Files modified:** `tests/test_datagram.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `31ad63b`

---

**Total deviations:** 1 auto-fixed (Rule 1: 1)
**Impact on plan:** 修正既有测试预期以匹配本计划新增的 RTP 接收行为，没有扩大到 RTCP 或新增依赖。

## Issues Encountered

- TDD RED 阶段按预期失败：Task 1 失败于 RTP datagram 未调用 unprotect；Task 2 失败于 receive-side H264 payload type 仍 unsupported。
- H264 capacity 路径使用 `RTC_STATUS_CAPACITY_PACKET_CACHE` 保持与既有 RTP packet/cache 容量错误一致。

## Known Stubs

None - 本计划目标内没有保留阻塞性 stub。RTCP SR/RR/SDES 和 PLI/NACK 仍按阶段计划留给 05-04 与 05-05。

## Threat Flags

无。新增 RTP receive、SRTP unprotect gate 和 H264 reassembly drop 行为均覆盖在计划 threat model T-05-07、T-05-08、T-05-09 范围内。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure` - PASS
- `grep -R "RTC_NET_PROTOCOL_RTCP" src/api/peer_connection.c` - PASS，RTCP 仍未误路由到 RTP handler。
- Task 1 acceptance grep - PASS
- Task 2 acceptance grep - PASS
- Stub scan - PASS，无 TODO/FIXME/placeholder/空 mock 数据。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

05-04 可以在现有 datagram demux 和固定 media slot 模式上接入 SRTCP unprotect、RTCP SR/RR/SDES parse/write 和 stats。05-05 可以复用 typed media feedback observer 上报 PLI/NACK。

## Self-Check: PASSED

- 文件存在性检查通过：`src/rtp/rtp.h`、`src/rtp/rtp.c`、`src/media/media.h`、`src/media/media.c`、`tests/test_rtp.c`、`05-03-SUMMARY.md` 均存在。
- 提交存在性检查通过：`dd03447`、`31ad63b`、`2736a95`、`b7550d8` 均可在 git log 中找到。
- 计划级验证通过：`cmake --build build && ctest --test-dir build --output-on-failure`。
- RTCP 路由检查通过：`grep -R "RTC_NET_PROTOCOL_RTCP" src/api/peer_connection.c` 仍只显示 RTCP demux case，没有接入 RTP handler。

---
*Phase: 05-rtp-rtcp-media-plane*
*Completed: 2026-05-11*
