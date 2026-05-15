#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
FORMAT="json"
OUTPUT_PATH="$ROOT_DIR/build/reports/api-residuals.json"
SYMBOL_MAP_PATH=""

usage() {
    cat <<'EOF'
Usage: scripts/scan-api-residuals.sh [options]

Options:
  --format json              Report format. Default: json.
  --output <path>            Output path. Relative paths resolve from repo root.
                             Default: build/reports/api-residuals.json.
  --check-symbol-map <path>  Verify current installed header old symbols are mapped.
  --help, -h                 Show this help.

The scan classifies old mrtc_*, MRTC_*, AWS and KVS residuals without printing
matched source lines. Generated directories, reflib, E2E runtime artifacts and
local runtime config files are excluded by default.
EOF
}

fail_usage() {
    printf 'scan-api-residuals: %s\n\n' "$1" >&2
    usage >&2
    exit 2
}

normalize_repo_path() {
    local target_var="$1"
    local option_name="$2"
    local input_path="$3"
    local candidate_path
    local resolved_path

    case "$input_path" in
        /*) candidate_path="$input_path" ;;
        *) candidate_path="$ROOT_DIR/$input_path" ;;
    esac

    if ! resolved_path="$(realpath -m -- "$candidate_path" 2>/dev/null)"; then
        fail_usage "$option_name path could not be normalized: $input_path"
    fi

    case "$resolved_path" in
        "$ROOT_DIR"|"$ROOT_DIR"/*)
            printf -v "$target_var" '%s' "$resolved_path"
            ;;
        *)
            fail_usage "$option_name path must stay inside repo root: $input_path"
            ;;
    esac
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --format)
            [ "$#" -ge 2 ] || fail_usage "--format requires a value"
            FORMAT="$2"
            shift 2
            ;;
        --format=*)
            FORMAT="${1#--format=}"
            shift
            ;;
        --output)
            [ "$#" -ge 2 ] || fail_usage "--output requires a path"
            normalize_repo_path OUTPUT_PATH "--output" "$2"
            shift 2
            ;;
        --output=*)
            normalize_repo_path OUTPUT_PATH "--output" "${1#--output=}"
            shift
            ;;
        --check-symbol-map)
            [ "$#" -ge 2 ] || fail_usage "--check-symbol-map requires a path"
            normalize_repo_path SYMBOL_MAP_PATH "--check-symbol-map" "$2"
            shift 2
            ;;
        --check-symbol-map=*)
            normalize_repo_path SYMBOL_MAP_PATH "--check-symbol-map" "${1#--check-symbol-map=}"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail_usage "unknown option: $1"
            ;;
    esac
done

[ "$FORMAT" = "json" ] || fail_usage "unsupported format: $FORMAT"

mkdir -p "$(dirname "$OUTPUT_PATH")" || {
    printf 'scan-api-residuals: failed to create report directory\n' >&2
    exit 1
}

cd "$ROOT_DIR" || exit 1

node - "$ROOT_DIR" "$OUTPUT_PATH" "$SYMBOL_MAP_PATH" <<'NODE'
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");

const rootDir = process.argv[2];
const outputPath = process.argv[3];
const symbolMapPath = process.argv[4] || "";
const startedAt = Date.now();

const categories = [
  "forbidden_public_residual",
  "migration_doc_whitelist",
  "source_compliance_record",
  "test_harness_non_public_api",
  "ignored_generated_artifact",
];

function toRepoPath(filePath) {
  const relativePath = path.relative(rootDir, path.resolve(rootDir, filePath));
  return relativePath.split(path.sep).join("/");
}

function isIgnoredPath(repoPath) {
  return (
    repoPath === "mrtc-ice-servers.local.json" ||
    repoPath.endsWith("/mrtc-ice-servers.local.json") ||
    repoPath === ".git" ||
    repoPath.startsWith(".git/") ||
    repoPath === "reflib" ||
    repoPath.startsWith("reflib/") ||
    repoPath === "build" ||
    repoPath.startsWith("build/") ||
    /^build-[^/]+(?:\/|$)/.test(repoPath) ||
    repoPath === "cmake-build-debug" ||
    repoPath.startsWith("cmake-build-debug/") ||
    repoPath.startsWith("tests/e2e/node_modules/") ||
    repoPath.startsWith("tests/e2e/artifacts/")
  );
}

function classify(repoPath) {
  if (isIgnoredPath(repoPath)) {
    return {
      category: "ignored_generated_artifact",
      reason: "generated or local runtime artifact excluded from source policy",
    };
  }
  if (repoPath === "docs/api-v1.1-boundary.md" || repoPath === "docs/api-v1.1-symbol-map.md") {
    return {
      category: "migration_doc_whitelist",
      reason: "migration mapping records historical old API names",
    };
  }
  if (repoPath.startsWith(".planning/phases/07-api/")) {
    return {
      category: "migration_doc_whitelist",
      reason: "phase planning record describes the migration policy",
    };
  }
  if (
    repoPath.startsWith(".planning/milestones/") ||
    repoPath.startsWith(".planning/") ||
    /(?:COMPLIANCE|SOURCE-MANIFEST|LICENSE|NOTICE)(?:\.md)?$/.test(repoPath)
  ) {
    return {
      category: "source_compliance_record",
      reason: "provenance or planning record preserves historical source names",
    };
  }
  if (repoPath.startsWith("include/micrortc/") && repoPath.endsWith(".h")) {
    return {
      category: "forbidden_public_residual",
      reason: "installed public header must use rtc/Rtc/RTC names",
    };
  }
  if (
    repoPath.startsWith("src/") ||
    repoPath.startsWith("tests/") ||
    repoPath.startsWith("examples/chrome-e2e/") ||
    repoPath.startsWith("scripts/")
  ) {
    return {
      category: "test_harness_non_public_api",
      reason: "non-public implementation or harness residual outside installed API",
    };
  }
  if (repoPath.startsWith("examples/") || /(^|\/)README(?:\.md)?$/i.test(repoPath)) {
    return {
      category: "forbidden_public_residual",
      reason: "public-facing example or README must not expose old API names",
    };
  }
  return {
    category: "source_compliance_record",
    reason: "repository record preserves historical source names",
  };
}

function safeSymbol(symbol) {
  return symbol
    .replace(/raw-user/gi, "<redacted>")
    .replace(/raw-credential/gi, "<redacted>")
    .replace(/raw-password/gi, "<redacted>")
    .replace(/usernames?/gi, "<redacted>")
    .replace(/credentials?/gi, "<redacted>")
    .replace(/passwords?/gi, "<redacted>");
}

function listExistingRoots() {
  return ["include", "src", "tests", "examples", "docs", ".planning"].filter((entry) => {
    return fs.existsSync(path.join(rootDir, entry));
  });
}

function runResidualScan() {
  const roots = listExistingRoots();
  if (roots.length === 0) {
    return [];
  }

  const args = [
    "--json",
    "--no-heading",
    "--line-number",
    "--only-matching",
    "--color",
    "never",
    "--glob",
    "!reflib/**",
    "--glob",
    "!.git/**",
    "--glob",
    "!build/**",
    "--glob",
    "!build-*/**",
    "--glob",
    "!cmake-build-debug/**",
    "--glob",
    "!tests/e2e/node_modules/**",
    "--glob",
    "!tests/e2e/artifacts/**",
    "--glob",
    "!mrtc-ice-servers.local.json",
    "\\b(mrtc_[A-Za-z0-9_]+|MRTC_[A-Z0-9_]+|AWS|KVS)\\b",
    ...roots,
  ];

  const result = spawnSync("rg", args, {
    cwd: rootDir,
    encoding: "utf8",
    maxBuffer: 64 * 1024 * 1024,
  });

  if (result.error) {
    throw new Error(`failed to run rg: ${result.error.message}`);
  }
  if (result.status !== 0 && result.status !== 1) {
    throw new Error(`rg exited with status ${result.status}`);
  }

  const seen = new Set();
  const findings = [];
  for (const line of result.stdout.split(/\n/)) {
    if (!line) {
      continue;
    }
    const event = JSON.parse(line);
    if (event.type !== "match") {
      continue;
    }
    const repoPath = toRepoPath(event.data.path.text);
    if (isIgnoredPath(repoPath)) {
      continue;
    }
    const lineNumber = Number(event.data.line_number || 0);
    for (const submatch of event.data.submatches || []) {
      const symbol = submatch.match.text;
      const key = `${repoPath}\0${lineNumber}\0${symbol}`;
      if (seen.has(key)) {
        continue;
      }
      seen.add(key);
      const classification = classify(repoPath);
      findings.push({
        path: repoPath,
        line: lineNumber,
        symbol: safeSymbol(symbol),
        category: classification.category,
        reason: classification.reason,
      });
    }
  }

  findings.sort((left, right) => {
    const pathOrder = left.path.localeCompare(right.path);
    if (pathOrder !== 0) {
      return pathOrder;
    }
    if (left.line !== right.line) {
      return left.line - right.line;
    }
    return left.symbol.localeCompare(right.symbol);
  });
  return findings;
}

