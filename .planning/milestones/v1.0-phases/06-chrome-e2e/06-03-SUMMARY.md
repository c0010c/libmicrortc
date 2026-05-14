---
phase: 06-chrome-e2e
plan: "03"
subsystem: chrome-e2e-transport
tags: [sdp, ice, datachannel, playwright, chrome, transport-smoke]
requires:
  - phase: 06-chrome-e2e
    provides: 06-01/06-02 browser harness、signaling bridge 和 C answerer JSON-line demo
provides:
  - Chrome offer media 顺序与 mid 驱动的 answer SDP
  - PeerConnection 主路径绑定的真实本地 UDP host candidate 端口
  - DataChannel ping:<nonce> / pong:<nonce> loopback 语义
  - Chromium transport smoke 的 signaling/connection/datachannel 分层 summary
affects: [phase-06, chrome-e2e, sdp, ice, datachannel, transport]
tech-stack:
  added: []
  patterns: [remote-offer-driven SDP mids, peerconnection-owned host endpoint, filtered signaling bridge, staged transport smoke]
key-files:
  created:
    - tests/e2e/chrome-transport.spec.js
  modified:
    - src/sdp.c
    - src/ice/ice_agent.c
    - src/ice/ice_agent.h
    - src/peer_connection.c
    - examples/chrome-e2e/browser-client.js
    - tests/e2e/signaling-server.js
    - tests/fixtures/minimal_offer.sdp
    - tests/sdp/test_sdp_roundtrip.c
    - tests/peer_connection/test_peer_connection_api.c
    - tests/transport/test_transport_smoke.c
key-decisions:
  - "06-03: Answer SDP 必须保留 remote Chrome offer 的 media section 顺序和 mid token。"
  - "06-03: PeerConnection/answerer 主路径不再把 port 9 placeholder candidate 当作成功路径。"
  - "06-03: Browser diagnostic/error 只进入测试 summary，不转发到 C answerer stdin。"
  - "06-03: 当前 DTLS/SCTP 仍未与 Chrome 真实互通，smoke 默认记录 blocked；强制验收使用 MRTC_E2E_REQUIRE_TRANSPORT=1。"
patterns-established:
  - "Chrome transport smoke 必须从 browser 侧 connectionState 和 DataChannel pong 判定真实互通。"
  - "分层 E2E artifact 至少记录 signaling、connection、datachannel、failure_stage 和 failure_reason。"
requirements-addressed: [E2E-03, E2E-04, E2E-05, TEST-02, TEST-04]
requirements-completed: []
duration: unknown
completed: 2026-05-13
---

# Phase 06 Plan 03: 真实 Chrome Host Transport 前置修复 Summary

**Chrome offer mid 对齐、真实 host candidate 端口和 transport smoke 分层诊断已建立，但真实 DTLS/SCTP Chrome 连接仍阻塞在 connection 阶段。**

## Performance

- **Duration:** unknown
- **Started:** 2026-05-13T11:48:34Z
- **Completed:** 2026-05-13
- **Tasks:** 4 planned tasks plus 1 regression fix
- **Files modified:** 10 tracked source/test files plus this summary

## Accomplishments

- `mrtc_sdp_parse()` 现在记录 remote offer 的 media kind 与 `a=mid`，answer 生成会按 Chrome offer 的 media 顺序和 mid 输出 `a=group:BUNDLE 0 1 2` 与对应 m-line。
- PeerConnection 主路径会绑定本地 UDP socket，answerer 输出真实 host candidate 端口，不再把 `127.0.0.1 9 typ host` 作为 Chrome E2E 成功路径。
- `mrtc_data_channel_send()` 保留既有 `ping`/`pong` 语义，并支持 `ping:<nonce>` 返回 `pong:<nonce>`。
- 新增 `tests/e2e/chrome-transport.spec.js`，可启动 signaling server、C answerer 和 browser 页面，写出 transport smoke 分层 summary。
- Signaling server 过滤 browser diagnostic/error，避免把非 signaling/control schema 消息写入 C answerer stdin。

## Task Commits

1. **Task 06-03-01: 修正 Chrome offer / C answer SDP mid 与 m-line 对齐** - `71c66d2` (fix)
2. **Task 06-03-02: 实现 Chrome 可连接的本地 host candidate endpoint** - `190ae0b` (fix)
3. **Task 06-03-03: 保留 DataChannel nonce ping/pong 行为** - `fb34f92` (fix)
4. **Task 06-03-04: 新增 Chrome transport smoke verifier** - `9c41c9b` (test)
5. **Regression fix: 跳过 remote offer 中本地未声明的 media section** - `f0aeb79` (fix)

## Files Created/Modified

- `tests/e2e/chrome-transport.spec.js` - Chrome/Chromium transport smoke，记录 signaling、connection、datachannel 和 failure stage。
- `src/sdp.c` - 解析 remote media kind/mid，按 remote offer 顺序生成 answer，并跳过本地未声明的 remote media section。
- `src/ice/ice_agent.c` / `src/ice/ice_agent.h` - 增加 host endpoint bind/format/close lifecycle。
- `src/peer_connection.c` - 在 PeerConnection local description 主路径绑定真实 host endpoint，释放时关闭，并保留 `ping:<nonce>` / `pong:<nonce>`。
- `examples/chrome-e2e/browser-client.js` - remote answer apply 前缓存 candidate，避免早到 candidate 触发浏览器错误。
- `tests/e2e/signaling-server.js` - 只转发允许的 browser-to-answerer signaling/control message，diagnostic/error 留在 summary。
- `tests/fixtures/minimal_offer.sdp`、`tests/sdp/test_sdp_roundtrip.c`、`tests/peer_connection/test_peer_connection_api.c`、`tests/transport/test_transport_smoke.c` - 覆盖 mid 对齐、candidate 端口和 ping/pong 行为。

