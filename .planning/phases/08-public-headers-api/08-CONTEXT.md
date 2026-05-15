# Phase 8: Public Headers 与核心 API 改名 - Context

**Gathered:** 2026-05-15
**Status:** Ready for planning

<domain>
## Phase Boundary

本阶段交付 v1.1 public C API 的实际改名落地：installed `include/micrortc/*.h`、核心实现签名、repo 内公开调用点和必要测试调用点都迁移到 `rtc_*`、`Rtc*`、`RTC_*`。Phase 8 必须让开发者可以通过 installed public headers 使用新 API，并证明 public headers 自包含、顺序无关、不会泄漏 private transport/RTP/RTCP/SRTP/SDP/media helper API。

本阶段不是协议行为重构，也不是 private/internal 命名全面清理。Private helper 可以为编译适配新 public types，但 private 函数名、内部 struct/type 的旧 `mrtc_*` / `MRTC_*` 残留留给 Phase 10 分类处理。

</domain>

<decisions>
## Implementation Decisions

### 改名切面边界

- **D-01:** Phase 8 改名切面覆盖 installed public headers、核心实现签名和必要 CTest 调用点。不能留下故意红灯等 Phase 9 再修。
- **D-02:** Phase 8 可以顺手全迁移 public API 调用点，包括 `tests/package-consumer`、README 代码片段和 `examples/chrome-e2e/mrtc_chrome_answerer.c`。Phase 9 后续重点转为 installed package consumer 验证、公开调用点审计和行为覆盖确认。
- **D-03:** `docs/api-v1.1-symbol-map.md` 覆盖的旧 public symbols 在源码、测试、示例、package consumer、README 代码片段中全量迁移到新名。迁移文档、symbol map、boundary doc、溯源/合规记录可继续提及旧名作为白名单事实。
- **D-04:** Phase 8 后 public-facing residual 必须清零：`include/`、README、公开示例、package consumer 不得残留旧 public symbol。`src/` private/internal 和 white-box tests 中的旧内部命名残留只分类记录，留给 Phase 10。

### Opaque 类型细节

- **D-05:** Public opaque handle 类型锁定为 `RtcPeerConnection`、`RtcDataChannel`、`RtcRtpTransceiver`，不带 `Handle` 后缀，并遵守 Phase 7 symbol map。
- **D-06:** Public header 中的 opaque struct tag 也要同步改名，不保留 `struct MRTC_*` tag。Planner 可选择类似 `struct RtcPeerConnectionImpl`、`struct RtcDataChannelImpl`、`struct RtcRtpTransceiverImpl` 的内部 tag。
- **D-07:** Public function 参数名只做必要命名替换，不顺手统一或重塑参数命名风格；参数语义、顺序和 API 形状尽量保持不变。
- **D-08:** Public struct 字段名不在 Phase 8 重命名。Phase 8 只改 symbol map 中冻结的 public type、typedef、enum type、function、macro 和 enum constant。

### Private helper 暴露策略

- **D-09:** Phase 8 不新增 public helper 来替代 private media helper。`src/media/media_transceiver.h` 等能力继续是 private/test-only；不能因为 E2E 或 white-box tests 方便而扩大 installed public API。
- **D-10:** Private helper 只做编译必需适配。它们可以跟随 public type rename 使用 `Rtc*`，但 private 函数名和内部 `mrtc_*` / `MRTC_*` 命名残留不作为 Phase 8 主目标。
- **D-11:** 新增专门 include-order C 测试，覆盖 `#include <micrortc/micrortc.h>` 单独编译、`#include <micrortc/peer_connection.h>` 单独编译、umbrella 先子头后、子头先 umbrella 后。
- **D-12:** Public header private leak 检查采用编译 gate + 文本扫描：include-order test 证明不依赖 `src/` private include；扫描检查 public headers 不出现 private include 或 private transport/RTP/RTCP/SRTP/SDP/media helper symbol。

### 验证门槛

- **D-13:** Phase 8 完成门槛包括 residual scan、include-order test、默认 CTest、package consumer build/run 和 `scripts/verify-v1.sh`。
- **D-14:** 如果 `./mrtc-ice-servers.local.json` 存在，执行 `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json`；如果不存在，记录 skipped，不阻塞 Phase 8。任何情况下都不得读取、打印或提交该本地 TURN secret 文件内容。
- **D-15:** Phase 8 后 residual scan 中 `forbidden_public_residual > 0` 必须阻塞。Migration docs、symbol map、boundary doc、溯源/合规记录和 private/internal/test harness 分类不阻塞本阶段。
- **D-16:** 验证失败时优先修 API/编译，再修 E2E。先确保 residual scan、include-order、CTest、package consumer 通过；`verify-v1.sh` 失败再按回归问题处理，必要时拆后续 fix plan。

### the agent's Discretion

