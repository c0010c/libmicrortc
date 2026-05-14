---
phase: 05-h264-opus
plan: "02"
subsystem: rtp-codec-rtcp-primitives
tags: [c, cmake, rtp, rtcp, h264, opus, annex-b, nack, pli]
requires:
  - phase: 05-h264-opus
    provides: MRTC_FRAME、transceiver public API、H264/Opus SDP 合约和 Phase 5 来源追溯模式
provides:
  - RTP v2 private packet build/serialize/parse helper
  - H264 Annex-B Single NALU/FU-A packetize 和 Annex-B depacketize helper
  - Opus direct payload/depayload 和 100ns 到 48kHz RTP timestamp helper
  - RTCP SR/RR/NACK/PLI 基础 generate/parse helper
  - Phase 5 RTP/codec/RTCP 来源 manifest 行
affects: [phase-05-h264-opus, phase-06-chrome-e2e, media, rtp, rtcp]
tech-stack:
  added: []
  patterns:
    - RTP/codec/RTCP primitives 保持 private src/ 模块并由 CTest 直接覆盖
    - codec helper 不引入采集、编码、容器解析或应用层 signaling
key-files:
  created:
    - src/rtp/rtp_packet.h
    - src/rtp/rtp_packet.c
    - src/rtp/codecs/h264.h
    - src/rtp/codecs/h264.c
    - src/rtp/codecs/opus.h
    - src/rtp/codecs/opus.c
    - src/rtcp/rtcp_packet.h
    - src/rtcp/rtcp_packet.c
    - tests/media/test_rtp_packet.c
    - tests/media/test_h264_packetizer.c
    - tests/media/test_opus_codec.c
    - tests/media/test_rtcp_packet.c
  modified:
    - CMakeLists.txt
    - .planning/phases/01-/SOURCE-MANIFEST.md
key-decisions:
  - "H264 helper 仅接受 Annex-B bytestream；无 start code 或 AVCC-like length-prefixed 输入返回 MRTC_STATUS_PARSE_ERROR。"
  - "Opus timestamp 明确按 MRTC_FRAME.presentation_ts 的 100ns 单位转换为 48kHz RTP timestamp。"
  - "RTCP helpers 先提供 SR/RR/NACK/PLI packet primitive，不在本计划接入 PeerConnection 行为编排。"
patterns-established:
  - "Private packet primitive tests: tests/media/* 通过 src/ 私有头直接验证 RTP/codec/RTCP 行为。"
  - "Deterministic parse errors: RTP/H264/RTCP malformed input 使用 MRTC_STATUS_PARSE_ERROR。"
requirements-completed: [PROTO-04, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06]
duration: 8min
completed: 2026-05-13
---

# Phase 05 Plan 02: RTP Packet 与 H264/Opus Codec Primitives Summary

**RTP v2 packet、H264 Annex-B/FU-A、Opus 直拷贝和 RTCP SR/RR/NACK/PLI 的 private C primitives。**

## Performance

- **Duration:** 8 min
- **Started:** 2026-05-13T05:25:33Z
- **Completed:** 2026-05-13T05:32:18Z
- **Tasks:** 5/5
- **Files modified:** 14

## Accomplishments

- 新增 `src/rtp/rtp_packet.*`，支持 RTP v2 build、serialize、parse、header size 计算、raw/payload 指针记录和 16-bit sequence rollover。
- 新增 `src/rtp/codecs/h264.*`，支持 Annex-B 3/4 字节 start code 扫描，小 NALU 输出 Single NALU，大 NALU 输出 FU-A，并将接收 payload depacketize 为 4 字节 start code Annex-B。
- 新增 `src/rtp/codecs/opus.*`，实现 Opus payload/depayload 直拷贝，并将 `MRTC_FRAME.presentation_ts` 按 100ns 单位转换为 48kHz RTP timestamp。
- 新增 `src/rtcp/rtcp_packet.*`，覆盖 RTCP header、SR/RR generate/parse、NACK PID/BLP sequence extraction 和 PLI media SSRC generate/parse。
- 更新 CMake 测试入口和 Phase 1 source manifest，记录所有新增 RTP/codec/RTCP 文件的本地 KVS 来源追溯。

## Task Commits

