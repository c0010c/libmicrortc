### Step 3 设计：最小 SDP 解析与 Answer 生成（Answer-only）

**Summary**
- 目标：在不引入新线程、无堆增长、保持现有状态机风格下，完成 Chrome Offer 最小解析与可用 Answer 生成。
- 已确认决策：`部分接受不支持codec`、`Answer内嵌candidate并继续回调`、`远端candidate可为空`、`仅接受type=offer`、`start可先于offer`、`不支持m-line用端口0拒绝`、`本地candidate IP走平台自动探测，探测失败直接报错`。
- 参考对齐：对齐 `libpeer` 的逐行属性解析、`amazon-kinesis` 的固定容量/显式错误、`libdatachannel` 的 session+media 属性优先级与 `rtcp-mux` 生成、`metaRTC` 的 C 风格 line-dispatch 结构。

**Approach Choice（已定）**
- 采用 `A: 在 ICE 层内聚实现`（最小改动，贴合现有 `rtc_ice_set_remote_description` 路径）。
- 不采用独立 `sdp_min` 模块（改动偏大），不下沉到 session（会污染编排层职责）。

**Implementation Changes**
- 在 [rtc_ice.c](/home/qshl/dev/rtc/s1/src/ice/rtc_ice.c) / [rtc_ice.h](/home/qshl/dev/rtc/s1/src/ice/rtc_ice.h) 增加最小 SDP 解析与 Answer 生成能力：
  - `rtc_ice_set_remote_description()` 仅接受 `type=offer`；否则返回 `RTC_ERR_NOT_SUPPORTED`。
  - 行解析支持 `\r\n` 和 `\n`，固定缓冲、固定上限，不使用动态分配。
  - 解析并校验字段：`ice-ufrag`、`ice-pwd`、`fingerprint`、`setup`、`m=audio/video`、`rtpmap/fmtp`、`candidate`、`mid`、`group:BUNDLE`、`rtcp-mux`。
  - 编解码选择规则：
    - 视频仅接受 H264（优先 `packetization-mode=1` 的 PT；否则首个 H264 PT）。
    - 音频接受 PCMA/PCMU（优先 PCMA，再 PCMU）。
    - 不支持的 m-line：Answer 保留该 m-line 但 `port=0`。
    - 若最终 audio/video 都不可用：返回 `RTC_ERR_NOT_SUPPORTED`。
  - candidate 处理：
    - SDP 内 candidate 逐条入 `remote_candidates`（上限仍受 `RTC_CFG_MAX_REMOTE_CANDIDATES` 约束，超限报错）。
    - SDP 无 candidate 不报错（记录日志），允许后续 `rtc_peer_add_remote_candidate()` 补充。
  - Answer 生成规则：
    - `type=answer`。
    - 含 `a=ice-lite`、`a=setup:passive`、`a=rtcp-mux`、`a=group:BUNDLE`。
    - 含本地 `a=candidate`（与回调一致）。
    - `setup` 冲突（远端 `passive`）返回 `RTC_ERR_NOT_SUPPORTED`。
- 在 [rtc_platform.h](/home/qshl/dev/rtc/s1/src/platform/rtc_platform.h) / [rtc_platform.c](/home/qshl/dev/rtc/s1/src/platform/linux/rtc_platform.c) 增加本地 IPv4 探测接口：
  - 新增 `rtc_platform_get_default_ipv4(uint8_t out_ip[4])`。
  - Linux 实现使用固定容量 `ioctl(SIOCGIFCONF)` 路径，选第一个 `UP && !LOOPBACK` 的 IPv4。
  - 探测失败：返回 `RTC_ERR_NOT_SUPPORTED`，拒绝生成 Answer。
- 在 [rtc_session.c](/home/qshl/dev/rtc/s1/src/session/rtc_session.c) 做最小编排调整：
  - peer 创建后将本地 `ip+port` 注入 ICE（新增内部 setter）。
  - `start` 先于 `offer` 时不失败：状态机等待，待 offer 合法落地后再发 `on_local_description/on_local_candidate`。
  - `rtc_peer_set_remote_description()` 成功后，尝试用 SDP 内 host candidate 更新 transport remote（与 `add_remote_candidate()` 行为一致）。
- 可观测性（沿用现有日志风格）：
  - INFO：`remote offer parsed`、`answer generated`（带 accepted/rejected m-line 概要）。
  - WARN：`unsupported codec m-line rejected`、`offer has no candidate`。
  - ERROR：`missing required attr`、`invalid setup/fingerprint`、`local ipv4 discovery failed`。
  - 错误路径不吞错，返回值全检查。

**Public Interfaces / Types**
- `include/rtc/rtc.h` 不改（无新增公开 API）。
- 内部接口变更：
  - ICE 上下文新增固定字段（远端协商结果、已选 PT、answer_ready 标志、本地 host 信息）。
  - 新增内部函数 `rtc_ice_set_local_host(...)`（session 调用）。
  - 新增平台函数 `rtc_platform_get_default_ipv4(...)`（内部使用）。

**Test Plan**
- 更新现有 `test_rtc_api.c`/`test_rtc_interop_smoke.c` 的占位 SDP，改为真实 Chrome Offer fixture（去敏）：
  - `set_remote_description(offer)` 成功，`on_local_description` 输出 `answer`。
  - Answer 必含：`a=ice-lite`、`a=setup:passive`、`a=rtcp-mux`、`a=group:BUNDLE`、`a=candidate`。
- 新增/扩展用例（同测试二进制内）：
  - 仅 Opus（无 PCMA/PCMU）时：音频 m-line 端口置 0，视频可协商则整体成功。
  - audio/video 都不支持：返回 `RTC_ERR_NOT_SUPPORTED`，日志含上下文。
  - 缺失 `ice-pwd` / `fingerprint` / 非法 `setup`：返回 `RTC_ERR_PROTOCOL` 或 `RTC_ERR_NOT_SUPPORTED`（按错误类型）。
  - `start` 先于 `offer`：先不发本地描述，收到 offer 后再发 answer/candidate。
  - SDP 无 candidate：`set_remote_description` 成功，后续 `add_remote_candidate` 后进入连接路径。
- 回归：`ctest --output-on-failure` 全绿。

**Assumptions / Defaults**
- 只做 Answer-only；`type!=offer` 一律拒绝。
- 不支持 m-line 的处理固定为 `port=0`，不删除 m-line。
- 音频优先级固定 `PCMA > PCMU`。
- 本地 candidate 必须是可达 IPv4；探测失败即失败，不降级到 `127.0.0.1/0.0.0.0`。
- 内存开销仅为每个 peer 的固定字段增加（有界）；不引入新线程与无界容器。

