---
phase: 06-chrome-end-to-end-acceptance
verified: 2026-05-11T07:00:43Z
status: gaps_found
score: 4/9 must-haves verified
overrides_applied: 0
gaps:
  - truth: "用户可以完成 1v1 音视频通话，并观察到 ICE、DTLS、SRTP、RTP/RTCP 和媒体事件"
    status: failed
    reason: "真实 Chrome E2E 路径无法成功：默认 full run 停在 dtls/optional_security_backend_disabled；启用可选安全宏后 security_backend_chrome.c 仍返回 RTC_STATUS_UNSUPPORTED；C runtime 没有成功退出条件；媒体发送还在错误 executor 上调用。"
    artifacts:
      - path: "examples/chrome_e2e/security_backend_chrome.c"
        issue: "RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY 定义时仍直接返回 RTC_STATUS_UNSUPPORTED，没有填充 config->security_backend。"
      - path: "examples/chrome_e2e/rtc_chrome_e2e.c"
        issue: "run_runtime 超时后无条件写 pass:false/layer:signaling/reason:no offer received；send_sample_frame 使用 RTC_EXECUTOR_SIGNALING 调用 media API。"
      - path: "examples/chrome_e2e/run_e2e.mjs"
        issue: "full run 的实测 summary 为 pass:false/layer:dtls/reason:optional_security_backend_disabled。"
    missing:
      - "实现真实可选 DTLS/SRTP backend，或删除声称可启用成功的路径。"
      - "为 C 示例 runtime 增加 offer/answer、ICE、SRTP、RTP/RTCP、media file 的成功状态机和 passing summary。"
      - "在 RTC_EXECUTOR_MEDIA 上调用 rtc_peer_connection_send_media_frame，并检查返回值。"
  - truth: "本地 Chrome 页面与 C 示例可以通过最小信令示例完成 SDP 和 trickle ICE candidate 交换"
    status: failed
    reason: "信令服务按 runId 隔离广播；页面使用随机 runId，C 示例硬编码 c-example，run_e2e.mjs 未把同一 runId 传给二者，offer/answer 不会互达。"
    artifacts:
      - path: "examples/chrome_e2e/page/app.js"
        issue: "state.runId 初始化为 crypto.randomUUID()，没有从 URL 或编排脚本读取共享 runId。"
      - path: "examples/chrome_e2e/rtc_chrome_e2e.c"
        issue: "E2E_RUN_ID 硬编码为 c-example，CLI 无 --run-id。"
      - path: "examples/chrome_e2e/signaling.mjs"
        issue: "只向同一 runId 的 peers 广播，隔离逻辑本身正确，但当前两端未加入同一 run。"
    missing:
      - "run_e2e.mjs 生成 runId，并同时传给页面 URL 与 C 示例 --run-id。"
      - "C 示例所有 hello/answer/candidate/status/summary 消息使用 options.run_id。"
  - truth: "最小信令示例在真实 TCP/WebSocket 条件下能稳定交换 Chrome SDP"
    status: failed
    reason: "C 示例 WebSocket client 假设一次 recv 读完整 header/payload；非阻塞 TCP 可分片，较大的 Chrome SDP frame 可能被误判失败。"
    artifacts:
      - path: "examples/chrome_e2e/rtc_chrome_e2e.c"
        issue: "ws_recv_text 对 header、extended length 和 payload 都使用单次 recv 且要求长度完全相等，没有持久输入缓冲或 frame 状态机。"
    missing:
      - "实现可跨 EAGAIN/EWOULDBLOCK 保留进度的 WebSocket frame 读取状态机，至少具备 read-exact 语义。"
  - truth: "ACC-01：用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收"
    status: failed
    reason: "需求文件、ROADMAP、STATE 与实测命令都表明 ACC-01 仍未关闭；当前没有真实 Chrome DTLS/SRTP full E2E 成功记录，也没有 VLC/ffplay 人工播放批准记录。"
    artifacts:
      - path: ".planning/REQUIREMENTS.md"
        issue: "ACC-01 标记为部分完成/待人工验收。"
      - path: ".planning/phases/06-chrome-end-to-end-acceptance/06-UAT.md"
        issue: "明确 full E2E 与 VLC 人工播放尚未完成。"
      - path: "examples/chrome_e2e/run_e2e.mjs"
        issue: "summary 始终包含 manual_vlc_required:true；full run 实测未生成 media_files。"
    missing:
      - "启用真实安全 backend 后 full E2E 通过，输出非空 received-opus.packets 与 received-h264.264。"
      - "记录 VLC/ffplay 人工播放通过结果，用于关闭 ACC-01。"
