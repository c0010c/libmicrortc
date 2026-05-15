# v1.1 API 命名/代码风格清理风险研究

**Domain:** 已工作的 C WebRTC 库破坏式公共 API 命名清理  
**Project:** libmicrortc v1.1 API 清理  
**Researched:** 2026-05-15  
**Confidence:** HIGH，本文件基于当前仓库文档、public header、实现、测试、示例、安装消费路径和 v1 验收脚本检查。

## 建议的 v1.1 阶段切分

后续 roadmap 应把 API 清理拆成可验证的窄阶段，避免一次性全仓改名后只能靠人工排错。

1. **Phase 1: API 盘点与命名契约**
   - 冻结当前 public surface：`include/micrortc/micrortc.h`、`include/micrortc/peer_connection.h`、CMake package target、环境变量、示例 CLI。
   - 产出旧名到新名映射、保留/删除策略、迁移文档草稿和 source provenance 更新策略。

2. **Phase 2: 公共头与下游消费改名**
   - 只改 public headers、实现导出函数、public API 测试、package consumer 和 README/API 文档。
   - 验收必须包含 install/export consumer，而不只跑 build/CTest。

3. **Phase 3: 内部标识符与 private header 风格收敛**
   - 改 `src/` private 类型、宏、helper、测试专用入口。
   - 示例和 E2E harness 不应继续依赖 private struct 字段，除非显式标为测试 hook。

4. **Phase 4: 文档、迁移与溯源收口**
   - 更新 README、迁移指南、SOURCE-MANIFEST 或 v1.1 provenance 文件。
   - 明确 v1.1 是破坏式源码 API 变更，不承诺 v1.0 源码兼容。

5. **Phase 5: v1 能力回归与发布门禁**
   - 跑 `scripts/verify-v1.sh` 和带 TURN 配置的 relay E2E。
   - 产出 `build/reports/mrtc-v1-summary.json`，并补跑 package consumer 和 clang-tidy/命名检查。

## Critical Pitfalls

### Pitfall 1: 破坏式 API 变更没有被显式发布和映射

**What goes wrong:**  
v1.1 允许重命名 public C API，但如果只在代码里直接替换，使用者无法判断哪些旧符号被有意删除、哪些是漏改。当前 public API 覆盖 PeerConnection、DataChannel、transceiver、encoded frame、状态码和回调表，影响面大于普通内部重构。

**Why it happens:**  
v1.0 从 AWS 风格裁剪起步，很多符号同时承担“公共 API”和“内部实现连接点”的角色，例如 `MRTC_PEER_CONNECTION_HANDLE`、`MRTC_FRAME`、`mrtc_transceiver_write_frame()`。执行者容易把命名清理当成全局搜索替换，而不是一次正式的 API 版本变更。

**How to avoid:**  
Phase 1 必须先写旧名到新名映射表，至少覆盖 `include/micrortc/*.h` 中全部 typedef、enum、macro、function、struct field。Phase 2 只接受映射表内的公共改名，所有删除项要写入迁移文档，并在 package consumer 中只使用新 API。

**Warning signs:**  
- PR 里出现 public header 改动，但没有迁移表。
- README 仍列出 `MRTC_*` 旧类型或旧函数名。
- package consumer 还能使用旧名编译，或者新名没有被 consumer 覆盖。

**Phase to address:**  
Phase 1 定义契约；Phase 2 强制 consumer 验证；Phase 4 发布迁移说明。

---

### Pitfall 2: 部分改名导致 old/new 符号混用

**What goes wrong:**  
public header、`src/peer_connection.c`、`src/media/media_transceiver.h`、tests、Chrome answerer 和 package consumer 使用同一批符号。只改 public header 或只改实现会导致编译失败；更隐蔽的是内部私有入口继续暴露旧风格名称，v1.1 看起来完成但代码风格仍不收敛。

**Why it happens:**  
当前 `rg` 显示 `MRTC_*`/`mrtc_*` 引用散布在 `include`、`src`、`tests`、`examples/chrome-e2e`、CMake 和脚本中。示例 `mrtc_chrome_answerer.c` 还包含 `media/media_transceiver.h` 并直接读 `transceiver->kind`，使 public/private 边界更容易被改名影响。

**How to avoid:**  
Phase 1 生成符号清单和禁止残留 grep。Phase 2 后运行旧 public 符号残留检查，Phase 3 后运行 private 符号残留检查。对允许保留的宏前缀、环境变量、CMake option 单独列白名单。

