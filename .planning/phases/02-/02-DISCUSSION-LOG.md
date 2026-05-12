# Phase 2: 独立构建与库骨架 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-12T19:34:59+08:00
**Phase:** 2-独立构建与库骨架
**Areas discussed:** 骨架目录与 target 命名, 首版静态库内容边界, 第三方依赖入口, 构建验收与溯源更新

---

## 骨架目录与 Target 命名

| Option | Description | Selected |
|--------|-------------|----------|
| 清爽 libmicrortc 布局 | `include/micrortc/`、`src/`、`cmake/`、`tests/`；CMake target 用 `micrortc`，生成 `libmicrortc.a`。 | ✓ |
| AWS 过渡布局 | 保留类似 `include/com/amazonaws/...` 的公开头路径和较多 AWS 命名，target 暂时沿用 `kvsWebrtcClient` 或兼容别名。 | |
| 双层兼容布局 | 正式 target 用 `micrortc`，同时提供 AWS 风格兼容 include/alias。 | |

**User's choice:** 清爽 libmicrortc 布局。
**Notes:** 用户随后选择最小公共头 `include/micrortc/micrortc.h`，不提前放 PeerConnection/SDP/API 骨架；CMake package/export 采用 `micrortc::micrortc`，不提供 AWS/KVS alias。

---

## 首版静态库内容边界

| Option | Description | Selected |
|--------|-------------|----------|
| 最小可链接壳 | 只新增自有 `version/init` 等极小源码，让静态库有符号、smoke test 可链接；不搬 AWS 协议源码。 | ✓ |
| 搬入基础公共能力 | 迁入或派生 allocator/logging/error 基础层，为后续协议模块准备地基。 | |
| 先搬一个低风险协议模块 | 例如只搬 SDP/jsmn 或 STUN 的一小部分，让库不是空壳。 | |
| 只建空 target | 不放源码或只放 dummy。 | |

**User's choice:** 最小可链接壳。
**Notes:** 用户选择纯 C + 自有状态码风格，例如 `MRTC_STATUS` 与 `mrtc_*` 函数；不采用只暴露宏，也不沿用 AWS 的 `STATUS`、`PCHAR` 命名。PIC/kvsCommonLws 基础设施明确留空并记录待设计，不预留内部 allocator/logging/state 头，也不临时链接 `kvspicUtils` 或 `kvspicState`。

---

## 第三方依赖入口

| Option | Description | Selected |
|--------|-------------|----------|
| 先不强制依赖 | 最小壳不需要 OpenSSL、libsrtp、usrsctp；Phase 2 只创建边界说明，真正查找和链接等协议模块引入时再做。 | ✓ |
| 系统包优先 find_package | 现在就查找 OpenSSL、libsrtp、usrsctp，提前暴露环境问题。 | |
| ExternalProject 兼容 AWS 依赖构建 | 尽早复用参考库 dependency 下载/构建模式。 | |
| 系统包 + 可选 vendored 开关 | Phase 2 就建立双模式。 | |

**User's choice:** 先不强制依赖。
**Notes:** 用户选择轻量边界保护：核心 `micrortc` target 的 link list 保持极小，不链接 libwebsockets、AWS SDK、KVS signaling、credential/storage、GStreamer sample、kvsCommonLws/PIC 等 excluded 或 reference-only 依赖。依赖边界要同时写入 Phase context 和开发者可见构建文档。

---

## 构建验收与溯源更新

| Option | Description | Selected |
|--------|-------------|----------|
| 构建 + 链接 smoke test + install/export | 生成 `libmicrortc.a`；小测试程序 include `micrortc/micrortc.h` 并链接 `micrortc::micrortc`；验证安装后的 package/export 可消费。 | ✓ |
| 只要求生成静态库 | 只检查 `.a` 文件是否存在。 | |
| 再加 CI 脚本骨架 | 在 smoke test 和 install/export 基础上增加脚本/CI 入口。 | |
| 只做本地开发命令文档 | 不添加测试目标，靠 README 命令说明验收。 | |

**User's choice:** 构建 + 链接 smoke test + install/export。
**Notes:** 用户选择 `SOURCE-MANIFEST.md` 继续只记录 AWS 派生、参考或排除条目。Phase 2 自有骨架文件在 context 或 summary 中标为 `original`，不混入派生 manifest。

---

## the agent's Discretion

- Planner 可决定最小 API 的具体函数名、状态码枚举值、CMake 文件拆分方式、测试目录细节、install/export 文件名和构建文档落点。
- Planner 不可改变 target/export 命名、清爽目录布局、最小壳范围、不搬协议源码、不链接 AWS/KVS/PIC 依赖、验收覆盖 install/export 消费等已锁决策。

## Deferred Ideas

- PIC/kvsCommonLws 中 allocator、logging、state、utils 等基础能力的替换、裁剪或临时迁移策略留给后续协议模块搬迁阶段。
- PeerConnection、SDP、ICE candidate、DataChannel 和媒体 API 公共头设计留给 Phase 3+。
- OpenSSL/libsrtp/usrsctp 的系统包、pkg-config、vendored 或 ExternalProject 具体策略留给协议模块真正引入时决定。
