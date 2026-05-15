# Feature Research

**Domain:** C WebRTC 协议栈 v1.1 公共 API 命名与代码风格清理
**Researched:** 2026-05-15
**Confidence:** HIGH

## 结论

v1.1 应该做一次有边界的破坏式 API 清理：公共函数继续使用 `mrtc_` 前缀和 `lower_case`，公共类型从当前 `MRTC_*` 全大写 typedef 收敛到 `Mrtc*` CamelCase，宏和枚举常量保留 `MRTC_*` UPPER_CASE。这样同时符合 C 库显式命名空间实践、仓库 `.clang-tidy` 规则，以及 libmicrortc 摆脱 AWS/KVS 风格的目标。

本里程碑不应该改变 WebRTC 协议能力、线程模型、signaling 边界、媒体边界或浏览器矩阵。所有需求都应围绕“新 API 可编译、示例/测试已迁移、旧 AWS/KVS 命名不再出现在公共头、v1.0 验收继续通过”来定义。

## Feature Landscape

### Table Stakes (Users Expect These)

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| 公共 API 命名规范冻结 | v1.1 的核心价值是让使用者看到独立 libmicrortc 风格，而不是继续暴露 AWS/KVS 派生命名 | MEDIUM | 明确规则：函数 `mrtc_*`；类型 `MrtcStatus`、`MrtcPeerConnectionConfig`、`MrtcFrame`；枚举常量/宏 `MRTC_STATUS_OK`、`MRTC_FRAME_FLAG_KEY_FRAME`。验收可用 `clang-tidy` 和 header grep。 |
| 公开头文件表面收敛 | C 库用户首先接触的是 `include/`；公共头必须只暴露 v1.1 承诺支持的 API | MEDIUM | 保留 `<micrortc/micrortc.h>` umbrella header；按需要拆分 `peer_connection.h`、`data_channel.h`、`transceiver.h`、`media_types.h`，但不得把 private transport/RTP/SDP helper 泄漏成 supported API。 |
| opaque object 类型替代 `*_HANDLE` 风格 | 当前 `MRTC_PEER_CONNECTION_HANDLE` 等更像来源 SDK/Windows handle 命名；独立 C 库更适合显式 opaque struct 指针 | MEDIUM | 推荐 `typedef struct MrtcPeerConnection MrtcPeerConnection;`，API 使用 `MrtcPeerConnection *pc`；`DataChannel`、`RtpTransceiver` 同理。若保留 handle typedef，也应是 `MrtcPeerConnectionHandle`，但不优先。 |
| 结构体、回调、枚举类型 CamelCase 化 | `.clang-tidy` 已要求 Struct/Typedef/Enum 使用 CamelCase；当前 `MRTC_*` 类型会持续制造 lint debt | MEDIUM | 如 `MRTC_PEER_CONNECTION_CALLBACKS` -> `MrtcPeerConnectionCallbacks`，`MRTC_DATA_CHANNEL_MESSAGE_TYPE` -> `MrtcDataChannelMessageType`。字段名保持 `lower_case`。 |
| 语义不变的迁移映射文档 | 破坏式改名如果没有旧名到新名的表，用户无法可靠升级 | LOW | 新增/更新 `docs/MIGRATING-v1.0-to-v1.1.md` 或 README 迁移章节，列出 public symbol mapping、include mapping、行为不变项、删除项和示例 diff。 |
| 示例和 package consumer 全量迁移 | 用户会照抄示例；示例若仍用旧名会削弱 v1.1 API 信号 | MEDIUM | 更新 `examples/chrome-e2e/mrtc_chrome_answerer.c`、`tests/package-consumer/package_consumer.c`、README 代码片段。验收：示例和外部 consumer 用新 API 编译链接。 |
| 公共 API contract 测试迁移 | 需要证明新命名不是只改头文件，而是完整可用 | MEDIUM | 更新 `tests/peer_connection/test_peer_connection_api.c` 覆盖 PeerConnection lifecycle、SDP、ICE candidate、DataChannel、transceiver、encoded frame send/callback、selected candidate info。 |
| v1.0 能力回归验证 | 命名清理不能破坏已验证能力 | HIGH | `scripts/verify-v1.sh` 仍必须通过；有 TURN 配置时 `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` 仍验证 relay、DataChannel、H264/Opus 双向媒体。 |
| 旧 AWS/KVS 公共残留扫描 | API 清理完成标准需要可机器验证，不能只靠人工读头文件 | LOW | 对 `include/`、README、examples、tests 建立 grep/脚本检查：禁止 `KVS`、`Kinesis`、`Aws`、`RtcPeerConnection`、`PRtc*`、`STATUS` 作为类型后缀等来源语义残留；license/source comments 不应误杀。 |
| 版本/发布说明标记 breaking change | 破坏式公共 API 改名是用户可见兼容性事件 | LOW | README/CHANGELOG 标明 v1.1 是 API cleanup milestone；由于项目当前 CMake version 是 `0.1.0` 且规划命名为 v1.1，需至少在文档中明确“source compatibility breaking”。 |

