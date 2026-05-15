#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const http = require("node:http");
const os = require("node:os");
const path = require("node:path");
const { spawn, execFileSync } = require("node:child_process");

const projectRoot = path.resolve(__dirname, "..", "..");
const WebSocket = require(path.join(projectRoot, "tests", "e2e", "node_modules", "ws"));
const { WebSocketServer } = WebSocket;

function parseArgs(argv) {
  const options = {
    mediaDir: path.join(projectRoot, "build", "fanout-memory", "media"),
    answerer: path.join(projectRoot, "build", "fanout-memory", "fanout_answerer"),
    host: "0.0.0.0",
    publicHost: null,
    port: 0,
    httpPort: 0,
    reportDir: path.join(projectRoot, "build", "fanout-memory", "reports", timestampSlug()),
    sampleMs: 1000,
  };
  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--media-dir") options.mediaDir = path.resolve(argv[++i]);
    else if (arg === "--answerer") options.answerer = path.resolve(argv[++i]);
    else if (arg === "--host") options.host = argv[++i];
    else if (arg === "--public-host") options.publicHost = argv[++i];
    else if (arg === "--port") options.port = Number(argv[++i]);
    else if (arg === "--http-port") options.httpPort = Number(argv[++i]);
    else if (arg === "--report-dir") options.reportDir = path.resolve(argv[++i]);
    else if (arg === "--sample-ms") options.sampleMs = Number(argv[++i]);
    else if (arg === "--help" || arg === "-h") {
      process.stdout.write(`Usage: node examples/fanout-memory/run-fanout.js [--media-dir DIR]\n`);
      process.exit(0);
    } else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }
  return options;
}

function timestampSlug() {
  return new Date().toISOString().replace(/[-:]/g, "").replace(/\..*/, "").replace("T", "-");
}

function detectPublicHost() {
  for (const entries of Object.values(os.networkInterfaces())) {
    for (const item of entries || []) {
      if (item.family === "IPv4" && !item.internal && item.address) return item.address;
    }
  }
  return "127.0.0.1";
}

