# 研究摘要

## 技术栈

推荐采用纯 C 核心库、自研最小协议编排、固定 arena、三执行器模型和可插拔 DTLS/SRTP/crypto backend。CMake 静态库作为交付形态；Mbed TLS、wolfSSL、libsrtp 可作为参考适配候选，但不应成为核心 API 硬依赖。

## 基础必备能力

首版必须覆盖 `PeerConnection` 生命周期、Chrome 最小 SDP/JSEP、Full ICE + STUN、DTLS-SRTP、RTP/RTCP、Opus/H264 编码帧级媒体 API、固定内存、无线程执行器、UDP datagram 边界、可观测性和本地 Chrome 验收示例。

## 需要警惕

最大风险不是单个协议点，而是边界被慢慢侵蚀：隐式动态内存、线程假设、socket 假设、第三方安全库类型泄漏、泛 SDP 野心和后补可观测性都会让首版失去可验收性。

## 构建顺序

1. 先建立公共 API、arena/limits、执行器、observer 和可观测性骨架。
2. 再实现 SDP/JSEP 最小模型，让 Chrome offer/answer 能 round-trip。
3. 接着实现 ICE/STUN 和 datagram demux。
4. 然后接入 DTLS/SRTP backend vtable 和参考适配。
5. 再做 RTP/RTCP、Opus、H264。
6. 最后用 `PeerConnection` 串起 Chrome 页面、信令示例和端到端验收。

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
