---
phase: 05-rtp-rtcp-media-plane
plan: 01
subsystem: media
tags: [c, rtp, rtcp, media-api, fixed-memory, counters, trace]

requires:
  - phase: 04-dtls-srtp-secure-transport
    provides: internal SRTP/SRTCP wrapper 和 `observer.on_datagram` 安全输出边界
provides:
  - typed `rtc_media_frame_t` 与 `rtc_media_feedback_t` 公共契约
  - `rtc_peer_connection_send_media_frame` 与 `rtc_peer_connection_request_keyframe` public API 入口
  - media/RTP/RTCP limits、counters、trace 常量和 create-time 固定槽初始化
affects: [05-02-rtp-send, 05-03-rtp-receive, 05-04-rtcp, 05-05-feedback, docs]

tech-stack:
  added: []
  patterns:
    - 纯 C public header 契约
    - create-time arena 固定容量分配
    - 自研 CTest/TDD 红绿提交

key-files:
  created:
    - include/rtc/media.h
    - tests/test_media_api.c
  modified:
    - include/rtc/peer_connection.h
    - include/rtc/observer.h
    - include/rtc/limits.h
    - include/rtc/counters.h
    - include/rtc/trace.h
    - include/rtc/config.h
    - src/api/peer_connection.h
    - src/api/peer_connection.c
    - src/observability/counters.c
    - CMakeLists.txt
    - tests/test_main.c
    - tests/test_peer_connection.c
    - tests/test_jsep.c
    - tests/test_ice.c
    - tests/test_datagram.c
    - tests/test_security.c
    - tests/test_observability.c
    - examples/create_destroy.c

key-decisions:
  - "第 5 阶段 public media API 采用 `rtc_media_frame_t` typed generic 形状，v1 只公开 Opus audio 和 H264 video kind。"
  - "有效媒体发送和 keyframe 请求先固定亲和、参数和 kind 校验，RTP/RTCP 实际路径保留给 05-02 与 05-05。"
  - "media/RTP/RTCP 跨 executor 所需容量全部进入 create-time limits，0 值配置直接拒绝。"

patterns-established:
  - "typed media observer: 新增 `on_media_frame_typed` 与 `on_media_feedback`，旧 `on_media_frame(data,len)` 保留兼容。"
  - "media capacity baseline: `PeerConnection` 创建时分配 media queue slots、packet cache 和 H264 reassembly buffer。"

requirements-completed: [RTP-05, OBS-04]

duration: 7min
completed: 2026-05-10
---

# Phase 05 Plan 01: typed media API 与媒体容量基线 Summary

**typed media frame/feedback 公共契约，以及 media/RTP/RTCP 固定容量、counter 和 trace 基线已进入默认构建。**

## Performance

- **Duration:** 7min
- **Started:** 2026-05-10T15:49:19Z
- **Completed:** 2026-05-10T15:56:40Z
- **Tasks:** 2
- **Files modified:** 21

## Accomplishments

- 新增 `include/rtc/media.h`，定义 `RTC_MEDIA_KIND_AUDIO_OPUS`、`RTC_MEDIA_KIND_VIDEO_H264`、`rtc_media_frame_t`、`RTC_MEDIA_FEEDBACK_PLI`、`RTC_MEDIA_FEEDBACK_NACK` 和 `rtc_media_feedback_t`。
- 在 observer vtable 中新增 typed media frame 与 media feedback 回调，并在 `peer_connection.h` 暴露媒体发送和 keyframe 请求 API。
- 扩展 RTP/RTCP limits、media/RTP/RTCP counters、trace event/field，并在 `rtc_peer_connection_create` 中从 arena 分配固定 media queue、packet cache 和 H264 reassembly buffer。
- 新增 `tests/test_media_api.c`，覆盖 header contract、非法 kind、空 frame、media executor affinity、limits 0 值拒绝、counter 清零和固定槽初始化。

## Task Commits

1. **Task 1 RED: typed media API 失败测试** - `12fa563` (test)
2. **Task 1 GREEN: typed media API contract** - `321278e` (feat)
3. **Task 2 RED: media capacity baseline 失败测试** - `2e44983` (test)
4. **Task 2 GREEN: media capacity baseline** - `53aea00` (feat)

## Files Created/Modified

- `include/rtc/media.h` - typed media frame、media kind 和 feedback 类型。
- `include/rtc/observer.h` - typed frame 和 feedback observer 回调。
- `include/rtc/peer_connection.h` - media send 和 keyframe request public API。
- `include/rtc/limits.h` - RTP payload、packet/frame、reassembly、media queue 和 RTCP feedback/SDES capacity。
- `include/rtc/counters.h` - RTP、RTCP 和 media counter 分组。
- `include/rtc/trace.h` - media/RTP/RTCP/feedback trace event 和字段常量。
- `src/api/peer_connection.c` / `src/api/peer_connection.h` - media API 校验和 create-time 固定槽初始化。
- `src/observability/counters.c` - 新 counters 显式清零。
- `tests/test_media_api.c` - 第 5 阶段媒体 API 与容量基线测试。

