# Stack Research

**Domain:** C/CMake 项目的破坏式公共 API 命名清理与 `.clang-tidy` 风格收敛  
**Project:** libmicrortc v1.1 API 清理  
**Researched:** 2026-05-15  
**Confidence:** HIGH

## 结论

v1.1 不需要更换核心构建栈，也不需要引入新的 C 运行时依赖。推荐保留当前 C99 + CMake 静态库结构，把新增工作限制在 LLVM clang-tools、少量仓库脚本、迁移映射文件和 CI 验证顺序上。

最重要的工具链变化是：把 `clang-format` 和 `clang-tidy` 变成显式开发/CI 依赖；用 `CMAKE_EXPORT_COMPILE_COMMANDS=ON` 生成编译数据库；用仓库脚本统一格式检查、tidy 检查、API 迁移映射生成和旧符号残留扫描。当前本机环境中 `clang-tidy` 不在 PATH，`clang-format` 命令被 Chromium 包装器劫持，因此脚本必须支持 `CLANG_TIDY`、`CLANG_FORMAT` 环境变量和带版本后缀的工具名。

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| CMake | 保持 `cmake_minimum_required(VERSION 3.16)` | 构建静态库、测试、install/export package | 现有项目已经稳定使用 CMake；CMake 3.16 足够支持 `C_CLANG_TIDY` target property 和 `CMAKE_EXPORT_COMPILE_COMMANDS`，无需为了 v1.1 抬高最低版本。 |
| C | C99 | 核心库语言标准 | 现有 `micrortc` target 已设置 `C_STANDARD 99`、`C_EXTENSIONS OFF`；API 命名清理不应同时改变语言边界。 |
| LLVM clang-tools | 推荐 LLVM 18+，CI 固定一个具体版本 | `clang-format`、`clang-tidy`、`run-clang-tidy` | `.clang-tidy` 已经是项目风格来源；LLVM 官方文档确认 `clang-tidy` 可基于编译数据库运行，`readability-identifier-naming` 可约束 typedef、enum、function、parameter、member、macro 等命名。 |
| Python | 3.8+，仅标准库 | 生成迁移映射、扫描旧 API 残留 | 仓库已有 Bash/Node/CMake，但 API rename map 更适合用 Python 做结构化文本处理；避免引入 PyYAML、libclang Python binding 或自定义 clang 插件。 |
| Node + Playwright | 保持当前 `@playwright/test 1.60.0`、`ws 8.20.1` | 继续跑 Chrome E2E | v1.1 目标是 API/风格，不改变浏览器验收栈；`scripts/verify-v1.sh` 必须继续作为行为回归入口。 |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `clang-format` | 代码布局检查/修复 | 使用现有 `.clang-format`，只管布局；命名规则交给 `.clang-tidy`。脚本应优先 `${CLANG_FORMAT}`，再查找 `clang-format-18`、`clang-format`。 |
| `clang-tidy` | 静态分析和命名规则检查 | 使用现有 `.clang-tidy`；v1.1 应把 `include/`、`src/`、`tests/`、`examples/` 都纳入检查输入。 |
| `run-clang-tidy` | 并行跑全仓 C translation units | 适合 CI 和本地全量检查；没有该工具时脚本可回退到 `git ls-files '*.c' | xargs clang-tidy -p build`。 |
| `git ls-files` | 稳定列出受版本控制的 C/H 文件 | 避免扫进 `tests/e2e/node_modules/`、`build/`、E2E artifacts。 |
| `scripts/verify-v1.sh` | v1.0 行为能力回归 | 保留原职责：build、CTest、Chrome host E2E、可选 TURN relay E2E；不要把 lint 塞进它导致行为验收和风格验收耦合。 |

## Recommended Tooling Changes

### 1. 生成 fresh compile database

推荐所有 lint/tidy 前先重新 configure，避免现有 `build/compile_commands.json` 残留旧文件名：

```bash
cmake -S . -B build \
  -DMRTC_BUILD_TESTS=ON \
  -DMRTC_BUILD_EXAMPLES=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

`MRTC_BUILD_EXAMPLES=ON` 对 v1.1 很重要，因为 demo 也要迁移到新 API。CMake 官方文档确认 `CMAKE_EXPORT_COMPILE_COMMANDS` 会生成每个 translation unit 的真实编译命令，clang-tidy 官方文档也推荐基于 compilation database 运行。

### 2. 新增格式脚本

推荐新增 `scripts/format.sh`：

```bash
# 检查
CLANG_FORMAT=clang-format-18 scripts/format.sh --check