### Differentiators (Competitive Advantage)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| “最小但完整”的独立 C API | 与保留 AWS SDK 兼容层相比，更容易把 libmicrortc 作为嵌入式/原生应用依赖 | MEDIUM | 只公开 PeerConnection、SDP offer/answer、ICE candidate、DataChannel、transceiver、encoded frame 和回调相关类型；不公开内部协议模块。 |
| 迁移表由测试背书 | 迁移文档不只是说明文字，而是和 package consumer / examples / contract tests 一起验证 | MEDIUM | 每个迁移后的核心 API 至少在一个编译目标中出现；迁移表中“保留语义”的条目对应测试覆盖。 |
| 命名清理同时约束内部边界 | 公共 API 清理容易只停在 include；v1.1 可以把内部/public 边界也收紧 | MEDIUM | public header 不 include private headers；private headers 可逐步改名，但优先保证 public ABI/API 表面和 examples/tests。 |
| 自动化 API 表面快照 | 后续里程碑改 API 时能看到明确 diff，避免偶发泄漏 | LOW | 生成 `build/reports/mrtc-public-api.txt` 或类似列表，包含 public headers、exported `mrtc_*` 函数、public typedef/enum/macro；v1.1 可作为基线。 |
| 双层验收叙事 | 同时证明“API 名字干净”和“WebRTC 行为没坏” | LOW | 报告中区分 API cleanup checks、CTest/package consumer、Chrome host、TURN relay；便于 roadmap 把风格工作和协议回归分开。 |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| 为旧 AWS/KVS 名称保留完整兼容宏/typedef 层 | 迁移更容易，老代码可少改 | 与“摆脱 AWS/KVS 风格”和破坏式清理目标冲突；公共头继续携带旧语义，后续还要维护双 API | 提供迁移文档和一次性 mapping；不承诺 source compatibility。 |
| 在 v1.1 顺手新增协议/平台能力 | 清理 API 时容易发现缺口 | 会混淆验收：失败时无法区分命名改动还是新协议问题；违反“保留 v1.0 能力，不加新协议/platform” | 新能力进入后续 milestone；v1.1 只做命名、文档、示例、测试和验证。 |
| 把 signaling 纳入核心 API | 示例中已有自动化 signaling，看起来可以产品化 | 项目边界已明确 signaling 属于应用层；核心库内置会扩大职责并绑定应用协议 | 保留 demo/test 的 signaling harness；核心 API 只收/发 SDP 与 ICE candidate 字符串。 |
| 为了“更现代”改线程/回调/ownership 模型 | API 清理时重设计很诱人 | 会引入行为风险，破坏 v1.0 已验证路径；迁移成本远超命名清理 | 只重命名现有回调和生命周期语义；ownership 文档化但不重构模型。 |
| 暴露 RTP/RTCP/SRTP/SDP 内部 helper 作为新 public API | 高级用户可能想复用底层协议工具 | 一旦公开就形成长期兼容负担，且 v1.1 不是协议工具包扩展 | 保持 private；以后若要公开底层模块，单独做 API 设计和稳定性承诺。 |
| 自动生成一大层 deprecated wrappers | 看似专业，能减少破坏 | C 静态库和头文件会出现双命名空间，测试矩阵翻倍；本 milestone 明确允许 breaking cleanup | 若确实需要兼容，放到独立 `compat/aws_kvs_v1.h` 后续评估，v1.1 不默认做。 |
| 重命名 CMake target/package namespace | 名字清理容易扩散到构建接口 | `micrortc::micrortc`、`find_package(micrortc CONFIG REQUIRED)` 已是独立命名，改动只会打断已有 package consumer | 保持 CMake package/target 不变，更新 package consumer 使用新 C API 类型。 |

