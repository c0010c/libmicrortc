---
phase: 06
slug: chrome-e2e
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-13
---

# Phase 06 — Validation Strategy

> Phase 6 的验证目标是区分“编译通过”“协议单元测试通过”“Chrome 互通通过”“TURN relay 真实进入验收”“双向 H264/Opus 媒体流动通过”。

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Node/Playwright |
| **Config file** | `CMakeLists.txt`, `tests/e2e/package.json`, `tests/e2e/playwright.config.js` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `scripts/verify-v1.sh` |
| **TURN suite command** | `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` |
| **Estimated runtime** | ~30-180 seconds without TURN; TURN depends on network |

## Sampling Rate

- **After every task commit:** Run the task-level command from the PLAN.
- **After every plan wave:** Run build + CTest and any host Chrome E2E command introduced by the wave.
- **Before `$gsd-verify-work`:** `scripts/verify-v1.sh` must pass; TURN command must pass when local TURN config is available and explicitly requested.
- **Max feedback latency:** deterministic C checks < 60s; host Chrome E2E < 180s.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 1 | E2E-01, E2E-02, E2E-03 | T-01-01 | signaling stays in demo/test layer | integration | `npm --prefix tests/e2e test -- --list` | W0 | pending |
| 06-01-02 | 01 | 1 | TEST-03, TEST-04 | T-01-02 | output stages are explicit | cli | `tests/e2e/run-host-e2e.js --help` | W0 | pending |
| 06-02-01 | 02 | 2 | E2E-01, E2E-07, E2E-08 | T-02-01 | C demo emits redacted structured events | integration | `./build/examples/chrome-e2e/mrtc_chrome_answerer --help` | W0 | pending |
| 06-02-02 | 02 | 2 | E2E-05, E2E-07, E2E-08 | T-02-02 | fixed media source only, no capture/encoder | e2e | host Chrome runner smoke | W0 | pending |
| 06-03-01 | 03 | 3 | E2E-04, E2E-05, E2E-06, E2E-07, E2E-08 | T-03-01 | browser stats use stable deltas | e2e | `npm --prefix tests/e2e run test:host` | W0 | pending |
| 06-04-01 | 04 | 4 | NET-04 | T-04-01 | missing TURN config hard-fails | e2e | `npm --prefix tests/e2e run test:turn -- --turn-config ./mrtc-ice-servers.local.json` | W0 | pending |
| 06-04-02 | 04 | 4 | NET-04, E2E-05, E2E-06, E2E-07, E2E-08 | T-04-02 | relay selected path is proven | e2e | relay-only Playwright spec | W0 | pending |
| 06-05-01 | 05 | 5 | TEST-01, TEST-02, TEST-03, TEST-04 | T-05-01 | v1 script reports stage failures | cli | `scripts/verify-v1.sh` | W0 | pending |

## Wave 0 Requirements

- `tests/e2e/package.json` — Playwright dependency and scripts.
- `tests/e2e/playwright.config.js` — Chrome/Chromium project configuration, CI workers=1.
- `tests/e2e/signaling-server.js` — local signaling and C stdio bridge.
- `examples/chrome-e2e/` — C demo and browser page directory.
- `scripts/verify-v1.sh` — final acceptance entrypoint.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Real TURN credential availability | NET-04 | Secrets must not be committed; local network conditions vary | Create local `./mrtc-ice-servers.local.json`, then run `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json`. |

## Validation Sign-Off

- [x] All planned tasks have automated verify commands or explicit local TURN setup.
- [x] Sampling continuity: no 3 consecutive tasks without automated verify.
- [x] Wave 0 covers all missing test infrastructure references.
- [x] No watch-mode flags.
- [x] Feedback latency target documented.
- [x] `nyquist_compliant: true` set in frontmatter.

**Approval:** pending

