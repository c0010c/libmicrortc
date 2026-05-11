# 里程碑记录

## v1.0 Chrome 1v1 MVP

**状态：** 已发货  
**完成日期：** 2026-05-11  
**阶段：** 6  
**计划：** 35/35  
**需求：** 48/48 v1 需求完成  
**归档：** [ROADMAP](milestones/v1.0-ROADMAP.md) · [REQUIREMENTS](milestones/v1.0-REQUIREMENTS.md) · [AUDIT](milestones/v1.0-MILESTONE-AUDIT.md)

### 交付摘要

v1.0 交付了一个面向性能受限设备的 WebRTC 纯 C `PeerConnection` MVP，在固定内存、无线程、用户负责 UDP/socket 收发、不处理编解码、不引入 GPL/LGPL 依赖的边界下，完成与 Chrome 的 1v1 音视频互通验收。

### 关键成果

1. 建立纯 C CMake 静态库、公共 `rtc_status_t`、不透明 `PeerConnection`、固定 arena/limits、三执行器 vtable、observer、trace、计数器和最小 create/destroy 示例。
2. 完成 Chrome 1v1 最小 SDP/JSEP profile，覆盖 offer/answer writer、Chrome SDP parser、JSEP 状态机和 trickle ICE candidate 保存。
3. 实现 Full ICE + STUN、host/srflx candidate、远端 trickle candidate、regular nomination、STUN transaction 和 datagram demux。
4. 通过可插拔 `security_backend` vtable 完成 DTLS fingerprint 校验、SRTP key export、SRTP/SRTCP protect/unprotect wrapper 和安全错误诊断矩阵。
5. 完成 typed media API、Opus/H264 RTP packetize/depacketize、H264 single NALU/FU-A/STAP-A、RTCP SR/RR/SDES、PLI 和 NACK parse-only feedback。
6. 交付本地 Chrome 合成媒体页面、JSON-only WebSocket 信令、`rtc_chrome_e2e` C 示例、secure full E2E 自动化、七层 failure layering、共享 `runId`、WebSocket 分片读取、Chrome ICE/STUN authenticated Binding 互通和人工媒体播放验收。

### 验收结果

- `REQUIREMENTS.md` v1 需求 48/48 已完成。
- 第 6 阶段 secure full E2E compact summary 为 `pass:true`、`layer:"none"`。
- `examples/chrome_e2e/out/full-secure/received-h264.264` 和 `received-opus.packets` 为非空媒体产物。
- 用户已于 2026-05-11 批准 VLC/ffplay 人工媒体播放 gate，`ACC-01` 关闭。

### 已知 Gaps

用户选择在关闭 v1.0 时接受以下审计 gap 作为技术债继续归档：

- 第 1 阶段缺少严格工作流要求的 `01-VERIFICATION.md`。
- 第 3 阶段缺少严格工作流要求的 `03-VERIFICATION.md`。
- 第 4 阶段缺少严格工作流要求的 `04-VERIFICATION.md`。
- 第 2 阶段 Nyquist frontmatter 仍显示 partial，需要后续文档修复或 `$gsd-validate-phase` 收口。
- 第 6 阶段 deferred items 记录 GSD 工具链 npm audit moderate vulnerability，建议作为独立工具链维护任务处理。

### 下一步

运行 `$gsd-new-milestone` 开始下一轮里程碑定义；该流程会创建 fresh `.planning/REQUIREMENTS.md` 并生成新的研究和路线图。