## Recommended v1.1 Public Naming Shape

| Category | Current v1.0 Style | Recommended v1.1 Style | Testable Rule |
|----------|--------------------|-------------------------|---------------|
| 函数 | `mrtc_peer_connection_create` | 保持 `mrtc_peer_connection_create` | public function regex: `^mrtc_[a-z0-9_]+$` |
| status 类型 | `MRTC_STATUS` | `MrtcStatus` | public typedef/enum CamelCase |
| opaque object | `MRTC_PEER_CONNECTION_HANDLE` | `MrtcPeerConnection *` | public headers 不出现 `*_HANDLE` |
| config/callback 类型 | `MRTC_PEER_CONNECTION_CONFIG` | `MrtcPeerConnectionConfig` | Struct/Typedef CamelCase |
| enum 类型 | `MRTC_MEDIA_KIND` | `MrtcMediaKind` | Enum type CamelCase |
| enum constants | `MRTC_MEDIA_KIND_VIDEO` | 保持/收敛为 `MRTC_MEDIA_KIND_VIDEO` | constants UPPER_CASE with `MRTC_` prefix |
| frame 类型 | `MRTC_FRAME` | `MrtcFrame` | fields remain `data`, `size`, `presentation_ts`, etc. |
| header guard/macros | `MRTC_MICRORTC_H` | 保持 `MRTC_*` | MacroDefinitionCase UPPER_CASE |

## Feature Dependencies

```text
API 命名规则冻结
    -> public header rename
        -> examples/tests/package consumer migration
            -> migration docs verified against real symbols
                -> v1.0 full regression verification

public/private header boundary audit
    -> API surface snapshot
        -> future roadmap can detect accidental public API changes

旧名兼容层
    -conflicts-> AWS/KVS 命名清理目标
```

### Dependency Notes

- **先冻结命名规则，再改头文件：** 否则每个 phase 都可能重新命名，示例和文档会反复返工。
- **示例/测试必须跟随 public header：** 只改库源码不能证明开发者体验；package consumer 是外部用户视角的最低验收。
- **迁移文档依赖最终符号名：** 文档应在 API rename 基本稳定后写，避免映射表过期。
- **回归验证放最后：** 命名清理可能触及大量调用点，最后必须用 v1.0 总验收确认行为不退化。

## MVP Definition

### Launch With (v1.1)

- [ ] 公共命名规范文档化：函数、类型、枚举常量、宏、文件/头文件边界均有明确规则。
- [ ] `include/micrortc/` 不再暴露 AWS/KVS 派生命名或全大写 public typedef 类型。
- [ ] v1.0 所有 public API 调用点迁移到新类型名：PeerConnection、SDP、ICE、DataChannel、transceiver、encoded media frame。
- [ ] README、Chrome E2E 示例、package consumer 和 API contract tests 全部使用新 API。
- [ ] 迁移文档列出旧名 -> 新名映射、include 变化、行为不变项、明确删除项。
- [ ] `ctest`、package consumer、`scripts/verify-v1.sh` 通过；有 TURN 配置时 relay E2E 通过。

### Add After Validation (v1.1.x / 后续小版本)

- [ ] API surface snapshot/report：当 v1.1 命名稳定后再把 public symbol list 固化为回归基线。
- [ ] 可选 ABI/API diff 工具链：如果开始发布 shared library 或承诺 ABI，再引入 ABI compliance checker 类工具。
- [ ] Doxygen/API reference：当前优先迁移文档；自动 API reference 可在 public header 稳定后补。

