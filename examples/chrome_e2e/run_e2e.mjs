#!/usr/bin/env node

import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, unlinkSync } from "node:fs";
import { homedir } from "node:os";
import { resolve } from "node:path";
import { spawn } from "node:child_process";
import { chromium } from "@playwright/test";
import { createSignalingServer } from "./signaling.mjs";

const repoRoot = resolve(new URL("../..", import.meta.url).pathname);
const failureLayers = ["signaling","ice","dtls","srtp","rtp","rtcp","media_file"];
const requiredCEvents = ["offer.received", "answer.sent", "ice.connected", "dtls.connected", "srtp.ready"];

function parseArgs(argv) {
  const args = {
    dryRun: argv.includes("--dry-run"),
    pageSmoke: argv.includes("--page-smoke"),
    cExampleSmoke: argv.includes("--c-example-smoke"),
    mediaFileSmoke: argv.includes("--media-file-smoke"),
    securityGateSmoke: argv.includes("--security-gate-smoke"),
    timeoutMs: 30000,
    chromeChannel: "chrome",
    outputDir: "examples/chrome_e2e/out",
    keepOpen: argv.includes("--keep-open"),
    manualSecurityOk: argv.includes("--manual-security-ok"),
  };
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--timeout-ms" && argv[i + 1]) {
      args.timeoutMs = Number.parseInt(argv[++i], 10);
    } else if (argv[i] === "--chrome-channel" && argv[i + 1]) {
      args.chromeChannel = argv[++i];
    } else if (argv[i] === "--output-dir" && argv[i + 1]) {
      args.outputDir = argv[++i];
    }
  }
  if (!Number.isFinite(args.timeoutMs) || args.timeoutMs <= 0) {
    throw new Error("--timeout-ms must be a positive integer");
  }
  return args;
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

function findChromiumExecutable() {
  const candidates = [
    process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE,
    resolve(homedir(), ".cache/ms-playwright/chromium-1217/chrome-linux64/chrome"),
  ].filter(Boolean);
  return candidates.find((candidate) => existsSync(candidate));
}

function isSecurityBackendDisabled() {
  return process.env.RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY === "OFF";
}

function browserEnv() {
  const localLib = resolve(repoRoot, ".cache/playwright-libs/root/usr/lib/x86_64-linux-gnu");
  const libraryPath = [existsSync(localLib) ? localLib : null, process.env.LD_LIBRARY_PATH]
    .filter(Boolean)
    .join(":");
  return libraryPath ? { ...process.env, LD_LIBRARY_PATH: libraryPath } : process.env;
}

function parseJsonl(text) {
  return text
    .trim()
    .split(/\n+/)
    .filter(Boolean)
    .map((line) => JSON.parse(line));
}

function latestEvents(events, count = 5) {
  return events.slice(-count).map((event) => ({
    type: event.type,
    layer: event.layer,
    status: event.status,
    event: event.event,
    pass: event.pass,
    reason: event.reason,
  }));
}

function selectFailureLayer({ cEvents = [], pageSummary = null, timedOut = false, exitCode = 0 }) {
  if (timedOut) {
    return { layer: "signaling", reason: "timeout" };
  }
  const cSummary = cEvents.find((event) => event.type === "summary");
  if (cSummary && cSummary.pass === false) {
    return {
      layer: failureLayers.includes(cSummary.layer) ? cSummary.layer : "signaling",
      reason: cSummary.reason || "c_example_failed",
    };
  }
  const cError = cEvents.find((event) => event.type === "error" && failureLayers.includes(event.layer));
  if (cError) {
    return { layer: cError.layer, reason: cError.event || "c_example_error" };
  }
  if (pageSummary?.layer && pageSummary.layer !== "none" && failureLayers.includes(pageSummary.layer)) {
    return { layer: pageSummary.layer, reason: pageSummary.reason || "page_failed" };
  }
  if (exitCode !== 0) {
    return { layer: "signaling", reason: "c_example_exit_nonzero" };
  }
  return { layer: "none", reason: "" };
}

function summarizeMediaFiles(outputDir) {
  const absoluteDir = resolve(repoRoot, outputDir);
  if (!existsSync(absoluteDir)) {
    return [];
  }
  return readdirSync(absoluteDir)
    .filter((name) => name === "received-opus.packets" || name === "received-h264.264")
    .map((name) => {
      const path = resolve(absoluteDir, name);
      return { path: `${outputDir}/${name}`, bytes: statSync(path).size };
    });
}

