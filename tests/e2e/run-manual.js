#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const http = require("node:http");
const os = require("node:os");
const path = require("node:path");
const { startServer } = require("./signaling-server");

const projectRoot = path.resolve(__dirname, "..", "..");

function usage() {
  return `Usage: node tests/e2e/run-manual.js [options]

Options:
  --page <path>             Browser page to load. Default: examples/chrome-e2e/index.html
  --answerer <path>         C answerer executable path. Default: build/examples/chrome-e2e/mrtc_chrome_answerer
  --fixtures <path>         H264/Opus fixture directory. Default: tests/fixtures
  --ice-config <path>       Optional local ICE server config for the C answerer.
  --host <host>             HTTP/WebSocket listen host. Default: 0.0.0.0.
  --public-host <host>      Hostname/IP printed in the browser URL. Default: first non-loopback IPv4.
  --port <port>             Local WebSocket port. Default: 0.
  --http-port <port>        Local HTTP port. Default: 0.
  --artifacts-dir <path>    Directory for signaling artifacts. Default: tests/e2e/artifacts/manual
  --timeout-ms <ms>         Server timeout. Default: 600000.
  --help                    Show this help.
`;
}

function parseArgs(argv) {
  const options = {
    page: path.join(projectRoot, "examples", "chrome-e2e", "index.html"),
    answerer: path.join(projectRoot, "build", "examples", "chrome-e2e", "mrtc_chrome_answerer"),
    fixtures: path.join(projectRoot, "tests", "fixtures"),
    iceConfig: null,
    host: "0.0.0.0",
    publicHost: null,
    port: 0,
    httpPort: 0,
    artifactsDir: path.join(projectRoot, "tests", "e2e", "artifacts", "manual"),
    timeoutMs: 600_000,
    help: false,
  };

  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--page") {
      options.page = path.resolve(argv[++i]);
    } else if (arg === "--answerer") {
      options.answerer = path.resolve(argv[++i]);
    } else if (arg === "--fixtures") {
      options.fixtures = path.resolve(argv[++i]);
    } else if (arg === "--ice-config") {
      options.iceConfig = path.resolve(argv[++i]);
    } else if (arg === "--host") {
      options.host = argv[++i];
    } else if (arg === "--public-host") {
      options.publicHost = argv[++i];
    } else if (arg === "--port") {
      options.port = Number(argv[++i]);
    } else if (arg === "--http-port") {
      options.httpPort = Number(argv[++i]);
    } else if (arg === "--artifacts-dir") {
      options.artifactsDir = path.resolve(argv[++i]);
    } else if (arg === "--timeout-ms") {
      options.timeoutMs = Number(argv[++i]);
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }

  if (!Number.isInteger(options.port) || options.port < 0 || options.port > 65535) {
    throw new Error("--port must be an integer between 0 and 65535");
  }
  if (!Number.isInteger(options.httpPort) || options.httpPort < 0 || options.httpPort > 65535) {
    throw new Error("--http-port must be an integer between 0 and 65535");
  }
  if (!Number.isInteger(options.timeoutMs) || options.timeoutMs <= 0) {
    throw new Error("--timeout-ms must be a positive integer");
  }
  return options;
}

function requirePath(filePath, label) {
  if (!fs.existsSync(filePath)) {
    throw new Error(`${label} not found: ${filePath}`);
  }
}

function detectPublicHost() {
  const interfaces = os.networkInterfaces();
  for (const entries of Object.values(interfaces)) {
    for (const item of entries || []) {
      if (item.family === "IPv4" && !item.internal && item.address) {
        return item.address;
      }
    }
  }
  return "127.0.0.1";
}

function contentType(filePath) {
  if (filePath.endsWith(".html")) {
    return "text/html; charset=utf-8";
  }
  if (filePath.endsWith(".js")) {
    return "application/javascript; charset=utf-8";
  }
  if (filePath.endsWith(".css")) {
    return "text/css; charset=utf-8";
  }
  return "application/octet-stream";
}

function startHttpServer(options) {
  const root = path.dirname(options.page);
  const server = http.createServer((request, response) => {
    const parsed = new URL(request.url, `http://${request.headers.host || "localhost"}`);
    const pathname = parsed.pathname === "/" ? "/index.html" : parsed.pathname;
    const requested = path.resolve(root, `.${decodeURIComponent(pathname)}`);

    if (!requested.startsWith(root + path.sep) && requested !== root) {
      response.writeHead(403);
      response.end("forbidden\n");
      return;
    }
    fs.readFile(requested, (error, data) => {
      if (error) {
        response.writeHead(404);
        response.end("not found\n");
        return;
      }
      response.writeHead(200, { "content-type": contentType(requested) });
      response.end(data);
    });
  });

  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(options.httpPort, options.host, () => {
      server.off("error", reject);
      resolve(server);
    });
  });
}

async function run(options) {
  requirePath(options.page, "page");
  requirePath(options.answerer, "answerer");
  requirePath(options.fixtures, "fixtures");
  if (options.iceConfig) {
    requirePath(options.iceConfig, "ice config");
  }

  const answererArgs = ["--fixtures", options.fixtures];
  if (options.iceConfig) {
    answererArgs.push("--ice-config", options.iceConfig);
  }

  const publicHost = options.publicHost || detectPublicHost();
  const httpServer = await startHttpServer(options);
  const server = await startServer({
    host: options.host,
    publicHost,
    port: options.port,
    answerer: options.answerer,
    answererArgs,
    answererEnv: {
      MRTC_ICE_BIND_IP: publicHost,
      MRTC_ICE_ANNOUNCE_IP: publicHost,
    },
    artifactsDir: options.artifactsDir,
    timeoutMs: options.timeoutMs,
  });

  const httpAddress = httpServer.address();
  const url = new URL(`http://${publicHost}:${httpAddress.port}/index.html`);
  url.searchParams.set("manual", "1");
  url.searchParams.set("ws", server.url);

  process.stdout.write("\nManual Chrome E2E is ready for host Chrome.\n");
  process.stdout.write(`Open: ${url.toString()}\n`);
  process.stdout.write("Stop: Ctrl-C\n\n");
  process.stdout.write(`${JSON.stringify({ stage: "manual", status: "ready", url: url.toString(), ws: server.url, host: publicHost })}\n`);
  const stop = server.stop;
  server.stop = () => {
    httpServer.close();
    stop();
  };
  process.stdin.resume();
  return server;
}

async function main() {
  try {
    const options = parseArgs(process.argv);
    if (options.help) {
      process.stdout.write(usage());
      return;
    }
    await run(options);
  } catch (error) {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main();
}

module.exports = {
  parseArgs,
  run,
  usage,
};
