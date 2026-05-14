---
phase: 02
slug: independent-build-library-skeleton
status: verified
threats_open: 0
asvs_level: 1
register_authored_at_plan_time: true
created: 2026-05-12T21:33:49+08:00
updated: 2026-05-12T21:33:49+08:00
---

# Phase 2 — Security

> Phase 2 安全核验范围是独立 CMake 静态库骨架、public header、install/export package、依赖边界和来源归属边界。

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| 根工程与本地参考库边界 | 根 `CMakeLists.txt` 必须独立，不把 `reflib/kvs-webrtc-sdk` 作为子工程或源码入口接入 | CMake target、源码路径、构建规则 |
| 核心库与 excluded 组件边界 | `micrortc` target 不链接 AWS/KVS signaling、credential/storage、GStreamer、PIC/kvsCommonLws、libwebsockets 等组件 | link interface、依赖发现、构建产物 |
| Public API 边界 | Phase 2 只暴露最小 `MRTC_STATUS` 和 `mrtc_*` 符号，不提前锁定 PeerConnection/SDP/ICE/DataChannel API | public header、ABI/API surface |
| Build-tree 与 install-tree 边界 | 不能只证明 build-tree 可用，必须证明安装后的 package 可被独立 CMake consumer 消费 | installed headers、CMake package config、export target |
| 来源归属边界 | Phase 2 新增骨架文件是 `original`；若后续复制或改写 AWS-derived 内容，必须更新 SOURCE-MANIFEST | source attribution、派生记录、合规证据 |

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-02-01 | Supply-chain / boundary creep | Root CMake | mitigate | 根 CMake 不调用 `add_subdirectory(reflib/kvs-webrtc-sdk)`、`ExternalProject_Add`、`build_dependency`，也不引用 `reflib/kvs-webrtc-sdk/src/source` | closed |
| T-02-02 | Coupling | Core target link interface | mitigate | `micrortc` 核心 target 不链接 libwebsockets、AWS SDK、KVS signaling、kvsCommonLws、kvspicUtils、kvspicState、GStreamer 或 reflib target | closed |
| T-02-03 | API lock-in | Public header | mitigate | public header 仅保留 `MRTC_STATUS` 与 `mrtc_*` 最小符号，PeerConnection、SDP、ICE、DataChannel、AWS `STATUS` 和 `PCHAR` 延后到后续阶段 | closed |
| T-02-04 | Packaging false positive | Install/export | mitigate | 增加独立 package consumer，使用 `find_package(micrortc CONFIG REQUIRED)` 和 `micrortc::micrortc` 验证 install-tree | closed |
| T-02-05 | Attribution drift | Source manifest | mitigate | README/SUMMARY 标记 Phase 2 骨架为 `original`，且未在 Phase 2 修改 AWS-derived manifest | closed |

## Evidence

| Threat ID | Evidence |
|-----------|----------|
| T-02-01 | `rg -n "add_subdirectory\\(reflib/kvs-webrtc-sdk|ExternalProject_Add|build_dependency|BUILD_DEPENDENCIES|reflib/kvs-webrtc-sdk/src/source" CMakeLists.txt` produced no output. |
| T-02-02 | `rg -n "libwebsockets|AWS SDK|kvsWebrtcClient|kvsWebrtcSignalingClient|kvsCommonLws|kvspicUtils|kvspicState|GStreamer|reflib/kvs-webrtc-sdk|Signaling" CMakeLists.txt include src tests` produced no output. |
| T-02-03 | `include/micrortc/micrortc.h` contains only `MRTC_STATUS`, `mrtc_version_string`, `mrtc_initialize`, and `mrtc_shutdown`; forbidden API scan for `STATUS`, `PCHAR`, `PeerConnection`, `Ice`, `ICE`, `DataChannel`, and `SDP` produced no output. |
| T-02-04 | `cmake -S . -B build-secure -DMRTC_BUILD_TESTS=ON && cmake --build build-secure && ctest --test-dir build-secure --output-on-failure && cmake --install build-secure --prefix build-secure/install && cmake -S tests/package-consumer -B build-secure/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build-secure/install" && cmake --build build-secure/package-consumer && ./build-secure/package-consumer/package_consumer` passed. |
| T-02-05 | `README.md` and `02-01-SUMMARY.md` state Phase 2 skeleton files are `original`; `git log --oneline -- .planning/phases/01-/SOURCE-MANIFEST.md` shows only `dffb1cd docs(01-01): define source manifest rules`. |

## Summary Threat Flags

No `## Threat Flags` section or additional security findings were present in `02-01-SUMMARY.md` or `02-UAT.md`.

## Accepted Risks Log

No accepted risks.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-12 | 5 | 5 | 0 | Codex inline security audit |

## Sign-Off

- [x] All threats have a disposition: mitigate.
- [x] Accepted risks documented in Accepted Risks Log.
- [x] `threats_open: 0` confirmed.
- [x] `status: verified` set in frontmatter.

**Approval:** verified 2026-05-12
