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
  waitForBrowserInboundAudio,
  waitForBrowserInboundVideo,
};
