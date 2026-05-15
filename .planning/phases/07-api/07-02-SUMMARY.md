---
phase: 07-api
plan: 02
subsystem: api
tags: [api-contract, residual-scan, symbol-map, json-report, bash]

requires:
  - phase: 07-api
    provides: v1.1 public/private/test-only API boundary and old-to-new public symbol map
provides:
  - categorized old API residual scanner
  - machine-readable residual JSON report schema
  - public symbol-map coverage check for installed headers
  - secret-safe finding output for local TURN-related names
affects: [phase-08-public-headers, phase-09-consumer-migration, phase-10-private-boundary, phase-11-docs-release]

tech-stack:
  added: []
  patterns: [Bash CLI wrapper, rg JSON scanning, Node JSON report formatting, finite residual categories]

key-files:
  created:
    - scripts/scan-api-residuals.sh
    - build/reports/api-residuals.json
  modified: []

key-decisions:
  - "Residual report status may be failed while the scanner exits 0 when current public header old-name residuals are successfully reported."
  - "Symbol-map coverage checks installed public typedef/function/macro/enum names, but does not require opaque struct tags that are not mapped as public typedef symbols."
  - "Finding symbols are redacted when the token itself contains sensitive field words, so reports do not leak TURN-related local field names or values."

patterns-established:
  - "Residual scanner emits only path, line, symbol, category, and reason for findings; it never prints matched source lines."
  - "Report summary includes all five STYLE-04 categories even when a category has zero findings."
  - "Generated residual evidence stays in build/reports and is not tracked by git."

requirements-completed: [API-02, API-04, STYLE-04]

duration: 6min
completed: 2026-05-15
---

# Phase 07 Plan 02: Residual Scan Summary

**分类 residual scan 入口已落地，可生成不泄密 JSON evidence，并校验当前 installed public headers 的旧名都被 symbol map 覆盖。**

## Performance

- **Duration:** 6 min
- **Started:** 2026-05-15T11:03:56Z
- **Completed:** 2026-05-15T11:09:38Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- 创建 `scripts/scan-api-residuals.sh`，支持 `--format json`、`--output`、`--check-symbol-map` 和 `--help`。
- 扫描 `mrtc_*`、`MRTC_*`、`AWS`、`KVS` residual，并输出有限分类：`forbidden_public_residual`、`migration_doc_whitelist`、`source_compliance_record`、`test_harness_non_public_api`、`ignored_generated_artifact`。
- 生成 `build/reports/api-residuals.json` 本地证据，报告包含 `phase`、`status`、`summary`、`findings`、`failure_reason`、`duration_ms`，且末尾有换行。
- 实现 `--check-symbol-map docs/api-v1.1-symbol-map.md`，确认当前 `include/micrortc/*.h` 中旧 public symbols 都有对应新名映射。
- 报告不输出匹配行全文，并对 finding symbol 中的敏感字段词做脱敏；本地 TURN 配置和生成目录保持排除。

## Task Commits

Each task was committed atomically:

1. **Task 1: 创建安全的残留扫描脚本** - `2274a3d` (`feat`)
2. **Task 2: 生成报告并校验 symbol map 覆盖** - `d777df3` (`fix`)

Final metadata commit contains this SUMMARY plus state/roadmap/requirements updates.

## Files Created/Modified

- `scripts/scan-api-residuals.sh` - Bash residual scan 入口，负责参数校验、路径归一化、分类扫描、JSON 报告生成和 symbol-map 覆盖检查。
- `build/reports/api-residuals.json` - 生成的本地验证证据，已确认未加入 git 跟踪。

## Decisions Made

