#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEPS_DIR="$ROOT_DIR/build/fanout-memory/deps/mbedtls"
VERSION="2.28.0"
URL="http://archive.ubuntu.com/ubuntu/pool/universe/m/mbedtls/mbedtls_${VERSION}.orig.tar.gz"

usage() {
    cat <<'USAGE'
Usage: examples/fanout-memory/build-mbedtls.sh [options]

Build a fanout-memory-local mbedTLS with DTLS-SRTP enabled. The install is
written under build/fanout-memory/deps/mbedtls/install and is picked up
automatically by examples/fanout-memory/CMakeLists.txt on the next configure.

Options:
  --deps-dir <dir>   Output dependency directory.
  --version <ver>    mbedTLS upstream version. Default: 2.28.0
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
            URL="http://archive.ubuntu.com/ubuntu/pool/universe/m/mbedtls/mbedtls_${VERSION}.orig.tar.gz"
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
command -v python3 >/dev/null || { echo "python3 is required" >&2; exit 1; }
command -v tar >/dev/null || { echo "tar is required" >&2; exit 1; }

SRC_DIR="$DEPS_DIR/src"
BUILD_DIR="$DEPS_DIR/build"
INSTALL_DIR="$DEPS_DIR/install"
TARBALL="$DEPS_DIR/mbedtls_${VERSION}.orig.tar.gz"

mkdir -p "$DEPS_DIR"
if [ ! -f "$TARBALL" ]; then
    curl -L "$URL" -o "$TARBALL"
fi

rm -rf "$SRC_DIR" "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$SRC_DIR"
tar -xf "$TARBALL" -C "$SRC_DIR" --strip-components=1

config_macro() {
    local action="$1"
    local macro="$2"
    if grep -Eq "^[[:space:]*/]*#?[[:space:]]*define[[:space:]]+$macro([[:space:]]|$)" "$SRC_DIR/include/mbedtls/config.h"; then
        (cd "$SRC_DIR" && python3 scripts/config.py "$action" "$macro")
    fi
}

config_macro set MBEDTLS_SSL_DTLS_SRTP
config_macro set MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
config_macro set MBEDTLS_ECP_DP_SECP256R1_ENABLED

for macro in \
    MBEDTLS_SSL_PROTO_TLS1 \
    MBEDTLS_SSL_PROTO_TLS1_1 \
    MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED \
    MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED \
    MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED \
    MBEDTLS_KEY_EXCHANGE_RSA_ENABLED \
    MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED \
    MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED \
    MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED \
    MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED \
    MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED \
    MBEDTLS_ECP_DP_SECP192R1_ENABLED \
    MBEDTLS_ECP_DP_SECP224R1_ENABLED \
    MBEDTLS_ECP_DP_SECP384R1_ENABLED \
    MBEDTLS_ECP_DP_SECP521R1_ENABLED \
    MBEDTLS_ECP_DP_SECP192K1_ENABLED \
    MBEDTLS_ECP_DP_SECP224K1_ENABLED \
    MBEDTLS_ECP_DP_SECP256K1_ENABLED \
    MBEDTLS_ECP_DP_BP256R1_ENABLED \
    MBEDTLS_ECP_DP_BP384R1_ENABLED \
    MBEDTLS_ECP_DP_BP512R1_ENABLED \
    MBEDTLS_ECP_DP_CURVE25519_ENABLED \
    MBEDTLS_ECP_DP_CURVE448_ENABLED \
    MBEDTLS_ARC4_C \
    MBEDTLS_ARIA_C \
    MBEDTLS_BLOWFISH_C \
    MBEDTLS_CAMELLIA_C \
    MBEDTLS_CHACHA20_C \
    MBEDTLS_CHACHAPOLY_C \
    MBEDTLS_DES_C \
    MBEDTLS_DHM_C \
    MBEDTLS_ECJPAKE_C \
    MBEDTLS_HAVEGE_C \
    MBEDTLS_MD2_C \
    MBEDTLS_MD4_C \
    MBEDTLS_NIST_KW_C \
    MBEDTLS_PADLOCK_C \
    MBEDTLS_PEM_PARSE_C \
    MBEDTLS_PEM_WRITE_C \
    MBEDTLS_PKCS5_C \
    MBEDTLS_PKCS12_C \
    MBEDTLS_RIPEMD160_C \
    MBEDTLS_RSA_C \
    MBEDTLS_SHA512_C \
    MBEDTLS_SSL_CBC_RECORD_SPLITTING \
    MBEDTLS_X509_RSASSA_PSS_SUPPORT \
    MBEDTLS_XTEA_C; do
    config_macro unset "$macro"
done

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DUSE_SHARED_MBEDTLS_LIBRARY=ON \
    -DUSE_STATIC_MBEDTLS_LIBRARY=OFF \
    -DENABLE_TESTING=OFF \
    -DENABLE_PROGRAMS=OFF
cmake --build "$BUILD_DIR" --target install

printf 'Built DTLS-SRTP-enabled mbedTLS in %s\n' "$INSTALL_DIR"
printf 'Reconfigure fanout-memory so CMake picks it up:\n'
printf '  cmake -S examples/fanout-memory -B build/fanout-memory -DMICRORTC_ROOT=$PWD\n'
