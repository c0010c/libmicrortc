# S2 M2（含 DataChannel）实施计划（细粒度、可验证）

## 摘要
- 目标：在 `/home/qshl/dev/rtc/s2` 从零落地纯 C 轻量 WebRTC 核心库，达到 `M2`（音视频传输 + DataChannel）。
- 已锁定决策：`Linux 先跑通`、`RTOS 先保证可编译`、`默认 MbedTLS`、`不含信令`、`首版不含 TURN`。
- 完成判定：功能闭环测试通过 + 8 组 feature-gate 构建矩阵通过 + 分层与资源约束检查通过。

## 公共接口与类型（固定方案）
- 公共头：
`include/webrtc/webrtc.h`、`include/webrtc/webrtc_config.h`、`include/webrtc/webrtc_status.h`、`include/webrtc/webrtc_pump.h`、`include/webrtc/webrtc_pal.h`
- 核心 API：
`webrtc_init/deinit`、`webrtc_peer_connection_create/free`、`webrtc_set_local/remote_description`、`webrtc_add_ice_candidate`、`webrtc_pump_step`
- 编译开关：
`WEBRTC_ENABLE_AUDIO`、`WEBRTC_ENABLE_VIDEO`、`WEBRTC_ENABLE_DATA_CHANNEL`、`WEBRTC_ENABLE_TURN(OFF)`、`WEBRTC_USE_MBEDTLS(ON)`、`WEBRTC_PORT_POSIX(ON)`、`WEBRTC_PORT_RTOS(OFF)`
- 约束：
不暴露 POSIX 类型；不处理音视频编解码；默认无内部后台线程（外部 pump 驱动）。

## 详细计划表（每步可验收）

### Chunk 1：工程基线
| ID | 步骤 | 产出 | 验证命令 | 通过标准 |
|---|---|---|---|---|
| 1 | 建目录骨架（`include/src/ports/tests/cmake/scripts`） | 基础结构 | `find . -maxdepth 2 -type d | sort` | 目录齐全 |
| 2 | 新建根 `CMakeLists.txt`，仅静态库 `webrtc_core` | 可配置工程 | `cmake -S . -B build/base` | configure 成功 |
| 3 | 加入 feature-gate 默认值 | 开关可见 | `cmake -S . -B build/base -LA | rg WEBRTC_` | 默认值正确 |xu
| 4 | 新建最小公共头 `webrtc.h` | API 入口 | `test -f include/webrtc/webrtc.h` | 文件存在 |
| 5 | 新建 `src/api/webrtc_api.c` 空实现并入库 | 最小可编译库 | `cmake --build build/base -j` | 产出 `libwebrtc_core.a` |
| 6 | 新建 `tests/CMakeLists.txt` + 空 smoke test | 最小测试框架 | `ctest --test-dir build/base` | 至少 1 项 PASS |
| 7 | 统一选项到 `cmake/options.cmake` | 选项集中化 | `rg -n "option\\(WEBRTC_" cmake` | 选项集中 |
| 8 | README 写明范围/非目标 | 边界文档 | `rg -n "不含信令|不含编解码|TURN.*OFF" README.md` | 关键边界完整 |
| 9 | 新建 `scripts/build_min.sh` | 基线构建脚本 | `bash scripts/build_min.sh` | 退出码 0 |
| 10 | 记录里程碑 `M0-Base` | 里程碑说明 | `rg -n "M0-Base" docs -S` | 记录存在 |