human_verification:
  - test: "VLC/ffplay 媒体播放验收"
    expected: "启用真实安全 backend 后 full E2E 通过，输出音频和视频文件均可播放，并记录人工批准。"
    why_human: "首版不引入解码器/播放器自动依赖，媒体内容质量由人工确认。"
  - test: "Chrome 页面视觉可读性"
    expected: "双视频、七阶段状态条和 diagnostics 在目标浏览器尺寸下可读且不遮挡。"
    why_human: "视觉布局质量不能仅靠 grep 或 smoke 判定。"
---

# Phase 6: Chrome 端到端验收 Verification Report

**Phase Goal:** 把所有层集成成可验收的 `PeerConnection`，通过本地 Chrome 页面和信令示例完成 1v1 音视频通话。  
**Verified:** 2026-05-11T07:00:43Z  
**Status:** gaps_found  
**Re-verification:** 否，初次验证。未发现既有 `06-VERIFICATION.md`。

## Goal Achievement

第 6 阶段没有达到阶段目标。页面、信令服务、C 示例、样本解析、JSONL 和自动化脚本都存在，但真实 Chrome↔C 1v1 音视频验收链路被多个 blocker 阻断。`06-REVIEW.md` 中的 5 个 blocker 经源码复核仍成立。

`gsd-sdk` 在当前 shell 中不可用（`gsd-sdk: command not found`），因此 ROADMAP 成功标准和 PLAN must-have 通过直接读取 `.planning/ROADMAP.md`、`.planning/REQUIREMENTS.md` 和 Phase 06 PLAN frontmatter 建立。

### Observable Truths

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | 自动化测试覆盖 SDP/JSEP、ICE/STUN、DTLS/SRTP、RTP/RTCP、固定内存和关键错误路径 | VERIFIED | `ctest --test-dir build --output-on-failure` 通过；`run_e2e.mjs` 提供 dry-run/page/c-example/media/security smoke；`TST-01` 在需求追踪中按自动化范围关闭。 |
| 2 | 本地 Chrome 页面可以作为发起方交换 SDP 和 trickle ICE candidate | FAILED | 页面能生成 offer，但页面 `runId` 为随机 UUID，C 示例 hardcode `E2E_RUN_ID "c-example"`，信令服务只转发同一 runId 内消息。实际无法完成 Chrome↔C offer/answer 交换。 |
| 3 | 最小信令示例可以完成页面与 C 示例进程之间的消息交换 | FAILED | runId 不一致阻断消息互达；C WebSocket client 的 `ws_recv_text` 单次 `recv` 假设不满足真实 TCP/WebSocket。 |
| 4 | 用户可以完成 1v1 音视频通话，并观察到 ICE、DTLS、SRTP、RTP/RTCP 和媒体事件 | FAILED | full run 实测输出 `pass:false`、`layer:"dtls"`、`reason:"optional_security_backend_disabled"`；可选安全 backend 即使启用也返回 unsupported；runtime 没有成功 summary 路径。 |
| 5 | 端到端失败时 trace、计数器和错误事件足以定位失败阶段 | VERIFIED | 默认安全 gate 能稳定报告 `dtls/optional_security_backend_disabled`；脚本 failureLayers 覆盖 signaling/ice/dtls/srtp/rtp/rtcp/media_file。 |
| 6 | Chrome 页面使用 synthetic canvas/Web Audio，不请求摄像头/麦克风权限 | VERIFIED | `app.js` 使用 `canvas.captureStream(25)` 和 `createMediaStreamDestination()`；`--page-smoke` 实测 `permissionRequests:0`。 |
| 7 | 信令服务只转发 SDP/candidate/status/summary/error 等 JSON，不代理媒体 payload | VERIFIED | `signaling.mjs` 的 `allowedTypes` 和 `forbiddenFields` 拒绝 `media`、`payload`、`binary` 字段，并拒绝 binary frame。 |
| 8 | 自动化不宣称替代 VLC 人工媒体播放验收 | VERIFIED | README/UAT/run_e2e 均保留 `manual_vlc_required:true`；`06-UAT.md` 明确 ACC-01 待人工验收。 |
| 9 | ACC-01：用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收 | FAILED | `.planning/REQUIREMENTS.md` 仍标记 ACC-01 为部分完成/待人工验收；无真实安全 backend full E2E 成功记录和 VLC/ffplay 批准记录。 |

