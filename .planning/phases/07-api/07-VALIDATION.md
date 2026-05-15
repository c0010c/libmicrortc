---
phase: 7
slug: api
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-15
---

# Phase 7 — Validation Strategy

> Phase 7 validates the v1.1 API contract artifacts before later phases consume
> them for mechanical API migration.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest 3.22.1 for existing C regression tests; Bash for API residual classification |
| **Config file** | `CMakeLists.txt`, `cmake/MicroRtcTests.cmake`, `.clang-tidy` |
| **Quick run command** | `scripts/scan-api-residuals.sh` |
| **Full suite command** | `scripts/verify-v1.sh` |
| **Estimated runtime** | ~30 seconds for residual scan; v1 verification runtime depends on Chrome/TURN environment |

---

## Sampling Rate

- **After every task commit:** Run the task-specific documentation assertion and, once created, `scripts/scan-api-residuals.sh`.
- **After every plan wave:** Run all Phase 7 artifact checks plus `ctest --test-dir build --output-on-failure` when scripts or CMake/test behavior changed.
- **Before `$gsd-verify-work`:** `scripts/scan-api-residuals.sh` must pass and `scripts/verify-v1.sh` must remain available as the v1 regression entrypoint.
- **Max feedback latency:** 60 seconds for Phase 7-specific checks.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 07-01-01 | 01 | 1 | API-01 | — | N/A | docs | `test -f docs/api-v1.1-boundary.md && rg -n 'public|private|test-only|include/micrortc|src/' docs/api-v1.1-boundary.md` | ❌ W0 | ⬜ pending |
| 07-01-02 | 01 | 1 | API-02 | — | N/A | docs/scan | `test -f docs/api-v1.1-symbol-map.md && rg -n 'mrtc_|MRTC_|rtc_|Rtc|RTC_' docs/api-v1.1-symbol-map.md` | ❌ W0 | ⬜ pending |
| 07-01-03 | 01 | 1 | API-04 | — | N/A | docs | `rg -n '不提供兼容|no compatibility|wrapper|删除' docs/api-v1.1-boundary.md docs/api-v1.1-symbol-map.md` | ❌ W0 | ⬜ pending |
| 07-02-01 | 02 | 2 | STYLE-04 | T-07-01 | Residual findings include explicit category and source reason | script | `scripts/scan-api-residuals.sh --format json --output build/reports/api-residuals.json` | ❌ W0 | ⬜ pending |
| 07-02-02 | 02 | 2 | STYLE-04 | T-07-02 | Local TURN secrets are not printed by residual scans | script | `scripts/scan-api-residuals.sh --format json --output build/reports/api-residuals.json && ! rg -n 'mrtc-ice-servers.local.json.*password|credential' build/reports/api-residuals.json` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `docs/api-v1.1-boundary.md` — v1.1 public/private/test-only API boundary for API-01.
- [ ] `docs/api-v1.1-symbol-map.md` — complete current public symbol old-to-new mapping for API-02/API-04.
- [ ] `scripts/scan-api-residuals.sh` — categorized residual scan for STYLE-04.
- [ ] `build/reports/api-residuals.json` schema expectation — machine-readable residual evidence for verification.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Boundary classifications are semantically correct | API-01 | A script can check presence, but humans must confirm whether each API is truly public/private/test-only | Review `docs/api-v1.1-boundary.md` against `include/micrortc/*.h`, CMake install rules, and white-box test includes |
| New symbol names form a stable public contract | API-02/API-04 | Naming choices affect downstream migration ergonomics and should be reviewed before Phase 8 consumes them | Review every row in `docs/api-v1.1-symbol-map.md`; confirm new names use `rtc_*`, `Rtc*`, `RTC_*` and no compatibility wrapper is planned |

---

## Threat Model Hooks

| Threat Ref | Threat | Mitigation Expected In Plans |
|------------|--------|------------------------------|
| T-07-01 | Shell option/path injection in residual scan script | Quote shell variables, reject unknown options, avoid `eval`, and keep output paths repo-relative |
| T-07-02 | Secret leakage from local TURN config or generated reports | Do not print `mrtc-ice-servers.local.json` contents or credential fields in residual reports |
| T-07-03 | False compliance from over-broad whitelist categories | Residual report must include category, matched path, matched symbol, and whitelist reason |

---

## Validation Sign-Off

- [x] All planned artifacts have an automated or manual verification path.
- [x] Sampling continuity: no 3 consecutive tasks without automated verify.
- [x] Wave 0 covers all missing artifact references.
- [x] No watch-mode flags.
- [x] Feedback latency target < 60s for Phase 7-specific checks.
- [x] `nyquist_compliant: true` set in frontmatter.

**Approval:** pending
