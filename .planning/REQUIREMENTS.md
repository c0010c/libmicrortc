# Requirements: libmicrortc v1.1 API 清理

**Defined:** 2026-05-15
**Core Value:** 把 AWS KVS WebRTC C SDK 中可复用的 WebRTC 协议栈能力彻底剥离成一个独立、可构建、可验证、可逐步清理的 C 库。

## v1.1 Requirements

### API Contract

- [ ] **API-01**: 开发者可以在文档中看到冻结的 v1.1 public、private 和 test-only API 边界。
- [ ] **API-02**: 开发者可以查到完整旧名到新名映射，覆盖 `include/micrortc/*.h` 的 public symbols。
- [ ] **API-03**: 公共函数使用 `rtc_*` lower_case，公共类型、typedef 和 enum 类型收敛为 `Rtc*` CamelCase，宏和枚举常量使用 `RTC_*`。
- [ ] **API-04**: 公共 API 默认不保留旧 `mrtc_*`、`MRTC_*`、AWS/KVS 风格 typedef、handle 或 wrapper 兼容层。

### Public Headers

- [ ] **HDR-01**: 开发者可以只通过 installed `include/micrortc/*.h` 使用 v1.1 public API。
- [ ] **HDR-02**: `MRTC_*` 全大写 public typedef、struct 和 enum 类型被替换为 `Rtc*` CamelCase 类型。
- [ ] **HDR-03**: public header include-order 测试覆盖 umbrella header 和子头单独 include。
- [ ] **HDR-04**: public headers 不泄漏 private transport、RTP、RTCP、SRTP、SDP 或 media helper API。

### Consumer Migration

- [ ] **CONS-01**: `tests/package-consumer` 使用 installed package 和新 `rtc_*` API configure、build 和 run 通过。
- [ ] **CONS-02**: README 代码片段、public API tests 和 Chrome E2E answerer 全部迁移到新 API。
- [ ] **CONS-03**: PeerConnection、SDP、ICE candidate、DataChannel、transceiver 和 encoded frame 的现有行为通过新 API 测试覆盖。

### Style And Boundaries

- [ ] **STYLE-01**: `.clang-tidy` 命名规则进入 v1.1 验证闭环，并覆盖 `include`、`src`、`tests` 和 `examples`。
- [ ] **STYLE-02**: 内部类型、typedef、enum 和结构字段命名按 `.clang-tidy` 收敛，函数、变量、参数和 member 使用 lower_case。
- [ ] **STYLE-03**: 示例和 E2E harness 不再向用户展示 private struct 字段或 private header 作为正常 API。
- [ ] **STYLE-04**: 旧 AWS/KVS、`mrtc_*` 和 `MRTC_*` public API 残留扫描可以区分禁止残留、来源合规记录和迁移文档白名单。

### Documentation And Provenance

- [ ] **DOC-01**: 开发者可以阅读 v1.0 到 v1.1 迁移指南，包含 `mrtc_*`/`MRTC_*` 到 `rtc_*`/`Rtc*`/`RTC_*` 的映射、行为不变项和删除项。
- [ ] **DOC-02**: README、构建文档和发布说明明确 v1.1 是 source API breaking cleanup。
- [ ] **DOC-03**: AWS/KVS 派生文件改名或移动后的 provenance/source manifest 路径保持准确。

### Regression Gates

- [ ] **VER-01**: `ctest` 和 package consumer 验证通过。
- [ ] **VER-02**: `scripts/verify-v1.sh` 仍通过，并继续输出 `build/reports/mrtc-v1-summary.json`。
- [ ] **VER-03**: 存在 `mrtc-ice-servers.local.json` 时，TURN relay E2E 仍通过。
- [ ] **VER-04**: v1.1 总验收区分 API/style checks、package consumer、CTest、Chrome host E2E 和 TURN relay E2E。

## Future Requirements

Deferred to future releases. Tracked but not in current roadmap.

### API Governance

- **FUT-01**: API surface snapshot/report 可以作为后续公共 API 变更基线。
- **FUT-02**: Doxygen/API reference 可以在 public header 稳定后补充。
- **FUT-03**: ABI diff 工具可以等到 shared library 或 ABI 承诺出现后再引入。

### Feature Expansion

- **FUT-04**: Safari/Firefox、新 codec、simulcast、录制、屏幕共享、SFU/MCU 可在后续里程碑单独评估。
- **FUT-05**: GStreamer、FFmpeg 或其他媒体采集/编码 adapter 可在核心 API 清理完成后单独规划。
- **FUT-06**: 应用层 signaling adapter 或示例 SDK 可在保持核心库边界的前提下另行设计。

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Safari/Firefox 互通矩阵 | v1.1 聚焦 API 命名和代码风格清理，浏览器扩展会混淆验收。 |
| 新 codec、simulcast、录制、屏幕共享、SFU/MCU | 属于协议或产品能力扩展，不是 API cleanup 的完成条件。 |
| GStreamer/FFmpeg/媒体采集或编码 adapter | 核心库边界仍然只处理编码后媒体帧。 |
| 核心库内置 signaling adapter | signaling 属于应用层，demo/test 可以自动交换但不进入核心库。 |
| 旧 AWS/KVS 或 `mrtc_*` public API 兼容 wrapper 层 | 用户明确选择破坏式清理，并要求新前缀切换为 `rtc`。 |
| CMake package/target namespace 改名 | `micrortc::micrortc` 已是独立 package 接口，本里程碑只破坏 C public API。 |
| 协议状态机、SDP/ICE/DTLS/SRTP/SCTP/RTP 行为重构 | 命名清理不能混入协议语义变化；行为回归只用于证明 v1.0 能力未退化。 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| API-01 | TBD | Pending |
| API-02 | TBD | Pending |
| API-03 | TBD | Pending |
| API-04 | TBD | Pending |
| HDR-01 | TBD | Pending |
| HDR-02 | TBD | Pending |
| HDR-03 | TBD | Pending |
| HDR-04 | TBD | Pending |
| CONS-01 | TBD | Pending |
| CONS-02 | TBD | Pending |
| CONS-03 | TBD | Pending |
| STYLE-01 | TBD | Pending |
| STYLE-02 | TBD | Pending |
| STYLE-03 | TBD | Pending |
| STYLE-04 | TBD | Pending |
| DOC-01 | TBD | Pending |
| DOC-02 | TBD | Pending |
| DOC-03 | TBD | Pending |
| VER-01 | TBD | Pending |
| VER-02 | TBD | Pending |
| VER-03 | TBD | Pending |
| VER-04 | TBD | Pending |

**Coverage:**
- v1.1 requirements: 22 total
- Mapped to phases: 0
- Unmapped: 22

---
*Requirements defined: 2026-05-15*
*Last updated: 2026-05-15 after v1.1 requirement scoping*
