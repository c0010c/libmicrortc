---
phase: 06-chrome-end-to-end-acceptance
plan: 09
subsystem: chrome-e2e-security
tags: [chrome-e2e, dtls, srtp, openssl, libsrtp, optional-security]

requires:
  - phase: 06-chrome-end-to-end-acceptance
    provides: "06-08 已建立 C runtime 成功状态机、media executor 亲和和严格 full E2E pass 判定"
provides:
  - "RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON 时可配置真实 OpenSSL DTLS / libsrtp backend"
  - "默认 OFF 构建继续不查找、不链接 OpenSSL/libsrtp"
  - "DTLS/SRTP backend 失败进入具体 handshake、key export、SRTP protect/unprotect/replay 层"
affects: [06-10, ACC-01, chrome-e2e]

tech-stack:
  added: []
  patterns:
    - "OpenSSL/libsrtp 仅在 RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON 的示例 target 中包含和链接"
    - "核心库继续通过 rtc_security_backend_vtable_t 与第三方安全库隔离"
    - "示例 backend 内部拥有第三方对象，核心 session storage 仍由调用方提供"

key-files:
  created:
    - ".planning/phases/06-chrome-end-to-end-acceptance/06-09-SUMMARY.md"
  modified:
    - ".gitignore"
    - "examples/chrome_e2e/security_backend_chrome.c"
    - "examples/chrome_e2e/README.md"

key-decisions:
  - "Chrome DTLS 远端证书是自签证书，OpenSSL CA verify 不作为身份门；身份校验继续由核心 sha-256 fingerprint 比对完成。"
  - "SRTP protect/unprotect 不额外发送 backend event，避免把 RTP/RTCP 失败污染为 security/backend_event；核心 wrapper 负责 detail code 映射。"
  - "`ACC-01` 仍不标记完成；本计划只关闭 optional backend unsupported blocker，secure full E2E 和 VLC/ffplay 人工验收留给 06-10。"

patterns-established:
  - "可选示例 backend 可以在示例层动态分配 OpenSSL/libsrtp 对象，但必须在 destroy_session 释放，不改变核心固定内存边界。"
  - "默认构建 gate 必须同时验证 CMake configure/build 和 `--security-gate-smoke` 仍停在 `optional_security_backend_disabled`。"

requirements-completed: []

duration: 10min
completed: 2026-05-11
---

# Phase 06 Plan 09: Chrome 可选 DTLS/SRTP backend Summary

**Chrome E2E 示例现在具备默认关闭的真实 OpenSSL DTLS / libsrtp security backend，启用依赖后会填充 backend vtable，默认 OFF 构建仍保持无系统安全依赖。**

## Performance

- **Duration:** 约 10 min
- **Started:** 2026-05-11T09:44:23Z
- **Completed:** 2026-05-11T09:53:40Z
- **Tasks:** 3/3
- **Files modified:** 4

## Accomplishments

