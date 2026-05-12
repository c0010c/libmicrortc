---
phase: 02
slug: independent-build-library-skeleton
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-12T19:42:43+08:00
---

# Phase 02 — Validation Strategy

> Phase 2 是构建骨架阶段；验证重点是 CMake 静态库、public include、真实链接符号、install/export package 和 excluded 依赖边界。

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | shell + CMake + CTest + `rg` |
| **Config file** | root `CMakeLists.txt` after execution |
| **Quick run command** | `test -f CMakeLists.txt && test -f include/micrortc/micrortc.h && test -f src/micrortc.c` |
| **Full suite command** | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure && test -f build/libmicrortc.a && cmake --install build --prefix build/install && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer` |
| **Estimated runtime** | ~30 seconds |

## Sampling Rate

- **After every task commit:** Run the quick run command once the referenced files exist; for documentation-only tasks, run the task-specific `rg` assertion.
- **After every plan wave:** Run the full suite command.
- **Before `$gsd-verify-work`:** Full suite must be green.
- **Max feedback latency:** 30 seconds.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | BUILD-01, BUILD-02, BUILD-04 | T-02-01, T-02-02 | 独立 CMake target 不复用 AWS/KVS target 或 signaling 构建 | cmake/source | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && test -f build/libmicrortc.a && rg -n "add_library\\(micrortc STATIC|add_library\\(micrortc::micrortc ALIAS micrortc\\)|target_link_libraries\\(micrortc" CMakeLists.txt` | ❌ W1 | ⬜ pending |
| 02-01-02 | 01 | 1 | BUILD-01, BUILD-02 | T-02-03 | public header 和真实符号可被 C 程序链接 | compile/link | `ctest --test-dir build --output-on-failure` | ❌ W1 | ⬜ pending |
| 02-01-03 | 01 | 1 | BUILD-01, BUILD-02 | T-02-04 | 安装后的 CMake package 可被下游消费 | package | `cmake --install build --prefix build/install && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer` | ❌ W1 | ⬜ pending |
| 02-01-04 | 01 | 1 | BUILD-03, BUILD-04 | T-02-02 | runtime/core 与 excluded/reference-only 依赖边界可见且未进入核心 target | doc/source | `rg -n "OpenSSL|libsrtp|usrsctp|libwebsockets|kvsCommonLws|Signaling|GStreamer|AWS SDK|excluded|reference-only" README.md docs CMakeLists.txt` | ❌ W1 | ⬜ pending |
| 02-01-05 | 01 | 1 | BUILD-04 | T-02-05 | 自有骨架文件不被误登记为 AWS 派生代码 | doc/source | `rg -n "Phase 2|original|SOURCE-MANIFEST|reflib/kvs-webrtc-sdk" README.md .planning/phases/02-/02-01-SUMMARY.md .planning/phases/01-/SOURCE-MANIFEST.md` | ❌ W1 | ⬜ pending |

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No test framework installation is required.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 文档中的依赖边界措辞 | BUILD-03, BUILD-04 | 文档需要维护者确认是否足以指导后续协议搬迁 | 阅读 `README.md` 或 `docs/build.md` 中的依赖边界说明，确认 OpenSSL/libsrtp/usrsctp 是后续 runtime/core 边界，libwebsockets/AWS SDK/signaling/GStreamer/PIC 不进入核心 target |

## Validation Sign-Off

- [x] All tasks have automated verify commands or document-grep assertions.
- [x] Sampling continuity: no 3 consecutive tasks without automated verify.
- [x] Wave 0 covers all missing references.
- [x] No watch-mode flags.
- [x] Feedback latency < 30s.
- [x] `nyquist_compliant: true` set in frontmatter.

**Approval:** pending
