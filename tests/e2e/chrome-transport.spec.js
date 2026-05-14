const fs = require("node:fs");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
const { expect, test } = require("@playwright/test");
const { startServer } = require("./signaling-server");

const projectRoot = path.resolve(__dirname, "..", "..");
const artifactsDir = path.join(__dirname, "artifacts");
const summaryPath = path.join(artifactsDir, "summary.json");
const answererPath = path.join(projectRoot, "build", "examples", "chrome-e2e", "mrtc_chrome_answerer");
const pagePath = path.join(projectRoot, "examples", "chrome-e2e", "index.html");

function writeSummary(summary) {
  fs.mkdirSync(artifactsDir, { recursive: true });
  fs.writeFileSync(summaryPath, `${JSON.stringify(summary, null, 2)}\n`);
}

async function waitForPageState(page, predicate, timeoutMs) {
  try {
    await page.waitForFunction(predicate, undefined, { timeout: timeoutMs });
    return { ok: true, reason: null };
  } catch (error) {
    return { ok: false, reason: error.message };
  }
}

test("transport smoke: signaling, connection, and datachannel stages are classified", async ({ page }, testInfo) => {
  const requireTransport = process.env.MRTC_E2E_REQUIRE_TRANSPORT === "1";
  const browserChannel = process.env.MRTC_E2E_BROWSER_CHANNEL || "chrome";
  const summary = {
    test: "transport smoke",
    browser_channel: browserChannel,
    system_chrome_available: fs.existsSync("/opt/google/chrome/chrome"),
    chromium_fallback: browserChannel === "chromium",
    signaling: "pending",
    connection: "pending",
    datachannel: "pending",
    failure_stage: null,
    failure_reason: null,
  };
  let server;

  expect(fs.existsSync(answererPath), `answerer not built at ${answererPath}`).toBeTruthy();
  server = await startServer({
    port: 0,
    answerer: answererPath,
    artifactsDir: path.join(artifactsDir, "transport-signaling"),
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
      const state = window.__mrtcE2E.getState();
      return state.candidatesReceived > 0;
    }, 10_000);

    summary.signaling = answerApplied.ok && candidateApplied.ok ? "passed" : "failed";
    if (summary.signaling !== "passed") {
      summary.failure_stage = "signaling";
      summary.failure_reason = answerApplied.reason || candidateApplied.reason;
      writeSummary(summary);
      expect(summary.signaling).toBe("passed");
      return;
    }

    const connected = await waitForPageState(page, () => {
      const state = window.__mrtcE2E.getState();
      return state.connectionState === "connected";
    }, 10_000);
    summary.connection = connected.ok ? "passed" : "blocked";

    if (connected.ok) {
      const nonce = `nonce-${Date.now()}`;
      const pong = await page.evaluate(async (value) => {
        const api = window.__mrtcE2E;
        if (api.getState().dataChannelState !== "open") {
          await api.waitForEvent("datachannel.open", 10_000);
        }
        api.dataChannel.send(`ping:${value}`);
        const event = await api.waitForEvent("datachannel.message", 10_000);
        return event.fields && event.fields.data;
      }, nonce).catch((error) => ({ error: error.message }));
      summary.datachannel = pong === `pong:${nonce}` ? "passed" : "failed";
      if (summary.datachannel !== "passed") {
        summary.failure_stage = "datachannel";
        summary.failure_reason = typeof pong === "object" && pong.error ? pong.error : `unexpected pong: ${pong}`;
      }
    } else {
      summary.datachannel = "blocked";
      summary.failure_stage = "connection";
      summary.failure_reason = connected.reason;
    }

    writeSummary(summary);
    await testInfo.attach("transport-summary", { path: summaryPath, contentType: "application/json" });

    expect(summary.signaling).toBe("passed");
    if (requireTransport) {
      expect(summary.connection).toBe("passed");
      expect(summary.datachannel).toBe("passed");
    } else {
      expect(["passed", "blocked"]).toContain(summary.connection);
      expect(["passed", "blocked"]).toContain(summary.datachannel);
    }
  } finally {
    if (server) {
      server.stop();
    }
  }
});
