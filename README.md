# libmicrortc

`libmicrortc` 的目标是从本地 `reflib/kvs-webrtc-sdk` 基线中剥离可复用的 C WebRTC 协议栈能力，形成一个独立、可构建、可验证、可逐步清理的静态库。当前 Phase 3 已提供 AWS 风格裁剪后的 PeerConnection public API、signaling-free SDP helper 和基础 Answerer 流程；核心库仍不内置应用层 signaling、媒体采集或编码。

## 当前构建状态

当前工程提供独立 CMake 构建，核心 target 为 `micrortc`，导出命名空间为 `micrortc::`，静态库产物为 `libmicrortc.a`。CTest 会覆盖最小链接、SDP round-trip 和 PeerConnection Answerer API。

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
test -f build/libmicrortc.a
```

`MRTC_BUILD_TESTS=ON` 会构建 `micrortc_smoke_test`、`mrtc_sdp_roundtrip_test` 和 `mrtc_peer_connection_api_test`。这些测试分别验证 public include/静态库链接、SDP parse/serialize/answer helper，以及 remote offer -> local answer 的基础 API 流程。

## 安装与下游消费

可以把当前静态库安装到临时前缀，并用独立 consumer 工程验证 CMake package/export：

```bash
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
```

下游 CMake 工程应使用：

```cmake
find_package(micrortc CONFIG REQUIRED)
target_link_libraries(app PRIVATE micrortc::micrortc)
```

安装后会生成 `build/install/lib/cmake/micrortc/micrortcConfig.cmake`、`micrortcTargets.cmake` 和公开头文件 `build/install/include/micrortc/`。

## Phase 3 API 边界

当前公开 API 包含基础状态和初始化符号：

- `MRTC_STATUS`
- `mrtc_version_string()`
- `mrtc_initialize()`
- `mrtc_shutdown()`

Phase 3 新增 `include/micrortc/peer_connection.h`，并由 `<micrortc/micrortc.h>` umbrella header 引入。新增 public API 包括：

- `MRTC_PEER_CONNECTION_HANDLE`
- `MRTC_PEER_CONNECTION_CONFIG`
- `MRTC_PEER_CONNECTION_CALLBACKS`
- `mrtc_peer_connection_create()`
- `mrtc_peer_connection_free()`
- `mrtc_peer_connection_set_remote_description()`
- `mrtc_peer_connection_create_answer()`
- `mrtc_peer_connection_set_local_description()`
- `mrtc_peer_connection_create_offer()`
- `mrtc_peer_connection_add_ice_candidate()`

PeerConnection 使用 opaque handle，create 时一次性传入 config、callback table 和 `user_data`。SDP 和 ICE candidate 通过纯字符串边界交换，应用层可以用文件、stdio、HTTP、WebSocket 或测试 harness 自行完成 signaling，核心库不包含 signaling transport。

当前可用路径是 C 端 Answerer：调用方设置 remote offer 后，可以通过 `mrtc_peer_connection_create_answer()` 生成 local answer，再调用 `mrtc_peer_connection_set_local_description()`。`mrtc_peer_connection_create_offer()` 已预留接口，但在 Phase 3 明确返回 `MRTC_STATUS_NOT_IMPLEMENTED`。

Phase 4 已开始加入传输层能力：public API 提供 ICE server 配置、连接状态回调、DataChannel opaque handle、create/send/close 和 DataChannel callback table；SDP answer 会写入本地 ICE ufrag/pwd、DTLS fingerprint、`setup` role，并在存在 DataChannel 时加入 `m=application ... webrtc-datachannel` 和 `a=sctp-port:5000`。

当前实现仍处于 Phase 4 分层迁入过程中：host/STUN/TURN、DTLS/SRTP、SCTP/DataChannel 的真实互通会随后续计划逐步替换骨架实现。RTP/RTCP、H264/Opus 和媒体收发 API 仍保留到 Phase 5。

## Phase 4 本地 ICE 配置

仓库提供不含秘密的 `mrtc-ice-servers.example.json`：

```json
{
  "ice_servers": [
    {
      "urls": "turn:<host>:3478?transport=udp",
      "username": "<username>",
      "credential": "<credential>"
    }
  ]
}
```

真实 STUN/TURN 配置应放在项目根目录的 `mrtc-ice-servers.local.json`，该文件已被 `.gitignore` 忽略，不能提交真实 hostname、username、password、token 或 credential。真实网络验收命令会把配置缺失视为硬失败：

```bash
./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json --require-host --require-srflx --require-relay
```

Phase 4 的快速协议测试和完整传输验证命令如下：

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json --require-host --require-srflx --require-relay --require-dtls --require-srtp --require-datachannel
```

`ctest` 覆盖 public API、SDP transport attributes、ICE config、STUN helper、DTLS/SRTP wrapper、SCTP wrapper 和 DataChannel lifecycle。最后一条命令是 Phase 4 的本地真实网络验收入口；缺少 `mrtc-ice-servers.local.json` 时必须失败，不能自动跳过。

## 依赖边界

Phase 3 的 API、SDP 和 Answerer 基础流程仍不需要 OpenSSL、libsrtp 或 usrsctp，因此根 CMake 不查找、不下载、不链接这些依赖。它们仍然是 v1 runtime/core 依赖边界：

- OpenSSL：后续 DTLS/OpenSSL 路径进入时再决定发现和链接策略。
- libsrtp：后续 SRTP 会话和媒体保护进入时再链接。
- usrsctp：后续 SCTP/DataChannel 进入时再链接。

以下组件不参与核心 `micrortc` target，也不得被当前构建间接引入：

- libwebsockets
- AWS SDK C++
- KVS signaling
- AWS credential/storage/KVS service paths
- GStreamer sample
- kvsCommonLws
- kvspicUtils
- kvspicState

`kvsCommonLws`、`kvspicUtils` 和 `kvspicState` 只能作为后续迁移 allocator、logging、state、utils 等基础能力时的 reference-only 参考，不是当前核心库依赖。

## 来源归属

Phase 2 新增的 CMake、最小源码、测试和文档是 `original` 骨架文件。Phase 3 的 PeerConnection API、SDP helper、Answerer 状态机和测试 fixture 以本地 `reflib/kvs-webrtc-sdk` commit `9eebcc4` 为参考，采用 `rewritten-derived` 或 `reference-only` 方式重写并裁剪。

相关记录已追加到 `.planning/phases/01-/SOURCE-MANIFEST.md`。如果后续阶段从 `reflib/kvs-webrtc-sdk` 复制、裁剪、改名或 rewritten-derived 任意源码、头文件、CMake 片段或测试逻辑，也必须继续更新该 manifest，记录本地来源路径、目标路径、基线提交和派生方式。
