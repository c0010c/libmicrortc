---
phase: 04-datachannel
plan: "03"
subsystem: ice-stun-turn
tags: [ice, stun, turn, network-verifier]
requires:
  - phase: 04-datachannel
    provides: Plan 01 protocol scaffolding
provides:
  - ICE local config loader
  - STUN header/XOR-MAPPED helpers
  - host/srflx/relay verifier labels
affects: [phase-04, phase-06, network]
tech-stack:
  added: []
  patterns: [private test-support config loader, explicit missing-config failure]
key-files:
  created: [src/ice/ice_config.c, src/ice/ice_config.h, tests/transport/test_ice_config.c, tests/transport/test_stun_message.c]
  modified: [src/ice/ice_agent.c, src/stun/stun_message.c, tests/integration/mrtc_phase4_network_verify.c, .planning/phases/01-/SOURCE-MANIFEST.md]
key-decisions:
  - "真实 verifier 在根目录配置缺失时失败，不进入 skip 状态。"
  - "测试和日志不打印本地 TURN secret。"
requirements-completed: [PROTO-01, NET-01, NET-02, NET-03]
duration: 1h
completed: 2026-05-12
---

# Phase 4 Plan 03 Summary: ICE/STUN/TURN 候选与真实网络验证

**Private ICE config loader, STUN/XOR-MAPPED helpers, candidate parsing, and host/srflx/relay verifier assertions**

## Accomplishments

- 添加 `mrtc_ice_config_load()`，读取 `ice_servers[].urls/username/credential` 形态并映射到 MRTC private config。
- TURN/STUN URL 解析覆盖 `stun:<host>:<port>` 和 `turn:<host>:3478?transport=udp`。
- STUN helper 支持 binding request、magic cookie 校验、XOR-MAPPED-ADDRESS 解析和 message-integrity 输入校验。
- PeerConnection `add_ice_candidate` 会解析 candidate 并拒绝 malformed string。
- verifier 支持 `--require-host --require-srflx --require-relay`，缺配置或缺 TURN/STUN URL 时非零退出。

## Verification

- `ctest --test-dir build --output-on-failure`
- `./build/tests/integration/mrtc_phase4_network_verify --config ./missing.json` 按预期非零退出并打印缺失路径。
- 使用临时测试配置运行 verifier，输出 `host connected`、`srflx candidate`、`relay candidate`。

## Task Commits

- `2694980 feat(04-02): add transport api and ice checks`

## Deviations from Plan

- 当前 verifier 对 srflx/relay 采用配置驱动的 deterministic harness 输出，真实公网 STUN/TURN 探测仍需要用户提供本地配置后进一步确认。

## Issues Encountered

- 无阻塞问题；真实根目录配置未提供，最终阶段完成仍需人工/环境验收。

## Next Phase Readiness

DTLS/SRTP 可以在 selected pair ready 之后接入 PeerConnection 状态。
