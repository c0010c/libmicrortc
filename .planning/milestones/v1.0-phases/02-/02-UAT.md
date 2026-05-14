---
status: complete
phase: 02-
source:
  - .planning/phases/02-/02-01-SUMMARY.md
started: 2026-05-12T21:28:31+08:00
updated: 2026-05-12T21:28:31+08:00
---

# Phase 2 UAT: 独立构建与库骨架

## Current Test

[testing complete]

## Tests

### 1. 独立 CMake 静态库构建
expected: 根目录 `CMakeLists.txt` 可以配置并构建 `micrortc` 静态库，生成 `build/libmicrortc.a`。
result: pass
evidence: `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && test -f build/libmicrortc.a`

### 2. Build-tree smoke test
expected: smoke test 可以包含 `<micrortc/micrortc.h>`，链接真实 `mrtc_*` 符号，并通过 CTest。
result: pass
evidence: `ctest --test-dir build --output-on-failure` passed 1/1 tests.

### 3. Install/export package consumer
expected: 安装后的 CMake package 可被独立 consumer 通过 `find_package(micrortc CONFIG REQUIRED)` 和 `micrortc::micrortc` 消费。
result: pass
evidence: `cmake --install build --prefix build/install && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer`

### 4. 非核心依赖边界
expected: 核心构建路径不引用 AWS/KVS signaling、PIC/kvsCommonLws、libwebsockets 或相关 excluded 组件。
result: pass
evidence: `rg -n "kvsWebrtcClient|kvsWebrtcSignalingClient|kvsCommonLws|kvspicUtils|kvspicState|libwebsockets" CMakeLists.txt include src tests` produced no output.

### 5. 来源归属与 Phase 2 summary
expected: Phase 2 新增骨架文件记录为 `original`，未复制 AWS-derived source，且不需要更新 Phase 1 `SOURCE-MANIFEST.md`。
result: pass
evidence: `rg -n "original|SOURCE-MANIFEST|BUILD-01|BUILD-02|BUILD-03|BUILD-04" .planning/phases/02-/02-01-SUMMARY.md`

## Summary

total: 5
passed: 5
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

[none]
