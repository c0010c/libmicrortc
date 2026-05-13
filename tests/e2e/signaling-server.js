#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const readline = require("node:readline");
const { spawn } = require("node:child_process");
const { WebSocket, WebSocketServer } = require("ws");

const MESSAGE_TYPES = new Set(["hello", "offer", "answer", "candidate", "event", "done", "error", "start-media", "stop"]);
const ANSWERER_INPUT_TYPES = new Set(["hello", "offer", "candidate", "start-media", "stop"]);
const SECRET_KEYS = new Set(["credential", "password", "username"]);

function usage() {
  return `Usage: node tests/e2e/signaling-server.js [options]

Options:
  --port <port>             Local WebSocket port. Use 0 for an ephemeral port.
  --answerer <path>         C answerer executable path. Messages bridge over JSON lines on stdin/stdout.
  --artifacts-dir <path>    Directory for summary artifacts. Default: tests/e2e/artifacts/signaling
  --timeout-ms <ms>         Stop with an error after this timeout. Default: 60000
  --self-test-redaction     Run redaction checks and exit.
  --help                    Show this help.

Message types: hello, offer, answer, candidate, event, done, error.
`;
}

function parseArgs(argv) {
  const options = {
    port: 0,
    answerer: null,
    artifactsDir: path.join(__dirname, "artifacts", "signaling"),
    timeoutMs: 60_000,
    help: false,
    selfTestRedaction: false,
  };

  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--self-test-redaction") {
      options.selfTestRedaction = true;
    } else if (arg === "--port") {
      options.port = Number(argv[++i]);
    } else if (arg === "--answerer") {
      options.answerer = argv[++i];
    } else if (arg === "--artifacts-dir") {
      options.artifactsDir = argv[++i];
    } else if (arg === "--timeout-ms") {
      options.timeoutMs = Number(argv[++i]);
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }

  if (!Number.isInteger(options.port) || options.port < 0 || options.port > 65535) {
    throw new Error("--port must be an integer between 0 and 65535");
  }
  if (!Number.isInteger(options.timeoutMs) || options.timeoutMs <= 0) {
    throw new Error("--timeout-ms must be a positive integer");
  }
  return options;
}

