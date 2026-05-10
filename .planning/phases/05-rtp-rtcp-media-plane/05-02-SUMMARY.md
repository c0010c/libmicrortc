---
phase: 05-rtp-rtcp-media-plane
plan: 02
subsystem: media
tags: [c, rtp, opus, h264, srtp, fixed-memory]

requires:
  - phase: 05-rtp-rtcp-media-plane
    provides: 05-01 typed media API、media limits、固定 media queue slots 和 SRTP wrapper 调用边界
provides:
  - Opus frame 到受保护 RTP datagram 的发送链路
  - H264 Annex B access unit single NALU 与 FU-A RTP packetize
  - SRTP protect 失败时不输出明文 RTP 的测试锁定
affects: [05-03-rtp-receive, 05-04-rtcp, 05-06-docs, phase-6-chrome-e2e]

tech-stack:
  added: []
  patterns:
    - 纯 C RTP packetizer
    - media executor packetize 到 network executor SRTP protect/output
    - create-time 固定 packet slot，无运行期动态分配

key-files:
  created:
    - src/rtp/rtp.h
    - src/rtp/rtp.c
    - src/media/media.h
    - src/media/media.c
    - tests/test_rtp.c
  modified:
    - CMakeLists.txt
    - src/api/peer_connection.c
    - src/api/peer_connection.h
    - tests/test_main.c

key-decisions:
  - "Opus payload type 固定使用 Chrome profile 的 PT 111；H264 固定使用 PT 103。"
  - "RTP packet slot 容量按 RTP header + max_payload_bytes + SRTP trailer 预留，避免 protect 阶段写出固定槽。"
  - "H264 发送首版只接受 Annex B start code access unit，非 Annex B 返回 unsupported。"

patterns-established:
  - "RTP send path: media executor packetize，network executor 调用 `rtc_srtp_protect_rtp`，成功后复用 `observer.on_datagram`。"
  - "H264 FU-A: `max_payload_bytes - 2` 作为分片负载上限，marker bit 只在 access unit 最后一包设置。"

requirements-completed: [RTP-01, RTP-03]

duration: 10min
completed: 2026-05-11
---

# Phase 05 Plan 02: RTP 发送 packetize 与 SRTP 输出 Summary

**Opus 与 H264 编码帧现在可以在固定内存发送路径中生成 RTP，并且只在 SRTP protect 成功后输出 datagram。**

## Performance

- **Duration:** 10min
- **Started:** 2026-05-10T16:00:47Z
- **Completed:** 2026-05-10T16:10:53Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- 新增 `src/rtp`，实现 RTP 固定头写入、Opus 单包 packetize、H264 Annex B single NALU 和 FU-A packetize。
- 新增 `src/media`，将 `rtc_peer_connection_send_media_frame` 接入 media executor packetize、固定 slot、network executor SRTP protect 和 `observer.on_datagram` 输出。
- 用 deterministic security backend 测试锁定 Opus/H264 protect 调用、受保护 datagram 输出、H264 marker bit、FU-A header 和容量失败路径。
- 明确保护失败安全边界：`rtc_srtp_protect_rtp` 失败时不调用 `observer.on_datagram`，避免明文 RTP 泄漏。

## Task Commits

1. **Task 1 RED: Opus RTP 发送失败测试** - `5ca6699` (test)
2. **Task 1 GREEN: Opus RTP send path** - `911b42d` (feat)
3. **Task 2 RED: H264 RTP 发送失败测试** - `080c126` (test)
4. **Task 2 GREEN: H264 single NALU 与 FU-A packetize** - `8a8d8b8` (feat)

**Plan metadata:** 见最终 docs commit

## Files Created/Modified

