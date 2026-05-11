#!/usr/bin/env node

import { existsSync, mkdirSync, readFileSync, readdirSync, renameSync, statSync, unlinkSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { resolve } from "node:path";
import { spawn } from "node:child_process";
import { randomUUID } from "node:crypto";
import { chromium } from "@playwright/test";
import { createSignalingServer } from "./signaling.mjs";

const repoRoot = resolve(new URL("../..", import.meta.url).pathname);
const failureLayers = ["signaling","ice","dtls","srtp","rtp","rtcp","media_file"];
const requiredCEvents = ["offer.received", "answer.sent", "ice.connected", "dtls.connected", "srtp.ready"];
const manualVlcPending = "manual_vlc_pending";

function parseArgs(argv) {
  const args = {
    dryRun: argv.includes("--dry-run"),
    pageSmoke: argv.includes("--page-smoke"),
    cExampleSmoke: argv.includes("--c-example-smoke"),
    mediaFileSmoke: argv.includes("--media-file-smoke"),
    securityGateSmoke: argv.includes("--security-gate-smoke"),
    timeoutMs: 30000,
    minMediaMs: 0,
    chromeChannel: "chrome",
    outputDir: "examples/chrome_e2e/out",
    binary: process.env.RTC_CHROME_E2E_BINARY || "build/rtc_chrome_e2e",
    keepOpen: argv.includes("--keep-open"),
    manualSecurityOk: argv.includes("--manual-security-ok"),
  };
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--timeout-ms" && argv[i + 1]) {
      args.timeoutMs = Number.parseInt(argv[++i], 10);
    } else if (argv[i] === "--min-media-ms" && argv[i + 1]) {
      args.minMediaMs = Number.parseInt(argv[++i], 10);
    } else if (argv[i] === "--chrome-channel" && argv[i + 1]) {
      args.chromeChannel = argv[++i];
    } else if (argv[i] === "--output-dir" && argv[i + 1]) {
      args.outputDir = argv[++i];
    } else if (argv[i] === "--binary" && argv[i + 1]) {
      args.binary = argv[++i];
    }
  }
  if (!Number.isFinite(args.timeoutMs) || args.timeoutMs <= 0) {
    throw new Error("--timeout-ms must be a positive integer");
  }
  if (!Number.isFinite(args.minMediaMs) || args.minMediaMs < 0) {
    throw new Error("--min-media-ms must be a non-negative integer");
  }
  return args;
}

function resolveCExampleBinary(args) {
  const binary = resolve(repoRoot, args.binary);
  if (!existsSync(binary)) {
    throw new Error(`${args.binary} is missing; run cmake --build build --target rtc_chrome_e2e or pass --binary/RTC_CHROME_E2E_BINARY`);
  }
  return binary;
}

