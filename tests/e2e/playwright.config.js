const path = require("node:path");
const { defineConfig } = require("@playwright/test");

const artifactsDir = path.join(__dirname, "artifacts");
const requestedChannel = process.env.MRTC_E2E_BROWSER_CHANNEL || "chrome";
const channel = requestedChannel === "chromium" ? undefined : requestedChannel;

module.exports = defineConfig({
  testDir: __dirname,
  timeout: Number(process.env.MRTC_E2E_TEST_TIMEOUT_MS || 60_000),
  expect: {
    timeout: Number(process.env.MRTC_E2E_EXPECT_TIMEOUT_MS || 10_000),
  },
  workers: process.env.CI ? 1 : undefined,
  outputDir: path.join(artifactsDir, "test-results"),
  reporter: [
    ["list"],
    ["json", { outputFile: path.join(artifactsDir, "playwright-report.json") }],
    ["html", { outputFolder: path.join(artifactsDir, "html-report"), open: "never" }],
  ],
  projects: [
    {
      name: "chrome-host",
      use: {
        browserName: "chromium",
        channel,
        headless: process.env.MRTC_E2E_HEADLESS !== "0",
        trace: "retain-on-failure",
        video: "retain-on-failure",
      },
    },
    {
      name: "chrome-turn",
      use: {
        browserName: "chromium",
        channel,
        headless: process.env.MRTC_E2E_HEADLESS !== "0",
        trace: "retain-on-failure",
        video: "retain-on-failure",
      },
    },
  ],
});
