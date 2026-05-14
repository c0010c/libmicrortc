"use strict";

const fs = require("node:fs");
const path = require("node:path");

const SECRET_KEYS = new Set(["username", "credential", "password"]);
const SECRET_VALUE_KEYS = ["username", "credential", "password", "token", "secret"];

class TurnConfigError extends Error {
  constructor(message) {
    super(message);
    this.name = "TurnConfigError";
    this.stage = "turn-config";
  }
}

function parseArgs(argv) {
  const args = {
    turnConfig: null,
    selfTest: false,
  };

  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--turn-config") {
      args.turnConfig = argv[++i] || null;
    } else if (arg.startsWith("--turn-config=")) {
      args.turnConfig = arg.slice("--turn-config=".length);
    } else if (arg === "--self-test") {
      args.selfTest = true;
    }
  }

  return args;
}

function redactValue(key, value) {
  if (value == null) {
    return value;
  }
  if (SECRET_KEYS.has(String(key).toLowerCase())) {
    return "<redacted>";
  }
  if (typeof value === "string") {
    if (/^turns?:/i.test(value)) {
      return "turn:<redacted>";
    }
    let redacted = value;
    for (const secretKey of SECRET_VALUE_KEYS) {
      redacted = redacted.replace(new RegExp(`([?&]${secretKey}=)[^&#]+`, "gi"), "$1<redacted>");
      redacted = redacted.replace(new RegExp(`(${secretKey}[=:])[^\\s,;]+`, "gi"), "$1<redacted>");
    }
    return redacted;
  }
  return value;
}

function redactSecrets(input) {
  if (Array.isArray(input)) {
    return input.map((item) => redactSecrets(item));
  }
  if (input && typeof input === "object") {
    const output = {};
    for (const [key, value] of Object.entries(input)) {
      if (SECRET_KEYS.has(key.toLowerCase())) {
        continue;
      } else {
        output[key] = redactSecrets(value);
      }
    }
    return output;
  }
  return redactValue("", input);
}

function redactText(text) {
  return String(text)
    .replace(/("?(?:username|credential|password|token|secret)"?\s*[:=]\s*)("[^"]*"|[^\s,}]+)/gi, "$1\"<redacted>\"")
    .replace(/([?&](?:username|credential|password|token|secret)=)[^&#\s]+/gi, "$1<redacted>");
}

function normalizeUrls(urls, index) {
  const values = Array.isArray(urls) ? urls : [urls];
  if (values.length === 0) {
    throw new TurnConfigError(`ice_servers[${index}] is missing urls`);
  }
  for (const url of values) {
    if (typeof url !== "string" || url.length === 0) {
      throw new TurnConfigError(`ice_servers[${index}] contains an invalid url`);
    }
  }
  return values;
}

function loadTurnConfig(configPath) {
  if (!configPath) {
    throw new TurnConfigError("missing required --turn-config path");
  }

  const resolvedPath = path.resolve(configPath);
  let parsed;
  try {
    parsed = JSON.parse(fs.readFileSync(resolvedPath, "utf8"));
  } catch (error) {
    if (error && error.code === "ENOENT") {
      throw new TurnConfigError(`TURN config file not found: ${configPath}`);
    }
    throw new TurnConfigError(`failed to parse TURN config JSON: ${error.message}`);
  }

  if (!parsed || !Array.isArray(parsed.ice_servers) || parsed.ice_servers.length === 0) {
    throw new TurnConfigError("TURN config must contain a non-empty ice_servers array");
  }

  let sawTurnUrl = false;
  const iceServers = parsed.ice_servers.map((server, index) => {
    if (!server || typeof server !== "object") {
      throw new TurnConfigError(`ice_servers[${index}] must be an object`);
    }

    const urls = normalizeUrls(server.urls, index);
    const hasTurnUrl = urls.some((url) => url.toLowerCase().startsWith("turn:") || url.toLowerCase().startsWith("turns:"));
    sawTurnUrl = sawTurnUrl || hasTurnUrl;

    const username = server.username;
    const credential = server.credential != null ? server.credential : server.password;
    if (hasTurnUrl && (!username || !credential)) {
      throw new TurnConfigError(`ice_servers[${index}] TURN entry requires username and credential/password`);
    }

    const browserServer = { urls: Array.isArray(server.urls) ? urls : urls[0] };
    if (username != null) {
      browserServer.username = username;
    }
    if (credential != null) {
      browserServer.credential = credential;
    }
    return browserServer;
  });

  if (!sawTurnUrl) {
    throw new TurnConfigError("TURN config must contain at least one turn: or turns: URL");
  }

  return {
    stage: "turn-config",
    iceConfigPath: resolvedPath,
    browserConfig: {
      iceTransportPolicy: "relay",
      iceServers,
    },
    redacted: redactSecrets({
      iceConfigPath: resolvedPath,
      browserConfig: {
        iceTransportPolicy: "relay",
        iceServers,
      },
    }),
  };
}

function runSelfTest() {
  const config = loadTurnConfig(path.resolve(__dirname, "../../mrtc-ice-servers.example.json"));
  const raw = JSON.stringify(config);
  const redacted = JSON.stringify(config.redacted);
  if (!raw.includes("<credential>")) {
    throw new TurnConfigError("self-test fixture did not exercise placeholder credential");
  }
  if (redacted.includes("<credential>") || redacted.includes("<username>")) {
    throw new TurnConfigError("self-test redaction leaked placeholder credential fields");
  }
  return config.redacted;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  try {
    const output = args.selfTest ? runSelfTest() : loadTurnConfig(args.turnConfig).redacted;
    process.stdout.write(`${JSON.stringify(output, null, 2)}\n`);
  } catch (error) {
    const stage = error.stage || "turn-config";
    process.stderr.write(`[${stage}] ${redactText(error.message || String(error))}\n`);
    process.exitCode = 1;
  }
}

if (require.main === module) {
  main();
}

module.exports = {
  TurnConfigError,
  loadTurnConfig,
  redactSecrets,
  redactText,
};
