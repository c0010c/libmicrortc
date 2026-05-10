# 风险与陷阱研究

## 常见陷阱

| 风险 | 早期信号 | 预防策略 | 应覆盖阶段 |
|------|----------|----------|------------|
| 固定内存边界失守 | 子系统偷偷使用 malloc、字符串无上限 | 第一阶段建立 allocator 包装、测试钩子和容量失败用例 | Phase 1 |
| 执行器亲和混乱 | 回调跨 executor 修改状态，偶发竞态 | API 文档和 debug 断言明确每个函数亲和 | Phase 1 |
| SDP 过度泛化 | 解析器复杂但 Chrome 仍不互通 | 先保存 Chrome 样本 SDP，按最小画像做 golden tests | Phase 2 |
| ICE 状态机不可观测 | 连接失败只能看到 timeout | 每个 candidate pair、STUN transaction、提名状态都有 trace | Phase 3 |
| DTLS/SRTP 后端泄漏 | 公共 API 暴露第三方类型 | vtable 只暴露抽象 buffer、状态和错误码 | Phase 4 |
| RTP 时间戳/marker 错误 | Chrome 有画面卡顿、无声或延迟异常 | 为 Opus/H264 建立包级 golden tests 和 Wireshark 对照 | Phase 5 |
| RTCP 被低估 | Chrome 不请求关键帧或统计异常 | SR/RR、SDES、PLI 早纳入集成测试 | Phase 5 |
| NACK “只解析”语义不清 | 用户误以为库会自动重传 | observer 事件、计数器和文档明确不重传 | Phase 5 |
| 示例太晚 | 单元测试通过但浏览器互通失败 | 早期准备 Chrome harness，逐阶段接入 | Phase 6 |
| 可观测性后补 | 错误码和 trace 字段无法稳定 | 从 Phase 1 定义事件命名和稳定性策略 | Phase 1 |

## 特别注意

### Chrome 互通不是“实现 RFC 即可”

JSEP、BUNDLE、rtcp-mux、ICE trickle、payload type、方向属性、setup 角色和 fingerprint 都需要与 Chrome 行为对齐。建议保存真实 Chrome SDP 和候选样本作为测试资料。

### 固定内存会影响所有设计

candidate 上限、STUN transaction 上限、RTP 包缓存、SDP 字符串容量、trace 缓冲策略都必须显式建模。错误路径需要能告诉用户哪个 limit 不足。

### 不创建线程并不等于没有并发模型

执行器亲和是核心契约。公开 API、observer 回调、timer 回调、datagram 输入输出都要声明在哪个 executor 上运行，以及跨 executor 如何投递。

### 可插拔安全后端需要最小且严谨

DTLS 和 SRTP 的 backend vtable 应避免把第三方库对象生命周期暴露给核心调用者。密钥导出、fingerprint、重放保护、alert/error 映射和握手 datagram 输出都要有清晰边界。

## 资料来源

- RFC 8829 JSEP: https://www.rfc-editor.org/rfc/rfc8829
- RFC 8445 ICE: https://www.rfc-editor.org/rfc/rfc8445
- RFC 5764 DTLS-SRTP: https://www.rfc-editor.org/rfc/rfc5764
- RFC 8834 WebRTC RTP: https://www.rfc-editor.org/rfc/rfc8834
- W3C WebRTC: https://www.w3.org/TR/webrtc/
