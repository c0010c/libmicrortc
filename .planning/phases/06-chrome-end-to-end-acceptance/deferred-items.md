# 第 6 阶段延后事项

## 2026-05-11：npm audit 既有工具依赖告警

- **发现位置：** 执行 `06-01-PLAN.md` 时运行 `npm audit --audit-level=moderate`
- **问题：** `@anthropic-ai/sdk` 通过既有 `get-shit-done-cc` / `@anthropic-ai/claude-agent-sdk` 依赖链报告 2 个 moderate vulnerability。
- **范围判断：** 该依赖链来自仓库既有 GSD 工具依赖，不是 `06-01` 新增的 `ws` 或 `@playwright/test` 引入；本计划不升级 GSD 工具链。
- **建议处理：** 在独立工具链维护任务中评估 `npm audit fix` 或升级 `get-shit-done-cc`。
