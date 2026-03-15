#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
usage:
  test_libdc_recvonly.sh [--duration-sec <n>] [--window-sec <n>]
                        [--connect-timeout-sec <n>] [--answer-timeout-sec <n>]

environment:
  RTC_BUILD_DIR            build dir for rtc_signal_cli (default: <repo>/build)
  LIBDATACHANNEL_SRC_DIR   libdatachannel source path
                           (default: /home/qshl/dev/rtc/goodlib/libdatachannel)
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ext_dir="$repo_root/external-client"
ext_build_dir="$ext_dir/build"
rtc_build_dir="${RTC_BUILD_DIR:-$repo_root/build}"
libdc_src="${LIBDATACHANNEL_SRC_DIR:-/home/qshl/dev/rtc/goodlib/libdatachannel}"
mbedtls_include_dir="${MBEDTLS_INCLUDE_DIR:-$repo_root/third_party/mbedtls/include}"
mbedtls_library_dir="${MBEDTLS_LIBRARY_DIR:-$rtc_build_dir/third_party/mbedtls/library}"

duration_sec=20
window_sec=5
connect_timeout_sec=30
answer_timeout_sec=30

while [[ $# -gt 0 ]]; do
  case "$1" in
    --duration-sec)
      duration_sec="$2"
      shift 2
      ;;
    --window-sec)
      window_sec="$2"
      shift 2
      ;;
    --connect-timeout-sec)
      connect_timeout_sec="$2"
      shift 2
      ;;
    --answer-timeout-sec)
      answer_timeout_sec="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [[ ! -d "$libdc_src" ]]; then
  echo "[libdc-recvonly] missing libdatachannel source: $libdc_src" >&2
  exit 2
fi

if [[ ! -f "$ext_dir/CMakeLists.txt" ]]; then
  echo "[libdc-recvonly] missing external-client project: $ext_dir" >&2
  exit 2
fi

mkdir -p "$rtc_build_dir"
if [[ ! -x "$rtc_build_dir/rtc_signal_cli" ]]; then
  echo "[libdc-recvonly] building rtc_signal_cli in $rtc_build_dir"
  cmake -S "$repo_root" -B "$rtc_build_dir" >/dev/null
  cmake --build "$rtc_build_dir" --target rtc_signal_cli >/dev/null
fi

if [[ ! -f "$libdc_src/deps/plog/include/plog/Log.h" ]] || \
   [[ ! -f "$libdc_src/deps/usrsctp/usrsctplib/CMakeLists.txt" ]] || \
   [[ ! -f "$libdc_src/deps/libsrtp/CMakeLists.txt" ]]; then
  echo "[libdc-recvonly] initializing libdatachannel submodules"
  git -C "$libdc_src" submodule update --init --recursive >/dev/null
fi

echo "[libdc-recvonly] configuring external client"
cmake -S "$ext_dir" -B "$ext_build_dir" \
  -DLIBDATACHANNEL_SRC_DIR="$libdc_src" \
  -DLIBDATACHANNEL_MBEDTLS_INCLUDE_DIR="$mbedtls_include_dir" \
  -DLIBDATACHANNEL_MBEDTLS_LIBRARY_DIR="$mbedtls_library_dir" \
  -DCMAKE_BUILD_TYPE=Release >/dev/null

echo "[libdc-recvonly] building external client"
cmake --build "$ext_build_dir" --target libdc_recvonly_client >/dev/null

client_bin="$ext_build_dir/libdc_recvonly_client"
cli_bin="$rtc_build_dir/rtc_signal_cli"
if [[ ! -x "$client_bin" ]]; then
  echo "[libdc-recvonly] missing client binary: $client_bin" >&2
  exit 2
fi
if [[ ! -x "$cli_bin" ]]; then
  echo "[libdc-recvonly] missing rtc_signal_cli binary: $cli_bin" >&2
  exit 2
fi

tmp_dir="$(mktemp -d)"
offer_sdp="$tmp_dir/offer.sdp"
answer_sdp="$tmp_dir/answer.sdp"
evidence_json="$tmp_dir/evidence.json"
client_log="$tmp_dir/client.log"
cli_log="$tmp_dir/cli.log"
client_pid=""
cli_pid=""

