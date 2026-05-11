---
phase: 06-chrome-end-to-end-acceptance
verified: 2026-05-11T10:13:19Z
status: gaps_found
score: 7/9 must-haves verified
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 4/9
  gaps_closed:
    - "页面、编排脚本和 C 示例共享 runId，旧的信令 run 隔离 blocker 已修复。"
    - "C WebSocket client 已改为固定缓冲的分片读取状态机，旧的单次 recv 假设 blocker 已修复。"
    - "C runtime 已有成功状态机、准确 timeout layer、media executor 亲和和严格 full E2E pass 判定。"
    - "RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON 分支已实现 OpenSSL DTLS / libsrtp backend vtable，不再是简单 unsupported stub。"
  gaps_remaining:
    - "ACC-01 secure full E2E 和 VLC/ffplay 人工播放批准仍未完成。"
  regressions: []
gaps:
  - truth: "ACC-01：用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收"
    status: failed
    reason: "06-10 的 secure ON 配置因本机缺少可选 OpenSSL/libsrtp 开发依赖被 CMake gate 阻断；未运行 secure full E2E，未生成 full-secure 媒体文件，VLC/ffplay 人工批准未执行。"
    artifacts:
      - path: ".planning/phases/06-chrome-end-to-end-acceptance/06-UAT.md"
        issue: "记录了 environment_blocker，summary 为 pass:false/layer:dtls/reason:optional_security_backend_disabled，VLC/ffplay 结果为未执行。"
      - path: ".planning/REQUIREMENTS.md"
        issue: "ACC-01 仍为未勾选，需求追踪标记为部分完成/待人工验收。"
      - path: "examples/chrome_e2e/out/full-secure"
        issue: "当前没有 secure full E2E 输出目录和非空 received-opus.packets / received-h264.264 证据。"
    missing:
      - "在具备可选 OpenSSL/libsrtp 开发依赖的环境中配置并构建 RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON。"
      - "运行 full secure E2E，记录 pass:true、layer:none、received-opus.packets 和 received-h264.264 均非空的 summary。"
      - "使用 VLC/ffplay 完成人工音视频播放批准，并在 06-UAT.md 记录 VLC approved、视频结果和音频结果。"
human_verification:
  - test: "secure full E2E 环境验收"
    expected: "安装可选 OpenSSL/libsrtp 开发依赖后，RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON 配置和构建通过，full E2E 输出 pass:true/layer:none 且媒体文件非空。"
    why_human: "当前机器缺少可选系统开发依赖，自动验证只能确认 CMake gate 正确阻断。"
  - test: "VLC/ffplay 媒体播放批准"
    expected: "received-h264.264 和重封装后的 received-opus.packets 可播放，06-UAT.md 记录 VLC approved、视频结果：通过、音频结果：通过。"
    why_human: "项目首版不引入播放器/解码器自动依赖，媒体内容质量由人工确认。"
  - test: "Chrome 页面视觉可读性"
    expected: "双视频、七阶段状态条和 diagnostics 在目标 Chrome 视口下可读且不遮挡。"
    why_human: "视觉布局质量不能仅靠源码 grep 和 headless smoke 判定。"
---

# Phase 6: Chrome 端到端验收 Verification Report

**Phase Goal:** 把所有层集成成可验收的 `PeerConnection`，通过本地 Chrome 页面和信令示例完成 1v1 音视频通话。  
**Verified:** 2026-05-11T10:13:19Z  
**Status:** gaps_found  
**Re-verification:** 是，复核既有 `06-VERIFICATION.md` gaps 与 06-07..06-10 gap closure。

## Goal Achievement

第 6 阶段的 harness、示例层、默认 gate 和自动化 smoke 已经实质完成，旧 verification 中的 runId、WebSocket 分片、C runtime 成功状态机、media executor 亲和和 optional backend unsupported blocker 均已在源码中修复。

但阶段目标中的最终验收仍未达成：`ACC-01` 要求本地 Chrome 页面和信令示例完成 1v1 音视频通话验收。当前机器缺少可选 OpenSSL/libsrtp 开发依赖，secure full E2E 未运行，VLC/ffplay 人工媒体批准未执行。因此本阶段不能标记通过。

