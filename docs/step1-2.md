## ID 1-2 基线落地计划（基于 `docs/plan.md`）

### Summary
1. 仅交付 `id=1` 和 `id=2`：目录骨架 + 根 `CMakeLists.txt` 静态库目标。
2. 采用“方案 C”：目录组织参考 KVS 风格，但保持最小实现，不引入后续任务内容。
3. 不包含 `id=3+` 的 feature-gate、API 头、源码实现、测试实体。

### Implementation Changes
1. 创建/确保存在目录：`include/`、`src/`、`ports/`、`tests/`、`cmake/`、`scripts/`（`docs/` 已存在则不改）。
2. 新建根 `CMakeLists.txt`，内容固定为最小可配置工程：
3. `cmake_minimum_required(VERSION 3.6.3)`（与参考库兼容取向一致）。
4. `project(s2_webrtc LANGUAGES C)`。
5. 在构建目录生成占位 C 源文件（如 `${CMAKE_CURRENT_BINARY_DIR}/generated/webrtc_core_placeholder.c`），避免 `add_library(... STATIC)` 无源码报错。
6. `add_library(webrtc_core STATIC <占位源>)`。
7. `target_include_directories(webrtc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)`。
8. 不引入 `add_subdirectory(...)`；不引入 `cmake/options.cmake`；不引入平台/协议实现。

### Public APIs / Interfaces
1. 本次不新增公共 API/类型，不修改 ABI。
2. 仅新增构建入口接口（`webrtc_core` 静态库目标）。

### Test Plan
1. 验证目录骨架：`find . -maxdepth 2 -type d | sort`。
2. 验证配置成功：`cmake -S . -B build/base`。
3. 验证目标存在：`cmake --build build/base --target webrtc_core -j`（建议做，虽非 `id=2` 强制项）。

### Assumptions
1. 计划源文件按你确认使用 `docs/plan.md`（非 `docs/plan/plan.md`）。
2. 范围严格锁定 `id=1/2`，不提前实现 `id=3+`。
3. 当前目录非 git 仓库；本轮计划不包含提交历史对齐动作。
