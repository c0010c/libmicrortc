#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INPUT="$ROOT_DIR/Big_Buck_Bunny_720_10s_2MB.mp4"
OUTPUT="$ROOT_DIR/build/fanout-memory/media"
DURATION=60

usage() {
    cat <<'USAGE'
Usage: examples/fanout-memory/prepare-media.sh [options]

Options:
  --input <path>      Source MP4. Default: Big_Buck_Bunny_720_10s_2MB.mp4
  --output <dir>      Output media directory. Default: build/fanout-memory/media
  --duration <sec>    Generated media duration. Default: 60
  --help              Show this help.
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --input)
            INPUT="$2"
            shift 2
            ;;
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --duration)
            DURATION="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$INPUT" in
    /*) ;;
    *) INPUT="$ROOT_DIR/$INPUT" ;;
esac
case "$OUTPUT" in
    /*) ;;
    *) OUTPUT="$ROOT_DIR/$OUTPUT" ;;
esac

command -v ffmpeg >/dev/null || { echo "ffmpeg is required" >&2; exit 1; }
command -v node >/dev/null || { echo "node is required" >&2; exit 1; }
[ -f "$INPUT" ] || { echo "input not found: $INPUT" >&2; exit 1; }

mkdir -p "$OUTPUT"

ffmpeg -hide_banner -loglevel error -y \
    -stream_loop -1 -i "$INPUT" -t "$DURATION" -an \
    -c:v libx264 -profile:v baseline -level 3.1 -pix_fmt yuv420p \
    -r 30 -g 30 -keyint_min 30 -bf 0 \
    -x264-params "repeat-headers=1:aud=1:scenecut=0" \
    -f h264 "$OUTPUT/video.h264"

ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=${DURATION}" \
    -ac 2 -c:a libopus -application audio -frame_duration 20 -b:a 64k \
    "$OUTPUT/audio.ogg"

node "$ROOT_DIR/examples/fanout-memory/tools/ogg-opus-to-packets.js" \
    "$OUTPUT/audio.ogg" "$OUTPUT/audio.opus"

cat > "$OUTPUT/metadata.json" <<EOF
{
  "source": "$INPUT",
  "duration_seconds": $DURATION,
  "video": {
    "path": "video.h264",
    "format": "h264-annexb",
    "fps": 30
  },
  "audio": {
    "path": "audio.opus",
    "format": "big-endian-u16-length-prefixed-opus",
    "sample_rate": 48000,
    "packet_duration_ms": 20,
    "generated": "sine"
  }
}
EOF

printf 'Prepared fanout media in %s\n' "$OUTPUT"