# 修复
CLANG_FORMAT=clang-format-18 scripts/format.sh --fix
```

脚本内部建议：

```bash
files="$(git ls-files '*.c' '*.h' ':!:tests/e2e/node_modules/**')"
"${CLANG_FORMAT:-clang-format}" --style=file --dry-run --Werror $files
"${CLANG_FORMAT:-clang-format}" --style=file -i $files
```

注意：本机当前 `clang-format` 命令不可用，报 Chromium checkout 相关错误；所以不要在脚本中硬编码裸 `clang-format`，必须允许 `CLANG_FORMAT=clang-format-18` 这类覆盖。

### 3. 新增 clang-tidy 脚本

推荐新增 `scripts/tidy.sh`：

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
CLANG_TIDY=clang-tidy-18 RUN_CLANG_TIDY=run-clang-tidy-18 scripts/tidy.sh
```

建议检查范围：

- `src/**/*.c`
- `tests/**/*.c`
- `examples/**/*.c`
- 通过这些 translation units 覆盖 `include/**/*.h` 和 `src/**/*.h`

建议命令形态：

```bash
run-clang-tidy -p build -j "$(nproc)" \
  -header-filter='(^|.*/)(include|src|tests|examples)/.*'
```

如果没有 `run-clang-tidy`，回退：

```bash
git ls-files 'src/*.c' 'tests/*.c' 'examples/*.c' |
  xargs -r "${CLANG_TIDY:-clang-tidy}" -p build --quiet \
    --header-filter='(^|.*/)(include|src|tests|examples)/.*'
```

### 4. 调整 `.clang-tidy` header filter

当前配置是：

```yaml
HeaderFilterRegex: '^(include|src|tests)/.*'
```

建议 v1.1 改为：

```yaml
HeaderFilterRegex: '(^|.*/)(include|src|tests|examples)/.*'
```

原因：

- 当前 compile database 使用绝对路径，`^(include|src|tests)/.*` 对绝对头路径不稳。
- v1.1 明确要求 demo 迁移，`examples/` 应进入风格检查。
- 这个调整仍然尊重 `.clang-tidy` 作为唯一命名规则来源，只是修正作用范围。

### 5. CMake 集成方式

推荐先以脚本作为 CI gate，不默认在普通 build 中启用 clang-tidy。可新增 opt-in：

```cmake
option(MRTC_ENABLE_CLANG_TIDY "Run clang-tidy during CMake builds" OFF)

if(MRTC_ENABLE_CLANG_TIDY)
    find_program(MRTC_CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 REQUIRED)
    set(MRTC_CLANG_TIDY_COMMAND
        "${MRTC_CLANG_TIDY_EXE}"
        "--header-filter=(^|.*/)(include|src|tests|examples)/.*"
    )
    set_target_properties(micrortc PROPERTIES C_CLANG_TIDY "${MRTC_CLANG_TIDY_COMMAND}")
endif()
```

如果后续要让测试/demo target 也受 CMake build-time tidy 约束，建议把设置封装成 `mrtc_enable_tidy(target)`，在 `mrtc_add_test()` 和 `mrtc_add_chrome_e2e_targets()` 里复用。不要设置全局 `CMAKE_C_CLANG_TIDY`，否则 target 创建顺序和第三方/生成文件排除会更难控。

### 6. 公共 API 迁移映射生成

推荐新增一个人工维护的 source-of-truth 映射文件，例如：

```text
docs/api-renames-v1.1.tsv
kind	old	new	compat	notes
typedef	MRTC_STATUS	MrtcStatus	breaking	类型按 .clang-tidy TypedefCase 改 CamelCase
opaque	MRTC_PEER_CONNECTION_HANDLE	MrtcPeerConnection *	breaking	建议去掉 HANDLE typedef，公开 opaque struct 指针
enum	MRTC_PEER_CONNECTION_STATE	MrtcPeerConnectionState	breaking	枚举类型 CamelCase
constant	MRTC_STATUS_OK	MRTC_STATUS_OK	kept	枚举常量继续 UPPER_CASE
function	mrtc_peer_connection_create	mrtc_peer_connection_create	kept	函数已是 lower_case + mrtc_ 前缀
```

再新增 `scripts/generate-api-migration-map.py`，只用 Python 标准库输出：

- `docs/API-MIGRATION-v1.1.md`
- `build/reports/mrtc-api-migration-map.json`

同时新增 `scripts/check-api-migration.py`：

- 扫描 `include/`、`src/`、`tests/`、`examples/`、`README.md`。
- 对 `compat=breaking` 的旧 public symbols 报错。
- 对 `compat=kept` 的枚举常量、宏、函数允许保留。
- 对 `reflib/`、`build/`、`tests/e2e/node_modules/`、历史 `.planning/milestones/` 不扫描。

不要试图从 AST 自动推断迁移映射。v1.1 是公共 API 设计决策，映射应人工确认；自动脚本只负责生成文档和检查残留。

