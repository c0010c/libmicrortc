# Phase 4: 传输、安全与 DataChannel 协议核心 - Research

**Created:** 2026-05-12
**Status:** Complete
**Question:** What do I need to know to PLAN this phase well?

## Scope Summary

Phase 4 不是简单把 AWS KVS WebRTC C SDK 的协议目录复制进 `micrortc`。当前代码只有 Phase 3 的薄 PeerConnection：保存 remote/local SDP 字符串、生成最小 answer、保存最后一个 remote ICE candidate。Phase 4 必须在这个薄 API 后面接入真实传输链路，并保持核心库仍然不包含应用层 signaling、媒体采集、编码器和 Phase 5 的 RTP/RTCP 媒体路径。

本阶段的合理实现顺序是：

1. 先建立依赖、平台和 portability 基础层，让 KVS 基线里的协议模块能在 `micrortc` target 中编译。
2. 再扩展 public API 与配置，尤其是 ICE server、连接状态和 DataChannel handle。
3. 然后扩展 SDP 语义，让 ICE credentials、candidate、DTLS `setup`、fingerprint 和 SCTP/DataChannel m-line 能进入 offer/answer 流。
4. 之后逐步接入 ICE/STUN/TURN、DTLS/SRTP、SCTP/DataChannel。
5. 最后用真实 harness 验证 host、STUN srflx、TURN relay、DTLS/SRTP 初始化和 DataChannel open/send/receive/close。

## Current Code Findings

### Existing `micrortc` boundary

- `include/micrortc/peer_connection.h` 已有 opaque `MRTC_PEER_CONNECTION_HANDLE`、`MRTC_PEER_CONNECTION_CONFIG`、callback table、SDP set/create 和 add ICE candidate API。
- `src/peer_connection.c` 目前只有字符串状态机：`NEW`、`REMOTE_SET`、`LOCAL_SET`，没有 ICE agent、DTLS session、SRTP session 或 SCTP session。
- `src/sdp.c` 只做逐行合法性检查、保存第一个 `m=` 行、生成最小 answer，不解析 `a=ice-ufrag`、`a=ice-pwd`、`a=fingerprint`、`a=setup`、`a=mid`、`a=candidate`、`m=application` 或 `a=sctp-port`。
- `CMakeLists.txt` 只有 `micrortc` 静态库、C99、CTest 小型 C 可执行测试。Phase 4 应延续这个测试形态，但需要为 OpenSSL、libsrtp、usrsctp 增加可配置查找和链接。
- 现有测试都是无框架 C `main()`，失败返回非零；这个模式适合继续用于协议 smoke/unit/harness。

### Local KVS baseline modules

以下来源必须以本地 `reflib/kvs-webrtc-sdk` 为准，当前 manifest 的默认 commit 是 `9eebcc4`。

