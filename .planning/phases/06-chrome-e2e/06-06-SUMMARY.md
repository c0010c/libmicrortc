---
phase: 06-chrome-e2e
plan: "06"
subsystem: testing
tags: [chrome-e2e, playwright, ctest, turn, summary, docs]
requires:
  - phase: 06-chrome-e2e
    provides: host Chrome/Chromium 双向媒体 E2E 和 TURN relay E2E
provides:
  - v1 总验收入口 scripts/verify-v1.sh
  - build/reports/mrtc-v1-summary.json 机器可读 summary schema
  - CTest/package consumer/README 测试矩阵收口
affects: [phase-06, v1-acceptance, chrome-e2e, turn, docs, requirements]
tech-stack:
  added: []
  patterns:
    - Bash 总验收脚本串起 CMake、CTest、Playwright host E2E 和显式 TURN E2E
    - Node summary helper 统一 JSON schema 并复用 TURN secret redaction
key-files:
  created:
    - scripts/verify-v1.sh
    - .planning/phases/06-chrome-e2e/06-06-SUMMARY.md
  modified:
    - tests/e2e/summary.js
    - tests/e2e/chrome-host.spec.js
    - tests/e2e/chrome-turn.spec.js
    - CMakeLists.txt
    - tests/package-consumer/package_consumer.c
    - README.md
    - .planning/ROADMAP.md
    - .planning/STATE.md
    - .planning/REQUIREMENTS.md
    - .planning/phases/01-/SOURCE-MANIFEST.md
key-decisions:
  - "06-06: v1 总验收默认使用 Playwright bundled Chromium；系统 Google Chrome 可用 --browser-channel chrome 显式选择。"
  - "06-06: TURN relay E2E 只在 --turn-config 显式传入时运行，缺配置必须非零失败。"
  - "06-06: build/reports/mrtc-v1-summary.json 是 canonical 机器可读 summary；JUnit 暂不新增。"
patterns-established:
  - "Stage banner: BUILD、CTEST、CHROME HOST E2E、CHROME TURN E2E、SUMMARY。"
  - "Summary schema: 每个验收层使用 status 对象，媒体层分 video/audio 计数和状态。"
requirements-completed: [NET-04, E2E-01, E2E-02, E2E-03, E2E-04, E2E-05, E2E-06, E2E-07, E2E-08, TEST-01, TEST-02, TEST-03, TEST-04]
duration: 31min
completed: 2026-05-14
---

# Phase 06 Plan 06: v1 总验收脚本、文档与测试收口 Summary

**v1 验收入口串起 CMake、CTest、host Chromium E2E 和显式 TURN relay E2E，并输出脱敏 JSON summary。**

## Performance

- **Duration:** 31 min
- **Started:** 2026-05-14T04:50:44Z
- **Completed:** 2026-05-14T04:59:51Z
- **Tasks:** 4
- **Files modified:** 11

## Accomplishments

- 新增 `scripts/verify-v1.sh`，支持 `--build-dir`、`--fixtures`、`--turn-config`、`--skip-npm-install`、`--browser-channel` 和 `--help`。
- 统一 `tests/e2e/summary.js` schema，最终 summary 写入 `build/reports/mrtc-v1-summary.json`，并复用 TURN redaction 防止 credential 泄露。
- CTest 增加 `protocol`、`transport`、`media`、`integration` 等 labels；package consumer 编译覆盖 DataChannel label/id 和 selected candidate diagnostic accessor。
- README、ROADMAP、STATE、REQUIREMENTS 和 SOURCE-MANIFEST 已按真实验证结果更新。

## Task Commits

1. **Task 06-06-01/02: v1 总验收入口与 summary schema** - `575c1c2` (feat)
2. **Task 06-06-03: CTest/package consumer 收口** - `ee6891b` (feat)
3. **Task 06-06-04: 文档、状态和来源追溯收口** - pending metadata commit

## Files Created/Modified

