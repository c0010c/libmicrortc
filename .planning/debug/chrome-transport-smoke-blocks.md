---
status: blocked
trigger: "Chrome transport smoke blocks at connection stage after SDP mid alignment and real host candidate; investigate DTLS/SCTP/DataChannel interop needed for MRTC_E2E_REQUIRE_TRANSPORT=1"
created: 2026-05-13
updated: 2026-05-13
---

# Debug Session: chrome-transport-smoke-blocks

## Symptoms

- **Expected behavior:** `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` 应让浏览器侧 `RTCPeerConnection.connectionState === "connected"`，并完成 browser `ping:<nonce>` 到 C `pong:<nonce>` 的真实 DataChannel 往返。
- **Actual behavior:** 06-03 transport smoke 在分类模式下通过，但 artifact 记录 `signaling: passed`、`connection: blocked`、`datachannel: blocked`、`failure_stage: connection`。
- **Error messages:** Playwright 等待 browser connection 超时，最近 summary 记录 `page.waitForFunction: Timeout 10000ms exceeded.`。
- **Timeline:** 06-03 已修复 SDP mid/order、真实 host candidate 端口和 browser diagnostic filtering 后仍存在。
- **Reproduction:** 运行严格 smoke：`MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"`。

## Current Focus

- **hypothesis:** 根因已定位：当前代码只绑定并发布 UDP host candidate，没有 ICE connectivity check 处理、没有 UDP receive loop、没有真实 DTLS handshake，也没有 usrsctp DataChannel transport；因此 Chrome 只能进入 ICE checking / PeerConnection connecting。
- **test:** 严格 smoke 已复现；代码审计确认 `mrtc_peer_connection_add_ice_candidate()` 在收到 remote candidate 后直接把 C 内部状态置为 connected，但没有网络收包路径。
- **expecting:** 要让 `MRTC_E2E_REQUIRE_TRANSPORT=1` 通过，需要先实现/接入真实 ICE STUN binding response、DTLS 1.2 handshake、SRTP key export、SCTP/DCEP/DataChannel over DTLS。
- **next_action:** 规划真实 Chrome transport 实现，优先从本地 `reflib/kvs-webrtc-sdk` 搬迁 ICE/DTLS/SCTP packet path 或接入系统 libsrtp/usrsctp 后重构当前 stub。
- **reasoning_checkpoint:** 
- **tdd_checkpoint:** 

## Evidence

- timestamp: 2026-05-13T12:08:58Z
  observation: 严格命令 `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` 失败。
  detail: Playwright 期望 `summary.connection === "passed"`，实际为 `blocked`；浏览器本地事件进入 `ice.checking` / `pc.connecting` 后超时。
- timestamp: 2026-05-13T12:08:58Z
  observation: C answerer 已输出真实 candidate，例如 `candidate:1 1 UDP ... 127.0.0.1 52240 typ host`。
  detail: 信令和 candidate 交换不是当前第一失败点。
- timestamp: 2026-05-13T12:09:20Z
  observation: `src/ice/ice_agent.c` 只实现 candidate parse/format、host UDP bind/close 和 `mrtc_ice_packet_is_stun()` 判断。
  detail: 没有 `recvfrom` / `sendto` 读写循环，也没有 STUN Binding success response 生成和 ICE nominated pair 状态机。
- timestamp: 2026-05-13T12:09:30Z
  observation: `src/peer_connection.c` 的 `mrtc_peer_connection_add_ice_candidate()` 在有 local/remote description 后直接调用 `mrtc_peer_connection_start_secure_transport()` 并设置 C 内部 connected。
  detail: 该路径由 remote candidate 触发，不依赖 Chrome STUN connectivity check 或 DTLS handshake，属于诊断假阳性。
- timestamp: 2026-05-13T12:09:40Z
  observation: `src/dtls/dtls_session.c` 是 stub 级 fingerprint/keying wrapper。
  detail: 没有 OpenSSL DTLS BIO、handshake packet IO、certificate verify 或真实 SRTP key export。
