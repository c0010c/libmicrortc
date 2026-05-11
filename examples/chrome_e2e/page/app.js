const params = new URLSearchParams(window.location.search);

const state = {
  runId: params.get("runId") ?? crypto.randomUUID(),
  pc: null,
  ws: null,
  localStream: null,
  remoteRecorder: null,
  remoteRecordingChunks: [],
  audioContext: null,
  oscillator: null,
  animationFrame: 0,
  statsTimer: 0,
  candidateCount: 0,
  offerCreated: false,
  summary: { layer: "none", ok: false, manual_vlc_required: true },
  events: [],
};

const elements = {
  start: document.querySelector("#start-call"),
  stop: document.querySelector("#stop-run"),
  copy: document.querySelector("#copy-summary"),
  localVideo: document.querySelector("#local-video"),
  remoteVideo: document.querySelector("#remote-video"),
  canvas: document.querySelector("#synthetic-canvas"),
  localEmpty: document.querySelector("#local-empty"),
  remoteEmpty: document.querySelector("#remote-empty"),
  localTrackState: document.querySelector("#local-track-state"),
  remoteTrackState: document.querySelector("#remote-track-state"),
  summaryLayer: document.querySelector("#summary-layer"),
  candidateCount: document.querySelector("#candidate-count"),
  statsFrames: document.querySelector("#stats-frames"),
  statsBytes: document.querySelector("#stats-bytes"),
  connectionState: document.querySelector("#connection-state"),
  trackSummary: document.querySelector("#track-summary"),
  failureCallout: document.querySelector("#failure-callout"),
  failureTitle: document.querySelector("#failure-title"),
  failureDetail: document.querySelector("#failure-detail"),
  eventLog: document.querySelector("#event-log"),
};

function stage(testId) {
  return document.querySelector(`[data-testid="${testId}"]`);
}

function setStage(testId, value) {
  const item = stage(testId);
  item.dataset.state = value;
  item.title = `${item.querySelector(".label").textContent} ${value}`;
  item.querySelector(".state-text").textContent = value;
}

function logEvent(type, detail = {}) {
  const event = { at: new Date().toISOString(), type, ...detail };
  state.events.push(event);
  state.events = state.events.slice(-5);
  const latestEvents = state.events.map((item) => ({
    type: item.type,
    state: item.state,
    layer: item.layer,
  }));
  elements.eventLog.replaceChildren(...state.events.map((item) => {
    const li = document.createElement("li");
    li.textContent = `${item.type} ${item.state ?? item.layer ?? ""}`.trim();
    return li;
  }));
  window.__chromeE2E = {
    ...(window.__chromeE2E ?? {}),
    latestEvents,
  };
}

function updateSummary(summary) {
  state.summary = { manual_vlc_required: true, ...state.summary, ...summary };
  const displayLayer =
    (state.summary.pass === true || state.summary.ok === true) &&
    state.summary.manual_vlc_required === true &&
    (!state.summary.layer || state.summary.layer === "none")
      ? "manual_vlc_pending"
      : state.summary.layer ?? "none";
  elements.summaryLayer.textContent = displayLayer;
  if (state.summary.layer && state.summary.layer !== "none") {
    elements.failureTitle.textContent = `${state.summary.layer} failed`;
    elements.failureDetail.textContent = "Check the latest JSONL event and rerun after fixing the reported layer.";
    elements.failureCallout.hidden = false;
  } else {
    elements.failureCallout.hidden = true;
  }
}

function drawSyntheticFrame(startedAt) {
  const canvas = elements.canvas;
  const ctx = canvas.getContext("2d");
  const elapsed = (performance.now() - startedAt) / 1000;
  const barX = Math.floor((elapsed * 160) % canvas.width);

  ctx.fillStyle = "#F7F8FA";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = "#0B6B5E";
  ctx.fillRect(barX, 0, 96, canvas.height);
  ctx.fillStyle = "#FFFFFF";
  ctx.fillRect(40, 40, 440, 120);
  ctx.fillStyle = "#1F2937";
  ctx.font = "600 24px system-ui, sans-serif";
  ctx.fillText("Chrome synthetic media", 64, 112);
  ctx.font = "400 18px system-ui, sans-serif";
  ctx.fillText(`run ${state.runId.slice(0, 8)}`, 64, 144);

  state.animationFrame = requestAnimationFrame(() => drawSyntheticFrame(startedAt));
}