- `scripts/verify-v1.sh` - v1 总验收脚本。
- `tests/e2e/summary.js` - canonical summary schema、merge/finalize CLI、secret redaction 输出。
- `tests/e2e/chrome-host.spec.js` - 使用 `MRTC_E2E_FIXTURES`，写入对象化 status/counters。
- `tests/e2e/chrome-turn.spec.js` - TURN relay summary 使用对象化 status/counters。
- `CMakeLists.txt` - 为 CTest suites 添加 labels，保持 Chrome E2E 不进默认 CTest。
- `tests/package-consumer/package_consumer.c` - 编译覆盖 Phase 6 public diagnostic accessors。
- `README.md` - v1 验收矩阵、TURN opt-in、summary artifact 和 secret policy。
- `.planning/ROADMAP.md` - Phase 6 Plan 05/06 标记完成并补总验收约束。
- `.planning/STATE.md` - Phase 6/v1 状态更新为完成。
- `.planning/REQUIREMENTS.md` - NET-04、E2E-06、TEST-02 按验证证据标记完成。
- `.planning/phases/01-/SOURCE-MANIFEST.md` - 追加 Phase 6 demo/E2E/summary/verify 来源追溯。

## Decisions Made

- 默认 `scripts/verify-v1.sh` 使用 `chromium`，避免没有系统 `/opt/google/chrome/chrome` 的 Linux 环境阻塞；仍可用 `--browser-channel chrome` 强制系统 Chrome。
- TURN relay 只通过 `--turn-config` 进入总验收；不传参数时 summary 明确 `chrome_turn.status: "skipped"`。
- JSON summary 作为 canonical artifact；本计划未新增 JUnit XML，避免重复维护两套验收 schema。

## Verification

- `scripts/verify-v1.sh --help` - passed。
- `scripts/verify-v1.sh` - passed；build、CTest、host Chromium E2E 均通过，summary 写入 `build/reports/mrtc-v1-summary.json`。
- `scripts/verify-v1.sh --turn-config ./missing.json` - expected failed；在 `CHROME TURN E2E` 阶段因配置不存在非零退出。
- `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` - passed；TURN relay selected pair local/remote 均为 relay。
- `node -e "const s=require('./build/reports/mrtc-v1-summary.json'); ..."` - passed；验证 phase、build、ctest、chrome_host、chrome_turn、turn_relay schema/status。
- `rg -n "credential|password|username|token|secret|<credential>|<username>" build/reports/mrtc-v1-summary.json tests/e2e/artifacts/summary.json` - passed，无匹配。
- `cmake --install build --prefix build/install && rm -rf build/package-consumer && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer` - passed。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] 默认 browser channel 从 system Chrome 改为 bundled Chromium**
- **Found during:** Task 06-06-01/02
- **Issue:** 本机没有 `/opt/google/chrome/chrome`，默认 `chrome` channel 导致 `scripts/verify-v1.sh` 在 host E2E 阶段失败。
- **Fix:** `scripts/verify-v1.sh` 默认 `--browser-channel chromium`，并保留 `--browser-channel chrome` 给安装了系统 Chrome 的环境。
- **Files modified:** `scripts/verify-v1.sh`, `README.md`, `.planning/STATE.md`
- **Verification:** `scripts/verify-v1.sh` 和 `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` 均通过。
- **Committed in:** `575c1c2`

**Total deviations:** 1 auto-fixed (Rule 3)

## Issues Encountered

- `tests/e2e/summary.js` 初版 merge 后出现 `chrome_turn.status: "passed"` 但 `skipped: true` 的矛盾状态；已修正 normalize 逻辑并重新 finalize summary。

## Known Stubs

None.

## Threat Flags

None - 新增 surface 仅为本地验收脚本和测试 artifact；TURN secret redaction 已覆盖并验证。

## Self-Check: PASSED

- `scripts/verify-v1.sh` exists.
- `build/reports/mrtc-v1-summary.json` exists after verification.
- Commits `575c1c2` and `ee6891b` exist in git log.

## User Setup Required

真实 TURN relay 验收需要用户在根目录提供被 `.gitignore` 忽略的 `mrtc-ice-servers.local.json`。本次执行环境已存在该文件，且命令已通过；该文件未提交。

## Next Phase Readiness

Phase 6 的 v1 验收闭环已完成。后续可进入 `$gsd-verify-work` 或发起发布/PR 分支整理。

---
*Phase: 06-chrome-e2e*
*Completed: 2026-05-14*