- timestamp: 2026-05-13T12:09:50Z
  observation: `src/sctp/sctp_session.c` 是 in-process callback wrapper。
  detail: `mrtc_sctp_session_connect()` 直接标记 connected 并回调 DCEP-like payload，没有 usrsctp、SCTP over DTLS packetization 或 browser DCEP 互通。
- timestamp: 2026-05-13T12:10:13Z
  observation: 修复 browser-client 后，分类模式 smoke 通过，远端诊断事件变为 `remote.pc.connected` / `remote.datachannel.open`。
  detail: 远端 C 诊断不再污染 browser 本地 `pc.connected` / `datachannel.message` 事件，后续 smoke 只会相信真实 browser local state。
- timestamp: 2026-05-13T12:10:30Z
  observation: `cmake -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` 配置失败。
  detail: 本机缺少 `libsrtp` 和 `usrsctp`，CMake 输出 `libsrtp is required when MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON`。
- timestamp: 2026-05-13T12:51:22Z
  observation: 06-07 部分收口后，classification transport smoke 通过但 strict transport smoke 仍失败。
  detail: `tests/e2e/artifacts/summary.json` 记录 `signaling: passed`、`connection: blocked`、`datachannel: blocked`、`failure_stage: connection`；answerer 不再因 UDP polling 失败退出，浏览器仍停在 `pc.connecting`。
- timestamp: 2026-05-13T12:51:22Z
  observation: 本机严格 transport configure 仍失败在系统依赖缺口。
  detail: `cmake -S . -B build-transport -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DMRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON` 报告缺少 `libsrtp` 和 `usrsctp`；OpenSSL 可用。

## Eliminated

- hypothesis: SDP mid/order 不匹配导致 Chrome 拒绝 answer。
  reason: 06-03 已修复 answer BUNDLE/mid 对齐；严格复现中 browser 成功 apply answer，并进入 ICE checking。
- hypothesis: answerer 仍输出 port 9 placeholder candidate。
  reason: 严格复现中 C candidate 为真实本地 UDP 端口。
- hypothesis: browser diagnostic/error 被转发到 C answerer stdin 导致进程提前退出。
  reason: signaling server 已过滤 browser event/error；本次失败发生在 browser connection wait 超时之后。

## Resolution

- **root_cause:** 当前 PeerConnection transport 是内部 harness 语义，不是真实 Chrome WebRTC transport：host candidate 有真实 UDP socket，但没有 ICE STUN request/response 和 nominated pair；DTLS/SCTP/DataChannel 也是 stub/loopback，C 内部 connected/datachannel.open 由 remote candidate 触发，不能让 browser 侧 connectionState connected。
- **fix:** 已修复 smoke 假阳性风险并推进部分 transport 收口：browser-client 诊断事件仍保持 `remote.*`；C answerer 主循环会轮询 PeerConnection UDP/STUN transport；STUN helper 覆盖 Binding request/response、XOR-MAPPED-ADDRESS、MESSAGE-INTEGRITY 和 FINGERPRINT；DTLS fingerprint 改为 OpenSSL self-signed certificate SHA-256 digest；SCTP/DataChannel 不再在 connect/addIceCandidate 时触发 in-process open/message 假阳性。
- **verification:** `ctest --test-dir build --output-on-failure` 18/18 passed；classification `MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` passed；strict `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` 仍失败在 connection stage。
- **files_changed:** `src/stun/stun_message.*`, `src/ice/ice_agent.*`, `src/dtls/dtls_session.c`, `src/sctp/sctp_session.*`, `src/data_channel/data_channel.h`, `src/peer_connection.c`, `include/micrortc/peer_connection.h`, `examples/chrome-e2e/mrtc_chrome_answerer.c`, `tests/transport/test_stun_message.c`, `tests/transport/test_sctp_data_channel.c`, `tests/peer_connection/test_peer_connection_api.c`, `.planning/phases/01-/SOURCE-MANIFEST.md`, `.planning/debug/chrome-transport-smoke-blocks.md`
