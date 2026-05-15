---
phase: 07-api
plan: 01
subsystem: api
tags: [api-contract, public-headers, symbol-map, migration, residual-scan]

requires:
  - phase: v1.0
    provides: shipped libmicrortc v1 public headers, package target, and regression baseline
provides:
  - v1.1 public/private/test-only API boundary contract
  - complete installed public header old-to-new symbol map
  - no-wrapper/no-alias deletion policy for old public names
  - residual classification contract for Phase 7 Wave 2
affects: [phase-08-public-headers, phase-09-consumer-migration, phase-10-private-boundary, phase-11-docs-release]

tech-stack:
  added: []
  patterns: [developer-facing Markdown API contract, field-definition mapping table, residual classification taxonomy]

key-files:
  created:
    - docs/api-v1.1-boundary.md
    - docs/api-v1.1-symbol-map.md
  modified: []

key-decisions:
  - "v1.1 public API facts are sourced from installed include/micrortc/*.h and package target micrortc::micrortc."
  - "Old mrtc_*/MRTC_*/AWS/KVS public names are rename/delete targets with no compatibility wrapper, macro alias, or typedef alias."
  - "Residual scan findings must distinguish forbidden public residuals from migration docs, source compliance records, test harness residuals, and generated artifacts."

patterns-established:
  - "Boundary contract: public API is installed include/micrortc/*.h; src/ and WITH_PRIVATE_INCLUDES remain private/test-only."
  - "Symbol map: each public old symbol has a concrete v1.1 name, category, action, and source."

requirements-completed: [API-01, API-02, API-04, STYLE-04]

duration: 5min
completed: 2026-05-15
---

# Phase 07 Plan 01: API 边界与 Symbol Map Summary

**v1.1 public API 边界、旧名删除策略和 installed header 完整 symbol rename map 已冻结为 Phase 8+ 可消费的文档契约。**

## Performance

- **Duration:** 5 min
- **Started:** 2026-05-15T10:54:32Z
- **Completed:** 2026-05-15T10:59:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- 创建 `docs/api-v1.1-boundary.md`，明确 public、private、test-only/demo-only API 边界。
- 创建 `docs/api-v1.1-symbol-map.md`，覆盖 `include/micrortc/micrortc.h` 和 `include/micrortc/peer_connection.h` 中当前 public symbols 的旧名到新名映射。
- 固定 API-04 删除策略：旧 `mrtc_*`、`MRTC_*`、AWS/KVS public names 默认删除，不提供 compatibility wrapper、macro alias 或 typedef alias。
- 固定 STYLE-04 残留分类：`forbidden_public_residual`、`migration_doc_whitelist`、`source_compliance_record`、`test_harness_non_public_api`、`ignored_generated_artifact`。

## Task Commits

Each task was committed atomically:

1. **Task 1: 编写 public/private/test-only API 边界文档** - `6ba4ba2` (`docs`)
2. **Task 2: 编写完整 public symbol 旧名到新名映射** - `83a5109` (`docs`)

Final metadata commit contains this SUMMARY plus state/roadmap/requirements updates.

## Files Created/Modified

- `docs/api-v1.1-boundary.md` - v1.1 public/private/test-only API 边界、删除策略、残留分类契约和 Phase 8+ 使用说明。
- `docs/api-v1.1-symbol-map.md` - installed `include/micrortc/*.h` public symbols 的完整旧名到新名映射，包含字段定义、允许分类、enum constants 展开表和 no-wrapper/no-alias action。

## Decisions Made

- Public API source 固定为 installed `include/micrortc/*.h` 和 CMake package target `micrortc::micrortc`。
- `src/`、`WITH_PRIVATE_INCLUDES`、Chrome answerer private include 和 white-box tests 不提升为 public API。
- Symbol map 只覆盖 installed public headers，不把 private helpers 写成 public migration targets。
- Wave 2 residual scan 必须输出 category、path、line、symbol、reason，且不能读取或打印 `mrtc-ice-servers.local.json` secret 内容。

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## Known Stubs

None.

## Auth Gates

None.

## Threat Flags

None - this plan added documentation only and did not introduce network endpoints, auth paths, file access behavior, or schema trust boundaries.

## Verification

- `test -f docs/api-v1.1-boundary.md` plus required boundary headings, classification values, `include/micrortc`, `src/`, `WITH_PRIVATE_INCLUDES`, and `micrortc::micrortc` checks passed.
- `rg -n 'compatibility wrapper|macro alias|typedef alias|不提供兼容|默认删除' docs/api-v1.1-boundary.md` passed.
- `test -f docs/api-v1.1-symbol-map.md` plus required key symbol checks passed.
- `rg -n 'no wrapper|no alias|不提供兼容|删除旧名|rename/delete' docs/api-v1.1-symbol-map.md` passed.
- Exhaustive static check confirmed every current public header old symbol listed in the plan is present in `docs/api-v1.1-symbol-map.md`.
- `rg -n 'mrtc_|MRTC_|AWS|KVS' docs/api-v1.1-boundary.md docs/api-v1.1-symbol-map.md` was reviewed; hits are in migration mapping, deletion policy, whitelist, source compliance, private/test-only boundary, or source filename contexts.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 7 Plan 02 can consume the residual classification contract and symbol map to implement `scripts/scan-api-residuals.sh --check-symbol-map`. Phase 8 can consume the mapping table as the canonical public header rename input.

## Self-Check: PASSED

- Found `docs/api-v1.1-boundary.md`.
- Found `docs/api-v1.1-symbol-map.md`.
- Found `.planning/phases/07-api/07-01-SUMMARY.md`.
- Found task commit `6ba4ba2`.
- Found task commit `83a5109`.

---
*Phase: 07-api*
*Completed: 2026-05-15*
