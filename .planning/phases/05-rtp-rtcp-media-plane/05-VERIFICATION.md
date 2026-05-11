---
phase: 05-rtp-rtcp-media-plane
verified: 2026-05-10T17:04:36Z
status: passed
score: 10/10 must-haves verified
overrides_applied: 0
human_approved: 2026-05-11T01:06:17+08:00
human_verification:
  - test: "异步 executor 语义确认（承接 05-REVIEW.md WR-01）"
    expected: "如果 executor.post 合法延迟执行，媒体 API 返回值、后续失败上报、slot 生命周期和 destroy 时排队任务处理语义应被产品契约接受，或后续用延迟执行器测试补齐。"
    result: "approved"
    why_human: "当前自动化测试使用同步 test executor；代码复审将该项列为 advisory warning，非阻塞但需要开发者确认 API 语义。开发者已批准当前语义继续收口。"
---

# Phase 05：RTP/RTCP 媒体平面验证报告

**阶段目标：** 完成 1 路 Opus 音频和 1 路 H264 视频的 RTP/RTCP 媒体平面，并把关键反馈事件暴露给用户。  
**验证时间：** 2026-05-10T17:04:36Z  
**状态：** passed  
**复验模式：** 否，初次阶段级验证。  
**说明：** `gsd-sdk` 在当前 shell 中不可用，因此路线图与需求合同直接从 `.planning/ROADMAP.md`、`.planning/REQUIREMENTS.md` 和 05 阶段 PLAN frontmatter 读取。

## 目标达成

### 可观察事实

| # | 事实 | 状态 | 代码证据 |
|---|------|------|----------|
| 1 | 用户提交 Opus frame 后生成 RTP，并且只有 SRTP protect 成功后才输出 datagram。 | 已验证 | `rtc_peer_connection_send_media_frame` 校验 media executor 后接入 `rtc_media_send_frame`；`rtc_media_send_opus` 调用 `rtc_rtp_packetize_opus`，`rtc_media_network_send_task` 调用 `rtc_srtp_protect_rtp` 后才调用 `observer.on_datagram`。测试：`test_opus_send_outputs_protected_rtp_datagram`、`test_opus_protect_failure_does_not_output_plaintext`。 |
| 2 | 受保护 Opus RTP 输入后，SRTP unprotect 成功才输出 typed Opus frame。 | 已验证 | `rtc_peer_connection_receive_datagram` 的 RTP 分支接入 `rtc_media_handle_rtp_datagram`；该函数先调用 `rtc_srtp_unprotect_rtp`，再 parse header 并投递 media executor；Opus payload 通过 `on_media_frame_typed` 输出。测试覆盖 unprotect 成功和失败不输出。 |
| 3 | H264 access unit 发送支持 Annex B single NALU 和 FU-A。 | 已验证 | `rtc_rtp_packetize_h264` 解析 `00 00 01`/`00 00 00 01` start code，按 `max_payload_bytes` 选择 single NALU 或 FU-A；marker 只在最后包设置。测试覆盖 single NALU、FU-A、多包 marker 和 `max_packets_per_frame` 容量失败。 |
| 4 | H264 接收支持 single NALU、FU-A 重组和有限 STAP-A，失败不输出损坏 access unit。 | 已验证 | `rtc_rtp_depacketize_h264` 覆盖 single NALU、STAP-A 和 FU-A；sequence gap、missing start、capacity、STAP-A length 错误会 drop 并记录 `h264_reassembly_drops`/trace。测试覆盖 single、FU-A、STAP-A、中断 AU、gap 和容量失败。 |
| 5 | 单个 `PeerConnection` 的 v1 媒体规模限制为 1 路 Opus audio 和 1 路 H264 video，并且容量来自 create-time limits。 | 已验证 | `include/rtc/media.h` 只公开 `RTC_MEDIA_KIND_AUDIO_OPUS` 与 `RTC_MEDIA_KIND_VIDEO_H264`；`rtc_validate_config` 要求 media/RTP/RTCP limits 非零；create 路径分配固定 media queue、packet cache 和 H264 reassembly buffer。 |
| 6 | RTCP Sender Report 和 Receiver Report 可生成、解析并更新计数器。 | 已验证 | `rtc_rtcp_write_sender_report`、`rtc_rtcp_write_receiver_report`、`rtc_rtcp_parse_compound` 存在；`rtcp_sr_received`、`rtcp_rr_received`、`rtcp_sr_sent` 在代码和测试中被验证。 |
| 7 | RTCP SDES 可生成、解析并受 CNAME limit 约束。 | 已验证 | `rtc_rtcp_write_sdes` 按 `max_sdes_cname_bytes` 拒绝过长 CNAME；parse 路径更新 `rtcp_sdes_received`；测试覆盖 round trip 与越界。 |
| 8 | PLI 可由用户显式请求发送，远端 PLI 通过 feedback observer 上报。 | 已验证 | `rtc_peer_connection_request_keyframe(video)` 只允许 media executor 和 H264；`rtc_media_request_keyframe` 写 PLI，SRTCP protect 成功后输出 datagram；远端 PLI 通过 `rtc_media_emit_pli_feedback` 上报 `RTC_MEDIA_FEEDBACK_PLI`。 |
| 9 | NACK 只解析并上报，不执行重传。 | 已验证 | `rtc_rtcp_parse_nack` 展开 PID/BLP 到固定 17 项数组；`rtc_media_emit_nack_feedback` 设置 `retransmit_performed = 0`，递增 `nack_received` 和 `nack_no_retransmit`，trace reason 为 `nack_no_retransmit`。源码反向扫描未发现 `rtx`、`retransmit_cache` 或 `resend`。 |
| 10 | 关键反馈和安全失败有可观测输出。 | 已验证 | PLI/NACK 走 `on_media_feedback`、counter 和 trace；SRTP/SRTCP protect/unprotect 失败由 `src/srtp` wrapper 记录错误、trace 和 counter，媒体路径在失败时不输出 datagram/frame/feedback。 |

