---
phase: 06-chrome-end-to-end-acceptance
plan: 06
status: manual-gate-pending
created: 2026-05-11
---

# 第 6 阶段 UAT 清单

本清单用于收口 Chrome 端到端验收。自动化负责确认链路状态、counter、JSONL summary 和媒体文件产物；媒体播放质量仍由人工 VLC/ffplay 检查确认。只要 `manual_vlc_required:true` 仍存在且没有人工批准记录，`ACC-01` 就保持待人工验收。

## 自动化检查

| 项目 | 命令 | 期望 |
|------|------|------|
| Node 依赖 | `npm install` | 安装 `ws` 与 `@playwright/test`，不引入 GPL/LGPL 依赖 |
| C 示例构建 | `cmake -S . -B build && cmake --build build --target rtc_chrome_e2e` | 生成 `build/rtc_chrome_e2e` |
| dry-run | `node examples/chrome_e2e/run_e2e.mjs --dry-run` | 样本文件、脚本入口和构建产物 preflight 通过 |
| 页面 smoke | `node examples/chrome_e2e/run_e2e.mjs --page-smoke` | Chrome 页面生成 offer，且不请求摄像头/麦克风权限 |
| C 示例 smoke | `node examples/chrome_e2e/run_e2e.mjs --c-example-smoke` | C dry-run JSONL summary 通过，示例层 API 串联存在 |
| 媒体文件 smoke | `node examples/chrome_e2e/run_e2e.mjs --media-file-smoke` | 示例层 parser 与 `received-opus.packets`、`received-h264.264` dry-run 产物检查通过 |
| 安全 gate smoke | `node examples/chrome_e2e/run_e2e.mjs --security-gate-smoke` | 默认构建报告 `dtls/optional_security_backend_disabled` |
| full E2E | `node examples/chrome_e2e/run_e2e.mjs --timeout-ms 30000` | 默认安全 backend OFF 时允许失败在 `dtls`；启用可选 backend 后应继续检查 `srtp.ready`、RTP/RTCP 和 media files |

## 页面视觉检查

- 打开本地 Chrome E2E 页面后，首屏应直接显示本地 synthetic media、远端 C sample media、七阶段状态条和 diagnostics 面板。
- `Signaling`、`ICE`、`DTLS`、`SRTP`、`RTP`、`RTCP`、`Media files` 七个阶段应可读，不遮挡视频或 diagnostics。
- `summary-layer` 在未完成人工媒体检查时应显示 `manual_vlc_pending` 或当前失败层级。
- 页面媒体源必须来自 synthetic canvas/Web Audio，不出现摄像头或麦克风授权提示。

## VLC/ffplay 人工播放检查

前置条件：

- full E2E 不再停在 `dtls/optional_security_backend_disabled`。
- compact JSON summary 中 `media_files` 列出 `received-opus.packets` 和 `received-h264.264`，且字节数大于 0。
- summary 仍包含 `manual_vlc_required:true`，提醒人工 gate 尚未关闭。

人工步骤：

1. 记录 full E2E 输出的一行 compact JSON summary。
2. 打开输出目录中的 `received-h264.264`，可使用 VLC 或 `ffplay -f h264 received-h264.264`。
3. `received-opus.packets` 是 4 字节 big-endian 长度前缀的 Opus packet 序列；先重新封装为可播放容器，再用 VLC/ffplay 检查音频。
4. 如果视频和音频都能播放，并且 JSONL 没有新的 `signaling`、`ice`、`dtls`、`srtp`、`rtp`、`rtcp` 或 `media_file` error，人工记录 `VLC approved`、日期、运行命令和 summary。

当前状态：默认安全 backend 仍关闭，真实 Chrome DTLS/SRTP full E2E 与 VLC 人工播放尚未完成；不得宣称自动化已经替代人工媒体播放检查。

## 06-10 执行记录

日期：2026-05-11
命令：`cmake -S . -B build -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=OFF`
结果：通过，默认 OFF 构建仍不查找或链接可选 OpenSSL/libsrtp 依赖。

命令：`cmake --build build --target rtc_chrome_e2e`
结果：通过，生成 `build/rtc_chrome_e2e`。

命令：`npm run e2e:chrome:dry-run`
结果：通过，样本文件、页面、信令脚本和 npm smoke 入口存在。

命令：`npm run e2e:chrome:security-gate`
结果：通过，summary 为 `pass:false`、`layer:"dtls"`、`reason:"optional_security_backend_disabled"`，符合默认安全 backend OFF gate。

命令：`cmake -S . -B build-secure -DRTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=ON`
结果：环境阻断。本机缺少可选 OpenSSL/libsrtp 开发依赖，CMake 在 `FindRtcOptionalSecurity.cmake` 按预期失败并提示安装依赖或改回 OFF。

```json
{"pass":false,"layer":"dtls","reason":"optional_security_backend_disabled","manual_vlc_required":true,"media_files":{"received-opus.packets":{"exists":false,"bytes":0},"received-h264.264":{"exists":false,"bytes":0}},"environment_blocker":"missing optional OpenSSL/libsrtp development dependencies","output_dir":"examples/chrome_e2e/out/full-secure"}
```

VLC/ffplay 结果：未执行。由于 secure ON 构建未完成，未生成 `examples/chrome_e2e/out/full-secure/received-opus.packets` 和 `examples/chrome_e2e/out/full-secure/received-h264.264`，不能进行人工播放批准。

下一步：安装可选 OpenSSL/libsrtp2 开发包后重新运行 secure build、full E2E 和 VLC/ffplay 人工检查；在 full E2E 输出 `pass:true`、`layer:"none"` 且音视频人工播放通过前，`ACC-01` 保持未完成。

## JSONL Summary 检查

每次 full run 至少保存或记录以下字段：

| 字段 | 检查 |
|------|------|
| `pass` | 只有自动化链路通过时才为 `true`；VLC 未确认时仍不能单独关闭 `ACC-01` |
| `layer` | 必须是 `signaling`、`ice`、`dtls`、`srtp`、`rtp`、`rtcp`、`media_file` 或 `none` |
| `reason` | 默认安全 gate 应为 `optional_security_backend_disabled` |
| `manual_vlc_required` | 必须为 `true`，直到人工播放验收另行记录 |
| `media_files` | 应列出 `received-opus.packets`、`received-h264.264` 的存在性和字节数 |
| `latestEvents` | 最近事件足以定位失败层级 |

## 失败层级记录模板

```text
日期：
命令：
可选安全 backend：OFF / ON（依赖与许可证说明）
summary.pass：
summary.layer：
summary.reason：
manual_vlc_required：
received-opus.packets：存在/缺失，字节数：
received-h264.264：存在/缺失，字节数：
VLC/ffplay 结果：未执行/通过/失败
最近 JSONL 事件：
下一步：
```

## 完成判定

- `TST-01`：自动化测试、smoke、full run failure layering 和本 UAT 文档已覆盖，可按第 6 阶段自动化范围关闭。
- `EXM-01`：Chrome 合成媒体页面、状态面板和 page smoke 已完成。
- `EXM-02`：WebSocket 信令服务与 C 示例信令 client 已完成。
- `ACC-01`：保持部分完成/待人工验收，直到启用真实可选安全 backend 后 full E2E 通过，并完成 VLC/ffplay 人工播放确认。
