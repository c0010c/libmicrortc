---
status: resolved
trigger: "Chrome strict transport smoke now passes after STUN/ICE/SCTP stream fixes; keep this as historical evidence for the 06-08 gate"
created: 2026-05-13
updated: 2026-05-14
---

# Debug Session: chrome-transport-smoke-blocks

## Symptoms

- **Expected behavior:** `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` 应让浏览器侧 `RTCPeerConnection.connectionState === "connected"`，并完成 browser `ping:<nonce>` 到 C `pong:<nonce>` 的真实 DataChannel 往返。
- **Actual behavior:** 已解决。最新严格 smoke 记录 `signaling: passed`、`connection: passed`、`datachannel: passed`，浏览器收到 `pong:<nonce>`。
- **Historical error messages:** 早期 Playwright 等待 browser connection 或 `datachannel.open/message` 超时。
- **Timeline:** 06-03 已修复 SDP mid/order、真实 host candidate 端口和 browser diagnostic filtering 后仍存在。
- **Reproduction:** 运行严格 smoke：`MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"`。

## Current Focus

- **hypothesis:** 已验证。剩余 blocker 不是 SDP/signaling，而是 STUN HMAC、ICE username 匹配、SCTP AF_CONN 端口字节序和远端 DataChannel stream 绑定。
- **test:** 严格 smoke 已通过；C 传输测试 6/6 通过。
- **expecting:** 06-04 可以在真实 Chrome transport 上继续补媒体 E2E 断言。
- **next_action:** 执行 06-04 host Chrome E2E 媒体计划。
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
- timestamp: 2026-05-13T13:16:38Z
  observation: 06-08 前置依赖门槛执行后仍阻塞。
  detail: 严格 configure 一次性汇总缺失依赖为 `libsrtp` 和 `usrsctp`；按 06-08 计划停止后续 DTLS/SCTP/PeerConnection packet pump 任务，避免生成假阳性 transport 通过。
- timestamp: 2026-05-14T02:46:37Z
  observation: 依赖安装后重新执行 06-08，严格 system transport configure/build 和 `ctest -R "dtls|srtp|sctp|data_channel|peer_connection|transport"` 均通过。
  detail: `build-transport/CMakeCache.txt` 中 `MRTC_SRTP_LIBRARY=/usr/lib/x86_64-linux-gnu/libsrtp2.so`、`MRTC_USRSCTP_LIBRARY=/usr/lib/x86_64-linux-gnu/libusrsctp.so`；DTLS memory BIO 单测覆盖 OpenSSL handshake、fingerprint mismatch 和 libsrtp protect/unprotect。
- timestamp: 2026-05-14T02:46:37Z
  observation: 严格 Chrome transport smoke 仍失败在 browser connection stage。
  detail: `tests/e2e/artifacts/summary.json` 记录 `signaling: "passed"`、`connection: "blocked"`、`datachannel: "blocked"`、`failure_stage: "connection"`、`failure_reason: "page.waitForFunction: Timeout 10000ms exceeded."`。signaling artifact 显示 answer/candidate 已交换，browser 进入 `ice.checking` / `pc.connecting` 后未到 `connected`。
- timestamp: 2026-05-14T03:02:11Z
  observation: 严格 Chrome transport smoke 通过。
  detail: `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` 1/1 passed；browser 本地事件包含 `pc.connected`、`datachannel.open` 和 `datachannel.message`，payload 为 `pong:<nonce>`。
- timestamp: 2026-05-14T03:02:11Z
  observation: 根因修复覆盖 STUN、ICE、SCTP 和 DataChannel stream。
  detail: 修正 STUN `MESSAGE-INTEGRITY` HMAC 输入范围；ICE username 接受 Chrome 的 `localUfrag:remoteUfrag` 形式；SCTP `sockaddr_conn.sconn_port` 改为 `htons(5000)`；远端 DCEP 到来时复用 channel 继承 Chrome stream id。

## Eliminated

- hypothesis: SDP mid/order 不匹配导致 Chrome 拒绝 answer。
  reason: 06-03 已修复 answer BUNDLE/mid 对齐；严格复现中 browser 成功 apply answer，并进入 ICE checking。
- hypothesis: answerer 仍输出 port 9 placeholder candidate。
  reason: 严格复现中 C candidate 为真实本地 UDP 端口。
- hypothesis: browser diagnostic/error 被转发到 C answerer stdin 导致进程提前退出。
  reason: signaling server 已过滤 browser event/error；本次失败发生在 browser connection wait 超时之后。

## Resolution

- **root_cause:** 严格 transport path 已从 stub 迁移到真实包路径后，Chrome 互通仍被几处协议细节阻塞：STUN HMAC 覆盖长度不符合 Chrome、ICE username 只接受错误方向、SCTP AF_CONN 端口不是网络字节序、pong 发送到本地默认 stream 而不是 Chrome DCEP stream。
- **fix:** 修正 STUN HMAC、ICE username 匹配、SCTP `htons(5000)`、DataChannel 远端 stream 绑定，并让 strict smoke 发送 ping 前等待 browser `datachannel.open`。
- **verification:** `ctest --test-dir build-transport -R "stun|ice|dtls|srtp|sctp|data_channel|peer_connection|transport" --output-on-failure` 6/6 passed；strict `MRTC_E2E_REQUIRE_TRANSPORT=1 MRTC_E2E_BROWSER_CHANNEL=chromium npm --prefix tests/e2e run test:host -- --grep "transport smoke"` 1/1 passed。
- **latest_06_08_gate:** 严格 system transport 依赖门槛已解除，OpenSSL/libsrtp/usrsctp 均可用；严格 Chrome smoke 已通过，06-04 host media E2E 可以继续。
- **files_changed:** `src/stun/stun_message.*`, `src/ice/ice_agent.*`, `src/dtls/dtls_session.c`, `src/sctp/sctp_session.*`, `src/data_channel/data_channel.h`, `src/peer_connection.c`, `include/micrortc/peer_connection.h`, `examples/chrome-e2e/mrtc_chrome_answerer.c`, `tests/transport/test_stun_message.c`, `tests/transport/test_sctp_data_channel.c`, `tests/peer_connection/test_peer_connection_api.c`, `.planning/phases/01-/SOURCE-MANIFEST.md`, `.planning/debug/chrome-transport-smoke-blocks.md`
