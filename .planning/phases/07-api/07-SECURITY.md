---
phase: 07
slug: api
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-15
updated: 2026-05-15
---

# Phase 07 — Security

> Phase 7 安全审计：验证计划期 threat model 中的缓解措施是否真实落地于文档、脚本和验证证据。

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| maintainer -> docs | 维护者把本地 source/header 事实转写为开发者 API 边界和 symbol map 文档。 | public/private/test-only API 分类、旧名到新名映射、删除策略 |
| docs -> future scripts | 后续扫描脚本消费 Phase 7 文档中的分类和 mapping 语义。 | residual 分类枚举、finding 必填字段、symbol map coverage 规则 |
| shell args -> script | 用户传入 output/map path 和 CLI options。 | 文件路径、format、symbol map path |
| repository files -> report | residual scanner 读取源码、文档和测试文件并生成报告。 | path/line/symbol/category/reason 元数据 |
| whitelist rules -> compliance result | 分类规则决定旧 API residual 是禁止项、迁移文档白名单、来源合规记录还是 test harness 残留。 | finite category、reason、summary counts |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-07-01 | Tampering | `scripts/scan-api-residuals.sh` | mitigate | 脚本引用 path/format/output 参数，未知参数 `exit 2`，不用 `eval`，并把 output/map path 归一化到 repo 内；验证命令覆盖 unknown option、no `eval` 和 symbol-map/report 生成。 | closed |
| T-07-02 | Information Disclosure | `mrtc-ice-servers.local.json`, generated reports | mitigate | 边界文档声明本地 TURN config 是 local secret；脚本排除 `mrtc-ice-servers.local.json`，只输出 path/line/symbol/category/reason，不输出匹配行全文，并对敏感 symbol 词脱敏；验证报告不包含 secret 字段词。 | closed |
| T-07-03 | Repudiation | residual whitelist categories | mitigate | 文档和脚本使用有限 residual category；每条 finding 都包含 path、line、symbol、category、reason，summary 保留五类统计；symbol map 每行包含 old/new/category/action/source。 | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Verification Evidence

| Threat ID | Evidence |
|-----------|----------|
| T-07-01 | `scripts/scan-api-residuals.sh:33` 定义 path normalization；`scripts/scan-api-residuals.sh:59` 起解析 CLI 并在未知参数时 `exit 2`；`scripts/scan-api-residuals.sh:98` 拒绝非 JSON format；`scripts/scan-api-residuals.sh:107` 使用 quoted args 调用 Node；`! rg -n 'eval ' scripts/scan-api-residuals.sh` 通过。 |
| T-07-02 | `docs/api-v1.1-boundary.md:122` 限制报告只输出 path/line/symbol/category/reason 并禁止输出 TURN secret 字段；`docs/api-v1.1-boundary.md:167` 声明本地 TURN config 不读取、不打印、不提交；`scripts/scan-api-residuals.sh:132` 和 `scripts/scan-api-residuals.sh:252` 排除 `mrtc-ice-servers.local.json`；`scripts/scan-api-residuals.sh:208` 对敏感 symbol 词脱敏；`! rg -n 'raw-user|raw-credential|raw-password|username|credential|password' build/reports/api-residuals.json` 通过。 |
| T-07-03 | `docs/api-v1.1-boundary.md:100` 定义有限分类；`docs/api-v1.1-boundary.md:114` 定义 finding 必填字段；`docs/api-v1.1-symbol-map.md:22` 要求 category/action/source；`scripts/scan-api-residuals.sh:119` 固定五类 category；`scripts/scan-api-residuals.sh:293` 写入 path/line/symbol/category/reason；Node schema 检查确认每条 finding 都有必填字段。 |

## Summary Threat Flags

| Summary | Threat Flags | Disposition |
|---------|--------------|-------------|
| `07-01-SUMMARY.md` | None；该计划仅新增文档，未引入网络端点、认证路径、文件访问行为或 schema 信任边界。 | maps to existing plan-time T-07-01/T-07-02/T-07-03 context; no unregistered flag |
| `07-02-SUMMARY.md` | None；新增文件读取和报告写入面已在 `07-02-PLAN.md` threat model 中覆盖。 | maps to T-07-01/T-07-02/T-07-03; no unregistered flag |

## Accepted Risks Log

No accepted risks.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-15 | 3 | 3 | 0 | Codex / gsd-secure-phase |

## Security Audit 2026-05-15

| Metric | Count |
|--------|-------|
| Threats found | 3 |
| Closed | 3 |
| Open | 0 |

## Verification Commands

| Command | Result |
|---------|--------|
| `gsd-sdk query config-get workflow.security_enforcement --raw 2>/dev/null || echo true` | `true` |
| `scripts/scan-api-residuals.sh --check-symbol-map docs/api-v1.1-symbol-map.md --format json --output build/reports/api-residuals.json` | passed; script exited 0 |
| Node schema check over `build/reports/api-residuals.json` | passed; required top-level fields and finding fields present |
| `! rg -n 'raw-user|raw-credential|raw-password|username|credential|password' build/reports/api-residuals.json` | passed |
| `! rg -n 'eval ' scripts/scan-api-residuals.sh` | passed |
| `scripts/scan-api-residuals.sh --unknown-option ...; test "$?" -eq 2` | passed |

Note: `build/reports/api-residuals.json` currently reports `status: "failed"` because Phase 7 intentionally captures pre-Phase-8 public old-name residuals as baseline evidence. This does not indicate an unmitigated Phase 7 security threat because the scanner exits 0, emits finite categories, and reports those residuals as `forbidden_public_residual` for the next phase gate.

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-15
