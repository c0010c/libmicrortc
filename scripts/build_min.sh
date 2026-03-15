#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build/base}"

echo "[1/3] Configure: ${BUILD_DIR}"
echo "      WEBRTC_ENABLE_AUDIO=ON"
echo "      WEBRTC_ENABLE_VIDEO=ON"
echo "      WEBRTC_ENABLE_DATA_CHANNEL=ON"
echo "      WEBRTC_ENABLE_TURN=OFF"
echo "      WEBRTC_USE_MBEDTLS=ON"
echo "      WEBRTC_PORT_POSIX=ON"
echo "      WEBRTC_PORT_RTOS=OFF"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWEBRTC_ENABLE_AUDIO=ON \
    -DWEBRTC_ENABLE_VIDEO=ON \
    -DWEBRTC_ENABLE_DATA_CHANNEL=ON \
    -DWEBRTC_ENABLE_TURN=OFF \
    -DWEBRTC_USE_MBEDTLS=ON \
    -DWEBRTC_PORT_POSIX=ON \
    -DWEBRTC_PORT_RTOS=OFF

echo "[2/3] Build"
cmake --build "${BUILD_DIR}" -j

echo "[3/3] Test"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "Done."
