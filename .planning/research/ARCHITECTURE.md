# Architecture Research

**Domain:** libmicrortc v1.1 API 命名/代码风格清理  
**Researched:** 2026-05-15  
**Confidence:** HIGH，本结论基于本地 `.planning/PROJECT.md`、`CMakeLists.txt`、`include/`、`src/`、`tests/`、`examples/` 和 `scripts/verify-v1.sh`。未使用外部资料。

## 结论摘要

v1.1 应被规划为一次“公共 ABI/API 面收口 + 内部命名跟随”的破坏式重命名，而不是协议栈重构。现有架构已经有清晰的安装边界：`include/micrortc/*.h`、`micrortc` CMake target、`micrortc::micrortc` package export、README 中记录的调用模型，以及 `tests/package-consumer` 构成下游可见面。`src/` 下协议模块原则上是内部面，即使若干测试和 Chrome E2E answerer 现在直接包含了内部头。

风险最大的不是函数实现改名，而是误把内部白盒测试接口当成公共 API 发布，或在公共头重命名后遗漏 install/export、package consumer、Chrome E2E answerer、README 和验收脚本中的旧符号。推荐的实施顺序是先冻结 API 映射表，再改公共头和公共实现入口，然后只让 public-only 测试/consumer 编译通过，最后处理内部模块、白盒测试、示例和文档。

命名策略应服从 `.clang-tidy`：类型/typedef/enum 类型使用 `CamelCase`，函数、变量、参数、member 使用 `lower_case`，宏和枚举常量保持 `UPPER_CASE`。因此公共函数 `mrtc_*` 已经大体符合目标，优先清理的是 `MRTC_STATUS`、`MRTC_PEER_CONNECTION_HANDLE`、`MRTC_FRAME` 等 typedef/struct/enum 类型名，以及 `src/` 内部类型名。枚举常量和宏可以继续使用 `MRTC_*` 前缀，但应确保语义是 libmicrortc 而非 AWS/KVS。

## 当前系统概览

```
应用/测试层
├── examples/chrome-e2e/mrtc_chrome_answerer.c
├── tests/peer_connection、tests/smoke、tests/package-consumer
├── tests/media、tests/transport、tests/integration 白盒协议测试
└── scripts/verify-v1.sh 串联 build、CTest、Chrome host、可选 TURN
        │
        ▼
公共 API 层
├── include/micrortc/micrortc.h
└── include/micrortc/peer_connection.h
        │
        ▼
核心编排层
├── src/micrortc.c
├── src/peer_connection.c
└── src/sdp.c
        │
        ▼
协议模块层
├── ICE/STUN/TURN: src/ice、src/stun、src/turn
├── DTLS/SRTP/SCTP/DataChannel: src/dtls、src/srtp、src/sctp、src/data_channel
├── Media/RTP/RTCP/Codecs: src/media、src/rtp、src/rtcp
└── Common: src/common
        │
        ▼
构建/发布层
├── cmake/MicroRtcSources.cmake
├── cmake/MicroRtcTests.cmake
├── cmake/MicroRtcInstall.cmake
└── cmake/micrortcConfig.cmake.in
```

## 公共面与内部面判定

### 明确公共面

| 组件/文件 | 公共性 | v1.1 处理 |
|-----------|--------|-----------|
| `include/micrortc/micrortc.h` | 安装导出的 umbrella header | 必改。`MRTC_STATUS` 重命名为 CamelCase 类型；`mrtc_version_string()`、`mrtc_initialize()`、`mrtc_shutdown()` 可保持 lower_case，除非 API 映射表决定统一词汇。 |
| `include/micrortc/peer_connection.h` | 安装导出的主要 API | 必改。opaque handle、config/callback/frame/transceiver/data channel/enum 类型是核心重命名对象。 |
| `micrortc` target、`micrortc::micrortc` alias/export | 下游 CMake 契约 | 不建议改名。库 target 和 package 名已是独立项目语义，改它会增加迁移成本且不服务本次目标。 |
| `tests/package-consumer` | package/export 验收消费者 | 必须随公共 API 改名，是 public API 是否可被安装消费的第一道验收。 |
| README 公共 API 列表与安装说明 | 用户文档面 | 必须更新，并新增旧 API 到新 API 的迁移表。 |

### 内部面

