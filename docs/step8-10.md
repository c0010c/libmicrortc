### ID 8-10 实施计划（README + 最小构建脚本 + M0-Base）

#### Summary
- 完成 `docs/plan.md` 的 `id=8~10`，仅改文档与脚本，不触碰核心代码行为。
- 采用你确认的“方案A 最小稳态”：
  - `README` 只写边界与最小入口；
  - `build_min.sh` 做最小可复现构建闭环；
  - 里程碑独立文档记录 `M0-Base`。
- 参考库对齐点：沿用“脚本单一职责、文档分层”的思路，不把进度记录塞进构建说明。

#### Key Changes
- 新增 [README.md](/home/qshl/dev/rtc/s2/README.md)
  - 固定章节：项目目标、当前范围、非目标、编解码边界、编译开关默认值、最小构建命令、里程碑链接。
  - 必含关键词用于验收：`不含信令`、`不含编解码`、`TURN 默认 OFF`（可写成 `TURN: OFF`）。
- 新增 [build_min.sh](/home/qshl/dev/rtc/s2/scripts/build_min.sh)
  - 固定流程：`cmake configure -> cmake build -> ctest`。
  - 显式传关键开关：`WEBRTC_ENABLE_AUDIO/VIDEO/DATA_CHANNEL/TURN`、`WEBRTC_USE_MBEDTLS`、`WEBRTC_PORT_POSIX/RTOS`。
  - Shell 规则：`set -eu`（可加 `-o pipefail`），任一步失败即退出非零。
- 新增 [milestones.md](/home/qshl/dev/rtc/s2/docs/milestones.md)
  - 记录 `M0-Base`：完成内容、验收命令、当前平台验证边界（仅宿主机 POSIX）。
  - 在 README 放到该文档的跳转链接。

#### Public APIs / Interfaces / Types
- 不新增、不修改公共 API/ABI/类型定义。
- 仅新增一个开发脚本接口：`scripts/build_min.sh`（构建入口脚本）。

#### Test Plan
1. `rg -n "不含信令|不含编解码|TURN.*OFF" README.md`
2. `bash scripts/build_min.sh`
3. `rg -n "M0-Base" docs -S`
4. （可选补充）`cmake -S . -B build/base -LA | rg WEBRTC_`，确认脚本中的显式开关与当前默认值一致。

#### Assumptions
- 文档语言使用中文，和现有仓库风格一致。
- `build_min.sh` 目标是“最小可验证闭环”，不是矩阵脚本，不覆盖 RTOS 实机验证。
- 本轮不扩展到 `id=11+`，不引入任何信令、编解码或平台专用核心依赖。
