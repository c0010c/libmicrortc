---
phase: 05-h264-opus
plan: "01"
subsystem: media-api-sdp
tags: [c, cmake, peerconnection, transceiver, h264, opus, sdp]
requires:
  - phase: 04-datachannel
    provides: ICE/DTLS/SRTP/SCTP/DataChannel 基础 PeerConnection owner 和 SDP/DataChannel 行为
provides:
  - MRTC public transceiver/media frame API contract
  - PeerConnection-owned private media transceiver list
  - H264/Opus media-aware SDP offer/answer helpers
  - Phase 5 source manifest traceability rows
affects: [phase-05-h264-opus, phase-06-chrome-e2e, media, sdp]
tech-stack:
  added: []
  patterns:
    - PeerConnection 持有 private transceiver 链表并负责释放
    - private SDP helper 接收 transceiver list 生成 deterministic media m-lines
key-files:
  created:
    - src/media/media_transceiver.h
    - src/media/media_transceiver.c
  modified:
    - include/micrortc/peer_connection.h
    - src/peer_connection.c
    - src/sdp.c
    - src/sdp.h
    - CMakeLists.txt
    - tests/peer_connection/test_peer_connection_api.c
    - tests/sdp/test_sdp_roundtrip.c
    - .planning/phases/01-/SOURCE-MANIFEST.md
key-decisions:
  - "媒体 public API 采用 MRTC 命名的 AWS 薄裁剪模型，不暴露 AWS/PIC 类型。"
  - "Plan 01 只建立 write_frame 校验与 INVALID_STATE stub，真实 SRTP/ICE media send 留给后续 Phase 5 计划。"
  - "SDP media m-line 由 PeerConnection transceiver list 驱动，DataChannel application m-line 保持兼容。"
patterns-established:
  - "Transceiver owner: PeerConnection append-only list assigns deterministic mids, payload type, SSRC and callback state."
  - "SDP media contract: H264 uses PT 96 and Opus uses PT 111 with exact Chrome-compatible rtpmap/fmtp/feedback lines."
requirements-completed: [API-04, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-05]
duration: 8min
completed: 2026-05-13
---

# Phase 5 Plan 01: 媒体 Public API、Transceiver 与 SDP 合约 Summary

**MRTC transceiver/frame public API、PeerConnection-owned media transceiver state，以及 deterministic H264/Opus SDP offer/answer 合约。**

## Performance

- **Duration:** 8 min
- **Started:** 2026-05-13T05:12:02Z
- **Completed:** 2026-05-13T05:19:34Z
- **Tasks:** 4/4
- **Files modified:** 10

## Accomplishments

- `include/micrortc/peer_connection.h` 暴露 `MRTC_RTP_TRANSCEIVER_HANDLE`、media kind、codec、direction、`MRTC_FRAME`、frame/PLI callbacks 和 transceiver write/free API，未引入 AWS/PIC 类型或采集/编码入口。
- `src/media/media_transceiver.*` 和 `src/peer_connection.c` 建立 private transceiver owner：PeerConnection 分配、挂载、释放 transceiver，并校验 write-frame null、zero-size、direction 和 media transport 未就绪状态。
- `src/sdp.*` 根据 transceiver list 生成 H264/Opus m-lines，包含 deterministic mids、ICE/fingerprint/setup、rtcp-mux、SSRC/msid、H264 nack/PLI、Opus fmtp，并保留 DataChannel SDP。
- `.planning/phases/01-/SOURCE-MANIFEST.md` 追加 Phase 5 API、media transceiver 和 SDP 派生来源追溯。

## Task Commits

1. **Task 1: 扩展 MRTC 媒体 public API** - `852f9e9` (feat)
2. **Task 2: 实现 private transceiver owner 和 PeerConnection 挂载** - `842a085` (feat)
3. **Task 3: 扩展 H264/Opus media-aware SDP 生成与解析** - `1ea777a` (feat)
4. **Task 4: 更新 Phase 5 API/SDP 来源追溯** - `fdd834c` (docs)