**Warning signs:**  
- `include/` 已是新名，但 `tests/package-consumer/package_consumer.c` 仍用旧名。
- `examples/chrome-e2e/mrtc_chrome_answerer.c` 通过 private header 绕开 public API。
- `.clang-tidy` 命名检查未作为门禁，旧 typedef 仍是全大写。

**Phase to address:**  
Phase 1 清单；Phase 2 public surface；Phase 3 private surface；Phase 5 全仓残留门禁。

---

### Pitfall 3: C 全局命名空间中的宏、enum、typedef 碰撞

**What goes wrong:**  
C 没有 namespace。把 `MRTC_STATUS` 改成 CamelCase、保留旧 alias、同时保留 `MRTC_STATUS_OK` 等 enum 常量，可能引入重复 typedef、重复 enum tag、宏保护失效或下游项目宏名冲突。当前 `MRTC_STATUS` 定义在 `micrortc.h` 和 `peer_connection.h` 中重复出现，并依赖 `MRTC_STATUS_DEFINED` 保护。

**Why it happens:**  
`.clang-tidy` 要求类型 `CamelCase`，enum 常量和宏 `UPPER_CASE`。这意味着 public 类型可能从 `MRTC_STATUS` 变成 `MrtcStatus` 或类似形式，但枚举常量大概率仍需 `MRTC_STATUS_*`。如果为了兼容旧代码同时定义旧 typedef，很容易制造双重定义和 include 顺序差异。

**How to avoid:**  
Phase 1 明确类型名、enum tag、typedef、macro guard 的最终规则。Phase 2 应把 shared status 定义抽到单一 public header 或单一 guarded block，增加“只 include `peer_connection.h`”和“只 include `micrortc.h`”的编译测试。除非 roadmap 明确要求兼容层，否则不要保留旧 typedef alias。

**Warning signs:**  
- 新旧 typedef 同时存在且没有 deprecation 策略。
- `MRTC_STATUS_DEFINED` 这类 guard 被复制出更多变体。
- 只 include 子头文件时和 include umbrella header 时编译结果不同。

**Phase to address:**  
Phase 1 命名契约；Phase 2 header include-order tests；Phase 5 package consumer。

---

### Pitfall 4: ABI/API 边界被误判，只验证源码编译不验证导出消费

**What goes wrong:**  
v1.1 是静态库和 CMake package 的 public API 变更。只跑根工程 `ctest` 可能无法发现安装头、export target、`find_package(micrortc CONFIG REQUIRED)`、include 路径或下游源码迁移失败。当前 README 有 package consumer 命令，但 `scripts/verify-v1.sh` 不执行 install/export consumer。

**Why it happens:**  
根工程测试通常使用 build-tree include 和 target，绕过 installed package。公共头改名后，build-tree 测试可能都已同步更新，但真实下游的 `find_package`、安装后的 header layout 或目标命名仍坏掉。

**How to avoid:**  
Phase 2 必须把 package consumer 升级为 v1.1 API contract test，并在验收命令中执行：

```bash
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
```

Phase 5 应把这组命令纳入 v1.1 总验收脚本，或新增 `scripts/verify-v1.1.sh`。

**Warning signs:**  
- `scripts/verify-v1.sh` 通过，但 install-tree consumer 没跑。
- `tests/package-consumer` 仍使用旧 public API。
- CMake package version 仍是 `0.1.0`，没有标明破坏式 API 变更。

**Phase to address:**  
Phase 2 consumer 更新；Phase 5 总验收脚本。

---

### Pitfall 5: 示例/E2E harness 依赖 private header，API 清理破坏浏览器验收

**What goes wrong:**  
Chrome E2E 的 C answerer 当前 include `<micrortc/micrortc.h>` 之外，还 include `"media/media_transceiver.h"`，并访问 `transceiver->kind` 等 private struct 字段。v1.1 若清理 private struct 命名或隐藏内部字段，E2E harness 会编译失败；若为了 E2E 保留 private 依赖，public API 边界会继续泄漏。

**Why it happens:**  
v1.0 为了快速证明双向媒体和 TURN relay，示例承担了测试 hook、媒体统计和真实 demo 多重职责。它不是纯 public API consumer。

**How to avoid:**  
Phase 2 不要让 public API 改名依赖 private header。Phase 3 应为 E2E 需要的媒体方向/统计提供明确的 test-only helper 或 public inspection API，或者把 answerer 内部统计改为通过回调上下文记录，避免读 transceiver struct 字段。

**Warning signs:**  
- `examples/chrome-e2e/mrtc_chrome_answerer.c` 继续 include `src/` private header。
- E2E 只能通过给示例加 private include path 才能编译。
- public header 看似干净，但示例文档教用户使用 private struct 字段。