**Score:** 4/9 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| `examples/chrome_e2e/page/index.html` | Chrome E2E 验收页面 | VERIFIED | DOM testid、双视频、七阶段状态条存在；page smoke 通过。 |
| `examples/chrome_e2e/page/app.js` | Chrome 主叫、synthetic media、WebSocket 信令 | PARTIAL | synthetic media 正确；但 runId 不可由编排脚本共享，阻断 C 示例信令互通。 |
| `examples/chrome_e2e/signaling.mjs` | JSON-only WebSocket 信令服务 | VERIFIED | runId 隔离和 forbidden fields 实现存在；问题在调用方未共享 runId。 |
| `examples/chrome_e2e/run_e2e.mjs` | E2E 编排脚本和 failure layering | PARTIAL | 能编排并报告失败层；但 full run 当前失败，且没有把共享 runId 传给页面/C 示例。 |
| `examples/chrome_e2e/rtc_chrome_e2e.c` | C 侧 Chrome E2E 示例进程 | FAILED | WebSocket 分片不健壮、runId hardcode、media executor 错误、runtime 无成功条件。 |
| `examples/chrome_e2e/security_backend_chrome.c` | 可选真实安全 backend gate | FAILED | 宏开启时仍返回 `RTC_STATUS_UNSUPPORTED`，没有任何 backend vtable。 |
| `examples/chrome_e2e/media_samples.c` | H264/Ogg Opus 样本读取 | VERIFIED | media-file smoke 读取样本并生成 dry-run 产物；该 smoke 不代表真实 Chrome 接收媒体。 |
| `tests/test_chrome_e2e_samples.c` | 样本解析 deterministic tests | VERIFIED | `ctest` 通过。 |
| `.planning/phases/06-chrome-end-to-end-acceptance/06-UAT.md` | 自动化与人工验收清单 | VERIFIED | 明确 full E2E/VLC 尚未完成，未夸大 ACC-01。 |

### Key Link Verification

| From | To | Via | Status | Details |
|---|---|---|---|---|
| Chrome page | Signaling server | WebSocket `hello`/`offer`/`candidate` | PARTIAL | 页面发送 JSON，但 runId 随机，C 示例不在同一 run。 |
| Signaling server | C example | Same-run broadcast | NOT_WIRED | `signaling.mjs` 只广播同一 runId；页面和 C 示例 runId 不同。 |
| C example | PeerConnection JSEP | `setRemoteDescription`、`createAnswer`、`setLocalDescription` | PARTIAL | 源码调用存在，但真实 offer 因 runId 不一致不会到达。 |
| C example | UDP/datagram API | `receive_datagram` 和 `observer.on_datagram` | PARTIAL | 源码接入存在；真实端到端未走到成功 ICE/媒体路径。 |
| C example | Security backend | `rtc_chrome_e2e_configure_security_backend` | NOT_WIRED | 配置函数始终 unsupported；没有可用真实 backend。 |
| C example | Media send API | `rtc_peer_connection_send_media_frame` | NOT_WIRED | 在 `RTC_EXECUTOR_SIGNALING` 上调用，违反 `RTC_EXECUTOR_MEDIA` 亲和契约，且返回值被丢弃。 |
| Full E2E automation | ACC-01 acceptance | compact JSON summary + media files + VLC gate | NOT_WIRED | full run 无 media files，`manual_vlc_required:true`，ACC-01 未关闭。 |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|---|---|---|---|---|
| `page/app.js` | `state.runId` | `crypto.randomUUID()` | 否，用于真实 C 示例时不共享 | HOLLOW |
| `rtc_chrome_e2e.c` | `E2E_RUN_ID` | hardcoded `"c-example"` | 否，不能由编排脚本传入 | HOLLOW |
| `security_backend_chrome.c` | `config->security_backend` | `rtc_chrome_e2e_configure_security_backend` | 否，宏开启时仍 unsupported | DISCONNECTED |
| `rtc_chrome_e2e.c` | `audio/video_bytes_received` | `on_media_frame_typed` | 仅 dry-run/回调可增；真实 full run 当前为 0 | HOLLOW |
| `run_e2e.mjs` | `summary.media_files` | 输出目录扫描 | full run 当前为空 | DISCONNECTED |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| C 示例可构建 | `cmake --build build --target rtc_chrome_e2e` | target 构建成功 | PASS |
| dry-run preflight | `node examples/chrome_e2e/run_e2e.mjs --dry-run` | `ok:true`, `layer:none` | PASS |
| page smoke | `node examples/chrome_e2e/run_e2e.mjs --page-smoke` | `offerCreated:true`, `permissionRequests:0` | PASS |
| C example smoke | `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke` | dry-run JSONL 通过 | PASS |
| media-file smoke | `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke` | dry-run 生成 1 个音频/视频样本统计 | PASS |
| security gate smoke | `node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke` | `dtls/optional_security_backend_disabled` | PASS |
| full E2E | `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 1000` | `pass:false`, `layer:"dtls"`, `reason:"optional_security_backend_disabled"`, `media_files:[]` | FAIL |
| deterministic tests | `ctest --test-dir build --output-on-failure` | 1/1 `rtc_tests` passed | PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|---|---|---|---|---|
| TST-01 | 06-05, 06-06 | 项目包含 SDP/JSEP、ICE/STUN、RTP/RTCP、固定内存和错误路径的自动化测试 | SATISFIED | `ctest` 通过；E2E smoke 和 failure layering 存在。注意 smoke 多为 preflight/source scan，不能证明 ACC-01。 |
| EXM-01 | 06-01, 06-06 | 本地 Chrome 页面用于发起或接听 1v1 音视频通话 | SATISFIED | 页面存在，synthetic media 与 page smoke 通过；当前主要覆盖 Chrome 主叫。 |
| EXM-02 | 06-01, 06-02, 06-06 | 最小信令示例用于交换 SDP 和 trickle ICE candidate | BLOCKED | 信令服务和 C client 存在，但 runId 不一致导致真实页面与 C 示例消息不互达；WebSocket 读取也不健壮。 |
| ACC-01 | 06-02..06-06 | 用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收 | BLOCKED | full run 当前失败在 `dtls/optional_security_backend_disabled`；可选 backend 无实现；无 VLC/ffplay 批准记录。 |

