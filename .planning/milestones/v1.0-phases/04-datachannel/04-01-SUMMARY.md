---
phase: 04-datachannel
plan: "01"
subsystem: transport-foundation
tags: [cmake, transport, portability, ice, stun, dtls, srtp, sctp]
requires:
  - phase: 03-aws-api-signaling-free-peerconnection
    provides: signaling-free PeerConnection API and SDP helper
provides:
  - Phase 4 private portability helpers
  - Protocol directory and CTest harness skeleton
  - Secret-free ICE server example config
affects: [phase-04, phase-05, transport]
tech-stack:
  added: [Threads, optional OpenSSL/libsrtp/usrsctp discovery]
  patterns: [private C99 protocol wrappers, CTest executable tests]
key-files:
  created: [src/common/mrtc_common.c, src/ice/ice_agent.c, src/stun/stun_message.c, tests/integration/mrtc_phase4_network_verify.c, mrtc-ice-servers.example.json]
  modified: [CMakeLists.txt, include/micrortc/peer_connection.h, .planning/phases/01-/SOURCE-MANIFEST.md]
key-decisions:
  - "真实配置文件命名为 mrtc-ice-servers.local.json，并加入 .gitignore。"
  - "系统 OpenSSL/libsrtp/usrsctp 缺失时默认保持可构建，严格依赖可用 MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS 开启。"
requirements-completed: [PROTO-01, PROTO-02, PROTO-03, PROTO-05]
duration: 1h
completed: 2026-05-12
---

# Phase 4 Plan 01 Summary: 协议基础设施、依赖与测试骨架

**Phase 4 private portability layer, protocol wrapper skeletons, dependency discovery, and real-network verifier entrypoint**

## Accomplishments

- 验证本地 `reflib/kvs-webrtc-sdk` 基线为 `9eebcc4`。
- 添加 `src/common/`、`src/ice/`、`src/stun/`、`src/turn/`、`src/dtls/`、`src/srtp/`、`src/sctp/` 和 `src/data_channel/` 私有骨架。
- CMake 增加 Threads 和可选 OpenSSL/libsrtp/usrsctp 私有发现；核心 target 仍不引入 KVS signaling。
- 添加 `mrtc_phase4_network_verify`，缺少 `mrtc-ice-servers.local.json` 时明确失败。
- 添加 secret-free `mrtc-ice-servers.example.json` 和忽略规则。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json` 按预期因本地配置缺失失败。
- boundary grep 对 KVS signaling/libwebsockets/AWS SDK/credential/storage 无命中。

## Task Commits

- `95ba79c feat(04-01): add transport scaffolding`

## Deviations from Plan

- 将 public API 壳提前加入 Plan 01，以便 `src/data_channel/` private skeleton 能编译；行为实现仍在后续计划补齐。
- CMake 默认不强制 OpenSSL/libsrtp/usrsctp headers 存在，因为当前环境缺少这些开发包；保留严格模式开关。

## Issues Encountered

- 无阻塞问题。

## Next Phase Readiness

Wave 2 可以在既有 protocol skeleton 上扩展 public API、SDP transport attributes 和 ICE/STUN/TURN 验证。
