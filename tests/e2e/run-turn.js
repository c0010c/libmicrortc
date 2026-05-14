#!/usr/bin/env node
"use strict";

const path = require("node:path");
const { spawnSync } = require("node:child_process");
const { loadTurnConfig, redactText } = require("./turn-config");

function parseArgs(argv) {
  const passthrough = [];
  let turnConfig = null;

  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--turn-config") {
      turnConfig = argv[++i] || null;
    } else if (arg.startsWith("--turn-config=")) {
      turnConfig = arg.slice("--turn-config=".length);
    } else {
      passthrough.push(arg);
    }
  }

  return { passthrough, turnConfig };
}

function hasProjectArg(args) {
  return args.some((arg) => arg === "--project" || arg.startsWith("--project="));
}

function main() {
  const { passthrough, turnConfig } = parseArgs(process.argv.slice(2));
  let config;
  const configPath = turnConfig && !path.isAbsolute(turnConfig)
    ? path.resolve(process.env.INIT_CWD || process.cwd(), turnConfig)
    : turnConfig;

  try {
    config = loadTurnConfig(configPath);
  } catch (error) {
    process.stderr.write(`[${error.stage || "turn-config"}] ${redactText(error.message || String(error))}\n`);
    process.exit(1);
  }

  const playwrightBin = path.join(__dirname, "node_modules", ".bin", "playwright");
  const args = [
    "test",
    "--config=playwright.config.js",
    "--grep",
    "@turn",
  ];
  if (!hasProjectArg(passthrough)) {
    args.push("--project=chrome-turn");
  }
  args.push(...passthrough);

  const result = spawnSync(playwrightBin, args, {
    cwd: __dirname,
    stdio: "inherit",
    env: {
      ...process.env,
      MRTC_E2E_TURN_CONFIG: config.iceConfigPath,
    },
  });

  if (result.error) {
    process.stderr.write(`[turn-relay] ${redactText(result.error.message)}\n`);
    process.exit(1);
  }
  process.exit(result.status == null ? 1 : result.status);
}

if (require.main === module) {
  main();
}
