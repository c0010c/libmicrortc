# libmicrortc v1.1 研究综合

**Project:** libmicrortc  
**Domain:** C WebRTC 协议栈公共 API 命名清理与 `.clang-tidy` 风格收敛  
**Researched:** 2026-05-15  
**Confidence:** HIGH

## Executive Summary

v1.1 应定位为一次有边界的破坏式 API 清理，而不是协议能力扩展。libmicrortc 已在 v1.0 验证 Chrome、DataChannel、TURN relay、H264/Opus 双向媒体和自动化 E2E；本里程碑的价值是把继承自 AWS/KVS 风格的公共 C API 收敛为独立 libmicrortc 风格，并让测试、示例、文档和安装消费路径全部跟随新 API。

推荐策略是：公共函数继续保留 `mrtc_` + `lower_case`；公共类型、typedef、enum 类型改为 `Mrtc*` CamelCase；宏和枚举常量继续使用 `MRTC_*` UPPER_CASE；默认不保留旧 `MRTC_*` public typedef/handle 兼容别名。新增工作应集中在 clang-tools 门禁、API 迁移映射、旧符号残留扫描、package consumer 和 v1.0 行为回归，而不是引入 signaling、媒体采集/编码、新 codec、新浏览器或协议重构。

主要风险是一次性全仓机械替换导致 public/private 边界混乱、install/export consumer 漏验、E2E harness 依赖 private header、以及命名清理混入协议行为变化。路线图应按“先冻结契约，再改公共面，再收敛内部和示例，最后文档与全量验收”的顺序拆分，每个阶段都有可机器验证的门禁。

## Key Findings

### Stack 与工具补充

保留现有 C99 + CMake 静态库结构，不提高 `cmake_minimum_required(VERSION 3.16)`，不新增 C 运行时依赖。v1.1 需要把 LLVM clang-tools 和少量仓库脚本纳入开发/CI 验证闭环。

