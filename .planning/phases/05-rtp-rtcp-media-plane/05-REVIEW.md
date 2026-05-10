---
phase: 05-rtp-rtcp-media-plane
reviewed: 2026-05-10T16:57:07Z
depth: standard
files_reviewed: 29
files_reviewed_list:
  - docs/000-设计边界记录.md
  - docs/API-执行器与内存契约.md
  - examples/create_destroy.c
  - include/rtc/config.h
  - include/rtc/counters.h
  - include/rtc/limits.h
  - include/rtc/media.h
  - include/rtc/observer.h
  - include/rtc/peer_connection.h
  - include/rtc/trace.h
  - src/api/peer_connection.c
  - src/api/peer_connection.h
  - src/media/media.c
  - src/media/media.h
  - src/observability/counters.c
  - src/rtcp/rtcp.c
  - src/rtcp/rtcp.h
  - src/rtp/rtp.c
  - src/rtp/rtp.h
  - tests/test_datagram.c
  - tests/test_ice.c
  - tests/test_jsep.c
  - tests/test_main.c
  - tests/test_media_api.c
  - tests/test_observability.c
  - tests/test_peer_connection.c
  - tests/test_rtcp.c
  - tests/test_rtp.c
  - tests/test_security.c
findings:
  critical: 3
  warning: 2
  info: 0
  total: 5
status: issues_found
---

# Phase 5：代码审查报告

**Reviewed:** 2026-05-10T16:57:07Z
**Depth:** standard
**Files Reviewed:** 29
**Status:** issues_found

## Summary

本次按标准深度审查了第 5 阶段 RTP/RTCP 媒体平面相关公共头文件、实现、示例、文档和测试。构建与测试命令 `cmake --build build && ctest --test-dir build --output-on-failure` 当前通过，但审查发现 3 个必须修复的行为缺陷，以及 2 个会降低可靠性或掩盖真实集成问题的警告。

## Critical Issues

### CR-01：H264 发送会泄漏未使用的固定 media slot

**分类：BLOCKER**
**File:** `src/media/media.c:640`
**Issue:** `rtc_media_send_h264` 先按 `slot_count = min(max_packets_per_frame, max_media_queue_slots)` 预占多个固定槽，但 `rtc_rtp_packetize_h264` 实际可能只产生 `packet_count` 个包。函数只 dispatch `0..packet_count-1`，没有释放 `packet_count..slot_count-1`。例如默认测试配置 `max_packets_per_frame=4`、单个小 H264 NALU 只产生 1 个 RTP 包，剩余 3 个 slot 永久 `in_use=1`；下一次 H264 发送只能拿到一个槽，随后返回 `RTC_STATUS_CAPACITY`。这破坏固定内存槽复用，真实视频流会在第二帧或数帧后停止发送。
**Fix:**
```c
/* packetize 成功后，先释放没有被使用的预占槽。 */
for (i = packet_count; i < slot_count; ++i) {
    rtc_media_release_slot(slots[i]);
    slots[i] = 0;
}

for (i = 0; i < packet_count; ++i) {
    slots[i]->payload_len = packets[i].len;
    status = rtc_media_dispatch_slot(pc, slots[i], frame->kind,
                                     packets[i].sequence,
                                     packets[i].timestamp);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slots(slots + i + 1, packet_count - i - 1);
        return status;
    }
}
```

### CR-02：H264 丢失 FU-A 尾包后会把下一帧拼进损坏 access unit

