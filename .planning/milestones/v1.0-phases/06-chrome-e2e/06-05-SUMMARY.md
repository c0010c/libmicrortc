---
phase: 06-chrome-e2e
plan: "05"
subsystem: turn-relay-e2e
tags: [chrome, e2e, turn, relay, ice, stun, media, playwright]
requires:
  - phase: 06-chrome-e2e
    provides: 默认 host Chrome 双向媒体 E2E
provides:
  - 显式 TURN relay-only Chrome E2E
  - C 端 srflx 与 relay candidate 收集
  - C 端 TURN UDP relay Send/Data Indication 收发路径
  - selected candidate pair relay 证据
  - TURN 配置硬失败与 secret redaction 验证
affects: [phase-06, chrome-e2e, turn, ice, peer-connection, media, tests]
tech-stack:
  added: []
  patterns: [turn-relay-e2e, selected-candidate-evidence, secret-redaction]
key-files:
  created:
    - .planning/phases/06-chrome-e2e/06-05-SUMMARY.md
  modified:
    - examples/chrome-e2e/browser-client.js
    - examples/chrome-e2e/mrtc_chrome_answerer.c
    - include/micrortc/peer_connection.h
    - src/ice/ice_agent.c
    - src/ice/ice_agent.h
    - src/ice/ice_config.c
    - src/peer_connection.c
    - src/stun/stun_message.c
    - src/stun/stun_message.h
    - src/turn/turn_client.c
    - src/turn/turn_client.h
    - tests/e2e/chrome-turn.spec.js
    - tests/e2e/summary.js
    - tests/e2e/turn-config.js
    - tests/integration/mrtc_phase5_media_verify.c
    - tests/media/test_media_fixtures.c
    - tests/transport/test_ice_config.c
    - tests/transport/test_stun_message.c
key-decisions:
  - "06-05: 公网 TURN 可用的前提是 C 端也必须发出公网可达候选；仅强制 Chrome relay 不足以完成 relay E2E。"
  - "06-05: 为保持真实 relay 验收，C 端补最小 TURN Allocate/CreatePermission/Send Indication/Data Indication 路径。"
  - "06-05: relay-only 验收同时要求 browser selected local candidate 为 relay，且 C 端 selected local candidate 为 relay。"
requirements-addressed: [NET-04, E2E-03, E2E-04, E2E-05, E2E-06, E2E-07, E2E-08, TEST-02, TEST-04]
requirements-completed: [NET-04, E2E-03]
duration: 95 min
completed: 2026-05-14
---

# Phase 06 Plan 05: TURN Relay E2E、Relay Stats 校验与 Secret Redaction Summary

**TURN relay E2E 已通过：C 端会通过公网 TURN 收集 srflx 和 relay candidate，Chrome 与 C answerer 最终 selected pair 均走 relay，并覆盖 connection、DataChannel、双向 H264/Opus 媒体。**

## Performance

- **Duration:** 95 min
- **Completed:** 2026-05-14
- **Tasks:** 4/4 completed
- **Files modified:** STUN/TURN primitive、PeerConnection selected transport、Chrome E2E、C answerer diagnostics、redaction helpers、media fixture verifier 和本 summary

## Accomplishments

- 新增 TURN Allocate、CreatePermission、Send Indication、Data Indication 的最小 UDP client 能力，并支持 long-term credential 认证。
- STUN/TURN message parser 扩展到 REALM、NONCE、ERROR-CODE、XOR-RELAYED-ADDRESS、XOR-MAPPED-ADDRESS、XOR-PEER-ADDRESS 和 DATA。
- PeerConnection 在配置 TURN credential 时收集 host、srflx、relay candidate，并在选中 relay 本地候选后通过 TURN Send/Data Indication 发送 DTLS/SRTP/SCTP 数据。
- C answerer 输出 `selected_pair` 诊断事件，报告 local/remote candidate type，不包含 TURN secret。
- Playwright TURN spec 强制 `iceTransportPolicy: "relay"`，等待 C 端发出 relay candidate，并断言 browser selected local candidate 与 C selected local candidate 均为 `relay`。
- TURN 配置 loader 保持缺配置硬失败，并对 summary/signaling/C demo 输出做 secret redaction 自测。
- 修复媒体 fixture verifier 对较大 H264 fixture 的容量假设，并接受 depacketizer 规范化 Annex-B start code 后的有效 H264 帧。

