#!/usr/bin/env node

import { createServer } from "node:http";
import { createReadStream, existsSync } from "node:fs";
import { extname, normalize, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { WebSocketServer } from "ws";

const here = fileURLToPath(new URL(".", import.meta.url));
const pageRoot = resolve(here, "page");

export const allowedTypes = new Set([
  "hello",
  "offer",
  "answer",
  "candidate",
  "status",
  "summary",
  "error",
]);

const forbiddenFields = new Set(["media", "payload", "binary"]);

const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
]);

function parseArgs(argv) {
  const args = { host: "127.0.0.1", port: 8080 };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--host") {
      args.host = argv[++i];
    } else if (arg === "--port") {
      args.port = Number.parseInt(argv[++i], 10);
    }
  }
  return args;
}

function resolveStaticPath(urlPath) {
  const requestPath = urlPath === "/" ? "/index.html" : urlPath;
  const normalized = normalize(decodeURIComponent(requestPath)).replace(/^(\.\.[/\\])+/, "");
  const absolute = resolve(pageRoot, `.${normalized}`);
  if (absolute !== pageRoot && !absolute.startsWith(`${pageRoot}/`)) {
    return null;
  }
  return absolute;
}

function sendJson(ws, message) {
  if (ws.readyState === ws.OPEN) {
    ws.send(JSON.stringify(message));
  }
}

function rejectMessage(ws, runId, reason) {
  sendJson(ws, {
    type: "error",
    runId,
    reason,
  });
}

function validateMessage(raw) {
  let message;
  try {
    message = JSON.parse(raw.toString("utf8"));
  } catch {
    return { ok: false, reason: "invalid_json" };
  }

  if (!message || typeof message !== "object" || Array.isArray(message)) {
    return { ok: false, reason: "invalid_json_object" };
  }
  if (!allowedTypes.has(message.type)) {
    return { ok: false, reason: "unknown_type" };
  }
  for (const field of forbiddenFields) {
    if (Object.prototype.hasOwnProperty.call(message, field)) {
      return { ok: false, reason: `forbidden_field:${field}` };
    }
  }
  if (typeof message.runId !== "string" || message.runId.length === 0) {
    return { ok: false, reason: "missing_run_id" };
  }

  return { ok: true, message };
}

export function createSignalingServer(options = {}) {
  const host = options.host ?? "127.0.0.1";
  const port = options.port ?? 8080;
  const clientsByRun = new Map();

  const httpServer = createServer((req, res) => {
    if (!req.url) {
      res.writeHead(400);
      res.end("bad request");
      return;
    }
    const url = new URL(req.url, `http://${req.headers.host ?? "localhost"}`);
    const filePath = resolveStaticPath(url.pathname);
    if (!filePath || !existsSync(filePath)) {
      res.writeHead(404);
      res.end("not found");
      return;
    }
    const type = contentTypes.get(extname(filePath)) ?? "application/octet-stream";
    res.writeHead(200, {
      "content-type": type,
      "cache-control": "no-store",
    });
    createReadStream(filePath).pipe(res);
  });

  const wsServer = new WebSocketServer({ server: httpServer, path: "/ws" });

  wsServer.on("connection", (ws) => {
    ws.runId = null;

    ws.on("message", (raw, isBinary) => {
      if (isBinary) {
        rejectMessage(ws, ws.runId, "binary_rejected");
        return;
      }
      const result = validateMessage(raw);
      if (!result.ok) {
        rejectMessage(ws, ws.runId, result.reason);
        return;
      }

      const message = result.message;
      ws.runId = message.runId;
      if (!clientsByRun.has(message.runId)) {
        clientsByRun.set(message.runId, new Set());
      }
      clientsByRun.get(message.runId).add(ws);

      const peers = clientsByRun.get(message.runId);
      for (const peer of peers) {
        if (peer !== ws && peer.readyState === peer.OPEN) {
          peer.send(JSON.stringify(message));
        }
      }
    });

    ws.on("close", () => {
      if (!ws.runId) {
        return;
      }
      const peers = clientsByRun.get(ws.runId);
      if (!peers) {
        return;
      }
      peers.delete(ws);
      if (peers.size === 0) {
        clientsByRun.delete(ws.runId);
      }
    });
  });

  return {
    host,
    port,
    httpServer,
    wsServer,
    get url() {
      const address = httpServer.address();
      const actualPort = typeof address === "object" && address ? address.port : port;
      return `http://${host}:${actualPort}/`;
    },
    get wsUrl() {
      const address = httpServer.address();
      const actualPort = typeof address === "object" && address ? address.port : port;
      return `ws://${host}:${actualPort}/ws`;
    },
    listen() {
      return new Promise((resolveListen, rejectListen) => {
        httpServer.once("error", rejectListen);
        httpServer.listen(port, host, () => {
          httpServer.off("error", rejectListen);
          resolveListen(this);
        });
      });
    },
    close() {
      for (const client of wsServer.clients) {
        client.close();
      }
      return new Promise((resolveClose) => {
        wsServer.close(() => {
          httpServer.close(() => resolveClose());
        });
      });
    },
  };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const options = parseArgs(process.argv.slice(2));
  const server = createSignalingServer(options);
  await server.listen();
  console.log(JSON.stringify({
    type: "ready",
    url: server.url,
    wsUrl: server.wsUrl,
  }));

  const shutdown = async () => {
    await server.close();
    process.exit(0);
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}
