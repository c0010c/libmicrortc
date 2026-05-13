---
phase: 05-h264-opus
reviewed: 2026-05-13T06:27:31Z
depth: standard
files_reviewed: 37
files_reviewed_list:
  - cmake/micrortcConfig.cmake.in
  - include/micrortc/peer_connection.h
  - src/media/media_transceiver.c
  - src/media/media_transceiver.h
  - src/peer_connection.c
  - src/rtcp/retransmitter.c
  - src/rtcp/retransmitter.h
  - src/rtcp/rtcp_packet.c
  - src/rtcp/rtcp_packet.h
  - src/rtcp/rtp_rolling_buffer.c
  - src/rtcp/rtp_rolling_buffer.h
  - src/rtp/codecs/h264.c
  - src/rtp/codecs/h264.h
  - src/rtp/codecs/opus.c
  - src/rtp/codecs/opus.h
  - src/rtp/rtp_packet.c
  - src/rtp/rtp_packet.h
  - src/sdp.c
  - src/sdp.h
  - src/srtp/srtp_session.c
  - src/srtp/srtp_session.h
  - tests/fixtures/h264_annexb_sample.h264
  - tests/fixtures/opus_packets.bin
  - tests/integration/mrtc_phase5_media_verify.c
  - tests/media/test_h264_packetizer.c
  - tests/media/test_media_fixtures.c
  - tests/media/test_media_send.c
  - tests/media/test_opus_codec.c
  - tests/media/test_rtcp_packet.c
  - tests/media/test_rtcp_pli_sr_rr.c
  - tests/media/test_rtcp_retransmit.c
  - tests/media/test_rtp_packet.c
  - tests/media/test_rtp_rolling_buffer.c
  - tests/package-consumer/package_consumer.c
  - tests/peer_connection/test_peer_connection_api.c
  - tests/sdp/test_sdp_roundtrip.c
  - tests/transport/test_dtls_srtp.c
findings:
  critical: 0
  warning: 2
  info: 0
  total: 2
original_findings:
  critical: 2
  warning: 3
  info: 0
  total: 5
remediation_commit: a8bc539
status: issues_found
---

# Phase 05: Code Review Report

**Reviewed:** 2026-05-13T06:27:31Z
**Depth:** standard
**Files Reviewed:** 37
**Status:** issues_found

## Summary

审查覆盖了 Phase 05 的公共 API、RTP/H264/Opus/RTCP/SRTP 媒体路径、SDP 生成、打包配置和相关测试/fixture。当前 `build` 配置中 `MRTC_SRTP_LIBRARY` 与 `MRTC_USRSCTP_LIBRARY` 均为 `NOTFOUND`，`ctest --test-dir build --output-on-failure` 18/18 通过，但这些测试是在 SRTP 明文透传 fallback 下通过的，不能证明真实 WebRTC 安全媒体路径可用。

主要风险最初集中在两处：默认缺少 libsrtp 时仍把会话标记为 ready 并使用可检测的 RTP/RTCP passthrough fallback；接收端按 payload type 绑定 SSRC 时会先绑定到不能接收的 transceiver，导致合法入站媒体被永久拒绝。

## Remediation Update

修复提交：`a8bc539 fix(05): address media review findings`

当前开放项：

- `CR-01` 已降级为 Phase 6/真实依赖风险：Phase 5 计划明确要求无 libsrtp 时保留 deterministic passthrough fallback，并通过 `mrtc_srtp_session_is_passthrough()` 可检测；真实 libsrtp/browser E2E 属于 Phase 6。
- `WR-03` 仍为 warning：默认 SDP helper 的固定 ICE/fingerprint 测试便利入口后续应收紧或限制可见性。

已修复项：

- `CR-02` 已修复：入站 SSRC 只绑定到可接收 transceiver，并补充 sendonly-before-recvonly 回归测试。
- `WR-01` 已修复：H264 packetizer 改为动态 NALU 表，并补充超过 32 个 NALU 的回归测试。
- `WR-02` 已修复：H264 接收缓冲增加最大帧大小与扩容/加法溢出保护。

## Critical Issues

### CR-01: SRTP 缺失时媒体路径退化为明文透传 [ACCEPTED RISK FOR PHASE 5]

**File:** `src/srtp/srtp_session.c:116`

**Issue:** `MRTC_HAVE_SRTP` 未定义时，`mrtc_srtp_session_init_from_dtls()` 仍设置 `session->ready = 1` 并在 `#else` 分支把 `session->passthrough = 1`。这意味着完成 DTLS 后，`mrtc_srtp_protect_rtp()` 和 `mrtc_srtp_unprotect_rtp()` 只是返回 OK，不加密、不认证 RTP/RTCP。当前默认构建允许 `MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=OFF`，本机配置也显示 libsrtp 未找到，所以测试实际覆盖的是明文路径。对 Phase 05 的 WebRTC 媒体来说这是安全漏洞，也会和浏览器的 SRTP 预期不兼容。