未发现 Phase 06 之外的新需求被本阶段遗漏；ROADMAP 只有 6 个阶段，因此本次 blocker 没有后续阶段可明确承接，不能作为 deferred 过滤。

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---:|---|---|---|
| `examples/chrome_e2e/security_backend_chrome.c` | 12 | `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` 分支仍返回 unsupported | BLOCKER | 启用依赖后仍无法进入真实 DTLS/SRTP。 |
| `examples/chrome_e2e/rtc_chrome_e2e.c` | 28 | hardcoded `E2E_RUN_ID "c-example"` | BLOCKER | 与页面随机 runId 隔离，信令不互达。 |
| `examples/chrome_e2e/page/app.js` | 2 | `crypto.randomUUID()` 作为唯一 runId 来源 | BLOCKER | 编排脚本无法保证页面与 C 示例同 run。 |
| `examples/chrome_e2e/rtc_chrome_e2e.c` | 615 | `ws_recv_text` 单次 `recv` 完整 frame 假设 | BLOCKER | 真实 Chrome SDP WebSocket frame 可被 TCP 分片，导致信令不稳定失败。 |
| `examples/chrome_e2e/rtc_chrome_e2e.c` | 1053 | media API 在 signaling executor 调用 | BLOCKER | `send_media_frame` 会亲和违规，样本媒体不会进入 RTP 发送路径。 |
| `examples/chrome_e2e/rtc_chrome_e2e.c` | 1328 | runtime loop 后无条件失败 summary | BLOCKER | 即使链路修复，仍会返回失败。 |
| `examples/chrome_e2e/run_e2e.mjs` | 364 | `--manual-security-ok` 可把 security gate 失败转为 pass | WARNING | 文档说明仅开发 smoke，但该开关容易被误用为验收通过信号。 |

### Human Verification Required

#### 1. VLC/ffplay 媒体播放验收

**Test:** 启用真实安全 backend 后运行 full E2E，用 VLC/ffplay 播放 `received-h264.264`，并将 `received-opus.packets` 重封装后播放。  
**Expected:** 音频和视频都可播放；JSONL 没有 signaling/ice/dtls/srtp/rtp/rtcp/media_file error；记录人工批准。  
**Why human:** 项目明确不引入播放器/解码器自动依赖，媒体内容质量由人工确认。

#### 2. Chrome 页面视觉检查

**Test:** 打开本地页面，检查双视频、七阶段状态条、diagnostics、failure callout 在常见桌面视口下可读且不遮挡。  
**Expected:** 状态信息可读，按钮和 diagnostics 可操作，页面不请求摄像头/麦克风权限。  
**Why human:** 视觉质量和可读性需要人工确认。

### Gaps Summary

阶段目标要求“完成 1v1 音视频通话验收”，但当前实现只完成了 harness 的外壳、preflight、dry-run 和失败分层。真实互通路径缺少可用安全 backend，信令 runId 未接通，C runtime 没有成功状态机，媒体发送违反 executor 契约，WebSocket 读取也不足以承载真实 Chrome SDP。`ACC-01` 因此必须保持未完成。

---

_Verified: 2026-05-11T07:00:43Z_  
_Verifier: the agent (gsd-verifier)_
