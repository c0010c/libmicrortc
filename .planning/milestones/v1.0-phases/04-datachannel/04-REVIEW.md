---
phase: 04
title: Phase 4 DataChannel Code Review
status: clean
depth: standard
files_reviewed: 39
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
fixed_findings: 2
reviewed_at: "2026-05-12T23:52:23+08:00"
---

# Phase 4 代码审查

## 结论

当前 Phase 4 代码在复验后没有未解决的 Critical / Warning / Info 发现。由于 Codex runtime 只允许在用户明确请求 sub-agent 时自动 spawn，本次按 `$gsd-code-review` 的 standard 深度要求在当前 agent 内完成人工审查，并把修复纳入本轮收尾提交。

Phase 4 仍不是 complete：真实网络验收依赖开发者提供 `./mrtc-ice-servers.local.json`，当前仓库没有该本地文件，verifier 会按预期失败并阻止阶段完成。

## 已修复发现

### WR-04-001: SCTP session 初始化未清零 callback/state

- Severity: Warning
- File: `src/sctp/sctp_session.c`
- Status: Fixed

`mrtc_sctp_session_init()` 原先只写入部分字段。如果调用方传入栈上未初始化的 `MRTC_SCTP_SESSION`，后续 `connect/write` 路径可能观察到垃圾 callback 或 user_data。

修复：初始化入口先 `memset(session, 0, sizeof(*session))`，再进入全局 SCTP init 和字段赋值。

### WR-04-002: ICE config loader 会跨 server 对象读取 username/secret

- Severity: Warning
- Files:
  - `src/ice/ice_config.c`
  - `tests/transport/test_ice_config.c`
- Status: Fixed

JSON loader 原先在找到当前 `urls` 后，从后续整个 JSON 文本搜索 `username` 和 secret 字段。当前 server 只有 STUN URL、后一个 server 才有 TURN secret 时，第一个 server 可能错误继承后一个对象的认证信息。

修复：loader 先定位 `ice_servers` 数组中的单个 `{...}` 对象，再把 `urls`、`username` 和 secret 查找都限制在该对象边界内。测试调整为 STUN 在前、TURN 在后，验证 STUN server 不继承认证字段。

## 审查范围

覆盖 Phase 4 commits 中的构建、public API、private transport wrappers、PeerConnection wiring、测试和 package consumer 文件，排除 `.planning/` 执行产物本身。

重点检查：

- public header 是否暴露 AWS/PIC/signaling/storage 类型。
- PeerConnection 对 ICE config、SDP、DTLS/SRTP/SCTP/DataChannel 生命周期的状态转换。
- ICE config/STUN/TURN parser 的边界和失败行为。
- DTLS/SRTP/SCTP wrappers 在系统依赖缺失时的可构建占位行为，以及 strict dependency 开关。
- DataChannel open/send/close callback 语义。
- package install/export 是否能被独立 consumer 使用。

## 复验

已通过：

```bash
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
./build/tests/integration/mrtc_phase4_network_verify --config build/mrtc-verify-test.json --require-host --require-srflx --require-relay --require-dtls --require-srtp --require-datachannel
rg -n "add_subdirectory\(reflib|kvsWebrtcSignalingClient|src/source/Signaling|libwebsockets|AWSSDK|credential|storage" CMakeLists.txt include src tests
```

按预期失败并作为人工验收门保留：

```bash
./build/tests/integration/mrtc_phase4_network_verify --config ./mrtc-ice-servers.local.json --require-host --require-srflx --require-relay --require-dtls --require-srtp --require-datachannel
```

当前结果：退出码 `3`，原因是缺少 `./mrtc-ice-servers.local.json` 或其中没有可用 `ice_servers`。
