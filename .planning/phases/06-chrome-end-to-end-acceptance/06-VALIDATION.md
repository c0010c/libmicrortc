---
phase: 6
slug: chrome-end-to-end-acceptance
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-11
---

# Phase 6 — Validation Strategy

> 第 6 阶段验证契约：自动化确认连接链路、状态、counter 和文件产物；媒体播放质量保留为人工 VLC 验收。

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Node/Playwright 编排 |
| **Config file** | `CMakeLists.txt`, `package.json`, `examples/chrome_e2e/run_e2e.mjs` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure && node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000` |
| **Estimated runtime** | ~60-120 seconds after Chrome dependencies are present |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **After E2E harness tasks:** Run the relevant smoke command, for example `node examples/chrome_e2e/run_e2e.mjs --dry-run`
- **After every plan wave:** Run `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite plus manual VLC UAT checklist must be complete
- **Max feedback latency:** 120 seconds for automated checks, excluding first-time browser install

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 06-01 | 1 | EXM-01, EXM-02 | T-06-01 | WebSocket 信令不承载媒体 payload，只转发 SDP/candidate/status | smoke | `node examples/chrome_e2e/run_e2e.mjs --dry-run` | ❌ W0 | ⬜ pending |
| 06-01-02 | 06-01 | 1 | EXM-01 | T-06-02 | 页面只使用本地合成媒体，不请求真实摄像头/麦克风权限 | browser | `node examples/chrome_e2e/run_e2e.mjs --page-smoke` | ❌ W0 | ⬜ pending |
| 06-02-01 | 06-02 | 2 | EXM-02, ACC-01 | T-06-03 | C 示例的 UDP/socket 只在示例层，核心库不新增 socket/thread | unit/smoke | `cmake --build build --target rtc_chrome_e2e` | ❌ W0 | ⬜ pending |
| 06-02-02 | 06-02 | 2 | ACC-01 | T-06-04 | JSONL error detail 不泄漏未认证媒体内容，summary 只输出诊断字段 | smoke | `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke` | ❌ W0 | ⬜ pending |
| 06-03-01 | 06-03 | 3 | ACC-01 | T-06-05 | 样本解析只在示例层，核心库不新增编解码器 | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0 | ⬜ pending |
| 06-03-02 | 06-03 | 3 | ACC-01 | T-06-06 | 接收媒体落盘失败进入 `media_file` failure layer | smoke | `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke` | ❌ W0 | ⬜ pending |
| 06-04-01 | 06-04 | 4 | ACC-01 | T-06-07 | DTLS/SRTP backend 依赖可选且许可证非 GPL/LGPL，默认构建不强制 | unit/config | `cmake -S . -B build && cmake --build build` | ❌ W0 | ⬜ pending |
| 06-04-02 | 06-04 | 4 | ACC-01 | T-06-08 | fingerprint mismatch、key export、SRTP protect/unprotect 失败仍分层可诊断 | unit | `ctest --test-dir build --output-on-failure` | ❌ W0 | ⬜ pending |
| 06-05-01 | 06-05 | 5 | TST-01, ACC-01 | T-06-09 | E2E 脚本失败层级限定为 signaling/ice/dtls/srtp/rtp/rtcp/media_file | e2e | `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000` | ❌ W0 | ⬜ pending |
| 06-06-01 | 06-06 | 6 | TST-01, EXM-01, EXM-02, ACC-01 | T-06-10 | 文档不宣称自动化已替代人工媒体播放验收 | docs | `grep -R "VLC" .planning/phases/06-chrome-end-to-end-acceptance docs examples -n` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `examples/chrome_e2e/` — Chrome 页面、信令服务、C 示例和运行脚本目录。
- [ ] `examples/chrome_e2e/run_e2e.mjs` — 至少支持 `--dry-run`，能检查端口、样本文件、构建产物路径和输出目录。
- [ ] `examples/chrome_e2e/README.md` — 中文说明本地运行、Chrome 合成媒体、C 示例 UDP/socket 边界、VLC 人工验收步骤。
- [ ] `CMakeLists.txt` — 新增 `rtc_chrome_e2e` 示例 target，不影响默认 `rtc_tests`。
- [ ] `package.json` — 增加 E2E 脚本和必要 dev dependencies，并记录非 GPL/LGPL 许可证。

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| VLC 播放 C 示例输出媒体文件 | ACC-01 | 首版不引入播放器/解码器自动化依赖，媒体内容质量由用户确认 | 运行 E2E 脚本后，用 VLC 打开输出目录中的接收音频/视频文件；确认可播放或记录失败层级和 JSONL summary |
| Chrome 页面状态面板视觉检查 | EXM-01, ACC-01 | 页面 compact 状态可读性需要人工确认 | 打开本地页面，确认 signaling、ICE/connection、tracks、candidate、stats 摘要可见且不遮挡媒体 |
| 可选真实安全 backend 依赖安装 | ACC-01 | 不同系统的 OpenSSL/libsrtp 或等价 backend 安装方式不同 | 按 README 安装可选依赖，重新运行 `cmake` 和 E2E 脚本 |

---

## Threat Model

| Threat | Severity | Mitigation |
|--------|----------|------------|
| 信令服务误承载媒体或代理 UDP，模糊用户负责 socket 边界 | high | 信令 JSON schema 只允许 SDP/candidate/status/summary；测试扫描禁止二进制媒体字段 |
| 示例层异步发送借用 `on_datagram` 回调 buffer，导致 use-after-return | high | C 示例必须复制 datagram 到自己的发送 buffer；代码 review 和 smoke test 覆盖 |
| 样本解析把编解码逻辑引入核心库 | high | 所有 Ogg/H264 sample parsing 文件位于 `examples/chrome_e2e/` 或 `tests/`，`src/` 不新增 codec/parser 依赖 |
| 可选 DTLS/SRTP backend 引入 GPL/LGPL 依赖或默认构建强制依赖 | high | dependency/license gate；默认 CMake 不要求外部安全 backend 开发包 |
| 自动化脚本把页面状态误判为真实媒体可播放 | medium | summary 区分 automated pass 与 manual VLC pending/approved |
| Chrome mDNS/local candidate 导致 ICE 失败不可诊断 | medium | JSONL 和页面状态必须记录 candidate 字符串、candidate count 和 failure layer |

---

## Validation Sign-Off

- [x] All tasks have planned `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency target < 120s after dependencies
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
