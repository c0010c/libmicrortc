# v1.1 Research: Architecture

**Milestone:** v1.1 剥离收口
**Date:** 2026-05-14

## 当前架构观察

v1.0 已形成以下结构：

- Public headers: `include/micrortc/micrortc.h`、`include/micrortc/peer_connection.h`
- Core modules: `src/ice`、`src/stun`、`src/turn`、`src/dtls`、`src/srtp`、`src/sctp`、`src/data_channel`、`src/media`、`src/rtp`、`src/rtcp`、`src/sdp`
- Package path: `CMakeLists.txt` + `cmake/micrortcConfig.cmake.in`
- Acceptance path: `scripts/verify-v1.sh` + CTest + Playwright Chrome E2E + TURN config
- Boundary: core library 不内置 signaling，不采集/编码媒体，只处理编码后帧

仓库 grep 显示核心 `include/` 和 `src/` 下已经基本使用 `mrtc`/`micrortc` 命名；AWS/KVS 残留主要在 README、规划文档和“历史定位说明”中。v1.1 的架构风险不是简单字符串替换，而是 public API 分层、发布边界和兼容策略是否清楚。

## 建议架构切分

### Phase A: API surface inventory

先生成并维护 public surface 清单：

- public headers 列表
- public typedef/enum/struct/function 列表
- 当前未实现或部分实现 API 列表
- public enum/status 语义
- opaque handle 生命周期约定

输出应进入文档或生成文件，后续清理时作为 review 基线。

### Phase B: Compatibility layer

如果引入更独立的新 API 命名，应避免 v1.1 直接破坏 v1.0 调用方：

- 保留现有 `mrtc_*` 作为 v1.1 stable API，或明确它就是最终前缀。
- 对需要重命名的符号提供 alias/deprecation macro。
- 文档说明 deprecated API 的替代项和预计移除里程碑。
- 测试同时覆盖旧入口和新入口。

这符合 SemVer 对 minor deprecation 的建议：先在文档和小版本中给迁移窗口，后续 major 再移除。

### Phase C: Package boundary hardening

CMake package 架构应保持简单：

- `micrortc` target 继续只暴露 `include/` usage requirement。
- OpenSSL/libsrtp/usrsctp 作为 private implementation dependencies，除非 public headers 直接依赖其类型。
- install/export package 用 `micrortc::micrortc` namespace。
- package consumer 必须从 install prefix 构建，不复用源码树 include。
- config version 使用 `SameMajorVersion`，并和 `project(... VERSION ...)`、`mrtc_version_string()` 保持一致。

### Phase D: Compliance boundary

把合规能力视为架构边界的一部分：

- `reflib/kvs-webrtc-sdk` 是只读剥离基线。
- 搬迁或派生代码应记录在 source manifest 或 phase artifact 中。
- NOTICE、LICENSE、SPDX header、third-party notices 与 release package 同步检查。

### Phase E: Regression gate

所有 API/package/compliance 清理都必须经过同一个回归漏斗：

1. clean configure/build
2. CTest
3. install/export/package-consumer
4. `scripts/verify-v1.sh`
5. TURN relay E2E（当本地 TURN config 存在或 CI secret 可用）
6. summary JSON redaction/assertion 检查

## 构建顺序建议

1. 先做 API surface inventory 和文档化限制。
2. 再做命名/compatibility 层，避免清理过程中失去可验证基线。
3. 接着加固 package/release 验收。
4. 然后做 license/source audit。
5. 最后统一跑 v1.0 E2E regression，证明剥离收口没有退化协议能力。

## Sources

- CMake Importing and Exporting Guide: https://cmake.org/cmake/help/v3.23/guide/importing-exporting/index.html
- CMake relocatable package guidance: https://cmake.org/cmake/help/v3.22/command/target_include_directories.html
- Semantic Versioning 2.0.0: https://semver.org/
- REUSE Specification 3.3: https://reuse.software/spec-3.3/
