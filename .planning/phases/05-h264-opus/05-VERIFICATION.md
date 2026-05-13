---
phase: 05-h264-opus
verified: 2026-05-13T06:37:56Z
status: passed
score: 12/12 must-haves verified
overrides_applied: 0
---

# Phase 5: H264/Opus 媒体路径 Verification Report

**Phase Goal:** H264/Opus media path for encoded frames, including public media API, RTP/H264/Opus primitives, SRTP media integration, RTCP NACK/PLI/SR/RR behavior, fixed media fixture verifier, and package consumer validation. Phase 5 使用 C fixture harness，不声明 Chrome/browser E2E 完成。
**Verified:** 2026-05-13T06:37:56Z
**Status:** passed
**Re-verification:** No - initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | API 支持添加 audio/video transceiver，并通过 `mrtc_transceiver_write_frame()` 发送编码后媒体帧。 | VERIFIED | `include/micrortc/peer_connection.h` 定义 `MRTC_RTP_TRANSCEIVER_HANDLE`、`MRTC_TRANSCEIVER_INIT`、`MRTC_FRAME`、`mrtc_peer_connection_add_transceiver()`、`mrtc_transceiver_write_frame()`；`src/peer_connection.c` 实现 add/write，并由 `mrtc_phase5_media_verify` 输出 `public media api ok`、`h264 send ok`、`opus send ok`。 |
| 2 | Public API 只暴露 `MRTC_*` / `mrtc_*` 和标准 C 类型，不暴露 AWS/PIC 类型。 | VERIFIED | Public header 使用 `uint64_t`、`size_t`、opaque handles；边界 grep 未发现 `PCHAR`、`PVOID`、`BOOL`、`UINT64` 或裸 AWS `STATUS` 泄漏。 |
| 3 | 核心库不包含采集器、编码器、GStreamer、FFmpeg、Ogg/MP4/AVCC 容器入口。 | VERIFIED | Public header 和 media implementation 未提供 capture/encoder/container API；Phase 5 fixture 使用 raw Annex-B H264 与 length-prefixed Opus packet，README 明确不引入这些职责。 |
| 4 | SDP 为 H264/Opus 生成 Chrome-compatible media m-line、rtpmap/fmtp、RTCP feedback 和方向。 | VERIFIED | `src/sdp.c` 生成 `m=audio ... 111`、`opus/48000/2`、`minptime=10;useinbandfec=1`、`m=video ... 96`、`H264/90000`、`packetization-mode=1;level-asymmetry-allowed=1;profile-level-id=42e01f`、`nack`、`nack pli`；`mrtc_sdp_roundtrip_test` 与 `mrtc_peer_connection_api_test` 通过。 |
| 5 | RTP primitives 可以 parse/serialize RTP v2 packet，并进入 core target。 | VERIFIED | `src/rtp/rtp_packet.c` 在 `CMakeLists.txt` core source list 中；`mrtc_rtp_packet_test` 覆盖 version、payload type、sequence、timestamp、SSRC、marker round trip 和短包/版本错误。 |
| 6 | H264 Annex-B packetizer/depacketizer 支持 3/4 byte start code、Single NALU、FU-A、Annex-B 重组，并拒绝 AVCC-like input。 | VERIFIED | `src/rtp/codecs/h264.c` 两遍扫描 NALU，动态分配 NALU 表；`test_h264_packetizer.c` 覆盖 single NALU、FU-A start/middle/end、invalid AVCC-like input、超过 32 个 NALU。 |
| 7 | Opus 与 H264 共用 `MRTC_FRAME`/write API，payload/depayload 为直拷贝，timestamp 按 48kHz 从 100ns presentation timestamp 转换。 | VERIFIED | `src/rtp/codecs/opus.c` 直拷贝 payload/depayload 并实现 `timestamp_100ns * 48000 / 10000000`；`mrtc_opus_codec_test` 和 Phase 5 verifier 的 Opus monotonic timestamp 断言通过。 |
| 8 | SRTP media integration 已连接发送/接收路径；无 libsrtp 时保留计划要求的 deterministic passthrough fallback，并可被识别。 | VERIFIED | `src/srtp/srtp_session.c` 在 `MRTC_HAVE_SRTP` 时调用 libsrtp protect/unprotect，否则 `passthrough=1`；`mrtc_peer_connection_media_is_srtp_passthrough()` 暴露 fallback 状态。当前构建缺少 libsrtp，CTest/verifier 覆盖 fallback。真实 libsrtp/browser 互通风险属于 Phase 6/real-deps。 |
| 9 | 接收路径通过 transceiver `on_frame` 回调输出 H264 Annex-B 重组帧与非空 Opus payload frame。 | VERIFIED | `src/peer_connection.c` `receive_protected_media_packet()` 解 SRTP、解析 RTP、按 payload/SSRC 找可接收 transceiver，H264 same-timestamp 聚合后 marker delivery，Opus per packet delivery；Phase 5 verifier 输出 `h264 receive ok`、`opus receive ok`。 |
| 10 | RTCP SR/RR/NACK/PLI primitives 存在，且 NACK/PLI 不只是 parser，能驱动行为。 | VERIFIED | `src/rtcp/rtcp_packet.c` 支持 SR/RR/NACK/PLI parse/generate；`src/rtcp/retransmitter.c` 用 rolling RTP buffer 按 NACK 重发；`src/peer_connection.c` PLI 调用 `on_picture_loss`，SR/RR 更新 counters；verifier 输出 `rtcp nack retransmit ok`、`pli callback ok`。 |
| 11 | 固定 H264/Opus fixtures 可读，H264 在 IDR 前有 SPS/PPS，Opus 是非空 length-prefixed packet 序列。 | VERIFIED | `tests/fixtures/h264_annexb_sample.h264` 含 SPS/PPS/IDR Annex-B start code；`tests/fixtures/opus_packets.bin` 含两个 non-empty big-endian 16-bit length-prefixed packets；`mrtc_media_fixtures_test` 通过。 |
| 12 | Installed package consumer 能只用 public headers 创建 H264/Opus transceiver 并链接运行。 | VERIFIED | `tests/package-consumer/package_consumer.c` 包含 `micrortc/peer_connection.h`，创建 video/audio transceiver；install/configure/build/run package consumer 命令 exit 0。 |

