# Quick Task 260514-kea: 浏览器手动一键开始拉流验证入口

## 目标

让开发者可以在浏览器中手动验证 Chrome E2E host 拉流链路：启动本地 bridge 后，打开页面，只点击一次“开始拉流”即可完成连接、触发 C 端发送 H264/Opus fixture，并在页面上看到连接与媒体状态。

## 任务

1. 检查现有 Chrome E2E 示例是否已经支持手动一键拉流。
   - 发现：现有页面支持自动化测试链路，但页面无手动按钮，`start-media` 主要由 Playwright 控制台 API 触发。

2. 补充手动验证入口。
   - 更新 `examples/chrome-e2e/index.html` 和 `browser-client.js`，加入手动模式、按钮、状态与事件展示。
   - 保持无 `manual=1` 时的自动化测试行为不变。

3. 补充本地启动脚本与说明。
   - 新增 `tests/e2e/run-manual.js` 和 `npm run manual`。
   - 更新 `examples/chrome-e2e/README.md`，给出手动验证步骤。

## 验证

- 构建 `mrtc_chrome_answerer`。
- 运行相关 Node 脚本 dry-run 或启动检查。
- 运行现有 host Chrome E2E，确认自动化入口未被破坏。