**得分：** 10/10 个 must-have 已通过代码和测试证据验证；1 个人工确认项已由开发者批准。

### 必要工件

| 工件 | 期望 | 状态 | 细节 |
|------|------|------|------|
| `include/rtc/media.h` | typed frame、media kind、feedback 类型 | 已验证 | 定义 `rtc_media_frame_t`、`rtc_media_feedback_t`、PLI/NACK 和固定 lost sequence 数组。 |
| `include/rtc/peer_connection.h` | public media API | 已验证 | 暴露 `rtc_peer_connection_send_media_frame` 和 `rtc_peer_connection_request_keyframe`。 |
| `include/rtc/observer.h` | typed frame/feedback observer | 已验证 | 暴露 `on_media_frame_typed`、`on_media_feedback` 和既有 `on_datagram`。 |
| `src/media/media.c` | media API、executor 分层、SRTP/SRTCP gate | 已验证 | 发送、接收、RTCP report、PLI/NACK feedback 主链路均已接入。 |
| `src/rtp/rtp.c` | RTP header、Opus/H264 packetize/depacketize | 已验证 | 包含 Opus packetize、H264 single/FU-A 发送和 H264 single/FU-A/STAP-A 接收。 |
| `src/rtcp/rtcp.c` | SR/RR/SDES/PLI/NACK codec | 已验证 | 包含 writer/parser 和 compound dispatch。 |
| `tests/test_media_api.c` | API contract、亲和和 limits 测试 | 已验证 | 已在 `tests/test_main.c` 注册。 |
| `tests/test_rtp.c` | RTP 发送/接收与 SRTP 失败测试 | 已验证 | 覆盖 Opus/H264、protect/unprotect、capacity 和 reassembly。 |
| `tests/test_rtcp.c` | RTCP 与 feedback 测试 | 已验证 | 覆盖 SR/RR/SDES、PLI、NACK、SRTCP protect/unprotect。 |
| `05-REVIEW.md` | 最新代码复审状态 | 已验证 | advisory；0 critical，1 warning（WR-01）保留为人工确认项。 |

### 关键链路

| From | To | Via | 状态 | 细节 |
|------|----|-----|------|------|
| `rtc_peer_connection_send_media_frame` | RTP packetize + SRTP protect | `src/api/peer_connection.c` -> `rtc_media_send_frame` -> `rtc_rtp_packetize_*` -> `rtc_srtp_protect_rtp` | 已接线 | protect 成功才 `on_datagram`。 |
| `rtc_peer_connection_receive_datagram` | RTP depacketize + typed frame | demux RTP -> `rtc_media_handle_rtp_datagram` -> `rtc_srtp_unprotect_rtp` -> media executor callback | 已接线 | unprotect 失败直接返回，不输出 media frame。 |
| `rtc_peer_connection_receive_datagram` | RTCP parse + feedback | demux RTCP -> `rtc_media_handle_rtcp_datagram` -> `rtc_srtp_unprotect_rtcp` -> `rtc_rtcp_parse_compound` | 已接线 | RTCP feedback 在 media executor 上报。 |
| `rtc_peer_connection_request_keyframe` | protected PLI datagram | API -> `rtc_media_request_keyframe` -> `rtc_rtcp_write_pli` -> `rtc_srtp_protect_rtcp` -> `on_datagram` | 已接线 | Opus keyframe 请求返回 unsupported。 |
| NACK RTCP packet | observer feedback/counter/trace | `rtc_rtcp_parse_nack` -> `rtc_media_emit_nack_feedback` | 已接线 | `retransmit_performed = 0`，无重传缓存。 |

### Data-Flow Trace

| 工件 | 数据变量 | 来源 | 产生真实数据 | 状态 |
|------|----------|------|--------------|------|
| `src/media/media.c` Opus send | `frame->data` -> RTP payload -> `slot->payload` | 用户传入 `rtc_media_frame_t`，复制/写入固定 slot | 是 | flowing |
| `src/media/media.c` RTP receive | protected datagram -> `slot->payload` -> typed frame | 用户输入 datagram，SRTP unprotect 后 parse RTP header | 是 | flowing |
| `src/rtp/rtp.c` H264 reassembly | `reassembly_buffer` / `h264_reassembly_len` | single/FU-A/STAP-A RTP payload | 是 | flowing |
| `src/rtcp/rtcp.c` NACK feedback | `rtc_media_feedback_t` | RTCP RTPFB Generic NACK FCI PID/BLP | 是 | flowing |
| `src/media/media.c` PLI send | RTCP PLI packet | public `request_keyframe(video)` | 是 | flowing |

