# Stack Research

**Domain:** 纯 C、跨平台、资源受限设备上的 WebRTC 媒体传输库  
**Project:** libmicrortc  
**Researched:** 2026-05-12  
**Confidence:** HIGH（协议基线和默认安全依赖来自 RFC/官方文档；Chrome 具体协商偏好仍需互通验证）

## 推荐结论

v1 应采用“自研 Sans-I/O 协议核心 + 可替换安全 vtable + Linux 适配外壳”的栈。不要把 libwebrtc、libnice、GStreamer、FFmpeg 或 usrsctp 拉进核心；它们会破坏固定内存、纯 C ABI、Sans-I/O 和“只做媒体传输”的边界。

默认生产适配选 `mbedTLS 4.1.0 LTS` 做 DTLS/DTLS-SRTP 协商，选 `libsrtp 2.8.0` 做 SRTP/SRTCP 包保护。核心协议、ICE/STUN/TURN、SDP、RTP/RTCP、H264/Opus packetizer 都应由本项目实现，因为这些部分直接决定状态机、内存预算、可观测性和 Sans-I/O 可测试性。

v1 协议目标不是完整浏览器 API，而是 Chrome 1v1 H264/Opus 媒体互通。路线图应优先落地 RFC 8834/8835 定义的 WebRTC 媒体传输必需子集：ICE full、STUN、TURN UDP relay、DTLS-SRTP、SRTP/SRTCP、RTP/RTCP mux、BUNDLE、H264 packetization-mode=1、Opus 48 kHz RTP clock、基础 RTCP SR/RR/SDES/BYE/PLI/FIR/NACK。TURN over TCP/TLS、ICE-TCP、DataChannel/SCTP、拥塞控制闭环不进 v1，但要在文档中标为与完整 WebRTC transport conformance 的已知差距。

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended | Confidence |
|------------|---------|---------|-----------------|------------|
| C | C99 ABI baseline | 公共 API、协议核心、平台适配接口 | mbedTLS 官方构建要求 C99；C99 对嵌入式和老工具链最稳。项目可在内部禁止 VLA、隐藏全局状态、使用显式 `stdint.h` 类型。 | HIGH |
| Sans-I/O core | 项目内实现 | ICE/STUN/TURN、SDP、RTP/RTCP、packetizer 状态机 | 固定内存、确定性测试、可替换 I/O/时钟/随机数/日志都依赖核心不直接碰 socket/thread/time。 | HIGH |
| CMake + CTest | `>= 3.20.2`，CI 可用当前 4.x | 构建、安装导出、测试编排、交叉编译入口 | mbedTLS 官方 README 要求 CMake 3.20.2+；CMake Presets 的 build/test presets 从 3.20 可用。不要要求最新 4.x，避免抬高集成门槛。 | HIGH |
| Ninja | distro current | 默认生成器 | Linux CI 和开发机速度快、输出稳定，适合多 preset 构建。 | HIGH |
| POSIX/Linux adapter | Linux v1 | UDP socket、epoll/timerfd 或 poll、随机数、日志、时间 | v1 优先 Linux；适配层可以先工程化互通测试，同时不污染核心。 | HIGH |

### Default Security Dependencies

| Library | Version | Purpose | When to Use | Confidence |
|---------|---------|---------|-------------|------------|
| mbedTLS | `4.1.0` LTS | 默认 DTLS 1.2/DTLS-SRTP vtable 实现、证书指纹、SRTP key exporter | v1 默认安全后端。启用 DTLS datagram、`use_srtp`、export keys、ECDSA P-256/SHA-256、CTR_DRBG/entropy。 | HIGH |
| TF-PSA-Crypto | `1.1.0` bundled with mbedTLS 4.1.0 | mbedTLS 4.x 的 PSA crypto 子树 | 使用 mbedTLS 官方 tarball，避免 GitHub 自动 `source.zip` 缺依赖/不可配置的问题。 | HIGH |
| libsrtp | `2.8.0` tag | 默认 SRTP/SRTCP vtable 实现 | v1 保护/解保护 RTP/RTCP；优先 `SRTP_AES128_CM_HMAC_SHA1_80` 互通，AES-GCM 后续用互通矩阵决定是否默认启用。 | MEDIUM-HIGH |

**mbedTLS 版本选择:** 推荐 4.1.0 LTS，而不是 3.6.x，原因是 4.1 是新 LTS，官方承诺维护到至少 2029-03。风险是 4.x API 相比 3.x 有破坏性变化，且生态中的旧示例多基于 2.x/3.x；因此 DTLS 适配层必须窄接口封装，并保留 3.6.6 LTS 编译 preset 作为回退验证项。

