"use strict";

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function inboundStats(stats, kind) {
  return stats.filter((item) => item.type === "inbound-rtp" && item.kind === kind && !item.isRemote);
}

function sumStats(items, fields) {
  const total = {};
  for (const field of fields) {
    total[field] = 0;
  }
  for (const item of items) {
    for (const field of fields) {
      if (typeof item[field] === "number" && Number.isFinite(item[field])) {
        total[field] += item[field];
      }
    }
  }
  return total;
}

function increased(current, previous, field) {
  return current[field] > previous[field];
}

function mapStatsById(stats) {
  const byId = new Map();
  for (const item of stats) {
    if (item && item.id) {
      byId.set(item.id, item);
    }
  }
  return byId;
}

function selectedPairFromStats(stats) {
  const byId = mapStatsById(stats);
  const selectedPairIds = [];
  const fallbackPairIds = [];

  for (const item of stats) {
    if (item.type === "transport" && item.selectedCandidatePairId) {
      selectedPairIds.push(item.selectedCandidatePairId);
      const pair = byId.get(item.selectedCandidatePairId);
      if (pair) {
        return { pair, selectedPairIds, fallbackPairIds };
      }
    }
  }

  for (const item of stats) {
    if (item.type !== "candidate-pair") {
      continue;
    }
    const succeeded = item.state === "succeeded" || item.state === "connected";
    const nominated = item.nominated === true || item.selected === true;
    if (succeeded && nominated) {
      fallbackPairIds.push(item.id);
      return { pair: item, selectedPairIds, fallbackPairIds };
    }
    if (succeeded || nominated) {
      fallbackPairIds.push(item.id);
    }
  }

  return { pair: null, selectedPairIds, fallbackPairIds };
}

function relayCandidateSummary(pair, localCandidate) {
  return {
    status: "passed",
    selected_pair_id: pair.id || null,
    local_candidate_id: pair.localCandidateId || null,
    local_candidate_type: localCandidate.candidateType || null,
    protocol: localCandidate.protocol || null,
    relay_protocol: localCandidate.relayProtocol || null,
    selected_pair_bytes: {
      sent: Number(pair.bytesSent || 0),
      received: Number(pair.bytesReceived || 0),
    },
    selected_pair_packets: {
      sent: Number(pair.packetsSent || 0),
      received: Number(pair.packetsReceived || 0),
    },
  };
}

async function sampleBrowserMedia(page, kind) {
  return page.evaluate(async (mediaKind) => {
    const api = window.__mrtcE2E;
    return {
      media: api.getRemoteMediaState(),
      stats: await api.getStatsSnapshot(),
      kind: mediaKind,
    };
  }, kind);
}

async function waitForIncreasingInbound(page, kind, options = {}) {
  const timeoutMs = options.timeoutMs || 15_000;
  const intervalMs = options.intervalMs || 500;
  const deadline = Date.now() + timeoutMs;
  let last = null;
  let increases = 0;
  let lastDetail = null;

  while (Date.now() < deadline) {
    const sample = await sampleBrowserMedia(page, kind);
    const totals = sumStats(inboundStats(sample.stats, kind), [
      "bytesReceived",
      "packetsReceived",
      "framesDecoded",
      "totalAudioEnergy",
      "totalSamplesReceived",
    ]);
    lastDetail = {
      totals,
      media: sample.media,
    };
    if (last &&
        increased(totals, last, "bytesReceived") &&
        increased(totals, last, "packetsReceived")) {
      increases += 1;
      if (increases >= 2) {
        return lastDetail;
      }
    }
    last = totals;
    await sleep(intervalMs);
  }
  throw new Error(`${kind} inbound RTP did not increase twice; last=${JSON.stringify(lastDetail)}`);
}

async function waitForSelectedRelayCandidate(page, options = {}) {
  const timeoutMs = options.timeoutMs || 15_000;
  const intervalMs = options.intervalMs || 500;
  const deadline = Date.now() + timeoutMs;
  let lastDetail = null;

  while (Date.now() < deadline) {
    const stats = await page.evaluate(async () => window.__mrtcE2E.getStatsSnapshot());
    const byId = mapStatsById(stats);
    const selected = selectedPairFromStats(stats);
    const pair = selected.pair;
    const localCandidate = pair && pair.localCandidateId ? byId.get(pair.localCandidateId) : null;

    lastDetail = {
      selectedPairIds: selected.selectedPairIds,
      fallbackPairIds: selected.fallbackPairIds,
      selectedPairId: pair && pair.id ? pair.id : null,
      localCandidateId: pair && pair.localCandidateId ? pair.localCandidateId : null,
      localCandidateType: localCandidate && localCandidate.candidateType ? localCandidate.candidateType : null,
    };

    if (pair && localCandidate && localCandidate.candidateType === "relay") {
      return relayCandidateSummary(pair, localCandidate);
    }
    await sleep(intervalMs);
  }

  throw new Error(`selected relay candidate not found; inspected=${JSON.stringify(lastDetail)}`);
}

async function waitForBrowserInboundVideo(page, options = {}) {
  const detail = await waitForIncreasingInbound(page, "video", options);
  const video = detail.media.video || {};

  if (video.readyState < 2 || video.videoWidth <= 0 || video.videoHeight <= 0) {
    throw new Error(`video element not ready; readyState=${video.readyState} width=${video.videoWidth} height=${video.videoHeight} stats=${JSON.stringify(detail.totals)}`);
  }
  if (detail.totals.framesDecoded && detail.totals.framesDecoded <= 0) {
    throw new Error(`video framesDecoded did not grow; stats=${JSON.stringify(detail.totals)}`);
  }
  return {
    status: "passed",
    readyState: video.readyState,
    videoWidth: video.videoWidth,
    videoHeight: video.videoHeight,
    bytesReceived: detail.totals.bytesReceived,
    packetsReceived: detail.totals.packetsReceived,
    framesDecoded: detail.totals.framesDecoded,
  };
}

async function waitForBrowserInboundAudio(page, options = {}) {
  const detail = await waitForIncreasingInbound(page, "audio", options);
  const audio = detail.media.audio || {};
  const hasEnergy = audio.energy > (options.energyThreshold || 0.001) ||
    detail.totals.totalAudioEnergy > 0 ||
    detail.totals.totalSamplesReceived > 0;

  if (!hasEnergy) {
    throw new Error(`audio playout evidence missing; readyState=${audio.readyState} energy=${audio.energy} stats=${JSON.stringify(detail.totals)}`);
  }
  return {
    status: "passed",
    readyState: audio.readyState,
    energy: audio.energy,
    bytesReceived: detail.totals.bytesReceived,
    packetsReceived: detail.totals.packetsReceived,
    totalAudioEnergy: detail.totals.totalAudioEnergy,
    totalSamplesReceived: detail.totals.totalSamplesReceived,
  };
}

module.exports = {
  waitForSelectedRelayCandidate,
  waitForBrowserInboundAudio,
  waitForBrowserInboundVideo,
};
