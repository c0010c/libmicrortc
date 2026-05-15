(function () {
  "use strict";

  const params = new URLSearchParams(window.location.search);
  const wsUrl = params.get("ws");
  const peers = new Map();
  const memorySamples = [];
  let ws = null;
  let localStreamPromise = null;
  let started = false;
  let addTimer = null;
  let answererDataChannel = false;

  function byId(id) {
    return document.getElementById(id);
  }

  function setText(id, text) {
    const node = byId(id);
    if (node) node.textContent = text;
  }

  function eventLine(text) {
    const list = byId("events");
    const item = document.createElement("li");
    item.textContent = `${new Date().toLocaleTimeString()} ${text}`;
    list.prepend(item);
    while (list.children.length > 80) list.removeChild(list.lastChild);
  }

  function badge(value) {
    if (["connected", "open", "playing"].includes(value)) return "badge ok";
    if (["failed", "closed", "disconnected"].includes(value)) return "badge bad";
    return "badge";
  }

  async function localStream() {
    if (localStreamPromise) return localStreamPromise;
    localStreamPromise = Promise.resolve().then(() => {
      const canvas = document.createElement("canvas");
      canvas.width = 640;
      canvas.height = 360;
      const ctx = canvas.getContext("2d");
      let frame = 0;
      window.setInterval(() => {
        ctx.fillStyle = "#1f2937";
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        ctx.fillStyle = "#f8fafc";
        ctx.font = "32px system-ui";
        ctx.fillText(`fanout ${frame++}`, 36, 72);
      }, 33);
      const stream = canvas.captureStream(30);
      const audio = new AudioContext();
      const oscillator = audio.createOscillator();
      const gain = audio.createGain();
      const dest = audio.createMediaStreamDestination();
      oscillator.frequency.value = 330;
      gain.gain.value = 0.02;
      oscillator.connect(gain).connect(dest);
      oscillator.start();
      dest.stream.getAudioTracks().forEach((track) => stream.addTrack(track));
      return stream;
    });
    return localStreamPromise;
  }

  function preferCodec(pc, kind, codecName) {
    if (!window.RTCRtpSender || !RTCRtpSender.getCapabilities) return;
    const caps = RTCRtpSender.getCapabilities(kind);
    if (!caps || !caps.codecs) return;
    const preferred = caps.codecs.filter((codec) => codec.mimeType.toLowerCase() === `${kind}/${codecName}`.toLowerCase());
    if (!preferred.length) return;
    const rest = caps.codecs.filter((codec) => !preferred.includes(codec));
    pc.getTransceivers().forEach((transceiver) => {
      if (transceiver.sender && transceiver.sender.track && transceiver.sender.track.kind === kind && transceiver.setCodecPreferences) {
        transceiver.setCodecPreferences(preferred.concat(rest));
      }
    });
  }

  function createPeerElement(peer) {
    const container = document.createElement("article");
    container.className = "peer";
    container.id = `peer-${peer.id}`;
    container.innerHTML = `
      <h2>${peer.id}</h2>
      <video playsinline autoplay muted></video>
      <audio autoplay controls></audio>
      <div class="status">
        <span>Peer</span><span data-field="peer" class="badge">new</span>
        <span>ICE</span><span data-field="ice" class="badge">new</span>
        <span>DataChannel</span><span data-field="data" class="badge">new</span>
        <span>Video</span><span data-field="video" class="badge">pending</span>
        <span>Audio</span><span data-field="audio" class="badge">pending</span>
        <span>C frames</span><span data-field="frames" class="badge">0 / 0</span>
      </div>`;
    byId("peers").appendChild(container);
    peer.element = container;
    peer.video = container.querySelector("video");
    peer.audio = container.querySelector("audio");
  }

  function updatePeer(peer) {
    if (!peer.element) return;
    const fields = {
      peer: peer.pc ? peer.pc.connectionState : "new",
      ice: peer.pc ? peer.pc.iceConnectionState : "new",
      data: peer.dataState || "new",
      video: peer.video.readyState >= 2 && peer.video.videoWidth > 0 ? "playing" : "pending",
      audio: peer.audio.readyState >= 2 ? "playing" : "pending",
      frames: `${peer.videoFrames || 0} / ${peer.audioFrames || 0}`,
    };
    Object.entries(fields).forEach(([key, value]) => {
      const node = peer.element.querySelector(`[data-field="${key}"]`);
      if (node) {
        node.textContent = value;
        node.className = badge(value);
      }
    });
  }

  function send(message) {
    ws.send(JSON.stringify(message));
  }

  function dataChannelEnabled() {
    const checkbox = byId("data-channel");
    return answererDataChannel && checkbox && checkbox.checked;
  }

  async function addPeer() {
    const peerId = `peer-${peers.size + 1}`;
    const peer = {
      id: peerId,
      dataState: dataChannelEnabled() ? "new" : "off",
      videoFrames: 0,
      audioFrames: 0,
      answerApplied: false,
      pendingCandidates: [],
    };
    peers.set(peerId, peer);
    createPeerElement(peer);

    const stream = await localStream();
    const pc = new RTCPeerConnection();
    peer.pc = pc;
    stream.getTracks().forEach((track) => pc.addTrack(track, stream));
    preferCodec(pc, "video", "H264");

    if (dataChannelEnabled()) {
      const dc = pc.createDataChannel("fanout-memory");
      dc.onopen = () => {
        peer.dataState = "open";
        dc.send(`ping:${peerId}`);
        updatePeer(peer);
      };
      dc.onclose = () => {
        peer.dataState = "closed";
        updatePeer(peer);
      };
      dc.onmessage = () => updatePeer(peer);
    }

    pc.onconnectionstatechange = () => updatePeer(peer);
    pc.oniceconnectionstatechange = () => updatePeer(peer);
    pc.onicecandidate = (event) => {
      if (event.candidate) {
        send({
          type: "candidate",
          peerId,
          candidate: event.candidate.candidate,
          sdpMid: event.candidate.sdpMid,
          sdpMLineIndex: event.candidate.sdpMLineIndex,
        });
      }
    };
    pc.ontrack = (event) => {
      const streamForTrack = event.streams[0] || new MediaStream([event.track]);
      if (event.track.kind === "video") peer.video.srcObject = streamForTrack;
      if (event.track.kind === "audio") peer.audio.srcObject = streamForTrack;
      updatePeer(peer);
    };

    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);
    send({ type: "offer", peerId, sdp: offer.sdp });
    eventLine(`created ${peerId}`);
    updatePeer(peer);
    return peer;
  }

  function schedulePeers() {
    const intervalSec = Math.max(1, Number(byId("interval").value || 5));
    const maxPeers = Math.max(1, Math.min(32, Number(byId("max-peers").value || 3)));
    addPeer().then(() => send({ type: "start-media" })).catch((error) => eventLine(error.message));
    addTimer = window.setInterval(() => {
      if (peers.size >= maxPeers) {
        window.clearInterval(addTimer);
        return;
      }
      addPeer().catch((error) => eventLine(error.message));
    }, intervalSec * 1000);
  }

  function handleAnswer(message) {
    const peer = peers.get(message.peerId);
    if (!peer) return;
    peer.pc
      .setRemoteDescription({ type: "answer", sdp: message.sdp })
      .then(async () => {
        peer.answerApplied = true;
        while (peer.pendingCandidates.length) {
          await applyRemoteCandidate(peer, peer.pendingCandidates.shift());
        }
      })
      .catch((error) => eventLine(error.message));
  }

  function handleCandidate(message) {
    const peer = peers.get(message.peerId);
    if (!peer || !message.candidate) return;
    if (!peer.answerApplied) {
      peer.pendingCandidates.push(message);
      return;
    }
    applyRemoteCandidate(peer, message).catch((error) => eventLine(error.message));
  }

  async function applyRemoteCandidate(peer, message) {
    await peer.pc.addIceCandidate({
      candidate: message.candidate,
      sdpMid: message.sdpMid || null,
      sdpMLineIndex: typeof message.sdpMLineIndex === "number" ? message.sdpMLineIndex : null,
    });
  }

  function handleEvent(message) {
    if (message.peerId) {
      const peer = peers.get(message.peerId);
      if (peer && message.name === "media.stats") {
        peer.videoFrames = message.fields.videoFrames;
        peer.audioFrames = message.fields.audioFrames;
        updatePeer(peer);
      }
      eventLine(`${message.peerId} ${message.name}`);
    } else {
      eventLine(message.name || "event");
    }
  }

  function drawChart() {
    const canvas = byId("memory-chart");
    const ctx = canvas.getContext("2d");
    const width = canvas.width;
    const height = canvas.height;
    ctx.clearRect(0, 0, width, height);
    ctx.strokeStyle = "#d8dee9";
    ctx.beginPath();
    ctx.moveTo(0, height - 24);
    ctx.lineTo(width, height - 24);
    ctx.stroke();
    if (memorySamples.length < 2) return;
    const values = memorySamples.map((sample) => sample.deltaPrivateDirtyKiB);
    const max = Math.max(1, ...values);
    ctx.strokeStyle = "#1f6feb";
    ctx.lineWidth = 2;
    ctx.beginPath();
    values.forEach((value, index) => {
      const x = (index / Math.max(1, values.length - 1)) * width;
      const y = height - 24 - (value / max) * (height - 40);
      if (index === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
  }

  function handleMemory(message) {
    const sample = message.sample;
    memorySamples.push(sample);
    while (memorySamples.length > 300) memorySamples.shift();
    setText("delta-private", `${sample.deltaPrivateDirtyKiB} KiB`);
    setText("delta-anon", `${sample.deltaAnonymousKiB} KiB`);
    setText("pss", `${sample.pssKiB} KiB`);
    setText("rss", `${sample.rssKiB} KiB`);
    setText("rss-anon", `${sample.rssAnonKiB} KiB`);
    setText("rss-file", `${sample.rssFileKiB} KiB`);
    setText("threads-fds", `${sample.threads} / ${sample.fdCount}`);
    drawChart();
  }

  function handleHello(message) {
    answererDataChannel = message.dataChannel === true;
    const checkbox = byId("data-channel");
    if (checkbox) {
      checkbox.disabled = !answererDataChannel || started;
      if (!answererDataChannel) checkbox.checked = false;
    }
    eventLine(`answerer ready, datachannel ${answererDataChannel ? "on" : "off"}`);
  }

  function connect() {
    ws = new WebSocket(wsUrl);
    ws.onopen = () => {
      setText("runner-status", "runner connected");
      send({ type: "hello", role: "browser" });
    };
    ws.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.type === "answer") handleAnswer(message);
      else if (message.type === "candidate") handleCandidate(message);
      else if (message.type === "event") handleEvent(message);
      else if (message.type === "memory") handleMemory(message);
      else if (message.type === "hello") handleHello(message);
      else if (message.type === "runner") setText("runner-status", `PID ${message.pid}`);
      else if (message.type === "error") eventLine(`${message.stage}: ${message.message}`);
    };
    ws.onclose = () => setText("runner-status", "runner closed");
  }

  byId("start").addEventListener("click", () => {
    if (started) return;
    started = true;
    const checkbox = byId("data-channel");
    if (checkbox) checkbox.disabled = true;
    schedulePeers();
  });
  byId("stop").addEventListener("click", () => {
    send({ type: "stop" });
    window.clearInterval(addTimer);
  });

  if (!wsUrl) {
    setText("runner-status", "missing ws query parameter");
  } else {
    connect();
  }
  window.setInterval(() => peers.forEach(updatePeer), 500);
}());
