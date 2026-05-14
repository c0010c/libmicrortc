# Phase 6: Chrome 自动化 E2E 与测试收口 - Discussion Log

> **仅作审计记录。** 不要把本文作为 planning、research 或 execution agent 的输入。
> 决策已记录在 CONTEXT.md；本文只保留讨论过的备选项。

**Date:** 2026-05-13T16:29:22+08:00
**Phase:** 6-Chrome 自动化 E2E 与测试收口
**Areas discussed:** E2E harness 拓扑, 媒体流动证明方式, TURN relay 验收边界, 测试命令与输出收口

---

## E2E Harness 拓扑

| Option | Description | Selected |
|--------|-------------|----------|
| 本地 WebSocket signaling server | runner 启动测试用 signaling server，C demo 和 Chrome 页面都连接它交换 SDP/candidate；核心库保持 signaling-free。 | ✓ |
| 文件/stdout 交换 | 实现更薄，但 Chrome 页面和 C 进程编排较别扭，后续扩展弱。 | |
| HTTP polling/REST 交换 | 比文件方式清楚，但 trickle candidate 和状态同步比 WebSocket 笨重。 | |

**User's choice:** 本地 WebSocket signaling server  
**Notes:** 锁定为 demo/测试层 signaling，不进入核心库。

| Option | Description | Selected |
|--------|-------------|----------|
| Node + Playwright 主导 | Playwright 启动 Chrome 和页面，Node 管理 signaling server 与 C demo 子进程；浏览器断言最自然。 | ✓ |
| C/POSIX runner 主导 | 贴近 C 测试风格，但驱动 Chrome 和收集浏览器状态更吃力。 | |
| Shell/CMake 编排多个进程 | 依赖最少，但失败诊断和异步状态管理脆弱。 | |

**User's choice:** Node + Playwright 主导  
**Notes:** Playwright 是测试工具依赖，不是核心库依赖。

| Option | Description | Selected |
|--------|-------------|----------|
| Answerer 优先 | 延续 Phase 3 的 Answerer 先行：Chrome 创建 offer，C demo answer；风险最低。 | ✓ |
| Offerer + Answerer 都必须做 | 覆盖更完整，但会扩大 Phase 6 压力。 | |
| Offerer 优先 | 补齐预留能力，但偏离已有 Answerer 主路径。 | |

**User's choice:** Answerer 优先  
**Notes:** C 端 Offerer 完整 E2E 不作为本阶段必须项。

| Option | Description | Selected |
|--------|-------------|----------|
| `examples/chrome-e2e/` + `tests/e2e/` 分层 | C demo 和浏览器页面作为可运行示例，Playwright 测试和 runner 放 E2E 测试目录。 | ✓ |
| 全部放 `tests/e2e/` | 更像纯测试资产，不强调 demo 可复用。 | |
| 全部放 `examples/chrome-e2e/` | demo 一体化更强，但测试脚本和示例代码边界会混在一起。 | |

**User's choice:** `examples/chrome-e2e/` + `tests/e2e/` 分层  
**Notes:** 示例可手动运行，自动化测试也有独立入口。

---

## 媒体流动证明方式

| Option | Description | Selected |
|--------|-------------|----------|
| HTMLVideoElement + `getStats()` 组合 | 断言视频元素可播放/有尺寸，同时 inbound RTP frames/bytes 增长；比只看 stats 更贴近真实输出。 | ✓ |
| 只用 `getStats()` | 自动化稳定，但离用户可观察视频输出远一点。 | |
| Canvas 抽帧/像素检查 + stats | 证明更强，但对解码、渲染时序和无头 Chrome 更敏感。 | |

**User's choice:** HTMLVideoElement + `getStats()` 组合  
**Notes:** 用作浏览器侧 H264 接收的主要断言。

| Option | Description | Selected |
|--------|-------------|----------|
| WebAudio 能量/采样活动 + `getStats()` | 证明音频管线有活动，同时看 inbound RTP bytes/packets；不要求人工听音。 | ✓ |
| 只用 `getStats()` | 更稳定，但只能证明包流入。 | |
| 录制短音频片段再分析 | 证明更强，但实现和 CI 更脆。 | |

**User's choice:** WebAudio 能量/采样活动 + `getStats()`  
**Notes:** 不要求生成可听音频文件。

| Option | Description | Selected |
|--------|-------------|----------|
| 编码后 frame 回调计数 + 基本元数据校验 | C demo 统计 `on_frame`、非空 frame、kind/codec、timestamp 单调；贴合核心库编码后帧边界。 | ✓ |
| RTP packet 计数即可 | 更底层更容易，但没证明 depacketize 后的编码帧输出。 | |
| 写文件并做二次验证 | 诊断价值高，但可播放/可解码不应成为 v1 必需。 | |

**User's choice:** 编码后 frame 回调计数 + 基本元数据校验  
**Notes:** 只统计 RTP packet 不足以满足本阶段。

| Option | Description | Selected |
|--------|-------------|----------|
| 短时稳定阈值 | 每路媒体在限定时间内达到最小 packets/bytes/frames，且连续几次增长；兼顾稳定性和速度。 | ✓ |
| 只要收到一次即可 | 跑得快，但容易误判瞬时成功。 | |
| 固定持续时长 | 更接近真实通话，但 CI 时间更长，失败面更多。 | |

**User's choice:** 短时稳定阈值  
**Notes:** 具体阈值留给 planner 按 CI 稳定性确定。

---

## TURN Relay 验收边界

