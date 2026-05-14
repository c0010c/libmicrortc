---
phase: 05-h264-opus
plan: "03"
subsystem: media-srtp-integration
tags: [c, cmake, srtp, rtp, h264, opus, transceiver]
requires:
  - phase: 05-h264-opus
    provides: MRTC_FRAME/transceiver API、H264/Opus SDP 合约、RTP packet helper、H264/Opus codec helpers、RTCP primitives
provides:
  - SRTP RTP/RTCP protect/unprotect private wrapper
  - H264/Opus write_frame 到 RTP/SRTP/media send hook 路径
  - protected RTP receive、depay/reassembly 和 transceiver on_frame delivery
  - Phase 5 media integration 来源追溯记录
affects: [phase-05-h264-opus, phase-06-chrome-e2e, media, srtp, rtp]
tech-stack:
  added: []
  patterns:
    - private PeerConnection media send hook 用于后续 transport 接入和单元测试观测
    - SRTP wrapper 在 MRTC_HAVE_SRTP 可用时走 libsrtp，否则使用显式 passthrough fallback
key-files:
  created:
    - tests/media/test_media_send.c
    - .planning/phases/05-h264-opus/05-03-SUMMARY.md
  modified:
    - CMakeLists.txt
    - src/srtp/srtp_session.h
    - src/srtp/srtp_session.c
    - src/media/media_transceiver.h
    - src/media/media_transceiver.c
    - src/peer_connection.c
    - tests/transport/test_dtls_srtp.c
    - .planning/phases/01-/SOURCE-MANIFEST.md
key-decisions:
  - "SRTP wrapper 在缺少系统 libsrtp 时保持 ready 但以 mrtc_srtp_session_is_passthrough() 明确标记非加密 fallback。"
  - "media send hook 保持 private，不把 application signaling、采集或编码引入核心库。"
  - "接收侧先实现顺序 H264 FU-A accumulation 和 Opus per-packet delivery，不搬迁完整 KVS jitter buffer。"
patterns-established:
  - "Protected-media private entry points: write_frame 负责 packetize/protect/send，receive_protected_media_packet 负责 unprotect/parse/depay/callback。"
  - "Frame counters and sequence counters only advance after successful packet send or callback delivery。"
requirements-completed: [API-04, PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06]
duration: 10min
completed: 2026-05-13
---

# Phase 5 Plan 03: SRTP 媒体发送/接收集成 Summary

**H264/Opus 编码帧经 RTP packetize、SRTP protect、private media send hook 发送，并支持 protected RTP 接收、depay/reassembly 与 on_frame 回调。**

## Performance

- **Duration:** 10 min
- **Started:** 2026-05-13T05:37:00Z
- **Completed:** 2026-05-13T05:46:59Z
- **Tasks:** 4/4
- **Files modified:** 9

## Accomplishments

- `src/srtp/srtp_session.*` 新增 RTP/RTCP protect/unprotect wrapper；本机未安装 libsrtp 时使用显式可检测 passthrough fallback，测试仍验证 RTP/RTCP round trip。
- `mrtc_transceiver_write_frame()` 将 H264 Annex-B 和 Opus encoded frames packetize 为 RTP，设置 payload type、SSRC、sequence、timestamp、marker，经 SRTP protect 后交给 private media send hook。
- 新增 protected RTP receive entry，完成 SRTP unprotect、RTP parse、SSRC/payload type route、H264 FU-A Annex-B reassembly、Opus payload delivery，并通过 `on_frame` 输出 `MRTC_FRAME`。
- `tests/media/test_media_send.c` 覆盖 H264 small NALU、多包 FU-A、Opus PT 111、未就绪 INVALID_STATE、sequence/frame counters、H264/Opus receive callback 和 unknown media packet rejection。
- `.planning/phases/01-/SOURCE-MANIFEST.md` 追加 Plan 03 所用本地 KVS SRTP/RTP/JitterBuffer/PeerConnection 来源追溯。

## Task Commits

1. **Task 1: 扩展 SRTP wrapper 支持 RTP/RTCP protect 与 unprotect** - `d6d265d` (feat)
2. **Task 2: 实现 transceiver write_frame 发送路径** - `86b434a` (feat)
3. **Task 3: 实现 SRTP RTP 接收、depay 和 on_frame 回调** - `94afd4c` (feat)
4. **Task 4: 更新 Phase 5 SRTP/media integration 来源追溯** - `50cefc3` (docs)

## Files Created/Modified

- `src/srtp/srtp_session.h` / `src/srtp/srtp_session.c` - SRTP key/salt 保存、libsrtp session context、RTP/RTCP protect/unprotect 和 passthrough 标记。
- `src/media/media_transceiver.h` / `src/media/media_transceiver.c` - private media hook API、send/receive counters 和 H264 receive buffer lifecycle。
- `src/peer_connection.c` - media send hook、write_frame RTP/SRTP 发送路径、protected RTP receive/depay/on_frame 路径。
- `tests/transport/test_dtls_srtp.c` - RTP/RTCP SRTP wrapper round-trip 和 fallback/active path 输出。
- `tests/media/test_media_send.c` - H264/Opus send/receive media integration 单元测试。
- `CMakeLists.txt` - 注册 `mrtc_media_send_test`。
- `.planning/phases/01-/SOURCE-MANIFEST.md` - 追加 Phase 5 Plan 03 来源追溯。

## Decisions Made

- fallback SRTP 不伪装成加密：`mrtc_srtp_session_is_passthrough()` 让测试和后续 verifier 能明确区分 libsrtp active 与 deterministic passthrough。
- private media send hook 是当前 transport seam；核心库仍不引入 application signaling、采集或编码，真实 ICE send 接入可在后续计划复用同一入口。
- 接收侧实现最小顺序 reassembly，足以覆盖 H264 FU-A 和 Opus payload 行为；完整 jitter/reorder buffer 仍按 Phase 5 patterns 留给后续扩展。

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `cmake` 配置阶段继续报告本机缺少可选 `MRTC_SRTP` / `MRTC_USRSCTP` 系统依赖；`MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS` 未启用，构建通过，并且本计划显式 fallback 测试覆盖该环境。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON` - PASS
- `cmake --build build` - PASS
- `ctest --test-dir build -R "dtls|srtp|media|h264|opus|peer_connection" --output-on-failure` - PASS，5/5 matched tests passed。
- `rg -n 'SrtpSession|PeerConnection/Rtp|JitterBuffer|MEDIA-04|MEDIA-06|\| `05` \|' .planning/phases/01-/SOURCE-MANIFEST.md` - PASS。

## Known Stubs

None. SRTP passthrough fallback 是本计划要求的 deterministic fallback，并通过 API 显式标记为非加密路径。

## Threat Flags

None - 本计划新增的 media send/receive 私有入口已在计划 threat model 中覆盖，并由 SRTP wrapper、send hook 和 receive depay tests 验证。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 04/05 可以基于当前 protected media send/receive entry 继续接入 RTCP/NACK、rolling buffer、fixtures 和后续 Chrome E2E；H264/Opus encoded media 已具备无浏览器单元可观测路径。

## Self-Check: PASSED

- Created files exist: `tests/media/test_media_send.c`, `.planning/phases/05-h264-opus/05-03-SUMMARY.md`
- Task commits exist: `d6d265d`, `86b434a`, `94afd4c`, `50cefc3`
- Plan verification commands passed.

---
*Phase: 05-h264-opus*
*Completed: 2026-05-13*