**核心工具：**
- CMake 3.16：继续负责静态库、测试、install/export package；启用 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`。
- C99：保持现有语言边界，API 清理不同时改变 ABI/并发/所有权模型。
- LLVM clang-tools：固定 CI 版本，使用 `clang-format`、`clang-tidy`、可选 `run-clang-tidy`。
- Python 3.8+ 标准库：生成 API 迁移映射、扫描旧 public symbols，不引入 libclang/PyYAML。
- Node + Playwright：保持现有 Chrome E2E 栈，不因 v1.1 扩展浏览器矩阵。

**建议新增或强化：**
- `scripts/format.sh`：基于现有 `.clang-format` 做 `--check` / `--fix`，支持 `CLANG_FORMAT` 环境变量和带版本后缀工具名。
- `scripts/tidy.sh`：基于 fresh compile database 跑 `.clang-tidy`，覆盖 `include/`、`src/`、`tests/`、`examples/`。
- `.clang-tidy` 的 `HeaderFilterRegex` 调整为可匹配绝对路径，并纳入 `examples/`。
- API 映射源文件和生成脚本，例如 `docs/api-renames-v1.1.tsv`、`docs/API-MIGRATION-v1.1.md`、`build/reports/mrtc-api-migration-map.json`。
- 旧符号残留扫描脚本，排除 `reflib/`、`build/`、`node_modules/`、历史 milestone archive。

### Feature Table Stakes

**v1.1 必须具备：**
- 公共 API 命名规范冻结：函数 `mrtc_*`；类型 `MrtcStatus`、`MrtcPeerConnectionConfig`、`MrtcFrame`；宏/枚举常量 `MRTC_*`。
- 公共头文件收敛：`include/micrortc/` 不再暴露 AWS/KVS 派生命名或全大写 public typedef 类型。
- opaque object 清理：优先使用 `typedef struct MrtcPeerConnection MrtcPeerConnection;` + `MrtcPeerConnection *`，而不是 `*_HANDLE` 风格。
- 示例、README、API contract tests、`tests/package-consumer` 全量迁移到新 API。
- 迁移文档列出旧名到新名映射、行为不变项、删除项和破坏式变更说明。
- v1.0 行为不退化：`ctest`、package consumer、`scripts/verify-v1.sh` 和可选 TURN relay E2E 通过。

**有价值但可后置：**
- API surface snapshot/report，作为后续公共 API 变更基线。
- Doxygen/API reference，等 public header 稳定后补。
- ABI diff 工具，仅在开始发布 shared library 或承诺 ABI 后引入。

**明确不进入 v1.1：**
- 新协议功能、Safari/Firefox 矩阵、GStreamer/FFmpeg/采集编码 adapter。
- 核心库 signaling adapter 或应用层 signaling。
- RTP/RTCP/SRTP/SDP 内部 helper public API。
- 默认旧 AWS/KVS 兼容层、自动 wrapper 或双 API 命名空间。
- CMake target/package namespace 改名；保留 `micrortc::micrortc`。

### Architecture Integration Points

现有架构已有明确安装边界：`include/micrortc/*.h`、`micrortc` target、`micrortc::micrortc` export、README 调用模型和 `tests/package-consumer` 是下游可见面。`src/` 下 ICE/STUN/TURN、DTLS/SRTP/SCTP、DataChannel、RTP/RTCP/media、SDP 等模块应继续作为内部实现。

**重点集成点：**
1. 公共 API 层：`include/micrortc/micrortc.h`、`include/micrortc/peer_connection.h` 必改；函数名大多可保留，类型名是核心改动。
2. 核心编排层：`src/micrortc.c`、`src/peer_connection.c` 签名和 public 类型同步，但不改 SDP/ICE/DTLS/SRTP/SCTP/RTP 行为。
3. package/export：安装后 consumer 必须使用新 API 构建运行，不能只验证 build-tree。
4. 测试与示例：public-only tests 先迁移；white-box tests 和 Chrome E2E answerer 后迁移，并避免示例继续教用户 include private header。
5. 文档与溯源：README、迁移指南、SOURCE-MANIFEST/provenance 路径需要收口，尤其是文件 rename/move 后的 AWS/KVS 来源记录。

### Watch-outs / Pitfalls

1. **没有迁移表就改 public API**：先冻结旧名到新名映射，删除项必须进入迁移文档。
2. **old/new 符号混用**：阶段性运行 public/private 残留扫描，白名单只允许迁移文档、release notes 或显式 compat 草案。
3. **C 命名空间碰撞**：避免旧 typedef alias 与新 CamelCase 类型并存；status 类型应集中定义并覆盖 include-order 编译测试。
4. **只跑根工程测试，不跑 installed consumer**：v1.1 是 public API 变更，必须验证 `cmake --install` 后的 `find_package(micrortc CONFIG REQUIRED)`。
5. **E2E 示例依赖 private struct 字段**：Chrome answerer 中的 private media hook 要么明确 test-only，要么通过 public callback/user_data 记录状态。
6. **命名清理混入协议行为变化**：不得顺手改 SDP、ICE candidate、DTLS role、RTP timestamp、payload type、TURN relay 或媒体路径逻辑。
7. **clang-tidy 只配置不执行**：`.clang-tidy` 必须进入 v1.1 验证闭环，否则风格收敛不可验证。
8. **误改非 C API 外部接口**：CMake options、环境变量、`scripts/verify-v1.sh` 参数和 summary JSON 路径默认保持不变。

## Implications for Roadmap

### Phase 1: API 盘点与命名契约冻结

**Rationale:** 先定义 public/private/private-test-only 边界，避免后续阶段反复改名或误删外部接口。  
**Delivers:** public API inventory、旧名到新名映射、保留/删除策略、clang-format/tidy 执行策略、残留扫描白名单、provenance 更新策略。  
**Addresses:** 公共 API 命名规范冻结、破坏式变更沟通、外部接口分层。  
**Avoids:** 无迁移表改 API、old/new 混用、误改 CMake/env/script 接口、来源追溯丢失。

### Phase 2: 公共头与核心 PeerConnection API 改名

**Rationale:** 最大用户可见风险在 `include/` 和核心 public signatures，必须先让 public-only 编译失败尽早暴露。  
**Delivers:** `include/micrortc/*.h` 新类型名、`src/micrortc.c`/`src/peer_connection.c` 签名同步、header include-order 编译测试、public API contract tests 初步迁移。  
**Addresses:** public typedef/enum/opaque handle 清理、宏/enum/typedef 碰撞处理。  
**Avoids:** 旧 public typedef alias 进入最终 API、只改实现不改头、include 顺序差异。

### Phase 3: Package Consumer 与公共调用点迁移

**Rationale:** 下游真实体验不是 build-tree 测试，而是安装后通过 CMake package 使用新 API。  
**Delivers:** `tests/package-consumer`、README 代码片段、public-only tests、smoke tests 使用新 API；install/export consumer 构建运行。  
**Addresses:** 示例/测试可用性、迁移表可验证性、CMake package 保持兼容。  
**Avoids:** `scripts/verify-v1.sh` 通过但 installed consumer 坏掉。

### Phase 4: 内部标识符与 private boundary 收敛

**Rationale:** public API 干净后，再按模块收敛 `src/` 和 white-box tests，降低协议行为回归定位成本。  
**Delivers:** common、SDP、ICE/STUN/TURN、DTLS/SRTP/SCTP/DataChannel、RTP/RTCP/media 的类型命名收敛；white-box tests 更新；E2E answerer private 依赖收口。  
**Addresses:** `.clang-tidy` 类型命名、AWS/KVS 内部命名残留、示例/private 边界。  
**Avoids:** 全仓机械替换、把 internal helper 提升为 public API、示例继续暴露 private struct 字段。

### Phase 5: 文档、迁移指南与发布门禁

**Rationale:** 最后统一文档和验收证据，证明“名字干净”和“v1.0 行为没坏”同时成立。  
**Delivers:** README、`docs/API-MIGRATION-v1.1.md`、source/provenance 更新、残留扫描报告、format/tidy 通过、package consumer、CTest、Chrome host E2E、可选 TURN relay E2E。  
**Addresses:** breaking change 发布说明、来源追溯、风格门禁、v1.0 能力回归。  
**Avoids:** 文档仍列旧 API、SOURCE-MANIFEST 路径失效、协议行为回归未被 E2E 捕获。

### Phase Ordering Rationale

- 命名契约必须先于代码改动，否则 public header、tests、docs 会反复返工。
- public surface 先于 private sweep，可把错误限定在用户可见 API 和编译边界。
- package consumer 必须早于最终 E2E，避免安装/导出问题被内部测试掩盖。
- internal/style sweep 放在 public API 稳定后，减少把协议重构混进命名清理的概率。
- 文档和全量验收放最后，但迁移映射草稿要在 Phase 1 就存在。

### Verification Gates

每个阶段应有最小门禁：

- Phase 1：public inventory 和 rename map 完成；旧 public 符号 `rg` 结果已分类。
- Phase 2：`cmake -S . -B build -DMRTC_BUILD_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 与 `cmake --build build` 通过；header include-order tests 通过。
- Phase 3：`cmake --install build --prefix build/install`；`tests/package-consumer` 用新 API configure/build/run 通过。
- Phase 4：`ctest --test-dir build --output-on-failure` 通过；private 残留扫描结果可解释；`clang-tidy` 或 identifier scan 进入门禁。
- Phase 5：`scripts/format.sh --check`、`scripts/tidy.sh`、API migration check、package consumer、`ctest`、`scripts/verify-v1.sh` 全部通过；有 TURN 配置时执行 `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json`。

### Research Flags

**需要更深阶段研究：**
- Phase 1：需要精确盘点当前 public/private/test-only 符号和 provenance 位置，避免误分类。
- Phase 4：E2E answerer private media hook 如何收口需要看具体代码，可能需要小型设计判断。
- Phase 5：若要新增统一 `scripts/verify-v1.1.sh`，需要确认 CI 约束和本机 clang-tools 可用性。

**可按标准模式执行：**
- Phase 2：公共头和签名改名是明确的 C API rename 工作，按映射表实施即可。
- Phase 3：package consumer、README、public tests 迁移模式清晰。
- 格式检查脚本：直接基于 `.clang-format`、`.clang-tidy`、compile database 和工具环境变量覆盖。

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | 基于现有 CMake/C99 项目结构、官方 LLVM/CMake 文档和本地工具约束；不需要引入新运行时依赖。 |
| Features | HIGH | 与 `.planning/PROJECT.md` 的 v1.1 active goals 和 out-of-scope 完全一致，并核对了 public headers、tests、examples。 |
| Architecture | HIGH | 基于本地 `include/`、`src/`、`tests/`、`examples/`、CMake install/export 和 `scripts/verify-v1.sh`。 |
| Pitfalls | HIGH | 风险来自当前仓库实际边界：private header 泄漏、package consumer 缺口、clang-tidy 未闭环、provenance 路径问题。 |

**Overall confidence:** HIGH

### Gaps to Address

- 最终 public rename map 尚未冻结：Phase 1 必须逐符号确认，不应靠全文替换推断。
- `clang-tidy`/`clang-format` 本机命令名不稳定：脚本必须支持 `CLANG_TIDY`、`CLANG_FORMAT`、`RUN_CLANG_TIDY` 覆盖。
- Chrome E2E answerer 当前 private 依赖需要代码级评估：Phase 4 决定 test-only helper 或 callback/user_data 收口方式。
- provenance 更新位置需确认：Phase 1 决定新增 v1.1 manifest 还是延续 v1.0 archive，并修正 README 中失效路径。
- TURN relay 验证依赖本地配置：Phase 5 将默认 host E2E 设为必跑，TURN relay 在存在 `mrtc-ice-servers.local.json` 时作为强回归门禁。

## Sources

### Primary

- `.planning/PROJECT.md` — v1.1 目标、约束、v1.0 验收能力和 out-of-scope。
- `.planning/research/STACK.md` — clang-tools、CMake、Python 脚本和验证流水线建议。
- `.planning/research/FEATURES.md` — table stakes、differentiators、anti-features 和 MVP 定义。
- `.planning/research/ARCHITECTURE.md` — public/private 边界、构建测试流和推荐实施顺序。
- `.planning/research/PITFALLS.md` — 关键风险、阶段映射和完成度检查清单。
- 本地仓库文件：`.clang-tidy`、`include/micrortc/*.h`、`src/`、`tests/`、`examples/chrome-e2e/`、`cmake/`、`scripts/verify-v1.sh`。

### External / Official

- LLVM clang-tidy 文档 — compile database、`run-clang-tidy`、`readability-identifier-naming`。
- LLVM clang-format 文档 — `--style=file`、`--dry-run`、`--Werror`、`-i`。
- CMake 文档 — `CMAKE_EXPORT_COMPILE_COMMANDS`、`<LANG>_CLANG_TIDY`、target property 集成方式。
- GNU Coding Standards — C 库外部符号命名应有库前缀，宏/常量大写。
- Semantic Versioning 2.0.0 — public API breaking changes 需要明确沟通。

---
*Research completed: 2026-05-15*  
*Ready for roadmap: yes*
