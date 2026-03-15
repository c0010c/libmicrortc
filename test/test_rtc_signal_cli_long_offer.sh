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
trap 'rm -f "$tmp_out" "$tmp_err"' EXIT

{
  cat <<'EOF_OFFER'
v=0
o=- 4254106394216585985 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS *
a=ice-ufrag:IuwM
a=ice-pwd:k2MLxUeVINjsR5SXsLjpFv2S
a=fingerprint:sha-256 07:3F:C5:E4:2C:35:44:33:C1:0E:D1:15:2C:77:85:68:96:06:AD:45:7C:33:7B:56:26:C2:8B:2E:C9:DB:EF:5B
a=setup:actpass
m=audio 9 UDP/TLS/RTP/SAVPF 8
c=IN IP4 0.0.0.0
a=mid:0
a=rtcp-mux
a=rtpmap:8 PCMA/8000
a=ssrc:3001001 cname:audio-track
a=candidate:1 1 udp 2130706431 127.0.0.1 5000 typ host
m=video 9 UDP/TLS/RTP/SAVPF 109
c=IN IP4 0.0.0.0
a=mid:1
a=rtcp-mux
a=rtpmap:109 H264/90000
a=fmtp:109 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=ssrc:3002001 cname:video-track
a=candidate:2 1 udp 2130706431 127.0.0.1 5000 typ host
EOF_OFFER
  for i in $(seq 1 900); do
    echo "a=x-pad-${i}:1234567890123456789012345678901234567890"
  done
} | "$cli_bin" --run-ms 120 >"$tmp_out" 2>"$tmp_err"

grep -q "^a=setup:" "$tmp_out"
if grep -q "buffer_too_small" "$tmp_err"; then
  echo "unexpected SDP overflow for long offer" >&2
  exit 1
fi

echo "rtc_signal_cli long-offer passed"
