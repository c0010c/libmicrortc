---
status: complete
phase: 02-sdp-jsep-offer-answer
source:
  - 02-01-SUMMARY.md
  - 02-02-SUMMARY.md
  - 02-03-SUMMARY.md
  - 02-04-SUMMARY.md
started: "2026-05-10T19:48:41+08:00"
updated: "2026-05-10T19:51:20+08:00"
---

# 第 2 阶段 UAT：SDP/JSEP 与 Offer/Answer

## Current Test

[testing complete]

## Tests

### 1. 固定内存 SDP/JSEP 契约
expected: 集成者可以在 create-time config 中提供 SDP 参数、arena 和 limits；PeerConnection 创建阶段会切分本地/远端 SDP buffer 与远端 candidate 固定槽，容量不足时返回可诊断错误，运行期不动态增长。
result: pass

### 2. 本地 Offer/Answer SDP 生成
expected: `createOffer` 和 `createAnswer` 可以生成 Chrome 1v1 profile 的 SDP，包含 BUNDLE、rtcp-mux、trickle、ICE 参数、DTLS fingerprint/setup、Opus 111 和 H264 103；answer 使用 `a=setup:active`，生成侧不枚举本地网络、不输出 candidate。
result: pass

### 3. Chrome SDP 解析与 Profile 校验
expected: `setRemoteDescription` 可以解析 Chrome offer/answer fixture 中的 BUNDLE、audio/video mid、rtcp-mux、ICE、DTLS、方向、Opus 和 H264；缺失 BUNDLE、非法方向或不支持 profile 会返回稳定错误。
result: pass

### 4. Offer/Answer JSEP 状态机
expected: `createOffer`、`createAnswer`、`setLocalDescription` 和 `setRemoteDescription` 能推进 `stable`、`have-local-offer`、`have-remote-offer` 状态，并拒绝非法状态转换、协议错误和不支持方向/codec。
result: pass

### 5. 远端 ICE Candidate 保存边界
expected: `rtc_peer_connection_add_ice_candidate` 会把远端 candidate 复制进固定槽，受 `limits.ice.max_candidates` 和槽大小限制；调用不会启动 ICE checks、不会创建 socket、不会保存调用方 buffer 指针。
result: pass

### 6. 第 2 阶段构建、测试与中文 API 文档
expected: 从构建目录运行 `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 可以通过；`docs/API-执行器与内存契约.md` 用中文说明第 2 阶段已实现 SDP/JSEP API 和仍未实现的网络/ICE/DTLS/RTP 边界。
result: pass

## Summary

total: 6
passed: 6
issues: 0
pending: 0
skipped: 0
blocked: 0

## Evidence

- `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 通过，`rtc_tests` 1/1 passed。
- `rg -n "\bmalloc\b|\bcalloc\b|\brealloc\b" src include` 无输出，核心源码没有绕过固定内存边界。
- 检查 `include/rtc/config.h`、`include/rtc/limits.h`、`src/api/peer_connection.c` 和 `tests/test_peer_connection.c`：SDP 参数、SDP buffer、远端 candidate 固定槽和容量诊断存在。
- 检查 `src/sdp/sdp_writer.c`、`tests/test_sdp_writer.c`、`tests/fixtures/expected-local-offer.sdp`、`tests/fixtures/expected-local-answer.sdp`：BUNDLE、rtcp-mux、trickle、ICE 参数、DTLS fingerprint/setup、Opus 111、H264 103、`a=setup:active` 和“不输出 candidate”均有覆盖。
- 检查 `src/sdp/sdp_parser.c`、`tests/test_sdp_parser.c`、Chrome offer/answer fixtures 和 invalid fixtures：Chrome profile 成功路径、缺失 BUNDLE/ICE/fingerprint、未知 codec、`sendonly` 等错误路径均有覆盖。
- 检查 `src/jsep/jsep.c`、`src/api/peer_connection.c`、`tests/test_jsep.c` 和文档：`stable`、`have-local-offer`、`have-remote-offer` 状态流、非法状态与错误码路径有覆盖。
- 检查 `rtc_peer_connection_add_ice_candidate`、`tests/test_jsep.c` 和 `docs/API-执行器与内存契约.md`：远端 candidate 复制保存、容量超限、不保存调用方 buffer、不隐式启动 checks/socket 的边界有覆盖。
- 检查 `docs/API-执行器与内存契约.md`：第 2 阶段 signaling API、Chrome SDP/JSEP profile、错误语义和网络/ICE/DTLS/RTP 非本阶段边界已用中文说明。

## Gaps

[none yet]