| Area | Local reference | Planning note |
|------|-----------------|---------------|
| ICE/STUN/TURN | `src/source/Ice/`, `src/source/Stun/` | 包含 ICE agent、candidate pair、socket listener、TURN allocation/channel bind、STUN parser/transaction。依赖 KVS 基础类型、timer queue、state machine、hash/list/socket abstractions，需要先裁剪适配。 |
| DTLS OpenSSL | `src/source/Crypto/Dtls.h`, `Dtls_openssl.c`, `Crypto.c`, `IOBuffer.*` | 已有自签名证书、SHA-256 fingerprint、DTLS role start、outbound packet callback、remote fingerprint 校验、SRTP keying material 导出模式。v1 优先 OpenSSL，mbedtls 路径不应进入本阶段默认实现。 |
| SRTP | `src/source/Srtp/SrtpSession.*` | 使用 libsrtp 创建 transmit/receive sessions，profile 来自 DTLS-SRTP 协商；Phase 4 只需证明 session 创建，不需要真实 RTP protect/unprotect 流动。 |
| SCTP/DataChannel | `src/source/Sctp/Sctp.*`, `src/source/PeerConnection/DataChannel.*` | 使用 usrsctp；PPID 包含 DCEP、string、binary；支持 DCEP open/ack 和 message callback。需要裁剪成 MRTC DataChannel handle/API。 |
| PeerConnection integration | `src/source/PeerConnection/PeerConnection.*`, `SessionDescription.*` | 展示 KVS 如何把 ICE selected pair、DTLS、SRTP、SCTP 和 DataChannel 组合起来；也包含很多 Phase 5 媒体、RTP/RTCP、transceiver 逻辑，应作为参考而不是整块复制。 |
| Public API source | `src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | 可参考 `RtcIceServer`、`RtcConfiguration`、`RtcDataChannel`、`RtcDataChannelInit`、`RtcOnMessage`、`RtcOnOpen`、`createDataChannel`、`dataChannelSend` 等模型，但 public header 必须改为 `mrtc_*` 和 MRTC 类型，不暴露 AWS/PIC 基础类型。 |

## Implementation Strategy

### 1. Portability and dependency layer

KVS 源码使用 `STATUS`、`PBYTE`、`BOOL`、`MUTEX`、timer queue、state machine、double list、hash table、socket helpers 等 PIC/KVS 基础设施。直接复制会把大量非核心支撑代码带入 public/API 边界，且容易误引入 signaling 或 credential/storage。

推荐计划中显式建立 `src/core/` 或 `src/common/` 级别的 private portability 层：

- status/error mapping 统一映射到 `MRTC_STATUS`，内部可以有 private `mrtc_status_t` 扩展错误码。
- memory/string/time/random/log helpers 保持 private，不进入 installed public API。
- timer、mutex、thread、condition、hash/list/queue 可以先以最小功能迁入或重写，目标是满足 ICE/DTLS/SCTP 状态机，而不是保留完整 KVS 通用库。
- socket abstraction 需要支持 Linux x86_64、UDP、TCP/TURN relay 所需路径；v1 不做跨平台包装。

该基础层应在 Wave 1 先落地，否则后续每个协议模块都会临时发明自己的类型和错误处理。

### 2. Public API expansion

Phase 4 public API 应保持 Phase 3 风格：

- `MRTC_STATUS`
- `mrtc_*` 函数命名
- opaque handle
- callback table
- caller-owned buffer 或明确生命周期
- 不暴露 AWS/PIC 类型

建议新增：

- `MRTC_ICE_SERVER`：`urls`、`username`、`credential`，允许 STUN/TURN URL。
- `MRTC_PEER_CONNECTION_CONFIG` 扩展：`ice_servers`、`ice_server_count`、可选 `ice_config_file` 或 loader API。用户决策要求真实配置来自项目根目录文件，计划可选择提供测试 helper 读取 JSON，再填入 config。
- `MRTC_PEER_CONNECTION_STATE` 和 `on_connection_state_change`，用于 harness 判断 ICE/DTLS/SCTP 路径。
- `MRTC_DATA_CHANNEL_HANDLE`、`MRTC_DATA_CHANNEL_INIT`、`MRTC_DATA_CHANNEL_CALLBACKS`。
- `mrtc_peer_connection_create_data_channel()`、`mrtc_data_channel_send()` 或拆分 `send_text` / `send_binary`、`mrtc_data_channel_close()`、`mrtc_data_channel_free()`。

DataChannel 回调至少需要：

- `on_open(user_data, channel)`
- `on_message(user_data, channel, is_binary, data, data_len)`
- `on_close(user_data, channel)`

`negotiated=false` 的 DCEP 常规打开流程是 Phase 4 必选；`negotiated=true` 不应成为阻塞目标。

### 3. SDP expansion

Phase 4 的 SDP helper 需要从“最小 answer”升级为 WebRTC transport answer builder：

- Parse remote session-level and media-level `a=ice-ufrag` / `a=ice-pwd`。
- Parse remote `a=fingerprint:sha-256 ...`，并在 DTLS 握手后校验。
- Parse remote `a=setup:actpass|active|passive`，推导本端 DTLS role。
- Generate local ICE credentials。
- Generate `a=fingerprint:sha-256 ...` from library-generated certificate.
- Generate host/srflx/relay candidate lines through `on_ice_candidate` callback and/or SDP initial answer。
- Generate `m=application ... UDP/DTLS/SCTP webrtc-datachannel` and `a=sctp-port:5000` when DataChannel is present.
- Preserve Phase 5 media sections without pretending RTP/RTCP is implemented in Phase 4.

角色推导边界：

| Remote `setup` | Local answer `setup` | Local DTLS role |
|----------------|----------------------|-----------------|
| `actpass` | `active` or `passive` by chosen policy | If answer says `active`, local is client; if `passive`, local is server. For Chrome interop, answerer commonly chooses `active`, but planner/executor must verify against KVS behavior and tests. |
| `active` | `passive` | Local is server. |
| `passive` | `active` | Local is client. |

计划必须避免“测试配置硬指定 DTLS role”；测试可以输入 SDP，但 role 来源必须是 SDP。

### 4. ICE/STUN/TURN integration

ICE 计划需要拆成可验证子层：

- STUN message parse/serialize、transaction id、XOR-MAPPED-ADDRESS、MESSAGE-INTEGRITY/FINGERPRINT。
- ICE server URL parsing：`stun:`, `turn:`, `turns:` 可先限定 v1 支持范围，至少覆盖 `turn:<host>:3478?transport=udp` 和 STUN 同 server。
- Host candidate gathering：Linux IPv4 UDP socket，排除 loopback 策略由 planner 决定，但 harness 要能验证 host path。
- Srflx gathering：对配置 STUN server 发 binding request，得到 server-reflexive candidate。
- TURN relay：allocation、create permission、channel bind、relay candidate 和通过 relay 发送数据。
- ICE connectivity checks：remote credentials、candidate pair、nomination、selected pair、keepalive。

用户决策要求 Phase 4 的真实 STUN/TURN 验证是硬前置：配置文件缺失或不可读时，真实网络验证必须失败，不可静默 skip。为避免默认 `ctest` 在无秘密环境中总失败，计划可以拆分命令：

- `ctest --test-dir build`：构建和不含秘密的协议单元测试。
- `cmake --build build --target mrtc_phase4_network_verify` 或 `./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json`：真实 host/STUN/TURN/DTLS/DataChannel 验证，缺配置即失败。

如果执行阶段选择把真实网络验证加入 CTest，应使用明确 label，并在 Phase 4 验收命令中强制运行该 label。

### 5. DTLS and SRTP integration

推荐基于 OpenSSL 路径规划：

- CMake 查找 OpenSSL，并定义 private OpenSSL implementation。
- PeerConnection 创建时生成或持有自签名证书。
- 本地 SDP answer 写入 `a=fingerprint:sha-256 ...`。
- DTLS outbound callback 通过 ICE selected pair 发送。
- ICE inbound data demux 到 STUN / DTLS / SCTP / later RTP。
- 握手完成后校验 remote fingerprint。
- 使用 DTLS keying material 创建 SRTP session。
- 记录 DTLS/SRTP 初始化状态，供 harness 断言。

Phase 4 不应要求真实 RTP 包 SRTP protect/unprotect；可以添加 SRTP session 创建单元测试和 DTLS key export integration test。

### 6. SCTP/DataChannel integration

SCTP/DataChannel 应挂在 DTLS application data 上：

- Initialize/deinitialize usrsctp once，避免每个 PeerConnection 重复全局初始化导致 teardown 问题。
- DCEP open/ack 使用 PPID 50；文本 PPID 51/56；二进制 PPID 53/57。
- `mrtc_peer_connection_create_data_channel()` 创建本地 channel，分配 stream id，等待 SCTP ready 后发送 DCEP open。
- 收到远端 DCEP open 时创建 remote DataChannel handle 并触发 PeerConnection 级 `on_data_channel` 或 channel callback。
- 支持 text ping/pong 和一个基础 binary buffer 用例。
- Close 路径要释放 handle、从 PeerConnection hash/list 移除，并触发 `on_close`。

## Suggested Plan Shape

推荐拆为 5 个 plan / 4 个 wave：

| Wave | Plan | Purpose | Requirements |
|------|------|---------|--------------|
| 1 | 04-01 | 建立协议基础设施、依赖查找、source manifest 策略和测试骨架 | PROTO-01, PROTO-02, PROTO-03, PROTO-05 |
| 2 | 04-02 | 扩展 public API、配置、SDP transport attributes 和证书/fingerprint | API-05, PROTO-02 |
| 2 | 04-03 | 接入 ICE/STUN/TURN 并提供 host/STUN/TURN harness | PROTO-01, NET-01, NET-02, NET-03 |
| 3 | 04-04 | 接入 DTLS OpenSSL、SRTP session 和 selected pair 数据通道 | PROTO-02, PROTO-03 |
| 4 | 04-05 | 接入 SCTP/DataChannel，完成真实路径 DataChannel 验证和文档收口 | API-05, PROTO-05, NET-01, NET-02, NET-03 |

Wave 2 的 API/SDP 与 ICE 可以并行到一定程度，但 DTLS/SRTP 依赖 selected ICE pair，DataChannel 依赖 DTLS application data，因此 Wave 3/4 必须串行。

## Validation Architecture

### Automated test layers

| Layer | Command shape | Must prove |
|-------|---------------|------------|
| Build | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build` | `micrortc` links OpenSSL/libsrtp/usrsctp without AWS signaling or sample targets. |
| Unit CTest | `ctest --test-dir build --output-on-failure` | SDP parser, config loader, STUN packet helpers, DTLS/SRTP wrappers, SCTP/DataChannel unit behavior. |
| Boundary grep | `rg -n "kvsWebrtcSignalingClient|src/source/Signaling|libwebsockets|AWSSDK|credential|storage" CMakeLists.txt include src tests` should have no core-library hits | Core library remains signaling-free and AWS/KVS app-layer-free. |
| Real network verification | `./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json` | Config missing fails; with real config, host/STUN/TURN paths connect, DTLS reaches connected, SRTP session created, DataChannel text/binary open/send/receive/close succeeds. |
| Source manifest check | `rg -n "phase_added.*04|PROTO-01|PROTO-02|PROTO-03|PROTO-05|API-05|NET-01|NET-02|NET-03" .planning/phases/01-/SOURCE-MANIFEST.md` | All KVS-derived or reference-only Phase 4 source entries are recorded. |