### Chunk 2：错误码、配置、PAL 与端口层
| ID | 步骤 | 产出 | 验证命令 | 通过标准 |
|---|---|---|---|---|
| 11 | 定义 `webrtc_status.h`（CONFIG/SDP/ICE/DTLS/SRTP/RTP/SCTP） | 统一错误语义 | `rg -n "WEBRTC_STATUS_" include/webrtc/webrtc_status.h` | 分段完整 |
| 12 | 定义 `webrtc_config.h`（默认值+单位+边界） | 配置中心 | `rg -n "max_|timeout|queue" include/webrtc/webrtc_config.h` | 参数完整 |
| 13 | 实现 `webrtc_config_init_default()` | 默认配置函数 | `ctest --test-dir build/base -R test_config_defaults` | PASS |
| 14 | 实现 `webrtc_config_validate()` | 边界校验 | `ctest --test-dir build/base -R test_config_validate_bounds` | PASS |
| 15 | 定义 `webrtc_pal.h` vtable（mem/time/lock/socket/random） | PAL 契约 | `rg -n "webrtc_pal_vtable" include/webrtc/webrtc_pal.h` | 接口稳定 |
| 16 | 实现 `ports/posix/` 最小端口 | POSIX 端口 | `ctest --test-dir build/base -R test_pal_posix_smoke` | PASS |
| 17 | 实现 `ports/rtos_stub/` 空桩可编译 | RTOS 占位 | `cmake -S . -B build/rtos -DWEBRTC_PORT_POSIX=OFF -DWEBRTC_PORT_RTOS=ON && cmake --build build/rtos -j` | 构建通过 |
| 18 | 核心层改为仅依赖 PAL | 分层收敛 | `rg -n "pthread_|poll.h|unistd.h|fcntl.h|sys/" src/core include/webrtc` | 无命中 |
| 19 | 加 PAL 合约测试（时间单调/随机/基础收发） | 可移植接口验证 | `ctest --test-dir build/base -R test_pal_contract` | PASS |
| 20 | 文档化 init/deinit、create/free 对称关系 | 生命周期规则 | `rg -n "init|deinit|create|free" include/webrtc README.md` | 规则明确 |

### Chunk 3：Pump + SDP/STUN/ICE（Host+STUN）
| ID | 步骤 | 产出 | 验证命令 | 通过标准 |
|---|---|---|---|---|
| 21 | 定义 `webrtc_pump_step(ctx, now_ms, budget)` | 外部驱动接口 | `rg -n "webrtc_pump_step" include/webrtc/webrtc_pump.h` | API 可用 |
| 22 | 实现固定容量事件队列 | 有界队列 | `ctest --test-dir build/base -R test_event_queue_bounded` | PASS |
| 23 | 实现队列满背压返回码 | 背压语义 | `ctest --test-dir build/base -R test_event_queue_backpressure` | PASS |
| 24 | 实现 transport-only SDP 解析/序列化 | SDP 基础 | `ctest --test-dir build/base -R test_sdp_roundtrip_transport_only` | PASS |
| 25 | 按 A/V/DC gate 生成 m-line | 开关联动 | `ctest --test-dir build/base -R test_sdp_media_gate` | PASS |
| 26 | 实现 STUN Binding 编解码 | STUN 基础 | `ctest --test-dir build/base -R test_stun_encode_decode` | PASS |
| 27 | STUN 事务表固定上限 | 有界事务管理 | `ctest --test-dir build/base -R test_stun_txn_capacity` | PASS |
| 28 | 实现 ICE candidate 解析与上限控制 | 候选管理 | `ctest --test-dir build/base -R test_ice_candidate_parse` | PASS |
| 29 | 通过 PAL 收集 host candidates | Host gather | `ctest --test-dir build/base -R test_ice_host_gather_posix` | PASS |
| 30 | 实现 Host+STUN 连通性检查与选举 | ICE 主流程 | `ctest --test-dir build/base -R test_ice_loopback_connectivity` | PASS |
| 31 | `WEBRTC_ENABLE_TURN=OFF` 真裁剪验证 | TURN 不入库 | `cmake -S . -B build/no-turn -DWEBRTC_ENABLE_TURN=OFF && cmake --build build/no-turn -j && nm -g --defined-only build/no-turn/libwebrtc_core.a | rg -i turn` | 无 TURN 符号 |
| 32 | ICE 状态机测试（new/checking/connected/failed） | 状态可预测 | `ctest --test-dir build/base -R test_ice_state_machine` | PASS |

