所有 Markdown（`.md`）文档尽可能使用中文编写。

## 项目上下文

本仓库使用 GSD 规划目录 `.planning/` 管理项目上下文、需求、研究、路线图和状态。

优先阅读：

1. `.planning/PROJECT.md`
2. `.planning/REQUIREMENTS.md`
3. `.planning/ROADMAP.md`
4. `.planning/STATE.md`
5. 当前阶段目录中的 `PLAN.md`、`SPEC.md`、`UI-SPEC.md` 或其他阶段文档

## 工作约束

- 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发的项目边界。
- 不要引入 GPL/LGPL 依赖。
- Markdown 文档必须使用中文；技术标识符、API 名称、协议名和需求 ID 可以保留英文。
- 修改规划文档时同步维护需求追踪和项目状态。