- 扫描成功生成报告时退出 0；当前 Phase 7 仍有 public header 旧名，所以报告 `status` 为 `failed` 是预期的 baseline 证据，不表示脚本执行失败。
- `--check-symbol-map` 只要求 installed public headers 中实际 public 旧名有映射，不把 opaque `struct MRTC_*` tag 误升为必须映射的 public typedef symbol。
- 报告对 symbol 字段中的敏感字段词做脱敏，避免严格 grep 检查和后续日志系统记录本地 TURN 相关字段名。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 修正 opaque struct tag 被误判为 public mapping 缺失**
- **Found during:** Task 2 (生成报告并校验 symbol map 覆盖)
- **Issue:** 初版 `--check-symbol-map` 从 header token 中枚举到 `struct MRTC_PEER_CONNECTION`、`struct MRTC_DATA_CHANNEL`、`struct MRTC_RTP_TRANSCEIVER` tag，错误要求 symbol map 为这些非 typedef public names 建立映射。
- **Fix:** 枚举 public old symbols 时跳过紧跟在 `struct ` 后的 opaque tag，只检查已映射的 typedef、enum、macro 和 function names。
- **Files modified:** `scripts/scan-api-residuals.sh`
- **Verification:** `scripts/scan-api-residuals.sh --check-symbol-map docs/api-v1.1-symbol-map.md --format json --output build/reports/api-residuals.json`
- **Committed in:** `d777df3`

**2. [Rule 2 - Missing Critical] 对 finding symbol 中的敏感字段词做脱敏**
- **Found during:** Task 2 (生成报告并校验 symbol map 覆盖)
- **Issue:** 报告没有输出 secret 值或匹配行，但部分内部 residual symbol 名包含敏感字段词，无法通过计划要求的严格报告 grep。
- **Fix:** 写入 JSON 前对 `raw-user`、`raw-credential`、`raw-password` 以及对应字段词做 symbol-level redaction。
- **Files modified:** `scripts/scan-api-residuals.sh`
- **Verification:** `! rg -n 'raw-user|raw-credential|raw-password|username|credential|password' build/reports/api-residuals.json`
- **Committed in:** `d777df3`

---

**Total deviations:** 2 auto-fixed (1 bug, 1 missing critical security hardening)
**Impact on plan:** 两个修正都直接服务于 Task 2 的正确性和 T-07-02/T-07-03 mitigation；未扩大计划范围。

## Issues Encountered

- `build/reports/api-residuals.json` 当前包含 `forbidden_public_residual`，这是 Phase 8 机械改名前的预期 baseline，不是脚本失败。

## Known Stubs

None. `SYMBOL_MAP_PATH=""` 是可选 CLI 参数的空默认值，不流向 UI 或未完成数据源。

## Auth Gates

None.

## Threat Flags

None - 本计划新增的文件读取和报告写入面已在 07-02 PLAN threat model 中覆盖，未引入额外网络端点、认证路径、schema 或新的信任边界。

## Verification

- `test -x scripts/scan-api-residuals.sh && scripts/scan-api-residuals.sh --help >/tmp/mrtc-scan-help.txt && rg -n -- '--format|--output|--check-symbol-map' /tmp/mrtc-scan-help.txt`
- `! rg -n 'eval ' scripts/scan-api-residuals.sh`
- `scripts/scan-api-residuals.sh --unknown-option >/tmp/mrtc-scan-stdout.txt 2>/tmp/mrtc-scan-stderr.txt; test "$?" -eq 2`
- `scripts/scan-api-residuals.sh --format json --output build/reports/api-residuals.json`
- Node schema check confirmed top-level fields, finding fields, pretty JSON newline, and at least one current `forbidden_public_residual`.
- `! rg -n 'raw-user|raw-credential|raw-password|username|credential|password' build/reports/api-residuals.json`
- `scripts/scan-api-residuals.sh --check-symbol-map docs/api-v1.1-symbol-map.md --format json --output build/reports/api-residuals.json`
- `test -z "$(git ls-files build/reports/api-residuals.json)"`

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 8 can consume `docs/api-v1.1-symbol-map.md` and use `scripts/scan-api-residuals.sh --check-symbol-map docs/api-v1.1-symbol-map.md --format json --output build/reports/api-residuals.json` as the public-header old-name deletion gate. Current report deliberately exposes remaining public old-name residuals before Phase 8 performs the mechanical rename.

## Self-Check: PASSED

- Found `scripts/scan-api-residuals.sh`.
- Found `.planning/phases/07-api/07-02-SUMMARY.md`.
- Found generated local evidence `build/reports/api-residuals.json`.
- Found task commit `2274a3d`.
- Found task commit `d777df3`.

---
*Phase: 07-api*
*Completed: 2026-05-15*