async function createSyntheticMedia() {
  drawSyntheticFrame(performance.now());
  const videoStream = elements.canvas.captureStream(25);

  const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
  const audioContext = new AudioContextCtor();
  const oscillator = audioContext.createOscillator();
  const gain = audioContext.createGain();
  const destination = audioContext.createMediaStreamDestination();
  oscillator.frequency.value = 440;
  gain.gain.value = 0.04;
  oscillator.connect(gain);
  gain.connect(destination);
  oscillator.start();

  const stream = new MediaStream([
    ...videoStream.getVideoTracks(),
    ...destination.stream.getAudioTracks(),
  ]);

  state.audioContext = audioContext;
  state.oscillator = oscillator;
  state.localStream = stream;
  elements.localVideo.srcObject = stream;
  elements.localEmpty.textContent = "Synthetic canvas and oscillator are running";
  elements.localTrackState.textContent = "audio 1 / video 1";
  return stream;
}

function connectSignaling() {
  const url = new URL("/ws", window.location.href);
  url.protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  const ws = new WebSocket(url);
  state.ws = ws;

  const opened = new Promise((resolve, reject) => {
    ws.addEventListener("open", resolve, { once: true });
    ws.addEventListener("error", reject, { once: true });
  });

  ws.addEventListener("open", () => {
    setStage("stage-signaling", "connected");
    logEvent("signaling.connected");
    ws.send(JSON.stringify({ type: "hello", runId: state.runId, role: "chrome" }));
  });

  ws.addEventListener("message", async (event) => {
    const message = JSON.parse(event.data);
    if (message.type === "answer" && message.sdp) {
      await state.pc.setRemoteDescription({ type: "answer", sdp: message.sdp });
      logEvent("answer.received");
    } else if (message.type === "candidate" && message.candidate) {
      await state.pc.addIceCandidate(message.candidate);
      logEvent("remote_candidate.received");
    } else if (message.type === "summary") {
      updateSummary(message.summary ?? message);
      logEvent("summary", { layer: message.summary?.layer ?? message.layer });
    } else if (message.type === "status") {
      logEvent("status", { state: message.state });
    } else if (message.type === "error") {
      updateSummary({ layer: "signaling", ok: false });
      setStage("stage-signaling", "failed");
      logEvent("error", { layer: "signaling" });
    }
  });

  ws.addEventListener("close", () => {
    if (state.pc) {
      logEvent("signaling.closed");
    }
  });

  ws.addEventListener("error", () => {
    setStage("stage-signaling", "failed");
    updateSummary({ layer: "signaling", ok: false });
    logEvent("signaling.error");
  });

  return opened;
}

function sendSignal(message) {
  if (state.ws?.readyState === WebSocket.OPEN) {
    state.ws.send(JSON.stringify({ runId: state.runId, ...message }));
  }
}

function installPeerConnectionHandlers(pc) {
  pc.addEventListener("icecandidate", (event) => {
    if (!event.candidate) {
      return;
    }
    state.candidateCount += 1;
    elements.candidateCount.textContent = String(state.candidateCount);
    sendSignal({ type: "candidate", candidate: event.candidate.toJSON() });
  });

  pc.addEventListener("iceconnectionstatechange", () => {
    const iceState = pc.iceConnectionState;
    const stageState = iceState === "connected" || iceState === "completed" ? "connected" : iceState === "failed" ? "failed" : "running";
    setStage("stage-ice", stageState);
    logEvent("ice", { state: iceState });
  });

  pc.addEventListener("connectionstatechange", () => {
    elements.connectionState.textContent = pc.connectionState;
    if (pc.connectionState === "connected") {
      setStage("stage-dtls", "connected");
      setStage("stage-srtp", "ready");
    } else if (pc.connectionState === "failed") {
      setStage("stage-dtls", "failed");
      setStage("stage-srtp", "failed");
    }
  });

  pc.addEventListener("track", (event) => {
    const [stream] = event.streams;
    if (stream) {
      elements.remoteVideo.srcObject = stream;
      if (!state.remoteRecorder && window.MediaRecorder) {
        state.remoteRecordingChunks = [];
        state.remoteRecorder = new MediaRecorder(stream);
        state.remoteRecorder.addEventListener("dataavailable", (recordEvent) => {
          if (recordEvent.data && recordEvent.data.size > 0) {
            state.remoteRecordingChunks.push(recordEvent.data);
          }
        });
        state.remoteRecorder.start(1000);
      }
    }
    elements.remoteVideo.muted = true;
    elements.remoteVideo.play().catch(() => {});
    elements.remoteTrackState.textContent = "receiving";
    elements.remoteEmpty.textContent = "Remote media track attached";
    setStage("stage-rtp", "receiving");
    setStage("stage-media-files", "running");
    logEvent("track.received", { state: event.track.kind });
  });
}

