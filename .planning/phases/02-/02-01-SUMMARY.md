---
phase: 02-
plan: "01"
subsystem: build
tags: [cmake, c, static-library, packaging, ctest]
requires:
  - phase: 01-
    provides: local AWS KVS WebRTC SDK baseline, source attribution rules, and excluded component boundaries
provides:
  - standalone micrortc static library target
  - build-tree smoke test for public header and real symbols
  - install-tree CMake package export and independent consumer verification
  - Chinese build documentation with dependency and attribution boundaries
affects: [phase-03-api, phase-04-transport, phase-05-media, packaging]
tech-stack:
  added: [CMake, CTest]
  patterns: [micrortc:: namespace export, original skeleton files, installed package consumer]
key-files:
  created:
    - CMakeLists.txt
    - cmake/micrortcConfig.cmake.in
    - include/micrortc/micrortc.h
    - src/micrortc.c
    - tests/smoke/test_link.c
    - tests/package-consumer/CMakeLists.txt
    - tests/package-consumer/package_consumer.c
    - README.md
  modified: []
key-decisions:
  - "Phase 2 skeleton files are original and did not copy or rewrite AWS-derived source."
  - "OpenSSL, libsrtp, and usrsctp remain later v1 runtime/core dependency boundaries and are not required by the minimal skeleton."
  - "AWS/KVS signaling, credential/storage, GStreamer sample, libwebsockets, kvsCommonLws, kvspicUtils, and kvspicState stay outside the core micrortc target."
patterns-established:
  - "CMake package consumers use find_package(micrortc CONFIG REQUIRED) and target_link_libraries(... PRIVATE micrortc::micrortc)."
  - "Build verification must prove public include, real static-library symbols, CTest, install, and independent package consumption."
requirements-completed: [BUILD-01, BUILD-02, BUILD-03, BUILD-04]
duration: 10 min
completed: 2026-05-12
---

# Phase 2 Plan 01: 独立构建与库骨架 Summary

**独立 CMake 静态库骨架，带 build-tree smoke test、install-tree package export 和中文依赖边界文档**

## Performance

- **Duration:** 10 min
- **Started:** 2026-05-12T11:46:00Z
- **Completed:** 2026-05-12T11:56:41Z
- **Tasks:** 6
- **Files modified:** 9

## Accomplishments

- 创建独立根目录 `CMakeLists.txt`，定义 `micrortc` 静态库和 `micrortc::micrortc` namespace export。
- 添加最小 public C API 和无外部依赖实现，生成 `build/libmicrortc.a` 并导出真实 `mrtc_*` 符号。
- 添加 CTest smoke test 和独立 install-tree package consumer，验证 `find_package(micrortc CONFIG REQUIRED)` 可用。
- 编写中文 README，说明构建命令、OpenSSL/libsrtp/usrsctp 后续边界，以及 AWS/KVS/PIC excluded 组件。
- 确认 `.planning/phases/01-/SOURCE-MANIFEST.md` 未修改；本阶段新增骨架文件均为 `original`。

## Task Commits

Each task was committed atomically:

1. **Task 1: Create standalone CMake skeleton and static library target** - `fc8616f` (feat)
2. **Task 2: Add minimal public C API and linkable implementation** - `e177bf8` (feat)
3. **Task 3: Add build-tree smoke test** - `78a18c8` (test)
4. **Task 4: Add install/export consumer verification** - `7222efe` (test)
5. **Task 5: Document build commands and dependency boundaries** - `9ccd6a6` (docs)
6. **Task 6: Run final verification and write Phase 2 summary** - metadata commit

## Files Created/Modified

- `CMakeLists.txt` - 独立 CMake 入口、静态库 target、CTest、install/export 和 package config 生成。
- `cmake/micrortcConfig.cmake.in` - 安装后 CMake package config 模板。
- `include/micrortc/micrortc.h` - 最小 public C API，包含 `MRTC_STATUS` 和 `mrtc_*` 符号。
- `src/micrortc.c` - 最小可链接实现，无第三方依赖。
- `tests/smoke/test_link.c` - build-tree 公开头和真实符号链接测试。
- `tests/package-consumer/CMakeLists.txt` - 独立安装包消费验证工程。
- `tests/package-consumer/package_consumer.c` - install-tree 消费端链接验证程序。
- `README.md` - 中文构建命令、依赖边界、excluded 组件和来源归属说明。
- `.planning/phases/02-/02-01-SUMMARY.md` - 本执行总结。

## Decisions Made

- Phase 2 保持最小 API，只证明构建/链接/install/export，不提前承诺 PeerConnection、SDP、ICE、DataChannel 或媒体 API。
- Phase 2 不查找、不下载、不链接 OpenSSL、libsrtp 或 usrsctp；这些依赖在后续协议模块实际进入时再激活。
- 不修改 `SOURCE-MANIFEST.md`，因为本阶段没有复制、裁剪、改名或 rewritten-derived 本地 `reflib/kvs-webrtc-sdk` 内容。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Task 1 build verification required Task 2 source files**
- **Found during:** Task 1 (Create standalone CMake skeleton and static library target)
- **Issue:** Task 1 的验证要求 `cmake --build build` 生成 `libmicrortc.a`，但 `src/micrortc.c` 和 public header 在 Task 2 才创建。
- **Fix:** 在同一编辑批次先写入最小 API/source，使 Task 1 构建验收可运行；提交仍按 Task 1/Task 2 文件边界分开。
- **Files modified:** `include/micrortc/micrortc.h`, `src/micrortc.c`
- **Verification:** `cmake --build build && test -f build/libmicrortc.a` 通过。
- **Committed in:** `e177bf8` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** 无范围扩张；修正了任务排序与构建验收之间的依赖矛盾。

## Issues Encountered

- Plan 中 `rg -n "PeerConnection|STATUS\\b|PCHAR|Ice|DataChannel|SDP" include/micrortc/micrortc.h` 的 `STATUS\\b` 会匹配合法的 `MRTC_STATUS`，因此该断言过宽。实际检查确认 public header 没有 AWS `STATUS`、`PCHAR`、PeerConnection、SDP、ICE candidate 或 DataChannel API；`MRTC_STATUS` 是 Phase 2 明确要求的自有类型。

## Verification

Full validation passed:

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
test -f build/libmicrortc.a
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
```

Boundary verification passed:

```bash
rg -n "kvsWebrtcClient|kvsWebrtcSignalingClient|kvsCommonLws|kvspicUtils|kvspicState|libwebsockets" CMakeLists.txt include src tests
```

The boundary command produced no output.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 2 now provides the standalone build/package shell that Phase 3+ can extend. Future protocol migrations should add new source files to the `micrortc` target, keep public API decisions in the relevant phase, and update `.planning/phases/01-/SOURCE-MANIFEST.md` whenever AWS-derived content enters the project.

## Self-Check: PASSED

- BUILD-01, BUILD-02, BUILD-03, and BUILD-04 are represented in deliverables.
- `libmicrortc.a` builds on Linux x86_64 with the current toolchain.
- Build-tree and install-tree consumers both link real `mrtc_*` symbols.
- Core target does not link or reference AWS/KVS signaling, credential/storage, GStreamer sample, libwebsockets, kvsCommonLws, kvspicUtils, or kvspicState.
- Phase 2 skeleton files are documented as `original`; `SOURCE-MANIFEST.md` remains unchanged.

---
*Phase: 02-*
*Completed: 2026-05-12*
