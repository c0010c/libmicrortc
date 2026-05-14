# Phase 4: 传输、安全与 DataChannel 协议核心 - Patterns

**Created:** 2026-05-12

## Existing Project Patterns To Reuse

| Target area | Current analog | Reuse rule |
|-------------|----------------|------------|
| Public headers | `include/micrortc/micrortc.h`, `include/micrortc/peer_connection.h` | 保留 include guard、`extern "C"`、`MRTC_STATUS`、opaque handle、`mrtc_*` 命名；不要暴露 AWS/PIC 类型。 |
| PeerConnection state | `src/peer_connection.c` | 继续在 private struct 中持有 owned resources；所有字符串/对象在 `mrtc_peer_connection_free()` 释放。 |
| SDP helper | `src/sdp.h`, `src/sdp.c` | 继续 private helper，不安装；可拆成 `src/sdp_*` 文件，但 public API 不直接暴露 parser internals。 |
| Tests | `tests/sdp/test_sdp_roundtrip.c`, `tests/peer_connection/test_peer_connection_api.c` | 小型 C `main()` + return code；CTest 注册；fixture 放 `tests/fixtures/`。 |
| Build | `CMakeLists.txt` | 所有核心 sources 进入 `micrortc` target；测试 executable 只在 `MRTC_BUILD_TESTS` 下添加。 |
| Source tracing | `.planning/phases/01-/SOURCE-MANIFEST.md` | Phase 4 实际派生/裁剪/reference-only 的 KVS 文件必须追加记录，不用全量列目录。 |

## Reference Patterns From Local KVS Baseline