## API Naming Recommendation

`.clang-tidy` 当前规则要求：

- `StructCase: CamelCase`
- `TypedefCase: CamelCase`
- `EnumCase: CamelCase`
- `FunctionCase: lower_case`
- `Variable/Parameter/MemberCase: lower_case`
- `EnumConstantCase` 和 `MacroDefinitionCase: UPPER_CASE`

因此 v1.1 公共 API 推荐方向：

| Current | Recommended | Rationale |
|---------|-------------|-----------|
| `MRTC_STATUS` | `MrtcStatus` | typedef/enum 类型必须 CamelCase。 |
| `MRTC_PEER_CONNECTION_CONFIG` | `MrtcPeerConnectionConfig` | 公共配置类型 CamelCase，保留 `Mrtc` 项目前缀。 |
| `MRTC_PEER_CONNECTION_HANDLE` | `MrtcPeerConnection *` | 破坏式清理时建议不再隐藏指针所有权；opaque struct 名称 CamelCase。 |
| `MRTC_DATA_CHANNEL_HANDLE` | `MrtcDataChannel *` | 同上。 |
| `MRTC_RTP_TRANSCEIVER_HANDLE` | `MrtcRtpTransceiver *` | 同上；缩写按项目选择固定为 `Rtp`、`Rtc` 这种 CamelCase。 |
| `MRTC_STATUS_OK` | 保留 | enum constant 规则就是 UPPER_CASE；保留 `MRTC_` 宏/枚举常量前缀可避免 C 命名空间冲突。 |
| `mrtc_peer_connection_create` | 保留 | 函数已经 lower_case，且 `mrtc_` 前缀是 C API 命名空间。 |

兼容性决策：v1.1 推荐默认不提供旧类型别名。旧别名会直接违反 `.clang-tidy` 的 TypedefCase/EnumCase，除非给兼容头加大量 `NOLINT` 或放宽规则，这会削弱本里程碑目标。若必须提供过渡层，应作为显式 opt-in 的 `include/micrortc/compat/v1.h`，并标记不进入默认 install/API 文档；但这不是推荐路径。

## Verification Pipeline

推荐 CI/本地验收顺序：

```bash
# 1. fresh configure + compile database
cmake -S . -B build -DMRTC_BUILD_TESTS=ON -DMRTC_BUILD_EXAMPLES=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 2. 风格检查
CLANG_FORMAT=clang-format-18 scripts/format.sh --check
CLANG_TIDY=clang-tidy-18 RUN_CLANG_TIDY=run-clang-tidy-18 scripts/tidy.sh

# 3. API 迁移文档和旧符号残留检查
python3 scripts/generate-api-migration-map.py --check
python3 scripts/check-api-migration.py

# 4. 构建与 CTest
cmake --build build
ctest --test-dir build --output-on-failure

# 5. install/export package consumer
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer

# 6. v1.0 行为回归
scripts/verify-v1.sh

# 7. 有本地 TURN 配置时继续跑 relay 回归
scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json
```

`scripts/verify-v1.sh` 不应被替换。API 命名清理完成的最低证据必须同时包含：format/tidy 通过、迁移映射无旧 public symbols 残留、CTest 通过、package consumer 使用新 API 通过、Chrome host E2E 通过；TURN relay 在有本地配置时继续作为强回归。

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| LLVM clang-tools + 仓库脚本 | `cpplint` | 不是当前项目风格来源，和 `.clang-tidy` 命名规则重复且容易冲突。 |
| `clang-format` | `astyle` / `uncrustify` | 项目已有 `.clang-format`；换格式器会制造无关 diff。 |
| 人工维护 TSV/Markdown migration map + Python 检查 | libclang AST 自动生成 rename map | 公共 API rename 是设计决策，不是可完全自动推断的问题；libclang 还会增加平台安装复杂度。 |
| 脚本式 tidy gate | 默认所有 build 都跑 `C_CLANG_TIDY` | 普通开发构建会变慢，且本机缺工具时体验差；CI gate 更可控。 |
| 保持 CMake 3.16 | 升到 CMake 3.27+ 使用 `SKIP_LINTING` | v1.1 不需要这个能力；提高最低版本会扩大迁移面。 |
| 破坏式 API 清理，无默认旧别名 | 默认兼容旧 API | 旧 `MRTC_*` 类型别名违反 `.clang-tidy` 类型命名目标；兼容层会稀释 v1.1 的清理成果。 |

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| 新 signaling 框架或 signaling API | v1.1 不是功能扩展；核心库不内置应用层 signaling。 | 继续让 demo/test 自动化交换 SDP/ICE。 |
| GStreamer、FFmpeg、摄像头/麦克风采集适配 | 核心库只处理编码后媒体帧；v1.1 不研究媒体采集/编码。 | 保持固定 H264/Opus fixtures 和现有 E2E。 |
| 新 WebRTC 协议实现库 | v1.0 协议能力已验证；v1.1 只清理 API/风格。 | 保持现有 `src/` 协议栈。 |
| C++ wrapper 作为主 API | 会改变项目语言边界和 ABI 讨论范围。 | 继续 C API，函数 lower_case，类型 CamelCase。 |
| 默认旧 API compatibility header | 会保留 `.clang-tidy` 明确要清掉的旧类型命名。 | 文档化 breaking changes；必要时提供 opt-in compat 草案但不推荐。 |
| 自定义 clang-tidy check/plugin | 成本高，CI 安装复杂。 | 使用现有 `readability-identifier-naming` 和 Python 残留扫描。 |
| 在 `scripts/verify-v1.sh` 里混入 lint | 行为验收和风格验收职责不同，失败原因会变模糊。 | 新增 `scripts/verify-style.sh` 或 CI 单独 step。 |

