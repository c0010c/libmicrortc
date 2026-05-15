---
phase: 07-api
reviewed: 2026-05-15T11:20:22Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - docs/api-v1.1-boundary.md
  - docs/api-v1.1-symbol-map.md
  - scripts/scan-api-residuals.sh
findings:
  critical: 2
  warning: 0
  info: 0
  total: 2
status: issues_found
---

# Phase 07: Code Review Report

**Reviewed:** 2026-05-15T11:20:22Z
**Depth:** standard
**Files Reviewed:** 3
**Status:** issues_found

## Summary

已按 standard 深度审查 API 边界文档、symbol map 文档和 residual scan 脚本。文档边界与当前 installed public headers 的 symbol map 覆盖关系一致：当前 header 旧 public symbol 为 69 个，symbol map 映射 69 个，未发现缺失映射。

脚本存在两个必须修复的问题：质量门失败时仍返回成功退出码，以及路径参数没有限制在 repo 内，违反边界文档对路径归一化和不安全路径拒绝的要求。

## Critical Issues

### CR-01 [BLOCKER]: forbidden residual 报告失败但脚本退出码仍为 0

**File:** `scripts/scan-api-residuals.sh:349`

**Issue:** `forbiddenCount > 0` 时报告 JSON 的 `status` 被写成 `"failed"`，但 Node 程序没有设置非零退出码。实测 `bash scripts/scan-api-residuals.sh --output /tmp/libmicrortc-api-residuals-review.json --check-symbol-map docs/api-v1.1-symbol-map.md` 返回 `exit=0`，同时报告中 `status=failed`、`forbidden_public_residual=172`。这会让 CI 或调用方把失败的 residual gate 当作成功，公开 API 残留可直接漏过。

**Fix:**
```javascript
  fs.writeFileSync(outputPath, `${JSON.stringify(report, null, 2)}\n`);
  if (forbiddenCount > 0) {
    process.exitCode = 1;
  }
```

如果调用方需要“只生成报告不失败”的模式，应显式增加 `--report-only` 一类选项；默认扫描语义应与报告 `status: failed` 保持一致。

### CR-02 [BLOCKER]: `--output` 和 `--check-symbol-map` 允许路径逃逸 repo

**File:** `scripts/scan-api-residuals.sh:33`

**Issue:** `normalize_path()` 只把相对路径拼到 `$ROOT_DIR` 后面，绝对路径原样接受，没有 `realpath` 归一化，也没有校验最终路径仍位于 repo 内。结果是 `--output ../...` 可写到 repo 外，`--check-symbol-map /path/outside` 可读取 repo 外文件。这违反 `docs/api-v1.1-boundary.md:133` 中 output path、symbol map path 必须归一到 repo 内或拒绝不安全位置的契约，也让脚本暴露 path traversal 写文件/读文件面。

**Fix:**
```bash
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"

normalize_repo_path() {
    local raw_path="$1"
    local resolved_path

    case "$raw_path" in
        /*) resolved_path="$(realpath -m -- "$raw_path")" ;;
        *) resolved_path="$(realpath -m -- "$ROOT_DIR/$raw_path")" ;;
    esac

    case "$resolved_path" in
        "$ROOT_DIR"|"$ROOT_DIR"/*) printf '%s\n' "$resolved_path" ;;
        *) fail_usage "path escapes repository: $raw_path" ;;
    esac
}
```

然后让 `--output` 和 `--check-symbol-map` 都调用该函数，并在 `mkdir -p "$(dirname "$OUTPUT_PATH")"` 之前完成拒绝。

---

_Reviewed: 2026-05-15T11:20:22Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
