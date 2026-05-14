#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
const { startServer } = require("./signaling-server");

const projectRoot = path.resolve(__dirname, "..", "..");

function usage() {
  return `Usage: node tests/e2e/run-manual.js [options]

Options:
  --page <path>             Browser page to load. Default: examples/chrome-e2e/index.html
  --answerer <path>         C answerer executable path. Default: build/examples/chrome-e2e/mrtc_chrome_answerer
  --fixtures <path>         H264/Opus fixture directory. Default: tests/fixtures
  --ice-config <path>       Optional local ICE server config for the C answerer.
  --port <port>             Local WebSocket port. Default: 0.
  --artifacts-dir <path>    Directory for signaling artifacts. Default: tests/e2e/artifacts/manual
  --timeout-ms <ms>         Server timeout. Default: 600000.
  --help                    Show this help.
`;
}

function parseArgs(argv) {
  const options = {
    page: path.join(projectRoot, "examples", "chrome-e2e", "index.html"),
    answerer: path.join(projectRoot, "build", "examples", "chrome-e2e", "mrtc_chrome_answerer"),
    fixtures: path.join(projectRoot, "tests", "fixtures"),
    iceConfig: null,
    port: 0,
    artifactsDir: path.join(projectRoot, "tests", "e2e", "artifacts", "manual"),
    timeoutMs: 600_000,
    help: false,
  };

  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--page") {
      options.page = path.resolve(argv[++i]);
    } else if (arg === "--answerer") {
      options.answerer = path.resolve(argv[++i]);
    } else if (arg === "--fixtures") {
      options.fixtures = path.resolve(argv[++i]);
    } else if (arg === "--ice-config") {
      options.iceConfig = path.resolve(argv[++i]);
    } else if (arg === "--port") {
      options.port = Number(argv[++i]);
    } else if (arg === "--artifacts-dir") {
      options.artifactsDir = path.resolve(argv[++i]);
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

function requirePath(filePath, label) {
  if (!fs.existsSync(filePath)) {
    throw new Error(`${label} not found: ${filePath}`);
  }
}

async function run(options) {
  requirePath(options.page, "page");
  requirePath(options.answerer, "answerer");
  requirePath(options.fixtures, "fixtures");
  if (options.iceConfig) {
    requirePath(options.iceConfig, "ice config");
  }

  const answererArgs = ["--fixtures", options.fixtures];
  if (options.iceConfig) {
    answererArgs.push("--ice-config", options.iceConfig);
  }

  const server = await startServer({
    port: options.port,
    answerer: options.answerer,
    answererArgs,
    artifactsDir: options.artifactsDir,
    timeoutMs: options.timeoutMs,
  });

  const url = new URL(pathToFileURL(options.page).toString());
  url.searchParams.set("manual", "1");
  url.searchParams.set("ws", server.url);

  process.stdout.write("\nManual Chrome E2E is ready.\n");
  process.stdout.write(`Open: ${url.toString()}\n`);
  process.stdout.write("Stop: Ctrl-C\n\n");
  process.stdout.write(`${JSON.stringify({ stage: "manual", status: "ready", url: url.toString(), ws: server.url })}\n`);
  process.stdin.resume();
  return server;
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
