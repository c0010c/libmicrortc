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
trap 'rm -f "$tmp_out"' EXIT

cat <<'EOF_CMDS' | "$cli_bin" >"$tmp_out"
set-offer-begin
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
set-offer-end
start
run 200 10 500
get-answer
add-remote-candidate candidate:9 1 udp 2130706431 127.0.0.1 6000 typ host
add-remote-candidate candidate:10 1 tcp 1518280447 127.0.0.1 9 typ host tcptype passive
state
stats
restart-peer
set-offer-begin
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
set-offer-end
start
run 200 10 500
get-answer
quit
EOF_CMDS

grep -q "^EVENT LOCAL_DESCRIPTION_BEGIN answer$" "$tmp_out"
grep -q "^EVENT LOCAL_DESCRIPTION_END$" "$tmp_out"
grep -q "^EVENT LOCAL_CANDIDATE " "$tmp_out"
grep -q "^ERR -7 " "$tmp_out"

desc_count="$(grep -c "^EVENT LOCAL_DESCRIPTION_BEGIN answer$" "$tmp_out" || true)"
if [[ "${desc_count:-0}" -lt 2 ]]; then
  echo "expected at least 2 local description events, got $desc_count" >&2
  exit 1
fi

echo "rtc_signal_cli smoke passed"
