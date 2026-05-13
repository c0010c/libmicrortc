---
phase: 05-h264-opus
created: 2026-05-13
---

# Phase 5: H264/Opus 媒体路径 - Patterns

## Existing Project Patterns To Reuse

| Target area | Current analog | Reuse rule |
|-------------|----------------|------------|
| Public headers | `include/micrortc/micrortc.h`, `include/micrortc/peer_connection.h` | 继续使用 include guard、`extern "C"`、opaque handle、`MRTC_STATUS`、`MRTC_*` 类型和 `mrtc_*` 函数；不要暴露 AWS/PIC 类型。 |
| PeerConnection owner | `src/peer_connection.c` | private struct 持有 owned resources；transceiver list、media counters、SSRC、payload type 和 callbacks 在 `mrtc_peer_connection_free()` 清理。 |
| SDP helper | `src/sdp.c`, `src/sdp.h` | 保持 private helper；扩展为 media-aware offer/answer，而不是把 SDP parser 暴露成 public API。 |
| SRTP wrapper | `src/srtp/srtp_session.c`, `src/srtp/srtp_session.h` | 在现有 DTLS key direction 基础上新增 RTP/RTCP protect/unprotect 边界；第三方类型保持 private。 |
| Tests | `tests/peer_connection/test_peer_connection_api.c`, `tests/transport/test_dtls_srtp.c` | 小型 C `main()` + CTest；新媒体 unit 放 `tests/media/`，集成 verifier 放 `tests/integration/`。 |
| Integration verifier | `tests/integration/mrtc_phase4_network_verify.c` | 使用清晰 label 输出，缺少必需输入时非零退出；Phase 5 verifier 要区分 H264/Opus/RTCP/SRTP 层。 |
| Source tracing | `.planning/phases/01-/SOURCE-MANIFEST.md` | 每个 KVS 派生或参考文件追加 Phase 5 行，origin_commit 使用 `9eebcc4`。 |

## Reference Patterns From Local KVS Baseline

| Capability | Local reference | Extraction guidance |
|------------|-----------------|---------------------|
| Media public API | `reflib/kvs-webrtc-sdk/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h` | 参考 `Frame`、`addTransceiver`、`transceiverOnFrame`、`transceiverOnPictureLoss`、`writeFrame`，改为 MRTC 命名和标准 C 类型。 |
| RTP packet parse/serialize | `reflib/kvs-webrtc-sdk/src/source/Rtp/RtpPacket.*` | 重写为 private `src/rtp/rtp_packet.*`，只覆盖 RTP v2 header、payload、sequence、timestamp、SSRC、marker。 |
| H264 payloader | `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpH264Payloader.*` | 保留 Annex-B start code、Single NALU、FU-A、Annex-B depay 输出语义；不做 AVCC。 |
| Opus payloader | `reflib/kvs-webrtc-sdk/src/source/Rtp/Codecs/RtpOpusPayloader.*` | 保留直拷贝 packetize/depay 语义。 |
| RTCP packet | `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtcpPacket.*` | 先覆盖 SR、RR、Generic RTP Feedback NACK、Payload Specific Feedback PLI。 |
| Rolling buffer | `reflib/kvs-webrtc-sdk/src/source/Rtcp/RtpRollingBuffer.*`, `RollingBuffer.*` | 重写为 small fixed-capacity RTP packet buffer，按 sequence number 查找重发包。 |
| Send path | `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtp.c` | 参考 payload -> RTP packet -> SRTP protect -> ICE send -> rolling buffer 顺序。 |
| RTCP handling | `reflib/kvs-webrtc-sdk/src/source/PeerConnection/Rtcp.c`, `Retransmitter.*` | 参考 NACK 触发重发、PLI 触发 callback、SR/RR 处理入口。 |
| Receive path | `reflib/kvs-webrtc-sdk/src/source/PeerConnection/JitterBuffer.*`, `PeerConnection.c` | 参考 RTP packet reorder/reassembly 到 frame-ready callback；重写为 MRTC private。 |
| Media SDP | `reflib/kvs-webrtc-sdk/src/source/PeerConnection/SessionDescription.*` | 参考 H264/Opus rtpmap/fmtp/rtcp-fb、direction、mid、SSRC 语义。 |

