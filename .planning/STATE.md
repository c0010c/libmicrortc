---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: API 清理
status: ready_to_plan
last_updated: "2026-05-15T17:23:30+08:00"
last_activity: 2026-05-15
progress:
  total_phases: 5
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# State: libmicrortc

**Initialized:** 2026-05-12
**Workflow Mode:** yolo
**Granularity:** standard
**Parallelization:** true

## Project Reference

See: `.planning/PROJECT.md` (updated 2026-05-15)

**Core value:** 把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。
**Current focus:** Phase 7 - API 盘点与命名契约冻结
**Last activity:** 2026-05-15 - Created v1.1 roadmap for API 清理.

## Current Position

Phase: 7 of 11 (v1.1 的 1/5) - API 盘点与命名契约冻结
Plan: TBD
Status: Ready to plan
Last activity: 2026-05-15 - v1.1 roadmap created with 22/22 requirements mapped

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 21
- Average duration: not tracked
- Total execution time: not tracked

**By Milestone:**

| Milestone | Phases | Plans | Requirements | Status |
|-----------|--------|-------|--------------|--------|
| v1.0 libmicrortc v1 | 6/6 | 21/21 | 45/45 | Complete |
| v1.1 API 清理 | 0/5 | 0/TBD | 0/22 | Ready to plan |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table. Recent decisions affecting current work:

- v1.1 public C API prefix is `rtc`, not `mrtc`.
- Public functions use `rtc_*`; public types, typedefs, and enum types use `Rtc*`; macros and enum constants use `RTC_*`.
- Old `mrtc_*`, `MRTC_*`, AWS/KVS style public API names are migration/removal targets, with no compatibility wrapper layer by default.
- CMake package/target namespace remains `micrortc::micrortc` unless a future requirement changes it.
- v1.1 scope is API naming cleanup, `.clang-tidy` style convergence, docs/provenance, and v1.0 regression gates only.

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 7 must inventory actual public/private/test-only symbols before mechanical renaming starts.
- Phase 10 needs code-level judgment on E2E answerer private dependencies to avoid promoting private helpers into public API.
- TURN relay verification in Phase 11 depends on local `mrtc-ice-servers.local.json` being present.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Browser expansion | Safari/Firefox interoperability | Deferred to future milestone | v1.1 scope |
| Media adapters | GStreamer/FFmpeg/capture/encoding adapters | Deferred to future milestone | v1.1 scope |
| API governance | API snapshot, Doxygen, ABI diff | Deferred until public API stabilizes further | v1.1 requirements |

## Session Continuity

Last session: 2026-05-15 17:23
Stopped at: v1.1 roadmap created, ready to plan Phase 7
Resume file: None
