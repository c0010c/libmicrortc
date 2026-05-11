# Chrome E2E 验收 Harness

本目录提供第 6 阶段本地 Chrome 端到端验收的浏览器侧 harness：

- `signaling.mjs` 同时提供静态页面 HTTP 服务和 `/ws` WebSocket 信令服务。
- `run_e2e.mjs` 提供 dry-run、页面 smoke 和后续完整 E2E 编排入口。
- `page/` 放置原生 HTML/CSS/JS 页面；页面只使用 canvas 与 Web Audio 合成媒体，不请求摄像头或麦克风权限。

## 信令边界

WebSocket 信令只转发同一 `runId` 内的 JSON 消息，允许类型为 `hello`、`offer`、`answer`、`candidate`、`status`、`summary` 和 `error`。服务拒绝非 JSON、未知 `type`、二进制帧，以及包含 `media`、`payload` 或 `binary` 字段的消息。

信令服务不代理 UDP，不承载 RTP/RTCP/DTLS/SRTP 媒体 payload，也不替代用户侧 socket 收发责任。

## 运行

```bash
npm run e2e:chrome:dry-run
```

后续计划会把 C 示例、样本媒体解析和完整 E2E 编排接入同一入口。

## 依赖许可证

- `ws`: MIT
- `@playwright/test`: Apache-2.0