| Option | Description | Selected |
|--------|-------------|----------|
| 单独真实网络命令 | 默认 E2E 可在 host/local 跑通；TURN relay 用显式命令和本地配置跑，缺配置时硬失败。 | ✓ |
| 默认 E2E 必跑 relay | 验收最硬，但所有环境都必须有 TURN 配置和网络。 | |
| 缺配置时自动跳过 | 开发体验好，但容易让 TURN 验收长期没跑。 | |

**User's choice:** 单独真实网络命令  
**Notes:** TURN relay 不进入默认本地/host E2E 必跑路径。

| Option | Description | Selected |
|--------|-------------|----------|
| 沿用 Phase 4 根目录本地 JSON | 继续使用 `ice_servers`、`urls`、`username`、`credential/password` 字段，避免多套秘密配置。 | ✓ |
| Phase 6 单独配置文件 | E2E 参数更丰富，但多一套 secret 管理。 | |
| 全部从环境变量读取 | CI 友好，但本地复现和多 server 配置不如 JSON 清晰。 | |

**User's choice:** 沿用 Phase 4 根目录本地 JSON  
**Notes:** 示例模板不含真实 credential。

| Option | Description | Selected |
|--------|-------------|----------|
| 强制 `iceTransportPolicy=relay` + selected candidate pair 校验 | Chrome 侧 relay-only，并用 stats 检查 selected pair/local candidate type 为 relay；C 端也输出证据。 | ✓ |
| 只检查配置里有 TURN URL | 容易实现，但不能证明实际选中了 relay path。 | |
| 网络层封锁 host/srflx 后再跑 | 证明强，但实现和 CI 要求高。 | |

**User's choice:** 强制 `iceTransportPolicy=relay` + selected candidate pair 校验  
**Notes:** 不能只检查 TURN URL 存在。

| Option | Description | Selected |
|--------|-------------|----------|
| 连接 + DataChannel + 双向媒体全部覆盖 | Phase 6 是 v1 真正验收点，relay path 必须证明完整能力可用。 | ✓ |
| 只覆盖 PeerConnection connected | 最快，但对 NET-04/E2E 收口力度不足。 | |
| 连接 + DataChannel，媒体仍走默认 host E2E | 折中，但 TURN 下媒体真实流动仍会留下缺口。 | |

**User's choice:** 连接 + DataChannel + 双向媒体全部覆盖  
**Notes:** relay E2E 必须覆盖完整能力。

---

## 测试命令与输出收口

| Option | Description | Selected |
|--------|-------------|----------|
| 默认 `ctest` 只跑确定性 C/协议测试；Chrome E2E 单独命令 | 保持 CTest 快速稳定，浏览器/Node/进程编排用显式 E2E 命令收口。 | ✓ |
| 默认 `ctest` 包含 host Chrome E2E | 一条命令验收更强，但要求所有环境都有 Node/Playwright/Chrome。 | |
| 默认 `ctest` 包含所有 E2E，包括 TURN | 最硬但外部依赖最多。 | |

**User's choice:** 默认 `ctest` 只跑确定性 C/协议测试；Chrome E2E 单独命令  
**Notes:** 默认 C 开发循环不被浏览器依赖拖慢。

| Option | Description | Selected |
|--------|-------------|----------|
| 需要总脚本，但 TURN 显式 opt-in | 默认跑 build + CTest + host Chrome E2E；加 `--turn-config` 才跑 relay E2E，缺配置硬失败。 | ✓ |
| 不需要总脚本，只提供分散命令 | 简单，但 v1 收口时容易漏跑。 | |
| 总脚本默认要求 TURN | 最严格，但普通本地验证门槛高。 | |

**User's choice:** 需要总脚本，但 TURN 显式 opt-in  
**Notes:** 总脚本用于 v1 收口，TURN 通过显式参数打开。

| Option | Description | Selected |
|--------|-------------|----------|
| 分阶段清晰 banner + 机器可读摘要 | 明确区分 build、ctest、chrome-host-e2e、chrome-turn-e2e、media-flow，最后生成摘要。 | ✓ |
| 只依赖各工具原生日志 | 工作量最少，但失败定位困难。 | |
| 只做人类可读日志，不做机器摘要 | 本地友好，CI 后续还要补。 | |

**User's choice:** 分阶段清晰 banner + 机器可读摘要  
**Notes:** 输出需要能定位失败层。

| Option | Description | Selected |
|--------|-------------|----------|
| 仓库内 `tests/e2e/package.json` 固定依赖 | Playwright 作为测试工具，版本锁定在测试目录；不污染核心 C 库。 | ✓ |
| 不提交 Node 配置，只在文档里说明安装 | 仓库更轻，但自动化验收不可复现。 | |
| 根目录 package.json | JS 工具友好，但项目主体是 C 库，根目录会变重。 | |

**User's choice:** 仓库内 `tests/e2e/package.json` 固定依赖  
**Notes:** Playwright 依赖限制在 `tests/e2e/`。

---

## the agent's Discretion

- WebSocket signaling message schema、runner 文件名、具体断言阈值、摘要格式、总验收脚本名称和可选 CTest label 由 planner 决定。
- 页面 UI 只需服务 E2E/demo，可保持最小但应便于手动观察。

## Deferred Ideas

- C 端 Offerer 完整 Chrome E2E。
- Firefox/Safari 互通和多浏览器矩阵。
- 媒体采集、编码、GStreamer、FFmpeg、Ogg/MP4 容器解析。
- 网络层封锁 host/srflx 作为 relay 证明手段。