- 在 `examples/chrome_e2e/security_backend_chrome.c` 的 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY` 分支中新增 `chrome_security_vtable` 和 `chrome_security_config`。
- `rtc_chrome_e2e_configure_security_backend()` 在 ON 分支填充 `config->security_backend`、`session_storage_bytes = sizeof(chrome_security_session_t)` 并返回 `RTC_STATUS_OK`；OFF 分支保持 `RTC_STATUS_UNSUPPORTED`。
- `create_session` 使用调用方 storage 保存 session，同时在示例 backend 内部创建并释放 OpenSSL `SSL_CTX`、`SSL`、内存 BIO、自签证书和私钥。
- `get_local_fingerprint` / `get_peer_fingerprint` 输出 `sha-256 XX:...` 格式，供 SDP fingerprint 和核心 fingerprint verification 使用。
- `start_dtls` / `handle_dtls_datagram` 驱动 `SSL_do_handshake`，通过 backend event 输出 DTLS datagram，并在握手完成后发送 `HANDSHAKE_COMPLETE`。
- `export_keying_material` 调用 `SSL_export_keying_material` 并接受核心传入的 `EXTRACTOR-dtls_srtp` label。
- `init_srtp_context` 使用 libsrtp2 初始化 send/recv context，并按 DTLS role 切分 client/server write key 和 salt。
- `srtp_protect_rtp`、`srtp_unprotect_rtp`、`srtcp_protect`、`srtcp_unprotect` 接入 libsrtp protect/unprotect，容量不足时拒绝写入。
- README 新增 secure build 命令，并说明 OpenSSL/libsrtp 许可证、默认 OFF 无依赖和启用者安装维护责任。
- `.gitignore` 新增本地 `build/`、`build-secure/`、`node_modules/` 忽略项，避免验证输出污染工作树。

## Task Commits

1. **Task 1: 建立 OpenSSL DTLS session 和本地 fingerprint** - `b87d691` (`feat`)
2. **Task 2: 实现 DTLS handshake、key export 和事件输出** - `2f5b6ff` (`fix`)
3. **Task 3: 接入 libsrtp protect/unprotect 并保持失败分层** - `70e94cd` (`docs`)

Additional correctness/hygiene commits:

- `d15d5d0` (`chore`) - 忽略本地 verification 输出目录。
- `3ca4220` (`fix`) - 修正 create failure 释放路径，并保持 SRTP 失败由核心 wrapper 分层。
- `0cfb54c` (`docs`) - 在 backend 文件中标注 SRTP detail code 映射。

## Files Created/Modified

- `examples/chrome_e2e/security_backend_chrome.c` - 新增默认关闭的 OpenSSL DTLS / libsrtp backend 实现。
- `examples/chrome_e2e/README.md` - 新增 ON 构建命令、许可证和安装责任说明。
- `.gitignore` - 忽略本地 CMake/Node 验证输出。
- `.planning/phases/06-chrome-end-to-end-acceptance/06-09-SUMMARY.md` - 本执行摘要。

## Verification

- `cmake -S . -B build -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=OFF`：PASS。
- `cmake --build build --target rtc_chrome_e2e`：PASS。
- `node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke`：PASS，summary 仍为 `pass:false`、`layer:"dtls"`、`reason:"optional_security_backend_disabled"`。
- `rg -n "chrome_security_vtable" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "SSL_CTX_new" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "X509_digest|EVP_sha256" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "chrome_security_config\\.session_storage_bytes = sizeof\\(chrome_security_session_t\\)" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "config->security_backend = &chrome_security_config" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "SSL_do_handshake|BIO_read|BIO_write|SSL_export_keying_material|HANDSHAKE_COMPLETE" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM|RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "SSL_get_peer_certificate|SSL_get1_peer_certificate" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "RTC_SECURITY_DETAIL_HANDSHAKE_FAILED|RTC_SECURITY_DETAIL_KEY_EXPORT_FAILED" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "srtp_init|srtp_create|srtp_protect|srtp_unprotect|srtp_protect_rtcp|srtp_unprotect_rtcp" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "RTC_SECURITY_DETAIL_SRTP_INIT_FAILED|RTC_SECURITY_DETAIL_SRTP_PROTECT_FAILED|RTC_SECURITY_DETAIL_SRTP_UNPROTECT_FAILED" examples/chrome_e2e/security_backend_chrome.c`：PASS。
- `rg -n "RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON" examples/chrome_e2e/README.md`：PASS。
- `cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON`：ENV-LIMITED，本机缺少可选 OpenSSL/libsrtp 开发依赖，按 `FindRtcOptionalSecurity.cmake` 预期失败并提示安装依赖或改回 OFF；因此未运行 secure build/full E2E。

## Decisions Made

- OpenSSL verify mode 使用 `SSL_VERIFY_NONE`，因为 WebRTC DTLS 使用自签证书；真实身份校验由核心比较 SDP fingerprint 与 peer certificate fingerprint 完成。
- SRTP replay 使用 `RTC_STATUS_PROTOCOL_ERROR` 返回，让核心 wrapper 映射到 `RTC_SECURITY_DETAIL_SRTP_REPLAY_FAILED`；普通 protect/unprotect 失败返回 backend/protocol failure 后由核心映射到对应 detail。
- 本计划不修改 `FindRtcOptionalSecurity.cmake` 的 fatal gate；ON 分支缺依赖时应失败，OFF 分支不查找依赖。

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] 允许 Chrome 自签 DTLS 证书进入 fingerprint 校验路径**
- **Found during:** Task 2
- **Issue:** 初始实现使用 OpenSSL CA verify，会在 Chrome 自签 DTLS certificate 上提前失败，无法进入核心 fingerprint verification。
- **Fix:** 改为 `SSL_VERIFY_NONE`，由核心 `rtc_security_verify_peer_fingerprint()` 执行 `sha-256` fingerprint 身份校验。
- **Files modified:** `examples/chrome_e2e/security_backend_chrome.c`
- **Verification:** 默认 OFF 构建通过；Task 2 grep 覆盖 DTLS handshake、peer fingerprint 和 key export 路径。
- **Committed in:** `2f5b6ff`

**2. [Rule 1 - Bug] 修正 create_session 失败路径资源释放**
- **Found during:** Task 3
- **Issue:** OpenSSL 对象在示例 backend 内部分配，`create_session` 中途失败时核心不会持有 session 并调用 `destroy_session`，需要 backend 自行释放。
- **Fix:** 新增 `chrome_create_fail()`，失败时先发错误 event 再调用 `chrome_destroy_session()`；补充 BIO transfer 标记，避免泄漏或重复释放。
- **Files modified:** `examples/chrome_e2e/security_backend_chrome.c`
- **Verification:** 默认 OFF 构建通过。
- **Committed in:** `3ca4220`

**3. [Rule 1 - Bug] 保持 SRTP 失败分层由核心 wrapper 负责**
- **Found during:** Task 3
- **Issue:** backend protect/unprotect 如果额外发送 `RTC_SECURITY_BACKEND_EVENT_ERROR`，运行时可能同时看到 `security/backend_event` 和 `rtp/rtcp/srtp` 失败，污染计划要求的失败层级。
- **Fix:** SRTP 函数只返回 status；核心 `src/srtp/srtp.c` 继续负责 `RTC_SECURITY_DETAIL_SRTP_*` detail 映射和 observer error。
- **Files modified:** `examples/chrome_e2e/security_backend_chrome.c`
- **Verification:** `rg` detail code 验收通过；默认 OFF security gate smoke 通过。
- **Committed in:** `3ca4220`, `0cfb54c`

**4. [Rule 3 - Blocking] 忽略本地验证输出**
- **Found during:** Overall verification
- **Issue:** ON configure 探测产生 `build-secure/`，仓库此前也未忽略 `build/` 和 `node_modules/`，会把本地验证输出留成未跟踪噪声。
- **Fix:** `.gitignore` 增加 `build/`、`build-secure/`、`node_modules/`。
- **Files modified:** `.gitignore`
- **Verification:** `git status --short` 不再列出这些生成目录。
- **Committed in:** `d15d5d0`

---

**Total deviations:** 4 auto-fixed (3 Rule 1, 1 Rule 3)
**Impact on plan:** 偏差均用于确保真实 Chrome DTLS/SRTP 路径可用、失败层级准确和验证输出可控；未改变核心库边界或默认依赖策略。

## Known Stubs

None. 扫描未发现 `TODO`、`FIXME`、placeholder、coming soon 或会影响目标达成的空数据 stub。

## Threat Flags

| Flag | File | Description |
|------|------|-------------|
| threat_flag: optional-crypto-backend | `examples/chrome_e2e/security_backend_chrome.c` | 新增示例层 OpenSSL DTLS 和 libsrtp 集成面；默认 OFF，不进入核心库默认依赖，身份校验仍由 fingerprint 比对承担。 |

## Issues Encountered

- 本机缺少 OpenSSL/libsrtp 开发依赖，`RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON` 只能验证到 CMake 依赖 gate，无法构建 secure target 或运行 full E2E。
- `gsd-sdk` 不在默认 shell PATH；后续状态更新使用 `node node_modules/get-shit-done-cc/bin/gsd-sdk.js ...` 调用本地 CLI。

## User Setup Required

如需运行 secure full E2E，需要在本机安装 OpenSSL 和 libsrtp2 开发包，然后执行：

```bash
cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON && cmake --build build-secure --target rtc_chrome_e2e
```

## Next Phase Readiness

`06-10` 可以在安装可选安全依赖的环境中继续执行 secure full E2E、检查 JSONL failure layer 是否越过 `optional_security_backend_disabled`，并完成 VLC/ffplay 人工播放验收。`ACC-01` 在 06-10 完成前仍保持未完成。

## Self-Check: PASSED

- `06-09-SUMMARY.md`、`security_backend_chrome.c`、`README.md` 和 `.gitignore` 均存在。
- 任务/修正提交 `b87d691`、`2f5b6ff`、`70e94cd`、`d15d5d0`、`3ca4220`、`0cfb54c` 均可在 git log 中找到。
- 默认 OFF 验证命令 `cmake -S . -B build -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=OFF && cmake --build build --target rtc_chrome_e2e && node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke` 通过。
- 所有 task acceptance `rg` 检查均通过。
- ON configure 在本机因缺少可选 OpenSSL/libsrtp 开发依赖按预期停在 CMake gate，已记录为环境受限验证。
- 未发现本计划提交包含意外删除文件。

---
*Phase: 06-chrome-end-to-end-acceptance*
*Completed: 2026-05-11*
