---
phase: 7
slug: api
status: verified
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-15
updated: 2026-05-15
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
| 07-01-01 | 01 | 1 | API-01 | — | N/A | docs | `test -f docs/api-v1.1-boundary.md && rg -n 'public|private|test-only|include/micrortc|src/' docs/api-v1.1-boundary.md` | ✅ current | ✅ green |
| 07-01-02 | 01 | 1 | API-02 | — | N/A | docs/scan | `test -f docs/api-v1.1-symbol-map.md && rg -n 'mrtc_|MRTC_|rtc_|Rtc|RTC_' docs/api-v1.1-symbol-map.md` | ✅ current | ✅ green |
| 07-01-03 | 01 | 1 | API-04 | — | N/A | docs | `rg -n '不提供兼容|no compatibility|wrapper|删除' docs/api-v1.1-boundary.md docs/api-v1.1-symbol-map.md` | ✅ current | ✅ green |
| 07-02-01 | 02 | 2 | STYLE-04 | T-07-01/T-07-03 | Residual findings include explicit category and source reason | script/schema | `scripts/scan-api-residuals.sh --format json --output build/reports/api-residuals.json` plus JSON schema check | ✅ current | ✅ green |
| 07-02-02 | 02 | 2 | STYLE-04 | T-07-02 | Local TURN secrets are not printed by residual scans | script/security | `scripts/scan-api-residuals.sh --format json --output build/reports/api-residuals.json && ! rg -n 'raw-user|raw-credential|raw-password|username|credential|password' build/reports/api-residuals.json` | ✅ current | ✅ green |
| 07-02-03 | 02 | 2 | API-02/API-04 | T-07-03 | Current installed-header old public symbols are covered by the frozen symbol map | script/coverage | `scripts/scan-api-residuals.sh --check-symbol-map docs/api-v1.1-symbol-map.md --format json --output build/reports/api-residuals.json` | ✅ current | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] `docs/api-v1.1-boundary.md` — v1.1 public/private/test-only API boundary for API-01.
- [x] `docs/api-v1.1-symbol-map.md` — complete current public symbol old-to-new mapping for API-02/API-04.
- [x] `scripts/scan-api-residuals.sh` — categorized residual scan for STYLE-04.
- [x] `build/reports/api-residuals.json` schema expectation — machine-readable residual evidence for verification.

---

## Manual-Only Verifications

No mandatory manual-only verifications remain for Phase 7 Nyquist coverage. The semantic checks that were initially listed here are now covered by `07-VERIFICATION.md`, `07-REVIEW.md`, `07-SECURITY.md`, and the executable scanner/schema checks above.

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

**Approval:** verified 2026-05-15

## Validation Audit 2026-05-15

| Metric | Count |
|--------|-------|
| Coverage gaps found | 0 |
| Missing test files required | 0 |
| Resolved by existing automated evidence | 6 |
| Escalated to manual-only | 0 |

### Audit Evidence

- `07-01-PLAN.md` and `07-02-PLAN.md` map Phase 7 to API-01, API-02, API-04, and STYLE-04.
- `07-01-SUMMARY.md` and `07-02-SUMMARY.md` record completed artifacts and task-level verification.
- `07-VERIFICATION.md` records 8/8 must-haves verified, including scanner schema, symbol-map coverage, build, and CTest.
- Re-run on 2026-05-15 passed Phase 7-specific documentation assertions, scanner CLI checks, JSON schema checks, secret-field grep, symbol-map coverage, `cmake --build build -j2`, and `ctest --test-dir build --output-on-failure` with 18/18 tests passing.

### Notes

- `build/reports/api-residuals.json` may report `status: "failed"` while the scanner exits 0. This is the expected Phase 7 baseline because Phase 8 has not yet performed the mechanical public API rename; current old public names are intentionally classified as `forbidden_public_residual`.
- No new test files were generated during this audit because every Phase 7 requirement already had executable coverage. The audit only refreshed stale Wave 0/pending metadata in this validation strategy.