## Planned Files And Closest Analogs

| Planned file/path | Role | Closest analog |
|-------------------|------|----------------|
| `include/micrortc/peer_connection.h` | public transceiver/frame/media callbacks/write API | Existing header + KVS `Include.h` |
| `src/media/media_transceiver.h`, `src/media/media_transceiver.c` | private transceiver owner, sender/receiver state, callback dispatch | Current `src/data_channel/*`, KVS `PeerConnection/Rtp.h` |
| `src/rtp/rtp_packet.h`, `src/rtp/rtp_packet.c` | RTP parse/serialize/build helpers | KVS `RtpPacket.*` |
| `src/rtp/codecs/h264.h`, `src/rtp/codecs/h264.c` | H264 Annex-B packetize/depacketize | KVS `RtpH264Payloader.*` |
| `src/rtp/codecs/opus.h`, `src/rtp/codecs/opus.c` | Opus packetize/depacketize | KVS `RtpOpusPayloader.*` |
| `src/rtcp/rtcp_packet.h`, `src/rtcp/rtcp_packet.c` | SR/RR/NACK/PLI parse/generate | KVS `RtcpPacket.*` |
| `src/rtcp/rtp_rolling_buffer.h`, `src/rtcp/rtp_rolling_buffer.c` | sent RTP packet retention for NACK | KVS `RtpRollingBuffer.*` |
| `src/rtcp/retransmitter.h`, `src/rtcp/retransmitter.c` | NACK sequence list to retransmit send path | KVS `Retransmitter.*` |
| `src/srtp/srtp_session.*` | RTP/RTCP protect/unprotect extension | Current SRTP wrapper + KVS `SrtpSession.*` |
| `src/sdp.*` | H264/Opus m-line and feedback SDP | Current SDP helper + KVS `SessionDescription.*` |
| `tests/media/*` | codec/RTP/RTCP unit tests and fixture readers | Current CTest style |
| `tests/fixtures/h264_annexb_sample.h264` | fixed H264 bytestream fixture | Context D-06/D-08 |
| `tests/fixtures/opus_packets.bin` | length-prefixed Opus packet fixture | Context D-15 |
| `tests/integration/mrtc_phase5_media_verify.c` | layered Phase 5 harness | Phase 4 verifier |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | source trace updates | Existing manifest |

## CMake Pattern

- Add new core sources directly to `add_library(micrortc STATIC ...)`.
- Keep RTP/RTCP/media headers private under `src/`.
- Register unit tests under `if(MRTC_BUILD_TESTS)`.
- Keep package consumer build green; installed public headers must not require libsrtp/usrsctp/OpenSSL headers.

## Testing Pattern

| Group | Expected tests |
|-------|----------------|
| Public API | `mrtc_peer_connection_add_transceiver` invalid args, H264/Opus transceiver creation, callback setter, write before SRTP ready deterministic status. |
| RTP/codec | H264 Single NALU, FU-A fragmentation/reassembly, invalid Annex-B input, Opus direct payload, RTP sequence/timestamp/header serialization. |
| RTCP/NACK/PLI | NACK PID/BLP parse, rolling buffer lookup, retransmit callback, PLI callback dispatch, SR/RR parse/generate. |
| SRTP integration | RTP/RTCP protect/unprotect wrapper calls, key direction preserved, fallback behavior deterministic when libsrtp unavailable. |
| Fixture flow | H264 Annex-B fixture sends and receives a frame; Opus length-prefixed fixture sends and receives non-empty packets with monotonic timestamp. |

## Quality Constraints For Plans

- Every plan must reference relevant D-IDs in `<must_haves>` or task actions.
- Any plan touching KVS-derived behavior must include `.planning/phases/01-/SOURCE-MANIFEST.md` acceptance criteria.
- Public API plans must grep public headers for AWS/PIC type leaks.
- RTCP/NACK plans must prove behavior, not only parser coverage.
- Fixture/harness plan must not claim Phase 6 Chrome automated E2E is complete.
