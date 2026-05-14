---
phase: 05-h264-opus
plan: "05"
subsystem: media-fixture-harness
tags: [c, cmake, h264, opus, rtp, rtcp, srtp, fixtures]
requires:
  - phase: 05-h264-opus
    provides: MRTC media API、RTP/H264/Opus/RTCP primitives、SRTP media send/receive、NACK/PLI 行为
provides:
  - 固定 H264 Annex-B SPS/PPS/IDR fixture
  - 固定 big-endian 16-bit length-prefixed Opus packet fixture
  - Phase 5 C fixture media verifier
  - installed package consumer media public API coverage
  - Phase 5 README/ROADMAP/STATE/source manifest 收口记录
affects: [phase-05-h264-opus, phase-06-chrome-e2e, media, packaging, verification]
tech-stack:
  added: []
  patterns:
    - 固定媒体 fixture 使用 C reader/harness 验证编码后媒体路径，不引入解码器或容器 parser
    - integration verifier 输出逐层成功 label，便于区分 build、CTest、media verifier 和 package consumer 失败
key-files:
  created:
    - tests/fixtures/h264_annexb_sample.h264
    - tests/fixtures/opus_packets.bin
    - tests/media/test_media_fixtures.c
    - tests/integration/mrtc_phase5_media_verify.c
    - .planning/phases/05-h264-opus/05-05-SUMMARY.md
  modified:
    - CMakeLists.txt
    - src/peer_connection.c
    - cmake/micrortcConfig.cmake.in
    - tests/package-consumer/package_consumer.c
    - README.md
    - .planning/ROADMAP.md
    - .planning/STATE.md
    - .planning/phases/01-/SOURCE-MANIFEST.md
key-decisions:
  - "Phase 5 固定媒体验收采用 C fixture harness，不声明 Chrome 自动化 browser media E2E 完成。"
  - "Opus fixture 格式固定为 big-endian 16-bit length-prefixed packet 序列，不引入 Ogg/MP4/container parser。"
  - "安装包 consumer 必须直接编译 media public API，静态导出 target 需要在 package config 中查找 OpenSSL 依赖。"
patterns-established:
  - "Fixture verifier labels: public media api、H264/Opus send/receive、SRTP media path、NACK retransmit、PLI callback 分层输出。"
  - "H264 same-timestamp receive aggregation: 同一 RTP timestamp 的 SPS/PPS/IDR Annex-B NALU 聚合为一个输出 frame。"
requirements-completed: [MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06, MEDIA-07, PROTO-04, API-04]
duration: 11min
completed: 2026-05-13
---

# Phase 5 Plan 05: 固定媒体 Fixture、双向 Harness 与收口 Summary

**固定 H264/Opus 媒体 fixture、Phase 5 C verifier、安装包 media API 防回归和文档/溯源收口。**

## Performance

- **Duration:** 11 min
- **Started:** 2026-05-13T06:09:15Z
- **Completed:** 2026-05-13T06:20:32Z
- **Tasks:** 4/4
- **Files modified:** 13

## Accomplishments

- 新增 `tests/fixtures/h264_annexb_sample.h264`，包含 Annex-B start code、SPS、PPS 和 IDR，`tests/media/test_media_fixtures.c` 断言 SPS/PPS 出现在 IDR 前且 fixture 可被 H264 packetizer 读取。
- 新增 `tests/fixtures/opus_packets.bin`，格式为 big-endian 16-bit length-prefixed Opus packet 序列，fixture test 断言至少两个非空 packet。
- 新增 `tests/integration/mrtc_phase5_media_verify.c`，接受 `--fixtures ./tests/fixtures`，缺失 fixture 时明确失败；成功时输出所有计划要求的 exact labels。
- verifier 通过 in-process PeerConnection/transceiver/media hook 覆盖 public media API、H264 send/receive、Opus send/receive、SRTP media path、RTCP NACK retransmit 和 PLI callback，没有使用 Chrome 自动化。
- package consumer 直接包含 `micrortc/peer_connection.h` 并创建 H264/Opus transceiver，验证 installed package 的 public media API 编译边界。
- README、ROADMAP、STATE 和 SOURCE-MANIFEST 已更新 Phase 5 范围，并明确 Chrome 自动化 browser media E2E 仍属于 Phase 6。

## Task Commits

1. **Task 1: 添加 H264 Annex-B 和 Opus length-prefixed fixture reader** - `d50d2ad` (test)
2. **Task 2: 实现 Phase 5 分层媒体 verifier** - `c999f48` (feat)
3. **Task 3: 运行完整 Phase 5 验证与 package consumer 防回归** - `c956a12` (fix)
4. **Task 4: 更新 README、ROADMAP、STATE 与来源追溯** - `89c0bdf` (docs)

## Files Created/Modified

- `tests/fixtures/h264_annexb_sample.h264` - 固定 H264 Annex-B SPS/PPS/IDR fixture。
- `tests/fixtures/opus_packets.bin` - 固定 length-prefixed Opus packet fixture。
- `tests/media/test_media_fixtures.c` - fixture reader/validation tests。
- `tests/integration/mrtc_phase5_media_verify.c` - Phase 5 分层 media verifier。
- `src/peer_connection.c` - 修复 same-timestamp H264 多 NALU receive aggregation。
- `CMakeLists.txt` - 注册 fixture test、Phase 5 verifier 和 package config dependency substitution。
- `cmake/micrortcConfig.cmake.in` - installed static package 按需查找 OpenSSL。
- `tests/package-consumer/package_consumer.c` - 下游 consumer 编译 public media transceiver API。
- `README.md` - 记录 Phase 5 verifier、fixture 格式和 package consumer 命令。
- `.planning/ROADMAP.md` - 标记 Phase 5 五个 plan 完成，并保留 Phase 6 Chrome 自动化边界。
- `.planning/STATE.md` - 更新 Phase 5 完成状态与 Phase 6 下一步。
- `.planning/phases/01-/SOURCE-MANIFEST.md` - 追加 fixture/verifier 溯源行。