function removeStaleRunOutputs(outputDir) {
  for (const name of ["rtc_chrome_e2e.jsonl", "received-opus.packets", "received-h264.264"]) {
    const path = resolve(repoRoot, outputDir, name);
    if (existsSync(path)) {
      unlinkSync(path);
    }
  }
}

function waitForChildExit(child, timeoutMs) {
  return new Promise((resolveExit) => {
    let stdout = "";
    let stderr = "";
    let settled = false;
    const timer = setTimeout(() => {
      if (!settled) {
        settled = true;
        child.kill("SIGTERM");
        resolveExit({ code: null, stdout, stderr, timedOut: true });
      }
    }, timeoutMs);
    child.stdout.on("data", (chunk) => {
      stdout += chunk.toString("utf8");
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString("utf8");
    });
    child.on("close", (code) => {
      if (!settled) {
        settled = true;
        clearTimeout(timer);
        resolveExit({ code, stdout, stderr, timedOut: false });
      }
    });
  });
}

async function waitForServerReady(server) {
  await server.listen();
  return server;
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
    failureLayers,
    files: requiredFiles,
    scripts,
  };
  console.log(JSON.stringify(summary, null, 2));
  if (!ok) {
    process.exitCode = 1;
  }
}

async function pageSmoke() {
  const server = await waitForServerReady(createSignalingServer({ port: 0 }));

  let browser;
  try {
    const executablePath = findChromiumExecutable();
    browser = await chromium.launch({
      headless: true,
      executablePath,
      env: browserEnv(),
    });
    const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
    await page.addInitScript(() => {
      window.__mediaPermissionRequests = 0;
      const mediaDevices = navigator.mediaDevices ?? {};
      Object.defineProperty(navigator, "mediaDevices", {
        configurable: true,
        value: {
          ...mediaDevices,
          getUserMedia: async () => {
            window.__mediaPermissionRequests += 1;
            throw new Error("getUserMedia must not be used by Chrome E2E smoke");
          },
        },
      });
    });

    await page.goto(server.url);
    const testIds = [
      "local-video",
      "remote-video",
      "stage-signaling",
      "stage-ice",
      "stage-dtls",
      "stage-srtp",
      "stage-rtp",
      "stage-rtcp",
      "stage-media-files",
      "summary-layer",
      "candidate-count",
      "stats-frames",
    ];

    for (const testId of testIds) {
      await page.getByTestId(testId).waitFor({ state: "attached" });
    }

    const beforeRequests = await page.evaluate(() => window.__mediaPermissionRequests);
    if (beforeRequests !== 0) {
      throw new Error("unexpected permission request before Start Call");
    }

    await page.getByRole("button", { name: "Start Call" }).click();
    await page.waitForFunction(() => window.__chromeE2E?.offerCreated === true, null, { timeout: 10000 });
    await page.waitForFunction(() => {
      const stage = document.querySelector('[data-testid="stage-signaling"]');
      return stage?.dataset.state === "running" || stage?.dataset.state === "connected";
    });

    const afterRequests = await page.evaluate(() => window.__mediaPermissionRequests);
    if (afterRequests !== 0) {
      throw new Error("page requested camera or microphone permissions");
    }

    const summary = {
      ok: true,
      layer: "none",
      offerCreated: true,
      permissionRequests: afterRequests,
      url: server.url,
    };
    console.log(JSON.stringify(summary, null, 2));
  } finally {
    if (browser) {
      await browser.close();
    }
    await server.close();
  }
}