**Score:** 12/12 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `include/micrortc/peer_connection.h` | Public media API, frame model, callbacks | VERIFIED | 195 lines; exposes transceiver, codec, direction, `MRTC_FRAME`, write/callback APIs without AWS/PIC types. |
| `src/media/media_transceiver.*` | Private transceiver state owner | VERIFIED | Stores kind/codec/direction/MID/SSRC/PT/sequence/callbacks/rolling buffer; core target includes `.c`. |
| `src/peer_connection.c` | API wiring, media send/receive, RTCP ingress | VERIFIED | Implements add/write/protected media receive/protected RTCP receive; review fixes for sendonly SSRC binding and H264 frame cap are present. |
| `src/rtp/rtp_packet.*` | RTP v2 packet primitive | VERIFIED | Built into `micrortc`, tested by `mrtc_rtp_packet_test`. |
| `src/rtp/codecs/h264.*` | H264 Annex-B Single NALU/FU-A primitive | VERIFIED | Dynamic NALU allocation, Annex-B depay, invalid input handling; tests cover >32 NALUs. |
| `src/rtp/codecs/opus.*` | Opus direct payload primitive | VERIFIED | Direct copy and 48kHz timestamp helper; tests pass. |
| `src/rtcp/rtcp_packet.*`, `src/rtcp/retransmitter.*`, `src/rtcp/rtp_rolling_buffer.*` | RTCP SR/RR/NACK/PLI and retransmit behavior | VERIFIED | NACK retransmit uses stored protected RTP bytes; PLI/SR/RR behavior covered. |
| `src/srtp/srtp_session.*` | RTP/RTCP protect/unprotect wrapper | VERIFIED | Real libsrtp branch exists; current no-libsrtp build uses documented passthrough fallback. |
| `tests/integration/mrtc_phase5_media_verify.c` | Phase 5 C fixture verifier | VERIFIED | 557 lines; requires `--fixtures`; prints all planned success labels and exits nonzero on missing fixtures. |
| `tests/package-consumer/package_consumer.c` | Installed public API package validation | VERIFIED | Builds and runs after `cmake --install` with installed `micrortc::micrortc`. |

### Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| Public API | Private transceiver list | `mrtc_peer_connection_add_transceiver()` -> `mrtc_media_transceiver_alloc()` | WIRED | PeerConnection owns list and frees transceivers. |
| `mrtc_transceiver_write_frame()` | H264/Opus RTP send path | codec packetize -> RTP serialize -> SRTP protect -> media send hook | WIRED | `mrtc_media_send_test` and verifier observe packet counts, payload type, SSRC, sequence, marker. |
| Protected inbound RTP | Application `on_frame` | SRTP unprotect -> RTP parse -> depayload -> `mrtc_transceiver_deliver_frame()` | WIRED | H264/Opus callbacks receive reconstructed encoded frames in unit test and verifier. |
| Protected inbound RTCP NACK | RTP rolling buffer resend | `receive_protected_rtcp_packet()` -> `mrtc_rtcp_retransmit_nack()` | WIRED | Verifier requests 2 packets and sees retransmitted count 2. |
| Protected inbound RTCP PLI | Application `on_picture_loss` | `receive_protected_rtcp_packet()` -> `mrtc_peer_connection_handle_pli()` | WIRED | Verifier sees video callback count 1 and audio count 0. |
| CMake install package | Public consumer | installed `micrortc::micrortc` target | WIRED | package consumer configure/build/run exit 0. |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
| --- | --- | --- | --- | --- |
| `mrtc_transceiver_write_frame()` | `MRTC_FRAME.data/size/presentation_ts` | Application/fixture passes encoded H264/Opus frames | Yes | FLOWING |
| H264 receive path | `receive_frame_buffer` -> callback `MRTC_FRAME` | RTP payloads from sent fixture packets | Yes, Annex-B SPS/PPS/IDR bytes round trip | FLOWING |
| Opus receive path | callback `MRTC_FRAME` | RTP payloads from length-prefixed Opus fixture packets | Yes, non-empty packet bytes and monotonic timestamps | FLOWING |
| RTCP NACK path | `sequence_numbers` | RTCP NACK PID/BLP parsed from packet | Yes, packets pulled from rolling buffer and resent | FLOWING |
| Package consumer | public transceiver handles | installed library and headers | Yes, consumer creates H264/Opus handles | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| Full build and test suite | `cmake -S . -B build -DMRTC_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure` | 18/18 tests passed; config reports optional `MRTC_SRTP`/`MRTC_USRSCTP` not found | PASS |
| Phase 5 fixture verifier | `./build/tests/integration/mrtc_phase5_media_verify --fixtures ./tests/fixtures` | Printed all planned labels through `phase5 media verifier complete` | PASS |
| Verifier missing fixture arg | `./build/tests/integration/mrtc_phase5_media_verify` | Nonzero as expected; stderr contains `missing --fixtures path` | PASS |
| Installed package consumer | `cmake --install build --prefix build/install && rm -rf build/package-consumer && cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install" && cmake --build build/package-consumer && ./build/package-consumer/package_consumer` | Configure/build/run exit 0 | PASS |

