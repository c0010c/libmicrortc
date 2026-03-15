#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <rtc_signal_cli_path>" >&2
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
tmp_video="$(mktemp)"
tmp_b64_sdp="$(mktemp)"
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_offer" "$tmp_video" "$tmp_b64_sdp"' EXIT

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
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtpmap:8 PCMA/8000
a=rtpmap:0 PCMU/8000
a=ssrc:3001001 cname:audio-track
a=candidate:1 1 udp 2130706431 127.0.0.1 5000 typ host
m=video 9 UDP/TLS/RTP/SAVPF 96 97
c=IN IP4 0.0.0.0
a=mid:1
a=rtcp-mux
a=rtpmap:96 H264/90000
a=fmtp:96 packetization-mode=1;profile-level-id=42e01f;level-asymmetry-allowed=1
a=rtpmap:97 VP8/90000
a=ssrc:3002001 cname:video-track
a=candidate:2 1 udp 2130706431 127.0.0.1 5000 typ host
EOF_OFFER

if ! "$cli_bin" --run-ms 200 <"$tmp_offer" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected success for valid offer" >&2
  cat "$tmp_err" >&2
  exit 1
fi

grep -q "^v=0" "$tmp_out"
grep -q "^a=ice-ufrag:" "$tmp_out"
grep -q "^a=ice-pwd:" "$tmp_out"
grep -q "^a=fingerprint:" "$tmp_out"
grep -q "^m=audio " "$tmp_out"
grep -q "^m=video " "$tmp_out"
grep -q "^a=setup:" "$tmp_out"
if grep -Eq "^(OK|ERR|EVENT) " "$tmp_out"; then
  echo "stdout must contain SDP only" >&2
  exit 1
fi

if ! "$cli_bin" --answer-b64 --run-ms 200 <"$tmp_offer" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected success for --answer-b64" >&2
  cat "$tmp_err" >&2
  exit 1
fi
if ! tr -d '\r\n' <"$tmp_out" | grep -Eq '^[A-Za-z0-9+/=]+$'; then
  echo "expected base64-only output for --answer-b64" >&2
  exit 1
fi
tr -d '\r\n' <"$tmp_out" | base64 -d >"$tmp_b64_sdp"
grep -q "^v=0" "$tmp_b64_sdp"
grep -q "^a=ice-ufrag:" "$tmp_b64_sdp"

printf '\x00\x00\x00\x01\x67\x42\xe0\x1e\x8d\x68\x54\x05\x01\xed\x00\xf0\x88\x45\x80\x00\x00\x00\x01\x68\xce\x06\xe2\x00\x00\x00\x01\x65\x88\x80\x20\x07\xbf\xfe\xf7\xd9\x20' >"$tmp_video"
if ! "$cli_bin" --video-file "$tmp_video" --run-ms 200 <"$tmp_offer" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected success with readable --video-file" >&2
  cat "$tmp_err" >&2
  exit 1
fi
grep -q "^a=setup:" "$tmp_out"

if ! "$cli_bin" --video-file "$tmp_video.missing" --run-ms 200 <"$tmp_offer" >"$tmp_out" 2>"$tmp_err"; then
  echo "expected fallback success for missing --video-file" >&2
  cat "$tmp_err" >&2
  exit 1
fi
grep -q "^a=setup:" "$tmp_out"
grep -q "video_file_open_failed" "$tmp_err"

if "$cli_bin" --run-ms 50 </dev/null >"$tmp_out" 2>"$tmp_err"; then
  echo "expected failure for empty offer" >&2
  exit 1
fi
grep -Eq "offer|invalid|failed|empty" "$tmp_err"

if printf 'this-is-not-sdp\n' | "$cli_bin" --run-ms 50 >"$tmp_out" 2>"$tmp_err"; then
  echo "expected failure for invalid offer" >&2
  exit 1
fi
grep -Eq "offer|invalid|failed|parse|description" "$tmp_err"

echo "rtc_signal_cli smoke passed"