**Phase to address:**  
Phase 2 public API consumer；Phase 3 示例/private 边界；Phase 5 Chrome E2E。

---

### Pitfall 6: 命名清理改变协议行为而不是只改 API

**What goes wrong:**  
API 清理阶段顺手重构 PeerConnection 状态机、DTLS/SRTP/SCTP、ICE/TURN、SDP 生成、H264/Opus packetization 或媒体 timestamp，导致 Chrome、DataChannel、TURN relay 或双向媒体回归。最危险的是行为变化被命名 PR 掩盖，review 难以定位。

**Why it happens:**  
当前实现的协议路径集中在 `src/peer_connection.c`，同时连接 public API、SDP、ICE、DTLS、SRTP、SCTP、DataChannel、RTP/RTCP 和媒体回调。全局改名会触碰同一文件的大量代码，容易把“顺手清理”混进来。

**How to avoid:**  
Phase 2 只允许为 public rename 做机械改动。Phase 3 只做命名和 private boundary 收敛，不做协议语义优化。任何状态机、媒体、网络或 SDP 行为变化必须拆成后续 milestone。Phase 5 必须跑 host Chrome E2E 和 TURN relay E2E。

**Warning signs:**  
- API 清理 PR 同时修改 SDP 字符串、payload type、candidate format、DTLS role 或 RTP timestamp 逻辑。
- `build/reports/mrtc-v1-summary.json` 缺少 browser media 或 TURN relay 证据。
- 单元测试通过但 Chrome E2E 未跑。

**Phase to address:**  
所有阶段的 scope control；Phase 5 回归验收。

---

### Pitfall 7: 来源追溯在改名/移动文件时丢失

**What goes wrong:**  
v1.0 已把 AWS/KVS 派生和 reference-only 关系记录在 `.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md`。v1.1 若移动、改名或重写派生文件而不更新 provenance，后续无法判断哪些代码仍来自本地 `reflib/kvs-webrtc-sdk` commit `9eebcc4`，许可证/NOTICE 和审计链断裂。

**Why it happens:**  
命名清理常被误认为“纯机械重命名”，执行者可能只看 git rename，不更新 manifest。当前 README 还提到 `.planning/phases/01-/SOURCE-MANIFEST.md`，但该路径在当前工作区不存在，真实记录在 v1.0 milestone archive 下，这会误导后续维护者。

**How to avoid:**  
Phase 1 决定 v1.1 provenance 位置：要么创建新的 active source manifest，要么明确更新 archived manifest 的延续规则。Phase 4 必须修正 README 中不存在的 `.planning/phases/...` 路径，并为每个被 rename/move 的派生文件记录目标路径变化。

**Warning signs:**  
- 文件移动或改名后，SOURCE-MANIFEST 仍只列旧路径。
- README 指向不存在的 `.planning/phases/01-/SOURCE-MANIFEST.md`。
- 新增“rewritten-derived”代码没有来源路径和 derivation。

**Phase to address:**  
Phase 1 provenance 策略；Phase 4 文档和 manifest 收口。

---

### Pitfall 8: clang-tidy 命名规则存在但没有进入验证闭环

**What goes wrong:**  
`.clang-tidy` 已启用 `readability-identifier-naming`，要求类型 `CamelCase`，函数、变量、参数、member `lower_case`，enum 常量和宏 `UPPER_CASE`。但当前 v1.0 代码大量 private/public typedef 仍是 `MRTC_*` 全大写；如果 v1.1 不把 clang-tidy 作为门禁，风格清理会变成主观 review。

**Why it happens:**  
仓库有配置文件，但未发现总验收脚本执行 clang-tidy。C 项目又常通过宏和 typedef 躲过简单 grep，导致“看起来改了主要 API”，实际还有大量 private 类型不符合规则。

**How to avoid:**  
Phase 1 确定 clang-tidy 的执行方式和白名单。Phase 3 在 private sweep 后运行 clang-tidy 或等价 identifier scan。短期可以先只对 `include/` 和 v1.1 触碰文件启用硬门禁，再扩大到 `src/` 和 `tests/`。

**Warning signs:**  
- `.clang-tidy` 存在，但 roadmap 验收没有 `clang-tidy` 或 identifier scan。
- public API 已改 CamelCase，private headers 仍全是 `MRTC_DTLS_SESSION`、`MRTC_RTP_PACKET` 等 typedef。
- 为了通过检查大量添加 suppression，而不是收敛命名。

**Phase to address:**  
Phase 1 lint 策略；Phase 3 private style；Phase 5 release gate。

