---
phase: 5
slug: rtp-rtcp-media-plane
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-10
---

# Phase 5 — Validation Strategy

> 第 5 阶段执行期间的验证采样契约，覆盖 typed media API、Opus/H264 RTP packetize/depacketize、SRTP/SRTCP 集成、RTCP SR/RR/SDES、PLI/NACK 和媒体可观测性。

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | 纯 C 自研 test runner + CTest |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cmake --build build && ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~1 秒 |

## Sampling Rate

- **After every task commit:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **After every plan wave:** Run `cmake --build build && ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 1 秒

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | RTP-05, OBS-04 | public typed media API、observer feedback、limits/counters/trace 均进入默认构建 | unit/header | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-01-02 | 01 | 1 | RTP-05 | media/network 固定槽容量来自 create-time arena，容量不足可诊断 | unit | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-02-01 | 02 | 2 | RTP-01 | Opus frame 生成 RTP，调用 `rtc_srtp_protect_rtp` 后输出 datagram | integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-02-02 | 02 | 2 | RTP-03 | H264 access unit 支持 single NALU 和 FU-A 发送，protect 失败不输出明文 | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-03-01 | 03 | 3 | RTP-02 | 受保护 Opus RTP unprotect 后输出 typed Opus frame | integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-03-02 | 03 | 3 | RTP-04 | H264 single NALU、FU-A、STAP-A 接收重组为 access unit；失败路径不输出未认证数据 | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-04-01 | 04 | 4 | RTCP-01, RTCP-02 | SR/RR/SDES parse/write 和 stats 更新可重复验证 | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-05-01 | 05 | 5 | RTCP-03 | 用户显式 PLI 请求生成 SRTCP datagram，远端 PLI 上报 feedback | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-05-02 | 05 | 5 | RTCP-04, OBS-04 | NACK 解析并上报，trace/counter 明确 `nack_no_retransmit` | unit/integration | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: planned | pending |
| 05-06-01 | 06 | 6 | RTP-01..OBS-04 | 文档、需求追踪和状态准确表达媒体边界，不宣称 Chrome E2E 已完成 | docs/traceability | `cmake --build build && ctest --test-dir build --output-on-failure` | W0: existing docs | pending |

## Wave 0 Requirements

`wave_0_complete: false` 表示这些文件不是执行前置人工步骤，而是第 5 阶段计划要创建或修改的对象：

- [ ] `include/rtc/media.h` — typed media frame、media kind、feedback/event 类型。
- [ ] `src/media/` — media API、固定跨 executor 槽、dispatch helper。
- [ ] `src/rtp/` — RTP header、Opus/H264 packetize/depacketize。
- [ ] `src/rtcp/` — SR/RR/SDES/PLI/NACK parse/write 与 stats。
- [ ] `tests/test_media_api.c` / `tests/test_rtp.c` / `tests/test_rtcp.c` — 默认测试注册。

## Threat Model References

| Ref | Pattern | STRIDE | Required Mitigation |
|-----|---------|--------|---------------------|
| T-05-01 | SRTP protect 失败后输出明文 RTP/RTCP | Information Disclosure | protect 失败不得调用 `observer.on_datagram` |
| T-05-02 | SRTP unprotect/replay 失败后输出媒体帧 | Tampering | unprotect 失败不得调用 typed media observer |
| T-05-03 | 跨 executor 保存调用方 buffer 指针 | Tampering / Reliability | 所有跨 executor 数据复制进 create-time 固定槽 |
| T-05-04 | H264 FU-A 丢片/乱序仍输出 corrupt access unit | Tampering | 重组失败丢弃当前 AU，并输出 trace/counter |
| T-05-05 | NACK 被误实现为重传 | Reliability | NACK 只 observer 上报，trace reason `nack_no_retransmit` |
| T-05-06 | 多路媒体越界导致状态混淆 | Reliability | v1 只接受 1 路 audio + 1 路 video，非法 kind/超限返回诊断错误 |

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 真实 Chrome 音视频通话 | ACC-01 | 第 5 阶段不包含 Chrome 页面和信令示例，属于第 6 阶段 | 确认文档和状态仍把 Chrome E2E 留在第 6 阶段 |
| 真实编解码器控制关键帧 | RTCP-03 | 库不处理编码器；收到 PLI 后由用户触发编码器 | 确认 API 文档说明用户负责关键帧生成 |

## Validation Sign-Off

- [x] All tasks have automated verification
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 requirements are planned, not assumed complete
- [x] No watch-mode flags
- [x] Feedback latency < 1s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