cleanup() {
  set +e
  if [[ -n "${cli_pid}" ]] && kill -0 "${cli_pid}" 2>/dev/null; then
    kill "${cli_pid}" 2>/dev/null || true
    wait "${cli_pid}" 2>/dev/null || true
  fi
  if [[ -n "${client_pid}" ]] && kill -0 "${client_pid}" 2>/dev/null; then
    kill "${client_pid}" 2>/dev/null || true
    wait "${client_pid}" 2>/dev/null || true
  fi
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

echo "[libdc-recvonly] starting client"
"$client_bin" \
  --offer-out "$offer_sdp" \
  --answer-in "$answer_sdp" \
  --evidence-out "$evidence_json" \
  --duration-sec "$duration_sec" \
  --window-sec "$window_sec" \
  --answer-timeout-sec "$answer_timeout_sec" \
  --connect-timeout-sec "$connect_timeout_sec" \
  --log-level info >"$client_log" 2>&1 &
client_pid="$!"

offer_deadline=$(( $(date +%s) + 30 ))
while [[ ! -s "$offer_sdp" ]]; do
  if ! kill -0 "$client_pid" 2>/dev/null; then
    echo "[libdc-recvonly] client exited before offer" >&2
    cat "$client_log" >&2 || true
    exit 1
  fi
  if [[ $(date +%s) -ge "$offer_deadline" ]]; then
    echo "[libdc-recvonly] timeout waiting offer file: $offer_sdp" >&2
    cat "$client_log" >&2 || true
    exit 1
  fi
  sleep 0.2
done

echo "[libdc-recvonly] starting rtc_signal_cli"
cli_run_ms=$(( duration_sec * 1000 + 60000 ))
cat "$offer_sdp" | "$cli_bin" --run-ms "$cli_run_ms" >"$answer_sdp" 2>"$cli_log" &
cli_pid="$!"

set +e
wait "$client_pid"
client_rc=$?
set -e
client_pid=""

if [[ -n "$cli_pid" ]] && kill -0 "$cli_pid" 2>/dev/null; then
  kill "$cli_pid" 2>/dev/null || true
  wait "$cli_pid" 2>/dev/null || true
fi
cli_pid=""

if [[ $client_rc -ne 0 ]]; then
  echo "[libdc-recvonly] client failed rc=$client_rc" >&2
  echo "--- client.log ---" >&2
  cat "$client_log" >&2 || true
  echo "--- cli.log ---" >&2
  cat "$cli_log" >&2 || true
  exit 1
fi

if [[ ! -s "$evidence_json" ]]; then
  echo "[libdc-recvonly] missing evidence file: $evidence_json" >&2
  exit 1
fi

grep -q '"schema":[[:space:]]*"rtc-libdc-recvonly-evidence-v1"' "$evidence_json" || {
  echo "[libdc-recvonly] invalid evidence schema" >&2
  cat "$evidence_json" >&2
  exit 1
}

connection_failed="$(grep -o '"connection_failed":[[:space:]]*\(true\|false\)' "$evidence_json" | head -n1 | awk -F: '{gsub(/[[:space:]]/, "", $2); print $2}')"
audio_growth_ok="$(grep -o '"audio_growth_windows_ok":[[:space:]]*\(true\|false\)' "$evidence_json" | head -n1 | awk -F: '{gsub(/[[:space:]]/, "", $2); print $2}')"
video_growth_ok="$(grep -o '"video_growth_windows_ok":[[:space:]]*\(true\|false\)' "$evidence_json" | head -n1 | awk -F: '{gsub(/[[:space:]]/, "", $2); print $2}')"
audio_packets="$(grep -o '"audio_packets_total":[[:space:]]*[0-9]\+' "$evidence_json" | head -n1 | grep -o '[0-9]\+' || true)"
video_packets="$(grep -o '"video_packets_total":[[:space:]]*[0-9]\+' "$evidence_json" | head -n1 | grep -o '[0-9]\+' || true)"
failure_reason="$(grep -o '"failure_reason":[[:space:]]*"[^"]*"' "$evidence_json" | head -n1 | sed -E 's/.*"failure_reason":[[:space:]]*"([^"]*)"/\1/' || true)"

[[ "$connection_failed" == "false" ]] || {
  echo "[libdc-recvonly] evidence reports connection_failed=true" >&2
  cat "$evidence_json" >&2
  exit 1
}
[[ "$audio_growth_ok" == "true" ]] || {
  echo "[libdc-recvonly] evidence reports audio growth failure" >&2
  cat "$evidence_json" >&2
  exit 1
}
[[ "$video_growth_ok" == "true" ]] || {
  echo "[libdc-recvonly] evidence reports video growth failure" >&2
  cat "$evidence_json" >&2
  exit 1
}
[[ -n "$audio_packets" && "$audio_packets" -gt 0 ]] || {
  echo "[libdc-recvonly] audio_packets_total must be > 0" >&2
  cat "$evidence_json" >&2
  exit 1
}
[[ -n "$video_packets" && "$video_packets" -gt 0 ]] || {
  echo "[libdc-recvonly] video_packets_total must be > 0" >&2
  cat "$evidence_json" >&2
  exit 1
}

echo "[libdc-recvonly] PASS duration=${duration_sec}s audio=${audio_packets} video=${video_packets} reason=${failure_reason}"