### Observable Truths

| # | Truth | Status | Evidence |
|---|---|---|---|
| 1 | 自动化测试覆盖 SDP/JSEP、ICE/STUN、DTLS/SRTP、RTP/RTCP、固定内存和关键错误路径 | VERIFIED | `ctest --test-dir build --output-on-failure` 通过；`run_e2e.mjs` 覆盖 dry-run/page/c/media/security smoke 和七层 failureLayers。 |
| 2 | 本地 Chrome 页面可以作为发起方交换 SDP 和 trickle ICE candidate | VERIFIED | `page/app.js` 使用 `RTCPeerConnection`、synthetic media、offer/candidate 信令；`run_e2e.mjs` 用同一 `runId` 打开页面；page smoke 通过，`offerCreated:true`、`permissionRequests:0`。 |
| 3 | 最小信令示例可以完成本地页面与 C 示例进程之间的消息交换 | VERIFIED | `signaling.mjs` 按 runId 广播 JSON；C 示例支持 `--run-id`，hello/answer/candidate/status 使用 escaped run id；`ws_recv_text` 有固定缓冲分片状态机。实际 secure full E2E 因环境 gate 未运行。 |
| 4 | 用户可以完成 1v1 音视频通话，并观察到 ICE、DTLS、SRTP、RTP/RTCP 和媒体事件 | FAILED | 默认 full run 实测仍按预期停在 `dtls/optional_security_backend_disabled`；secure ON 配置被缺依赖阻断；没有 pass:true/layer:none 和人工媒体播放证据。 |
| 5 | 端到端失败时 trace、计数器和错误事件足以定位失败阶段 | VERIFIED | security gate smoke 输出 `layer:"dtls"`、`reason:"optional_security_backend_disabled"`；脚本固定 `signaling/ice/dtls/srtp/rtp/rtcp/media_file` failureLayers。 |
| 6 | Chrome 页面使用 synthetic canvas/Web Audio，不请求摄像头/麦克风权限 | VERIFIED | `app.js` 使用 `canvas.captureStream(25)` 和 `createMediaStreamDestination()`；page smoke 记录 `permissionRequests:0`。 |
| 7 | 信令服务只转发 SDP/candidate/status/summary/error 等 JSON，不代理媒体 payload | VERIFIED | `signaling.mjs` 的 `allowedTypes` 和 `forbiddenFields` 拒绝 media/payload/binary，binary frame 也被拒绝。 |
| 8 | 自动化不宣称替代 VLC 人工媒体播放验收 | VERIFIED | `run_e2e.mjs` 始终保留 `manual_vlc_required:true`/`manual_vlc_pending`；`06-UAT.md` 明确 ACC-01 待人工验收。 |
| 9 | ACC-01：用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收 | FAILED | `.planning/REQUIREMENTS.md` 中 ACC-01 未勾选；`06-UAT.md` 记录 secure ON 环境阻断和 VLC/ffplay 未执行。 |

**Score:** 7/9 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| `examples/chrome_e2e/page/index.html` | Chrome E2E 验收页面 | VERIFIED | 双视频、七阶段状态条和 diagnostics DOM 存在；page smoke 通过。 |
| `examples/chrome_e2e/page/app.js` | Chrome 主叫、synthetic media、WebSocket 信令 | VERIFIED | 从 URL 读取 `runId`，生成 synthetic audio/video、offer 和 candidate。 |
| `examples/chrome_e2e/signaling.mjs` | JSON-only WebSocket 信令服务 | VERIFIED | runId 隔离广播、allowedTypes 和 forbiddenFields 实现存在。 |
| `examples/chrome_e2e/run_e2e.mjs` | E2E 编排脚本和 failure layering | VERIFIED | full run 统一 runId，pass 判定要求 C summary、退出码和非空媒体文件；默认 OFF 不误报成功。 |
| `examples/chrome_e2e/rtc_chrome_e2e.c` | C 侧 Chrome E2E 示例进程 | VERIFIED | `--run-id`、WebSocket 分片读取、成功状态机、media executor 发送和 JSONL summary 均存在；secure full E2E 未能在当前环境运行。 |
| `examples/chrome_e2e/security_backend_chrome.c` | 可选真实安全 backend | VERIFIED | ON 分支实现 OpenSSL DTLS / libsrtp vtable，OFF 分支返回 unsupported；ON 构建因缺依赖被 CMake gate 阻断。 |
| `examples/chrome_e2e/media_samples.c` | H264/Ogg Opus 样本读取 | VERIFIED | media smoke 生成非空 dry-run 产物，parser tests 通过。 |
| `.planning/phases/06-chrome-end-to-end-acceptance/06-UAT.md` | UAT 与人工 gate 记录 | PARTIAL | 自动化和环境 blocker 记录完整；缺 secure full E2E 成功 summary 与 VLC approved。 |

