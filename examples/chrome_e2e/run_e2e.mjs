#!/usr/bin/env node

import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";

const repoRoot = resolve(new URL("../..", import.meta.url).pathname);

function parseArgs(argv) {
  return {
    dryRun: argv.includes("--dry-run"),
    pageSmoke: argv.includes("--page-smoke"),
  };
}

function checkPath(relativePath) {
  return {
    path: relativePath,
    exists: existsSync(resolve(repoRoot, relativePath)),
  };
}

function loadPackage() {
  return JSON.parse(readFileSync(resolve(repoRoot, "package.json"), "utf8"));
}

async function dryRun() {
  const requiredFiles = [
    "sample1.opus",
    "test-25fps.h264",
    "examples/chrome_e2e/page/index.html",
    "examples/chrome_e2e/signaling.mjs",
  ].map(checkPath);

  const pkg = loadPackage();
  const scripts = {
    "e2e:chrome": pkg.scripts?.["e2e:chrome"] === "node examples/chrome_e2e/run_e2e.mjs",
    "e2e:chrome:dry-run": pkg.scripts?.["e2e:chrome:dry-run"] === "node examples/chrome_e2e/run_e2e.mjs --dry-run",
  };

  const ok = requiredFiles.every((item) => item.exists) && Object.values(scripts).every(Boolean);
  const summary = {
    ok,
    layer: ok ? "none" : "preflight",
    files: requiredFiles,
    scripts,
  };
  console.log(JSON.stringify(summary, null, 2));
  if (!ok) {
    process.exitCode = 1;
  }
}

async function pageSmoke() {
  console.error("--page-smoke will be implemented by Task 2");
  process.exitCode = 1;
}

const args = parseArgs(process.argv.slice(2));

if (args.dryRun) {
  await dryRun();
} else if (args.pageSmoke) {
  await pageSmoke();
} else {
  await dryRun();
}