**libsrtp 版本选择:** 推荐 2.8.0 tag。官方 GitHub releases 页面可能显示较旧 release，但 `v2.8.0` tag 与 `CHANGES` 已存在；锁版本时应记录 tag SHA，并在首次集成阶段确认是否有正式 tarball/签名发布物。libsrtp README 明确 SRTP mandatory features 支持、AES-GCM 支持、CMake/Autotools/Meson 构建和缓冲区尾部 auth tag 写入要求；这会影响 libmicrortc 的 packet buffer 预留策略。

### Supporting Libraries

| Library | Version | Purpose | When to Use | Confidence |
|---------|---------|---------|-------------|------------|
| Unity Test | current GitHub | 纯 C 单元测试框架 | 核心协议、Sans-I/O 状态机、packetizer、固定 allocator 测试。单 C 文件 + headers，适合嵌入式风格。 | HIGH |
| libFuzzer | matching Clang | 结构化输入 fuzz | SDP、STUN/TURN、RTP/RTCP、H264 FU-A/STAP-A、Opus RTP payload 解析器。配 ASan/UBSan 跑。 | HIGH |
| coturn | current Docker/release | STUN/TURN 互通服务器 | 本地和公网 NAT lab；v1 重点测 UDP relay，后续再测 TURN TCP/TLS。 | HIGH |
| Wireshark/tshark | distro/current | 包级诊断、pcap 回归 | 调试 ICE/STUN/DTLS/SRTP/RTP/RTCP；结合 mbedTLS export callback 生成 key log。 | MEDIUM |
| Chrome Stable/Canary | current | 主互通对象 | 自动化 offer/answer、ICE、media send/recv、`getStats()` 和 `chrome://webrtc-internals` dump。 | HIGH |
| Pion WebRTC | v4.x, auxiliary only | 非浏览器互通对照 | 辅助定位 SDP/ICE/RTP 问题；不要把 Pion 通过当作 v1 验收替代 Chrome。 | MEDIUM |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| clang + ASan/UBSan | 内存越界、UAF、未定义行为检测 | CI 必须有 `asan-ubsan` preset；ASan runtime 不进生产产物。 |
| gcc | 交叉编译和 GNU 工具链覆盖 | CI 至少 gcc + clang 双编译，开启 `-Wall -Wextra -Wpedantic -Wconversion` 的可控子集。 |
| clang-format | C 风格一致性 | 早期固定风格，避免协议代码 review 被格式噪声淹没。 |
| clang-tidy/cppcheck | 静态分析 | 作为 warning gate 逐步收紧；不要一开始阻塞所有遗留第三方头文件。 |
| gcov/llvm-cov | 覆盖率 | 关注 parser、状态机和错误路径覆盖，不追求无意义百分比。 |
| Docker/netns/tc/iptables | 网络矩阵 | 模拟 NAT、丢包、重排、MTU、TURN relay；公网 TURN/真实 NAT 仍需独立验证。 |

## 协议标准基线

### v1 必须实现/遵守

