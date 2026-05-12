# Phase 1: 基线、范围与合规边界 - Pattern Map

**Generated:** 2026-05-12T18:58:55+08:00
**Status:** Ready for planning

## PATTERN MAPPING COMPLETE

## 目标文件与最近参考

| Planned file | Role | Closest source/analog | Pattern to reuse |
|--------------|------|-----------------------|------------------|
| `.planning/phases/01-/BASELINE.md` | 本地参考库基线快照 | `.planning/PROJECT.md`, `.planning/ROADMAP.md`, `reflib/kvs-webrtc-sdk/CMakeLists.txt`, `reflib/kvs-webrtc-sdk/src/CMakeLists.txt` | 使用中文 Markdown 标题、表格和可复查命令；记录事实来源和约束，不修改代码 |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | 后续派生文件来源追溯规范 | `.planning/phases/01-/01-CONTEXT.md`, `reflib/kvs-webrtc-sdk/src/source/*` | Markdown 表格；字段至少包含 `source_path`、`target_path`、`origin_commit`、`derivation`、`notes` |
| `.planning/phases/01-/COMPLIANCE.md` | 许可证、NOTICE、第三方依赖策略 | `reflib/kvs-webrtc-sdk/LICENSE`, `reflib/kvs-webrtc-sdk/NOTICE`, `reflib/kvs-webrtc-sdk/CMake/Dependencies/*` | 以 Apache-2.0/NOTICE/依赖清单为核心；按依赖名、来源、版本、用途、v1 状态记录 |

## 数据流

1. `git -C reflib/kvs-webrtc-sdk ...` 提供 commit、tag、dirty 状态。
2. `reflib/kvs-webrtc-sdk/CMakeLists.txt` 与 `src/CMakeLists.txt` 提供构建选项、target 和链接关系。
3. `reflib/kvs-webrtc-sdk/src/source` 提供模块边界分类输入。
4. `reflib/kvs-webrtc-sdk/CMake/Dependencies` 提供第三方依赖来源和版本输入。
5. 三份 Phase 1 文档输出后，后续 Phase 2+ 以 `SOURCE-MANIFEST.md` 追加实际派生文件记录。

## 执行注意事项

- 不要预填 `reflib` 全量源码文件；Phase 1 只建立 manifest 格式和核心候选模块清单。
- `Signaling` 必须在 `BASELINE.md` 和 `SOURCE-MANIFEST.md` 中被标注为 `excluded`。
- `kvsCommonLws` / PIC 公共库能力应标注为 `supporting/reference-only`，避免被误认为长期核心边界。
- `COMPLIANCE.md` 应明确根目录/发布包后续需要 LICENSE 与 NOTICE，但本阶段不要求直接修改根目录。