### 行为抽查

| 行为 | 命令 | 结果 | 状态 |
|------|------|------|------|
| 全量构建和 CTest | `cmake --build build && ctest --test-dir build --output-on-failure` | 1/1 `rtc_tests` passed | 通过 |
| 禁止运行期动态分配/线程/socket | `rg` 扫描 `malloc/calloc/realloc`、`pthread_create`、`socket(` | 目标源码无命中 | 通过 |
| 禁止 NACK 重传实现 | `rg` 扫描 `rtx/retransmit_cache/resend` | `src/include/tests` 无命中 | 通过 |
| Chrome E2E 边界 | 检查 PROJECT/REQUIREMENTS/ROADMAP/STATE/docs | `ACC-01`、`EXM-01`、`EXM-02` 保持第 6 阶段待开始；没有 Phase 5 完成 Chrome E2E 的有效宣称 | 通过 |

### 需求覆盖

| Requirement | Source Plan | 描述 | 状态 | 证据 |
|-------------|-------------|------|------|------|
| RTP-01 | 05-02, 05-06 | Opus frame packetize、timestamp、sequence、SRTP protect | 满足 | Opus packetize/send path + tests。 |
| RTP-02 | 05-03, 05-06 | Opus RTP depacketize 到 frame | 满足 | RTP unprotect 后 typed Opus callback + tests。 |
| RTP-03 | 05-02, 05-06 | H264 single NALU / FU-A 发送 | 满足 | H264 packetizer + tests。 |
| RTP-04 | 05-03, 05-06 | H264 access unit 接收，有限 STAP-A | 满足 | H264 depacketizer/reassembly + tests。 |
| RTP-05 | 05-01, 05-06 | 1 路音频 + 1 路视频 | 满足 | public kind 仅 Opus/H264，非法 kind 测试，文档说明 v1 不做多路。 |
| RTCP-01 | 05-04, 05-06 | Sender Report / Receiver Report | 满足 | SR/RR writer/parser/counter tests。 |
| RTCP-02 | 05-04, 05-06 | SDES | 满足 | SDES writer/parser/limit tests。 |
| RTCP-03 | 05-05, 05-06 | PLI 发送和接收 | 满足 | request_keyframe + PLI feedback tests。 |
| RTCP-04 | 05-05, 05-06 | NACK 上报、不重传 | 满足 | NACK parser + no-retransmit counter/trace + reverse scan。 |
| OBS-04 | 05-01, 05-05, 05-06 | NACK/PLI/SRTP 失败等可观测 | 满足 | feedback observer、counter、trace 和 SRTP failure tests。 |

未发现 Phase 5 需求孤儿项；Phase 6 的 `TST-01`、`EXM-01`、`EXM-02`、`ACC-01` 在路线图中明确待开始。

### 反模式扫描

| 文件 | 行 | 模式 | 严重性 | 影响 |
|------|----|------|--------|------|
| `src/media/media.c` | 205, 335, 359, 567 | `post()` 后立即读取 `slot->dispatch_status` | WARNING | 与 `05-REVIEW.md` WR-01 一致：同步测试覆盖了当前行为，但真实异步 executor 下 API 返回值和 slot 生命周期语义需确认。 |

源码目标文件未发现 TODO/FIXME/placeholder、运行期 `malloc/calloc/realloc`、线程/socket 创建或 NACK 重传缓存。

### 人工验证结果

#### 1. 异步 executor 语义确认

**测试：** 使用合法但延迟执行的 media/network executor，验证 `send_media_frame`、`receive_datagram`、`request_keyframe` 在 `post()` 后任务稍后执行时的返回值语义、失败上报、slot 生命周期和 destroy 行为。  
**期望：** 若 public API 语义是“已入队”，则返回值只表示入队成功，后续 protect/unprotect/parse 失败必须通过 observer error、trace 或 counter 可观测；若语义要求同步返回失败，则实现需要调整。  
**结果：** passed — 开发者批准当前语义，允许第 5 阶段作为非阻塞设计警告继续收口。`05-REVIEW.md` 的 WR-01 保留为后续契约/延迟执行器测试增强项。

**为什么需要人工：** 当前阶段不修改源码；现有测试 executor 同步执行，`05-REVIEW.md` 已将该项定为 advisory warning（0 critical，1 warning）。

### 缺口摘要

没有发现阻塞阶段目标的实现缺口。所有 Phase 5 requirement IDs 均能从 public API、源码实现、测试和中文文档追踪到实际证据。异步 executor 语义已由开发者批准作为非阻塞设计警告继续收口；这不是代码复审 critical，也不是必须阻断的实现缺失。

---

_Verified: 2026-05-10T17:04:36Z_  
_Verifier: the agent (gsd-verifier)_