| 组件/文件 | 内部性 | v1.1 处理 |
|-----------|--------|-----------|
| `src/common/*` | 内部基础能力 | 可跟随 `.clang-tidy` 清理类型名，但不应进入安装头。 |
| `src/sdp.*` | 内部 SDP parser/generator | 仍作为 PeerConnection 的实现细节；测试可继续白盒覆盖，但不要发布为 public SDP API，除非单独立项。 |
| `src/ice/*`、`src/stun/*`、`src/turn/*` | 内部传输协议模块 | 命名可清理；配置文件 loader 当前只服务测试/answerer，不应成为 v1.1 public API。 |
| `src/dtls/*`、`src/srtp/*`、`src/sctp/*` | 内部安全/数据传输模块 | 保持在 `src/` 私有 include 边界内。 |
| `src/rtp/*`、`src/rtcp/*`、`src/media/*` | 内部媒体/RTP/RTCP 实现 | public 只暴露 encoded frame/transceiver 抽象；RTP packet、RTCP packet、rolling buffer 等不发布。 |
| `src/data_channel/*` | 内部 DataChannel handle 实体 | public 只暴露 opaque handle 和 send/close/callback。 |

### 内部但已泄漏到测试/示例的面

这些不是 public API，但会在 v1.1 改名时造成大量编译失败，需要在路线图中显式安排：

| 使用方 | 当前泄漏 | 建议 |
|--------|----------|------|
| `examples/chrome-e2e/mrtc_chrome_answerer.c` | 包含 `"media/media_transceiver.h"`，使用 `mrtc_peer_connection_set_media_send_hook()`、`mrtc_peer_connection_receive_protected_media_packet()` 并读取 transceiver 内部字段 | 短期保留为 E2E harness 私有钩子，但改名时归入“示例/验收适配”阶段；中期应迁移到 `tests/harness` 或引入 private harness header，避免示例看起来像公共用法。 |
| `tests/sdp/test_sdp_roundtrip.c` | 直接包含 `src/sdp.h` 和 `src/media/media_transceiver.h` | 归类为白盒单测；跟随内部命名改，不影响公共 API。 |
| `tests/media/*`、`tests/transport/*`、`tests/integration/*` | 多处直接包含 `src/` 内部协议头 | 保留白盒覆盖价值，但构建顺序上应晚于 public-only 编译验收。 |
| `MicroRtcTests.cmake` 的 `WITH_PRIVATE_INCLUDES` | 允许测试包含 `src` | 不要移除；v1.1 只是更清楚地区分 public-only tests 与 private white-box tests。 |

## 推荐命名边界

### 公共类型

公共类型建议从全大写 typedef 收敛到 CamelCase，函数保持 `mrtc_` lower_case。例如：

| 当前公共符号 | 推荐方向 | 理由 |
|--------------|----------|------|
| `MRTC_STATUS` | `MrtcStatus` | typedef/enum 类型应为 CamelCase。 |
| `MRTC_PEER_CONNECTION_HANDLE` | `MrtcPeerConnectionHandle`，或更简洁的 `MrtcPeerConnection *` opaque pointer | 类型名 CamelCase；opaque handle 仍保留 C API 的封装边界。 |
| `MRTC_DATA_CHANNEL_HANDLE` | `MrtcDataChannelHandle` | 与 PeerConnection handle 一致。 |
| `MRTC_RTP_TRANSCEIVER_HANDLE` | `MrtcTransceiverHandle` 或 `MrtcRtpTransceiverHandle` | 若 public API 只表达 WebRTC transceiver，推荐去掉过长的 RTP 暴露；若要保守迁移，使用 `MrtcRtpTransceiverHandle`。 |
| `MRTC_ICE_SERVER` | `MrtcIceServer` | ICE server 是 public config 的一部分。 |
| `MRTC_FRAME` | `MrtcFrame` | public encoded-frame 输入/输出核心类型。 |
| `MRTC_*_CALLBACKS` / `MRTC_*_CONFIG` / `MRTC_*_INIT` | `Mrtc*Callbacks` / `Mrtc*Config` / `Mrtc*Init` | 与 `.clang-tidy` TypedefCase 对齐。 |

### 公共枚举常量和宏

枚举类型应改为 CamelCase；枚举常量可继续 `MRTC_*` 全大写，因为 `.clang-tidy` 要求 enum constants 为 `UPPER_CASE`。不建议把 enum constants 改成 `Mrtc...`，否则会与当前 lint 目标冲突。

### 内部类型/函数

内部 `src/` 函数已经大多是 `mrtc_*` lower_case，可按模块渐进清理类型名：`MRTC_DTLS_SESSION` → `MrtcDtlsSession`、`MRTC_RTP_PACKET` → `MrtcRtpPacket`、`MRTC_STUN_MESSAGE` → `MrtcStunMessage`。内部改名不应改变 `MRTC_LIBRARY_SOURCES` 的模块边界。