1. **Task 1: 实现 private RTP packet parse/serialize helper** - `1fd1e01` (feat)
2. **Task 2: 实现 H264 Annex-B packetize/depacketize** - `8096e49` (feat)
3. **Task 3: 实现 Opus direct payload/depayload 和 timestamp helper** - `f758080` (feat)
4. **Task 4: 实现基础 RTCP packet helpers** - `f4bf3ed` (feat)
5. **Task 5: 更新 Phase 5 RTP/codec/RTCP 来源追溯** - `e746df7` (docs)

## Files Created/Modified

- `src/rtp/rtp_packet.h` / `src/rtp/rtp_packet.c` - RTP v2 private packet helper。
- `src/rtp/codecs/h264.h` / `src/rtp/codecs/h264.c` - H264 Annex-B packetize/depacketize helper。
- `src/rtp/codecs/opus.h` / `src/rtp/codecs/opus.c` - Opus direct payload/depayload 和 timestamp helper。
- `src/rtcp/rtcp_packet.h` / `src/rtcp/rtcp_packet.c` - RTCP SR/RR/NACK/PLI helper。
- `tests/media/test_rtp_packet.c` - RTP serialize/parse round-trip、invalid packet 和 sequence rollover 覆盖。
- `tests/media/test_h264_packetizer.c` - 3/4 字节 start code、Single NALU、FU-A flags/reassembly 和 invalid AVCC-like input 覆盖。
- `tests/media/test_opus_codec.c` - Opus payload/depayload 字节保持和 `200000` 100ns 到 `960` RTP timestamp 覆盖。
- `tests/media/test_rtcp_packet.c` - SR/RR type parse、NACK PID/BLP 展开和 PLI SSRC 往返覆盖。
- `CMakeLists.txt` - 将新增 core sources 和 CTest executables 接入构建。
- `.planning/phases/01-/SOURCE-MANIFEST.md` - 追加本地 KVS RTP/codec/RTCP 来源追溯行。

## Decisions Made

- H264 packetizer 拒绝非 Annex-B 输入，不尝试兼容 AVCC length-prefixed 数据，保持 Phase 5 D-06 边界清晰。
- H264 depacketizer 对 Single NALU 和 FU-A start fragment 输出 4 字节 Annex-B start code；FU-A middle/end fragment 输出连续 NALU bytes，调用方可顺序拼接。
- RTCP RR generator 生成一个 receiver report block，满足当前 SR/RR primitive 测试和后续编排层接入需求。

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `cmake` 配置阶段继续报告本机缺少可选 `MRTC_SRTP` / `MRTC_USRSCTP` 系统依赖；当前配置未启用 `MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS`，构建和本计划测试均通过。这是既有可选依赖状态，不影响 05-02 primitives。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON` - PASS
- `cmake --build build` - PASS
- `ctest --test-dir build -R "rtp" --output-on-failure` - PASS
- `ctest --test-dir build -R "h264|rtp" --output-on-failure` - PASS
- `ctest --test-dir build -R "opus|rtp" --output-on-failure` - PASS
- `ctest --test-dir build -R "rtcp|nack|pli" --output-on-failure` - PASS
- `ctest --test-dir build -R "rtp|h264|opus|rtcp|nack|pli" --output-on-failure` - PASS，5/5 matched tests passed。
- `rg -n "RtpPacket|RtpH264Payloader|RtpOpusPayloader|RtcpPacket|PROTO-04|MEDIA-06" .planning/phases/01-/SOURCE-MANIFEST.md` - PASS。

## Known Stubs

None.

## Threat Flags

None - 本计划只新增 private packet parsing/generation primitives 和本地单元测试，没有新增 network endpoint、auth path、文件访问边界或 schema 变更。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 03 可以直接复用 RTP packet helper、H264/Opus codec helper 和 RTCP primitive，将 media send/receive 编排接入 transceiver、SRTP 和 ICE send path。

## Self-Check: PASSED

- Created files exist: `src/rtp/rtp_packet.h`, `src/rtp/rtp_packet.c`, `src/rtp/codecs/h264.h`, `src/rtp/codecs/h264.c`, `src/rtp/codecs/opus.h`, `src/rtp/codecs/opus.c`, `src/rtcp/rtcp_packet.h`, `src/rtcp/rtcp_packet.c`, `tests/media/test_rtp_packet.c`, `tests/media/test_h264_packetizer.c`, `tests/media/test_opus_codec.c`, `tests/media/test_rtcp_packet.c`
- Task commits exist: `1fd1e01`, `8096e49`, `f758080`, `f4bf3ed`, `e746df7`
- Plan verification commands passed.

---
*Phase: 05-h264-opus*
*Completed: 2026-05-13*
