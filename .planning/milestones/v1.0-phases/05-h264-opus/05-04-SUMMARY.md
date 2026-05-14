---
phase: 05-h264-opus
plan: "04"
subsystem: rtcp-media-recovery
tags: [c, cmake, rtp, rtcp, nack, pli, h264, opus]
requires:
  - phase: 05-h264-opus
    provides: protected media send/receive private entry points、SRTP RTP/RTCP wrappers、media send hook、transceiver counters、H264/Opus receive delivery
provides:
  - RTP rolling buffer，按 sequence number 保存可重发的 protected packet bytes
  - protected RTCP NACK ingress 到 media send hook 的重传路径
  - PLI 到 video transceiver on_picture_loss callback 的行为路由
  - transceiver counter 驱动的 SR/RR 生成与解析观测
  - Phase 5 RTCP/NACK/PLI 来源追溯记录
affects: [phase-05-h264-opus, phase-06-chrome-e2e, media, rtp, rtcp, srtp]
tech-stack:
  added: []
  patterns:
    - protected RTP bytes 在发送成功后写入 transceiver rolling buffer，NACK 重发直接复用原始 protected bytes
    - protected RTCP ingress 统一 unprotect 后按 RTCP packet type 分派到 NACK/PLI/SR/RR 行为
key-files:
  created:
    - src/rtcp/rtp_rolling_buffer.h
    - src/rtcp/rtp_rolling_buffer.c
    - src/rtcp/retransmitter.h
    - src/rtcp/retransmitter.c
    - tests/media/test_rtp_rolling_buffer.c
    - tests/media/test_rtcp_retransmit.c
    - tests/media/test_rtcp_pli_sr_rr.c
    - .planning/phases/05-h264-opus/05-04-SUMMARY.md
  modified:
    - CMakeLists.txt
    - src/media/media_transceiver.h
    - src/media/media_transceiver.c
    - src/peer_connection.c
    - .planning/phases/01-/SOURCE-MANIFEST.md
key-decisions:
  - "RTP rolling buffer 保存已 protected RTP packet bytes，NACK 重发不重新 SRTP protect 旧 sequence。"
  - "未知 SSRC 和缺失 sequence 走确定性非崩溃路径：未知 SSRC 返回 INVALID_STATE，缺包通过 missing_count/counter 暴露。"
  - "Plan 04 只实现 SR/RR/NACK/PLI 最小行为，不实现 REMB、TWCC、FIR、SLI 或拥塞控制。"
patterns-established:
  - "RTCP behavior ingress: receive_protected_rtcp_packet 先 SRTP unprotect，再由 packet type 驱动 media recovery 行为。"
  - "Transceiver RTCP counters provide behavior-level test observability without exposing application signaling/capture/encoding。"
requirements-completed: [PROTO-04, API-04, MEDIA-03, MEDIA-04]
duration: 13min
completed: 2026-05-13
---

# Phase 5 Plan 04: RTCP SR/RR、NACK 重传与 PLI 回调 Summary

**RTCP NACK/PLI/SR/RR 从 packet primitive 扩展为可驱动媒体恢复行为：NACK 从 RTP rolling buffer 重发，PLI 触发视频 picture-loss callback，SR/RR 使用 transceiver counters 验证。**

## Performance

- **Duration:** 13 min
- **Started:** 2026-05-13T05:51:31Z
- **Completed:** 2026-05-13T06:04:29Z
- **Tasks:** 4/4
- **Files modified:** 13

## Accomplishments

- 新增 `src/rtcp/rtp_rolling_buffer.*`，固定容量保存可重发 RTP packet bytes，覆盖 overwrite、lookup、missing、copy 和 remove。
- 将 transceiver 发送成功后的 protected RTP bytes 写入 rolling buffer；NACK ingress 通过 `mrtc_srtp_unprotect_rtcp()`、NACK PID/BLP 解析、SSRC 查找和 media send hook 完成重传。
- 新增 `src/rtcp/retransmitter.*`，未知 SSRC 不触发 hook，缺失 sequence 通过 `missing_count` 和 transceiver counters 暴露。
- PLI 路由到 matching video transceiver 的 `on_picture_loss` callback；未知 SSRC 返回确定性 `MRTC_STATUS_INVALID_STATE`。
- SR/RR 生成与解析接入 transceiver RTP packet/octet/timestamp counters，并通过行为测试验证 packet type 200/201 与字段值。
- `.planning/phases/01-/SOURCE-MANIFEST.md` 追加 RTCP/NACK/PLI 相关 KVS 来源追溯。