### Future Consideration (v1.2+)

- [ ] Safari/Firefox 互通矩阵。
- [ ] GStreamer 或其他采集/编码 adapter。
- [ ] 应用层 signaling adapter 或示例 SDK。
- [ ] 更多 codec、simulcast、screen share、recording、SFU/MCU。
- [ ] 底层 RTP/RTCP/SRTP/SDP toolkit public API。

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| 公共 API 命名规范冻结 | HIGH | LOW | P1 |
| public header rename | HIGH | MEDIUM | P1 |
| examples/tests/package consumer 迁移 | HIGH | MEDIUM | P1 |
| 迁移文档 | HIGH | LOW | P1 |
| v1.0 行为回归验证 | HIGH | HIGH | P1 |
| AWS/KVS 公共残留扫描 | MEDIUM | LOW | P1 |
| API surface snapshot | MEDIUM | LOW | P2 |
| Doxygen/API reference | MEDIUM | MEDIUM | P2 |
| 旧名兼容层 | LOW | MEDIUM | P3 / Avoid |
| 新协议/平台能力 | LOW for v1.1 | HIGH | P3 / Out of Scope |

**Priority key:**
- P1: v1.1 必须有，否则 API cleanup 不完整。
- P2: 有价值，但可在核心迁移完成后补。
- P3: 不应进入 v1.1 主线；会制造范围膨胀或违背目标。

## Verification Behavior

| Verification | Expected Behavior | Scope |
|--------------|-------------------|-------|
| Header grep/lint | `include/` public typedef/enum/struct 为 CamelCase；public 函数为 `mrtc_*` lower_case；宏/枚举常量为 `MRTC_*` UPPER_CASE | API naming |
| Forbidden residue scan | public headers、README、examples、tests 不出现 AWS/KVS 产品语义旧 API；合规/来源文档可豁免 | independence |
| Compile contract | `tests/package-consumer` 只 include installed public headers 并使用新 API 编译链接 | developer-facing |
| API behavior tests | PeerConnection/DataChannel/transceiver/frame contract tests 用新 API 通过，错误码行为不变 | behavior preservation |
| Default regression | `scripts/verify-v1.sh` 通过并生成 `build/reports/mrtc-v1-summary.json` | v1.0 capability preservation |
| TURN regression | 有本地 TURN config 时 `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` 证明 relay path 仍通过 | v1.0 relay preservation |

## Sources

- HIGH: `.planning/PROJECT.md` — v1.1 目标、约束、v1.0 已验证能力和 out-of-scope。
- HIGH: `.planning/MILESTONES.md`、`.planning/STATE.md` — v1.0 已交付能力和 v1.1 当前 planning 状态。
- HIGH: `include/micrortc/micrortc.h`、`include/micrortc/peer_connection.h` — 当前 public API 表面。
- HIGH: `.clang-tidy` — 当前命名规则：类型 CamelCase、函数/变量/参数/member lower_case、宏/枚举常量 UPPER_CASE。
- HIGH: `examples/chrome-e2e/mrtc_chrome_answerer.c`、`tests/peer_connection/test_peer_connection_api.c`、`tests/package-consumer/package_consumer.c` — 必须迁移和验证的开发者调用点。
- HIGH: GNU Coding Standards, Library Behavior and Names — C 库外部符号应使用库前缀，函数/变量名应有意义，单词用下划线分隔，宏/枚举常量用大写。https://www.gnu.org/prep/standards/standards.html
- HIGH: LLVM clang-tidy `readability-identifier-naming` — 支持按 function/typedef/enum/macro 等类别配置和检查命名规则。https://clang.llvm.org/extra/clang-tidy/checks/readability/identifier-naming.html
- MEDIUM: Semantic Versioning 2.0.0 — public API 不兼容变化应作为 breaking change 明确沟通；本项目当前版本号策略与 milestone 命名需在文档中消歧。https://semver.org/

---
*Feature research for: libmicrortc v1.1 API 命名/代码风格清理*
*Researched: 2026-05-15*
