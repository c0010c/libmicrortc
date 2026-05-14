---
status: complete
quick_id: 260514-kea
slug: manual-browser-pull
date: 2026-05-14
commit: 7781fbd
---

# Quick Task 260514-kea 总结：浏览器手动一键开始拉流验证入口

## 完成内容

- 确认现有 Chrome E2E 链路已经具备浏览器、WebSocket signaling bridge、C answerer 和 `start-media` 控制消息，但页面缺少面向手动验证的一键入口。
- 为 `examples/chrome-e2e/index.html` 增加手动验证界面：视频、音频、连接状态、媒体状态和事件日志。
- 为 `examples/chrome-e2e/browser-client.js` 增加 `manual=1` 模式；点击“开始拉流”后自动完成连接，并等待本地与 C 端 DataChannel 都打开后再发送 `start-media`，避免 C 端媒体发送过早。
- 新增 `tests/e2e/run-manual.js` 和 `npm --prefix tests/e2e run manual`，启动本地 signaling bridge 和 C answerer，并打印可直接用 Chrome 打开的 `file://...manual=1&ws=...` URL。
- 更新 `examples/chrome-e2e/README.md`，记录手动验证步骤和期望状态。

## 验证

- `node --check tests/e2e/run-manual.js`
- `node --check examples/chrome-e2e/browser-client.js`
- `cmake -S . -B build -DMRTC_BUILD_TESTS=ON`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `timeout 3s npm --prefix tests/e2e run manual -- --timeout-ms 10000`
- Playwright 手动模式 smoke：打开 `manual=1` 页面、点击“开始拉流”、等待 `connected` 与 `remote.media.sent`
- `MRTC_E2E_BROWSER_CHANNEL=chromium MRTC_E2E_FIXTURES=/home/qs/dev/libmicrortc/tests/fixtures npm --prefix tests/e2e run test:host`

## 备注

- 第一次手动 smoke 暴露出 `start-media` 发得过早的问题；已改为等待双端 DataChannel ready 后再触发媒体发送。
- 曾有一次 host E2E 因手动传入相对 `MRTC_E2E_FIXTURES=tests/fixtures` 导致子进程找不到 fixture；使用绝对路径后通过。