### Key Link Verification

| From | To | Via | Status | Details |
|---|---|---|---|---|
| `run_e2e.mjs` | Chrome page + C example | shared `runId` | WIRED | `randomUUID()` 同时传给页面 URL 和 C 示例 `--run-id`。 |
| Chrome page | Signaling server | WebSocket `hello`/`offer`/`candidate` | WIRED | 页面发送同一 runId 的 JSON；page smoke 验证 offer 生成。 |
| Signaling server | C example | same-run broadcast | WIRED | 服务只向同一 runId peers 广播；C 示例 hello/answer/candidate/status 使用 options run id。 |
| C example | PeerConnection JSEP | `setRemoteDescription`、`createAnswer`、`setLocalDescription` | WIRED | `handle_offer()` 调用链存在，answer 发送失败会阻断 `answer_sent`。 |
| C example | WebSocket transport | fixed buffer frame reader/writer | WIRED | `ws_recv_text()` 可跨 EAGAIN 保留进度；`ws_send_text()` 循环处理短写。 |
| C example | Security backend | `rtc_chrome_e2e_configure_security_backend` | PARTIAL | ON 分支填充 backend vtable；当前环境缺依赖，不能运行 secure full E2E。 |
| C example | Media send API | `rtc_peer_connection_send_media_frame` | WIRED | 发送前切到 `RTC_EXECUTOR_MEDIA`，检查返回值并记录 `send_media_frame_failed`。 |
| Full E2E automation | ACC-01 acceptance | summary + media files + VLC gate | NOT_WIRED | 没有 secure full E2E 成功 summary，也没有人工媒体批准。 |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|---|---|---|---|---|
| `page/app.js` | `state.runId` | URL `runId` 或 `crypto.randomUUID()` | 是，编排脚本注入同一 runId | FLOWING |
| `rtc_chrome_e2e.c` | `options->run_id` | CLI `--run-id` | 是，hello/answer/candidate/status 统一使用 | FLOWING |
| `rtc_chrome_e2e.c` | `offer_received/answer_sent/ice_connected/srtp_ready/media_files_ready` | WebSocket、observer 和 media callbacks | 是，runtime 用状态位决定 success/failure layer | FLOWING |
| `security_backend_chrome.c` | `config->security_backend` | ON 分支 `chrome_security_config` | 实现存在；当前环境无法构建运行 | ENV-BLOCKED |
| `run_e2e.mjs` | `summary.pass` | failure layer、C exit code、C summary、media file bytes | 是，默认 OFF 和空媒体文件不能通过 | FLOWING |
| `06-UAT.md` | `VLC approved` | 人工记录 | 否，未执行 | DISCONNECTED |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| C 示例可构建 | `cmake --build build --target rtc_chrome_e2e` | target 构建成功 | PASS |
| deterministic tests | `ctest --test-dir build --output-on-failure` | 1/1 tests passed | PASS |
| dry-run preflight | `npm run e2e:chrome:dry-run` | `ok:true`, `layer:none` | PASS |
| page smoke | `npm run e2e:chrome:page-smoke` | `offerCreated:true`, `permissionRequests:0` | PASS |
| C example smoke | `npm run e2e:chrome:c-smoke` | dry-run JSONL 通过 | PASS |
| media-file smoke | `npm run e2e:chrome:media-smoke` | dry-run 音视频产物非空 | PASS |
| security gate smoke | `npm run e2e:chrome:security-gate` | `dtls/optional_security_backend_disabled` | PASS |
| default full E2E | `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 3000 --output-dir examples/chrome_e2e/out/verification-full-default` | `pass:false`, `layer:"dtls"`, `reason:"optional_security_backend_disabled"` | PASS for default gate / FAIL for ACC-01 |
| secure ON configure | `cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` | fails with optional dependency install message | ENV-BLOCKED |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|---|---|---|---|---|
| TST-01 | 06-05, 06-06, 06-10 | 项目包含 SDP/JSEP、ICE/STUN、RTP/RTCP、固定内存和错误路径的自动化测试 | SATISFIED | CTest 和 Chrome E2E smoke 通过；failure layering 覆盖七层。 |
| EXM-01 | 06-01, 06-10 | 本地 Chrome 页面用于发起或接听 1v1 音视频通话 | SATISFIED | 页面存在，synthetic media 与 page smoke 通过；本阶段实现 Chrome 主叫路径。 |
| EXM-02 | 06-01, 06-02, 06-07, 06-10 | 最小信令示例用于交换 SDP 和 trickle ICE candidate | SATISFIED | 信令服务、C WebSocket client、shared runId、answer/candidate 发送路径和分片读取均已接通；secure full E2E 因环境 gate 未跑通。 |
| ACC-01 | 06-02..06-10 | 用户可以通过本地 Chrome 页面和信令示例完成 1v1 音视频通话验收 | BLOCKED | secure ON 配置缺依赖，full-secure 媒体文件不存在，VLC/ffplay 未批准。 |

