#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 ]]; then
  echo "usage: $0 [<libpeer_client_build_dir>]" >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
client_dir="$repo_root/external-client/libpeer-client"
build_dir="${1:-$client_dir/build}"

if [[ ! -f "$client_dir/CMakeLists.txt" ]]; then
  echo "[libpeer-b64] missing project: $client_dir" >&2
  exit 2
fi

echo "[libpeer-b64] configuring libpeer-client"
cmake -S "$client_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release >/dev/null

echo "[libpeer-b64] building libpeer_b64_answer_cli"
cmake --build "$build_dir" --target libpeer_b64_answer_cli >/dev/null

bin="$build_dir/libpeer_b64_answer_cli"
if [[ ! -x "$bin" ]]; then
  echo "[libpeer-b64] missing binary: $bin" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
tmp_offer="$tmp_dir/offer.sdp"
tmp_offer_b64="$tmp_dir/offer.b64"
tmp_long_offer="$tmp_dir/offer.long.sdp"
tmp_out="$tmp_dir/out.log"
tmp_err="$tmp_dir/err.log"
tmp_answer_b64="$tmp_dir/answer.b64"
tmp_answer_sdp="$tmp_dir/answer.sdp"
tmp_big_raw="$tmp_dir/big.raw"
cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

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

if ! "$bin" --run-ms 120 <"$tmp_offer_b64" >"$tmp_out" 2>"$tmp_err"; then
  echo "[libpeer-b64] expected success for valid offer" >&2
  cat "$tmp_out" >&2 || true
  cat "$tmp_err" >&2 || true
  exit 1
fi

grep -q 'answer_ready' "$tmp_out"
grep -E '^[A-Za-z0-9+/=]+$' "$tmp_out" | head -n1 >"$tmp_answer_b64"
if [[ ! -s "$tmp_answer_b64" ]]; then
  echo "[libpeer-b64] expected answer b64 line" >&2
  cat "$tmp_out" >&2
  exit 1
fi

tr -d '\r\n' <"$tmp_answer_b64" | base64 -d >"$tmp_answer_sdp"
grep -q '^v=0' "$tmp_answer_sdp"
grep -q '^m=audio ' "$tmp_answer_sdp"
grep -q '^m=video ' "$tmp_answer_sdp"
grep -q '^a=ice-ufrag:' "$tmp_answer_sdp"

if "$bin" --run-ms 50 </dev/null >"$tmp_out" 2>"$tmp_err"; then
  echo "[libpeer-b64] expected failure for empty offer" >&2
  exit 1
fi
grep -Eq 'offer|decode|failed' "$tmp_out"

if echo '%%%INVALID%%%' | "$bin" --run-ms 50 >"$tmp_out" 2>"$tmp_err"; then
  echo "[libpeer-b64] expected failure for invalid base64 chars" >&2
  exit 1
fi
grep -Eq 'offer|decode|failed' "$tmp_out"

if printf 'A===' | "$bin" --run-ms 50 >"$tmp_out" 2>"$tmp_err"; then
  echo "[libpeer-b64] expected failure for invalid base64 padding" >&2
  exit 1
fi
grep -Eq 'decode|failed' "$tmp_out"

head -c 120000 /dev/zero | tr '\0' 'A' >"$tmp_big_raw"
if "$bin" --run-ms 50 <"$tmp_big_raw" >"$tmp_out" 2>"$tmp_err"; then
  echo "[libpeer-b64] expected failure for oversized input" >&2
  exit 1
fi
grep -Eq 'offer|decode|failed' "$tmp_out"

{
  cat "$tmp_offer"
  for i in $(seq 1 200); do
    echo "a=x-pad-${i}:1234567890123456789012345678901234567890"
  done
} >"$tmp_long_offer"
base64 -w 0 "$tmp_long_offer" >"$tmp_offer_b64"

if ! "$bin" --run-ms 120 <"$tmp_offer_b64" >"$tmp_out" 2>"$tmp_err"; then
  echo "[libpeer-b64] expected success for long offer" >&2
  cat "$tmp_out" >&2 || true
  cat "$tmp_err" >&2 || true
  exit 1
fi
grep -E '^[A-Za-z0-9+/=]+$' "$tmp_out" | head -n1 >"$tmp_answer_b64"
tr -d '\r\n' <"$tmp_answer_b64" | base64 -d >"$tmp_answer_sdp"
grep -q '^a=setup:' "$tmp_answer_sdp"

echo "[libpeer-b64] smoke passed"
