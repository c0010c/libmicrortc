# Phase 2: 独立构建与库骨架 - Research

**Generated:** 2026-05-12T19:42:43+08:00
**Status:** Ready for planning

## RESEARCH COMPLETE

## 研究问题

Phase 2 需要回答的问题是：如何在不搬迁 AWS KVS WebRTC 协议源码、不引入 signaling/PIC/AWS 依赖的前提下，建立一个可独立配置、构建、安装并被下游 CMake 工程消费的 `libmicrortc.a` 静态库骨架。

## 核心结论

1. Phase 2 应创建自有最小 C 静态库，而不是复用 `reflib/kvs-webrtc-sdk` 的 target 或源码 glob。
2. 根目录 `CMakeLists.txt` 应定义 `micrortc` 静态库 target、build-tree alias `micrortc::micrortc`、public include、CTest smoke test、install/export 和 package config。
3. 最小公共头只应提供 `include/micrortc/micrortc.h`，暴露少量 `MRTC_STATUS` / `mrtc_*` 符号，用于证明真实链接和消费链路。
4. Phase 2 不应 `find_package(OpenSSL)`、`pkg_check_modules(libsrtp)`、查找 usrsctp 或运行 ExternalProject；这些依赖仍作为 v1 runtime/core 边界记录，等协议模块实际引入时再激活。
5. 验收必须超过“`.a` 文件存在”：需要证明 build-tree target 可链接、安装后 `find_package(micrortc CONFIG)` 可消费、核心 target 未链接 AWS/KVS/PIC/signaling/sample 组件。

## 本地参考库观察

### 顶层 CMake 耦合点

`reflib/kvs-webrtc-sdk/CMakeLists.txt` 同时管理依赖构建、协议源码、signaling target、sample、测试和 benchmark。关键风险点：

- `BUILD_DEPENDENCIES` 默认打开，并通过 `CMake/Dependencies` 构建 OpenSSL、libwebsockets、libsrtp、usrsctp、kvsCommonLws/PIC 等依赖。
- 即使协议 target 不使用 KVS signaling，顶层仍无条件调用 `build_dependency(kvsCommonLws ...)`，而该路径会带来 PIC 公共能力。
- `kvsWebrtcClient` 当前 glob 多个协议目录，包括 `Crypto`、`Ice`、`PeerConnection`、`Rtcp`、`Rtp`、`Sdp`、`Srtp`、`Stun`、`Metrics`，并在 `ENABLE_DATA_CHANNEL` 时加入 `Sctp` 和 `DataChannel`。
- `kvsWebrtcClient` 链接 `kvspicUtils`、`kvspicState`、OpenSSL、libsrtp、usrsctp 和线程库。
- `kvsWebrtcSignalingClient` 单独 glob `src/source/Signaling/*.c`，并链接 `kvsCommonLws` 与 libwebsockets。
- `BUILD_SAMPLE` 默认打开并进入 `samples`，这与 libmicrortc 核心库边界相反。

### 子目录 CMake 反例

`reflib/kvs-webrtc-sdk/src/CMakeLists.txt` 递归包含 `source/*.c` 并链接 `client heap trace view mkvgen utils state jsmn crypto ssl`。这体现了参考库与 Producer C / PIC 公共库的历史耦合，不适合作为 Phase 2 的骨架模板。

### 依赖边界

Phase 1 已记录依赖状态：

| 依赖 | Phase 2 处理 | 后续触发点 |
|------|--------------|------------|
| OpenSSL | 不查找、不链接；文档记录为 v1 runtime/core | DTLS/OpenSSL 路径进入 Phase 4 |
| libsrtp | 不查找、不链接；文档记录为 v1 runtime/core | SRTP 会话进入 Phase 4/5 |
| usrsctp | 不查找、不链接；文档记录为 v1 runtime/core | SCTP/DataChannel 进入 Phase 4 |
| libwebsockets | excluded，不进入核心 target | 仅 demo/signaling 层可另行讨论 |
| AWS SDK C++ | excluded | 不进入 v1 核心库 |
| kvsCommonLws/PIC | supporting/reference-only，不链接 | 后续协议搬迁时单独设计 allocator/logging/state/utils |
| GStreamer sample | out of scope/excluded | v2 adapter 另行讨论 |

## 推荐骨架

### 目录布局

Phase 2 执行阶段建议新增：

| 路径 | 用途 | 来源 |
|------|------|------|
| `CMakeLists.txt` | 根目录独立 CMake 入口、静态库 target、install/export、CTest | original |
| `cmake/micrortcConfig.cmake.in` | 安装后 package config 模板 | original |
| `include/micrortc/micrortc.h` | 最小 public C API | original |
| `src/micrortc.c` | 最小可链接实现 | original |
| `tests/smoke/test_link.c` | build-tree 链接 smoke test | original |
| `tests/package-consumer/CMakeLists.txt` | 安装后 `find_package` 消费端验证 | original |
| `tests/package-consumer/package_consumer.c` | 安装包链接验证程序 | original |
| `README.md` 或 `docs/build.md` | 构建命令、依赖边界、excluded 说明 | original |

