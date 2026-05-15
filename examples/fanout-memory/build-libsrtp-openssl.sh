#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEPS_DIR="$ROOT_DIR/build/fanout-memory/deps/libsrtp-openssl"
VERSION="2.4.2"
URL="http://archive.ubuntu.com/ubuntu/pool/universe/libs/libsrtp2/libsrtp2_${VERSION}.orig.tar.gz"

usage() {
    cat <<'USAGE'
Usage: examples/fanout-memory/build-libsrtp-openssl.sh [options]

Build a fanout-memory-local libsrtp with the OpenSSL crypto backend. The install
is written under build/fanout-memory/deps/libsrtp-openssl/install and is picked
up automatically by examples/fanout-memory/CMakeLists.txt on the next configure.

Options:
  --deps-dir <dir>   Output dependency directory.
  --version <ver>    libsrtp upstream version. Default: 2.4.2
  --url <url>        Source tarball URL.
  --help             Show this help.
USAGE
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --deps-dir)
            DEPS_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            URL="http://archive.ubuntu.com/ubuntu/pool/universe/libs/libsrtp2/libsrtp2_${VERSION}.orig.tar.gz"
            shift 2
            ;;
        --url)
            URL="$2"
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

case "$DEPS_DIR" in
    /*) ;;
    *) DEPS_DIR="$ROOT_DIR/$DEPS_DIR" ;;
esac

command -v cmake >/dev/null || { echo "cmake is required" >&2; exit 1; }
command -v curl >/dev/null || { echo "curl is required" >&2; exit 1; }
command -v tar >/dev/null || { echo "tar is required" >&2; exit 1; }
[ -f /usr/include/openssl/ssl.h ] || { echo "OpenSSL headers are required" >&2; exit 1; }

SRC_DIR="$DEPS_DIR/src"
BUILD_DIR="$DEPS_DIR/build"
INSTALL_DIR="$DEPS_DIR/install"
TARBALL="$DEPS_DIR/libsrtp2_${VERSION}.orig.tar.gz"

mkdir -p "$DEPS_DIR"
if [ ! -f "$TARBALL" ]; then
    curl -L "$URL" -o "$TARBALL"
fi

rm -rf "$SRC_DIR" "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$SRC_DIR"
tar -xf "$TARBALL" -C "$SRC_DIR" --strip-components=1

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DENABLE_OPENSSL=ON \
    -DENABLE_NSS=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DTEST_APPS=OFF
cmake --build "$BUILD_DIR" --target install

printf 'Built OpenSSL-backend libsrtp in %s\n' "$INSTALL_DIR"
printf 'Reconfigure fanout-memory so CMake picks it up:\n'
printf '  cmake -S examples/fanout-memory -B build/fanout-memory -DMICRORTC_ROOT=$PWD\n'
