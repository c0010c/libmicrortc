"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { redactSecrets } = require("./turn-config");

function stage(status = "not-run", fields = {}) {
  return { status, ...fields };
}

function deepMerge(base, fields) {
  const output = { ...base };
  for (const [key, value] of Object.entries(fields || {})) {
    if (
      value &&
      typeof value === "object" &&
      !Array.isArray(value) &&
      output[key] &&
      typeof output[key] === "object" &&
      !Array.isArray(output[key])
    ) {
      output[key] = deepMerge(output[key], value);
    } else {
      output[key] = value;
    }
  }
  return output;
}

function statusOf(value, fallback = "not-run") {
  if (value && typeof value === "object" && typeof value.status === "string") {
    return value.status;
  }
  if (typeof value === "string") {
    return value;
  }
  return fallback;
}

function setStatus(target, key, status, fields = {}) {
  const existing = target[key];
  target[key] = existing && typeof existing === "object" && !Array.isArray(existing)
    ? { ...existing, status, ...fields }
    : { status, ...fields };
}

function createV1Summary(fields = {}) {
  return {
    phase: "06-chrome-e2e",
    build: stage("not-run"),
    ctest: stage("not-run"),
    chrome_host: stage("pending"),
    chrome_turn: stage("not-run", { skipped: true }),
    connection: stage("pending"),
    datachannel: stage("pending", { messages: 0 }),
    browser_media: {
      status: "pending",
      video: stage("pending", { frames: 0, bytes: 0 }),
      audio: stage("pending", { frames: 0, bytes: 0 }),
    },
    c_media: {
      status: "pending",
      video: stage("pending", { frames: 0, bytes: 0 }),
      audio: stage("pending", { frames: 0, bytes: 0 }),
    },
    turn_relay: stage("not-run", { skipped: true }),
    failure_stage: null,
    failure_reason: null,
    duration_ms: 0,
    ...deepMerge({}, fields),
  };
}

function createHostSummary(fields = {}) {
  return deepMerge(createV1Summary(), fields);
}

