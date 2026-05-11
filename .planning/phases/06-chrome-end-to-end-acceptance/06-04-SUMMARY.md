---
phase: 06-chrome-end-to-end-acceptance
plan: 04
subsystem: examples
tags: [chrome-e2e, dtls, srtp, optional-security, diagnostics, cmake]
requires:
  - phase: 06-chrome-end-to-end-acceptance
    plan: 02
    provides: C 示例运行时、WebSocket/UDP 示例层 I/O 和 JSONL observer
  - phase: 06-chrome-end-to-end-acceptance
    plan: 03
    provides: 样本解析、按节奏发送、接收媒体落盘和 media-file smoke
provides:
  - 默认关闭的 Chrome E2E 可选 DTLS/SRTP backend 构建 gate
  - `rtc_chrome_e2e_configure_security_backend()` 示例层适配入口
  - 默认 OFF 时 `dtls/optional_security_backend_disabled` JSONL summary
  - DTLS/SRTP/RTP/RTCP 安全失败分层诊断和 `--security-gate-smoke`
affects: [phase-06, examples, e2e, acceptance, security]
tech-stack:
  added: []
  patterns:
    - 默认构建不查找也不链接真实安全依赖
    - 可选安全依赖通过 CMake gate 和 README 许可证说明进入
    - observer error/trace 在示例层映射为 E2E failure layer
key-files:
  created:
    - cmake/FindRtcOptionalSecurity.cmake
    - examples/chrome_e2e/security_backend_chrome.c
    - examples/chrome_e2e/security_backend_chrome.h
    - .planning/phases/06-chrome-end-to-end-acceptance/06-04-SUMMARY.md
  modified:
    - CMakeLists.txt
    - examples/chrome_e2e/rtc_chrome_e2e.c
    - examples/chrome_e2e/run_e2e.mjs
    - examples/chrome_e2e/README.md
key-decisions:
  - "RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY 默认 OFF；默认构建不强制 OpenSSL/libsrtp。"
  - "06-04 只建立真实安全 backend gate 和失败分层，不声明 ACC-01 已完成。"
  - "fingerprint/handshake 映射 dtls，key_export/srtp_init 映射 srtp，RTP protect/unprotect 映射 rtp，RTCP/SRTCP protect/unprotect 映射 rtcp。"
patterns-established:
  - "full E2E 在默认 OFF 时先于 WebSocket/UDP I/O 停在 `optional_security_backend_disabled`。"
  - "`run_e2e.mjs --security-gate-smoke` 验证默认 gate，不要求真实 Chrome 互通。"
requirements-completed: []
duration: 20min
completed: 2026-05-11
---

# Phase 6 Plan 04: 可选 Chrome 安全 backend Gate Summary

**Chrome 真实 DTLS/SRTP 互通风险已前置为默认关闭的可选安全 backend gate，并通过 JSONL 明确分层到 DTLS/SRTP/RTP/RTCP。**

## Performance

- **Duration:** 20min
- **Started:** 2026-05-11T06:14:30Z
- **Completed:** 2026-05-11T06:34:19Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- 新增 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` CMake option，默认 `OFF`；默认构建不查找、不链接 OpenSSL、libsrtp 或其他系统安全依赖。
- 新增 `cmake/FindRtcOptionalSecurity.cmake`，启用 option 时查找 OpenSSL/libsrtp；缺失时 CMake 失败信息包含计划要求的安装或关闭 gate 指引。
- 新增 `security_backend_chrome.c/h`，暴露 `rtc_chrome_e2e_configure_security_backend(rtc_peer_connection_config_t *config)` 示例层入口；默认未启用时返回 `RTC_STATUS_UNSUPPORTED`。
- `rtc_chrome_e2e` 非 dry-run 在默认 OFF 时先于 WebSocket/UDP I/O 输出 `summary.pass=false`、`layer="dtls"`、`reason="optional_security_backend_disabled"`。
- C 示例 `on_error` 与 `on_trace` 将 security detail/operation 映射到 `dtls`、`srtp`、`rtp`、`rtcp`，并保留 `detail_code` 与 detail 名称。
- `run_e2e.mjs --security-gate-smoke` 验证默认安全 gate，不要求真实 Chrome 互通。
- README 中文记录 OpenSSL Apache-2.0、libsrtp BSD-3-Clause、禁止 GPL/LGPL，以及只有启用并配置可选 backend 后 full E2E 才可能达到 `dtls.connected` 和 `srtp.ready`。

## Task Commits

1. **Task 1: 增加可选 Chrome 安全 backend 构建开关和许可证 gate** - `7f37d75` (`feat`)
2. **Task 2 RED: 增加失败的 security gate smoke** - `3000d57` (`test`)
3. **Task 2 GREEN: 将安全 backend 失败映射到 JSONL 分层诊断** - `176c601` (`feat`)

## Files Created/Modified

- `CMakeLists.txt` - 新增默认 OFF 的 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` option，并把 security backend 源文件接入 `rtc_chrome_e2e`。
- `cmake/FindRtcOptionalSecurity.cmake` - 可选 OpenSSL/libsrtp 查找和缺失依赖 fatal gate。
- `examples/chrome_e2e/security_backend_chrome.h` - Chrome E2E security backend 配置入口声明。
- `examples/chrome_e2e/security_backend_chrome.c` - 默认未启用 gate 返回 `RTC_STATUS_UNSUPPORTED`，避免误报真实互通。
- `examples/chrome_e2e/rtc_chrome_e2e.c` - full E2E 默认 OFF summary、DTLS session storage limit、security error/trace 分层映射。
- `examples/chrome_e2e/run_e2e.mjs` - 新增 `--security-gate-smoke`。
- `examples/chrome_e2e/README.md` - 中文说明安全 gate、许可证、禁止 GPL/LGPL 和真实 E2E 前置条件。