## Commits

| Commit | Message |
|--------|---------|
| `e13724a` | `feat(06-05): add hard-fail TURN config loader` |
| `49086ba` | `feat(06-05): add relay-only Chrome E2E spec` |
| `f7367ea` | `feat(06-05): require selected relay candidate stats` |
| `07d6302` | `feat(06-05): add C-side TURN relay path` |

## Issues Encountered

- 初始 relay-only Chrome 测试失败，因为 Chrome 虽然通过公网 TURN 获得 relay candidate，但 C answerer 只发出 `127.0.0.1 typ host`，公网 TURN 无法访问 C 端 loopback 候选。
- 在不本地部署 TURN server 的约束下，下一步必须让 C 端也通过公网 TURN 收集 srflx/relay candidate，并补上 relay 数据面；否则 candidate 收集成功也无法承载 DTLS/SRTP/SCTP。
- 媒体回归测试暴露 H264 fixture 变大后固定 buffer 不足，以及 depacketizer 规范化 start code 导致逐字节比较不再成立；已改为动态 packet buffer 和有效 H264 Annex-B 验证。

## Verification

- `node tests/e2e/turn-config.js --turn-config ./does-not-exist.json` - passed，缺配置非零退出。
- `node tests/e2e/signaling-server.js --self-test-redaction` - passed。
- `node tests/e2e/summary.js --self-test-redaction` - passed。
- `build/examples/chrome-e2e/mrtc_chrome_answerer --self-test-redaction` - passed。
- `cmake --build build --target micrortc mrtc_stun_message_test mrtc_ice_config_test` - passed。
- `ctest --test-dir build --output-on-failure -R 'mrtc_stun_message_test|mrtc_ice_config_test|mrtc_transport_smoke_test|mrtc_phase4_network_verify'` - passed，2/2 matched tests。
- `cmake --build build --target mrtc_media_fixtures_test mrtc_phase5_media_verify` - passed。
- `ctest --test-dir build --output-on-failure -R 'mrtc_media_fixtures_test|mrtc_phase5_media_verify'` - passed，2/2 tests。
- `ctest --test-dir build --output-on-failure` - passed，18/18 tests。
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --project=chrome-host` - passed，2 passed / 1 skipped。
- `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:turn -- --turn-config ./mrtc-ice-servers.local.json --project=chrome-turn` - passed，C 端发出 srflx + relay candidate，selected pair local/remote 均为 relay，connection/DataChannel/browser media/C media 均通过。
- 本地 TURN credential secret scan - passed，summary 与 signaling artifacts 未包含原始 `username`、`credential` 或 `password`。

## Known Stubs

- TURN relay client 当前是满足 v1 E2E 的最小 UDP relay 路径，未实现完整 ICE checklist、refresh/channel binding/TCP/TLS TURN。
- E2E 仍使用本机 Playwright Chromium channel；如安装系统 Chrome，可后续切换 channel 做等价验收。

## Next Phase Readiness

06-06 可以继续执行 v1 最终验收：汇总 CTest、host E2E、TURN relay E2E、secret scan 与最终文档/脚本入口。

## Self-Check: PASSED

- 缺 TURN 配置硬失败。
- Chrome 强制 relay-only。
- Browser selected local candidate type 为 `relay`。
- C selected local candidate type 为 `relay`。
- Relay 路径覆盖连接、DataChannel 和双向 H264/Opus 媒体。
- 日志与 artifacts 未泄漏真实 TURN credential。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-14*
