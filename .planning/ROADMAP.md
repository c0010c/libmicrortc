# Roadmap: libmicrortc

**Created:** 2026-05-12
**Last updated:** 2026-05-15 after v1.1 roadmap creation
**Mode:** standard
**Granularity:** standard

## Milestones

- ✅ **v1.0 libmicrortc v1** - Phases 1-6 shipped on 2026-05-14. Full archive: [v1.0-ROADMAP.md](./milestones/v1.0-ROADMAP.md), [v1.0-REQUIREMENTS.md](./milestones/v1.0-REQUIREMENTS.md), [v1.0-MILESTONE-AUDIT.md](./milestones/v1.0-MILESTONE-AUDIT.md), [v1.0-phases/](./milestones/v1.0-phases/).
- 🚧 **v1.1 API 清理** - Phases 7-11 planned. Scope is breaking C public API naming cleanup, public/private boundary cleanup, `.clang-tidy` naming convergence, documentation, provenance, and v1.0 regression gates.

## Overview

v1.1 在 v1.0 已验证的 Chrome、DataChannel、TURN relay、H264/Opus 双向媒体能力之上，只做 API 和命名清理。公共 C API 统一切换为函数 `rtc_*`、类型 `Rtc*`、宏和枚举常量 `RTC_*`；旧 `mrtc_*`、`MRTC_*` 和 AWS/KVS 风格 public names 默认作为迁移/删除对象，不新增兼容 wrapper 层。CMake package 和 target namespace 保持 `micrortc::micrortc`。

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): planned milestone work.
- Decimal phases (7.1, 7.2): urgent insertions, if needed.
- v1.0 completed Phases 1-6, so v1.1 starts at Phase 7.

- [ ] **Phase 7: API 盘点与命名契约冻结** - 冻结 public/private/test-only 边界、旧名到新名映射、删除策略和残留扫描规则。
- [ ] **Phase 8: Public Headers 与核心 API 改名** - 将 installed public headers 和核心签名切换到 `rtc_*`、`Rtc*`、`RTC_*`，并守住 private API 不泄漏。
- [ ] **Phase 9: Package Consumer 与公开调用点迁移** - 让 installed package consumer、README、public tests 和 Chrome E2E answerer 全部使用新 API。
- [ ] **Phase 10: 内部标识符与 Private Boundary 收敛** - 按 `.clang-tidy` 收敛内部命名，并让示例/E2E 不再把 private 字段或 private header 当作用户 API。
- [ ] **Phase 11: 文档、迁移指南、溯源与发布门禁** - 完成迁移文档、breaking release 说明、provenance 更新和 v1.1 总验收。

## Phase Details

### Phase 7: API 盘点与命名契约冻结
**Goal**: 开发者和维护者可以在改代码前看到冻结的 v1.1 API 边界、命名规则、旧名迁移映射和删除策略。
**Depends on**: Phase 6
**Requirements**: API-01, API-02, API-04, STYLE-04
**Success Criteria** (what must be TRUE):
  1. 开发者可以在文档中看到 v1.1 public、private 和 test-only API 边界。
  2. 开发者可以查到 `include/micrortc/*.h` public symbols 的完整旧名到新名映射，且新名使用 `rtc_*`、`Rtc*`、`RTC_*`。
  3. 开发者可以明确看到旧 `mrtc_*`、`MRTC_*`、AWS/KVS 风格 public API 默认删除，不提供兼容 wrapper 层。
  4. 维护者可以通过残留扫描结果区分禁止残留、来源合规记录和迁移文档白名单。
**Plans**: 2 plans
Plans:
- [ ] 07-01-PLAN.md — 冻结 API 边界文档和 public symbol 旧名到新名映射。
- [ ] 07-02-PLAN.md — 创建分类 residual scan 脚本和 JSON 报告验证入口。

### Phase 8: Public Headers 与核心 API 改名
**Goal**: 开发者可以只通过 installed `include/micrortc/*.h` 使用新的 v1.1 public API，且 public headers 不暴露 private 协议实现面。
**Depends on**: Phase 7
**Requirements**: API-03, HDR-01, HDR-02, HDR-03, HDR-04
**Success Criteria** (what must be TRUE):
  1. 开发者可以在 installed public headers 中调用 `rtc_*` 函数、使用 `Rtc*` 类型并引用 `RTC_*` 宏和枚举常量。
  2. 开发者不会在 public headers 中看到旧 `MRTC_*` 全大写 public typedef、struct 或 enum 类型。
  3. umbrella header 和每个 public 子头都可以被单独 include，并通过 include-order 编译测试。
  4. public headers 不泄漏 private transport、RTP、RTCP、SRTP、SDP 或 media helper API。
**Plans**: TBD

### Phase 9: Package Consumer 与公开调用点迁移
**Goal**: 下游开发者可以通过 installed `micrortc::micrortc` package 和新 API 构建运行，仓库内公开示例和测试也只展示新 API。
**Depends on**: Phase 8
**Requirements**: CONS-01, CONS-02, CONS-03
**Success Criteria** (what must be TRUE):
  1. `tests/package-consumer` 可以通过 installed package、`micrortc::micrortc` target 和新 `rtc_*` API configure、build、run。
  2. README 代码片段、public API tests 和 Chrome E2E answerer 全部使用新 API。
  3. PeerConnection、SDP、ICE candidate、DataChannel、transceiver 和 encoded frame 的既有行为可以通过新 API 测试观察到。
  4. 使用者不需要改 CMake package/target namespace；仍然使用 `micrortc::micrortc`。
**Plans**: TBD

