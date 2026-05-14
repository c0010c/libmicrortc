# v1.1 Research Summary

**Milestone:** v1.1 剥离收口
**Date:** 2026-05-14

## Key findings

### Stack additions

- 不需要新增 WebRTC 协议依赖；继续使用现有 CMake、OpenSSL、libsrtp、usrsctp、CTest、Playwright E2E。
- 需要把 CMake install/export 从“已有 smoke”提升为 release/package 质量门：独立 install prefix、downstream `find_package()`、config version、路径可重定位。
- 需要引入 public API surface inventory，作为命名清理、兼容层和后续 SemVer 管理的依据。
- 可以考虑轻量 API/ABI check：v1.1 必需 API surface diff，`abidiff` 可选。

### Feature table stakes

- Public API/package/demo 输出层不再暴露 AWS/KVS 产品耦合。
- v1.0 API 能力在 v1.1 不能被无迁移窗口地破坏；需要兼容策略、deprecation 文档或 alias。
- Release package 必须可安装、可消费、可报告版本，并且不泄漏源码树/build tree。
- 许可证、NOTICE、SPDX/source manifest 必须继续覆盖从本地 `reflib/kvs-webrtc-sdk` 派生的内容。
- `scripts/verify-v1.sh` 默认 host E2E 和 TURN relay E2E 继续作为清理后的真实回归证据。

### Watch out for

- 不要把 v1.1 做成 v2.0 API 删除重写；破坏性移除应等 major release。
- 不要机械删除 README/规划/合规材料里的 AWS/KVS 历史说明；只清理 public-facing library coupling。
- 不要只验证 build/CTest；必须保留 Chrome、TURN、DataChannel、H264/Opus 双向媒体 evidence。
- 不要把 transport deps 从实现里“清掉”；v1.1 清理的是边界表达、包可消费性和依赖风险。

## Requirement implications

v1.1 requirements 应按四类组织：

1. API 独立性与兼容迁移
2. Package/release readiness
3. License/source/dependency boundary
4. Regression evidence

## Sources

- CMake Importing and Exporting Guide: https://cmake.org/cmake/help/v3.23/guide/importing-exporting/index.html
- CMake relocatable package guidance: https://cmake.org/cmake/help/v3.22/command/target_include_directories.html
- Semantic Versioning 2.0.0: https://semver.org/
- REUSE Specification 3.3: https://reuse.software/spec-3.3/
- OpenSSL migration guide: https://docs.openssl.org/3.5/man7/ossl-guide-migration/
- Libabigail `abidiff`: https://sourceware.org/libabigail/manual/abidiff.html
- WebRTC Native Code overview: https://webrtc.github.io/webrtc-org/native-code/
- libdatachannel project positioning: https://libdatachannel.org/
