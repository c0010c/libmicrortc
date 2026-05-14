---
phase: 03-aws-api-signaling-free-peerconnection
plan: "01"
subsystem: api
tags: [peer-connection, sdp, signaling-free, c-api, ctest]
requires:
  - phase: 02-
    provides: standalone micrortc static library target and package consumer path
provides:
  - AWS 风格裁剪后的 MRTC PeerConnection public API
  - signaling-free private SDP parse/serialize/answer helper
  - opaque PeerConnection Answerer 基础流程
  - CTest 覆盖和 install-tree package consumer 覆盖
  - Phase 3 来源追溯记录
affects: [phase-04-transport, phase-05-media, phase-06-e2e]
key-files:
  created:
    - include/micrortc/peer_connection.h
    - src/sdp.h
    - src/sdp.c
    - src/peer_connection.c
    - tests/fixtures/minimal_offer.sdp
    - tests/sdp/test_sdp_roundtrip.c
    - tests/peer_connection/test_peer_connection_api.c
  modified:
    - include/micrortc/micrortc.h
    - CMakeLists.txt
    - tests/package-consumer/package_consumer.c
    - README.md
    - .planning/phases/01-/SOURCE-MANIFEST.md
requirements-completed: [API-01, API-02, API-03, API-06, PROTO-06, PROTO-07, NET-05]
completed: 2026-05-12
---

# Phase 3 Plan 01: AWS 风格 API 与 signaling-free PeerConnection Summary

## Accomplishments

- 新增 `include/micrortc/peer_connection.h`，提供 `MRTC_PEER_CONNECTION_HANDLE`、config、callbacks、PeerConnection lifecycle、SDP offer/answer 和 ICE candidate 字符串 API。
- 扩展 `MRTC_STATUS`，新增 `MRTC_STATUS_NOT_IMPLEMENTED`、`MRTC_STATUS_INVALID_STATE` 和 `MRTC_STATUS_PARSE_ERROR`。
- 新增 private `src/sdp.h` / `src/sdp.c`，支持最小 SDP parse、serialize 和 remote offer -> local answer helper，不依赖 AWS signaling。
- 新增 `src/peer_connection.c`，实现 opaque handle、callback/user data 存储、remote/local description 状态机、Answerer 流程和 candidate 字符串边界。
- `mrtc_peer_connection_create_offer()` 已预留并明确返回 `MRTC_STATUS_NOT_IMPLEMENTED`，避免虚假承诺 Offerer 已完成。
- 扩展 CTest 和 package consumer，覆盖 SDP round-trip、Answerer API、安装后 public header 消费。
- 更新 README 中文说明和 Phase 1 `SOURCE-MANIFEST.md`，记录 Phase 3 rewritten-derived / reference-only 来源。

## Task Commits

1. **Task 2: Define public PeerConnection API contract** - `605f4aa`
2. **Task 3: Implement private SDP parse/serialize and answer helper** - `59bb221`
3. **Task 4: Implement signaling-free PeerConnection Answerer flow** - `418b639`
4. **Task 5: Run full verification and write Phase 3 summary** - metadata commit

Task 1 的本地基线检查已通过：`reflib/kvs-webrtc-sdk` 存在、HEAD 为 `9eebcc4`、参考库工作树干净。

## Source Attribution

- `include/micrortc/peer_connection.h`：rewritten-derived，参考 `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` 的 PeerConnection/SDP/candidate 使用模型，改为 MRTC 命名且不暴露 AWS/PIC 类型。
- `src/sdp.h` / `src/sdp.c`：rewritten-derived，参考 `src/source/Sdp/Sdp.h` 和 `Deserialize.c` 的逐行 SDP 模型，重写为 Phase 3 最小 private helper。
- `src/peer_connection.c`：rewritten-derived，参考 `src/source/PeerConnection/PeerConnection.c` 的 create/free、callback/user data 和 candidate 边界，重写为 signaling-free 状态机。
- `tests/fixtures/minimal_offer.sdp`：rewritten-derived，参考本地基线测试/样例 SDP 形态后重写为最小 offer fixture。
- `SessionDescription.c` 和 `Serialize.c` 仅作为 reference-only 行为参考，没有复制 JSON signaling wrapper 或完整 AWS 序列化实现。

## Deviations from Plan

None - plan executed within the Phase 3 boundary.

## Issues Encountered

- `gsd-sdk` 当前环境不可用，因此执行器按 `execute-phase` 的降级路径手动完成计划发现、任务执行、验证和 artifact 更新。
- 初次 Task 2 提交曾带入已暂存的 `.idea/` 文件；已立即 amend，把 IDE 元数据从提交中移除并保留在工作区，避免混入 Phase 3 代码提交。

## Verification

Full validation passed:

```bash
test -d reflib/kvs-webrtc-sdk
test "$(git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD)" = "9eebcc4"
test -z "$(git -C reflib/kvs-webrtc-sdk status --short)"
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
rg -n "mrtc_peer_connection_create|mrtc_peer_connection_create_answer|mrtc_peer_connection_add_ice_candidate|MRTC_STATUS_NOT_IMPLEMENTED" README.md
```

Boundary verification passed with no output:

```bash
rg -n "PCHAR|PVOID|BOOL|(^|[^A-Z_])STATUS([^A-Z_]|$)" include/micrortc
rg -n "kvsWebrtcSignalingClient|src/source/Signaling|libwebsockets|AWSSDK|credential|storage" CMakeLists.txt include src tests
```

## User Setup Required

None. The local `reflib/kvs-webrtc-sdk` baseline is present at commit `9eebcc4`.

## Next Phase Readiness

Phase 4 can attach ICE/STUN/TURN、DTLS、SRTP 和 DataChannel protocol modules behind the Phase 3 PeerConnection API. The core library remains signaling-free, and application/demo signaling stays outside `micrortc`.

## Self-Check: PASSED

- API-01, API-02, API-03, API-06, PROTO-06, PROTO-07, and NET-05 are represented in deliverables and tests.
- Public headers expose PeerConnection lifecycle, SDP, ICE candidate, config and callback API without AWS/PIC public types.
- Answerer remote offer -> local answer flow passes CTest.
- Offerer API exists and returns `MRTC_STATUS_NOT_IMPLEMENTED`.
- Core target does not include or link KVS signaling, libwebsockets, AWS SDK C++, credential/storage, DTLS/SRTP/DataChannel/RTP/media modules.
- Source manifest records Phase 3 derived/reference entries from local baseline commit `9eebcc4`.

---
*Phase: 03-aws-api-signaling-free-peerconnection*
*Completed: 2026-05-12*
