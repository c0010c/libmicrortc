# Phase 3: AWS 风格 API 与 signaling-free PeerConnection - Validation Strategy

## 目标

验证 Phase 3 是否真正交付 AWS 风格 API 与 signaling-free PeerConnection 边界，而不是只新增头文件声明或字符串透传。

## 必须验证的事实

| ID | 验证事实 | 证明方式 |
|----|----------|----------|
| V-03-01 | 本地 `reflib/kvs-webrtc-sdk` 已恢复且提交为 `9eebcc4` | `test -d reflib/kvs-webrtc-sdk && git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD` |
| V-03-02 | Public API 暴露 PeerConnection lifecycle、SDP、ICE candidate、config、callbacks | `rg` 检查 `include/micrortc/*.h` 中 `MRTC_PEER_CONNECTION_HANDLE`、`mrtc_peer_connection_create`、`create_answer`、`set_remote_description`、`add_ice_candidate` |
| V-03-03 | Public API 不暴露 AWS/PIC 基础类型 | 禁止 `PCHAR`、`PVOID`、`BOOL`、裸 `STATUS` 出现在 `include/micrortc/*.h` |
| V-03-04 | Core target 不依赖 KVS signaling、libwebsockets、AWS SDK C++ | 禁止 CMake/include/source 中出现 `kvsWebrtcSignalingClient`、`src/source/Signaling`、`libwebsockets`、`AWSSDK` |
| V-03-05 | SDP parse/serialize round-trip 可运行 | CTest 中 `mrtc_sdp_roundtrip_test` 退出 0 |
| V-03-06 | Answerer flow 可运行 | CTest 中 `mrtc_peer_connection_api_test` 远端 offer -> local answer 流程退出 0 |
| V-03-07 | Offerer API 已预留但不虚假承诺 | 测试断言 `mrtc_peer_connection_create_offer()` 返回 `MRTC_STATUS_NOT_IMPLEMENTED` |
| V-03-08 | 来源追溯已更新 | `SOURCE-MANIFEST.md` 包含 Phase 3 派生或 reference-only 记录 |

## 推荐验证命令

```bash
test -d reflib/kvs-webrtc-sdk
git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD

cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer

rg -n "PCHAR|PVOID|BOOL|(^|[^A-Z_])STATUS([^A-Z_]|$)" include/micrortc
rg -n "kvsWebrtcSignalingClient|src/source/Signaling|libwebsockets|AWSSDK|AWS SDK" CMakeLists.txt include src tests
rg -n "API-01|API-02|API-03|API-06|PROTO-06|PROTO-07|NET-05" .planning/phases/03-aws-api-signaling-free-peerconnection/*-PLAN.md
```

`rg` 禁止项命令期望无输出；如果裸 `STATUS` 检查误匹配 `MRTC_STATUS`，应改用更精确的正则，不得删除 `MRTC_STATUS`。

## 不属于本阶段的验收

- Chrome 自动化 E2E。
- ICE connected、STUN/TURN relay、DTLS/SRTP 初始化。
- DataChannel open/message。
- H264/Opus 媒体发送、接收或 transceiver/writeFrame API。

---
*Validation strategy created: 2026-05-12*
