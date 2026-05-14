#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
FIXTURES_DIR="$ROOT_DIR/tests/fixtures"
TURN_CONFIG=""
SKIP_NPM_INSTALL=0
BROWSER_CHANNEL="${MRTC_E2E_BROWSER_CHANNEL:-chromium}"

usage() {
    cat <<'EOF'
Usage: scripts/verify-v1.sh [options]

Options:
  --build-dir <path>        CMake build directory. Default: build
  --fixtures <path>         H264/Opus fixture directory. Default: tests/fixtures
  --turn-config <path>      Run TURN relay Chrome E2E with this local config.
  --skip-npm-install        Do not run npm ci/install in tests/e2e.
  --browser-channel <name>  Playwright browser channel. Default: chromium.
  --help                   Show this help.

Default verification runs BUILD, CTEST, CHROME HOST E2E and SUMMARY.
TURN relay E2E is opt-in and hard-fails when --turn-config is provided but invalid.
The final machine-readable summary is written to build/reports/mrtc-v1-summary.json.
EOF
}

banner() {
    printf '\n=== %s ===\n' "$1"
}

fail_usage() {
    printf 'verify-v1: %s\n\n' "$1" >&2
    usage >&2
    exit 2
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ "$#" -ge 2 ] || fail_usage "--build-dir requires a path"
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-dir=*)
            BUILD_DIR="${1#--build-dir=}"
            shift
            ;;
        --fixtures)
            [ "$#" -ge 2 ] || fail_usage "--fixtures requires a path"
            FIXTURES_DIR="$2"
            shift 2
            ;;
        --fixtures=*)
            FIXTURES_DIR="${1#--fixtures=}"
            shift
            ;;
        --turn-config)
            [ "$#" -ge 2 ] || fail_usage "--turn-config requires a path"
            TURN_CONFIG="$2"
            shift 2
            ;;
        --turn-config=*)
            TURN_CONFIG="${1#--turn-config=}"
            shift
            ;;
        --skip-npm-install)
            SKIP_NPM_INSTALL=1
            shift
            ;;
        --browser-channel)
            [ "$#" -ge 2 ] || fail_usage "--browser-channel requires a value"
            BROWSER_CHANNEL="$2"
            shift 2
            ;;
        --browser-channel=*)
            BROWSER_CHANNEL="${1#--browser-channel=}"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail_usage "unknown option: $1"
            ;;
    esac
done

