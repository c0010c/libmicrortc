(function () {
  "use strict";

  const params = new URLSearchParams(window.location.search);
  const manualMode = params.get("manual") === "1";
  const state = {
    events: [],
    messages: [],
    errors: [],
    connectionState: "new",
    iceConnectionState: "new",
    signalingState: "stable",
    dataChannelState: "new",
    offerSent: false,
    answerApplied: false,
    candidatesSent: 0,
    candidatesReceived: 0,
    remoteCandidateTypes: [],
    localMediaStarted: false,
    remoteVideoTrack: false,
    remoteAudioTrack: false,
    mediaStartRequested: false,
    remoteMediaStarted: false,
  };
  const waiters = new Map();

  let pc = null;
  let ws = null;
  let dataChannel = null;
  let remoteAudioAnalyser = null;
  let remoteAudioData = null;
  let localAudioContext = null;
  const localAudioNodes = [];
  const pendingRemoteCandidates = [];

  function byId(id) {
    return document.getElementById(id);
  }

  function setText(id, text) {
    const element = byId(id);
    if (element) {
      element.textContent = text;
    }
  }

  function badgeClass(value) {
    if (value === "connected" || value === "open" || value === "playing" || value === "started" || value === "received") {
      return "badge ok";
    }
    if (value === "checking" || value === "connecting" || value === "requested" || value === "pending") {
      return "badge warn";
    }
    if (value === "failed" || value === "closed" || value === "disconnected" || value === "error") {
      return "badge bad";
    }
    return "badge";
  }

  function setBadge(id, text, value) {
    const element = byId(id);
    if (element) {
      element.textContent = text;
      element.className = badgeClass(value || text);
    }
  }

  function latestRemoteMediaFrameCount() {
    return state.events
      .filter((event) => event.name === "remote.media.frame")
      .reduce((total, event) => total + Number(event.fields && event.fields.count || 0), 0);
  }

  function updateUi() {
    const wsState = ws
      ? ["connecting", "open", "closing", "closed"][ws.readyState] || String(ws.readyState)
      : "idle";
    const media = getRemoteMediaState();
    const videoPlaying = Boolean(media.video && media.video.readyState >= 2 && media.video.videoWidth > 0);
    const audioPlaying = Boolean(media.audio && media.audio.readyState >= 2);
    const cMediaFrames = latestRemoteMediaFrameCount();
    const startButton = byId("start-button");
    const statusLine = byId("status-line");

    setText("mode", manualMode ? "manual host" : "auto host");
    setBadge("status-ws", wsState, wsState);
    setBadge("status-peer", state.connectionState, state.connectionState);
    setBadge("status-ice", state.iceConnectionState, state.iceConnectionState);
    setBadge("status-data", state.dataChannelState, state.dataChannelState);
    setBadge("status-video", videoPlaying ? "playing" : "pending", videoPlaying ? "playing" : "pending");
    setBadge("status-audio", audioPlaying ? "playing" : "pending", audioPlaying ? "playing" : "pending");
    setBadge("status-cmedia", cMediaFrames > 0 ? `received ${cMediaFrames}` : "pending", cMediaFrames > 0 ? "received" : "pending");

    if (startButton) {
      startButton.disabled = state.mediaStartRequested || state.connectionState === "connecting";
      startButton.textContent = state.mediaStartRequested ? "拉流中" : "开始拉流";
    }
    if (statusLine) {
      if (state.errors.length) {
        statusLine.textContent = state.errors[state.errors.length - 1].message;
      } else if (state.mediaStartRequested) {
        statusLine.textContent = "已请求媒体";
      } else if (state.connectionState === "connected") {
        statusLine.textContent = "已连接";
      } else if (wsState === "idle") {
        statusLine.textContent = "等待启动";
      } else {
        statusLine.textContent = "连接中";
      }
    }
  }

  function appendEventLog(event) {
    const list = byId("events");
    if (!list) {
      return;
    }
    const item = document.createElement("li");
    const fields = event.fields && Object.keys(event.fields).length
      ? ` ${JSON.stringify(event.fields)}`
      : "";
    item.textContent = `${event.time.slice(11, 19)} ${event.name}${fields}`;
    list.prepend(item);
    while (list.children.length > 40) {
      list.removeChild(list.lastChild);
    }
  }

  function recordEvent(name, fields) {
    const event = {
      name,
      fields: fields || {},
      time: new Date().toISOString(),
    };
    state.events.push(event);

    const pending = waiters.get(name);
    if (pending) {
      waiters.delete(name);
      pending.forEach((waiter) => waiter.resolve(event));
    }

    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "event", name, fields: event.fields }));
    }
    appendEventLog(event);
    updateUi();
    return event;
  }

  function recordError(stage, error) {
    const entry = {
      stage,
      message: error && error.message ? error.message : String(error),
      time: new Date().toISOString(),
    };
    state.errors.push(entry);
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "error", stage, message: entry.message }));
    }
    recordEvent("error", entry);
  }

  function parseJsonParam(name, fallback) {
    const raw = params.get(name);
    if (!raw) {
      return fallback;
    }
    try {
      return JSON.parse(raw);
    } catch (error) {
      recordError(`query.${name}`, error);
      return fallback;
    }
  }

  function rtcConfigFromQuery() {
    const config = window.__mrtcRTCConfig
      ? JSON.parse(JSON.stringify(window.__mrtcRTCConfig))
      : parseJsonParam("rtcConfig", {});
    if (!config.iceServers) {
      const iceServers = parseJsonParam("iceServers", null);
      if (iceServers) {
        config.iceServers = iceServers;
      }
    }
    const iceTransportPolicy = params.get("iceTransportPolicy");
    if (iceTransportPolicy) {
      config.iceTransportPolicy = iceTransportPolicy;
    }
    return config;
  }

  function preferCodec(kind, codecName) {
    if (!window.RTCRtpSender || !RTCRtpSender.getCapabilities || !pc) {
      return;
    }
    const capabilities = RTCRtpSender.getCapabilities(kind);
    if (!capabilities || !capabilities.codecs || !capabilities.codecs.length) {
      return;
    }
    const preferred = capabilities.codecs.filter((codec) => codec.mimeType.toLowerCase() === `${kind}/${codecName}`.toLowerCase());
    if (!preferred.length) {
      return;
    }
    const remaining = capabilities.codecs.filter((codec) => !preferred.includes(codec));
    const transceiver = pc.getTransceivers().find((item) => item.receiver && item.receiver.track.kind === kind);
    if (transceiver && transceiver.setCodecPreferences) {
      transceiver.setCodecPreferences(preferred.concat(remaining));
    }
  }

  function attachRemoteTrack(event) {
    const [stream] = event.streams;
    const target = event.track.kind === "video"
      ? document.getElementById("remote-video")
      : document.getElementById("remote-audio");

    if (target && stream) {
      target.srcObject = stream;
      if (event.track.kind === "audio") {
        attachRemoteAudioAnalyser(stream);
        state.remoteAudioTrack = true;
      } else {
        state.remoteVideoTrack = true;
      }
      recordEvent(`track.${event.track.kind}`, {
        id: event.track.id,
        streams: event.streams.length,
      });
    }
  }

  function attachRemoteAudioAnalyser(stream) {
    try {
      const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextCtor || remoteAudioAnalyser) {
        return;
      }
      const context = new AudioContextCtor();
      const source = context.createMediaStreamSource(stream);
      remoteAudioAnalyser = context.createAnalyser();
      remoteAudioAnalyser.fftSize = 256;
      remoteAudioData = new Uint8Array(remoteAudioAnalyser.fftSize);
      source.connect(remoteAudioAnalyser);
      context.resume().catch(() => {});
    } catch (error) {
      recordError("audio.analyser", error);
    }
  }

  function createSyntheticVideoTrack() {
    const canvas = document.createElement("canvas");
    const context = canvas.getContext("2d");
    let frame = 0;

    canvas.width = 320;
    canvas.height = 180;
    function draw() {
      const hue = frame % 360;
      context.fillStyle = `hsl(${hue}, 70%, 45%)`;
      context.fillRect(0, 0, canvas.width, canvas.height);
      context.fillStyle = "#ffffff";
      context.fillRect((frame * 7) % canvas.width, 52, 44, 44);
      context.fillStyle = "#101820";
      context.font = "24px sans-serif";
      context.fillText(`mrtc ${frame}`, 24, 128);
      frame += 1;
    }
    draw();
    window.setInterval(draw, 100);
    return canvas.captureStream(10).getVideoTracks()[0];
  }

  function createSyntheticAudioTrack() {
    const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
    const context = new AudioContextCtor();
    const oscillator = context.createOscillator();
    const oscillatorB = context.createOscillator();
    const gain = context.createGain();
    const destination = context.createMediaStreamDestination();

    oscillator.frequency.value = 440;
    oscillatorB.frequency.value = 660;
    gain.gain.value = 0.12;
    oscillator.connect(gain);
    oscillatorB.connect(gain);
    gain.connect(destination);
    oscillator.start();
    oscillatorB.start();
    localAudioContext = context;
    localAudioNodes.push(oscillator, oscillatorB, gain, destination);
    context.resume().catch(() => {});
    window.setInterval(() => {
      if (context.state !== "running") {
        context.resume().catch(() => {});
      }
    }, 250);
    return destination.stream.getAudioTracks()[0];
  }

  function addSyntheticLocalMedia() {
    const stream = new MediaStream();
    const videoTrack = createSyntheticVideoTrack();
    const audioTrack = createSyntheticAudioTrack();

    stream.addTrack(videoTrack);
    stream.addTrack(audioTrack);
    pc.addTransceiver(videoTrack, { direction: "sendrecv", streams: [stream] });
    pc.addTransceiver(audioTrack, { direction: "sendrecv", streams: [stream] });
    state.localMediaStarted = true;
    recordEvent("local.media.started", {
      video: videoTrack.readyState,
      audio: audioTrack.readyState,
    });
  }

  function sendMessage(message) {
    state.messages.push({ direction: "out", message });
    ws.send(JSON.stringify(message));
  }

  async function createOffer() {
    pc = new RTCPeerConnection(rtcConfigFromQuery());
    window.__mrtcE2E.pc = pc;

    addSyntheticLocalMedia();
    preferCodec("video", "H264");
    preferCodec("audio", "opus");

    dataChannel = pc.createDataChannel("mrtc-e2e");
    window.__mrtcE2E.dataChannel = dataChannel;

    pc.addEventListener("connectionstatechange", () => {
      state.connectionState = pc.connectionState;
      recordEvent(`pc.${pc.connectionState}`);
    });
    pc.addEventListener("iceconnectionstatechange", () => {
      state.iceConnectionState = pc.iceConnectionState;
      recordEvent(`ice.${pc.iceConnectionState}`);
    });
    pc.addEventListener("signalingstatechange", () => {
      state.signalingState = pc.signalingState;
      recordEvent(`signaling.${pc.signalingState}`);
    });
    pc.addEventListener("track", attachRemoteTrack);
    pc.addEventListener("icecandidate", (event) => {
      if (event.candidate) {
        state.candidatesSent += 1;
        sendMessage({
          type: "candidate",
          candidate: event.candidate.candidate,
          sdpMid: event.candidate.sdpMid,
          sdpMLineIndex: event.candidate.sdpMLineIndex,
        });
      } else {
        recordEvent("ice.gathering.complete");
      }
    });

    dataChannel.addEventListener("open", () => {
      state.dataChannelState = "open";
      recordEvent("datachannel.open", { label: dataChannel.label });
    });
    dataChannel.addEventListener("close", () => {
      state.dataChannelState = "closed";
      recordEvent("datachannel.closed", { label: dataChannel.label });
    });
    dataChannel.addEventListener("message", (event) => {
      recordEvent("datachannel.message", { data: String(event.data) });
    });

    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);
    state.offerSent = true;
    sendMessage({ type: "offer", sdp: offer.sdp });
    recordEvent("offer.sent");
  }

  async function handleMessage(data) {
    const message = typeof data === "string" ? JSON.parse(data) : data;
    state.messages.push({ direction: "in", message });

    if (message.type === "answer") {
      await pc.setRemoteDescription({ type: "answer", sdp: message.sdp });
      state.answerApplied = true;
      recordEvent("answer.applied");
      while (pendingRemoteCandidates.length) {
        await applyRemoteCandidate(pendingRemoteCandidates.shift());
      }
    } else if (message.type === "candidate") {
      if (!state.answerApplied) {
        pendingRemoteCandidates.push(message);
        recordEvent("candidate.queued");
      } else {
        await applyRemoteCandidate(message);
      }
    } else if (message.type === "event") {
      recordEvent(`remote.${message.name || "event"}`, message.fields || {});
    } else if (message.type === "done") {
      recordEvent("remote.done", message.summary || {});
    } else if (message.type === "error") {
      recordError(message.stage || "remote", new Error(message.message || "remote error"));
    }
  }

  async function applyRemoteCandidate(message) {
    const candidate = String(message.candidate || "");
    const match = candidate.match(/\btyp\s+([a-z0-9]+)/i);

    state.candidatesReceived += 1;
    if (match) {
      state.remoteCandidateTypes.push(match[1].toLowerCase());
    }
    await pc.addIceCandidate({
      candidate: message.candidate,
      sdpMid: message.sdpMid || null,
      sdpMLineIndex: typeof message.sdpMLineIndex === "number" ? message.sdpMLineIndex : null,
    });
    recordEvent("candidate.applied", { candidateType: match ? match[1].toLowerCase() : null });
  }

  async function connect() {
    if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
      return;
    }
    const wsUrl = params.get("ws");
    if (!wsUrl) {
      recordError("query.ws", new Error("missing ?ws= WebSocket URL"));
      return;
    }

    ws = new WebSocket(wsUrl);
    window.__mrtcE2E.ws = ws;

    ws.addEventListener("open", async () => {
      try {
        sendMessage({ type: "hello", role: "browser" });
        await createOffer();
      } catch (error) {
        recordError("offer", error);
      }
    });
    ws.addEventListener("message", (event) => {
      handleMessage(event.data).catch((error) => recordError("message", error));
    });
    ws.addEventListener("close", () => recordEvent("ws.closed"));
    ws.addEventListener("error", () => recordError("ws", new Error("WebSocket error")));
    updateUi();
  }

  function waitForState(name, predicate, timeoutMs) {
    if (predicate()) {
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      const started = Date.now();
      const timer = window.setInterval(() => {
        if (predicate()) {
          window.clearInterval(timer);
          resolve();
        } else if (Date.now() - started > timeoutMs) {
          window.clearInterval(timer);
          reject(new Error(`timed out waiting for ${name}`));
        }
      }, 100);
    });
  }

  async function startManualFlow() {
    try {
      await connect();
      await waitForState("answer", () => state.answerApplied, 10_000);
      await waitForState("remote candidate", () => state.candidatesReceived > 0, 10_000);
      await waitForState("peer connection", () => state.connectionState === "connected", 15_000);
      await waitForState("local datachannel", () => state.dataChannelState === "open", 15_000);
      await waitForState("remote datachannel", () => state.events.some((event) => event.name === "remote.datachannel.open"), 15_000);
      state.mediaStartRequested = true;
      sendControlMessage({ type: "start-media" });
      recordEvent("media.start.requested");
      updateUi();
    } catch (error) {
      recordError("manual.start", error);
    }
  }

  async function getStatsSnapshot() {
    if (!pc) {
      return [];
    }
    const report = await pc.getStats();
    return Array.from(report.values()).map((item) => {
      const plain = {};
      Object.keys(item).forEach((key) => {
        plain[key] = item[key];
      });
      return plain;
    });
  }

  function getRemoteMediaState() {
    const video = document.getElementById("remote-video");
    const audio = document.getElementById("remote-audio");
    let audioEnergy = 0;

    if (remoteAudioAnalyser && remoteAudioData) {
      remoteAudioAnalyser.getByteTimeDomainData(remoteAudioData);
      for (const sample of remoteAudioData) {
        const centered = sample - 128;
        audioEnergy += centered * centered;
      }
      audioEnergy = Math.sqrt(audioEnergy / remoteAudioData.length) / 128;
    }
    return {
      video: video ? {
        readyState: video.readyState,
        videoWidth: video.videoWidth,
        videoHeight: video.videoHeight,
        currentTime: video.currentTime,
      } : null,
      audio: audio ? {
        readyState: audio.readyState,
        currentTime: audio.currentTime,
        energy: audioEnergy,
      } : null,
    };
  }

  function sendControlMessage(message) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      throw new Error("WebSocket is not open");
    }
    sendMessage(message);
  }

  function waitForEvent(name, timeoutMs) {
    const existing = state.events.find((event) => event.name === name);
    if (existing) {
      return Promise.resolve(existing);
    }

    return new Promise((resolve, reject) => {
      const timeout = window.setTimeout(() => {
        const pending = waiters.get(name) || [];
        waiters.set(name, pending.filter((waiter) => waiter.resolve !== resolve));
        reject(new Error(`timed out waiting for ${name}`));
      }, timeoutMs || 10_000);

      const waiter = {
        resolve: (event) => {
          window.clearTimeout(timeout);
          resolve(event);
        },
      };
      const pending = waiters.get(name) || [];
      pending.push(waiter);
      waiters.set(name, pending);
    });
  }

  window.__mrtcE2E = {
    pc: null,
    ws: null,
    dataChannel: null,
    getState: () => JSON.parse(JSON.stringify(state)),
    getStatsSnapshot,
    getRemoteMediaState,
    sendControlMessage,
    waitForEvent,
  };

  window.addEventListener("load", () => {
    const startButton = byId("start-button");
    if (startButton) {
      startButton.addEventListener("click", () => {
        startManualFlow().catch((error) => recordError("manual.start", error));
      });
    }
    updateUi();
    window.setInterval(updateUi, 500);
    if (!manualMode) {
      connect().catch((error) => recordError("connect", error));
    }
  });
}());
