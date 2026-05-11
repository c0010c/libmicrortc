---
phase: 06-chrome-end-to-end-acceptance
plan: 03
subsystem: examples
tags: [chrome-e2e, media-samples, h264, ogg-opus, jsonl, media-file]
requires:
  - phase: 06-chrome-end-to-end-acceptance
    plan: 02
    provides: C 示例运行时、WebSocket/UDP 示例层 I/O 和 JSONL observer
provides:
  - 示例层 H264 Annex B 与 Ogg Opus 样本读取
  - `rtc_chrome_e2e` 样本媒体按节奏发送和 typed media 接收落盘
  - JSONL summary 媒体 frame/byte 统计与 `--media-file-smoke`
affects: [phase-06, examples, e2e, acceptance, media]
tech-stack:
  added: []
  patterns:
    - 样本 parser 仅位于 `examples/chrome_e2e/` 示例层，测试直接读取根目录固定样本
    - H264 使用 25fps `40000us` 节奏，Opus 在无法安全推导 granule 时使用 `20000us`
    - 接收媒体文件失败统一落到 JSONL `media_file` layer
key-files:
  created:
    - examples/chrome_e2e/media_samples.c
    - examples/chrome_e2e/media_samples.h
    - tests/test_chrome_e2e_samples.c
  modified:
    - CMakeLists.txt
    - examples/chrome_e2e/rtc_chrome_e2e.c
    - examples/chrome_e2e/jsonl.c
    - examples/chrome_e2e/jsonl.h
    - examples/chrome_e2e/run_e2e.mjs
    - examples/chrome_e2e/README.md
    - tests/test_main.c
key-decisions:
  - "样本解析保持在 examples/chrome_e2e 示例层，核心 src/ 不新增 codec、container 或文件解析责任。"
  - "Ogg Opus 当前使用 20ms fallback 节奏；H264 固定 25fps `40000us`。"
  - "接收 Opus 使用长度前缀 packet 文件，接收 H264 使用 Annex B `.264` 文件，README 说明人工检查方式。"
patterns-established:
  - "`run_e2e.mjs --media-file-smoke` 复用 C 示例 dry-run，验证 parser、输出目录、媒体文件和 summary 字段。"
  - "JSONL summary 可携带 `audio_frames_received`、`video_frames_received`、`audio_bytes_received`、`video_bytes_received`。"
requirements-completed: []
duration: 5min
completed: 2026-05-11
---

# Phase 6 Plan 03: 样本解析与双向媒体文件 Summary

**示例层 H264/Ogg Opus 样本解析、按节奏发送门控、接收媒体落盘和 media-file smoke 已接入 C 示例。**

## Performance

- **Duration:** 5min
- **Started:** 2026-05-11T06:21:32Z
- **Completed:** 2026-05-11T06:26:46Z
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments

- 新增 `media_samples.h/.c`，实现窄范围 H264 Annex B start code 扫描和 Ogg Opus page/lacing packet 提取，跳过 `OpusHead` 与 `OpusTags`。
- 新增 `rtc_test_chrome_e2e_samples`，直接读取根目录 `sample1.opus` 和 `test-25fps.h264`，锁定 Opus/H264 至少可解析一帧、H264 `40000us`、Opus 正 duration。
- `rtc_chrome_e2e` 在 `srtp.ready` 后启动样本发送 loop：H264 按 25fps，Opus 按 parser duration 或 20ms fallback；不会一次性倾倒全部样本。
- `on_media_frame_typed` 将 Chrome 侧接收媒体写入 `received-opus.packets` 和 `received-h264.264`，并累计 frame/byte summary 字段。
- `run_e2e.mjs --media-file-smoke` 会运行 C 示例 dry-run，确认输出文件存在且 JSONL summary 媒体统计为正数。

## Task Commits

1. **Task 1 RED: 样本 parser 失败测试** - `84076ca` (`test`)
2. **Task 1 GREEN: 实现示例层样本 parser** - `dc010e4` (`feat`)
3. **Task 2: 接入样本发送、接收落盘和 media-file smoke** - `3932970` (`feat`)

## Files Created/Modified

- `examples/chrome_e2e/media_samples.h` - 示例层 sample reader/frame API。
- `examples/chrome_e2e/media_samples.c` - H264 Annex B 与 Ogg Opus packet parser。
- `tests/test_chrome_e2e_samples.c` - 根目录固定媒体样本 deterministic tests。
- `tests/test_main.c` - 注册 `rtc_test_chrome_e2e_samples`。
- `CMakeLists.txt` - 将 sample parser 编入 `rtc_tests` 与 `rtc_chrome_e2e` target。
- `examples/chrome_e2e/rtc_chrome_e2e.c` - 样本 pacing、`srtp.ready` gate、typed media 落盘和 summary counters。
- `examples/chrome_e2e/jsonl.c` / `jsonl.h` - 新增媒体统计 summary 输出函数。
- `examples/chrome_e2e/run_e2e.mjs` - 新增 `--media-file-smoke`。
- `examples/chrome_e2e/README.md` - 中文说明样本策略、输出文件格式和 VLC/ffplay 检查方式。