| Area | RFC / Spec | v1 Baseline | Confidence |
|------|------------|-------------|------------|
| WebRTC media transport | RFC 8834, RFC 8835 | RTP/RTCP/SRTP/DTLS-SRTP/ICE 组合；支持 fully bundled 单 5-tuple；媒体走 secure RTP。 | HIGH |
| Security architecture | RFC 8826, RFC 8827 | DTLS-SRTP 必须；SDP fingerprint 必须绑定信令和 DTLS 证书。 | HIGH |
| SDP/JSEP surface | RFC 8866, RFC 3264, RFC 8829 | 生成/解析 Chrome 能接受的 offer/answer；不实现 W3C JS API，但状态机要兼容 JSEP offer/answer 语义。 | HIGH |
| ICE SDP | RFC 8839, RFC 8838 | `candidate`、`ice-ufrag`、`ice-pwd`、trickle ICE candidate 输入输出；full ICE role/conflict handling。 | HIGH |
| ICE core | RFC 8445 | Full ICE，不做 ICE-Lite；candidate pair、checklist、nominated pair、triggered checks、keepalive。 | HIGH |
| STUN | RFC 8489 | Binding、transaction ID、magic cookie、MESSAGE-INTEGRITY、FINGERPRINT、XOR-MAPPED-ADDRESS、错误码。 | HIGH |
| TURN | RFC 8656 | v1 支持 UDP allocation/refresh/permission/channel data；默认不做 TURN TCP/TLS relay。 | HIGH |
| DTLS-SRTP | RFC 6347, RFC 5764, RFC 7983, RFC 8842 | DTLS 1.2、`use_srtp`、SRTP exporter、DTLS/RTP/RTCP/STUN demux、SDP `setup` role。 | HIGH |
| SRTP/SRTCP | RFC 3711, RFC 5124 | RTP/SAVPF + SRTP/SRTCP；先支持 AES_CM_128_HMAC_SHA1_80，保留 AES-GCM profile。 | HIGH |
| RTP/RTCP | RFC 3550, RFC 5761, RFC 4585, RFC 5104 | RTP sequence/timestamp/SSRC、RTCP SR/RR/SDES/BYE、rtcp-mux、PLI/FIR/NACK。 | HIGH |
| BUNDLE/MID | RFC 8843, RFC 8285 | 音视频共用 transport；MID SDP 与 RTP header extension 解析/发送。 | HIGH |
| H264 RTP payload | RFC 6184 | `H264/90000`，`packetization-mode=1`，Single NALU、STAP-A、FU-A，SPS/PPS 透传，不解码。 | HIGH |
| Opus RTP payload | RFC 7587 | `opus/48000/2`，RTP timestamp 固定 48 kHz；不编解码，只封装/解封装 frame。 | HIGH |
| W3C browser behavior | W3C WebRTC REC 2025-03-13, WebRTC Stats | 只作为 Chrome 端配置、状态、stats 字段和测试 harness 参考，不复刻 JS API。 | HIGH |

### v1 明确不实现但要预留

| Area | Why Deferred | Required Hook |
|------|--------------|---------------|
| 拥塞控制闭环 / GCC / TWCC / REMB | 项目已明确 v1 不做；实现错误比不实现更危险。 | RTP header extension parser 可忽略未知扩展；stats 暴露 RTT/loss/jitter/bytes；发送调度保留 pacing/bitrate 输入。 |
| TURN over TCP/TLS / ICE-TCP | 完整 RFC 8835 WebRTC endpoint 要求，但项目 v1 优先 UDP relay；这是受限网络互通风险。 | candidate 类型和 TURN transport 枚举不要写死 UDP-only。 |
| DataChannel/SCTP | Out of scope。 | DTLS vtable 不要假设只有 SRTP keying，但核心不引入 usrsctp。 |
| RTX/RED/FEC/Simulcast/SVC | v1 1v1 H264/Opus 基础互通即可。 | SDP parser 忽略/拒绝策略清晰，payload type registry 可扩展。 |
| Codec integration | 项目不编解码。 | 示例程序可接入外部 encoded frame source，但核心 API 只收发 encoded access unit/frame。 |

## Build and Dependency Pattern

### 推荐仓库结构

```text
include/libmicrortc/       # public C API
src/core/                  # Sans-I/O protocol core, no socket/thread/time
src/adapters/mbedtls/      # DTLS vtable default implementation
src/adapters/libsrtp/      # SRTP vtable default implementation
src/platform/linux/        # v1 callback-driven Linux shell
tests/unit/                # Unity + CTest
tests/fuzz/                # libFuzzer targets and seed corpus
tests/interop/             # Chrome/coturn/Pion harness
cmake/                     # package config, options, toolchains
```

### CMake options

```cmake
LIBMICRORTC_BUILD_TESTS=ON/OFF
LIBMICRORTC_BUILD_EXAMPLES=ON/OFF
LIBMICRORTC_BUILD_FUZZERS=ON/OFF
LIBMICRORTC_WITH_MBEDTLS=ON/OFF
LIBMICRORTC_WITH_LIBSRTP=ON/OFF
LIBMICRORTC_WITH_LINUX_ADAPTER=ON/OFF
LIBMICRORTC_SANITIZERS=address,undefined
LIBMICRORTC_MAX_PEERS=<N>
LIBMICRORTC_TRACE_PACKETS=ON/OFF
```

### Dependency policy

1. 核心库不能直接依赖 mbedTLS/libsrtp headers；只依赖项目内 vtable 类型。
2. 默认适配器用 `find_package()` 或用户传入路径链接外部库；不要默认 FetchContent 下载网络依赖。
3. CI 可以提供 pinned third-party build preset，用官方 tarball/tag 构建 mbedTLS/libsrtp/coturn 测试环境。
4. public ABI 不暴露第三方类型；错误码把第三方错误映射成 libmicrortc 错误域，并保留 debug detail hook。
5. 所有 packet buffer API 必须显式提供 capacity，因为 libsrtp 会在 RTP packet 尾部写 auth tag，容量不足会导致内存破坏。

