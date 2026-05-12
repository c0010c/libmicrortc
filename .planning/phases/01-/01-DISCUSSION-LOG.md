# Phase 1: 基线、范围与合规边界 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-12T18:49:01+08:00
**Phase:** 1-基线、范围与合规边界
**Areas discussed:** 来源清单粒度, 核心/非核心边界, 合规文档形态, 基线快照策略

---

## 来源清单粒度

| Decision Point | Options Considered | Selected | Notes |
|----------------|--------------------|----------|-------|
| 来源追溯粒度 | 文件级清单 / 模块级清单 / 先模块后文件 | 文件级清单 | 后续每个派生文件都能追到 `reflib` 原路径。 |
| 清单覆盖范围 | 只覆盖派生到 libmicrortc 的文件 / 覆盖 reflib 全量源码文件 / 覆盖候选核心模块 + 派生文件 | 只覆盖派生到 libmicrortc 的文件 | Phase 1 定义格式和规则，实际条目随搬迁逐步追加。 |
| 最低字段 | 来源 + 目标 + 派生方式 + 基线提交 / 再加许可证 NOTICE 标记 / 再加文件哈希 | 来源 + 目标 + 派生方式 + 基线提交 | 最低字段为 `source_path`、`target_path`、`origin_commit`、`derivation`、`notes`。 |
| manifest 格式 | Markdown 表格 / JSON/YAML / 双格式 | Markdown 表格 | 优先人工审阅；Phase 1 不要求双格式。 |

**User's choice:** 以上均选择选项 1。  
**Notes:** 许可证字段和 hash 不作为每条记录必填项。

---

## 核心/非核心边界

| Decision Point | Options Considered | Selected | Notes |
|----------------|--------------------|----------|-------|
| 模块边界分类 | 三类边界 / 二类边界 / 先不分类到模块 | 三类边界 | 使用 `core`、`excluded`、`supporting/reference-only`。 |
| 初始 core 候选 | 只收协议栈硬核心 / 硬核心 + Threadpool/Metrics / 硬核心 + StaticMedia 测试输入 | 只收协议栈硬核心 | 包含 Ice、Stun、Crypto/DTLS、Srtp、Sctp、Sdp、Rtp、Rtcp、PeerConnection。 |
| Signaling 模块 | 明确 excluded / supporting/reference-only / 拆分评估 | 明确 excluded | 不允许成为核心库依赖。 |
| 支撑资产 | supporting/reference-only / 全部 excluded / 分开处理 | supporting/reference-only | 包括 Threadpool、Metrics、StaticMedia、固定 H264/Opus 样本。 |
| kvsCommonLws/PIC 公共库依赖 | supporting/reference-only / excluded / 按能力拆分 | supporting/reference-only | 可参考或临时迁移 allocator、logging、state、utils。 |
| 参考库测试 | supporting/reference-only / core / excluded | supporting/reference-only | 协议测试可作为迁移参考；Signaling 测试只作为 excluded 模块参考。 |

**User's choice:** 前四问选择选项 1；用户要求继续补问后，额外两问也选择选项 1。  
**Notes:** 该区域补问重点是公共库依赖和测试边界。

---

## 合规文档形态

| Decision Point | Options Considered | Selected | Notes |
|----------------|--------------------|----------|-------|
| Phase 1 文档产物 | 三份文档 / 一份总文档 / 两份文档 | 三份文档 | `BASELINE.md`、`SOURCE-MANIFEST.md`、`COMPLIANCE.md`。 |
| 文档位置 | 阶段目录 + 根目录引用 / 直接放根目录 / 放 docs/ | 阶段目录 + 根目录引用 | 先集中在 Phase 1 阶段目录，后续再整理到根目录。 |
| LICENSE/NOTICE 策略 | 保留 Apache-2.0 + 独立 NOTICE 衍生说明 / 直接复制 AWS LICENSE/NOTICE / 只在 COMPLIANCE.md 说明 | 保留 Apache-2.0 + 独立 NOTICE 衍生说明 | 根/发布包必须包含 LICENSE；NOTICE 保留 AWS 原 NOTICE 并添加 libmicrortc 派生说明。 |
| 第三方依赖记录 | 依赖名 + 来源 + 用途 + v1 状态 / 只记录 v1 核心运行依赖 / 等实际构建迁移时再记录 | 依赖名 + 来源 + 用途 + v1 状态 | 覆盖 runtime/core、reference-only、test-only、excluded。 |

**User's choice:** 以上均选择选项 1。  
**Notes:** 文档职责拆分清晰，避免把 baseline、manifest、compliance 混在一起。

---

## 基线快照策略

| Decision Point | Options Considered | Selected | Notes |
|----------------|--------------------|----------|-------|
| baseline 快照深度 | 可审计快照 / 最小快照 / 强审计快照 | 可审计快照 | 记录路径、tag/version、commit、dirty 状态、目录结构、模块边界、构建选项、依赖清单；不要求全量 hash。 |
| 第三方依赖版本 | CMake 声明来源 + 已知 tag/URL / 全部解析到精确 commit / 只列依赖名 | CMake 声明来源 + 已知 tag/URL | 无法确认的项标注需 Phase 2 构建确认。 |
| reflib dirty 状态 | 硬性前置条件 / 只记录当前状态 / 允许 dirty 但记录 diff | 硬性前置条件 | 若 dirty，必须先记录补丁或停止搬迁。 |
| 是否强调不跟随上游最新 | 明确写成约束 / 只在 PROJECT.md 保留 / 允许对比上游但不作为基线 | 明确写成约束 | `BASELINE.md` 和 `CONTEXT.md` 都要写明。 |

**User's choice:** 以上均选择选项 1。  
**Notes:** 用户选择准备生成 CONTEXT.md，没有继续补问。

---

## 执行裁量范围

- 可以决定三份文档的章节组织、表格列顺序和生成命令。
- 不可降低来源追溯粒度、改变 `Signaling` 的 excluded 边界，或把基线切换到 GitHub 上游最新版本。

## Deferred Ideas

无。