---

### Pitfall 9: 外部可见但非 C API 的名称被误删或误改

**What goes wrong:**  
v1.1 聚焦 C public API，但仓库还有外部脚本和运行约定：CMake option `MRTC_BUILD_TESTS`、`MRTC_BUILD_EXAMPLES`、环境变量 `MRTC_E2E_BROWSER_CHANNEL`、`MRTC_E2E_FIXTURES`、`MRTC_ICE_BIND_IP`、`MRTC_ICE_ANNOUNCE_IP`、summary path `build/reports/mrtc-v1-summary.json`、示例命令和 TURN config 文件名。随手改这些名字会破坏 CI、手工验收或用户脚本。

**Why it happens:**  
“清理 MRTC 命名残留”容易被扩大到所有 token。实际上 CMake option、环境变量和脚本参数已经是外部接口，应该按兼容性成本单独决策。

**How to avoid:**  
Phase 1 把 public C API、build API、runtime env API、test harness API 分层列出。v1.1 默认只破坏 C public API；脚本/env/CMake option 保持不变，除非 roadmap 明确列为 breaking change 并同步文档。

**Warning signs:**  
- 改名 PR 修改 `scripts/verify-v1.sh` 参数或环境变量，但迁移文档只讨论 C header。
- 旧的 v1 验收命令无法运行。
- summary JSON schema 字段被改名，导致下游审计脚本失效。

**Phase to address:**  
Phase 1 API 分层；Phase 4 文档；Phase 5 验收脚本。

---

### Pitfall 10: 过度设计兼容层，抵消破坏式清理收益

**What goes wrong:**  
为了减少下游 breakage，保留旧 API alias、旧 macro、旧 typedef、旧 header include，同时新增新 API。结果 public header 同时暴露两套命名，碰撞风险上升，`.clang-tidy` 无法收敛，后续 roadmap 仍背负 AWS/v1.0 风格。

**Why it happens:**  
C API 的兼容层看似便宜：几个 typedef 和 inline wrapper 就能让旧代码编译。但本 milestone 的目标是破坏式命名清理，不是长期源码兼容。

**How to avoid:**  
Phase 1 明确“不保留旧 public alias”是默认策略。若确实需要迁移期 alias，必须限定到单独 `micrortc_compat_v1.h`，默认 umbrella header 不包含，且在同一 milestone 标注删除计划。

**Warning signs:**  
- `include/micrortc/peer_connection.h` 同时出现旧 `MRTC_*` 类型和新 CamelCase 类型。
- 测试既能用旧名也能用新名，但迁移文档没有说明兼容期。
- public header 为了兼容添加大量宏 alias。

**Phase to address:**  
Phase 1 兼容策略；Phase 2 public header；Phase 4 migration guide。

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| 只跑 `scripts/verify-v1.sh`，不跑 package consumer | 验收快 | 安装后的 CMake package/header 可能坏掉 | 不可接受，v1.1 改 public API 必须验证 installed consumer |
| 用宏 alias 保留旧 public API | 下游短期少改代码 | 两套 API 长期共存，命名清理失败，C namespace 碰撞 | 只可放在独立 compat header，且默认不启用 |
| 让 E2E 示例继续 include private header | 快速修复编译 | public/private 边界继续泄漏，后续内部重命名受阻 | 只可作为 Phase 3 临时测试 hook，需明确收口 |
| 把协议行为优化混入改名 PR | 看似一次完成更多清理 | E2E 回归难定位，review 成本高 | 不可接受，协议行为变化应拆 milestone |
| 不更新 provenance，只依赖 git rename | 少写文档 | AWS/KVS 派生链断裂，许可证审计困难 | 不可接受 |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| CMake install/export | 只验证 build-tree target | 安装到 `build/install` 后用 `tests/package-consumer` 验证 `find_package` 和 installed headers |
| Chrome E2E answerer | 继续读 private `MRTC_RTP_TRANSCEIVER` 字段 | 用 public callback/user_data 或明确 test-only helper 替代 private struct access |
| TURN relay E2E | 默认只跑 host E2E | Phase 5 必须同时记录 `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` 的结果 |
| README/API docs | 只替换函数名 | 同步迁移表、示例代码、package consumer 命令、SOURCE-MANIFEST 路径 |
| clang-tidy | 有配置但不执行 | 在 v1.1 验收里加入 include/public touched files 的命名检查 |

## "Looks Done But Isn't" Checklist

