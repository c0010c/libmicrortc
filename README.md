# libmicrortc

`libmicrortc` 的目标是从本地 `reflib/kvs-webrtc-sdk` 基线中剥离可复用的 C WebRTC 协议栈能力，形成一个独立、可构建、可验证、可逐步清理的静态库。当前 Phase 2 只建立最小构建骨架，不搬迁协议实现，也不内置应用层 signaling、媒体采集或编码。

## 当前构建状态

Phase 2 提供一个独立 CMake 工程，核心 target 为 `micrortc`，导出命名空间为 `micrortc::`，静态库产物为 `libmicrortc.a`。

```bash
cmake -S . -B build -DMRTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
test -f build/libmicrortc.a
```

`MRTC_BUILD_TESTS=ON` 会构建 `micrortc_smoke_test`。该测试只通过公开头文件 `<micrortc/micrortc.h>` 调用真实 `mrtc_*` 符号，并链接 `micrortc::micrortc`。

## 安装与下游消费

可以把当前静态库安装到临时前缀，并用独立 consumer 工程验证 CMake package/export：

```bash
cmake --install build --prefix build/install
cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/package-consumer
./build/package-consumer/package_consumer
```

下游 CMake 工程应使用：

```cmake
find_package(micrortc CONFIG REQUIRED)
target_link_libraries(app PRIVATE micrortc::micrortc)
```

安装后会生成 `build/install/lib/cmake/micrortc/micrortcConfig.cmake`、`micrortcTargets.cmake` 和公开头文件 `build/install/include/micrortc/micrortc.h`。

## Phase 2 API 边界

当前公开 API 仅包含 `MRTC_STATUS`、`mrtc_version_string()`、`mrtc_initialize()` 和 `mrtc_shutdown()`。这些符号只用于证明 public include、静态库链接和安装后消费路径可用。

PeerConnection、SDP、ICE candidate、transceiver、DataChannel、RTP/RTCP、DTLS、SRTP、SCTP 和媒体收发 API 都保留到后续阶段裁剪和实现。

## 依赖边界

Phase 2 最小骨架不需要 OpenSSL、libsrtp 或 usrsctp，因此根 CMake 不查找、不下载、不链接这些依赖。它们仍然是 v1 runtime/core 依赖边界：

- OpenSSL：后续 DTLS/OpenSSL 路径进入时再决定发现和链接策略。
- libsrtp：后续 SRTP 会话和媒体保护进入时再链接。
- usrsctp：后续 SCTP/DataChannel 进入时再链接。

以下组件不参与核心 `micrortc` target，也不得被 Phase 2 构建间接引入：

- libwebsockets
- AWS SDK C++
- KVS signaling
- AWS credential/storage/KVS service paths
- GStreamer sample
- kvsCommonLws
- kvspicUtils
- kvspicState

`kvsCommonLws`、`kvspicUtils` 和 `kvspicState` 只能作为后续迁移 allocator、logging、state、utils 等基础能力时的 reference-only 参考，不是当前核心库依赖。

## 来源归属

Phase 2 新增的 CMake、头文件、最小源码、测试和本文档都是 `original` 骨架文件，没有复制或重写派生 `reflib/kvs-webrtc-sdk` 的源码。

如果后续阶段从 `reflib/kvs-webrtc-sdk` 复制、裁剪、改名或 rewritten-derived 任意源码、头文件、CMake 片段或测试逻辑，必须更新 `.planning/phases/01-/SOURCE-MANIFEST.md`，记录本地来源路径、目标路径、基线提交和派生方式。