## 数据流影响

### Signaling/SDP 流

```
应用层 signaling
    │  remote offer / ICE candidate 字符串
    ▼
mrtc_peer_connection_set_remote_description()
mrtc_peer_connection_add_ice_candidate()
    │
    ▼
src/peer_connection.c 编排 SDP、ICE、DTLS fingerprint、SCTP/DataChannel、media transceiver
    │
    ▼
mrtc_peer_connection_create_answer()
mrtc_peer_connection_set_local_description()
    │  local answer / candidate 字符串
    ▼
应用层 signaling
```

v1.1 命名清理不应改变 SDP/ICE 字符串边界，也不应把 signaling transport 加入核心库。只需要更新函数签名中的类型名和调用点。

### DataChannel 流

```
public create_data_channel / on_data_channel callback
    ▼
src/data_channel/data_channel.*
    ▼
src/sctp/sctp_session.*
    ▼
src/dtls/dtls_session.*
    ▼
ICE/TURN selected packet send
```

DataChannel 的 public 面是 opaque handle、message type、callback table、send/close/label/id。内部 SCTP PPID、DCEP、test frame 仍是私有实现。

### 媒体流

```
public add_transceiver + write_frame / on_frame / on_picture_loss
    ▼
src/media/media_transceiver.*
    ▼
H264/Opus packetize/depayload
    ▼
RTP/RTCP + rolling buffer + retransmitter
    ▼
SRTP protect/unprotect
    ▼
PeerConnection selected transport send/receive
```

v1.1 不应改变“核心库只处理编码后媒体帧”的边界。`MrtcFrame` 这类 public frame 类型是关键迁移点；RTP packet、RTCP report、NACK retransmit 等保持内部。

## 构建与测试流变化

### 当前构建入口

`CMakeLists.txt` 通过 `cmake/MicroRtcSources.cmake` 枚举 `MRTC_LIBRARY_SOURCES`，创建静态库 `micrortc`，公开 include 目录为 `include/`，私有 include 目录为 `src/`。install/export 通过 `cmake/MicroRtcInstall.cmake` 安装整个 `include/` 目录并导出 `micrortc::micrortc`。

### v1.1 需要修改的构建/测试点

| 文件/组件 | 修改类型 | 目的 |
|-----------|----------|------|
| `include/micrortc/*.h` | 修改 | 新公共类型名、header guard、迁移后的声明。 |
| `src/micrortc.c`、`src/peer_connection.c` | 修改 | 实现签名与公共头一致；内部 struct tag 改为新类型名。 |
| `src/*/*.h`、`src/*/*.c` | 修改 | 内部类型名/字段名/enum 类型名按 `.clang-tidy` 收敛。 |
| `cmake/MicroRtcInstall.cmake` | 可能修改 | 若公共头文件拆分/改名，确保安装目录只导出目标 public header。 |
| `cmake/MicroRtcTests.cmake` | 修改 | 若测试目标源名或分层验收目标增加，更新 test target；建议新增/强调 public-only API compile test。 |
| `tests/package-consumer` | 修改 | 验证 install/export 后新 API 可被外部 CMake 项目消费。 |
| `tests/peer_connection/test_peer_connection_api.c`、`tests/smoke/test_link.c` | 修改 | 第一批 public API 行为回归。 |
| `examples/chrome-e2e/mrtc_chrome_answerer.c` | 修改 | Chrome E2E answerer 使用新 public 类型；私有 media hook 同步内部命名。 |
| `README.md`、迁移文档 | 新增/修改 | 说明旧符号到新符号映射和破坏性变更。 |
| `scripts/verify-v1.sh` | 尽量不改 | 保持验收入口稳定；只有 summary 名称或构建参数确需变化时才改。 |

## 新增组件与修改组件

### 建议新增

| 新组件 | 位置 | 原因 |
|--------|------|------|
| API 映射/迁移文档 | `docs/api-migration-v1.1.md` 或 README 中独立章节 | 破坏式公共 API 改名必须给用户明确迁移表；也是实施前冻结范围的依据。 |
| public API inventory | `.planning/` 阶段产物或 `docs/` 中临时清单 | 用 `include/` + package consumer + README API 列表确认哪些符号是 public。 |
| public-only compile/package test 强化 | `tests/package-consumer` 或新增 `tests/public-api` | 让公共头/API 问题在白盒内部测试之前暴露。 |