function markFailure(summary, stage, reason) {
  summary.failure_stage = stage;
  summary.failure_reason = reason || "unknown failure";
  if (stage === "connection") {
    setStatus(summary, "connection", "failed");
  } else if (stage === "datachannel") {
    setStatus(summary, "datachannel", "failed");
  } else if (stage === "browser-media") {
    summary.browser_media.status = "failed";
    if (summary.browser_media.video.status !== "passed") {
      summary.browser_media.video.status = "failed";
    }
    if (summary.browser_media.audio.status !== "passed") {
      summary.browser_media.audio.status = "failed";
    }
  } else if (stage === "c-media") {
    summary.c_media.status = "failed";
    if (summary.c_media.video.status !== "passed") {
      summary.c_media.video.status = "failed";
    }
    if (summary.c_media.audio.status !== "passed") {
      summary.c_media.audio.status = "failed";
    }
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

function readSummary(summaryPath) {
  return JSON.parse(fs.readFileSync(summaryPath, "utf8"));
}

function normalizeBrowserMedia(value) {
  const media = value && typeof value === "object" ? value : {};
  return {
    status: media.status || (
      statusOf(media.video, "pending") === "passed" && statusOf(media.audio, "pending") === "passed"
        ? "passed"
        : "pending"
    ),
    video: { frames: 0, bytes: 0, ...(media.video || {}), status: statusOf(media.video, "pending") },
    audio: { frames: 0, bytes: 0, ...(media.audio || {}), status: statusOf(media.audio, "pending") },
  };
}

function normalizeCMedia(value) {
  const media = normalizeBrowserMedia(value);
  media.video.frames = Number(media.video.frames || 0);
  media.video.bytes = Number(media.video.bytes || 0);
  media.audio.frames = Number(media.audio.frames || 0);
  media.audio.bytes = Number(media.audio.bytes || 0);
  return media;
}

function normalizeV1Summary(input = {}) {
  const summary = deepMerge(createV1Summary(), input);
  summary.build = { ...(typeof summary.build === "object" ? summary.build : {}), status: statusOf(summary.build) };
  summary.ctest = { ...(typeof summary.ctest === "object" ? summary.ctest : {}), status: statusOf(summary.ctest) };
  summary.chrome_host = { ...(typeof summary.chrome_host === "object" ? summary.chrome_host : {}), status: statusOf(summary.chrome_host, "pending") };
  summary.chrome_turn = { ...(typeof summary.chrome_turn === "object" ? summary.chrome_turn : {}), status: statusOf(summary.chrome_turn, "not-run") };
  summary.chrome_turn.skipped = summary.chrome_turn.status === "skipped";
  summary.connection = { ...(typeof summary.connection === "object" ? summary.connection : {}), status: statusOf(summary.connection, "pending") };
  summary.datachannel = { messages: 0, ...(typeof summary.datachannel === "object" ? summary.datachannel : {}), status: statusOf(summary.datachannel, "pending") };
  summary.browser_media = normalizeBrowserMedia(summary.browser_media);
  summary.c_media = normalizeCMedia(summary.c_media);
  summary.turn_relay = { ...(typeof summary.turn_relay === "object" ? summary.turn_relay : {}), status: statusOf(summary.turn_relay, "not-run") };
  summary.turn_relay.skipped = summary.turn_relay.status === "skipped";
  summary.duration_ms = Number(summary.duration_ms || 0);
  return redactSecrets(summary);
}

function applyStage(summary, stageName, status, durationMs, reason) {
  const fields = { duration_ms: Number(durationMs || 0) };
  if (stageName === "build") {
    setStatus(summary, "build", status, fields);
  } else if (stageName === "ctest") {
    setStatus(summary, "ctest", status, fields);
  } else if (stageName === "chrome_host") {
    setStatus(summary, "chrome_host", status, fields);
  } else if (stageName === "chrome_turn") {
    setStatus(summary, "chrome_turn", status, status === "skipped" ? { ...fields, skipped: true } : { ...fields, skipped: false });
    if (status === "skipped") {
      summary.turn_relay = { status: "skipped", skipped: true };
    }
  }
  if (status === "failed") {
    summary.failure_stage = stageName;
    summary.failure_reason = reason || "unknown failure";
  }
}

function mergeE2eSummary(summary, stageName, e2eSummary) {
  const normalized = normalizeV1Summary(e2eSummary);
  summary.connection = normalized.connection;
  summary.datachannel = normalized.datachannel;
  summary.browser_media = normalized.browser_media;
  summary.c_media = normalized.c_media;
  if (stageName === "chrome_turn") {
    summary.chrome_turn = normalized.chrome_turn;
    summary.turn_relay = normalized.turn_relay;
  } else {
    summary.chrome_host = normalized.chrome_host;
  }
  if (normalized.failure_stage) {
    summary.failure_stage = normalized.failure_stage;
    summary.failure_reason = normalized.failure_reason;
  }
}

function finalizeSummary(summaryPath, startedAtMs) {
  const summary = normalizeV1Summary(readSummary(summaryPath));
  const started = Number(startedAtMs || 0);
  if (started > 0) {
    summary.duration_ms = Date.now() - started;
  }
  writeSummary(summaryPath, summary);
}

module.exports = {
  createHostSummary,
  createV1Summary,
  mergeE2eSummary,
  markFailure,
  normalizeV1Summary,
  printStage,
  writeSummary,
};

function main(argv) {
  if (argv.includes("--self-test-redaction")) {
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
    return;
  }

  const command = argv[0];
  if (command === "--init-v1") {
    const summaryPath = argv[1];
    const turnRequested = argv[2] === "true";
    const summary = createV1Summary({
      chrome_turn: turnRequested ? { status: "pending", skipped: false } : { status: "skipped", skipped: true },
      turn_relay: turnRequested ? { status: "pending", skipped: false } : { status: "skipped", skipped: true },
    });
    writeSummary(summaryPath, summary);
    return;
  }
  if (command === "--stage") {
    const [summaryPath, stageName, status, durationMs, reason] = argv.slice(1);
    const summary = normalizeV1Summary(readSummary(summaryPath));
    applyStage(summary, stageName, status, durationMs, reason);
    writeSummary(summaryPath, summary);
    return;
  }
  if (command === "--merge-e2e") {
    const [summaryPath, stageName, e2ePath] = argv.slice(1);
    const summary = normalizeV1Summary(readSummary(summaryPath));
    mergeE2eSummary(summary, stageName, readSummary(e2ePath));
    writeSummary(summaryPath, summary);
    return;
  }
  if (command === "--finalize") {
    finalizeSummary(argv[1], argv[2]);
    return;
  }
}

if (require.main === module) {
  main(process.argv.slice(2));
}