## Installation

### Linux 开发机基础包

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential clang lld cmake ninja-build pkg-config git \
  python3 perl doxygen \
  tshark wireshark \
  docker.io
```

### 推荐构建命令

```bash
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLIBMICRORTC_BUILD_TESTS=ON \
  -DLIBMICRORTC_WITH_MBEDTLS=ON \
  -DLIBMICRORTC_WITH_LIBSRTP=ON

cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

### Sanitizer / fuzz preset

```bash
cmake -S . -B build/asan -G Ninja \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLIBMICRORTC_BUILD_TESTS=ON \
  -DLIBMICRORTC_BUILD_FUZZERS=ON \
  -DLIBMICRORTC_SANITIZERS=address,undefined

cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative | Why Not Default |
|-------------|-------------|-------------------------|-----------------|
| 自研 ICE/STUN/TURN Sans-I/O | libnice | 只做桌面 Linux app、可接受 GLib/event-loop/动态资源模型时 | 不符合固定内存和 Sans-I/O 核心目标。 |
| mbedTLS 4.1.0 LTS | OpenSSL 3.x | 服务器端、已有 OpenSSL 合规要求、需要成熟 tooling 时 | footprint/API surface 更大；嵌入式替换性较差。 |
| mbedTLS 4.1.0 LTS | BoringSSL | 只在 Google/Chromium 生态内部跟随源码时 | 无稳定 API/ABI 和正式 release 策略，不适合作为第三方 C 库默认依赖。 |
| libsrtp 2.8.0 | 自研 SRTP | 几乎不建议；除非安全审计预算充足 | SRTP/SRTCP/replay/ROC/crypto 错误代价高。 |
| Unity + CTest | cmocka | 需要复杂 mock object 和 xUnit/TAP 输出时 | Unity 更小、更贴近嵌入式；Sans-I/O 本身可通过 fake transport/time 避免大量 mock。 |
| Chrome interop harness | 只用 Pion/aiortc | 协议开发早期快速定位时 | v1 目标是 Chrome，非 Chrome 通过不能证明浏览器互通。 |
| CMake | Meson | 后续若项目团队统一 Meson 可评估 | CMake 对 C 库安装导出、交叉编译、CTest、第三方生态更通用。 |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| libwebrtc 作为核心依赖 | 巨大 C++ 栈、线程/任务/内存模型重、不可控 ABI，不符合纯 C 和固定内存目标。 | 自研核心 + Chrome 互通测试。 |
| GStreamer/FFmpeg/x264/libopus 进核心 | 编解码和媒体 pipeline 已明确 out of scope，会扩大依赖和测试矩阵。 | 示例层可选 encoded frame source/sink。 |
| usrsctp | DataChannel/SCTP 不在 v1。 | 不引入，DTLS vtable 保留未来扩展点。 |
| OpenSSL/BoringSSL 硬编码进 API | 会锁死嵌入式用户选择。 | mbedTLS 默认适配 + DTLS vtable。 |
| 运行时无界 malloc/realloc 容器 | 直接违反固定内存预算。 | 初始化 arena + 显式容量表 + error path 测试。 |
| 字符串拼接式 SDP parser | SDP/JSEP/ICE candidate 边界复杂，易出现兼容和安全问题。 | tokenizing parser + round-trip tests + fuzz corpus。 |

## Stack Patterns by Variant

**Linux v1 互通开发:**
- CMake + Ninja + clang/gcc 双编译。
- mbedTLS 4.1.0、libsrtp 2.8.0、coturn Docker、Chrome Stable/Canary。
- 因为第一目标是快速建立 Chrome 互通、pcap 可诊断和 sanitizer/fuzzer 基线。

**资源受限嵌入式集成:**
- 只链接 core + 必要 vtable adapter；关闭 examples、interop harness、packet trace。
- mbedTLS 使用裁剪 config；libsrtp 只启用实际协商的 SRTP profiles。
- 因为内存和 flash 预算需要在初始化时可计算。

**安全/合规客户要求非 mbedTLS:**
- 实现 DTLS vtable 的 OpenSSL/wolfSSL/BoringSSL 私有 adapter。
- public ABI 不变；同一套 DTLS-SRTP exporter/vector tests 必须通过。
- 因为安全后端替换是项目既定目标，但不应影响核心。

## Version Compatibility

| Package A | Compatible With | Notes | Confidence |
|-----------|-----------------|-------|------------|
| libmicrortc core | C99 compiler | public headers 保持 C99；内部不要依赖 C11 atomics/thread。 | HIGH |
| mbedTLS 4.1.0 | TF-PSA-Crypto 1.1.0 bundled | 官方 release 说明 4.1.0 只能与内含 TF-PSA-Crypto 1.1.0 构建；用官方 tarball。 | HIGH |
| mbedTLS 4.1.0 | CMake >= 3.20.2, Python/Perl for tests | 官方 README 给出最低工具要求；CI 若不跑 mbedTLS 自测可减少 Python/Perl 依赖。 | HIGH |
| mbedTLS 3.6.6 | fallback adapter preset | 3.6 LTS 支持到至少 2027-03；用于兼容旧平台或 4.x API 风险回退。 | MEDIUM |
| libsrtp 2.8.0 | CMake/Autotools/Meson | README 和 CHANGES 显示 CMake 支持、CMake target export、Meson 支持；v2.8.0 tag 需锁 SHA。 | MEDIUM-HIGH |
| libsrtp AES-GCM | third-party crypto backend | README 表示内部 crypto 不支持 AES-GCM，需第三方 backend；v1 可先不把 AES-GCM 设为必须。 | HIGH |
| CMakePresets test/build | CMake >= 3.20 | build/test presets 在 schema v2 加入；项目最低 CMake 可定 3.20。 | HIGH |
| libFuzzer | matching Clang | LLVM 官方要求 libFuzzer 与 Clang 版本匹配；fuzz target 尽量 deterministic。 | HIGH |

## 测试与互通矩阵

| Layer | Required Tests | Tools |
|-------|----------------|-------|
| Parser | SDP、ICE candidate、STUN/TURN attributes、RTP/RTCP、H264/Opus payload fuzz + corpus regression | Unity, CTest, libFuzzer, ASan/UBSan |
| State machine | ICE checklist、role conflict、timeout/retransmission、DTLS role、SRTP rollover/replay | Sans-I/O fake clock/network |
| Memory budget | 初始化预算、peer 数上限、packet pool、水位、OOM path | custom allocator/arena test hooks |
| Security adapter | DTLS-SRTP profile negotiation、exported key material、fingerprint validation、libsrtp protect/unprotect | mbedTLS self tests, libsrtp self tests, known vectors |
| Network | host/srflx/relay candidate、NAT、loss/reorder、MTU、ICE restart | coturn, Linux netns, tc, iptables/nftables |
| Chrome interop | H264 send/recv、Opus send/recv、rtcp-mux、BUNDLE、TURN UDP relay、stats consistency | Chrome Stable/Canary, Playwright/Puppeteer, getStats, webrtc-internals dump, tshark |
| Diagnostics | packet trace redaction、state events、error code mapping、pcap correlation | Wireshark/tshark, structured logs |

**验收优先级:** Chrome Stable 是 v1 gate；Chrome Canary 是提前发现协商变化；Pion 只做辅助 oracle；coturn 是 TURN lab 标配；公网真实 NAT 测试不能被本地 netns 完全替代。

## 未确认风险

| Risk | Impact | Mitigation | Confidence |
|------|--------|------------|------------|
| Chrome 当前 H264 profile-level-id、packetization-mode、rtcp-fb 偏好会随版本变化 | SDP 通过但媒体黑屏/无关键帧请求 | 互通阶段固定 Chrome Stable/Canary 版本，保存 SDP/pcap/getStats 样本。 | MEDIUM |
| mbedTLS 4.1.0 API 新，第三方示例少 | DTLS adapter 初期集成成本高 | adapter 窄接口；保留 mbedTLS 3.6.6 preset；写 RFC 5764 exporter vector/interop tests。 | MEDIUM |
| libsrtp GitHub release 页面与 tag 状态可能不一致 | 供应链锁版不清晰 | 锁 tag SHA，验证 tarball/签名/包管理器版本；必要时 vendor tag snapshot。 | MEDIUM |
| v1 不做 TURN TCP/TLS 与 ICE-TCP | UDP 受限网络下无法与 Chrome 建连 | 在 README/roadmap 明示；candidate model 预留 transport 类型；Phase 后续补。 | HIGH |
| 不做拥塞控制 | 弱网/高码率下丢包和延迟不可控 | v1 暴露 stats、RTCP feedback、应用层码率/关键帧回调；不要宣称完整 QoE。 | HIGH |
| H264 不编解码但要处理 SPS/PPS/IDR 边界 | 应用喂入格式不一致导致无法 packetize | API 明确 Annex-B/AVCC 输入格式，内部提供转换或只支持一种并测试。 | MEDIUM |

## Sources

- Context7 CLI `/mbed-tls/mbedtls` — DTLS datagram 配置、DTLS timer callback、compile-time config、export key/SRTP 相关 API（HIGH）。
- Context7 CLI `/kitware/cmake` — CTest、CMake package export、Presets 文档（HIGH）。
- Mbed TLS GitHub releases — 4.1.0 LTS 支持到至少 2029-03、4.1.0 官方 tarball 与 TF-PSA-Crypto 1.1.0 bundled、3.6.6 LTS 到 2027-03：https://github.com/Mbed-TLS/mbedtls/releases （HIGH）。
- Mbed TLS GitHub README — CMake 3.20.2+、C99 toolchain、submodule/tarball 构建要求：https://github.com/Mbed-TLS/mbedtls （HIGH）。
- Mbed TLS API docs — DTLS transport、use_srtp profiles、SRTP profile constants、export key callback：https://mbed-tls.readthedocs.io/projects/api/en/latest/api/file/ssl_8h/ （HIGH）。
- libsrtp README v2.8.0 — SRTP mandatory features、AES-GCM、backend、buffer capacity warning、tests：https://raw.githubusercontent.com/cisco/libsrtp/v2.8.0/README.md （HIGH）。
- libsrtp CHANGES v2.8.0 — 2.8.0/2.7.0/2.6.0 changes、CMake target export、MbedTLS backend history、fuzzer：https://raw.githubusercontent.com/cisco/libsrtp/v2.8.0/CHANGES （MEDIUM-HIGH）。
- RFC 8834 — WebRTC RTP/media transport requirements：https://www.rfc-editor.org/rfc/rfc8834.html （HIGH）。
- RFC 8835 — WebRTC transport, ICE full, TURN, DTLS-SRTP, demux, bundled 5-tuple requirements：https://www.rfc-editor.org/rfc/rfc8835.html （HIGH）。
- RFC 8445 / 8839 / 8838 — ICE full、ICE SDP、Trickle ICE：https://www.rfc-editor.org/rfc/rfc8445.html / https://www.rfc-editor.org/rfc/rfc8839.html / https://www.rfc-editor.org/rfc/rfc8838.html （HIGH）。
- RFC 8489 / 8656 — STUN/TURN：https://www.rfc-editor.org/rfc/rfc8489.html / https://www.rfc-editor.org/rfc/rfc8656.html （HIGH）。
- RFC 5764 / 3711 / 7983 / 8842 — DTLS-SRTP、SRTP/SRTCP、demux、SDP DTLS：https://www.rfc-editor.org/rfc/rfc5764.html / https://www.rfc-editor.org/rfc/rfc3711.html （HIGH）。
- RFC 3550 / 5761 / 4585 / 5104 / 8843 / 8285 — RTP/RTCP、rtcp-mux、feedback、BUNDLE/MID/header extensions（HIGH）。
- RFC 6184 / 7587 — H264/Opus RTP payload formats：https://www.rfc-editor.org/rfc/rfc6184.html / https://www.rfc-editor.org/rfc/rfc7587.html （HIGH）。
- W3C WebRTC Recommendation 2025-03-13 和 WebRTC Stats — Chrome 端 API/状态/stats 参考：https://www.w3.org/TR/webrtc/ / https://www.w3.org/TR/webrtc-stats/ （HIGH）。
- coturn GitHub README — STUN/TURN server、Docker、protocol support：https://github.com/coturn/coturn （HIGH）。
- LLVM libFuzzer docs、Clang ASan/UBSan docs — fuzz/sanitizer 策略：https://llvm.org/docs/LibFuzzer.html / https://clang.llvm.org/docs/AddressSanitizer.html / https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html （HIGH）。
- Unity Test GitHub README、cmocka docs — C test framework tradeoff：https://github.com/ThrowTheSwitch/Unity / https://cmocka.org/ （HIGH）。
- Pion WebRTC GitHub — 辅助互通 oracle，v4.x 当前状态：https://github.com/pion/webrtc （MEDIUM）。

---
*Stack research for: libmicrortc v1 WebRTC media transport stack*  
*Researched: 2026-05-12*