| Planned capability | Local reference | Extraction guidance |
|--------------------|-----------------|---------------------|
| ICE agent and candidate pair | `reflib/kvs-webrtc-sdk/src/source/Ice/IceAgent.*`, `IceUtils.*`, `ConnectionListener.*`, `SocketConnection.*`, `Network.*` | 保留 host/srflx/relay candidate、connectivity check、selected pair 和 inbound packet callback 语义；裁剪 KVS diagnostics 和非 v1 平台路径。 |
| STUN/TURN messages | `reflib/kvs-webrtc-sdk/src/source/Stun/Stun.*`, `Ice/TurnConnection.*` | 保留 binding、XOR-MAPPED-ADDRESS、MESSAGE-INTEGRITY、TURN allocation/permission/channel bind；真实 TURN credential 不进入 repo。 |
| DTLS OpenSSL | `reflib/kvs-webrtc-sdk/src/source/Crypto/Dtls.h`, `Dtls_openssl.c`, `Crypto.c`, `IOBuffer.*` | 优先 OpenSSL；保留 generated certificate、SHA-256 fingerprint、outbound packet callback、remote fingerprint verify 和 keying material。 |
| SRTP session | `reflib/kvs-webrtc-sdk/src/source/Srtp/SrtpSession.*` | 保留 libsrtp session 创建/释放和 profile mapping；Phase 4 只需 session 初始化，不做完整媒体 protect/unprotect 验收。 |
| SCTP/DataChannel | `reflib/kvs-webrtc-sdk/src/source/Sctp/Sctp.*`, `PeerConnection/DataChannel.*` | 保留 usrsctp lifecycle、DCEP open/ack、PPID string/binary 和 callbacks；public API 改成 MRTC handle。 |
| Transport orchestration | `reflib/kvs-webrtc-sdk/src/source/PeerConnection/PeerConnection.*` | 仅参考 ICE selected pair -> DTLS -> SRTP -> SCTP/DataChannel 编排；不要迁入 RTP/RTCP/media/transceiver 逻辑。 |
| SDP transport attributes | `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.*` | 参考 `setup`、fingerprint、ICE attrs、application m-line 处理；改造现有 `src/sdp.c` 或拆分 private 模块。 |
| Public API model | `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | 参考 `RtcIceServer`、`RtcDataChannel`、`RtcDataChannelInit`、callback signatures；改名并裁剪为 `MRTC_*`。 |

## Planned Files and Closest Analogs

| Planned file/path | Role | Closest analog | Notes |
|-------------------|------|----------------|-------|
| `include/micrortc/peer_connection.h` | 扩展 ICE/DataChannel/connection public API | Existing same file + KVS `Include.h` | 增加类型/函数时保持 C ABI；避免 AWS/PIC typedef。 |
| `src/peer_connection.c` | Transport orchestration owner | Existing same file + KVS `PeerConnection.c` | 从字符串状态机扩展为持有 ICE/DTLS/SRTP/SCTP/DataChannel private objects。 |
| `src/sdp.c`, `src/sdp.h` 或 `src/sdp_transport.c` | SDP transport parser/generator | Existing SDP helper + KVS `SessionDescription.*` | 添加 setup/fingerprint/ICE/SCTP 属性，不提前实现媒体 RTP。 |
| `src/common/*` 或 `src/core/*` | Private portability layer | KVS common/PIC usage as reference | 只暴露给 core sources；不要安装。 |
| `src/ice/*`, `src/stun/*`, `src/turn/*` | ICE/STUN/TURN implementation | KVS `Ice/`, `Stun/` | 可以裁剪/重写；必须保留 host/STUN/TURN 验证路径。 |
| `src/dtls/*` | DTLS OpenSSL wrapper | KVS `Crypto/Dtls_openssl.c`, `Dtls.h` | 包含 certificate/fingerprint/role/key export。 |
| `src/srtp/*` | SRTP session wrapper | KVS `Srtp/SrtpSession.*` | Link libsrtp；创建 session 即 Phase 4 必需。 |
| `src/sctp/*`, `src/data_channel/*` | usrsctp and DataChannel | KVS `Sctp/Sctp.*`, `DataChannel.*` | Support DCEP negotiated=false, text and binary. |
| `tests/transport/*` | Unit/integration harness | Existing CTest files | 分离 unit 和 real network harness；真实配置缺失必须失败。 |
| `mrtc-ice-servers.example.json` | Secret-free example config | None | 只放 placeholder；真实 `mrtc-ice-servers.local.json` 不提交。 |
| `.gitignore` | Local secret protection | Existing ignore file if any | 忽略真实 STUN/TURN config。 |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | Source trace updates | Existing manifest table | 每个实际迁入/派生文件追加 Phase 4 rows。 |

## CMake Pattern

Current style:

- `add_library(micrortc STATIC ...)`
- `add_library(micrortc::micrortc ALIAS micrortc)`
- `target_include_directories(micrortc PUBLIC ...)`
- tests under `if(MRTC_BUILD_TESTS)`

Phase 4 should extend this with private dependencies:

- `find_package(OpenSSL REQUIRED)` or documented equivalent.
- `find_package(PkgConfig)` / `pkg_check_modules` or CMake package lookup for libsrtp and usrsctp.
- Link dependencies with `target_link_libraries(micrortc PRIVATE ...)`, not public unless headers require them.
- Keep AWS/KVS signaling, libwebsockets, AWS SDK C++, credential/storage out of the core target.

## Testing Pattern

Use three groups:

| Group | Examples | Expected behavior |
|-------|----------|-------------------|
| Unit CTest | STUN parser, SDP transport attrs, config loader invalid/missing cases, DTLS role parser, DataChannel API invalid args | Always runnable without secrets. |
| Integration local | host candidate pair over loopback or local interfaces, in-process DTLS client/server, SCTP loop where appropriate | Runnable in developer env without TURN credential when no external path is needed. |
| Real network | host/STUN/TURN relay + DTLS/SRTP + DataChannel harness using `mrtc-ice-servers.local.json` | Required for Phase 4 completion; missing config fails clearly. |

## Quality Constraints for Plans

- Every plan must include `read_first` entries for current file and KVS reference file before edits.
- Every plan touching KVS-derived files must include a manifest acceptance criterion.
- Every public API task must include a grep/assertion that public headers do not expose AWS/PIC types such as `PCHAR`, `PVOID`, `STATUS`, `BOOL`, `UINT64` unless deliberately redefined as MRTC-owned types.
- Every transport integration task must name the observable state transition or harness output it will verify.
- Real credentials must never appear in PLAN.md, RESEARCH.md, PATTERNS.md, README, or example config.