window.__chromeE2EStopRemoteRecording = async function stopRemoteRecording() {
  const recorder = state.remoteRecorder;
  if (!recorder) {
    return null;
  }
  if (recorder.state !== "inactive") {
    await new Promise((resolve) => {
      recorder.addEventListener("stop", resolve, { once: true });
      recorder.stop();
    });
  }
  const blob = new Blob(state.remoteRecordingChunks, {
    type: recorder.mimeType || "video/webm",
  });
  const bytes = Array.from(new Uint8Array(await blob.arrayBuffer()));
  return { mimeType: blob.type, bytes };
};

async function collectStats() {
  if (!state.pc) {
    return;
  }
  const report = await state.pc.getStats();
  let frames = 0;
  let bytes = 0;
  let audioTracks = 0;
  let videoTracks = 0;

  for (const stat of report.values()) {
    if (stat.type === "outbound-rtp" || stat.type === "inbound-rtp") {
      frames += stat.framesEncoded ?? stat.framesDecoded ?? 0;
      bytes += stat.bytesSent ?? stat.bytesReceived ?? 0;
      if (stat.kind === "audio" || stat.mediaType === "audio") {
        audioTracks += 1;
      }
      if (stat.kind === "video" || stat.mediaType === "video") {
        videoTracks += 1;
      }
    }
    if (stat.type === "candidate-pair" && stat.state === "succeeded") {
      setStage("stage-rtcp", "running");
    }
  }

  elements.statsFrames.textContent = String(frames);
  elements.statsBytes.textContent = String(bytes);
  elements.trackSummary.textContent = `audio ${audioTracks} / video ${videoTracks}`;
  window.__chromeE2E = {
    offerCreated: state.offerCreated,
    candidateCount: state.candidateCount,
    summary: state.summary,
    latestEvents: state.events,
  };
}

async function startCall() {
  elements.start.disabled = true;
  updateSummary({ layer: "none", ok: false });
  setStage("stage-signaling", "running");
  setStage("stage-ice", "running");
  setStage("stage-dtls", "pending");
  setStage("stage-srtp", "pending");
  setStage("stage-rtp", "running");
  setStage("stage-rtcp", "pending");
  setStage("stage-media-files", "pending");

  const signalingReady = connectSignaling();
  const stream = await createSyntheticMedia();
  const pc = new RTCPeerConnection({ iceServers: [] });
  state.pc = pc;
  installPeerConnectionHandlers(pc);

  for (const track of stream.getTracks()) {
    pc.addTrack(track, stream);
  }

  const offer = await pc.createOffer();
  await pc.setLocalDescription(offer);
  await signalingReady;
  state.offerCreated = true;
  window.__chromeE2E = {
    offerCreated: true,
    candidateCount: state.candidateCount,
    summary: state.summary,
    latestEvents: state.events,
  };
  sendSignal({ type: "offer", sdp: offer.sdp });
  logEvent("offer.sent");

  state.statsTimer = window.setInterval(collectStats, 500);
  await collectStats();
}

function stopRun() {
  if (!window.confirm("Stop Run: Stop tracks, close signaling, and end the current E2E run?")) {
    return;
  }
  window.clearInterval(state.statsTimer);
  cancelAnimationFrame(state.animationFrame);
  state.localStream?.getTracks().forEach((track) => track.stop());
  state.oscillator?.stop();
  state.audioContext?.close();
  state.pc?.close();
  state.ws?.close();
  elements.start.disabled = false;
  logEvent("run.stopped");
}

async function copySummary() {
  await navigator.clipboard.writeText(JSON.stringify(state.summary));
  logEvent("summary.copied");
}

elements.start.addEventListener("click", () => {
  startCall().catch((error) => {
    console.error(error);
    updateSummary({ layer: "signaling", ok: false });
    setStage("stage-signaling", "failed");
    elements.start.disabled = false;
  });
});
elements.stop.addEventListener("click", stopRun);
elements.copy.addEventListener("click", copySummary);

window.__chromeE2E = {
  offerCreated: false,
  candidateCount: 0,
  summary: state.summary,
  latestEvents: [],
};