case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$ROOT_DIR/$BUILD_DIR" ;;
esac
case "$FIXTURES_DIR" in
    /*) ;;
    *) FIXTURES_DIR="$ROOT_DIR/$FIXTURES_DIR" ;;
esac
if [ -n "$TURN_CONFIG" ]; then
    case "$TURN_CONFIG" in
        /*) ;;
        *) TURN_CONFIG="$ROOT_DIR/$TURN_CONFIG" ;;
    esac
fi

REPORT_DIR="$BUILD_DIR/reports"
SUMMARY_PATH="$REPORT_DIR/mrtc-v1-summary.json"
E2E_SUMMARY_PATH="$ROOT_DIR/tests/e2e/artifacts/summary.json"
START_EPOCH_MS="$(node -e 'process.stdout.write(String(Date.now()))')"

summary_cli() {
    node "$ROOT_DIR/tests/e2e/summary.js" "$@"
}

stage_update() {
    local stage="$1"
    local status="$2"
    local started_ms="$3"
    local reason="${4:-}"
    local ended_ms duration_ms
    ended_ms="$(node -e 'process.stdout.write(String(Date.now()))')"
    duration_ms=$((ended_ms - started_ms))
    summary_cli --stage "$SUMMARY_PATH" "$stage" "$status" "$duration_ms" "$reason"
}

run_stage() {
    local stage="$1"
    shift
    local started_ms
    started_ms="$(node -e 'process.stdout.write(String(Date.now()))')"
    banner "$stage"
    if "$@"; then
        stage_update "$(printf '%s' "$stage" | tr '[:upper:] ' '[:lower:]_')" "passed" "$started_ms"
    else
        local status=$?
        stage_update "$(printf '%s' "$stage" | tr '[:upper:] ' '[:lower:]_')" "failed" "$started_ms" "command exited with status $status"
        banner "SUMMARY"
        summary_cli --finalize "$SUMMARY_PATH" "$START_EPOCH_MS"
        exit "$status"
    fi
}

cd "$ROOT_DIR" || exit 1
mkdir -p "$REPORT_DIR"
summary_cli --init-v1 "$SUMMARY_PATH" "$([ -n "$TURN_CONFIG" ] && printf true || printf false)"

run_stage "BUILD" cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DMRTC_BUILD_TESTS=ON
run_stage "BUILD" cmake --build "$BUILD_DIR"
run_stage "CTEST" ctest --test-dir "$BUILD_DIR" --output-on-failure

if [ "$SKIP_NPM_INSTALL" -eq 0 ]; then
    banner "CHROME HOST E2E"
    if ! npm --prefix "$ROOT_DIR/tests/e2e" ci; then
        npm --prefix "$ROOT_DIR/tests/e2e" install || {
            stage_update "chrome_host" "failed" "$(node -e 'process.stdout.write(String(Date.now()))')" "npm install failed"
            banner "SUMMARY"
            summary_cli --finalize "$SUMMARY_PATH" "$START_EPOCH_MS"
            exit 1
        }
    fi
else
    banner "CHROME HOST E2E"
    printf 'Skipping npm install because --skip-npm-install was provided.\n'
fi

rm -f "$E2E_SUMMARY_PATH"
HOST_STARTED="$(node -e 'process.stdout.write(String(Date.now()))')"
if MRTC_E2E_BROWSER_CHANNEL="$BROWSER_CHANNEL" MRTC_E2E_FIXTURES="$FIXTURES_DIR" npm --prefix "$ROOT_DIR/tests/e2e" run test:host; then
    stage_update "chrome_host" "passed" "$HOST_STARTED"
    summary_cli --merge-e2e "$SUMMARY_PATH" "chrome_host" "$E2E_SUMMARY_PATH"
else
    status=$?
    stage_update "chrome_host" "failed" "$HOST_STARTED" "host Chrome E2E exited with status $status"
    [ -f "$E2E_SUMMARY_PATH" ] && summary_cli --merge-e2e "$SUMMARY_PATH" "chrome_host" "$E2E_SUMMARY_PATH"
    banner "SUMMARY"
    summary_cli --finalize "$SUMMARY_PATH" "$START_EPOCH_MS"
    exit "$status"
fi

if [ -n "$TURN_CONFIG" ]; then
    rm -f "$E2E_SUMMARY_PATH"
    TURN_STARTED="$(node -e 'process.stdout.write(String(Date.now()))')"
    banner "CHROME TURN E2E"
    if MRTC_E2E_BROWSER_CHANNEL="$BROWSER_CHANNEL" MRTC_E2E_FIXTURES="$FIXTURES_DIR" npm --prefix "$ROOT_DIR/tests/e2e" run test:turn -- --turn-config "$TURN_CONFIG"; then
        stage_update "chrome_turn" "passed" "$TURN_STARTED"
        summary_cli --merge-e2e "$SUMMARY_PATH" "chrome_turn" "$E2E_SUMMARY_PATH"
    else
        status=$?
        stage_update "chrome_turn" "failed" "$TURN_STARTED" "TURN Chrome E2E exited with status $status"
        [ -f "$E2E_SUMMARY_PATH" ] && summary_cli --merge-e2e "$SUMMARY_PATH" "chrome_turn" "$E2E_SUMMARY_PATH"
        banner "SUMMARY"
        summary_cli --finalize "$SUMMARY_PATH" "$START_EPOCH_MS"
        exit "$status"
    fi
else
    summary_cli --stage "$SUMMARY_PATH" "chrome_turn" "skipped" "0" "TURN relay E2E requires explicit --turn-config"
fi

banner "SUMMARY"
summary_cli --finalize "$SUMMARY_PATH" "$START_EPOCH_MS"
printf 'Summary: %s\n' "$SUMMARY_PATH"