### Test data and secrets

- Commit only an example config such as `mrtc-ice-servers.example.json` with placeholder `username` and `credential` values.
- Real local config should be named predictably, for example `mrtc-ice-servers.local.json`, and added to `.gitignore` if not already ignored.
- Real network verification must fail with a clear error if the local config file is missing, unreadable, or lacks required `urls` fields.
- PLAN.md must not include real TURN credentials.

### Acceptance assertions for Phase 4

- `include/micrortc/peer_connection.h` exposes DataChannel handle/API and connection/DataChannel callbacks without AWS/PIC types.
- `CMakeLists.txt` has private OpenSSL/libsrtp/usrsctp discovery/linking and no KVS signaling/library targets.
- `src/sdp.c` or split SDP files parse/generate `ice-ufrag`, `ice-pwd`, `fingerprint`, `setup`, `candidate`, and SCTP/DataChannel attributes.
- PeerConnection private struct owns ICE, DTLS, SRTP and SCTP/DataChannel objects and frees them cleanly.
- Host, STUN srflx and TURN relay candidate paths are each asserted by a real harness.
- DTLS role is derived from SDP `setup`; remote fingerprint is verified.
- SRTP session is created from exported DTLS keying material.
- DataChannel validation runs over real ICE/DTLS/SCTP, not only API or loopback tests.

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| KVS private dependencies balloon into the core library | Hard to maintain; accidental AWS coupling | Plan an explicit portability layer and source manifest task before protocol migration. |
| Full PeerConnection copy drags media/RTP into Phase 4 | Scope creep into Phase 5 | Use PeerConnection KVS code as integration reference; only migrate DataChannel and transport pieces needed for Phase 4. |
| Real STUN/TURN credentials leak into docs or commits | Security issue | Commit placeholder example only; local config ignored; plan acceptance explicitly greps for real credential absence where feasible. |
| DTLS role hardcoded in tests | Violates user decision and WebRTC semantics | Require SDP `setup` parser test for `actpass`/`active`/`passive` and integration assertion naming chosen role. |
| Default CI cannot access TURN server | False failures or skipped verification | Separate fast CTest from explicit Phase 4 real-network verify command; Phase 4 completion requires the explicit command in the user environment. |
| usrsctp global lifecycle is mishandled | Flaky teardown/crashes | Add init/deinit wrapper and tests; serialize global init; document no watch-mode tests. |

## Open Questions for Execution

- Whether to choose local answer `a=setup:active` or `passive` for Chrome interop should be verified against KVS behavior and Chrome offer fixtures during implementation. The plan should require the executor to document the chosen policy in tests.
- Whether JSON parsing for `mrtc-ice-servers.local.json` uses a tiny embedded parser, a small local parser, or a narrow hand-written loader is an execution choice. It must remain private/test-support unless promoted to public API deliberately.
- If full TURN over TCP/TLS is too large, Phase 4 may constrain required real TURN path to UDP relay as long as the configured URL and tests clearly assert `typ relay` and relay data transfer.

## RESEARCH COMPLETE

Phase 4 can be planned with a layered transport-first approach: private portability/dependency base, public API/SDP expansion, ICE/STUN/TURN, DTLS/SRTP, then SCTP/DataChannel real-path verification.