## Task Commits

1. **Task 1: 实现 RTP rolling buffer** - `7dd43d4` (feat)
2. **Task 2: 接入 NACK 解析与重发** - `5f180f4` (feat)
3. **Task 3: 实现 PLI callback 和 SR/RR 基础处理** - `6dce0b9` (feat)
4. **Task 4: 更新 RTCP/NACK 来源追溯** - `aabff3c` (docs)

## Files Created/Modified

- `src/rtcp/rtp_rolling_buffer.h` / `src/rtcp/rtp_rolling_buffer.c` - RTP sequence keyed rolling byte buffer。
- `src/rtcp/retransmitter.h` / `src/rtcp/retransmitter.c` - NACK sequence list 到 protected packet retransmit 的私有编排。
- `src/media/media_transceiver.h` / `src/media/media_transceiver.c` - rolling buffer lifecycle、RTP/RTCP counters、SR/RR generation helpers。
- `src/peer_connection.c` - protected RTCP ingress，NACK/PLI/SR/RR dispatch，sent RTP byte retention。
- `tests/media/test_rtp_rolling_buffer.c` - rolling buffer overwrite/lookup/missing/remove 单元测试。
- `tests/media/test_rtcp_retransmit.c` - NACK 重传、未知 SSRC、缺包 result/counter 行为测试。
- `tests/media/test_rtcp_pli_sr_rr.c` - PLI callback 和 SR/RR counter 行为测试。
- `CMakeLists.txt` - 注册 RTCP rolling/retransmit/PLI SR RR 测试和新 core source。
- `.planning/phases/01-/SOURCE-MANIFEST.md` - 追加 Phase 5 Plan 04 来源追溯。

## Decisions Made

- rolling buffer 保存 protected RTP bytes，而不是未保护 RTP bytes；重传直接走 media send hook，避免对旧 sequence number 再次 SRTP protect。
- 缺失重传包不是 fatal：NACK 请求仍返回 OK，并通过 `MRTC_RTCP_RETRANSMIT_RESULT.missing_count` 与 transceiver `retransmit_packets_missing` 暴露。
- protected RTCP ingress 保持 private，不把 signaling、采集或编码职责引入核心库。

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `cmake` 配置阶段继续报告本机缺少可选 `MRTC_SRTP` / `MRTC_USRSCTP` 系统依赖；`MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS` 未启用，构建通过。当前测试环境使用 Plan 03 已记录的 SRTP passthrough fallback。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON` - PASS
- `cmake --build build` - PASS
- `ctest --test-dir build -R "rolling|rtcp|rtp" --output-on-failure` - PASS，4/4 matched tests passed。
- `ctest --test-dir build -R "nack|retransmit|rtcp" --output-on-failure` - PASS，2/2 matched tests passed。
- `ctest --test-dir build -R "pli|rtcp|media" --output-on-failure` - PASS，4/4 matched tests passed。
- `ctest --test-dir build -R "rtcp|nack|retransmit|pli|rolling" --output-on-failure` - PASS，4/4 matched tests passed。
- `ctest --test-dir build --output-on-failure` - PASS，16/16 tests passed。
- `rg -n 'RtpRollingBuffer|RollingBuffer|Retransmitter|PeerConnection/Rtcp|PROTO-04|\| `05` \|' .planning/phases/01-/SOURCE-MANIFEST.md` - PASS。

## Known Stubs

None.

## Threat Flags

None - 本计划新增 protected RTCP ingress、NACK 重传和 PLI callback 路径均在计划 threat model 中覆盖，并由行为测试验证。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 05/Phase 6 可以复用当前 RTCP behavior ingress、protected media send hook、rolling retransmit buffer 和 transceiver counters，继续补 fixture harness、Chrome E2E 和真实 transport 发送集成。当前实现仍按计划不包含 REMB/TWCC/FIR/SLI/拥塞控制。

## Self-Check: PASSED

- Created files exist: `src/rtcp/rtp_rolling_buffer.c`, `src/rtcp/retransmitter.c`, `tests/media/test_rtp_rolling_buffer.c`, `tests/media/test_rtcp_retransmit.c`, `tests/media/test_rtcp_pli_sr_rr.c`, `.planning/phases/05-h264-opus/05-04-SUMMARY.md`
- Task commits exist: `7dd43d4`, `5f180f4`, `6dce0b9`, `aabff3c`
- Plan verification commands passed.

---
*Phase: 05-h264-opus*
*Completed: 2026-05-13*
