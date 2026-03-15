#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <rtc_signal_b64_cli_path>" >&2
  exit 2
fi

cli_bin="$1"
if [[ ! -x "$cli_bin" ]]; then
  echo "cli binary not found: $cli_bin" >&2
  exit 1
fi

tmp_out="$(mktemp)"
tmp_err="$(mktemp)"
tmp_offer="$(mktemp)"
tmp_long_offer="$(mktemp)"
tmp_offer_b64="$(mktemp)"
tmp_answer_sdp="$(mktemp)"
tmp_big_raw="$(mktemp)"
tmp_answer_b64="$(mktemp)"
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_offer" "$tmp_long_offer" "$tmp_offer_b64" "$tmp_answer_sdp" "$tmp_big_raw" "$tmp_answer_b64"' EXIT

cat <<'EOF_OFFER' >"$tmp_offer"
v=0
o=- 897654321234567890 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS *
a=ice-ufrag:uf1234
a=ice-pwd:pw12345678901234567890
a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00
a=setup:actpass
m=audio 9 UDP/TLS/RTP/SAVPF 111 8 0
c=IN IP4 0.0.0.0
a=mid:0
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtpmap:8 PCMA/8000
a=rtpmap:0 PCMU/8000
a=ssrc:3001001 cname:audio-track
a=candidate:1 1 udp 2130706431 127.0.0.1 5000 typ host
m=video 9 UDP/TLS/RTP/SAVPF 96 97
c=IN IP4 0.0.0.0
a=mid:1
a=recvonly
a=rtcp-mux
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1;profile-level-id=42e01f;level-asymmetry-allowed=1
a=rtpmap:97 VP8/90000
a=ssrc:3002001 cname:video-track
a=candidate:2 1 udp 2130706431 127.0.0.1 5000 typ host
EOF_OFFER

base64 -w 0 "$tmp_offer" >"$tmp_offer_b64"

if ! "$cli_bin" --run-ms 200 <"$tmp_offer_b64" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected success for base64 offer" >&2
  cat "$tmp_out" >&2
  cat "$tmp_err" >&2
  exit 1
fi

# stdout should include logs and exactly one base64 answer line.
grep -Eq 'offer_b64_decoded|answer_b64_encoded' "$tmp_out"

grep -E '^[A-Za-z0-9+/=]+$' "$tmp_out" | head -n1 >"$tmp_answer_b64"
if [[ ! -s "$tmp_answer_b64" ]]; then
  echo "expected base64 answer line in stdout" >&2
  cat "$tmp_out" >&2
  exit 1
fi

tr -d '\r\n' <"$tmp_answer_b64" | base64 -d >"$tmp_answer_sdp"
grep -q '^v=0' "$tmp_answer_sdp"
grep -q '^a=ice-ufrag:' "$tmp_answer_sdp"
grep -q '^m=audio ' "$tmp_answer_sdp"
grep -q '^m=video ' "$tmp_answer_sdp"
grep -q '^a=sendonly' "$tmp_answer_sdp"

if "$cli_bin" --run-ms 50 </dev/null >"$tmp_out" 2>"$tmp_err"; then
  echo "expected failure for empty offer b64" >&2
  exit 1
fi
grep -Eq 'offer|base64|empty|invalid|failed' "$tmp_out"

if echo '%%%INVALID%%%' | "$cli_bin" --run-ms 50 >"$tmp_out" 2>"$tmp_err"; then
  echo "expected failure for invalid base64 characters" >&2
  exit 1
fi
grep -Eq 'base64|invalid|failed' "$tmp_out"

if printf 'A===' | "$cli_bin" --run-ms 50 >"$tmp_out" 2>"$tmp_err"; then
  echo "expected failure for invalid base64 padding" >&2
  exit 1
fi
grep -Eq 'base64|padding|invalid|failed' "$tmp_out"

# overflow: input size exceeds base64 receive buffer
head -c 90000 /dev/zero | tr '\0' 'A' >"$tmp_big_raw"
if "$cli_bin" --run-ms 50 <"$tmp_big_raw" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected failure for oversized base64 input" >&2
  exit 1
fi
grep -Eq 'buffer_too_small|overflow|too_small|base64' "$tmp_out"

# boundary: long offer but still within limit should pass
{
  cat "$tmp_offer"
  for i in $(seq 1 900); do
    echo "a=x-pad-${i}:1234567890123456789012345678901234567890"
  done
} >"$tmp_long_offer"
base64 -w 0 "$tmp_long_offer" >"$tmp_offer_b64"

if ! "$cli_bin" --run-ms 120 <"$tmp_offer_b64" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected success for long base64 offer" >&2
  cat "$tmp_out" >&2
  cat "$tmp_err" >&2
  exit 1
fi

grep -E '^[A-Za-z0-9+/=]+$' "$tmp_out" | head -n1 >"$tmp_answer_b64"
tr -d '\r\n' <"$tmp_answer_b64" | base64 -d >"$tmp_answer_sdp"
grep -q '^a=setup:' "$tmp_answer_sdp"
if grep -q 'buffer_too_small' "$tmp_out"; then
  echo "unexpected overflow for long base64 offer" >&2
  exit 1
fi

echo "rtc_signal_b64_cli smoke passed"
