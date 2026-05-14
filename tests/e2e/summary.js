"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { redactSecrets } = require("./turn-config");

function createHostSummary(fields = {}) {
  return {
    phase: "06-chrome-e2e",
    build: "not-run",
    ctest: "not-run",
    chrome_host: "pending",
    chrome_turn: "not-run",
    connection: "pending",
    datachannel: "pending",
    browser_media: {
      video: { status: "pending" },
      audio: { status: "pending" },
    },
    c_media: {
      video: { status: "pending", frames: 0, bytes: 0 },
      audio: { status: "pending", frames: 0, bytes: 0 },
    },
    turn_relay: "not-run",
    duration_ms: 0,
    failure_stage: null,
    failure_reason: null,
    ...fields,
  };
}

function markFailure(summary, stage, reason) {
  summary.failure_stage = stage;
  summary.failure_reason = reason || "unknown failure";
  if (stage === "connection") {
    summary.connection = "failed";
  } else if (stage === "datachannel") {
    summary.datachannel = "failed";
  } else if (stage === "browser-media") {
    summary.browser_media.status = "failed";
  } else if (stage === "c-media") {
    summary.c_media.status = "failed";
  }
}

function writeSummary(summaryPath, summary) {
  fs.mkdirSync(path.dirname(summaryPath), { recursive: true });
  fs.writeFileSync(summaryPath, `${JSON.stringify(redactSecrets(summary), null, 2)}\n`);
}

function printStage(name, details = "") {
  const suffix = details ? ` ${details}` : "";
  process.stdout.write(`\n=== ${name}${suffix} ===\n`);
}

module.exports = {
  createHostSummary,
  markFailure,
  printStage,
  writeSummary,
};

if (require.main === module && process.argv.includes("--self-test-redaction")) {
  const tempPath = path.join(__dirname, "artifacts", "summary-redaction-self-test.json");
  writeSummary(tempPath, {
    turn_relay: {
      urls: "turn:secret-turn.example:3478?transport=udp&username=raw-user&credential=raw-credential",
      username: "raw-user",
      credential: "raw-credential",
      password: "raw-password",
    },
  });
  const text = fs.readFileSync(tempPath, "utf8");
  const forbidden = ["secret-turn.example", "raw-user", "raw-credential", "raw-password", "username", "credential", "password"];
  if (forbidden.some((value) => text.includes(value))) {
    process.stderr.write(`summary redaction self-test failed: ${text}\n`);
    process.exit(1);
  }
  process.stdout.write("summary redaction self-test ok\n");
}
