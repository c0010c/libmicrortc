# Phase 8: Public Headers 与核心 API 改名 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-15
**Phase:** 8-Public Headers 与核心 API 改名
**Areas discussed:** 改名切面边界, Opaque 类型细节, Private helper 暴露策略, 验证门槛

---

## 改名切面边界

| Question | Options Presented | Selected |
|----------|-------------------|----------|
| Phase 8 第一波改名应该覆盖到哪里？ | Headers+core+CTest; Headers+core only; Headers only | Headers+core+CTest |
| Phase 9 资产提前碰到什么程度？ | 只保持分类记录; 最小修到构建通过; 顺手全迁移 | 顺手全迁移 |
| repo 内旧 public symbol 使用要改到什么粒度？ | 映射表覆盖的旧 public symbol 全量迁移; 只改编译相关 C/CMake 资产; 只改直接包含 public header 的调用点 | 映射表覆盖的旧 public symbol 全量迁移 |
| 迁移后的 residual scan 应怎么判定？ | public-facing 零旧名; 所有非白名单源码零旧名; 只强制 installed headers 零旧名 | public-facing 零旧名 |

**Notes:** 用户要求先通俗解释 residual gate 后选择 public-facing 零旧名。最终边界是 Phase 8 做全量 public API 调用点迁移，但 private/internal 残留留给 Phase 10。

---

## Opaque 类型细节

| Question | Options Presented | Selected |
|----------|-------------------|----------|
| opaque struct tag 要不要同步改名？ | 同步改 opaque tag; 只改 typedef 名; planner 判断 | 同步改 opaque tag |
| public handle 类型还要不要带 Handle 后缀？ | 不带 Handle; 带 Handle; planner 判断 | 不带 Handle |
| 函数参数命名是否也顺手清理？ | 只做必要命名替换; 顺手统一 public 参数命名; planner 判断 | 只做必要命名替换 |
| struct 字段名要不要动？ | 不改字段名; 只修明显旧前缀字段; 统一字段风格 | 不改字段名 |

**Notes:** 用户锁定 Phase 7 symbol map 中的 handle 新名，不扩大 API 形状变化。

---

## Private helper 暴露策略

| Question | Options Presented | Selected |
|----------|-------------------|----------|
| Phase 8 要不要新增 public helper 来替代 private media helper？ | 不新增 public helper; 新增最小 public helper; 留给 planner 判断 | 不新增 public helper |
| private helper 自身要不要在 Phase 8 改名？ | 只做编译必需适配; 顺手改 private helper 名; 保持 private helper 完全不动 | 只做编译必需适配 |
| include-order gate 要覆盖多严格？ | 专门新增 include-order test; 靠 package consumer 间接验证; 只在 residual scan 中检查 | 专门新增 include-order test |
| public header private leak 的检查方式是什么？ | 编译 gate + 文本扫描; 只靠编译 gate; 只靠人工审查 | 编译 gate + 文本扫描 |

**Notes:** 用户明确不把 private media helper 升格 public；自动化 gate 要覆盖 public header 自包含和 private leak。

---

## 验证门槛

| Question | Options Presented | Selected |
|----------|-------------------|----------|
| Phase 8 完成时必须跑哪些验证？ | API gates + CTest + package consumer; 再加 scripts/verify-v1.sh; 只跑 API gates + CTest 子集 | 再加 scripts/verify-v1.sh |
| TURN relay E2E 是否作为 Phase 8 必跑？ | 存在配置就跑; 必须跑 TURN; Phase 8 不跑 TURN | 存在配置就跑 |
| residual scan 报告状态失败时是否阻塞？ | public-facing forbidden 必须阻塞; 报告生成即可; 所有 residual 必须为 0 | public-facing forbidden 必须阻塞 |
| 验证失败时 planner 应优先怎么处理？ | 先修 API/编译，再修 E2E; 任何失败都立即停下转 debug; API gates 必须过，E2E 可诊断记录 | 先修 API/编译，再修 E2E |

**Notes:** Phase 8 验证较重，默认 host `scripts/verify-v1.sh` 是完成门槛；TURN 只在本地配置存在时执行且不得泄露 secret。

---

## the agent's Discretion

无用户选择 “you decide” 的事项。Planner 可在决策边界内决定任务拆分、rename 顺序和测试文件命名。

## Deferred Ideas

None.