### 应修改

| 修改组件 | 修改重点 |
|----------|----------|
| `include/micrortc/micrortc.h` | `MRTC_STATUS` 去全大写 typedef；避免 status 在两个 public headers 中重复定义的维护成本。可考虑拆出 `include/micrortc/types.h`，但只有确实减少重复时再新增。 |
| `include/micrortc/peer_connection.h` | 所有 public typedef/enum/struct 类型名改 CamelCase；函数仍以 `mrtc_` 前缀保留。 |
| `src/peer_connection.c` | struct tag、成员类型、函数签名同步；保持 PeerConnection 对协议模块的编排职责不变。 |
| `src/media/media_transceiver.h` | 明确私有 hook 和 transceiver 实体为 internal，不随 install 暴露；同步新 public 类型名。 |
| `tests/*` | 分两批更新：public-only tests 先更新，private white-box tests 后更新。 |
| `examples/chrome-e2e/*` | C answerer 和 README 更新，保证 Chrome E2E 仍覆盖双向 DataChannel 和 H264/Opus。 |

### 不建议新增或修改

| 组件 | 决策 |
|------|------|
| `micrortc` CMake project/target/package 名 | 保持不变，已经是独立 libmicrortc 语义。 |
| signaling transport | 不新增，继续由应用/test harness 负责。 |
| 媒体采集/编码 adapter | 不新增，继续只接收/输出 encoded frame。 |
| 协议模块拆分/重构 | 不作为 v1.1 主任务；命名清理期间重构会放大回归面。 |

## 推荐实施顺序

1. **冻结公共 API inventory 和迁移表**
   - 来源：`include/micrortc/*.h`、README API 列表、`tests/package-consumer`、`tests/peer_connection/test_peer_connection_api.c`、`examples/chrome-e2e/mrtc_chrome_answerer.c`。
   - 输出：旧符号 → 新符号映射；标注 public、private、private-test-only。
   - 验收：`rg` 旧 public 类型名的结果可解释，无未分类符号。

2. **先改公共头和最小实现入口**
   - 修改 `include/micrortc/*.h`、`src/micrortc.c`、`src/peer_connection.c` 的函数签名与 public 类型。
   - 暂时允许内部模块仍使用旧内部类型名，但 public 编译单元不能再暴露旧 public typedef。
   - 验收：`cmake -S . -B build -DMRTC_BUILD_TESTS=OFF`、`cmake --build build`。

3. **更新 public-only 消费者**
   - 更新 `tests/smoke/test_link.c`、`tests/peer_connection/test_peer_connection_api.c` 中只依赖 public header 的部分、`tests/package-consumer/package_consumer.c`。
   - 安装并构建 package consumer，确认 install/export 未漏新头。
   - 验收：`cmake --install build --prefix build/install`；`cmake -S tests/package-consumer -B build/package-consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"`；`cmake --build build/package-consumer`。

4. **更新内部协议模块和白盒测试**
   - 按模块顺序改：common → SDP → ICE/STUN/TURN → DTLS/SRTP/SCTP/DataChannel → RTP/RTCP/media。
   - 每改一组就运行相关 CTest label 或目标，避免一次性大改后难定位。
   - 验收：`ctest --test-dir build --output-on-failure`。

5. **更新 Chrome E2E answerer 和示例文档**
   - 同步 `examples/chrome-e2e/mrtc_chrome_answerer.c` public 类型名和私有 media hook 类型名。
   - 保持 stdin/stdout JSON line signaling 协议不变，避免同时改浏览器 harness。
   - 验收：`scripts/verify-v1.sh --skip-npm-install` 或完整 `scripts/verify-v1.sh`。

6. **更新 README 和迁移文档**
   - README public API 列表替换为新符号。
   - 迁移文档列出旧类型/枚举/函数/头文件到新命名；明确 v1.1 不保持 AWS 风格源码兼容。
   - 验收：文档中的代码片段能由 package consumer 或 public API compile test 覆盖。

7. **最终全量验证与残留扫描**
   - 必跑：`scripts/verify-v1.sh`。
   - 有 TURN 配置时跑：`scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json`。
   - 扫描旧公共符号：`rg 'MRTC_(STATUS|PEER_CONNECTION|DATA_CHANNEL|RTP_TRANSCEIVER|FRAME|ICE_SERVER|TRANSCEIVER)' include tests/package-consumer examples README.md docs`，剩余命中必须是迁移文档或 enum constants/macros。