### Chunk 4：DTLS(MbedTLS)+SRTP+RTP/RTCP+A/V 传输封装
| ID | 步骤 | 产出 | 验证命令 | 通过标准 |
|---|---|---|---|---|
| 33 | 建 `src/proto/dtls` 抽象接口层 | 后端可替换性 | `rg -n "dtls_backend" src/proto/dtls -S` | 抽象存在 |
| 34 | 接入 MbedTLS DTLS 后端 | 默认后端 | `ctest --test-dir build/base -R test_dtls_mbedtls_init` | PASS |
| 35 | 完成本地双端握手（pump 驱动） | 握手闭环 | `ctest --test-dir build/base -R test_dtls_handshake_localpair` | PASS |
| 36 | 导出 SRTP keying material/fingerprint | DTLS->SRTP 桥 | `ctest --test-dir build/base -R test_dtls_key_export` | PASS |
| 37 | SRTP protect/unprotect 封装 | SRTP 会话 | `ctest --test-dir build/base -R test_srtp_protect_unprotect` | PASS |
| 38 | RTP 包编解码 + seq/timestamp 推进 | RTP 基础 | `ctest --test-dir build/base -R test_rtp_packet_codec` | PASS |
| 39 | RTCP 最小集（RR/SR/NACK/PLI/FIR） | RTCP 反馈 | `ctest --test-dir build/base -R test_rtcp_feedback_packets` | PASS |
| 40 | 重排缓冲固定上限 | 抖动缓冲有界 | `ctest --test-dir build/base -R test_rtp_reorder_buffer_capacity` | PASS |
| 41 | 重传窗口固定上限 | 重传有界 | `ctest --test-dir build/base -R test_rtp_retransmit_window` | PASS |
| 42 | 音频轨（编码后 payload）收发封装 | 音频传输边界 | `ctest --test-dir build/base -R test_audio_track_encoded_io` | PASS |
| 43 | 视频轨（编码后 payload）收发封装 | 视频传输边界 | `ctest --test-dir build/base -R test_video_track_encoded_io` | PASS |
| 44 | 仅关音频的真裁剪验证 | 音频可独立关 | `cmake -S . -B build/no-audio -DWEBRTC_ENABLE_AUDIO=OFF && cmake --build build/no-audio -j && nm -g --defined-only build/no-audio/libwebrtc_core.a | rg -i audio_track` | 无音频符号 |
| 45 | 仅关视频的真裁剪验证 | 视频可独立关 | `cmake -S . -B build/no-video -DWEBRTC_ENABLE_VIDEO=OFF && cmake --build build/no-video -j && nm -g --defined-only build/no-video/libwebrtc_core.a | rg -i video_track` | 无视频符号 |

### Chunk 5：SCTP/DataChannel + PeerConnection 装配
| ID | 步骤 | 产出 | 验证命令 | 通过标准 |
|---|---|---|---|---|
| 46 | 建 SCTP 传输适配层（屏蔽 usrsctp 细节） | 中立公共 API | `rg -n "usrsctp" include/webrtc -S` | 公共头无 usrsctp 类型 |
| 47 | 实现 DCEP open/ack 流程 | DC 建链流程 | `ctest --test-dir build/base -R test_dcep_open_ack` | PASS |
| 48 | 实现 DataChannel 有界收发队列 + 回调 | DC 数据路径 | `ctest --test-dir build/base -R test_datachannel_send_recv` | PASS |
| 49 | 大消息分片/重组上限控制 | 内存上界可控 | `ctest --test-dir build/base -R test_datachannel_fragment_limit` | PASS |
| 50 | `WEBRTC_ENABLE_DATA_CHANNEL=OFF` 真裁剪 | DC 可独立关 | `cmake -S . -B build/no-dc -DWEBRTC_ENABLE_DATA_CHANNEL=OFF && cmake --build build/no-dc -j && nm -g --defined-only build/no-dc/libwebrtc_core.a | rg -i datachannel` | 无 DC 符号 |
| 51 | 实现 PeerConnection 生命周期 API | 会话装配入口 | `ctest --test-dir build/base -R test_peerconnection_lifecycle` | PASS |
| 52 | 实现 setLocal/RemoteDescription | SDP 注入流程 | `ctest --test-dir build/base -R test_peerconnection_sdp_apply` | PASS |
| 53 | 实现 ICE candidate 注入/导出 | 信令中立接口 | `ctest --test-dir build/base -R test_peerconnection_ice_api` | PASS |
| 54 | 接通状态回调（ICE/DTLS/SRTP/SCTP） | 状态可观测 | `ctest --test-dir build/base -R test_peerconnection_state_callback` | PASS |
| 55 | 本地双端端到端（A/V+DC） | M2 闭环 | `ctest --test-dir build/base -R test_e2e_local_av_dc --output-on-failure` | PASS |