### Probe Execution

| Probe | Command | Result | Status |
| --- | --- | --- | --- |
| Conventional shell probes | `find scripts -path '*/tests/probe-*.sh' -type f` | No probe scripts found | SKIPPED |
| Phase 5 declared executable verifier | `./build/tests/integration/mrtc_phase5_media_verify --fixtures ./tests/fixtures` | Exact success labels printed | PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
| --- | --- | --- | --- | --- |
| API-04 | 05-01, 05-03, 05-04, 05-05 | 添加音视频 transceiver，并通过 writeFrame 类接口发送编码后媒体帧 | SATISFIED | Public API, PeerConnection write path, verifier and package consumer all exercise H264/Opus transceivers. |
| PROTO-04 | 05-02, 05-03, 05-04, 05-05 | RTP/RTCP packetize/depacketize、RTCP 处理和 NACK 路径 | SATISFIED | RTP/H264/Opus/RTCP primitives in core target; CTest and verifier pass NACK retransmit. |
| MEDIA-01 | 05-01, 05-05 | 只接收和输出编码后媒体帧，不实现采集 | SATISFIED | API accepts `MRTC_FRAME` encoded bytes and emits encoded callback frames; no capture API. |
| MEDIA-02 | 05-01, 05-05 | 不实现编码器，使用外部或固定测试文件提供编码后帧 | SATISFIED | Fixtures provide encoded H264/Opus bytes; no encoder/container dependency. |
| MEDIA-03 | 05-01, 05-02, 05-03, 05-05 | H264 video send path | SATISFIED | H264 packetizer/send path and fixture verifier `h264 send ok`. |
| MEDIA-04 | 05-02, 05-03, 05-04, 05-05 | H264 receive path and observable media | SATISFIED | H264 depay/reassembly and verifier `h264 receive ok`; PLI behavior covered. |
| MEDIA-05 | 05-01, 05-02, 05-03, 05-05 | Opus audio send path | SATISFIED | Opus direct payload/write path and verifier `opus send ok`. |
| MEDIA-06 | 05-02, 05-03, 05-05 | Opus receive path and observable media | SATISFIED | Opus depayload/on_frame and verifier `opus receive ok`. |
| MEDIA-07 | 05-05 | C 端 E2E 发送媒体源支持读取固定 H264/Opus 测试文件 | SATISFIED | `tests/media/test_media_fixtures.c` and `mrtc_phase5_media_verify --fixtures` read both fixed fixtures. |

No Phase 5 requirement IDs are orphaned in `.planning/REQUIREMENTS.md`; the Phase 5 mapping lists exactly API-04, PROTO-04, MEDIA-01 through MEDIA-07.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| --- | --- | --- | --- | --- |
| `.planning/ROADMAP.md` | 89 | `placeholder 示例` | Info | Intentional non-secret TURN credential example note, not a stub and not in code path. |

No unreferenced `TBD`/`FIXME`/`XXX` debt markers were found in Phase 5 source/test artifacts. The known SRTP passthrough path is documented in Plan 05-03 and visible via `mrtc_srtp_session_is_passthrough()`.

### Human Verification Required

None for Phase 5 scope. Chrome/browser media E2E and real dependency validation are explicitly Phase 6 according to ROADMAP and this phase prompt.

### Gaps Summary

No blocking gaps found. Phase 5 achieves the C fixture harness goal and package consumer validation. Residual risk: this verification ran on the documented no-libsrtp passthrough fallback because local `MRTC_SRTP` was not found; real SRTP/browser interoperability remains Phase 6 scope.

---

_Verified: 2026-05-13T06:37:56Z_
_Verifier: the agent (gsd-verifier)_
