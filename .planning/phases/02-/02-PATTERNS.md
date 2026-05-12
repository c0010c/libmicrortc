# Phase 2: 独立构建与库骨架 - Pattern Map

**Generated:** 2026-05-12T19:42:43+08:00
**Status:** Ready for planning

## PATTERN MAPPING COMPLETE

## 目标文件与最近参考

| Planned file | Role | Closest source/analog | Pattern to reuse |
|--------------|------|-----------------------|------------------|
| `CMakeLists.txt` | 根目录独立构建入口 | `reflib/kvs-webrtc-sdk/CMakeLists.txt` as anti-pattern/reference, `.planning/phases/02-/02-CONTEXT.md` | 不复用参考库 glob/ExternalProject；只定义 `micrortc` 静态库、CTest、install/export |
| `cmake/micrortcConfig.cmake.in` | 安装后 package config 模板 | CMake `CMakePackageConfigHelpers` 标准模式 | `@PACKAGE_INIT@` + include installed `micrortcTargets.cmake` |
| `include/micrortc/micrortc.h` | 最小 public C API | `.planning/phases/02-/02-CONTEXT.md` decisions D-05 到 D-08 | C header guard、`extern "C"`、`MRTC_STATUS`、`mrtc_*` 函数；不引入 AWS 类型 |
| `src/micrortc.c` | 最小可链接实现 | `include/micrortc/micrortc.h` | 实现 header 中声明的真实符号，保持无外部依赖 |
| `tests/smoke/test_link.c` | build-tree 链接 smoke test | `include/micrortc/micrortc.h` | include public header，调用 `mrtc_version_string` 和 init/shutdown 函数 |
| `tests/package-consumer/CMakeLists.txt` | 安装包消费验证 | `cmake/micrortcConfig.cmake.in` | `find_package(micrortc CONFIG REQUIRED)` + `target_link_libraries(... PRIVATE micrortc::micrortc)` |
| `tests/package-consumer/package_consumer.c` | install/export 链接验证程序 | `tests/smoke/test_link.c` | 用安装后的 public header 和 namespaced target 编译运行 |
| `README.md` 或 `docs/build.md` | 构建说明与依赖边界 | `.planning/phases/01-/BASELINE.md`, `.planning/phases/01-/COMPLIANCE.md`, `.planning/phases/02-/02-CONTEXT.md` | 中文 Markdown；包含命令、current skeleton dependencies、excluded/reference-only 组件 |

## 数据流

1. `CMakeLists.txt` 定义 `micrortc` 静态库，消费 `include/` 和 `src/micrortc.c`。
2. `tests/smoke/test_link.c` 通过 build-tree alias `micrortc::micrortc` 验证 public header 和真实符号。
3. `cmake/micrortcConfig.cmake.in` 与 install/export 规则生成安装包 CMake config。
4. `tests/package-consumer` 通过 `CMAKE_PREFIX_PATH=$PWD/build/install` 验证安装后 package 可消费。
5. README/build 文档解释为什么 OpenSSL、libsrtp、usrsctp 暂不查找，以及哪些 AWS/KVS/PIC 组件被排除。

## 执行注意事项

- 不要在根目录 CMake 中 `add_subdirectory(reflib/kvs-webrtc-sdk)`。
- 不要 glob `reflib/kvs-webrtc-sdk/src/source/*.c`。
- 不要创建 `kvsWebrtcClient`、`kvsWebrtcSignalingClient`、`kvsCommonLws`、`kvspicUtils` 或 AWS/KVS alias。
- 不要在 public header 暴露 `STATUS`、`PCHAR`、`PeerConnection`、SDP、ICE candidate、transceiver 或 DataChannel API。
- 安装后 consumer 必须链接 `micrortc::micrortc`，证明 namespace export 工作。
- 自有 Phase 2 文件应在 summary 或文档中标为 `original`，不追加为 `SOURCE-MANIFEST.md` 的 AWS 派生记录。