### Phase 10: 内部标识符与 Private Boundary 收敛
**Goal**: 维护者可以用 `.clang-tidy` 规则验证 include、src、tests、examples 的命名风格，示例和 E2E 不再把 private 实现细节当作正常 API。
**Depends on**: Phase 9
**Requirements**: STYLE-01, STYLE-02, STYLE-03
**Success Criteria** (what must be TRUE):
  1. 维护者可以运行 v1.1 style 验证，覆盖 `include`、`src`、`tests` 和 `examples`。
  2. 内部类型、typedef、enum 和结构字段命名按 `.clang-tidy` 收敛，函数、变量、参数和 member 使用 lower_case。
  3. 示例和 E2E harness 不再向用户展示 private struct 字段作为正常 API。
  4. 示例和 E2E harness 不再要求用户 include private header 才能完成正常调用流程。
**Plans**: TBD

### Phase 11: 文档、迁移指南、溯源与发布门禁
**Goal**: 开发者可以完成 v1.0 到 v1.1 的 source API 迁移，维护者可以用总验收证明 API/style 清理完成且 v1.0 行为未退化。
**Depends on**: Phase 10
**Requirements**: DOC-01, DOC-02, DOC-03, VER-01, VER-02, VER-03, VER-04
**Success Criteria** (what must be TRUE):
  1. 开发者可以阅读 v1.0 到 v1.1 迁移指南，看到 `mrtc_*`/`MRTC_*` 到 `rtc_*`/`Rtc*`/`RTC_*` 的映射、行为不变项和删除项。
  2. README、构建文档和发布说明明确 v1.1 是 source API breaking cleanup。
  3. AWS/KVS 派生文件改名或移动后的 provenance/source manifest 路径保持准确。
  4. `ctest`、package consumer 和 `scripts/verify-v1.sh` 通过；存在 `mrtc-ice-servers.local.json` 时 TURN relay E2E 也通过。
  5. v1.1 总验收可以区分 API/style checks、package consumer、CTest、Chrome host E2E 和 TURN relay E2E。
**Plans**: TBD

## Coverage

| Requirement | Phase |
|-------------|-------|
| API-01 | Phase 7 |
| API-02 | Phase 7 |
| API-03 | Phase 8 |
| API-04 | Phase 7 |
| HDR-01 | Phase 8 |
| HDR-02 | Phase 8 |
| HDR-03 | Phase 8 |
| HDR-04 | Phase 8 |
| CONS-01 | Phase 9 |
| CONS-02 | Phase 9 |
| CONS-03 | Phase 9 |
| STYLE-01 | Phase 10 |
| STYLE-02 | Phase 10 |
| STYLE-03 | Phase 10 |
| STYLE-04 | Phase 7 |
| DOC-01 | Phase 11 |
| DOC-02 | Phase 11 |
| DOC-03 | Phase 11 |
| VER-01 | Phase 11 |
| VER-02 | Phase 11 |
| VER-03 | Phase 11 |
| VER-04 | Phase 11 |

**Mapped:** 22/22 v1.1 requirements
**Unmapped:** 0
**Duplicates:** 0

## Completed Milestone Summary

<details>
<summary>✅ v1.0 libmicrortc v1 (Phases 1-6) - SHIPPED 2026-05-14</summary>

| Phase | Name | Plans | Status |
|-------|------|-------|--------|
| 1 | 基线、范围与合规边界 | 1/1 | Complete |
| 2 | 独立构建与库骨架 | 1/1 | Complete |
| 3 | AWS 风格 API 与 signaling-free PeerConnection | 1/1 | Complete |
| 4 | 传输、安全与 DataChannel 协议核心 | 5/5 | Complete |
| 5 | H264/Opus 媒体路径 | 5/5 | Complete |
| 6 | Chrome 自动化 E2E 与测试收口 | 8/8 | Complete |

**Delivered:** 从本地 AWS KVS WebRTC C SDK 基线剥离出可独立构建、可自动验证的 C WebRTC 协议栈 v1，覆盖 Linux x86_64、Chrome、H264、Opus、DataChannel、TURN relay 和双向音视频 E2E。

**Acceptance entrypoints:**

```bash
scripts/verify-v1.sh
scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json
```

**Canonical evidence:** `build/reports/mrtc-v1-summary.json`

</details>

## Progress

**Execution Order:**
Phases execute in numeric order: 7 -> 8 -> 9 -> 10 -> 11.

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. 基线、范围与合规边界 | v1.0 | 1/1 | Complete | 2026-05-14 |
| 2. 独立构建与库骨架 | v1.0 | 1/1 | Complete | 2026-05-14 |
| 3. AWS 风格 API 与 signaling-free PeerConnection | v1.0 | 1/1 | Complete | 2026-05-14 |
| 4. 传输、安全与 DataChannel 协议核心 | v1.0 | 5/5 | Complete | 2026-05-14 |
| 5. H264/Opus 媒体路径 | v1.0 | 5/5 | Complete | 2026-05-14 |
| 6. Chrome 自动化 E2E 与测试收口 | v1.0 | 8/8 | Complete | 2026-05-14 |
| 7. API 盘点与命名契约冻结 | v1.1 | 0/TBD | Not started | - |
| 8. Public Headers 与核心 API 改名 | v1.1 | 0/TBD | Not started | - |
| 9. Package Consumer 与公开调用点迁移 | v1.1 | 0/TBD | Not started | - |
| 10. 内部标识符与 Private Boundary 收敛 | v1.1 | 0/TBD | Not started | - |
| 11. 文档、迁移指南、溯源与发布门禁 | v1.1 | 0/TBD | Not started | - |

## Next Step

Run `$gsd-plan-phase 7`.

---
*For detailed v1.0 phase history, see `.planning/milestones/v1.0-phases/`.*
