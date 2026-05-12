---
phase: 01
slug: baseline-scope-compliance
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-12T18:58:55+08:00
---

# Phase 01 — Validation Strategy

> Phase 1 是文档/审计类阶段；验证重点是本地基线可复现、三份文档存在、关键字段和排除边界可被命令断言。

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | shell + `git` + `rg` |
| **Config file** | none |
| **Quick run command** | `test -f .planning/phases/01-/BASELINE.md && test -f .planning/phases/01-/SOURCE-MANIFEST.md && test -f .planning/phases/01-/COMPLIANCE.md` |
| **Full suite command** | `git -C reflib/kvs-webrtc-sdk status --short && git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD && rg -n "reflib/kvs-webrtc-sdk|9eebcc4|v1.18.1|Signaling|excluded|Apache-2.0|NOTICE|source_path|target_path|origin_commit|derivation|notes" .planning/phases/01-/` |
| **Estimated runtime** | ~5 seconds |

## Sampling Rate

- **After every task commit:** Run the quick run command.
- **After every plan wave:** Run the full suite command.
- **Before `$gsd-verify-work`:** Full suite must be green.
- **Max feedback latency:** 5 seconds.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | BASE-01, BASE-02 | T-01-01 | 本地基线事实不被远端状态替代 | command/doc | `git -C reflib/kvs-webrtc-sdk status --short && git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD && rg -n "reflib/kvs-webrtc-sdk|9eebcc4|v1.18.1" .planning/phases/01-/BASELINE.md` | ✅ | ⬜ pending |
| 01-01-02 | 01 | 1 | BASE-04 | T-01-02 | 后续派生文件具备来源追溯字段 | doc | `rg -n "source_path|target_path|origin_commit|derivation|notes" .planning/phases/01-/SOURCE-MANIFEST.md` | ✅ | ⬜ pending |
| 01-01-03 | 01 | 1 | BASE-03 | T-01-03 | 许可证、NOTICE 和第三方依赖路径明确 | doc | `rg -n "Apache-2.0|NOTICE|OpenSSL|libsrtp|usrsctp|libwebsockets|jsmn|kvsCommonLws" .planning/phases/01-/COMPLIANCE.md` | ✅ | ⬜ pending |
| 01-01-04 | 01 | 1 | BASE-01, BASE-04 | T-01-02 | AWS/KVS signaling 被明确排除出核心库 | doc | `rg -n "Signaling|excluded|core|supporting/reference-only" .planning/phases/01-/BASELINE.md .planning/phases/01-/SOURCE-MANIFEST.md` | ✅ | ⬜ pending |

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No test framework installation is required.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| NOTICE 派生说明措辞最终确认 | BASE-03 | 合规措辞可能需要项目维护者按发布策略审阅 | 阅读 `.planning/phases/01-/COMPLIANCE.md` 中 NOTICE 策略，确认是否足以指导后续根目录/发布包落地 |

## Validation Sign-Off

- [x] All tasks have automated verify commands or document-grep assertions.
- [x] Sampling continuity: no 3 consecutive tasks without automated verify.
- [x] Wave 0 covers all missing references.
- [x] No watch-mode flags.
- [x] Feedback latency < 5s.
- [x] `nyquist_compliant: true` set in frontmatter.

**Approval:** pending
