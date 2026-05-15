# libmicrortc v1.1 API Boundary Contract

## 目标

本文冻结 libmicrortc v1.1 的 public、private、test-only/demo-only API 边界，供 Phase 8+ 做机械改名、残留扫描和公开调用点迁移时直接消费。

v1.1 的命名目标如下：

| 对象 | v1.1 规则 | 说明 |
|------|-----------|------|
| Public C 函数 | `rtc_*` | 例如 `rtc_peer_connection_create` |
| Public 类型、typedef、enum type | `Rtc*` | 例如 `RtcPeerConnection`、`RtcStatus` |
| Public 宏和 enum constants | `RTC_*` | 例如 `RTC_STATUS_OK` |
| CMake package/target | `micrortc::micrortc` | 保持不改 |

旧 `mrtc_*`、`MRTC_*`、AWS/KVS 风格 public names 是迁移和删除目标。v1.1 默认不保留旧 public API 兼容层。

## Canonical Sources

| 来源 | 用途 | 边界结论 |
|------|------|----------|
| `include/micrortc/micrortc.h` | Umbrella public header 和 core/status API 来源 | Public API source |
| `include/micrortc/peer_connection.h` | PeerConnection、DataChannel、transceiver 和 media frame public API 来源 | Public API source |
| `cmake/MicroRtcInstall.cmake` | `install(DIRECTORY include/ ...)` 和 `install(EXPORT ... NAMESPACE micrortc:: ...)` | Installed package boundary |
| `CMakeLists.txt` | `target_include_directories(micrortc PUBLIC include/ PRIVATE src/)` | Build public/private include boundary |
| `cmake/MicroRtcTests.cmake` | `WITH_PRIVATE_INCLUDES` 和 Chrome answerer private include 设置 | Test-only/demo-only boundary |
| `examples/chrome-e2e/mrtc_chrome_answerer.c` | Chrome E2E answerer 使用 public header 加 private media helper | Demo harness, not public API |
| `.planning/REQUIREMENTS.md` | API-01、API-02、API-04、STYLE-04 | v1.1 requirement source |
| `.planning/STATE.md` | `rtc_*` / `Rtc*` / `RTC_*` 和 no-wrapper 决策 | Naming decision source |
| `.planning/phases/07-api/07-RESEARCH.md` | Public/private/test-only 盘点和残留分类研究 | Phase 7 research source |
| `.planning/phases/07-api/07-PATTERNS.md` | 文档结构、残留扫描和报告字段模式 | Phase 7 pattern source |

本阶段不把 Doxygen、API snapshot 或 ABI diff 作为交付物；这些属于 `.planning/REQUIREMENTS.md` 中已延期的 FUT-01、FUT-02、FUT-03。

## Public API Boundary

Public API Boundary 只包含通过安装树和 CMake package 暴露给下游项目的接口。

| Public 面 | 包含内容 | 不包含内容 | 依据 |
|-----------|----------|------------|------|
| Installed headers | `include/micrortc/*.h`，当前准确文件为 `include/micrortc/micrortc.h` 和 `include/micrortc/peer_connection.h` | `src/**/*.h`、测试相对路径 include、E2E private helper | `cmake/MicroRtcInstall.cmake` 安装 `DIRECTORY include/` |
| CMake target | `micrortc::micrortc` | 任何 `mrtc::*`、`rtc::*` 或其他新 namespace | `CMakeLists.txt` 创建 alias，install export 使用 `NAMESPACE micrortc::` |
| Build include dirs | 下游通过 package 或 build interface 获取 `include/` | 下游不获得 `src/` | `target_include_directories(micrortc PUBLIC include/ PRIVATE src/)` |
| Public symbol inventory | `include/micrortc/micrortc.h`、`include/micrortc/peer_connection.h` 中声明的 public typedef、enum、macro、function | private helper、internal struct field、test fixture helper、script/env var | API-02 限定 installed `include/micrortc/*.h` |

Phase 8+ 判断 public API 时必须先看 installed `include/micrortc/*.h`。如果一个 symbol 只出现在 `src/`、`tests/`、`examples/` 或 `.planning/`，它不是 public C API，不能因为测试或 demo 依赖它而自动提升为 public header。

Public API 的使用者仍通过 `find_package(micrortc CONFIG REQUIRED)` 和 `target_link_libraries(... PRIVATE micrortc::micrortc)` 获取库。v1.1 的破坏式清理只针对 C public names，不规划 CMake package/target namespace 改名。

## Private API Boundary

Private API Boundary 是库内部实现面，主要位于 `src/`。这些文件可以被 `micrortc` target 自身包含，也可以被 white-box 测试在受控场景下包含，但不进入 installed public API。