**Fix:**
```c
#ifndef MRTC_HAVE_SRTP
    (void) session;
    (void) keying_material;
    (void) local_role;
    return MRTC_STATUS_INVALID_STATE;
#else
    if (mrtc_srtp_init_libsrtp_contexts(session) != MRTC_STATUS_OK) {
        mrtc_srtp_session_deinit(session);
        return MRTC_STATUS_INVALID_STATE;
    }
    session->passthrough = 0;
    return MRTC_STATUS_OK;
#endif
```

同时让配置在启用真实媒体功能时强制发现 libsrtp，并新增一个测试断言：协商完成后 `mrtc_peer_connection_media_is_srtp_passthrough(peer_connection)` 必须为 false，或在无 libsrtp 构建下连接失败而不是明文发送。

### CR-02: 入站 SSRC 会绑定到 sendonly/inactive transceiver，后续合法媒体被拒绝 [FIXED]

**File:** `src/peer_connection.c:259`

**Issue:** `mrtc_peer_connection_find_media_receiver()` 先按 payload type 选择第一个 `remote_ssrc == 0` 的 transceiver，并在返回前直接写入 `payload_match->remote_ssrc = packet->ssrc`。调用方在 `src/peer_connection.c:1098-1103` 才检查方向是否为 `SENDONLY` 或 `INACTIVE`。如果同一 payload type 有多个 transceiver，例如第一个 video 为 sendonly，第二个 video 为 recvonly/sendrecv，第一包远端 H264 会先污染 sendonly transceiver 的 `remote_ssrc`，本次返回 `INVALID_STATE`，后续同 SSRC 包也会继续命中这个 sendonly transceiver 并被拒绝。

**Fix:**
```c
static int mrtc_transceiver_can_receive(MRTC_RTP_TRANSCEIVER_HANDLE t)
{
    return t != 0 &&
           t->direction != MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY &&
           t->direction != MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE;
}

/* 在 find_media_receiver 中只考虑 can_receive 的 transceiver */
if (!mrtc_transceiver_can_receive(current)) {
    current = current->next;
    continue;
}
```

并把 `remote_ssrc` 绑定推迟到确认 transceiver 可接收之后。补一个回归测试：先添加 H264 sendonly，再添加 H264 recvonly/sendrecv，发送 payload type 96 的入站 RTP，断言帧交付给可接收 transceiver 且 sendonly 的 `remote_ssrc` 未被写入。

## Warnings

### WR-01: H264 packetizer 固定最多 32 个 NALU，会拒绝合法多 slice 帧 [FIXED]

**File:** `src/rtp/codecs/h264.c:177`

**Issue:** `mrtc_h264_packetize_annexb()` 使用 `MRTC_H264_NALU stack_nalus[32]`，并把 `nalu_count` 固定为 32。有效 H264 access unit 可以包含超过 32 个 NALU，特别是多 slice 编码、重复参数集或 SEI 较多时。当前实现会在 `mrtc_h264_find_nalus()` 中返回 `MRTC_STATUS_INVALID_ARG`，导致 `mrtc_transceiver_write_frame()` 拒绝合法帧。

**Fix:** 改成两遍解析：第一遍只计数，第二遍按实际 NALU 数动态分配数组；或者在 packetize 过程中边解析边累计 payload 大小，避免保存固定长度 NALU 表。新增超过 32 个小 NALU 的 Annex-B fixture/单测。

### WR-02: H264 接收重组缓冲区对远端输入没有上限或溢出保护 [FIXED]

**File:** `src/peer_connection.c:318`

**Issue:** `needed = receive_frame_size + data_size` 没有检查 `size_t` 溢出，随后循环翻倍 `capacity` 并 `realloc`。远端可以持续发送同一 timestamp、marker=0 的 H264 分片，让 `receive_frame_buffer` 无上限增长；极端情况下还可能在 size 计算溢出后执行越界 `memcpy`。这属于远端可触发的资源耗尽/内存安全风险。

**Fix:** 增加最大接收帧大小并做加法/扩容溢出检查。例如：
```c
if (data_size > MRTC_MAX_H264_FRAME_SIZE ||
    transceiver->receive_frame_size > MRTC_MAX_H264_FRAME_SIZE - data_size) {
    transceiver->receive_frame_size = 0;
    return MRTC_STATUS_INVALID_STATE;
}
```

同时在 H264 分片乱序、缺 marker、超大帧路径上补失败测试。

### WR-03: 默认 SDP helper 使用固定 ICE 凭据和固定指纹 [WARNING]

**File:** `src/sdp.c:590`

**Issue:** `mrtc_sdp_create_answer()` 调用 `mrtc_sdp_create_answer_ex()` 时传入固定 `mrtcufrag`、固定 `mrtcpassword...` 和固定 fingerprint。虽然 peer connection 当前走 `mrtc_sdp_create_answer_with_media()` 并注入随机本地 ICE 值，但这个非 static helper 仍会进入库符号表，内部测试也调用它。任何后续复用都会生成可预测 ICE 凭据和伪造指纹。

**Fix:** 移除无参数默认凭据 helper，或让它返回 `MRTC_STATUS_INVALID_ARG` 并要求调用方显式传入随机 ICE ufrag/pwd 与真实 DTLS fingerprint。保留测试时应改用 `mrtc_sdp_create_answer_ex()` 并传入测试专用值。

---

_Reviewed: 2026-05-13T06:27:31Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
