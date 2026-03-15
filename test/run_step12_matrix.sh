#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
usage:
  run_step12_matrix.sh [--auto-only]
  run_step12_matrix.sh --manual-evidence <web_evidence.json> <client_evidence.json>
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-}"
auto_only=0
web_evidence=""
client_evidence=""

if [[ -z "$build_dir" ]]; then
  if [[ -x "$repo_root/build/rtc_api_test" ]]; then
    build_dir="$repo_root/build"
  else
    build_dir="$(pwd)"
  fi
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --auto-only)
      auto_only=1
      shift
      ;;
    --manual-evidence)
      shift
      if [[ $# -lt 2 ]]; then
        usage
        exit 2
      fi
      web_evidence="$1"
      client_evidence="$2"
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

require_bin() {
  local path="$1"
  if [[ ! -x "$path" ]]; then
    echo "[step12] missing executable: $path" >&2
    exit 2
  fi
}

require_bin "$build_dir/rtc_api_test"
require_bin "$build_dir/rtc_transport_test"
require_bin "$build_dir/rtc_rtp_test"
require_bin "$build_dir/rtc_srtp_test"
require_bin "$build_dir/rtc_interop_smoke_test"
require_bin "$build_dir/rtc_signal_cli"

run_case() {
  local label="$1"
  shift
  echo "[step12][$label] $*"
  if ! "$@"; then
    echo "[step12][$label] FAIL"
    return 1
  fi
  echo "[step12][$label] PASS"
  return 0
}

overall_rc=0

echo "[step12] group=positive-auto"
run_case "positive-auto" env RTC_API_GROUP=positive "$build_dir/rtc_api_test" || overall_rc=1
run_case "positive-auto" "$build_dir/rtc_rtp_test" || overall_rc=1
run_case "positive-auto" "$build_dir/rtc_srtp_test" || overall_rc=1
run_case "positive-auto" "$build_dir/rtc_interop_smoke_test" || overall_rc=1
run_case "positive-auto" "$repo_root/test/test_rtc_signal_cli.sh" "$build_dir/rtc_signal_cli" || overall_rc=1

echo "[step12] group=negative-auto"
run_case "negative-auto" env RTC_API_GROUP=negative "$build_dir/rtc_api_test" || overall_rc=1

echo "[step12] group=boundary-auto"
run_case "boundary-auto" env RTC_API_GROUP=boundary "$build_dir/rtc_api_test" || overall_rc=1
run_case "boundary-auto" "$build_dir/rtc_transport_test" || overall_rc=1
run_case "boundary-auto" "$repo_root/test/test_rtc_signal_cli_long_offer.sh" "$build_dir/rtc_signal_cli" || overall_rc=1

if [[ "$auto_only" -eq 1 ]]; then
  if [[ "$overall_rc" -ne 0 ]]; then
    echo "[step12] auto matrix failed"
    exit 1
  fi
  echo "[step12] auto matrix passed"
  exit 0
fi

if [[ -z "$web_evidence" || -z "$client_evidence" ]]; then
  echo "[step12] manual evidence not provided, skip manual validation"
  if [[ "$overall_rc" -ne 0 ]]; then
    exit 1
  fi
  exit 0
fi

if [[ ! -f "$web_evidence" || ! -f "$client_evidence" ]]; then
  echo "[step12] evidence files not found" >&2
  exit 2
fi

grep -q '"schema":[[:space:]]*"rtc-step12-web-evidence-v1"' "$web_evidence" || {
  echo "[step12] invalid web evidence schema" >&2
  exit 1
}
grep -q '"schema":[[:space:]]*"rtc-step12-client-evidence-v1"' "$client_evidence" || {
  echo "[step12] invalid client evidence schema" >&2
  exit 1
}

duration_sec="$(grep -o '"duration_sec":[[:space:]]*[0-9]\+' "$web_evidence" | head -n1 | grep -o '[0-9]\+' || true)"
connection_failed="$(grep -o '"connection_failed":[[:space:]]*\(true\|false\)' "$web_evidence" | head -n1 | awk -F: '{gsub(/[[:space:]]/, "", $2); print $2}' || true)"
bidirectional_growth="$(grep -o '"bidirectional_growth":[[:space:]]*\(true\|false\)' "$web_evidence" | head -n1 | awk -F: '{gsub(/[[:space:]]/, "", $2); print $2}' || true)"
peer_state="$(grep -o '"peer_state":[[:space:]]*"[^"]*"' "$client_evidence" | head -n1 | awk -F\" '{print $4}' || true)"
rx_audio_frames="$(grep -o '"rx_audio_frames":[[:space:]]*[0-9]\+' "$client_evidence" | head -n1 | grep -o '[0-9]\+' || true)"
rx_video_frames="$(grep -o '"rx_video_frames":[[:space:]]*[0-9]\+' "$client_evidence" | head -n1 | grep -o '[0-9]\+' || true)"
dtls_last_error="$(grep -o '"dtls_last_error":[[:space:]]*[-0-9]\+' "$client_evidence" | head -n1 | grep -o '[-0-9]\+' || true)"
queue_overflow_count="$(grep -o '"queue_overflow_count":[[:space:]]*[0-9]\+' "$client_evidence" | head -n1 | grep -o '[0-9]\+' || true)"

[[ -n "$duration_sec" && "$duration_sec" -ge 300 ]] || {
  echo "[step12] duration_sec < 300" >&2
  exit 1
}
[[ "$connection_failed" == "false" ]] || {
  echo "[step12] web reported connection_failed" >&2
  exit 1
}
[[ "$bidirectional_growth" == "true" ]] || {
  echo "[step12] web bidirectional_growth is false" >&2
  exit 1
}
[[ "$peer_state" == "connected" ]] || {
  echo "[step12] client peer_state != connected" >&2
  exit 1
}
[[ -n "$rx_audio_frames" && "$rx_audio_frames" -gt 0 ]] || {
  echo "[step12] rx_audio_frames <= 0" >&2
  exit 1
}
[[ -n "$rx_video_frames" && "$rx_video_frames" -gt 0 ]] || {
  echo "[step12] rx_video_frames <= 0" >&2
  exit 1
}
[[ "$dtls_last_error" == "0" ]] || {
  echo "[step12] dtls_last_error != 0" >&2
  exit 1
}
[[ "$queue_overflow_count" == "0" ]] || {
  echo "[step12] queue_overflow_count != 0" >&2
  exit 1
}

if [[ "$overall_rc" -ne 0 ]]; then
  echo "[step12] auto matrix failed"
  exit 1
fi

echo "[step12] manual evidence validation passed"