async function fullE2E(args) {
  const startedAt = Date.now();
  const binary = resolve(repoRoot, "build/rtc_chrome_e2e");
  if (!existsSync(binary)) {
    throw new Error("build/rtc_chrome_e2e is missing; run cmake --build build --target rtc_chrome_e2e");
  }

  mkdirSync(resolve(repoRoot, args.outputDir), { recursive: true });
  removeStaleRunOutputs(args.outputDir);
  const server = await waitForServerReady(createSignalingServer({ port: 0 }));
  let browser;
  let page;
  let child;
  let childResult = { code: 1, stdout: "", stderr: "", timedOut: false };
  let pageState = null;

  try {
    const executablePath = args.chromeChannel === "chromium" ? findChromiumExecutable() : findChromiumExecutable();
    browser = await chromium.launch({
      headless: true,
      executablePath,
      env: browserEnv(),
    });
    page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
    await page.goto(server.url);

    child = spawn(binary, [
      "--ws-url",
      server.wsUrl,
      "--output-dir",
      args.outputDir,
      "--timeout-ms",
      String(args.timeoutMs),
    ], {
      cwd: repoRoot,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const childExit = waitForChildExit(child, args.timeoutMs + 2000);

    await page.getByRole("button", { name: "Start Call" }).click();
    await page.waitForFunction(() => window.__chromeE2E?.offerCreated === true, null, { timeout: Math.min(args.timeoutMs, 10000) });
    await Promise.race([
      page.waitForFunction(() => {
        const stage = document.querySelector('[data-testid="stage-signaling"]');
        return stage?.dataset.state === "connected";
      }, null, { timeout: Math.min(args.timeoutMs, 10000) }),
      childExit,
    ]);

    childResult = await childExit;
    pageState = await page.evaluate(() => window.__chromeE2E).catch(() => null);
    if (args.keepOpen) {
      await page.waitForTimeout(1000);
    }
  } finally {
    if (browser && !args.keepOpen) {
      await browser.close();
    }
    await server.close();
  }

  const jsonlPath = resolve(repoRoot, args.outputDir, "rtc_chrome_e2e.jsonl");
  const cEvents = existsSync(jsonlPath) ? parseJsonl(readFileSync(jsonlPath, "utf8")) : [];
  const cSummary = cEvents.find((event) => event.type === "summary") ?? null;
  const failure = selectFailureLayer({
    cEvents,
    pageSummary: pageState?.summary,
    timedOut: childResult.timedOut,
    exitCode: childResult.code ?? 1,
  });
  const mediaFiles = summarizeMediaFiles(args.outputDir);
  const manualVlcRequired = true;
  const optionalSecurityGate =
    failure.layer === "dtls" &&
    (failure.reason === "optional_security_backend_disabled" ||
      cSummary?.reason === "optional_security_backend_disabled" ||
      isSecurityBackendDisabled());
  const pass =
    failure.layer === "none" &&
    childResult.code === 0 &&
    cSummary?.pass === true &&
    mediaFiles.length > 0 &&
    mediaFiles.every((file) => file.bytes > 0);
  const summary = {
    pass: pass || (args.manualSecurityOk && optionalSecurityGate),
    layer: failure.layer,
    reason: args.manualSecurityOk && optionalSecurityGate ? "manual_security_ok" : failure.reason,
    duration_ms: Date.now() - startedAt,
    failureLayers,
    page: pageState ?? { summary: null },
    c_example: {
      exitCode: childResult.code,
      timedOut: childResult.timedOut,
      summary: cSummary,
      latestEvents: latestEvents(cEvents),
      stderr: childResult.stderr.trim(),
    },
    media_files: mediaFiles,
    manual_vlc_required: manualVlcRequired,
  };

  console.log(JSON.stringify(summary));
  if (!summary.pass) {
    process.exitCode = 1;
  }
}

function runCommand(command, args) {
  return new Promise((resolveRun) => {
    const child = spawn(command, args, {
      cwd: repoRoot,
      stdio: ["ignore", "pipe", "pipe"],
    });
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => {
      stdout += chunk.toString("utf8");
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk.toString("utf8");
    });
    child.on("close", (code) => {
      resolveRun({ code, stdout, stderr });
    });
  });
}

function requireSource(source, needle) {
  if (!source.includes(needle)) {
    throw new Error(`missing source marker: ${needle}`);
  }
}

async function cExampleSmoke() {
  const binary = resolve(repoRoot, "build/rtc_chrome_e2e");
  if (!existsSync(binary)) {
    throw new Error("build/rtc_chrome_e2e is missing; run cmake --build build --target rtc_chrome_e2e");
  }

  const run = await runCommand(binary, [
    "--dry-run",
    "--output-dir",
    "examples/chrome_e2e/out",
  ]);
  if (run.code !== 0) {
    throw new Error(`rtc_chrome_e2e --dry-run failed: ${run.stderr || run.stdout}`);
  }

  const jsonlPath = resolve(repoRoot, "examples/chrome_e2e/out/rtc_chrome_e2e.jsonl");
  const jsonl = readFileSync(jsonlPath, "utf8").trim().split(/\n+/).map((line) => JSON.parse(line));
  const hasStarted = jsonl.some((event) => event.event === "process.started");
  const summary = jsonl.find((event) => event.type === "summary");
  if (!hasStarted || !summary?.pass) {
    throw new Error("C example JSONL missing process.started or passing summary");
  }

  const source = readFileSync(resolve(repoRoot, "examples/chrome_e2e/rtc_chrome_e2e.c"), "utf8");
  [
    "rtc_peer_connection_set_remote_description",
    "rtc_peer_connection_create_answer",
    "rtc_peer_connection_set_local_description",
    "rtc_peer_connection_add_ice_candidate",
    "rtc_peer_connection_gather_candidates",
    "rtc_peer_connection_start_connectivity_checks",
    "rtc_peer_connection_receive_datagram",
    "on_datagram",
    "memcpy",
    "signaling.connected",
  ].forEach((needle) => requireSource(source, needle));

  console.log(JSON.stringify({
    ok: true,
    layer: "none",
    binary: "build/rtc_chrome_e2e",
    jsonl: "examples/chrome_e2e/out/rtc_chrome_e2e.jsonl",
    events: jsonl.length,
  }, null, 2));
}

async function mediaFileSmoke() {
  const binary = resolve(repoRoot, "build/rtc_chrome_e2e");
  if (!existsSync(binary)) {
    throw new Error("build/rtc_chrome_e2e is missing; run cmake --build build --target rtc_chrome_e2e");
  }

  const outputDir = "examples/chrome_e2e/out/media-file-smoke";
  const run = await runCommand(binary, [
    "--dry-run",
    "--output-dir",
    outputDir,
  ]);
  if (run.code !== 0) {
    throw new Error(`rtc_chrome_e2e media-file smoke failed: ${run.stderr || run.stdout}`);
  }

  const jsonlPath = resolve(repoRoot, outputDir, "rtc_chrome_e2e.jsonl");
  const jsonl = readFileSync(jsonlPath, "utf8").trim().split(/\n+/).map((line) => JSON.parse(line));
  const summary = jsonl.find((event) => event.type === "summary");
  if (!summary?.pass || summary.layer !== "none") {
    throw new Error("media-file smoke summary did not pass");
  }
  for (const field of [
    "audio_frames_received",
    "video_frames_received",
    "audio_bytes_received",
    "video_bytes_received",
  ]) {
    if (typeof summary[field] !== "number" || summary[field] <= 0) {
      throw new Error(`media-file smoke summary missing positive ${field}`);
    }
  }

  const opusPath = resolve(repoRoot, outputDir, "received-opus.packets");
  const h264Path = resolve(repoRoot, outputDir, "received-h264.264");
  if (!existsSync(opusPath) || !existsSync(h264Path)) {
    throw new Error("media-file smoke did not create received media files");
  }

  const source = readFileSync(resolve(repoRoot, "examples/chrome_e2e/rtc_chrome_e2e.c"), "utf8");
  [
    "rtc_peer_connection_send_media_frame",
    "srtp.ready",
    "received-opus.packets",
    "received-h264.264",
    "audio_frames_received",
    "video_frames_received",
    "media_file",
  ].forEach((needle) => requireSource(source, needle));

  console.log(JSON.stringify({
    ok: true,
    layer: "none",
    outputDir,
    summary,
  }, null, 2));
}

async function securityGateSmoke() {
  const binary = resolve(repoRoot, "build/rtc_chrome_e2e");
  if (!existsSync(binary)) {
    throw new Error("build/rtc_chrome_e2e is missing; run cmake --build build --target rtc_chrome_e2e");
  }

  const outputDir = "examples/chrome_e2e/out/security-gate-smoke";
  const run = await runCommand(binary, [
    "--output-dir",
    outputDir,
    "--timeout-ms",
    "1",
  ]);
  if (run.code === 0) {
    throw new Error("security gate smoke unexpectedly passed full E2E");
  }

  const jsonlPath = resolve(repoRoot, outputDir, "rtc_chrome_e2e.jsonl");
  const jsonl = readFileSync(jsonlPath, "utf8").trim().split(/\n+/).map((line) => JSON.parse(line));
  const summary = jsonl.find((event) => event.type === "summary");
  if (!summary || summary.pass !== false || summary.layer !== "dtls" || summary.reason !== "optional_security_backend_disabled") {
    throw new Error("security gate smoke did not stop at dtls optional_security_backend_disabled");
  }

  const source = readFileSync(resolve(repoRoot, "examples/chrome_e2e/rtc_chrome_e2e.c"), "utf8");
  [
    "fingerprint",
    "RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH",
    "key_export",
    "protect_rtp",
    "unprotect_rtp",
    "protect_rtcp",
    "unprotect_rtcp",
  ].forEach((needle) => requireSource(source, needle));

  console.log(JSON.stringify({
    ok: true,
    layer: "dtls",
    reason: "optional_security_backend_disabled",
    summary,
  }, null, 2));
}

const args = parseArgs(process.argv.slice(2));

if (args.dryRun) {
  await dryRun();
} else if (args.pageSmoke) {
  await pageSmoke();
} else if (args.cExampleSmoke) {
  await cExampleSmoke();
} else if (args.mediaFileSmoke) {
  await mediaFileSmoke();
} else if (args.securityGateSmoke) {
  await securityGateSmoke();
} else {
  await fullE2E(args);
}
