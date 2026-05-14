"use strict";

const fs = require("node:fs");
const path = require("node:path");

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
  fs.writeFileSync(summaryPath, `${JSON.stringify(summary, null, 2)}\n`);
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
