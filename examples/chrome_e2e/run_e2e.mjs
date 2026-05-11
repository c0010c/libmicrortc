#!/usr/bin/env node

import { existsSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { resolve } from "node:path";
import { spawn } from "node:child_process";
import { chromium } from "@playwright/test";
import { createSignalingServer } from "./signaling.mjs";

const repoRoot = resolve(new URL("../..", import.meta.url).pathname);

function parseArgs(argv) {
  return {
    dryRun: argv.includes("--dry-run"),
    pageSmoke: argv.includes("--page-smoke"),
    cExampleSmoke: argv.includes("--c-example-smoke"),
    mediaFileSmoke: argv.includes("--media-file-smoke"),
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

function findChromiumExecutable() {
  const candidates = [
    process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE,
    resolve(homedir(), ".cache/ms-playwright/chromium-1217/chrome-linux64/chrome"),
  ].filter(Boolean);
  return candidates.find((candidate) => existsSync(candidate));
}

function browserEnv() {
  const localLib = resolve(repoRoot, ".cache/playwright-libs/root/usr/lib/x86_64-linux-gnu");
  const libraryPath = [existsSync(localLib) ? localLib : null, process.env.LD_LIBRARY_PATH]
    .filter(Boolean)
    .join(":");
  return libraryPath ? { ...process.env, LD_LIBRARY_PATH: libraryPath } : process.env;
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
  const server = createSignalingServer({ port: 0 });
  await server.listen();

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

const args = parseArgs(process.argv.slice(2));

if (args.dryRun) {
  await dryRun();
} else if (args.pageSmoke) {
  await pageSmoke();
} else if (args.cExampleSmoke) {
  await cExampleSmoke();
} else if (args.mediaFileSmoke) {
  await mediaFileSmoke();
} else {
  await dryRun();
}
