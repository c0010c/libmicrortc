# 技术栈研究

## 结论

首版应采用“自研协议编排 + 可插拔安全后端 + 可选参考适配”的栈形态。核心库保持纯 C、固定内存、无线程、不直接操作 socket；第三方依赖通过 backend vtable 或示例适配隔离，不进入核心对象所有权模型。

## 推荐组成

| 层级 | 建议 | 理由 | 信心 |
|------|------|------|------|
| 构建系统 | CMake 静态库 | 与交付边界一致，便于嵌入式和桌面示例共用 | 高 |
| 公共 API | 纯 C 不透明句柄 + `rtc_status_t` | 与 C ABI、错误分类和生命周期控制兼容 | 高 |
| 内存 | 用户 arena + limits + 内部分区 allocator | 满足运行期不动态增长要求 | 高 |
| 调度 | `signaling` / `media` / `network` 三执行器 vtable | 保留无线程核心，同时支持多线程或 superloop 映射 | 高 |
| SDP/JSEP | 自研 Chrome 最小画像解析/生成 | 只覆盖首版互通所需字段，避免引入庞大 SDP 依赖 | 中高 |
| ICE/STUN | 自研 Full ICE 最小实现 | 固定内存和无线程边界强，通用 ICE 库往往较难适配 | 中高 |
| DTLS | backend vtable，首个参考适配优先评估 Mbed TLS 或 wolfSSL | 两者均面向嵌入式；wolfSSL 近期版本仍活跃，Mbed TLS 4.x 也在维护 | 中 |
| SRTP | backend vtable，参考适配可评估 Cisco libsrtp 2.x | SRTP 细节多，参考适配能降低互通风险；核心仍不绑定 | 中 |
| RTP/RTCP | 自研最小实现 | RTP/RTCP 需要与固定包缓存、观测事件和媒体 API 紧密结合 | 高 |
| 示例 | 本地 Chrome 页面 + 简单信令服务 + UDP 桥接示例 | 直接支撑首版验收 | 高 |

## 安全后端选择

### Mbed TLS

Mbed TLS 官方仓库显示 4.1.0 为 2026-03-31 的最新版本，且官方发布包包含子模块内容。它适合嵌入式和可裁剪配置，但 4.x 迁移成本需要单独评估。

### wolfSSL

wolfSSL 近期发布活跃，官网和发布页显示 2026-04 的 5.9.x 版本线。它强调可移植和嵌入式场景，适合作为 DTLS/crypto 参考候选，但需要关注版本安全公告和配置面。

### libsrtp

Cisco libsrtp 是 SRTP 参考候选，但最新稳定线更新节奏较慢。建议把它作为“参考适配”而非核心硬依赖，并在 vtable 中明确密钥导入、保护、解保护、重放窗口和错误映射职责。

## 不建议

- 不建议把 Google WebRTC Native 作为核心依赖：体量、线程、内存和构建模型都与本项目边界冲突。
- 不建议引入通用事件循环依赖：会模糊“库不创建线程、不拥有平台事件循环”的边界。
- 不建议首版抽象成任意 SDP 互通库：Chrome 1v1 最小画像更可验收。
- 不建议在核心库内直接绑定 OpenSSL、Mbed TLS、wolfSSL 或 libsrtp：许可、配置、内存和平台策略应留给集成者。

## 资料来源

- RFC 8829 JSEP: https://www.rfc-editor.org/rfc/rfc8829
- RFC 8445 ICE: https://www.rfc-editor.org/rfc/rfc8445
- RFC 8834 WebRTC RTP: https://www.rfc-editor.org/rfc/rfc8834
- RFC 5764 DTLS-SRTP: https://www.rfc-editor.org/rfc/rfc5764
- RFC 6184 H.264 RTP payload: https://www.rfc-editor.org/rfc/rfc6184
- RFC 7587 Opus RTP payload: https://www.rfc-editor.org/rfc/rfc7587
- W3C WebRTC: https://www.w3.org/TR/webrtc/
- Mbed TLS releases: https://github.com/Mbed-TLS/mbedtls/releases
- wolfSSL releases: https://github.com/wolfSSL/wolfssl/releases
- Cisco libsrtp releases: https://github.com/cisco/libsrtp/releases
