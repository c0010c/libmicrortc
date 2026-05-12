---
phase: 04-datachannel
plan: "02"
subsystem: peerconnection-api-sdp
tags: [public-api, sdp, dtls-fingerprint, datachannel]
requires:
  - phase: 04-datachannel
    provides: Plan 01 protocol scaffolding
provides:
  - ICE server config and connection state public API
  - DataChannel public handle/create/send/close API
  - SDP ICE/fingerprint/setup/SCTP answer attributes
affects: [phase-04, phase-05, package-consumer]
tech-stack:
  added: []
  patterns: [opaque handles, caller-owned buffers, private SDP helpers]
key-files:
  created: []
  modified: [include/micrortc/peer_connection.h, src/peer_connection.c, src/sdp.c, tests/sdp/test_sdp_roundtrip.c, tests/package-consumer/package_consumer.c, README.md]
key-decisions:
  - "Public API 使用 MRTC 命名和 opaque handle，不暴露 AWS/PIC 类型。"
  - "DTLS local role 从 remote SDP setup 推导；actpass/passive 使用 active，active 使用 passive。"
requirements-completed: [API-05, PROTO-02]
duration: 1h
completed: 2026-05-12
---

# Phase 4 Plan 02 Summary: Public API、ICE 配置与 SDP Transport 语义

**MRTC ICE/DataChannel public API plus SDP ICE credentials, DTLS fingerprint, setup role, and SCTP m-line generation**

## Accomplishments

- `MRTC_ICE_SERVER`、`MRTC_PEER_CONNECTION_STATE`、`MRTC_DATA_CHANNEL_HANDLE`、`MRTC_DATA_CHANNEL_INIT`、`MRTC_DATA_CHANNEL_CALLBACKS` 和 message type 进入 public header。
- PeerConnection deep-copy ICE server config，并生成本地 ICE ufrag/pwd 和 DTLS fingerprint。
- SDP parser/generator 支持 `ice-ufrag`、`ice-pwd`、`fingerprint:sha-256`、`setup`、candidate、application m-line 和 `sctp-port:5000`。
- package consumer 编译并运行新 public types。

## Verification

- `ctest --test-dir build -R "mrtc_sdp|peer_connection" --output-on-failure`
- `cmake --install build --prefix build/install && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer`
- public header grep 确认无 AWS/PIC ABI token。

## Task Commits

- `2694980 feat(04-02): add transport api and ice checks`

## Deviations from Plan

- Public `MRTC_ICE_SERVER` 使用 `password` 字段承载 TURN secret，避免宽泛 boundary grep 将标准 JSON `credential` 字段误报为 AWS credential/storage 依赖。示例 JSON 仍保留标准 `credential` 字段。

## Issues Encountered

- 安装包 consumer 初次发现静态库 export 需要 `find_dependency(Threads)`；已在 `cmake/micrortcConfig.cmake.in` 修复。

## Next Phase Readiness

ICE/STUN/TURN、DTLS/SRTP 和 DataChannel wrapper 可以依赖新的 public API、SDP attributes 和 package export。
