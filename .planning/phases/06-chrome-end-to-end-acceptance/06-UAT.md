---
status: complete
phase: 06-chrome-end-to-end-acceptance
source:
  - 06-01-SUMMARY.md
  - 06-02-SUMMARY.md
  - 06-03-SUMMARY.md
  - 06-04-SUMMARY.md
  - 06-05-SUMMARY.md
  - 06-06-SUMMARY.md
  - 06-07-SUMMARY.md
  - 06-08-SUMMARY.md
  - 06-09-SUMMARY.md
  - 06-10-SUMMARY.md
  - 06-11-SUMMARY.md
started: 2026-05-11T19:10:44+08:00
updated: 2026-05-11T19:50:00+08:00
---

# 第 6 阶段 UAT：Chrome 端到端验收

## Current Test

[testing complete]

## Tests

### 1. Cold Start Smoke Test
expected: 清理或忽略上一轮 `examples/chrome_e2e/out/` 输出后，从空闲状态启动验收命令。`npm run e2e:chrome:dry-run` 应通过；`cmake -S . -B build -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=OFF` 和 `cmake --build build --target rtc_chrome_e2e` 应通过；再次运行 smoke 不应依赖旧 JSONL 或旧媒体文件。
result: pass
evidence: `npm run e2e:chrome:dry-run` 通过；默认 OFF configure/build 通过。

### 2. Chrome 页面与合成媒体
expected: `npm run e2e:chrome:page-smoke` 应通过。Chrome 页面应能生成 offer，使用 canvas/Web Audio 合成音视频，不触发摄像头或麦克风权限请求；页面上本地媒体、远端媒体区域、七阶段状态条和 diagnostics 面板可见。
result: pass
evidence: `npm run e2e:chrome:page-smoke` 通过，`offerCreated:true`，`permissionRequests:0`。

### 3. JSON-only 信令与 C 示例 smoke
expected: `npm run e2e:chrome:c-smoke` 应通过。C 示例应支持 `--run-id`，与页面共享同一 runId，WebSocket text frame 分片读取稳定，JSONL summary 表示 C 示例 dry-run 通过；信令服务只转发 SDP、candidate、status、summary、error 等 JSON，不接受媒体 payload 或二进制帧。
result: pass
evidence: `npm run e2e:chrome:c-smoke` 通过，JSONL events=3，layer 为 `none`。

### 4. 媒体样本解析与文件输出 smoke
expected: `npm run e2e:chrome:media-smoke` 应通过。示例层能解析 `sample1.opus` 和 `test-25fps.h264`，dry-run 产物包含 `received-opus.packets` 与 `received-h264.264`，JSONL summary 中音频/视频 frame 和 byte 统计均大于 0。
result: pass
evidence: `npm run e2e:chrome:media-smoke` 通过，`audio_frames_received=1`、`video_frames_received=1`、`audio_bytes_received=407`、`video_bytes_received=15`。

### 5. 默认安全 backend OFF gate
expected: `npm run e2e:chrome:security-gate` 应通过。默认 `RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=OFF` 时，full E2E 不应误报成功，应清晰停在 `pass:false`、`layer:"dtls"`、`reason:"optional_security_backend_disabled"`，且 `manual_vlc_required:true` 保留。
result: pass
evidence: `npm run e2e:chrome:security-gate` 通过，summary 为 `pass:false`、`layer:"dtls"`、`reason:"optional_security_backend_disabled"`。

### 6. Secure ON 构建与 deterministic tests
expected: 使用用户级依赖路径 `~/.local/rtc-deps/openssl-1.1.1w` 和 `~/.local/rtc-deps/libsrtp-2.6.0` 后，`cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON -DOPENSSL_ROOT_DIR=... -DCMAKE_PREFIX_PATH=...`、`cmake --build build-secure --target rtc_chrome_e2e`、`cmake --build build-secure --target rtc_tests` 和 `ctest --test-dir build-secure --output-on-failure` 应通过。
result: pass
evidence: secure ON configure/build、`rtc_tests` 构建和 `ctest --test-dir build-secure --output-on-failure` 均通过；CMake 发现 OpenSSL 1.1.1w 和用户级 `libsrtp2.a`。

### 7. Secure full E2E
expected: 设置 `OPENSSL_ROOT_DIR`、`CMAKE_PREFIX_PATH` 和 `LD_LIBRARY_PATH` 后运行 `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000 --output-dir examples/chrome_e2e/out/full-secure`。compact summary 应显示 `pass:true`、`layer:"none"`，C 示例 summary pass 为 true，进程退出码为 0，`received-opus.packets` 与 `received-h264.264` 均存在且非空；如果失败，summary 的 layer 应准确落在 signaling、ice、dtls、srtp、rtp、rtcp 或 media_file。
result: pass
evidence: "06-11 修复后使用 `RTC_CHROME_E2E_BINARY=build-secure/rtc_chrome_e2e node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000 --output-dir examples/chrome_e2e/out/full-secure` 通过；compact summary 为 `pass:true`、`layer:\"none\"`，C 示例 exitCode=0、summary `pass:true`、`reason:\"e2e passed\"`，页面最新状态包含 `ice.connected`、`dtls.connected`、`srtp.ready`，`received-h264.264` 为 1479 字节，`received-opus.packets` 为 314 字节。"

### 8. VLC/ffplay 人工媒体播放批准
expected: 在 secure full E2E 生成非空媒体文件后，使用 VLC 或 `ffplay -f h264 examples/chrome_e2e/out/full-secure/received-h264.264` 检查视频；按 README 说明处理 `received-opus.packets` 的长度前缀 Opus packet 序列并检查音频。音频和视频都可播放后，记录 `VLC approved`、日期、运行命令、compact summary、视频结果和音频结果。未完成此项前，`ACC-01` 不能关闭。
result: pass
evidence: "VLC approved：2026-05-11 用户在 `$gsd-verify-work 6` Test 8 中回复 `1` 批准通过，确认 `examples/chrome_e2e/out/full-secure/received-h264.264` 和按 README 处理后的 `received-opus.packets` 音视频均可播放；compact summary 为 `pass:true`、`layer:\"none\"`、`reason:\"e2e passed\"`。"

### 9. Chrome 页面人工视觉检查
expected: 在本地 Chrome E2E 页面中，双视频区域、七阶段状态条和 diagnostics 面板应可读、不互相遮挡；`summary-layer` 在未通过人工媒体 gate 时显示当前失败层级或 `manual_vlc_pending`；页面仍不请求摄像头或麦克风权限。
result: pass
evidence: headless Playwright DOM/layout 检查通过；本地/远端视频元素可见且不重叠，七阶段状态条与 diagnostics 关键字段均可见；page smoke 同时确认 `permissionRequests:0`。

## Summary

total: 9
passed: 9
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

[none]
