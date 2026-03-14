# rtc-s1 (Embedded C WebRTC DataChannel skeleton)

This repository provides a compact, single-thread-oriented C architecture for an embedded WebRTC library.

Current implementation status:
- Public API for session lifecycle and DataChannel operations.
- Platform abstraction (`udp_send/recv`, `now_ms`, `timer_arm/cancel`, `rand_bytes`, `log`).
- STUN binding request/response codec.
- ICE + DTLS + SCTP layers wired into a single polling loop.
- `usrsctp` adapter now supports packet ingress/egress hooks and basic DCEP open/ack handling.
- SDP answer includes `a=fingerprint:sha-256`, `a=ice-ufrag`, and `a=ice-pwd`.
- DTLS now exposes application-data callbacks and SCTP payload is routed through DTLS application data.
- `rtc_callbacks_t` supports `on_local_candidate` for host/srflx candidate trickle.
- `rtc_config_t` supports external DTLS identity injection via `dtls_cert_pem` and `dtls_key_pem`.
- Linux default platform adapter.
- RTOS stub platform adapter (for interface mapping on non-Linux targets).
- Unit tests for mempool, STUN, ICE, DataChannel, and session flow.
- Example device app + browser manual-signaling page for end-to-end DataChannel bring-up.
- Vendored third-party dependencies under `third_party/` (`mbedTLS`, `usrsctp`).

Important:
- By default, CMake uses vendored `third_party/mbedtls` and `third_party/usrsctp`.
- If disabled or missing, it can fall back to system-installed `mbedTLS`/`usrsctp`.
- DTLS now has packet I/O + application-data integration points, and session routes SCTP packets through DTLS application data path.
- Answer SDP now advertises `a=setup:passive` (server-side DTLS role for answerer flow).
- mbedTLS DTLS server is configured with an embedded EC certificate/key and exports SHA-256 fingerprint for SDP.
- Browser manual-signaling interop is available for DataChannel bring-up, with ICE/DTLS/SCTP wired through the example path.
- TURN, media tracks (RTP/RTCP/SRTP), and advanced reliability modes are intentionally out of scope for this first milestone.

## Build

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Optional build flags:

```bash
cmake -S . -B build \
  -DRTC_BUILD_TESTS=ON \
  -DRTC_BUILD_EXAMPLES=ON \
  -DRTC_ENABLE_MBEDTLS=ON \
  -DRTC_ENABLE_USRSCTP=ON \
  -DRTC_VENDOR_MBEDTLS=ON \
  -DRTC_VENDOR_USRSCTP=ON
```

## Manual Browser Bring-up

1. Build the example app:

```bash
cmake -S . -B build -DRTC_BUILD_EXAMPLES=ON
cmake --build build -j --target example_device_manual
```

2. Open `examples/browser/manual_signaling.html` in Chrome/Edge.
3. Edit `examples/device/minimal_device_app.c` and set `cfg.bind_ip` to the real LAN IP of your device.
4. In browser page click `Create Offer` (it waits for ICE gathering and outputs full SDP with local candidates).
5. Run `build/example_device_manual`, paste browser offer SDP, then input `END_OFFER` on its own line.
6. Copy the printed answer SDP back to browser page and click `Apply Answer` (it auto-parses candidate lines in answer SDP).
7. If browser later logs extra `candidate:` lines, paste them into the device process directly (trickle mode).
8. After DataChannel opens, the device example sends `hello from device`, echoes browser text as `echo: ...`, and any plain text you type into the device terminal is sent to the active channel. Type `quit` to exit.
9. Runtime logs are mirrored to terminal and `build/rtc_device.log` (append mode, one session header per run).
10. For failure triage, check `build/rtc_device.log` for:
   - `ICE check request handled ... use-candidate=...`
   - `remote tuple locked ...`
   - `Starting DTLS handshake` or `mbedtls ... failed`