- [ ] **Public API 映射:** `include/micrortc/*.h` 每个旧 typedef、enum、macro、function、struct field 都有新名或删除说明。
- [ ] **旧名残留:** `rg` 对旧 public 符号无非白名单残留；白名单只允许迁移文档、release notes 或显式 compat header。
- [ ] **Header 独立性:** 单独 include `<micrortc/micrortc.h>`、单独 include `<micrortc/peer_connection.h>`、二者任意顺序 include 都能编译。
- [ ] **Package consumer:** installed package consumer 使用新 API 编译并运行。
- [ ] **E2E:** host Chrome 和 TURN relay E2E 都通过，summary JSON 保留分层字段。
- [ ] **Private boundary:** 示例和用户文档不要求 include `src/` private headers。
- [ ] **Provenance:** 移动/改名/派生文件的来源记录已更新，README 不再指向不存在的 `.planning/phases/...` manifest。
- [ ] **Scope:** API 清理阶段没有顺手改变 SDP、ICE、DTLS、SRTP、SCTP、RTP/RTCP、H264/Opus 行为。

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| public API 改名无迁移表 | MEDIUM | 暂停继续改名，从 git diff 反推旧/新映射，补迁移文档和 consumer，再恢复执行 |
| old/new 符号混用 | MEDIUM | 先冻结命名表，写残留 grep，按 public -> private -> tests/examples 顺序修复 |
| installed consumer 失败 | LOW/MEDIUM | 先修 installed headers/export target，再把 consumer 命令加入验收脚本 |
| Chrome E2E 因 private API 清理失败 | MEDIUM/HIGH | 判断是测试 hook 缺失还是行为回归；优先移除 answerer private field access，再跑 host/TURN |
| 协议行为回归 | HIGH | 回退非命名行为改动，保留机械 rename；用 v1.0 summary 对照定位 connection/DataChannel/media/TURN 阶段 |
| provenance 丢失 | MEDIUM | 用 git diff 和 v1.0 SOURCE-MANIFEST 重建 rename/move 表，Phase 4 补记录后再发布 |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| 破坏式 API 变更未沟通 | Phase 1, Phase 4 | 旧名/新名迁移表；README migration guide |
| 部分改名不一致 | Phase 1, Phase 2, Phase 3 | public/private 残留 grep；CTest；示例编译 |
| 宏/enum/typedef 碰撞 | Phase 1, Phase 2 | header include-order 编译测试；package consumer |
| package consumer breakage | Phase 2, Phase 5 | install/export consumer 命令必须通过 |
| E2E harness private 依赖 | Phase 3, Phase 5 | answerer 不依赖 private struct 字段；host/TURN E2E 通过 |
| 协议行为回归 | Phase 2-5 | `scripts/verify-v1.sh` 和 TURN relay E2E；summary JSON 检查 |
| 来源追溯丢失 | Phase 1, Phase 4 | v1.1 provenance 文件或 manifest 更新；README 路径修复 |
| clang-tidy 未闭环 | Phase 1, Phase 3, Phase 5 | clang-tidy 或 identifier scan 进入验收 |
| 非 C API 外部接口误改 | Phase 1, Phase 4, Phase 5 | 脚本/env/CMake option 清单；v1 验收命令仍可运行 |
| 兼容层过度设计 | Phase 1, Phase 2 | 默认 public header 不暴露旧 alias；compat header 若存在必须独立 |

## Sources

- `.planning/PROJECT.md` — v1.1 目标、v1.0 已验证能力、约束和 out-of-scope。
- `.planning/STATE.md` — 当前 milestone 为 v1.1 API 清理，尚处 planning。
- `.clang-tidy` — 命名规则：类型 CamelCase，函数/变量/参数/member lower_case，宏和 enum 常量 UPPER_CASE。
- `include/micrortc/micrortc.h` — umbrella public header 和重复 status guard。
- `include/micrortc/peer_connection.h` — 当前 public PeerConnection/DataChannel/transceiver/media API surface。
- `src/peer_connection.c`、`src/media/media_transceiver.h` — public API 与协议实现/private transceiver 边界。
- `examples/chrome-e2e/mrtc_chrome_answerer.c` — Chrome E2E answerer 当前依赖 private media header 和 transceiver 字段。
- `tests/package-consumer/`、`cmake/MicroRtcInstall.cmake` — install/export package consumer 验证路径。
- `scripts/verify-v1.sh` — 当前总验收覆盖 build、CTest、Chrome host、可选 TURN，但不覆盖 installed package consumer。
- `.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md` — AWS/KVS 派生和 reference-only 来源追溯规范及现有记录。

---

*Pitfalls research for: libmicrortc v1.1 API 命名/代码风格清理*  
*Researched: 2026-05-15*