## Version Compatibility

| Component | Compatible With | Notes |
|-----------|-----------------|-------|
| CMake 3.16 | `C_CLANG_TIDY` target property | CMake 文档说明 `<LANG>_CLANG_TIDY` 自 3.6 起支持 C/CXX/OBJC/OBJCXX。 |
| CMake 3.16 | `CMAKE_EXPORT_COMPILE_COMMANDS` | 官方文档说明可为 Makefile/Ninja 生成 `compile_commands.json`。 |
| CMake 3.16 | 不使用 `C_CLANG_TIDY_EXPORT_FIXES_DIR` | 该属性 3.26 才有；当前不建议为它提高最低 CMake。 |
| CMake 3.16 | 不使用 `SKIP_LINTING` | source-file `SKIP_LINTING` 3.27 才有；当前通过脚本 include/exclude 控制范围。 |
| LLVM clang-format | `--style=file --dry-run --Werror` | 官方 clang-format 文档列出这些选项；CI 应固定一个 LLVM 版本避免本机 wrapper/旧版本差异。 |
| LLVM clang-tidy | `.clang-tidy` + compile database | 官方 clang-tidy 文档推荐使用 compile database，并说明 `.clang-tidy` 会从源文件父目录查找。 |

## Sources

- Context7 `/websites/clang_llvm` — 查询 clang-tidy/LLVM 文档，确认 clang-tidy 属于 LLVM/Clang 工具链；返回内容有限，已用官方 LLVM 文档复核。
- LLVM clang-tidy 官方文档 — https://clang.llvm.org/extra/clang-tidy/ ：验证 compile database、`-checks`、`-warnings-as-errors`、`--export-fixes`、`run-clang-tidy`、diff 模式和 `NOLINT` 行为。
- LLVM `readability-identifier-naming` 官方文档 — https://clang.llvm.org/extra/clang-tidy/checks/readability/identifier-naming.html ：验证 `TypedefCase`、`EnumCase`、`FunctionCase`、`ParameterCase`、`MacroDefinitionCase` 等命名选项。
- LLVM clang-format 官方文档 — https://clang.llvm.org/docs/ClangFormat.html ：验证 `--style=file`、`--dry-run`、`--Werror`、`-i` 等用法。
- CMake `CMAKE_<LANG>_CLANG_TIDY` 官方文档 — https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_CLANG_TIDY.html ：验证变量初始化 target property。
- CMake `<LANG>_CLANG_TIDY` 官方文档 — https://cmake.org/cmake/help/v3.30/prop_tgt/LANG_CLANG_TIDY.html ：验证该 property 支持 C/CXX/OBJC/OBJCXX，Makefile/Ninja 会随编译运行工具。
- CMake `CMAKE_EXPORT_COMPILE_COMMANDS` 官方文档 — https://cmake.org/cmake/help/v3.6/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html ：验证生成 `compile_commands.json` 的行为和 Makefile/Ninja 限制。
- CMake `<LANG>_CLANG_TIDY_EXPORT_FIXES_DIR` 官方文档 — https://cmake.org/cmake/help/latest/prop_tgt/LANG_CLANG_TIDY_EXPORT_FIXES_DIR.html ：验证该能力 3.26 才有，因此不建议在 CMake 3.16 项目中依赖。
- CMake `SKIP_LINTING` 官方文档 — https://cmake.org/cmake/help/latest/prop_sf/SKIP_LINTING.html ：验证 source-file 排除能力 3.27 才有，因此不建议作为 v1.1 基础方案。

---
*Stack research for: libmicrortc v1.1 API 命名/代码风格清理*
*Researched: 2026-05-15*