无用户授权的自由裁量项。Planner 可以在不违反上述决策的前提下决定具体文件拆分、机械 rename 顺序、测试命名和扫描脚本实现细节。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project Authority

- `.planning/PROJECT.md` — 项目定位、v1.1 目标、API cleanup 决策和边界约束。
- `.planning/REQUIREMENTS.md` — Phase 8 覆盖 API-03、HDR-01、HDR-02、HDR-03、HDR-04；同时记录 Phase 9/10/11 的边界，避免 scope creep。
- `.planning/ROADMAP.md` — Phase 8 goal、success criteria 和 milestone sequencing。
- `.planning/STATE.md` — 当前状态、Phase 7 决策和 Phase 8 前置关注点。

### Phase 7 API Contract

- `docs/api-v1.1-boundary.md` — v1.1 public/private/test-only API 边界、删除策略、residual 分类契约和 Phase 8+ 使用说明。
- `docs/api-v1.1-symbol-map.md` — installed `include/micrortc/*.h` 旧 public symbols 到新 v1.1 names 的冻结映射。
- `.planning/phases/07-api/07-RESEARCH.md` — Phase 7 对 public/private/test-only API 盘点和 residual scan 策略的研究记录。

### Current Public API And Gates

- `include/micrortc/micrortc.h` — umbrella public header、status type 和 core lifecycle functions 当前来源。
- `include/micrortc/peer_connection.h` — PeerConnection、DataChannel、transceiver、encoded frame public API 当前来源。
- `scripts/scan-api-residuals.sh` — residual scan 入口；Phase 8 需要把 public-facing forbidden residual 从 baseline 清到 0。
- `cmake/MicroRtcTests.cmake` — CTest 注册模式、`WITH_PRIVATE_INCLUDES` test-only 边界和 Chrome answerer private include 设置。
- `tests/package-consumer/package_consumer.c` — installed package consumer 迁移和 build/run 验证入口。
- `examples/chrome-e2e/mrtc_chrome_answerer.c` — Chrome E2E answerer，目前使用 public API 也受控使用 private media helper。
- `scripts/verify-v1.sh` — v1.0 行为回归总入口；Phase 8 完成门槛必须执行默认 host verify，并在 TURN 配置存在时执行 TURN verify。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `docs/api-v1.1-symbol-map.md`: 可作为 mechanical rename 的权威旧名到新名表，避免 planner 重新发明命名。
- `scripts/scan-api-residuals.sh`: 已能生成 `build/reports/api-residuals.json`，并分类 `forbidden_public_residual`、`migration_doc_whitelist`、`source_compliance_record`、`test_harness_non_public_api`。
- `cmake/MicroRtcTests.cmake`: 已有 `mrtc_add_test` helper，可用于新增 include-order C 测试；`WITH_PRIVATE_INCLUDES` 也定义了 white-box test-only 边界。
- `tests/package-consumer`: 已是 package consumer 验证结构，Phase 8 应迁移到新 public API 并继续作为 installed package 面验证。
- `examples/chrome-e2e/mrtc_chrome_answerer.c` + `scripts/verify-v1.sh`: 可证明 public API 迁移没有破坏 Chrome host E2E 和媒体/DataChannel 行为。

### Established Patterns

- Public API 事实来源是 installed `include/micrortc/*.h`，不是 `src/`、tests 或 examples。
- CMake package/target namespace 保持 `micrortc::micrortc`，Phase 8 不改 CMake package identity。
- Tests 和 E2E harness 可以受控 include `src/`，但这种访问不能提升为 public API。
- Markdown docs 尽可能中文；迁移和合规文档可继续提及旧名作为白名单事实。
- 本地 `mrtc-ice-servers.local.json` 是 secret-bearing runtime config；只能检查存在性，不能读取内容。

### Integration Points

- Header rename 必须同步 `src/micrortc.c`、`src/peer_connection.c` 和依赖 public types 的 private/test-only code。
- Include-order gate 应接入 CTest，且测试 target 不应使用 `WITH_PRIVATE_INCLUDES`。
- Residual gate 应在 Phase 8 后把 public-facing forbidden residual 作为 blocker，而不是只生成 JSON。
- Package consumer 和 Chrome E2E answerer 的迁移纳入 Phase 8，但 private helper 的 public 化不纳入 Phase 8。

</code_context>

<specifics>
## Specific Ideas

- Public opaque implementation tag 倾向使用 `struct RtcPeerConnectionImpl` / `struct RtcDataChannelImpl` / `struct RtcRtpTransceiverImpl` 这类不暴露旧 `MRTC_*` 的 tag。
- Include-order test 至少覆盖四种编译单元：umbrella only、peer_connection only、umbrella then peer_connection、peer_connection then umbrella。
- Residual scan 完成标准要体现“public-facing 零旧名”，而不是“全 repo 零旧名”；Phase 10 仍负责 private/internal 命名收敛。

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 8-Public Headers 与核心 API 改名*
*Context gathered: 2026-05-15*