- `src/rtp/rtp.h` / `src/rtp/rtp.c` - RTP header writer、Opus packetizer、H264 Annex B single NALU/FU-A packetizer。
- `src/media/media.h` / `src/media/media.c` - media send frame 内部入口、固定 slot 管理、SRTP protect/output gate。
- `src/api/peer_connection.c` / `src/api/peer_connection.h` - 接入 `rtc_media_send_frame`，新增 RTP sender state 和 packet slot metadata。
- `tests/test_rtp.c` - Opus/H264 RTP 发送、SRTP protect、marker、容量和失败不泄漏测试。
- `CMakeLists.txt` / `tests/test_main.c` - 注册 RTP/media 源文件和测试。

## Decisions Made

- Opus timestamp 使用 frame timestamp；未提供时按 48 kHz clock 每包递增 960。
- H264 timestamp 使用 frame timestamp；未提供时沿用 sender state，并在成功 access unit 后按固定 90 kHz 步长推进。
- `max_payload_bytes` 表示 RTP payload 上限；内部 packet slot 额外预留 RTP header 和 SRTP trailer。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] RTP packet slot 需要包含 SRTP protect 扩展空间**
- **Found during:** Task 1
- **Issue:** 05-01 建立的 media packet cache 按 `max_payload_bytes` 分配，但 RTP 发送需要同时容纳 RTP header 和 SRTP authentication/tag 扩展。
- **Fix:** 新增 `media_packet_capacity = RTP header + max_payload_bytes + SRTP trailer`，固定 slot 按完整 packet 容量切分。
- **Files modified:** `src/api/peer_connection.c`, `src/api/peer_connection.h`, `src/rtp/rtp.h`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `911b42d`

**2. [Rule 1 - Bug] FU-A helper 不应把有效 payload 上限限制到 256 字节**
- **Found during:** Task 2
- **Issue:** 初始 FU-A 实现使用 256 字节临时栈缓冲，会错误拒绝合法的较大 `max_payload_bytes` 配置。
- **Fix:** FU-A 直接写入目标 RTP packet slot，不再使用临时 payload 缓冲。
- **Files modified:** `src/rtp/rtp.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `8a8d8b8`

---

**Total deviations:** 2 auto-fixed (Rule 1: 1, Rule 2: 1)
**Impact on plan:** 均为固定内存和安全输出边界的正确性修复，没有引入线程、socket 或 GPL/LGPL 依赖。

## Issues Encountered

- TDD RED 阶段按预期失败：Task 1 失败于有效 media send 仍返回 unsupported；Task 2 失败于 H264 kind 尚未实现。
- H264 FU-A 实现完成后补了一次容量边界修正，避免对合法 payload 上限形成隐藏限制。

## Known Stubs

None - 本计划目标内没有保留阻塞性 stub。RTP 接收、RTCP 和 PLI/NACK 仍按阶段计划留给 05-03 到 05-05。

## Threat Flags

无。新增 RTP 发送、SRTP protect gate、H264 容量限制均已覆盖在计划 threat model T-05-04、T-05-05、T-05-06 范围内。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure` - PASS
- `grep -R "malloc\\|calloc\\|realloc" src/rtp src/media` - PASS，无新增运行期动态分配。
- Task 1 acceptance grep - PASS
- Task 2 acceptance grep - PASS

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

05-03 可以复用 `src/rtp` 的 RTP header 语义和 `src/media` 的固定 slot 模式，实现 SRTP unprotect 后的 Opus/H264 depacketize 与 typed frame 输出。05-04/05-05 可以沿用同一 network executor protect/output gate 接入 SRTCP。

## Self-Check: PASSED

- 文件存在性检查通过：`src/rtp/rtp.h`、`src/rtp/rtp.c`、`src/media/media.h`、`src/media/media.c`、`tests/test_rtp.c`、`05-02-SUMMARY.md` 均存在。
- 提交存在性检查通过：`5ca6699`、`911b42d`、`080c126`、`8a8d8b8` 均可在 git log 中找到。
- 计划级验证通过：`cmake --build build && ctest --test-dir build --output-on-failure`。
- 边界检查通过：`grep -R "malloc\\|calloc\\|realloc" src/rtp src/media` 无输出。

---
*Phase: 05-rtp-rtcp-media-plane*
*Completed: 2026-05-11*