function extractPublicOldSymbols() {
  const symbols = new Set();
  for (const header of ["include/micrortc/micrortc.h", "include/micrortc/peer_connection.h"]) {
    const content = fs.readFileSync(path.join(rootDir, header), "utf8");
    for (const match of content.matchAll(/\b(mrtc_[A-Za-z0-9_]+|MRTC_[A-Z0-9_]+)\b/g)) {
      const prefix = content.slice(Math.max(0, match.index - "struct ".length), match.index);
      if (prefix === "struct ") {
        continue;
      }
      symbols.add(match[1]);
    }
  }
  return [...symbols].sort();
}

function checkSymbolMapCoverage(mapPath) {
  if (!mapPath) {
    return { checked: false, missing: [] };
  }
  const content = fs.readFileSync(mapPath, "utf8");
  const lines = content.split(/\n/);
  const missing = [];
  const missingNewName = [];
  for (const symbol of extractPublicOldSymbols()) {
    const row = lines.find((line) => line.includes(`\`${symbol}\``));
    if (!row) {
      missing.push(symbol);
      continue;
    }
    if (!/`(?:rtc_[A-Za-z0-9_]+|Rtc[A-Za-z0-9_]+|RTC_[A-Z0-9_]+)`/.test(row)) {
      missingNewName.push(symbol);
    }
  }
  if (missing.length > 0 || missingNewName.length > 0) {
    for (const symbol of missing) {
      process.stderr.write(`scan-api-residuals: symbol map missing old symbol ${symbol}\n`);
    }
    for (const symbol of missingNewName) {
      process.stderr.write(`scan-api-residuals: symbol map missing new symbol for ${symbol}\n`);
    }
    process.exit(1);
  }
  return { checked: true, missing: [] };
}

try {
  const symbolMap = checkSymbolMapCoverage(symbolMapPath);
  const findings = runResidualScan();
  const byCategory = Object.fromEntries(categories.map((category) => [category, 0]));
  for (const finding of findings) {
    byCategory[finding.category] += 1;
  }
  const forbiddenCount = byCategory.forbidden_public_residual;
  const report = {
    phase: "07-api",
    status: forbiddenCount > 0 ? "failed" : "passed",
    summary: {
      total_findings: findings.length,
      by_category: byCategory,
      scan_roots: listExistingRoots(),
      symbol_map_checked: symbolMap.checked,
    },
    findings,
    failure_reason: forbiddenCount > 0 ? "forbidden public residuals present" : null,
    duration_ms: Date.now() - startedAt,
  };
  fs.writeFileSync(outputPath, `${JSON.stringify(report, null, 2)}\n`);
} catch (error) {
  process.stderr.write(`scan-api-residuals: ${error.message || String(error)}\n`);
  process.exit(1);
}
NODE