## Decisions Made

- 不把 C 内部 `pc.connected` 或 `datachannel.open` 视为 Chrome 真实互通成功；真实成功门槛仍是 browser `RTCPeerConnection.connectionState === "connected"` 和 browser 收到 `pong:<nonce>`。
- 当前环境没有系统 `usrsctp` / `libsrtp`，仓库内 DTLS/SCTP 模块仍不足以与 Chrome 完成 full DTLS/SCTP/DataChannel 互通；因此 smoke 默认只把 connection 阻塞分类记录为 artifact。
- `MRTC_E2E_REQUIRE_TRANSPORT=1` 是后续强制验收开关；打开后当前 transport smoke 应失败，而不是产生假阳性。
- 因 self-check 未通过，本计划不标记 `E2E-03`、`E2E-04`、`E2E-05`、`TEST-02`、`TEST-04` 为完成。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 跳过本地未声明的 remote media section**
- **Found during:** Full CTest after Task 06-03-04
- **Issue:** remote offer 包含 audio/application，但部分 C 单测只创建 video transceiver；严格要求所有 remote section 都匹配导致 `mrtc_media_send_test` 和 `mrtc_rtcp_retransmit_test` 连接失败。
- **Fix:** Answer 生成按 remote 顺序处理，但只输出本地已声明 kind；仍要求所有本地 kind 都能在 remote offer 中找到。
- **Files modified:** `src/sdp.c`
- **Verification:** `ctest --test-dir build --output-on-failure` 18/18 passed。
- **Committed in:** `f0aeb79`

---

**Total deviations:** 1 auto-fixed (Rule 3)
**Impact on plan:** 该修复保持 Chrome offer 对齐目标，同时恢复既有单媒体本地测试兼容性。

## Issues Encountered

- Plan 级 success criteria 仍未达成：Chromium smoke 的 artifact 为 `signaling: passed`、`connection: blocked`、`datachannel: blocked`、`failure_stage: connection`。
- `src/dtls/dtls_session.c` 和 `src/sctp/sctp_session.c` 仍是轻量 stub/wrapper 级实现，当前未能完成真实 Chrome DTLS/SCTP/DataChannel 互通。
- CMake configure 仍报告缺少系统 `MRTC_SRTP` / `MRTC_USRSCTP`；本计划没有引入新的第三方依赖或 vendor 源码。
- 本机没有系统 Google Chrome，E2E smoke 使用 Playwright bundled Chromium fallback；summary 已记录 `system_chrome_available: false` 和 `chromium_fallback: true`。

## Verification

- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure` - passed，18/18 tests passed。
- `ctest --test-dir build -R "network|ice|data_channel|peer_connection|transport" --output-on-failure` - passed，4/4 tests passed。
- `ctest --test-dir build -R "data_channel|network|peer_connection" --output-on-failure` - passed，2/2 tests passed。
- `printf '{"type":"offer","sdp":...}' | ./build/examples/chrome-e2e/mrtc_chrome_answerer | rg '"type":"answer"'` - passed，answer 含 `BUNDLE 0 1 2` 和真实非 9 host candidate 端口。
- `MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` - passed in classification mode；artifact 明确记录 connection blocked。

## Known Stubs

- Chrome 真实 transport 仍缺 full DTLS/SCTP/DataChannel interop；当前 smoke 只提供分层失败定位，不提供 plan 级成功证明。
- SRTP/usrsctp 系统依赖仍未接入强制构建路径；媒体 E2E 不应在当前状态下推进为真实 browser transport 验收。

## Threat Flags

- **HIGH:** 若后续自动化只看 Playwright exit code，可能误把 classification-mode pass 当成真实 Chrome transport pass。必须同时检查 artifact 或使用 `MRTC_E2E_REQUIRE_TRANSPORT=1`。
- **HIGH:** 06-04 browser media 断言不能建立在当前 blocked transport 上；应先补真实 DTLS/SCTP/ICE packet path 或把 06-04 调整为明确的 staged-failure 验收。

## User Setup Required

None - 本计划不需要外部服务配置。若要验证默认 Chrome channel，需要在本机安装 Google Chrome；当前已通过 bundled Chromium fallback 记录 smoke 结果。

## Next Phase Readiness

06-04 尚不能按“真实 browser-connected transport 上做媒体断言”的原目标直接执行。下一步应优先补齐 Chrome 互通所需的 DTLS/SCTP/DataChannel 底层能力，或显式重切 06-04 范围为 staged failure 验收；完成后用 `MRTC_E2E_REQUIRE_TRANSPORT=1` 重新运行 transport smoke。

## Self-Check: FAILED

- 代码修复、单元/集成测试和分类模式 smoke 均已通过。
- 计划级成功标准未通过：Chrome/Chromium browser 侧没有达到 `connectionState === "connected"`，也没有完成真实 browser DataChannel `ping:<nonce>` / `pong:<nonce>` 往返。
- 因此本 summary 记录执行结果与阻塞层级，但不声明 Plan 06-03 完成，也不标记需求完成。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-13*
