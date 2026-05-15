#!/usr/bin/env node
"use strict";

const fs = require("node:fs");

if (process.argv.length !== 4) {
  process.stderr.write("Usage: ogg-opus-to-packets.js <input.ogg> <output.opus>\n");
  process.exit(2);
}

const input = fs.readFileSync(process.argv[2]);
const packets = [];
let offset = 0;
let current = [];

while (offset + 27 <= input.length) {
  if (input.toString("ascii", offset, offset + 4) !== "OggS") {
    throw new Error(`invalid Ogg capture pattern at offset ${offset}`);
  }
  const segmentCount = input[offset + 26];
  const headerEnd = offset + 27 + segmentCount;
  if (headerEnd > input.length) {
    throw new Error("truncated Ogg segment table");
  }
  const laces = Array.from(input.subarray(offset + 27, headerEnd));
  const payloadSize = laces.reduce((total, value) => total + value, 0);
  const payloadStart = headerEnd;
  const payloadEnd = payloadStart + payloadSize;
  if (payloadEnd > input.length) {
    throw new Error("truncated Ogg payload");
  }
  let cursor = payloadStart;
  for (const lace of laces) {
    current.push(input.subarray(cursor, cursor + lace));
    cursor += lace;
    if (lace < 255) {
      packets.push(Buffer.concat(current));
      current = [];
    }
  }
  offset = payloadEnd;
}

const audioPackets = packets.filter((packet) =>
  !packet.subarray(0, 8).equals(Buffer.from("OpusHead")) &&
  !packet.subarray(0, 8).equals(Buffer.from("OpusTags"))
);

const chunks = [];
for (const packet of audioPackets) {
  if (packet.length <= 0 || packet.length > 0xffff) {
    throw new Error(`unsupported Opus packet length: ${packet.length}`);
  }
  const prefix = Buffer.alloc(2);
  prefix.writeUInt16BE(packet.length, 0);
  chunks.push(prefix, packet);
}

fs.writeFileSync(process.argv[3], Buffer.concat(chunks));
process.stdout.write(`wrote ${audioPackets.length} Opus packets\n`);