## Decisions Made

- Ogg Opus granule 时序首版不强行推导；按计划 D-09 使用稳定 `20000us` fallback，避免从不完整上下文生成错误 pacing。
- H264 parser 输出 Annex B start-code 帧，保持与现有 RTP packetizer 输入契约一致。
- `received-opus.packets` 使用每包 4 字节 big-endian 长度前缀；`received-h264.264` 使用 Annex B start code，便于人工或工具检查。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 修正测试 include 路径**
- **Found during:** Task 1 GREEN
- **Issue:** RED 测试使用 `examples/chrome_e2e/media_samples.h` quote include，`rtc_tests` 编译单元从 `tests/` 目录搜索时找不到头文件。
- **Fix:** 改为相对 include `../examples/chrome_e2e/media_samples.h`，不扩大 target include path。
- **Files modified:** `tests/test_chrome_e2e_samples.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `dc010e4`

**2. [Rule 2 - Missing critical functionality] 补齐 C 示例 target 与 JSONL 媒体统计**
- **Found during:** Task 2
- **Issue:** `rtc_chrome_e2e` 需要链接 `media_samples.c` 才能使用 parser；计划要求 summary 报告 frame/byte 数，但既有 JSONL summary 只有 pass/layer/reason。
- **Fix:** 将 `media_samples.c` 编入 `rtc_chrome_e2e`，新增 `rtc_e2e_jsonl_summary_media()` 输出四个媒体统计字段。
- **Files modified:** `CMakeLists.txt`, `examples/chrome_e2e/jsonl.c`, `examples/chrome_e2e/jsonl.h`
- **Verification:** `cmake --build build --target rtc_chrome_e2e`; `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke`
- **Committed in:** `3932970`

---

**Total deviations:** 2 auto-fixed（Rule 3 blocking 1 项，Rule 2 missing critical functionality 1 项）
**Impact on plan:** 变更均限定在示例层、测试和 JSONL 诊断输出；核心 `src/` 未新增 parser、socket、线程或编解码责任。

## Issues Encountered

- TDD RED 阶段按预期失败：`examples/chrome_e2e/media_samples.c` 尚不存在，CMake 无法生成 `rtc_tests`。随后 GREEN 提交实现 parser 并通过构建/CTest。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure`：通过。
- `cmake --build build --target rtc_chrome_e2e`：通过。
- `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke`：通过，summary 中 `audio_frames_received=1`、`video_frames_received=1`、`audio_bytes_received=407`、`video_bytes_received=15`。
- `grep` 验收项：`rtc_e2e_sample_open_h264`、`rtc_e2e_sample_next_h264`、`rtc_e2e_sample_open_ogg_opus`、`rtc_e2e_sample_next_opus`、`OggS`、`OpusHead`、`40000`、`rtc_test_chrome_e2e_samples`、`rtc_peer_connection_send_media_frame`、`srtp.ready`、`received-opus.packets`、`received-h264.264`、`audio_frames_received`、`video_frames_received`、`media_file` 均通过。

## TDD Gate Compliance

- RED commit: `84076ca`。
- GREEN commit: `dc010e4`。
- Task 2 非 TDD，提交为 `3932970`。

## Known Stubs

None - 未发现阻止本计划目标达成的 stub。完整真实 Chrome 音视频互通仍依赖后续 `06-04` 真实 DTLS/SRTP backend gate、`06-05` full E2E 编排和 `06-06` UAT 收口。

## Auth Gates

None.

## Threat Flags

None - 新增 sample parser 与媒体文件输出均已在计划 threat model `T-06-07`、`T-06-08`、`T-06-09` 中覆盖并按计划缓解。

## User Setup Required

None - no external service configuration required for parser tests and media-file smoke.

## Next Phase Readiness

06-04 可以在当前 C 示例基础上接入真实或可选参考 DTLS/SRTP backend gate。06-05 可以复用 JSONL 媒体统计、输出文件路径和 `--media-file-smoke` 作为 full E2E 前置检查。

## Self-Check: PASSED

- 文件存在性检查通过：`06-03-SUMMARY.md`、`media_samples.c`、`media_samples.h`、`test_chrome_e2e_samples.c`、`rtc_chrome_e2e.c`、`run_e2e.mjs` 均存在。
- 提交存在性检查通过：`84076ca`、`dc010e4`、`3932970` 均可在 git log 中找到。
- 自动化复验通过：`cmake --build build && ctest --test-dir build --output-on-failure` 和 `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke` 均成功。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