## 回归可观测性

| 阶段 | 最快发现的问题 | 推荐命令 |
|------|----------------|----------|
| 公共头改名后 | public header 自洽、核心库能否编译 | `cmake -S . -B build -DMRTC_BUILD_TESTS=OFF && cmake --build build` |
| package/export 后 | install header、CMake namespace、外部消费缺失 | package consumer 构建与运行 |
| 白盒测试后 | 内部协议类型、私有 hook、RTP/RTCP/STUN/SCTP 编译与行为 | `ctest --test-dir build --output-on-failure` |
| Chrome host 后 | SDP/ICE/DataChannel/media 实际互通 | `scripts/verify-v1.sh` |
| TURN relay 后 | relay candidate 和真实 TURN 路径未回退 | `scripts/verify-v1.sh --turn-config ./mrtc-ice-servers.local.json` |

## 反模式

### 反模式 1：先全仓机械替换，再看哪里坏

**问题：** public API、内部实现、白盒测试、文档和 E2E harness 会同时失败，无法判断是命名映射错误、install/export 遗漏，还是协议行为回归。  
**替代：** 先 public inventory，再 public-only 编译，再内部模块分批更新。

### 反模式 2：为降低迁移痛苦保留旧 public typedef 别名

**问题：** v1.1 的目标是破坏式清理；保留旧别名会让旧 AWS/KVS 风格继续成为公共面，后续很难删除。  
**替代：** 只在迁移文档保留旧名映射；代码层面不要在 `include/` 中导出旧 typedef。若确需临时别名，应仅用于本地过渡分支，不能进入最终 v1.1。

### 反模式 3：把 `src/` 白盒测试接口提升到 public header

**问题：** RTP packet、RTCP packet、SRTP passthrough、media send hook 等接口是测试/实现细节，发布后会绑死内部协议结构。  
**替代：** 继续通过 private includes 和测试 harness 覆盖内部行为；public API 只暴露 PeerConnection、DataChannel、transceiver、encoded frame、SDP/ICE 字符串边界。

### 反模式 4：命名清理时顺手重构 signaling 或媒体边界

**问题：** 会把 API 命名风险和架构扩展风险叠加，Chrome/TURN/media E2E 一旦失败难以定位。  
**替代：** v1.1 不新增 signaling、不新增采集/编码、不改变 E2E JSON line 协议。

## Roadmap 集成建议

建议 roadmap phases 按以下顺序拆：

1. **公共 API inventory 与命名规范冻结**  
   产出 public/private/private-test-only 清单和迁移表，避免后续阶段争论边界。

2. **公共头和核心 PeerConnection API 改名**  
   只改 `include/`、`src/micrortc.c`、`src/peer_connection.c` 和 public-only tests，让最大用户可见风险先暴露。

3. **内部协议模块命名收敛**  
   按模块更新 `src/` 和白盒测试，保持 `MRTC_LIBRARY_SOURCES` 和协议行为不变。

4. **package/export、示例、Chrome E2E harness 更新**  
   更新 install/export consumer、`mrtc_chrome_answerer`、examples 文档；确认实际浏览器互通仍通过。

5. **文档、迁移指南和全量验收**  
   README + migration doc + `scripts/verify-v1.sh` + 可选 TURN relay，最后残留扫描旧 public 类型名。

这个顺序的关键是让回归从“编译可见”到“协议行为可见”逐层出现：public header/package 先失败，内部 CTest 次之，Chrome/TURN E2E 最后验证真实互通。

## Sources

- `.planning/PROJECT.md`：v1.1 目标、范围、命名约束和 v1.0 验收边界。
- `.planning/ROADMAP.md`：v1.0 已完成阶段和当前 roadmap 状态。
- `.clang-tidy`：identifier naming 规则。
- `CMakeLists.txt`、`cmake/MicroRtcSources.cmake`、`cmake/MicroRtcTests.cmake`、`cmake/MicroRtcInstall.cmake`：构建、测试、install/export 边界。
- `include/micrortc/micrortc.h`、`include/micrortc/peer_connection.h`：当前公共 API。
- `src/`：当前协议模块结构和内部头。
- `tests/`、`tests/package-consumer`：public API、白盒协议和 package consumer 验收。
- `examples/chrome-e2e/mrtc_chrome_answerer.c`、`scripts/verify-v1.sh`：Chrome E2E 与总验收入口。

---
*Architecture research for: libmicrortc v1.1 API 命名/代码风格清理*  
*Researched: 2026-05-15*
