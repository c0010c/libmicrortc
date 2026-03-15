# s2_webrtc

## 项目目标
本项目是面向嵌入式与资源受限系统的轻量级 WebRTC C 核心库，强调纯 C、静态链接友好、可交叉编译、可裁剪。

## 当前范围
当前阶段聚焦 WebRTC 数据面与协议面基础能力：
- PeerConnection 以下的会话装配
- ICE / DTLS / SRTP
- RTP / RTCP
- 音频传输（可裁剪）
- 视频传输（可裁剪）
- DataChannel 传输（可裁剪）

## 非目标与边界
- 不含信令（不在核心库内实现 WebSocket / HTTP / 房间 / 鉴权流程）
- 不含编解码（核心库不实现音视频编码、解码、转码）
- 不绑定具体云厂商业务控制面
- TURN 默认 OFF（`WEBRTC_ENABLE_TURN=OFF`）

## 编译开关默认值
- `WEBRTC_ENABLE_AUDIO=ON`
- `WEBRTC_ENABLE_VIDEO=ON`
- `WEBRTC_ENABLE_DATA_CHANNEL=ON`
- `WEBRTC_ENABLE_TURN=OFF`
- `WEBRTC_USE_MBEDTLS=ON`
- `WEBRTC_PORT_POSIX=ON`
- `WEBRTC_PORT_RTOS=OFF`

## 生命周期与所有权
- `webrtc_init(config, pal, out_instance)` / `webrtc_deinit(instance)` 成对使用。
- `webrtc_peer_connection_create(instance, out_pc)` / `webrtc_peer_connection_free(pc)` 成对使用。
- `webrtc_pump_step(instance, now_ms, budget)` 由外部事件循环驱动；`budget=0` 时为 no-op 并返回 `WEBRTC_STATUS_OK`。
- `webrtc_init` 要求调用者显式传入 `webrtc_config_t` 与 `webrtc_pal_vtable_t`，库会复制配置和 PAL 回调，不持有调用者传入对象的所有权。
- 实例内部事件队列是固定容量（`event_queue_capacity`）；队列满时返回 `WEBRTC_STATUS_QUEUE_FULL` 进行背压。
- 资源释放原则保持“谁创建，谁释放”。

## 最小构建入口
使用最小构建脚本执行“配置 -> 编译 -> 测试”闭环：

```bash
bash scripts/build_min.sh
```

## 里程碑
当前基线里程碑记录见：[docs/milestones.md](docs/milestones.md)。
