# 里程碑记录

## M0-Base
- 日期：2026-03-15
- 状态：完成

### 完成内容
- 完成工程基线（目录骨架、根 CMake、最小 API、最小 smoke test）。
- 补充边界文档（`README.md`）：明确不含信令、不含编解码、TURN 默认 OFF。
- 新增最小构建脚本（`scripts/build_min.sh`）：显式关键开关并执行 configure/build/ctest 闭环。

### 验收命令
```bash
rg -n "不含信令|不含编解码|TURN.*OFF" README.md
bash scripts/build_min.sh
rg -n "M0-Base" docs -S
cmake -S . -B build/base -LA | rg WEBRTC_
```

### 平台验证边界
- 已验证：宿主机 POSIX 构建链路（Linux）。
- 未验证：RTOS 端口运行时行为与真机联调。
