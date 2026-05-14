# v1.1 Research: Pitfalls

**Milestone:** v1.1 剥离收口
**Date:** 2026-05-14

## 主要风险

### 1. 把“剥离收口”做成破坏性 API 重写

如果 v1.1 直接删除或重命名 public API，会和 minor release 的迁移预期冲突。SemVer 的建议是先在文档中标注 deprecated，并在 minor release 提供迁移窗口，再在 major release 移除。

**Prevention:** v1.1 只做兼容层、新命名、文档和 deprecation；破坏性删除放到 v2.0。

### 2. CMake package 可以在源码树工作，但安装后不可用

常见问题包括 install config 泄漏 build tree 路径、依赖 include path 被硬编码、consumer 依赖源码树 layout。CMake 文档明确提醒 `INSTALL_INTERFACE` 不应写入依赖 include 的绝对路径。

**Prevention:** 用独立 build/install prefix + 外部 package consumer 验证，检查 generated config/targets 中是否包含源码树或 build tree 路径。

### 3. 清理 AWS/KVS 字符串时误删历史和合规上下文

AWS/KVS 字样在 public API 和产品边界中应清理，但在项目历史、来源追溯、许可证和基线说明中必须保留。无脑替换会破坏合规证据。

**Prevention:** 按区域分级：public API/package/demo 输出禁止产品耦合；planning/README 的历史与基线说明允许保留；source manifest 必须保留。

### 4. 依赖边界清理导致传输能力退化

OpenSSL/libsrtp/usrsctp 是 WebRTC transport/security/datachannel 路径的实际依赖。把它们从 public 边界清理出去，不等于从实现里删掉。

**Prevention:** CMake option、strict dependency mode、CTest 和 Chrome E2E 同时覆盖；缺依赖时要 fail 明确，而不是静默降级到不可用路径。

### 5. OpenSSL 3 deprecated API 风险被忽略

OpenSSL 3 将低层算法 API 正式 deprecated，未来版本可能进一步移除或加重 warning。DTLS/crypto 代码如果直接使用低层 API，会影响后续发行版构建体验。

**Prevention:** v1.1 做 OpenSSL API audit；能轻量迁移到高层 EVP/provider 路径的先迁移，不能迁移的记录为后续依赖抽象任务。

### 6. 静态库发布误以为不需要 ABI/API 纪律

当前静态库优先，ABI diff 不是最紧急门槛，但 public headers 一旦被 downstream 编译依赖，API 破坏仍会造成升级失败。

**Prevention:** 至少建立 public header/API surface diff；可选引入 `abidiff` 或导出符号清单，为未来 shared library 打基础。

### 7. 回归只看连接状态，不看真实媒体和 TURN 证据

v1.0 的核心价值是 Chrome、TURN relay、DataChannel、H264/Opus 双向媒体都有自动化证据。v1.1 清理过程中如果只跑编译或 CTest，可能漏掉 SDP/ICE/DTLS/SRTP/SCTP 的集成退化。

**Prevention:** 继续把 `scripts/verify-v1.sh` 和 TURN relay summary 作为里程碑验收，不用内部 connected 状态替代真实媒体断言。

## Sources

- Semantic Versioning 2.0.0: https://semver.org/
- CMake `target_include_directories` relocatable package guidance: https://cmake.org/cmake/help/v3.22/command/target_include_directories.html
- OpenSSL migration guide: https://docs.openssl.org/3.5/man7/ossl-guide-migration/
- Libabigail `abidiff`: https://sourceware.org/libabigail/manual/abidiff.html
