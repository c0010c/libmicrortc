---
phase: 4
slug: dtls-srtp-secure-transport
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-10
---

# Phase 4 — Validation Strategy

> 第 4 阶段执行期间的验证采样契约，覆盖 DTLS-SRTP 安全 backend、fingerprint 校验、key export、SRTP/SRTCP protect/unprotect 和错误可观测性。

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | 纯 C 自研 test runner + CTest |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~1 秒 |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **After every plan wave:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 1 秒

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 1 | SEC-01 | T-04-01 | `security_backend` vtable 定义 backend event/callback 契约，包含 outgoing datagram 和 handshake-complete 信号 | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-02-01 | 02 | 2 | SEC-01 | T-04-01 | runtime 在固定 storage 内创建 DTLS session，ICE connected 后自动启动 DTLS，早到 DTLS 被拒绝 | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-02-02 | 02 | 2 | SEC-01 | T-04-01 | backend `RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM` 输出的 bytes 通过既有 `observer.on_datagram` 发送 | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-03-01 | 03 | 3 | SEC-02 | T-04-02 | remote SDP fingerprint 与 DTLS peer cert fingerprint 不一致时进入 `dtls.failed`，不执行 key export | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-04-01 | 04 | 4 | SEC-03 | T-04-03 | `RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE` 触发 fingerprint verification，随后使用 `EXTRACTOR-dtls_srtp` 导出 key material，初始化单一 BUNDLE SRTP/SRTCP context，并进入 `srtp.ready` | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-04-02 | 04 | 4 | SEC-04 | T-04-04 | SRTP/SRTCP protect 失败不得调用 `observer.on_datagram` 输出未保护包；unprotect 失败不得向媒体层输出包 | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-05-01 | 05 | 5 | SEC-05 | T-04-05 | backend error 映射为稳定 `rtc_status_t`、observer error、trace reason 和 counter | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | ❌ W0: `tests/test_security.c` | ⬜ pending |
| 04-06-01 | 06 | 6 | SEC-01..SEC-05 | T-04-19..T-04-21 | 文档、需求追踪和项目状态准确表达 Phase 4 安全边界，不宣称真实 Chrome DTLS E2E 已完成 | docs/traceability | `cmake --build build && ctest --test-dir build --output-on-failure` | ✅ 既有文档 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

`wave_0_complete: false` 表示这些文件不是预先完成的 gate，而是已在 `04-01-PLAN.md` 和 `04-02-PLAN.md` 中规划创建或接入。执行器不应把 Wave 0 当作执行前置人工步骤；完成 04-01/04-02 后再把相关采样项标记为 green。

- [ ] `include/rtc/security.h` — 定义 single `security_backend` vtable/config 类型。
- [ ] `src/security/` — 安全状态机、DTLS role helper、fingerprint 校验、key export 和 SRTP/SRTCP wrapper。
- [ ] `tests/test_security.c` — deterministic backend matrix，覆盖 SEC-01 到 SEC-05。
- [ ] `include/rtc/counters.h` / `include/rtc/trace.h` — 增加 DTLS/SRTP counters、trace event/reason。

---

## Threat Model References

| Ref | Pattern | STRIDE | Required Mitigation |
|-----|---------|--------|---------------------|
| T-04-01 | DTLS datagram 在 ICE connected 前进入安全层 | Spoofing | `receive_datagram` 必须拒绝早到 DTLS，返回 `RTC_STATUS_INVALID_STATE` 并输出 trace/error |
| T-04-02 | SDP fingerprint mismatch / MITM | Spoofing | mismatch 硬失败，禁止 key export 和 `srtp.ready` |
| T-04-03 | key material label、长度、方向或 SRTP/SRTCP context init 错误 | Tampering | 使用 `EXTRACTOR-dtls_srtp`，用 deterministic backend 测 client/server 方向；`init_srtp_context` 失败时 reason `srtp_init_failed` 且不得进入 `srtp.ready` |
| T-04-04 | protect 失败后发送明文 RTP/RTCP | Information Disclosure | protect 失败不得调用 `observer.on_datagram` |
| T-04-05 | backend 细节泄漏到 public status | Reliability | public status 保持粗粒度，细节进入 observer detail、trace reason 和 counter |

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 真实 OpenSSL/libsrtp 参考 backend | SEC-01, SEC-03, SEC-04 | 第 4 阶段不要求真实第三方库默认构建通过，本地 `pkg-config` 未验证开发包可用 | 确认默认构建不依赖 OpenSSL/libsrtp；如新增可选骨架，文档必须说明固定 storage 契约 |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or planned Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 requirements are planned in 04-01/04-02; not complete before execution
- [x] No watch-mode flags
- [x] Feedback latency < 1s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