| Private 区域 | 代表文件 | 分类理由 |
|--------------|----------|----------|
| SDP | `src/sdp.h` | SDP parser/serializer 是协议实现细节；测试可 white-box 验证，但下游不直接 include |
| ICE/TURN/STUN | `src/ice/ice_agent.h`、`src/ice/ice_config.h`、`src/turn/turn_client.h`、`src/stun/stun_message.h` | transport negotiation 和 TURN/STUN 编解码属于内部传输层 |
| DTLS/SRTP/SCTP | `src/dtls/dtls_session.h`、`src/srtp/srtp_session.h`、`src/sctp/sctp_session.h` | 安全和 DataChannel 底层 session 不暴露为 public C API |
| RTP/RTCP/media | `src/rtp/**`、`src/rtcp/**`、`src/media/media_transceiver.h` | packetizer、retransmit、protected packet capture 等是媒体路径实现细节 |
| Common internals | `src/common/mrtc_common.h`、`src/common/mrtc_mutex.h`、`src/common/mrtc_socket.h` | 平台、锁、socket 和内部通用工具不进入 installed include |

Private header 中的 `mrtc_*`、`MRTC_*` 或 AWS/KVS 残留不由本计划直接改名。后续 Phase 10 会按 `.clang-tidy` 和 private boundary 目标处理内部命名。本计划只要求残留扫描能把 private/internal 命名与 public forbidden residual 区分开。

## Test-only / Demo-only Boundary

Test-only / Demo-only Boundary 覆盖为了自动化验证、white-box 覆盖或 Chrome E2E harness 存在的接口使用方式。它们可以依赖 private header，但不能被文档描述成普通下游 API。

| Test/demo 面 | 允许访问 | 限制 | 依据 |
|--------------|----------|------|------|
| C white-box tests | 通过 `WITH_PRIVATE_INCLUDES` 获取 `src/` include dir | 只能用于测试 target，不改变 installed package | `cmake/MicroRtcTests.cmake` |
| Media/integration tests | RTP/RTCP/media/private transport helper | 只证明内部行为，不定义 public C API | tests target 使用 `WITH_PRIVATE_INCLUDES` |
| Chrome answerer demo | `examples/chrome-e2e/mrtc_chrome_answerer.c` include `media/media_transceiver.h` | 这是 E2E harness 的私有发送/捕获需求，不是用户 API 教程 | `mrtc_chrome_answerer` target PRIVATE include `src` |
| Node/Playwright harness | `tests/e2e/*`、E2E JSON 消息、fixture 路径 | 不属于核心库 ABI/API，不进入 symbol map | E2E 作为验证工具 |
| Test env vars | `MRTC_E2E_*` 等脚本环境变量 | 属于 harness 配置，不是 public C symbol；是否改名留给 Phase 10/11 | Phase 7 STYLE-04 分类要求 |

示例和 E2E harness 如果临时展示 private struct 字段或 private include，必须标记为 `test_harness_non_public_api`。Phase 10 的目标是进一步收敛这些展示方式，但 Phase 7 不把它们提升为 public API。

## Deletion Policy

Deletion Policy 是 v1.1 public API 清理的硬约束：

| 旧名称类型 | v1.1 处理 | 禁止做法 |
|------------|-----------|----------|
| `mrtc_*` public functions | 重命名为 `rtc_*`，删除旧函数名 | 不提供 compatibility wrapper，不提供旧函数 inline wrapper |
| `MRTC_*` public typedef/type/enum type | 重命名为 `Rtc*`，删除旧 typedef/type 名 | 不提供旧 typedef alias |
| `MRTC_*` public macros/enum constants | 重命名为 `RTC_*`，删除旧宏/常量名 | 不提供 macro alias |
| AWS/KVS 风格 public names | 默认删除或改为 libmicrortc 中性名称 | 不保留 AWS/KVS public compatibility surface |
| Header guards/helper macros | 统一到 `RTC_*`，避免 installed headers 留旧 `MRTC_` | 不保留旧 guard/helper alias |

“删除旧名”表示 installed public headers 中不再声明旧名；仓库内迁移文档、source manifest、合规记录可以继续提及旧名作为历史或映射事实。后续 Phase 8+ 应迁移调用点，而不是通过兼容 wrapper、macro alias 或旧 typedef alias 让旧调用继续编译。

如果某个旧名必须临时存在，必须先有新的计划或需求显式改变 API-04；不能在机械改名中以“方便测试通过”为理由加入兼容层。

## Residual Classification Contract

Residual Classification Contract 为 STYLE-04 和 Wave 2 residual scan 定义分类枚举。扫描目标是分类旧 AWS/KVS、`mrtc_*`、`MRTC_*` residual，而不是简单计数。

### Classification Values

