所有 Markdown（`.md`）文档尽可能使用中文编写。

## 项目上下文

- 本项目是 `libmicrortc`，目标是从 AWS KVS WebRTC C SDK 中剥离通用 C WebRTC 协议栈。
- 需求、路线图和当前状态以 `.planning/PROJECT.md`、`.planning/REQUIREMENTS.md`、`.planning/ROADMAP.md`、`.planning/STATE.md` 为准。
- AWS KVS WebRTC SDK 的剥离基线必须以本地 `reflib/kvs-webrtc-sdk` 为准；不要默认使用 GitHub 上游最新版本。
- v1 优先 Linux x86_64、Chrome、H264、Opus、TURN relay、双向音视频和自动化 E2E 验收。
- 核心库不内置应用层 signaling，不做媒体采集和编码，只处理编码后媒体帧。