function redactUrlQueryCredentials(value) {
  return value.replace(/([?&](?:credential|password|username)=)[^&#]*/gi, "$1[REDACTED]");
}

function redactValue(key, value) {
  if (SECRET_KEYS.has(String(key).toLowerCase())) {
    return "[REDACTED]";
  }

  if (typeof value === "string") {
    if (/^turns?:/i.test(value)) {
      return "[REDACTED]";
    }
    return redactUrlQueryCredentials(value);
  }

  if (Array.isArray(value)) {
    return value.map((item) => redactValue(key, item));
  }

  if (value && typeof value === "object") {
    return redactSecrets(value);
  }

  return value;
}

function redactSecrets(input) {
  if (Array.isArray(input)) {
    return input.map((item) => redactSecrets(item));
  }
  if (!input || typeof input !== "object") {
    return redactValue("", input);
  }

  const redacted = {};
  for (const [key, value] of Object.entries(input)) {
    redacted[key] = redactValue(key, value);
  }
  return redacted;
}

function normalizeMessage(raw, source) {
  let message = raw;
  if (typeof raw === "string" || Buffer.isBuffer(raw)) {
    message = JSON.parse(String(raw));
  }
  if (!message || typeof message !== "object" || typeof message.type !== "string") {
    throw new Error(`${source}: missing string message type`);
  }
  if (!MESSAGE_TYPES.has(message.type)) {
    throw new Error(`${source}: unsupported message type '${message.type}'`);
  }
  return message;
}

function logEvent(stage, event, fields = {}) {
  const entry = {
    stage,
    event,
    time: new Date().toISOString(),
    ...redactSecrets(fields),
  };
  process.stdout.write(`${JSON.stringify(entry)}\n`);
}

function runRedactionSelfTest() {
  const sample = {
    credential: "secret-credential",
    password: "secret-password",
    username: "secret-user",
    nested: {
      urls: "turn:example.test:3478?transport=udp&username=u&credential=c",
      httpUrl: "https://example.test/path?username=u&password=p&ok=1",
    },
  };
  const text = JSON.stringify(redactSecrets(sample));
  const forbidden = ["secret-credential", "secret-password", "secret-user", "turn:example.test", "username=u", "password=p"];
  const passed = forbidden.every((value) => !text.includes(value)) && text.includes("[REDACTED]");
  if (!passed) {
    throw new Error(`redaction self-test failed: ${text}`);
  }
  process.stdout.write("redaction self-test ok\n");
}

function spawnAnswerer(answererPath, onMessage) {
  const child = spawn(answererPath, [], {
    stdio: ["pipe", "pipe", "pipe"],
  });

  const lines = readline.createInterface({ input: child.stdout });
  lines.on("line", (line) => {
    try {
      onMessage(normalizeMessage(line, "answerer stdout"));
    } catch (error) {
      onMessage({ type: "error", stage: "answerer.stdout", message: error.message });
    }
  });

  child.stderr.on("data", (chunk) => {
    logEvent("answerer", "stderr", { message: String(chunk).trim() });
  });
  child.on("exit", (code, signal) => {
    onMessage({ type: code === 0 ? "done" : "error", stage: "answerer.exit", code, signal });
  });

  return child;
}

async function startServer(options) {
  fs.mkdirSync(options.artifactsDir, { recursive: true });

  const clients = new Set();
  const summary = {
    started_at: new Date().toISOString(),
    messages: [],
    errors: [],
  };

  let child = null;
  const broadcast = (message) => {
    const normalized = normalizeMessage(message, "broadcast");
    summary.messages.push({ direction: "answerer-to-browser", message: redactSecrets(normalized) });
    const encoded = JSON.stringify(normalized);
    for (const client of clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(encoded);
      }
    }
    logEvent("signaling", normalized.type, normalized);
  };

  if (options.answerer) {
    child = spawnAnswerer(options.answerer, broadcast);
    logEvent("answerer", "spawned", { path: options.answerer });
  }

  const wss = new WebSocketServer({ host: "127.0.0.1", port: options.port });

  wss.on("connection", (socket) => {
    clients.add(socket);
    logEvent("browser", "connected", { clients: clients.size });

    socket.on("message", (data) => {
      try {
        const message = normalizeMessage(data, "browser websocket");
        summary.messages.push({ direction: "browser-to-answerer", message: redactSecrets(message) });
        logEvent("browser", message.type, message);
        if (child && child.stdin.writable && ANSWERER_INPUT_TYPES.has(message.type)) {
          child.stdin.write(`${JSON.stringify(message)}\n`);
        }
      } catch (error) {
        const err = { type: "error", stage: "browser.websocket", message: error.message };
        summary.errors.push(redactSecrets(err));
        socket.send(JSON.stringify(err));
      }
    });

    socket.on("close", () => {
      clients.delete(socket);
      logEvent("browser", "disconnected", { clients: clients.size });
    });
  });

  await new Promise((resolve) => wss.once("listening", resolve));
  const address = wss.address();
  const url = `ws://127.0.0.1:${address.port}`;
  logEvent("signaling", "listening", { url, message_types: [...MESSAGE_TYPES] });

  const timeout = setTimeout(() => {
    summary.errors.push({ type: "error", stage: "timeout", message: `timed out after ${options.timeoutMs}ms` });
    logEvent("signaling", "timeout", { timeout_ms: options.timeoutMs });
    wss.close();
    if (child) {
      child.kill("SIGTERM");
    }
    process.exitCode = 124;
  }, options.timeoutMs);

  const stop = () => {
    clearTimeout(timeout);
    summary.completed_at = new Date().toISOString();
    fs.writeFileSync(path.join(options.artifactsDir, "signaling-summary.json"), `${JSON.stringify(summary, null, 2)}\n`);
    if (child) {
      child.kill("SIGTERM");
    }
    wss.close();
  };

  process.once("SIGINT", stop);
  process.once("SIGTERM", stop);
  return { url, stop };
}

async function main() {
  try {
    const options = parseArgs(process.argv);
    if (options.help) {
      process.stdout.write(usage());
      return;
    }
    if (options.selfTestRedaction) {
      runRedactionSelfTest();
      return;
    }
    await startServer(options);
  } catch (error) {
    process.stderr.write(`${redactSecrets({ message: error.message }).message}\n`);
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main();
}

module.exports = {
  MESSAGE_TYPES,
  ANSWERER_INPUT_TYPES,
  normalizeMessage,
  parseArgs,
  redactSecrets,
  runRedactionSelfTest,
  startServer,
  usage,
};