`SOURCE-MANIFEST.md` 不应记录这些自有骨架文件为 AWS 派生；只有未来复制、裁剪、重写派生参考库内容时才追加 manifest 行。

### CMake target 策略

建议根目录 CMake 采用这些硬约束：

- `project(micrortc VERSION 0.1.0 LANGUAGES C)`。
- `add_library(micrortc STATIC src/micrortc.c)`。
- `add_library(micrortc::micrortc ALIAS micrortc)` 用于 build-tree 测试。
- `set_target_properties(micrortc PROPERTIES OUTPUT_NAME micrortc C_STANDARD 99 C_STANDARD_REQUIRED ON)`。
- `target_include_directories(micrortc PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:include>")`。
- 不调用 `find_package(OpenSSL)`、`find_package(PkgConfig)`、`pkg_check_modules(LIBWEBSOCKETS)` 或 `ExternalProject_Add`。
- 不引用 `reflib/kvs-webrtc-sdk` 源码路径。
- `install(TARGETS micrortc EXPORT micrortcTargets ARCHIVE DESTINATION lib INCLUDES DESTINATION include)`。
- `install(DIRECTORY include/ DESTINATION include)`。
- `install(EXPORT micrortcTargets NAMESPACE micrortc:: DESTINATION lib/cmake/micrortc)`。
- 通过 `configure_package_config_file` 和 `write_basic_package_version_file` 生成 `micrortcConfig.cmake` 与 `micrortcConfigVersion.cmake`。

### 最小 API 策略

Phase 2 API 只服务构建验收，不预承诺 Phase 3 PeerConnection API。建议最小形态：

- `typedef enum MRTC_STATUS { MRTC_STATUS_OK = 0, MRTC_STATUS_INVALID_ARG = 1 } MRTC_STATUS;`
- `const char *mrtc_version_string(void);`
- `MRTC_STATUS mrtc_initialize(void);`
- `void mrtc_shutdown(void);`

这些符号足以证明 public header、C ABI、静态库归档和链接器解析。不要加入 `PeerConnection`、SDP、ICE candidate、transceiver、DataChannel 或 AWS 公共类型。

## 验收策略

### 命令级验收

执行阶段可使用下面的命令序列证明 Phase 2 完成：

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
test -f build/libmicrortc.a
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
```

### 排除边界验收

还应加入 source/CMake grep 断言：

```bash
rg -n "kvsWebrtc|kvsCommonLws|kvspic|Signaling|libwebsockets|GStreamer|AWS SDK|BUILD_DEPENDENCIES|ExternalProject" CMakeLists.txt cmake include src tests README.md
```

该命令如果有输出，需要人工确认它只出现在文档的 excluded/reference-only 说明中，不能出现在 `target_link_libraries(micrortc ...)` 或源码 include/link 路径中。

## 关键风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 误复用参考库 CMake glob | 直接带入协议源码、PIC、signaling 或 sample | 根目录 CMake 从空骨架写起，不 `add_subdirectory(reflib/...)` |
| 过早查找 OpenSSL/libsrtp/usrsctp | Phase 2 环境门槛变高且掩盖核心目标 | 文档记录依赖边界，不在 target 中查找或链接 |
| 最小 API 提前承诺 PeerConnection 结构 | Phase 3 API 裁剪空间被锁死 | 只暴露 `MRTC_STATUS` 和 `mrtc_*` 最小壳 |
| 只验证 `.a` 存在 | 可能漏掉 public header、install/export 或 package config 问题 | 加 build-tree smoke test 与安装后 consumer 测试 |
| 自有文件被误记为 AWS 派生 | 溯源 manifest 失真 | Phase 2 summary/文档标明新增骨架为 `original` |

## Validation Architecture

Phase 2 的验证应采用 shell + CMake + CTest + `rg`。快速验证检查计划文件和将来 deliverable 路径是否存在；完整验证必须配置、构建、运行 smoke test、安装、再用独立 consumer 工程通过 `find_package(micrortc CONFIG)` 链接运行。

建议执行阶段的最大反馈延迟控制在 30 秒内，因为最小 C 库和 smoke test 不应拉取第三方依赖。

## 规划建议

生成一个单计划即可覆盖 Phase 2，因为所有工作围绕同一个构建骨架和消费验证闭环。计划应包含 5 到 6 个任务：

1. 建立根目录 CMake target 与 install/export。
2. 添加最小 public header 和 source。
3. 添加 build-tree smoke test。
4. 添加安装后 package consumer 验证。
5. 添加构建文档和 excluded/reference-only 依赖边界说明。
6. 运行完整验收并确认未修改 `SOURCE-MANIFEST.md` 的 AWS 派生记录。

---

*Research completed inline by Codex because subagent spawning was not explicitly requested in this runtime.*