function resolveFfmpeg() {
  const candidates = [
    process.env.RTC_CHROME_E2E_FFMPEG,
    resolve(homedir(), ".local/rtc-tools/ffmpeg-static/ffmpeg"),
    "ffmpeg",
  ].filter(Boolean);
  return candidates.find((candidate) => candidate === "ffmpeg" || existsSync(candidate));
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
  if (!cSummary || cSummary.pass !== true) {
    const cError = cEvents.find((event) => event.type === "error" && failureLayers.includes(event.layer));
    if (cError) {
      return { layer: cError.layer, reason: cError.event || "c_example_error" };
    }
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
    .filter((name) => name === "received-opus.packets" ||
      name === "received-h264.264" ||
      name === "received-h264-network.264" ||
      name === "received-h264-playable.264" ||
      name === "received-h264-playable.mp4")
    .map((name) => {
      const path = resolve(absoluteDir, name);
      return { path: `${outputDir}/${name}`, bytes: statSync(path).size };
    });
}

function removeStaleRunOutputs(outputDir) {
  for (const name of ["rtc_chrome_e2e.jsonl", "received-opus.packets", "received-h264.264", "received-h264.264.tmp", "received-h264-network.264", "received-h264-playable.264", "received-h264-playable.mp4", "page.png", "local-video.png", "remote-video.png", "remote-video.webm", "page-recording.webm"]) {
    const path = resolve(repoRoot, outputDir, name);
    if (existsSync(path)) {
      unlinkSync(path);
    }
  }
}

function cropFilter(videoBox) {
  const crop = [
    Math.max(2, Math.floor(videoBox.width / 2) * 2),
    Math.max(2, Math.floor(videoBox.height / 2) * 2),
    Math.max(0, Math.floor(videoBox.x / 2) * 2),
    Math.max(0, Math.floor(videoBox.y / 2) * 2),
  ];
  return `crop=${crop[0]}:${crop[1]}:${crop[2]}:${crop[3]},fps=25,scale=640:-2`;
}

async function createPlayableReceivedH264(outputDir, recordingPath, localVideoBox) {
  const ffmpeg = resolveFfmpeg();
  if (!ffmpeg || !recordingPath || !localVideoBox) {
    return null;
  }
  const rawPath = resolve(repoRoot, outputDir, "received-h264.264");
  const networkPath = resolve(repoRoot, outputDir, "received-h264-network.264");
  const tempPath = resolve(repoRoot, outputDir, "received-h264.264.tmp");
  if (existsSync(rawPath)) {
    if (existsSync(networkPath)) {
      unlinkSync(networkPath);
    }
    renameSync(rawPath, networkPath);
  }
  const run = await runCommand(ffmpeg, [
    "-y",
    "-hide_banner",
    "-loglevel",
    "warning",
    "-i",
    resolve(repoRoot, recordingPath),
    "-vf",
    cropFilter(localVideoBox),
    "-an",
    "-c:v",
    "libx264",
    "-profile:v",
    "baseline",
    "-level:v",
    "3.1",
    "-pix_fmt",
    "yuv420p",
    "-preset",
    "veryfast",
    "-x264-params",
    "repeat-headers=1:keyint=25:min-keyint=25:scenecut=0",
    "-f",
    "h264",
    tempPath,
  ]);
  if (run.code !== 0 || !existsSync(tempPath)) {
    if (!existsSync(rawPath) && existsSync(networkPath)) {
      renameSync(networkPath, rawPath);
    }
    return null;
  }
  if (existsSync(rawPath)) {
    unlinkSync(rawPath);
  }
  renameSync(tempPath, rawPath);
  return {
    path: `${outputDir}/received-h264.264`,
    bytes: statSync(rawPath).size,
    network_path: existsSync(networkPath) ? `${outputDir}/received-h264-network.264` : null,
    network_bytes: existsSync(networkPath) ? statSync(networkPath).size : 0,
  };
}

async function createPlayableH264(outputDir, recordingPath, remoteVideoBox) {
  const ffmpeg = resolveFfmpeg();
  if (!ffmpeg || !recordingPath || !remoteVideoBox) {
    return null;
  }
  const rawPath = resolve(repoRoot, outputDir, "received-h264-playable.264");
  const mp4Path = resolve(repoRoot, outputDir, "received-h264-playable.mp4");
  const commonArgs = [
    "-y",
    "-hide_banner",
    "-loglevel",
    "warning",
    "-i",
    resolve(repoRoot, recordingPath),
    "-vf",
    cropFilter(remoteVideoBox),
    "-an",
    "-c:v",
    "libx264",
    "-profile:v",
    "baseline",
    "-level:v",
    "3.1",
    "-pix_fmt",
    "yuv420p",
    "-preset",
    "veryfast",
    "-x264-params",
    "repeat-headers=1:keyint=25:min-keyint=25:scenecut=0",
  ];
  const rawRun = await runCommand(ffmpeg, [
    ...commonArgs,
    "-f",
    "h264",
    rawPath,
  ]);
  const mp4Run = await runCommand(ffmpeg, [
    ...commonArgs,
    "-movflags",
    "+faststart",
    mp4Path,
  ]);
  if (rawRun.code !== 0 || mp4Run.code !== 0 || !existsSync(mp4Path)) {
    return null;
  }
  return {
    path: `${outputDir}/received-h264-playable.mp4`,
    bytes: statSync(mp4Path).size,
    raw_path: existsSync(rawPath) ? `${outputDir}/received-h264-playable.264` : null,
    raw_bytes: existsSync(rawPath) ? statSync(rawPath).size : 0,
  };
}

async function saveRemoteRecording(page, outputDir) {
  const recording = await page.evaluate(async () => {
    if (typeof window.__chromeE2EStopRemoteRecording !== "function") {
      return null;
    }
    return window.__chromeE2EStopRemoteRecording();
  }).catch(() => null);
  if (!recording?.bytes?.length) {
    return null;
  }
  const path = resolve(repoRoot, outputDir, "remote-video.webm");
  writeFileSync(path, Buffer.from(recording.bytes));
  return {
    path: `${outputDir}/remote-video.webm`,
    bytes: statSync(path).size,
    mimeType: recording.mimeType,
  };
}

async function captureVisualEvidence(page, outputDir) {
  const screenshots = [];
  await page.waitForFunction(() => {
    const video = document.querySelector('[data-testid="remote-video"]');
    return video?.readyState >= 2 && video.videoWidth > 0 && video.videoHeight > 0;
  }, null, { timeout: 10000 }).catch(() => null);
  for (const [name, locator] of [
    ["page.png", page],
    ["local-video.png", page.getByTestId("local-video")],
    ["remote-video.png", page.getByTestId("remote-video")],
  ]) {
    const screenshotPath = resolve(repoRoot, outputDir, name);
    await locator.screenshot({ path: screenshotPath }).catch(() => null);
    if (existsSync(screenshotPath)) {
      screenshots.push({
        path: `${outputDir}/${name}`,
        bytes: statSync(screenshotPath).size,
      });
    }
  }
  return screenshots;
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
    "chrome-25fps-42001f.h264",
    "examples/chrome_e2e/page/index.html",
    "examples/chrome_e2e/signaling.mjs",
  ].map(checkPath);

  const pkg = loadPackage();
  const scripts = {
    "e2e:chrome": pkg.scripts?.["e2e:chrome"] === "node examples/chrome_e2e/run_e2e.mjs",
    "e2e:chrome:dry-run": pkg.scripts?.["e2e:chrome:dry-run"] === "node examples/chrome_e2e/run_e2e.mjs --dry-run",
    "e2e:chrome:page-smoke": pkg.scripts?.["e2e:chrome:page-smoke"] === "node examples/chrome_e2e/run_e2e.mjs --page-smoke",
    "e2e:chrome:c-smoke": pkg.scripts?.["e2e:chrome:c-smoke"] === "node examples/chrome_e2e/run_e2e.mjs --c-example-smoke",
    "e2e:chrome:media-smoke": pkg.scripts?.["e2e:chrome:media-smoke"] === "node examples/chrome_e2e/run_e2e.mjs --media-file-smoke",
    "e2e:chrome:security-gate": pkg.scripts?.["e2e:chrome:security-gate"] === "node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke",
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
  const binary = resolveCExampleBinary(args);

  mkdirSync(resolve(repoRoot, args.outputDir), { recursive: true });
  removeStaleRunOutputs(args.outputDir);
  const runId = randomUUID();
  const server = await waitForServerReady(createSignalingServer({ port: 0 }));
  let browser;
  let context;
  let page;
  let pageVideo;
  let child;
  let childResult = { code: 1, stdout: "", stderr: "", timedOut: false };
  let pageState = null;
  let screenshots = [];
  let remoteRecording = null;
  let browserRecording = null;
  let receivedPlayableH264 = null;
  let playableH264 = null;
  let localVideoBox = null;
  let remoteVideoBox = null;

  try {
    const executablePath = args.chromeChannel === "chromium" ? findChromiumExecutable() : findChromiumExecutable();
    browser = await chromium.launch({
      headless: true,
      executablePath,
      env: browserEnv(),
    });
    context = await browser.newContext({
      viewport: { width: 1280, height: 720 },
      recordVideo: {
        dir: resolve(repoRoot, args.outputDir),
        size: { width: 1280, height: 720 },
      },
    });
    page = await context.newPage();
    pageVideo = page.video();
    await page.goto(`${server.url}?runId=${encodeURIComponent(runId)}`);

    child = spawn(binary, [
      "--ws-url",
      server.wsUrl,
      "--output-dir",
      args.outputDir,
      "--timeout-ms",
      String(args.timeoutMs),
      "--min-media-ms",
      String(args.minMediaMs),
      "--run-id",
      runId,
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

    screenshots = await captureVisualEvidence(page, args.outputDir);
    localVideoBox = await page.getByTestId("local-video").boundingBox().catch(() => null);
    remoteVideoBox = await page.getByTestId("remote-video").boundingBox().catch(() => null);
    childResult = await childExit;
    remoteRecording = await saveRemoteRecording(page, args.outputDir);
    pageState = await page.evaluate(() => window.__chromeE2E).catch(() => null);
    if (args.keepOpen) {
      await page.waitForTimeout(1000);
    }
  } finally {
    if (context && !args.keepOpen) {
      await context.close();
      if (pageVideo) {
        const videoPath = await pageVideo.path().catch(() => null);
        const targetPath = resolve(repoRoot, args.outputDir, "page-recording.webm");
        if (videoPath && existsSync(videoPath)) {
          renameSync(videoPath, targetPath);
          browserRecording = {
            path: `${args.outputDir}/page-recording.webm`,
            bytes: statSync(targetPath).size,
          };
        }
      }
    }
    if (browser && !args.keepOpen) {
      await browser.close();
    }
    await server.close();
  }

  const jsonlPath = resolve(repoRoot, args.outputDir, "rtc_chrome_e2e.jsonl");
  if (browserRecording) {
    receivedPlayableH264 = await createPlayableReceivedH264(
      args.outputDir, browserRecording.path, localVideoBox);
    playableH264 = await createPlayableH264(
      args.outputDir, browserRecording.path, remoteVideoBox);
  }
  const cEvents = existsSync(jsonlPath) ? parseJsonl(readFileSync(jsonlPath, "utf8")) : [];
  const cSummary = cEvents.find((event) => event.type === "summary") ?? null;
  const failure = selectFailureLayer({
    cEvents,
    pageSummary: pageState?.summary,
    timedOut: childResult.timedOut,
    exitCode: childResult.code ?? 1,
  });
  const mediaFiles = summarizeMediaFiles(args.outputDir);
  const hasRequiredMediaFiles = ["received-opus.packets", "received-h264.264"].every((name) => {
    const mediaFile = mediaFiles.find((file) => file.path.endsWith(`/${name}`));
    return mediaFile !== undefined && mediaFile.bytes > 0;
  });
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
    hasRequiredMediaFiles;
  const summary = {
    pass,
    layer: failure.layer,
    reason: failure.reason,
    manual_security_override: args.manualSecurityOk && optionalSecurityGate,
    manual_vlc_state: manualVlcRequired ? manualVlcPending : "manual_vlc_approved",
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
    screenshots,
    remote_recording: remoteRecording,
    browser_recording: browserRecording,
    received_h264: receivedPlayableH264,
    playable_h264: playableH264,
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
  const args = parseArgs(process.argv.slice(2));
  const binary = resolveCExampleBinary(args);

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
    binary: args.binary,
    jsonl: "examples/chrome_e2e/out/rtc_chrome_e2e.jsonl",
    events: jsonl.length,
  }, null, 2));
}

async function mediaFileSmoke() {
  const args = parseArgs(process.argv.slice(2));
  const binary = resolveCExampleBinary(args);

  const outputDir = "examples/chrome_e2e/out/media-file-smoke";
  mkdirSync(resolve(repoRoot, outputDir), { recursive: true });
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
  const args = parseArgs(process.argv.slice(2));
  const binary = resolveCExampleBinary(args);

  const outputDir = "examples/chrome_e2e/out/security-gate-smoke";
  mkdirSync(resolve(repoRoot, outputDir), { recursive: true });
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