function contentType(filePath) {
  if (filePath.endsWith(".html")) return "text/html; charset=utf-8";
  if (filePath.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (filePath.endsWith(".css")) return "text/css; charset=utf-8";
  return "application/octet-stream";
}

function readKeyValueKiB(text, key) {
  const match = text.match(new RegExp(`^${key}:\\s+(\\d+)\\s+kB`, "m"));
  return match ? Number(match[1]) : 0;
}

function readStatusValue(text, key) {
  const match = text.match(new RegExp(`^${key}:\\s+(\\d+)`, "m"));
  return match ? Number(match[1]) : 0;
}

function sampleProcess(pid, baseline) {
  const status = fs.readFileSync(`/proc/${pid}/status`, "utf8");
  const smaps = fs.readFileSync(`/proc/${pid}/smaps_rollup`, "utf8");
  const fdCount = fs.readdirSync(`/proc/${pid}/fd`).length;
  const sample = {
    ts: Date.now(),
    rssKiB: readKeyValueKiB(status, "VmRSS"),
    rssAnonKiB: readKeyValueKiB(status, "RssAnon"),
    rssFileKiB: readKeyValueKiB(status, "RssFile"),
    vmSizeKiB: readKeyValueKiB(status, "VmSize"),
    threads: readStatusValue(status, "Threads"),
    fdCount,
    pssKiB: readKeyValueKiB(smaps, "Pss"),
    privateDirtyKiB: readKeyValueKiB(smaps, "Private_Dirty"),
    anonymousKiB: readKeyValueKiB(smaps, "Anonymous"),
    sharedCleanKiB: readKeyValueKiB(smaps, "Shared_Clean"),
    privateCleanKiB: readKeyValueKiB(smaps, "Private_Clean"),
    swapKiB: readKeyValueKiB(smaps, "Swap"),
  };
  if (baseline) {
    sample.deltaPrivateDirtyKiB = sample.privateDirtyKiB - baseline.privateDirtyKiB;
    sample.deltaAnonymousKiB = sample.anonymousKiB - baseline.anonymousKiB;
    sample.deltaPssKiB = sample.pssKiB - baseline.pssKiB;
  } else {
    sample.deltaPrivateDirtyKiB = 0;
    sample.deltaAnonymousKiB = 0;
    sample.deltaPssKiB = 0;
  }
  return sample;
}

function reportHtml(report) {
  const escaped = JSON.stringify(report).replace(/</g, "\\u003c");
  return `<!doctype html>
<html lang="zh-CN">
<meta charset="utf-8">
<title>Fanout Memory Report</title>
<style>
body{font-family:system-ui,sans-serif;margin:24px;color:#17202a;background:#f7f8fb}
table{border-collapse:collapse;width:100%;background:white}
td,th{border:1px solid #d8dee9;padding:6px 8px;text-align:right}
td:first-child,th:first-child{text-align:left}
pre{background:#111827;color:#e5e7eb;padding:16px;overflow:auto}
</style>
<h1>Fanout Memory Report</h1>
<p>Library memory estimate = baseline-subtracted Private_Dirty / Anonymous. Dynamic library file-backed pages are excluded by definition.</p>
<div id="summary"></div>
<pre id="json"></pre>
<script>
const report=${escaped};
document.getElementById("summary").innerHTML =
  "<table><tr><th>samples</th><td>"+report.samples.length+"</td></tr>"+
  "<tr><th>peers</th><td>"+Object.keys(report.peers).length+"</td></tr>"+
  "<tr><th>peak delta Private_Dirty KiB</th><td>"+report.summary.peakDeltaPrivateDirtyKiB+"</td></tr>"+
  "<tr><th>peak delta Anonymous KiB</th><td>"+report.summary.peakDeltaAnonymousKiB+"</td></tr>"+
  "<tr><th>peak RSS KiB</th><td>"+report.summary.peakRssKiB+"</td></tr>"+
  "<tr><th>peak RssAnon KiB</th><td>"+report.summary.peakRssAnonKiB+"</td></tr>"+
  "<tr><th>peak RssFile KiB</th><td>"+report.summary.peakRssFileKiB+"</td></tr></table>"+
  "<h2>Peer deltas</h2><table><tr><th>peer</th><th>Private_Dirty delta KiB</th><th>Anonymous delta KiB</th><th>incremental Private_Dirty KiB</th><th>incremental Anonymous KiB</th></tr>"+
  report.peerDeltas.map((peer) => "<tr><td>"+peer.peerId+"</td><td>"+peer.deltaPrivateDirtyKiB+"</td><td>"+peer.deltaAnonymousKiB+"</td><td>"+peer.incrementalPrivateDirtyKiB+"</td><td>"+peer.incrementalAnonymousKiB+"</td></tr>").join("")+
  "</table>";
document.getElementById("json").textContent = JSON.stringify(report, null, 2);
</script>`;
}

function summarize(samples) {
  return {
    peakDeltaPrivateDirtyKiB: Math.max(0, ...samples.map((s) => s.deltaPrivateDirtyKiB || 0)),
    peakDeltaAnonymousKiB: Math.max(0, ...samples.map((s) => s.deltaAnonymousKiB || 0)),
    peakRssKiB: Math.max(0, ...samples.map((s) => s.rssKiB || 0)),
    peakRssAnonKiB: Math.max(0, ...samples.map((s) => s.rssAnonKiB || 0)),
    peakRssFileKiB: Math.max(0, ...samples.map((s) => s.rssFileKiB || 0)),
    final: samples[samples.length - 1] || null,
  };
}

function summarizePeers(peers) {
  let previousSample = null;
  return Object.entries(peers)
    .sort((left, right) => (left[1].createdAt || 0) - (right[1].createdAt || 0))
    .map(([peerId, peer]) => {
      const sample = peer.firstSampleAfterConnected || peer.lastSample || null;
      const item = {
        peerId,
        createdAt: peer.createdAt || null,
        connectedAt: peer.connectedAt || null,
        dataChannelOpenAt: peer.dataChannelOpenAt || null,
        sample,
        deltaPrivateDirtyKiB: sample ? sample.deltaPrivateDirtyKiB : null,
        deltaAnonymousKiB: sample ? sample.deltaAnonymousKiB : null,
        deltaPssKiB: sample ? sample.deltaPssKiB : null,
        incrementalPrivateDirtyKiB: sample && previousSample ? sample.deltaPrivateDirtyKiB - previousSample.deltaPrivateDirtyKiB : null,
        incrementalAnonymousKiB: sample && previousSample ? sample.deltaAnonymousKiB - previousSample.deltaAnonymousKiB : null,
      };
      if (sample) previousSample = sample;
      return item;
    });
}

async function main() {
  const options = parseArgs(process.argv);
  const publicHost = options.publicHost || detectPublicHost();
  const publicDir = path.join(__dirname, "public");
  fs.mkdirSync(options.reportDir, { recursive: true });
  if (!fs.existsSync(options.answerer)) throw new Error(`answerer not found: ${options.answerer}`);
  if (!fs.existsSync(path.join(options.mediaDir, "video.h264"))) throw new Error(`media not prepared: ${options.mediaDir}`);

  const report = {
    startedAt: new Date().toISOString(),
    mediaLoadingStrategy: "streaming-indexed",
    memoryDefinition: "baseline-subtracted Private_Dirty/Anonymous from /proc/<pid>/smaps_rollup after indexed media load; dynamic-library file-backed pages excluded",
    options,
    answererPid: null,
    baseline: null,
    samples: [],
    events: [],
    peers: {},
    peerDeltas: [],
    pmapFinal: "",
    summary: {},
  };

  const child = spawn(options.answerer, ["--media-dir", options.mediaDir], {
    stdio: ["pipe", "pipe", "pipe"],
    env: {
      ...process.env,
      MRTC_ICE_BIND_IP: publicHost,
      MRTC_ICE_ANNOUNCE_IP: publicHost,
    },
  });
  report.answererPid = child.pid;

  const clients = new Set();
  const broadcast = (message) => {
    const encoded = JSON.stringify(message);
    for (const client of clients) {
      if (client.readyState === WebSocket.OPEN) client.send(encoded);
    }
  };

  const wss = new WebSocketServer({ host: options.host, port: options.port });
  await new Promise((resolve) => wss.once("listening", resolve));
  const wsPort = wss.address().port;
  const wsUrl = `ws://${publicHost}:${wsPort}`;

  child.stdout.setEncoding("utf8");
  let buffer = "";
  let ready = false;
  let answererHello = null;
  child.stdout.on("data", (chunk) => {
    buffer += chunk;
    for (;;) {
      const index = buffer.indexOf("\n");
      if (index < 0) break;
      const line = buffer.slice(0, index);
      buffer = buffer.slice(index + 1);
      if (!line.trim()) continue;
      try {
        const message = JSON.parse(line);
        report.events.push({ ts: Date.now(), direction: "answerer-to-browser", message });
        if (message.type === "hello") {
          ready = true;
          answererHello = message;
        }
        if (message.peerId) {
          report.peers[message.peerId] = report.peers[message.peerId] || {};
          if (message.type === "event") {
            const peer = report.peers[message.peerId];
            peer.lastEvent = message;
            if (message.name === "pc.connected" && !peer.connectedAt) peer.connectedAt = Date.now();
            if (message.name === "datachannel.open" && !peer.dataChannelOpenAt) peer.dataChannelOpenAt = Date.now();
            if (message.name === "media.stats") peer.lastMediaStats = message.fields || {};
          }
        }
        broadcast(message);
      } catch (error) {
        report.events.push({ ts: Date.now(), direction: "answerer-parse-error", line, error: error.message });
      }
    }
  });
  child.stderr.on("data", (chunk) => {
    const message = { type: "stderr", text: String(chunk) };
    report.events.push({ ts: Date.now(), direction: "answerer-stderr", message });
    broadcast(message);
  });
  child.on("exit", (code, signal) => {
    broadcast({ type: "answerer-exit", code, signal });
    writeReport();
  });

  wss.on("connection", (socket) => {
    clients.add(socket);
    socket.send(JSON.stringify({ type: "runner", ws: wsUrl, pid: child.pid, reportDir: options.reportDir }));
    if (answererHello) socket.send(JSON.stringify(answererHello));
    socket.on("message", (data) => {
      const message = JSON.parse(String(data));
      report.events.push({ ts: Date.now(), direction: "browser-to-answerer", message });
      if (message.peerId) {
        report.peers[message.peerId] = report.peers[message.peerId] || {};
        if (message.type === "offer" && !report.peers[message.peerId].createdAt) {
          report.peers[message.peerId].createdAt = Date.now();
        }
      }
      if (child.stdin.writable) child.stdin.write(`${JSON.stringify(message)}\n`);
      if (message.type === "stop") writeReport();
    });
    socket.on("close", () => clients.delete(socket));
  });

  const httpServer = http.createServer((request, response) => {
    const url = new URL(request.url, `http://${request.headers.host || "localhost"}`);
    if (url.pathname === "/report.json") {
      const current = currentReport();
      response.writeHead(200, { "content-type": "application/json" });
      response.end(`${JSON.stringify(current, null, 2)}\n`);
      return;
    }
    if (url.pathname === "/report.html") {
      response.writeHead(200, { "content-type": "text/html; charset=utf-8" });
      response.end(reportHtml(currentReport()));
      return;
    }
    const pathname = url.pathname === "/" ? "/index.html" : url.pathname;
    const filePath = path.resolve(publicDir, `.${decodeURIComponent(pathname)}`);
    if (!filePath.startsWith(publicDir + path.sep) && filePath !== publicDir) {
      response.writeHead(403);
      response.end("forbidden\n");
      return;
    }
    fs.readFile(filePath, (error, data) => {
      if (error) {
        response.writeHead(404);
        response.end("not found\n");
        return;
      }
      response.writeHead(200, { "content-type": contentType(filePath) });
      response.end(data);
    });
  });
  await new Promise((resolve) => httpServer.listen(options.httpPort, options.host, resolve));
  const httpPort = httpServer.address().port;

  const sampler = setInterval(() => {
    if (!ready) return;
    try {
      const sample = sampleProcess(child.pid, report.baseline);
      if (!report.baseline) report.baseline = sample;
      const withBaseline = sampleProcess(child.pid, report.baseline);
      report.samples.push(withBaseline);
      for (const peer of Object.values(report.peers)) {
        peer.lastSample = withBaseline;
        if (peer.connectedAt && !peer.firstSampleAfterConnected) {
          peer.firstSampleAfterConnected = withBaseline;
        }
      }
      broadcast({ type: "memory", sample: withBaseline, baseline: report.baseline });
    } catch (error) {
      broadcast({ type: "memory-error", message: error.message });
    }
  }, options.sampleMs);

  function currentReport() {
    report.summary = summarize(report.samples);
    report.peerDeltas = summarizePeers(report.peers);
    report.completedAt = new Date().toISOString();
    return report;
  }

  function writeReport() {
    clearInterval(sampler);
    try {
      report.pmapFinal = execFileSync("pmap", ["-x", String(child.pid)], { encoding: "utf8" });
    } catch {
      report.pmapFinal = "";
    }
    const current = currentReport();
    fs.writeFileSync(path.join(options.reportDir, "memory-report.json"), `${JSON.stringify(current, null, 2)}\n`);
    fs.writeFileSync(path.join(options.reportDir, "memory-report.html"), reportHtml(current));
  }

  const pageUrl = new URL(`http://${publicHost}:${httpPort}/index.html`);
  pageUrl.searchParams.set("ws", wsUrl);
  process.stdout.write("\nFanout memory example is ready.\n");
  process.stdout.write(`Open: ${pageUrl.toString()}\n`);
  process.stdout.write(`Report dir: ${options.reportDir}\n`);
  process.stdout.write("Stop: Ctrl-C\n\n");
  process.stdout.write(`${JSON.stringify({ status: "ready", url: pageUrl.toString(), ws: wsUrl, pid: child.pid, reportDir: options.reportDir })}\n`);

  process.on("SIGINT", () => {
    writeReport();
    child.kill("SIGTERM");
    httpServer.close();
    wss.close();
    process.exit(0);
  });
}

main().catch((error) => {
  process.stderr.write(`${error.stack || error.message}\n`);
  process.exit(1);
});