## Decisions Made

- 默认 OFF gate 在 `run_runtime()` 最前面执行，早于 UDP/WebSocket；这样没有可选安全 backend 时不会被信令或 socket 错误掩盖。
- 06-04 不把 `ACC-01` 标记完成；本计划只交付真实安全 backend 的风险 gate 和失败分层，full E2E 编排与人工验收仍在 06-05/06-06。
- 当前 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 仍是 manual adapter gate：CMake 先验证宽松许可依赖存在，真实 OpenSSL/libsrtp vtable 适配不在默认构建路径中伪装完成。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] 将默认安全 gate 前移到 WebSocket/UDP 之前**
- **Found during:** Task 2 RED
- **Issue:** Task 1 初版在 WebSocket 连接后才检查可选安全 backend；没有信令服务时会先失败为 `signaling`，掩盖真实 DTLS/SRTP backend 缺失。
- **Fix:** 在 `run_runtime()` 初始化配置后立即调用 `rtc_chrome_e2e_configure_security_backend()`；默认 OFF 直接输出 `dtls/optional_security_backend_disabled`。
- **Files modified:** `examples/chrome_e2e/rtc_chrome_e2e.c`
- **Verification:** `node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke`
- **Committed in:** `176c601`

---

**Total deviations:** 1 auto-fixed（Rule 2 missing critical functionality）
**Impact on plan:** 该修复强化了计划目标：安全 backend 缺失不会被信令层错误掩盖，也不会伪装成 E2E 成功。

## Issues Encountered

- Task 2 TDD RED 按预期失败：`--security-gate-smoke` 没有停在 `dtls/optional_security_backend_disabled`。GREEN 提交前移 gate 并补齐分层映射后通过。

## Verification

- `cmake -S . -B build && cmake --build build --target rtc_chrome_e2e`：通过。
- `node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke`：通过，summary 为 `pass=false`、`layer="dtls"`、`reason="optional_security_backend_disabled"`。
- `ctest --test-dir build --output-on-failure`：通过，1/1 tests passed。
- 兼容复验：`node examples/chrome_e2e/run_e2e.mjs --c-example-smoke` 与 `--media-file-smoke` 均通过。
- grep 验收项：`RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY`、`FindRtcOptionalSecurity`、`optional_security_backend_disabled`、`rtc_chrome_e2e_configure_security_backend`、`OpenSSL.*Apache-2.0`、`libsrtp.*BSD-3-Clause`、`GPL/LGPL`、`fingerprint`、`key_export`、`srtp`、`rtcp`、`--security-gate-smoke` 均命中。

## TDD Gate Compliance

- RED commit: `3000d57`。
- GREEN commit: `176c601`。

## Known Stubs

- `examples/chrome_e2e/security_backend_chrome.c` 在默认 OFF 和当前 ON adapter gate 下返回 `RTC_STATUS_UNSUPPORTED`。这是本计划的显式 manual gate，不阻塞 06-04 目标；真实 OpenSSL/libsrtp vtable 适配仍未声明完成，后续 full E2E 必须在启用并成功配置可选 backend 后才可宣称 `dtls.connected` / `srtp.ready`。

## Auth Gates

None.

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: optional-security-deps | `cmake/FindRtcOptionalSecurity.cmake` | 新增可选 OpenSSL/libsrtp 依赖探测；默认 OFF 不进入默认构建，README 明确 Apache-2.0/BSD-3-Clause 与 GPL/LGPL 禁止。 |

## User Setup Required

None for default smoke/build. 真实 Chrome DTLS/SRTP full E2E 需要用户显式启用 `-DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 并提供宽松许可安全 backend 依赖；当前默认路径会清晰停在 manual gate。

## Next Phase Readiness

06-05 可以把 `--security-gate-smoke` 作为 full E2E 前置检查：默认 OFF 时预期失败层为 `dtls`，启用可选安全 backend 后再继续检查 `dtls.connected`、`srtp.ready`、RTP/RTCP counters 和媒体文件。

## Self-Check: PASSED

- 文件存在性检查通过：`06-04-SUMMARY.md`、`FindRtcOptionalSecurity.cmake`、`security_backend_chrome.c`、`security_backend_chrome.h`、`rtc_chrome_e2e.c`、`run_e2e.mjs`、`README.md` 均存在。
- 提交存在性检查通过：`7f37d75`、`3000d57`、`176c601` 均可在 git log 中找到。
- 自动化复验通过：默认构建、`rtc_chrome_e2e` target、`--security-gate-smoke` 和 CTest 均成功。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
