const fs = require("node:fs");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
const { expect, test } = require("@playwright/test");
const { waitForBrowserInboundAudio, waitForBrowserInboundVideo } = require("./media-assertions");
const { createHostSummary, markFailure, printStage, writeSummary } = require("./summary");
const { startServer } = require("./signaling-server");

const projectRoot = path.resolve(__dirname, "..", "..");
const artifactsDir = path.join(__dirname, "artifacts");
const summaryPath = path.join(artifactsDir, "summary.json");
const answererPath = path.join(projectRoot, "build", "examples", "chrome-e2e", "mrtc_chrome_answerer");
const fixturesDir = process.env.MRTC_E2E_FIXTURES || path.join(projectRoot, "tests", "fixtures");
const pagePath = path.join(projectRoot, "examples", "chrome-e2e", "index.html");

async function waitForPageState(page, predicate, timeoutMs) {
  try {
    await page.waitForFunction(predicate, undefined, { timeout: timeoutMs });
    return { ok: true, reason: null };
  } catch (error) {
    return { ok: false, reason: error.message };
  }
}

async function sendPingPong(page) {
  const nonce = `nonce-${Date.now()}`;
  const pong = await page.evaluate(async (value) => {
    const api = window.__mrtcE2E;
    if (api.getState().dataChannelState !== "open") {
      await api.waitForEvent("datachannel.open", 10_000);
    }
    api.dataChannel.send(`ping:${value}`);
    const event = await api.waitForEvent("datachannel.message", 10_000);
    return event.fields && event.fields.data;
  }, nonce);
  if (pong !== `pong:${nonce}`) {
    throw new Error(`unexpected pong: ${pong}`);
  }
  return pong;
}

function cMediaSummary(events, kind) {
  const frames = events
    .filter((event) => event.name === "remote.media.frame" && event.fields && event.fields.kind === kind)
    .map((event) => event.fields);
  const latest = frames[frames.length - 1] || {};
  return {
    status: Number(latest.count || 0) > 0 ? "passed" : "pending",
    frames: Number(latest.count || 0),
    bytes: Number(latest.bytes || 0),
    timestamp_monotonic: latest.timestamp_monotonic !== false,
  };
}

async function waitForCMedia(page, options = {}) {
  const minVideoFrames = Number(process.env.MRTC_E2E_MIN_C_VIDEO_FRAMES || options.minVideoFrames || 2);
  const minAudioFrames = Number(process.env.MRTC_E2E_MIN_C_AUDIO_FRAMES || options.minAudioFrames || 2);

  await page.waitForFunction(({ minVideo, minAudio }) => {
    const events = window.__mrtcE2E.getState().events;
    const latestFor = (kind) => events
      .filter((event) => event.name === "remote.media.frame" && event.fields && event.fields.kind === kind)
      .map((event) => event.fields)
      .pop() || {};
    const video = latestFor("video");
    const audio = latestFor("audio");
    return Number(video.count || 0) >= minVideo &&
      Number(video.bytes || 0) > 0 &&
      video.timestamp_monotonic !== false &&
      Number(audio.count || 0) >= minAudio &&
      Number(audio.bytes || 0) > 0 &&
      audio.timestamp_monotonic !== false;
  }, { minVideo: minVideoFrames, minAudio: minAudioFrames }, { timeout: options.timeoutMs || 15_000 });

  const events = await page.evaluate(() => window.__mrtcE2E.getState().events);
  return {
    video: cMediaSummary(events, "video"),
    audio: cMediaSummary(events, "audio"),
  };
}

test("host Chrome E2E: connection, DataChannel, and bidirectional media", async ({ page }, testInfo) => {
  const startedAt = Date.now();
  const browserChannel = process.env.MRTC_E2E_BROWSER_CHANNEL || "chrome";
  const summary = createHostSummary({
    test: "host chrome e2e",
    browser_channel: browserChannel,
    system_chrome_available: fs.existsSync("/opt/google/chrome/chrome"),
    chromium_fallback: browserChannel === "chromium",
  });
  let server;

  expect(fs.existsSync(answererPath), `answerer not built at ${answererPath}`).toBeTruthy();
  expect(fs.existsSync(fixturesDir), `fixtures not found at ${fixturesDir}`).toBeTruthy();

  printStage("SIGNALING");
  server = await startServer({
    port: 0,
    answerer: answererPath,
    answererArgs: ["--fixtures", fixturesDir],
    artifactsDir: path.join(artifactsDir, "host-signaling"),
    timeoutMs: Number(process.env.MRTC_E2E_TEST_TIMEOUT_MS || 60_000),
  });

  try {
    const url = new URL(pathToFileURL(pagePath).toString());
    url.searchParams.set("ws", server.url);
    await page.goto(url.toString());

    const answerApplied = await waitForPageState(page, () => window.__mrtcE2E && window.__mrtcE2E.getState().answerApplied, 10_000);
    const candidateApplied = await waitForPageState(page, () => {
      if (!window.__mrtcE2E) {
        return false;
      }
      return window.__mrtcE2E.getState().candidatesReceived > 0;
    }, 10_000);
    if (!answerApplied.ok || !candidateApplied.ok) {
      throw new Error(answerApplied.reason || candidateApplied.reason);
    }
    summary.chrome_host.status = "signaled";

    printStage("CONNECTION");
    const connected = await waitForPageState(page, () => window.__mrtcE2E.getState().connectionState === "connected", 10_000);
    if (!connected.ok) {
      throw new Error(connected.reason);
    }
    summary.connection.status = "passed";

    printStage("DATACHANNEL");
    await sendPingPong(page);
    summary.datachannel.status = "passed";
    summary.datachannel.messages = 2;

    printStage("BROWSER MEDIA");
    await page.evaluate(() => window.__mrtcE2E.sendControlMessage({ type: "start-media" }));
    [summary.browser_media.video, summary.browser_media.audio] = await Promise.all([
      waitForBrowserInboundVideo(page, { timeoutMs: 15_000 }),
      waitForBrowserInboundAudio(page, { timeoutMs: 15_000 }),
    ]);
    summary.browser_media.status = "passed";

    printStage("C MEDIA");
    summary.c_media = await waitForCMedia(page, { timeoutMs: 15_000 });
    summary.c_media.status = "passed";
    summary.chrome_host.status = "passed";

    printStage("SUMMARY", "passed");
  } catch (error) {
    const stage =
      summary.connection.status !== "passed" ? "connection" :
      summary.datachannel.status !== "passed" ? "datachannel" :
      summary.browser_media.status !== "passed" ? "browser-media" :
      "c-media";
    markFailure(summary, stage, error.message);
    printStage("SUMMARY", `failed ${stage}: ${error.message}`);
    throw error;
  } finally {
    summary.duration_ms = Date.now() - startedAt;
    writeSummary(summaryPath, summary);
    await testInfo.attach("host-summary", { path: summaryPath, contentType: "application/json" });
    if (server) {
      server.stop();
    }
  }

  expect(summary.connection.status).toBe("passed");
  expect(summary.datachannel.status).toBe("passed");
  expect(summary.browser_media.status).toBe("passed");
  expect(summary.c_media.status).toBe("passed");
});