| Category | 含义 | 默认状态 | 必填 reason 示例 |
|----------|------|----------|------------------|
| `forbidden_public_residual` | installed public headers 或公开示例/README 中仍出现旧 public C API residual | 失败 | `installed public header must use rtc_*/Rtc*/RTC_* in v1.1` |
| `migration_doc_whitelist` | 迁移指南、symbol map、boundary doc 中为了说明旧名到新名映射而提及旧名 | 允许 | `migration document records old-to-new API mapping` |
| `source_compliance_record` | `.planning/milestones/**`、SOURCE-MANIFEST、COMPLIANCE、LICENSE/NOTICE 类记录中出现 AWS/KVS 或旧名 | 允许 | `provenance/compliance record must preserve source naming` |
| `test_harness_non_public_api` | tests、E2E harness、脚本环境变量或 demo harness 中出现非 public C API residual | 允许但需记录 | `test harness residual is outside installed public API` |
| `ignored_generated_artifact` | `build/`、`build-*`、`cmake-build-debug/`、E2E artifacts、node_modules 等生成物或外部依赖命中 | 默认忽略 | `generated artifact excluded from source residual policy` |

Wave 2 脚本可以增加更细粒度的内部分类，但不得删除上表五个基础值。

### Finding Required Fields

每条 residual finding 至少包含：

| 字段 | 要求 |
|------|------|
| `path` | repo-relative path；不要输出绝对路径里的本地用户名或 secret 位置 |
| `line` | 匹配行号 |
| `symbol` | 匹配 token，例如 `mrtc_peer_connection_create`、`MRTC_STATUS`、`AWS`、`KVS` |
| `category` | 上表中的分类值 |
| `reason` | 白名单或失败原因，必须足够具体 |

允许的报告信息只包含 path/line/symbol/category/reason 以及统计字段。报告不得读取、打印或提交 `mrtc-ice-servers.local.json` 内容；该文件是本地 TURN secret 配置，即使路径里包含 `mrtc` 也只能按本地运行时配置处理，不能输出 `username`、`credential`、`password` 字段值。

### Script Safety Requirements

Plan 02 的 `scripts/scan-api-residuals.sh` 必须满足以下安全要求：

| 要求 | 说明 |
|------|------|
| Quote variables | 所有 path、format、output 参数都要引用，避免空格或特殊字符破坏命令 |
| Reject unknown options | 未知参数必须 `exit 2`，不能静默忽略 |
| Avoid `eval` | 不用 `eval` 拼接执行用户输入 |
| Normalize repo-relative paths | output path、symbol map path 等必须归一到 repo 内或明确拒绝不安全位置 |
| Do not read local secrets | 不读取 `mrtc-ice-servers.local.json` 内容；只允许记录路径级别分类 |
| Exclude generated artifacts | 默认排除 `reflib/**`、`build/**`、`build-*/**`、`cmake-build-debug/**`、`tests/e2e/node_modules/**` 和 E2E artifact 目录 |

### Scan Context Rules

| 路径/上下文 | 旧名命中默认分类 |
|-------------|------------------|
| `include/micrortc/*.h` | `forbidden_public_residual` after Phase 8；Phase 7 当前状态可作为 baseline 事实 |
| `docs/api-v1.1-symbol-map.md` | `migration_doc_whitelist` |
| `docs/api-v1.1-boundary.md` | `migration_doc_whitelist` when mentioning old names by policy |
| `.planning/milestones/**/COMPLIANCE.md`、`.planning/milestones/**/SOURCE-MANIFEST.md` | `source_compliance_record` |
| `tests/**`、`examples/chrome-e2e/**`、`scripts/verify-v1.sh` | `test_harness_non_public_api` unless they present old names as public user API |
| `reflib/**`、`build/**`、`build-*/**`、`cmake-build-debug/**` | `ignored_generated_artifact` |

## Phase 8+ 使用说明

1. 改 public headers 前，先使用 `docs/api-v1.1-symbol-map.md` 作为旧名到新名事实来源。
2. public header 改名后，`include/micrortc/*.h` 不应再出现旧 `mrtc_*`、`MRTC_*` public names。
3. 调用点迁移必须迁移到新 names；不能通过 compatibility wrapper、macro alias 或旧 typedef alias 绕过 API-04。
4. private header 和 test harness 残留应按分类记录，不能因 white-box test include 而扩大 public API。
5. 新增 public symbol 时必须先判断是否位于 installed `include/micrortc/*.h`，并遵守 `rtc_*`、`Rtc*`、`RTC_*` 命名规则。
6. v1 行为回归仍以 `scripts/verify-v1.sh`、CTest、Chrome E2E 和 TURN relay 验收为核心，不由本边界文档替代。

## 检查清单

| 检查项 | 期望 |
|--------|------|
| Public API source | 只看 installed `include/micrortc/*.h` 和 package target `micrortc::micrortc` |
| Private implementation | `src/` 仍是 private include，不进入 install |
| Test-only include | `WITH_PRIVATE_INCLUDES` 和 Chrome answerer private include 只用于 harness |
| Old public names | 默认删除旧 `mrtc_*`、`MRTC_*`、AWS/KVS public names |
| Compatibility layer | 不提供兼容 wrapper、macro alias、typedef alias |
| Residual finding | 必须包含 category、matched path、matched symbol、whitelist/source reason |
| Local TURN config | `mrtc-ice-servers.local.json` 是 local secret，不读取、不打印、不提交 |
