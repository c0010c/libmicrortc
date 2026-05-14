# v1.1 Research: Stack

**Milestone:** v1.1 剥离收口
**Date:** 2026-05-14

## 研究问题

v1.1 不引入新的 WebRTC 协议栈能力，重点是把 v1.0 已经跑通的库收口成更独立、可发布、可消费、可维护的 C 库。因此 stack 侧关注：

- CMake install/export package 是否足够可重定位、可被 downstream `find_package()` 消费。
- 公共 API 版本和兼容策略是否能支撑从 AWS 风格裁剪 API 迁移到 libmicrortc 风格 API。
- 许可证和来源追溯是否适合继续从本地 AWS KVS WebRTC SDK 基线搬迁或派生代码。
- OpenSSL/libsrtp/usrsctp 等传输依赖是否被清晰地表达为核心库外部依赖，而不是泄漏为 AWS/KVS 产品依赖。
- 是否需要 ABI/API 回归检查作为发布质量门。

## 结论

### CMake package

CMake 官方导入/导出指南建议用 `install(TARGETS)` 和 `install(EXPORT)` 配合生成 downstream 可导入的 target，并使用命名空间 target，例如 `micrortc::micrortc`。当前项目已经具备 `install(EXPORT micrortcTargets NAMESPACE micrortc:: ...)`、`configure_package_config_file()` 和 `write_basic_package_version_file()`，v1.1 应把这条路径升级为显式验收项，而不是只保留 package-consumer smoke test。

对可重定位安装包，CMake 文档特别提醒 `INSTALL_INTERFACE` 不应写入构建机器上的依赖 include 绝对路径；依赖应该作为自己的 target 表达 usage requirements。当前项目对 OpenSSL 用 imported target，libsrtp/usrsctp 仍用 `find_library`/`find_path` 后以 private include/link 接入。v1.1 的要求应覆盖：安装后的 `micrortcConfig.cmake` 不泄漏 build tree，consumer 能从 install prefix 编译和链接，缺失 transport deps 时的错误信息明确。

### Versioning/API compatibility

SemVer 官方规范强调使用语义化版本的项目必须声明 public API；废弃 public API 时应更新文档，并先发布一个包含 deprecation 的 minor release，再在 major release 移除。对 libmicrortc 来说，v1.1 适合做“兼容层 + 新命名”的小版本：保留 v1.0 API 能力，新增或标记 libmicrortc 风格命名与迁移文档；真正删除 AWS 风格或历史不佳 API，应放到后续 major 里程碑。

### License/source tracking

REUSE 3.3 和 SPDX 的方向是让每个文件的版权与许可证信息可机器读取。项目已有 Apache-2.0/NOTICE 和来源追溯策略，v1.1 应把新增/搬迁/派生文件的 SPDX header、NOTICE、source manifest 检查纳入验收，尤其是继续参考本地 `reflib/kvs-webrtc-sdk` 时要区分“原样搬迁”“派生改写”“本项目原创”。

### OpenSSL dependency

OpenSSL 3.x migration guide 说明 OpenSSL 3 引入 provider 概念，算法实现通过高层 API 访问，低层算法 API 已正式 deprecated。v1.1 不一定要完成 OpenSSL 抽象替换，但应审计当前 DTLS/crypto 代码是否使用低层 deprecated API，并把“使用高层 API、避免未来 OpenSSL 版本警告/移除风险”纳入依赖清理需求。

### ABI/API checking

libabigail 的 `abidiff` 可比较二进制 ABI 语料，适合未来发布前检查 C ABI 漂移。v1.1 可以先建立“导出符号/API surface 快照 + package consumer + 可选 ABI diff”的轻量门槛；如果当前仍以静态库为主，ABI diff 可作为可选工具，不阻塞基础验收。

## 推荐 stack 决策

- 保持 CMake 3.16 baseline，继续静态库优先；不要为了 v1.1 引入包管理器或复杂发布系统。
- 保持 OpenSSL、libsrtp、usrsctp 为外部依赖；v1.1 只清理表达方式、错误信息和文档，不替换依赖实现。
- 将 public API 分成 “stable v1 surface”“deprecated compatibility surface”“internal headers” 三类。
- 增加 release/package 验收脚本，覆盖 install prefix、downstream `find_package(micrortc CONFIG REQUIRED)`、headers-only include order、link、运行 smoke。
- 增加 license/source audit 脚本或 checklist，保证从 `reflib/kvs-webrtc-sdk` 继续派生时可追溯。

## Sources

- CMake Importing and Exporting Guide: https://cmake.org/cmake/help/v3.23/guide/importing-exporting/index.html
- CMake `target_include_directories` relocatable packages: https://cmake.org/cmake/help/v3.22/command/target_include_directories.html
- Semantic Versioning 2.0.0: https://semver.org/
- REUSE Specification 3.3: https://reuse.software/spec-3.3/
- OpenSSL migration guide: https://docs.openssl.org/3.5/man7/ossl-guide-migration/
- Libabigail `abidiff`: https://sourceware.org/libabigail/manual/abidiff.html
