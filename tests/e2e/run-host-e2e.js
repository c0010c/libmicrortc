#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { startServer } = require("./signaling-server");

function usage() {
  return `Usage: node tests/e2e/run-host-e2e.js [options]

Options:
  --page <path>             Browser page to load. Default: examples/chrome-e2e/index.html
  --answerer <path>         C answerer executable path.
  --port <port>             Local WebSocket port. Default: 0.
  --artifacts-dir <path>    Directory for runner artifacts. Default: tests/e2e/artifacts/host
  --timeout-ms <ms>         Overall timeout. Default: 60000.
  --dry-run                 Validate inputs, print summary, and exit without launching browser or answerer.
  --help                    Show this help.

Stages: configure, signaling, answerer, browser, summary.
Message types: hello, offer, answer, candidate, event, done, error.
`;
}

function parseArgs(argv) {
  const options = {
    page: path.join("examples", "chrome-e2e", "index.html"),
    answerer: null,
    port: 0,
    artifactsDir: path.join("tests", "e2e", "artifacts", "host"),
    timeoutMs: 60_000,
    dryRun: false,
    help: false,
  };

  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--dry-run") {
      options.dryRun = true;
    } else if (arg === "--page") {
      options.page = argv[++i];
    } else if (arg === "--answerer") {
      options.answerer = argv[++i];
    } else if (arg === "--port") {
      options.port = Number(argv[++i]);
    } else if (arg === "--artifacts-dir") {
      options.artifactsDir = argv[++i];
    } else if (arg === "--timeout-ms") {
      options.timeoutMs = Number(argv[++i]);
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }

  if (!Number.isInteger(options.port) || options.port < 0 || options.port > 65535) {
    throw new Error("--port must be an integer between 0 and 65535");
  }
  if (!Number.isInteger(options.timeoutMs) || options.timeoutMs <= 0) {
    throw new Error("--timeout-ms must be a positive integer");
  }
  return options;
}

function validatePage(pagePath) {
  const absolute = path.resolve(pagePath);
  if (!fs.existsSync(absolute)) {
    throw new Error(`page not found: ${pagePath}`);
  }
  return absolute;
}

function validateAnswerer(answererPath) {
  if (!answererPath) {
    return null;
  }
  const absolute = path.resolve(answererPath);
  if (!fs.existsSync(absolute)) {
    throw new Error(`answerer not found: ${answererPath}`);
  }
  return absolute;
}

function writeSummary(options, summary) {
  fs.mkdirSync(options.artifactsDir, { recursive: true });
  const summaryPath = path.join(options.artifactsDir, "runner-summary.json");
  fs.writeFileSync(summaryPath, `${JSON.stringify(summary, null, 2)}\n`);
  return summaryPath;
}

async function run(options) {
  const page = validatePage(options.page);
  const answerer = validateAnswerer(options.answerer);
  const summary = {
    stage: options.dryRun ? "dry-run" : "configure",
    page,
    answerer,
    signaling: {
      port: options.port,
      timeout_ms: options.timeoutMs,
    },
  };

  if (options.dryRun) {
    const summaryPath = writeSummary(options, summary);
    process.stdout.write(`${JSON.stringify({ stage: "dry-run", status: "ok", summary: summaryPath })}\n`);
    return summary;
  }

  if (!answerer) {
    throw new Error("--answerer is required unless --dry-run is used");
  }

  const server = await startServer({
    port: options.port,
    answerer,
    artifactsDir: path.join(options.artifactsDir, "signaling"),
    timeoutMs: options.timeoutMs,
  });
  summary.stage = "signaling";
  summary.signaling.url = server.url;
  writeSummary(options, summary);
  process.stdout.write(`${JSON.stringify({ stage: "signaling", status: "listening", url: server.url })}\n`);
  return summary;
}

async function main() {
  try {
    const options = parseArgs(process.argv);
    if (options.help) {
      process.stdout.write(usage());
      return;
    }
    await run(options);
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main();
}

module.exports = {
  parseArgs,
  run,
  usage,
};