## Decisions Made

- 遵循计划采用统一 typed media API，不拆分 `send_opus_frame` / `send_h264_access_unit`。
- `rtc_peer_connection_send_media_frame` 和 `rtc_peer_connection_request_keyframe` 当前只完成第 05-01 契约校验；有效媒体路径仍返回 `RTC_STATUS_UNSUPPORTED`，避免提前实现 05-02/05-05 的 RTP/RTCP 行为。
- 新增 limits 是 create-time 正确性要求；测试和示例配置同步补齐非零 media limits。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Task 1 需要最小 API 定义才能链接媒体 API 测试**
- **Found during:** Task 1
- **Issue:** 计划的 Task 1 文件列表未显式包含 `src/api/peer_connection.c`，但测试必须调用 `rtc_peer_connection_send_media_frame` 验证非法 kind、空 frame 和 media executor affinity。
- **Fix:** 在 `src/api/peer_connection.c` 中加入最小 public API 定义，只做亲和、空值和 kind 校验，真实媒体发送路径保留给后续计划。
- **Files modified:** `src/api/peer_connection.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `321278e`

**2. [Rule 3 - Blocking] 新增必填 media limits 后旧测试配置需要同步补齐**
- **Found during:** Task 2
- **Issue:** 新增 limits 0 值拒绝后，旧测试和示例配置未设置 media/RTP/RTCP limits，会被正确判定为 invalid argument 或容量不足。
- **Fix:** 为测试 helper 和 `examples/create_destroy.c` 补齐非零 media/RTP/RTCP limits，并按新增 arena 消耗调整少量容量边界测试 arena 大小。
- **Files modified:** `tests/test_peer_connection.c`, `tests/test_jsep.c`, `tests/test_ice.c`, `tests/test_datagram.c`, `tests/test_security.c`, `tests/test_observability.c`, `examples/create_destroy.c`
- **Verification:** `cmake --build build && ctest --test-dir build --output-on-failure`
- **Committed in:** `53aea00`

---

**Total deviations:** 2 auto-fixed (Rule 3: 2)
**Impact on plan:** 均为完成计划验收所需的阻塞修复，没有引入线程、socket 或 GPL/LGPL 依赖。

## Issues Encountered

- TDD RED 阶段均按预期失败：Task 1 失败于 `rtc/media.h` 不存在；Task 2 失败于新增 limits/counters/trace/internal slot 字段不存在。
- Task 2 GREEN 初次运行时部分旧测试 arena 容量不再足够；已通过降低通用测试 media limits、保留媒体 API 专项测试的大容量配置解决。

## Known Stubs

| Stub | File | Reason |
|------|------|--------|
| `rtc_peer_connection_send_media_frame` 对有效 Opus/H264 frame 返回 `RTC_STATUS_UNSUPPORTED` | `src/api/peer_connection.c` | 05-01 只固定 public contract、亲和和输入校验；05-02 接入 RTP packetize/SRTP protect/datagram 输出。 |
| `rtc_peer_connection_request_keyframe` 对有效 kind 返回 `RTC_STATUS_UNSUPPORTED` | `src/api/peer_connection.c` | 05-01 只固定 API 入口；05-05 接入 PLI/SRTCP feedback 输出。 |

## Threat Flags

无。新增 media API、跨 executor 固定槽和容量拒绝均已在计划 threat model 的 T-05-01、T-05-02、T-05-03 范围内。

## Verification

- `cmake --build build && ctest --test-dir build --output-on-failure` - PASS
- `! grep -R "pthread_create\\|socket(" include src tests` - PASS
- Task 1 acceptance grep - PASS
- Task 2 acceptance grep - PASS

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

05-02 可以直接消费 `rtc_media_frame_t`、RTP payload limits、media queue slots 和 trace/counter 常量，实现 Opus/H264 RTP 发送链路。05-05 可以复用 `rtc_media_feedback_t` 和 `rtc_peer_connection_request_keyframe` 接入 PLI/NACK 反馈语义。

## Self-Check: PASSED

- 文件存在性检查通过：`include/rtc/media.h`、`tests/test_media_api.c`、`05-01-SUMMARY.md` 均存在。
- 提交存在性检查通过：`12fa563`、`321278e`、`2e44983`、`53aea00` 均可在 git log 中找到。
- 计划级验证通过：`cmake --build build && ctest --test-dir build --output-on-failure`。
- 边界检查通过：`! grep -R "pthread_create\\|socket(" include src tests`。

---
*Phase: 05-rtp-rtcp-media-plane*
*Completed: 2026-05-10*