### Chunk 6：矩阵、资源、边界与收尾
| ID | 步骤 | 产出 | 验证命令 | 通过标准 |
|---|---|---|---|---|
| 56 | 新建 `scripts/build_matrix.sh`（8 组 gate） | 一键矩阵脚本 | `bash scripts/build_matrix.sh` | 8 组构建通过 |
| 57 | 组合1：仅音频 | 构建结果 | `cmake ...A=ON,V=OFF,DC=OFF && cmake --build ...` | PASS |
| 58 | 组合2：仅视频 | 构建结果 | `cmake ...A=OFF,V=ON,DC=OFF && cmake --build ...` | PASS |
| 59 | 组合3：仅 DC | 构建结果 | `cmake ...A=OFF,V=OFF,DC=ON && cmake --build ...` | PASS |
| 60 | 组合4/5/6：两两组合 | 构建结果 | 三组命令 | PASS |
| 61 | 组合7：全开；组合8：全关最小核 | 构建结果 | 两组命令 | PASS |
| 62 | 跑 unit/integration 分层测试 | 测试报告 | `ctest --test-dir build/base -L unit && ctest --test-dir build/base -L integration` | 全 PASS |
| 63 | 核心层平台污染扫描 | 分层守护 | `rg -n "pthread_|semaphore.h|epoll|poll.h|eventfd|timerfd|unistd.h|fcntl.h|FILE\\*|errno" src/core include/webrtc` | 无命中 |
| 64 | 统计 ROM（静态库大小与符号） | ROM 基线 | `size build/base/libwebrtc_core.a && nm -g --defined-only build/base/libwebrtc_core.a | wc -l` | 数据可记录 |
| 65 | 默认 RAM 预算测试（<3MiB） | RAM 基线 | `ctest --test-dir build/base -R test_memory_budget_default` | PASS |
| 66 | 明确未验证边界（仅 Linux 已验证） | 风险文档 | `rg -n "未验证|RTOS|风险" docs -S` | 记录完整 |

## 测试场景与验收
- 必测功能：
`test_ice_loopback_connectivity`、`test_dtls_handshake_localpair`、`test_srtp_protect_unprotect`、`test_audio_track_encoded_io`、`test_video_track_encoded_io`、`test_datachannel_send_recv`、`test_e2e_local_av_dc`
- 必测构建矩阵：
仅音频、仅视频、仅 DC、音+视、音+DC、视+DC、全开、全关最小核
- 必测分层约束：
核心层无 POSIX 依赖；端口相关仅在 `ports/`
- 必测裁剪真实性：
关闭模块后源码/符号/注册路径均消失（不是仅隐藏 API）

## 假设与默认值
- 参考实现对齐 `amazon-kinesis-video-streams-webrtc-sdk-c` 的协议路径，但按你的约束主动去除 Signaling 与编解码相关部分。
- TURN 不在本次里程碑内（`WEBRTC_ENABLE_TURN=OFF`）。
- 首个 TLS/DTLS 后端固定 MbedTLS，接口预留后端替换点。
- RTOS 当前目标是“可编译验证”，不承诺真机联调通过。
- 所有队列/缓冲必须固定上界并具备背压或丢弃策略，默认预算以 `<3MiB` 为硬边界。