## Files Created/Modified

- `include/micrortc/peer_connection.h` - 新增 MRTC media/transceiver/frame public contract。
- `src/media/media_transceiver.h` - private transceiver owner 结构和 helper 声明。
- `src/media/media_transceiver.c` - transceiver 分配、释放、kind/codec/direction 校验和 direction 字符串 helper。
- `src/peer_connection.c` - PeerConnection 持有 transceiver list，新增 public transceiver API 实现和 SDP media helper 调用。
- `src/sdp.c` - 新增 transceiver-list driven media SDP generation。
- `src/sdp.h` - 新增 private media-aware offer/answer helper 声明。
- `CMakeLists.txt` - 将 `src/media/media_transceiver.c` 加入 `micrortc` target。
- `tests/peer_connection/test_peer_connection_api.c` - 覆盖 public API contract、transceiver creation/callback/write validation 和 PeerConnection SDP media output。
- `tests/sdp/test_sdp_roundtrip.c` - 覆盖 exact H264/Opus SDP lines、deterministic mids、rtcp-mux 和 DataChannel preservation。
- `.planning/phases/01-/SOURCE-MANIFEST.md` - 记录 Phase 5 本地 KVS 来源追溯。

## Decisions Made

- `mrtc_sdp_create_answer_ex()` 保持原有 no-media echo behavior，新增 `mrtc_sdp_create_answer_with_media()` 和 `mrtc_sdp_create_offer_with_media()` 承载 transceiver-list media SDP，避免破坏现有 private helper 调用。
- `mrtc_peer_connection_create_offer()` 在没有 media transceiver 时仍返回 `MRTC_STATUS_NOT_IMPLEMENTED`，但存在 media transceiver 时返回完整 offer，满足本计划对 media offer 的要求。
- `mrtc_transceiver_write_frame()` 当前完成参数与方向校验，SRTP/media transport 未接入时返回 `MRTC_STATUS_INVALID_STATE`，为后续 Plan 03 media send wiring 保留稳定入口。

## Deviations from Plan

None - plan executed as written.

## Issues Encountered

- Task 4 首次手动运行 manifest grep 时外层 shell 双引号触发了 Markdown backtick command substitution；立即用单引号重跑同一 grep，验证通过。实现和提交内容未受影响。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON` - PASS
- `cmake --build build` - PASS
- `ctest --test-dir build -R "peer_connection|sdp" --output-on-failure` - PASS，`mrtc_sdp_roundtrip_test` 和 `mrtc_peer_connection_api_test` 通过。
- `! rg -n "PCHAR|PVOID|\\bBOOL\\b|\\bUINT64\\b|(^|[^A-Z_])STATUS([^A-Z_]|$)|capture|encoder|gstreamer|ffmpeg|ogg|mp4" include/micrortc/peer_connection.h include/micrortc/micrortc.h` - PASS，无 public boundary violations。
- `rg -n '\\| `05` \\||phase_added.*05|API-04|MEDIA-03|MEDIA-05' .planning/phases/01-/SOURCE-MANIFEST.md` - PASS。

## Known Stubs

None. `mrtc_transceiver_write_frame()` 的 media transport 未就绪返回 `MRTC_STATUS_INVALID_STATE` 是本计划明确要求的 Plan 03 前置行为，不是未声明 stub。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 02+ 可以直接基于 `MRTC_FRAME`、transceiver owner state、payload type/SSRC/MID 和 deterministic H264/Opus SDP 合约实现 RTP/codec/RTCP/SRTP media paths。

## Self-Check: PASSED

- Created files exist: `src/media/media_transceiver.h`, `src/media/media_transceiver.c`, `.planning/phases/05-h264-opus/05-01-SUMMARY.md`
- Task commits exist: `852f9e9`, `842a085`, `1ea777a`, `fdd834c`
- Plan verification commands passed.

---
*Phase: 05-h264-opus*
*Completed: 2026-05-13*