没有后续阶段可承接 `ACC-01`，因此该 gap 不能作为 deferred 过滤。

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---:|---|---|---|
| `.planning/phases/06-chrome-end-to-end-acceptance/06-UAT.md` | 68 | `environment_blocker` / media files bytes 0 | BLOCKER | 证明 ACC-01 仍未完成；这是正确保留的 gate，不是代码 stub。 |
| `examples/chrome_e2e/README.md` | 57 | `--manual-security-ok` | INFO | 该开关只输出 override 诊断，不改变 `pass`；源码复核未发现误报通过路径。 |
| `examples/chrome_e2e/signaling.mjs` | 49 | `return null` | INFO | 静态路径拒绝的正常控制流，不是 stub。 |

### Human Verification Required

#### 1. secure full E2E 环境验收

**Test:** 安装可选 OpenSSL/libsrtp 开发依赖后运行 `cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON`、构建 `rtc_chrome_e2e`，再运行 full E2E。  
**Expected:** compact summary 包含 `pass:true`、`layer:"none"`、`manual_vlc_required:true`，且 `received-opus.packets` 和 `received-h264.264` 均存在且字节数大于 0。  
**Why human:** 当前环境缺少可选系统依赖，自动验证只能确认 gate 正确阻断。

#### 2. VLC/ffplay 媒体播放批准

**Test:** 播放 secure full E2E 输出的 H264 文件，并按 UAT/README 说明重封装或播放 Opus packets。  
**Expected:** 视频和音频都可播放，`06-UAT.md` 记录 `VLC approved`、命令、summary、`视频结果：通过` 和 `音频结果：通过`。  
**Why human:** 项目不引入自动播放器/解码器依赖，媒体质量由人工确认。

#### 3. Chrome 页面视觉检查

**Test:** 在本地 Chrome 页面检查双视频、七阶段状态条和 diagnostics。  
**Expected:** 状态信息可读，不遮挡视频或操作区，且无摄像头/麦克风权限提示。  
**Why human:** 视觉可读性需要人工确认。

### Gaps Summary

复验结论：06-07..06-09 已经关闭旧 verification 中的主要工程 blocker，06-10 也正确没有夸大验收结果。当前唯一 blocking gap 是 `ACC-01`：缺少 secure full E2E 成功运行和 VLC/ffplay 人工媒体批准。阶段目标要求“完成 1v1 音视频通话验收”，因此阶段总体仍为 `gaps_found`，不能进入“已通过”状态。

---

_Verified: 2026-05-11T10:13:19Z_  
_Verifier: the agent (gsd-verifier)_