**分类：BLOCKER**
**File:** `src/rtp/rtp.c:462`
**Issue:** `rtc_rtp_depacketize_h264` 对 single NALU/STAP-A 分支只在 `!*inout_active` 时清空 reassembly buffer。若上一帧收到 FU-A start 后缺少 end，`inout_active` 仍为 1，下一包 single NALU 或 STAP-A 会直接 append 到旧的半截 FU-A buffer；如果 marker 为 1，还会输出混合了旧帧残片和新帧 NALU 的损坏 H264 access unit。这正好违反阶段威胁模型 T-05-04：丢片/乱序不得输出 corrupt access unit。
**Fix:**
```c
if (nalu_type >= 1u && nalu_type <= 23u) {
    if (*inout_active) {
        *inout_active = 0;
        *inout_reassembly_len = 0;
        if (out_drop_reason != 0) {
            *out_drop_reason = "h264_interrupted_au";
        }
    } else {
        *inout_reassembly_len = 0;
    }
    /* 再 append 当前 single NALU */
}

/* STAP-A 分支同样需要在 active 时丢弃旧 AU 后重新开始。 */
```

### CR-03：RTCP PLI/NACK feedback 在 network executor 上回调

**分类：BLOCKER**
**File:** `src/media/media.c:435`
**Issue:** 阶段契约和文档要求 `observer.on_media_feedback` 在 `RTC_EXECUTOR_MEDIA` 上回调，但 RTCP 接收路径在 network executor 上完成 SRTCP unprotect 后直接调用 `rtc_rtcp_parse_compound`，解析器随后同步调用 `rtc_media_emit_pli_feedback` / `rtc_media_emit_nack_feedback`。因此远端 PLI/NACK 会在 network executor 上通知用户，破坏 media/network 亲和边界；用户如果在该回调里驱动编码器或媒体状态，会和文档承诺的 media executor 语义不一致。
**Fix:**
```c
/* RTCP unprotect 后不要直接 emit feedback。
 * 将解析出的 PLI/NACK 复制进固定 media slot / feedback slot，
 * 再通过 pc->executors.media.post(...) 在 media executor 上调用 observer。
 */
status = rtc_rtcp_parse_compound_to_events(pc, slot->payload, packet_len,
                                           fixed_feedback_slots,
                                           &feedback_count);
if (status == RTC_STATUS_OK && feedback_count > 0) {
    status = pc->executors.media.post(pc->executors.media.user_data,
                                      rtc_media_feedback_task,
                                      feedback_slot);
}
```

## Warnings

### WR-01：跨 executor post 后立即读取任务结果，只被同步测试覆盖

**分类：WARNING**
**File:** `src/media/media.c:205`
**Issue:** `rtc_media_dispatch_slot` 和 `rtc_media_dispatch_received_slot` 调用 `post()` 后立即读取 `slot->dispatch_status`。当前测试里的 `post()` 会同步执行任务，所以问题被掩盖；真实执行器可以合法地排队稍后执行，此时 API 会在 protect/unprotect/depacketize 尚未发生时返回旧的 `RTC_STATUS_OK`，并且调用者无法观察后续失败。该模式还让测试无法覆盖队列延迟、任务未运行前销毁 PeerConnection、以及异步 protect 失败的行为。
**Fix:** 明确媒体发送/接收跨 executor 后的 API 语义。如果需要同步错误，不能通过异步 `post()` 实现；如果接受异步完成，则 public API 只能表示“已入队”，后续 protect/unprotect/parse 失败必须通过 observer error、trace、counter 上报，并补充延迟执行器测试。

### WR-02：create_destroy 示例未清零扩展后的配置结构

**分类：WARNING**
**File:** `examples/create_destroy.c:346`
**Issue:** 示例声明 `rtc_peer_connection_config_t config;` 后逐字段赋值，但没有 `memset(&config, 0, sizeof(config))`。第 5 阶段新增 observer/media 字段后，示例没有初始化 `stun_server_count`、`stun_server`、`limits.dtls.max_session_storage_bytes`、`observer.on_media_frame_typed` 和 `observer.on_media_feedback` 等字段；栈上垃圾值可能导致 `rtc_peer_connection_create` 因 STUN 配置非法失败，或后续保存到 observer vtable 后形成无效函数指针。
**Fix:**
```c
rtc_peer_connection_config_t config;

memset(&config, 0, sizeof(config));
config.arena.data = arena;
config.arena.size = sizeof(arena);
/* 后续字段再逐项赋值。 */
```

---

_Reviewed: 2026-05-10T16:57:07Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