## Decisions Made

- Phase 5 收口以固定 C fixture harness 为完成标准；Chrome 自动化 browser media E2E 不在本计划声明完成。
- Opus fixture 使用 16-bit big-endian packet length 前缀，保持简单可读，不引入 Ogg Opus 或其他容器解析职责。
- `micrortcConfig.cmake` 在构建时发现并导出 OpenSSL link dependency 时同步生成 `find_dependency(OpenSSL)`，保证 static package consumer 可以配置。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 修复 H264 same-timestamp 多 NALU 接收聚合**
- **Found during:** Task 2（实现 Phase 5 分层媒体 verifier）
- **Issue:** 固定 H264 fixture 中 SPS、PPS、IDR 使用同一 RTP timestamp，但接收路径对每个 start NALU 都重置 receive buffer，导致 on_frame 只输出最后一个 IDR NALU。
- **Fix:** `src/peer_connection.c` 仅在 receive buffer 为空或 RTP timestamp 变化时重置 H264 receive frame；同一 timestamp 的 SPS/PPS/IDR 会聚合后在 marker packet 输出。
- **Files modified:** `src/peer_connection.c`
- **Verification:** `./build/tests/integration/mrtc_phase5_media_verify --fixtures ./tests/fixtures` 输出 `h264 receive ok` 并通过全套 CTest。
- **Committed in:** `c999f48`

**2. [Rule 3 - Blocking] 修复 CTest verifier 工作目录**
- **Found during:** Task 2（实现 Phase 5 分层媒体 verifier）
- **Issue:** direct verifier 通过，但 CTest 从 `build/` 运行时找不到 `tests/fixtures/minimal_offer.sdp`。
- **Fix:** 为 `mrtc_phase5_media_verify` 设置 CTest `WORKING_DIRECTORY` 为项目源码根目录。
- **Files modified:** `CMakeLists.txt`
- **Verification:** `ctest --test-dir build -R "mrtc_phase5_media_verify|media" --output-on-failure` 通过。
- **Committed in:** `c999f48`

**3. [Rule 3 - Blocking] 修复 installed static package OpenSSL dependency lookup**
- **Found during:** Task 3（package consumer 防回归）
- **Issue:** installed `micrortc::micrortc` static target 通过 `$<LINK_ONLY:OpenSSL::SSL>` 引用 OpenSSL imported targets，但 package config 未先执行 `find_package(OpenSSL)`。
- **Fix:** `cmake/micrortcConfig.cmake.in` 增加按需 `find_dependency(OpenSSL)` substitution，`CMakeLists.txt` 在 OpenSSL_FOUND 时生成该行。
- **Files modified:** `CMakeLists.txt`, `cmake/micrortcConfig.cmake.in`
- **Verification:** install 后 package consumer configure/build/run 全部通过。
- **Committed in:** `c956a12`

---

**Total deviations:** 3 auto-fixed（Rule 1: 1, Rule 3: 2）
**Impact on plan:** 均为当前 fixture verifier 和 package consumer 验证直接暴露的正确性/阻塞问题；未扩大 Phase 5 范围。

## Issues Encountered

- 本机构建仍提示缺少可选 `MRTC_SRTP` / `MRTC_USRSCTP` 系统依赖；`MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=OFF`，当前验证使用既有 deterministic SRTP passthrough fallback，所有测试通过。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON` - PASS
- `cmake --build build` - PASS
- `ctest --test-dir build --output-on-failure` - PASS，18/18 tests passed。
- `./build/tests/integration/mrtc_phase5_media_verify --fixtures ./tests/fixtures` - PASS，输出：
  - `public media api ok`
  - `h264 send ok`
  - `h264 receive ok`
  - `opus send ok`
  - `opus receive ok`
  - `srtp media path ok`
  - `rtcp nack retransmit ok`
  - `pli callback ok`
  - `phase5 media verifier complete`
- `cmake --install build --prefix build/install && rm -rf build/package-consumer && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer` - PASS
- `./build/tests/integration/mrtc_phase5_media_verify` - FAIL as expected，stderr 包含 `missing --fixtures path`。
- `rg -n "mrtc_phase5_media_verify|h264_annexb_sample|opus_packets|Chrome 自动化|Phase 6|browser media E2E" README.md .planning/ROADMAP.md .planning/STATE.md .planning/phases/01-/SOURCE-MANIFEST.md` - PASS

## Known Stubs

None. Stub scan only found an existing `.planning/ROADMAP.md` placeholder credential example unrelated to this plan and explicitly intended as non-secret documentation.

## Threat Flags

None - 本计划新增本地 fixture file readers 和 in-process integration verifier，没有新增 network endpoint、auth path、schema 变更或新的 production file access boundary。

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 6 可以基于当前 public media API、H264/Opus fixture、Phase 5 verifier labels 和 package consumer coverage 继续实现 Chrome 自动化 E2E、自动 signaling、TURN relay 和真实 browser media E2E。Phase 5 未声明 Chrome 自动化完成。

## Self-Check: PASSED

- Created files exist: `tests/fixtures/h264_annexb_sample.h264`, `tests/fixtures/opus_packets.bin`, `tests/media/test_media_fixtures.c`, `tests/integration/mrtc_phase5_media_verify.c`, `.planning/phases/05-h264-opus/05-05-SUMMARY.md`
- Task commits exist: `d50d2ad`, `c999f48`, `c956a12`, `89c0bdf`
- Plan verification commands passed.

---
*Phase: 05-h264-opus*
*Completed: 2026-05-13*
