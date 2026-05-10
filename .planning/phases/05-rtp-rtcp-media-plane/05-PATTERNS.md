# 第 5 阶段：RTP/RTCP 媒体平面 - 模式映射

**Mapped:** 2026-05-10  
**Files analyzed:** 16  
**Analogs found:** 16 / 16

## 文件分类

| 新增/修改文件 | 角色 | 数据流 | 最近类比 | 匹配质量 |
|---|---|---|---|---|
| `include/rtc/media.h` | public model / API contract | request-response, event-driven | `include/rtc/security.h`, `include/rtc/observer.h` | role-match |
| `include/rtc/peer_connection.h` | public API entry | request-response | `include/rtc/peer_connection.h` | exact |
| `include/rtc/observer.h` | public callback contract | event-driven | `include/rtc/observer.h` | exact |
| `include/rtc/limits.h` | fixed-memory config | fixed-memory | `include/rtc/limits.h` | exact |
| `include/rtc/counters.h` | observability model | event-driven | `include/rtc/counters.h` | exact |
| `include/rtc/trace.h` | observability constants | event-driven | `include/rtc/trace.h` | exact |
| `src/api/peer_connection.h` | internal aggregate | fixed-memory, state | `src/api/peer_connection.h` | exact |
| `src/api/peer_connection.c` | API integration | request-response, route | `src/api/peer_connection.c` | exact |
| `src/media/media.h` / `src/media/media.c` | internal media service | queue, dispatch | `src/security/security.c`, `src/ice/ice.c` | role-match |
| `src/rtp/rtp.h` / `src/rtp/rtp.c` | packet codec | transform | `src/stun/stun.c`, `src/net/demux.c` | role-match |
| `src/rtcp/rtcp.h` / `src/rtcp/rtcp.c` | control packet codec | transform, stats | `src/stun/stun.c`, `src/ice/ice.c` | role-match |
| `src/srtp/srtp.c` | security wrapper | transform, gate | `src/srtp/srtp.c` | exact |
| `tests/test_media_api.c` | API/fixed memory test | request-response | `tests/test_peer_connection.c`, `tests/test_executor_affinity.c` | role-match |
| `tests/test_rtp.c` | packet/media integration test | transform | `tests/test_datagram.c`, `tests/test_security.c` | role-match |
| `tests/test_rtcp.c` | control packet test | transform/event | `tests/test_stun.c`, `tests/test_security.c` | role-match |
| `CMakeLists.txt` | build graph | build | `CMakeLists.txt` | exact |

## 模式分配

### Public API 与 C ABI

**类比:** `include/rtc/peer_connection.h`, `include/rtc/security.h`

现有 public API 都返回 `rtc_status_t`，头文件保持 C ABI：

```c
#ifdef __cplusplus
extern "C" {
#endif

rtc_status_t rtc_peer_connection_receive_datagram(rtc_peer_connection_t *pc,
                                                  const uint8_t *data,
                                                  size_t data_len);
```

**规划提示:** 新增媒体 API 时保持同一风格：

```c
rtc_status_t rtc_peer_connection_send_media_frame(
    rtc_peer_connection_t *pc,
    const rtc_media_frame_t *frame);

rtc_status_t rtc_peer_connection_request_keyframe(
    rtc_peer_connection_t *pc,
    rtc_media_kind_t kind);
```

`rtc_media_frame_t` 应放在 `include/rtc/media.h`，由 `peer_connection.h` 引入。

### Observer 与 callback 生命周期

**类比:** `include/rtc/observer.h`

现有 observer 回调只在调用期间有效，`on_datagram` 已承担 UDP 输出：

```c
void (*on_datagram)(void *user_data, const uint8_t *data, size_t data_len);
```

**规划提示:** 第 5 阶段新增 typed callback，例如：

```c
void (*on_media_frame_typed)(void *user_data,
                             const rtc_media_frame_t *frame);
void (*on_media_feedback)(void *user_data,
                          const rtc_media_feedback_t *feedback);
```

不要新增 `on_rtp_datagram` 或 `on_rtcp_datagram`，受保护 datagram 继续复用 `on_datagram`。

### Fixed Memory Limits

**类比:** `include/rtc/limits.h`, `src/api/peer_connection.c`

现有 limits 按子系统聚合，create-time 分配后保存到 `pc->limits`：

```c
typedef struct rtc_rtp_limits_t {
    size_t max_packet_cache;
} rtc_rtp_limits_t;
```

**规划提示:** 扩展 `rtc_rtp_limits_t` 时把容量写成明确字段，例如 `max_payload_bytes`、`max_packets_per_frame`、`max_reassembly_bytes`、`max_media_queue_slots`。执行器之间不要携带外部指针。

### Datagram Demux 接入点

**类比:** `src/api/peer_connection.c`, `src/net/demux.c`

`receive_datagram` 已把 RTP/RTCP 识别出来，但当前直接返回 OK：

```c
if (protocol == RTC_NET_PROTOCOL_DTLS) {
    return rtc_security_handle_dtls_datagram(pc, data, data_len);
}

if (protocol == RTC_NET_PROTOCOL_UNKNOWN) {
    ...
}

return RTC_STATUS_OK;
```

**规划提示:** RTP/RTCP 分支应调用内部 media/rtp/rtcp 入口：

```c
if (protocol == RTC_NET_PROTOCOL_RTP) {
    return rtc_media_handle_rtp_datagram(pc, data, data_len);
}
if (protocol == RTC_NET_PROTOCOL_RTCP) {
    return rtc_media_handle_rtcp_datagram(pc, data, data_len);
}
```

内部入口先复制到固定 scratch/queue，再调用 `rtc_srtp_unprotect_rtp/rtcp`，unprotect 成功后才能投递 media executor。

### SRTP/SRTCP Wrapper

**类比:** `src/srtp/srtp.c`

第 4 阶段已经封装了 ready 检查、backend 调用、失败 observer/trace/counter：

```c
rtc_status_t rtc_srtp_protect_rtp(rtc_peer_connection_t *pc,
                                  uint8_t *packet,
                                  size_t *inout_len,
                                  size_t capacity);
```

**规划提示:** 第 5 阶段不要直接调用 `pc->security_backend->vtable`。发送路径必须先完成 RTP/RTCP 明文构造，再调用 wrapper；protect 失败直接返回，不调用 `observer.on_datagram`。

### Test Pattern

**类比:** `tests/test_security.c`

测试使用 `rtc_executor_set_current_for_test` 切换亲和，deterministic backend 记录调用次数和 observer 输出：

```c
rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_security_on_ice_connected(pc));
```

**规划提示:** `tests/test_rtp.c` 应复用 deterministic backend 的 protect/unprotect 模式，或抽取可共享 test backend helper；不要依赖真实 OpenSSL/libsrtp。

## Plan Hints

- 先创建 public typed contract，再创建 packet codec；否则后续测试会反复改 public header。
- `src/api/peer_connection.c` 只做 `rtc_require_pc`、executor affinity、参数空值检查和路由，复杂 packetize/depacketize 放到 `src/rtp` / `src/rtcp`。
- `src/media` 负责跨 executor 固定槽、media state、SSRC/sequence/timestamp 和 callback 输出。
- `src/rtp` / `src/rtcp` 应尽量是纯函数式 transform，便于单测。
- 每个计划都必须保持默认构建无 GPL/LGPL 依赖、无线程、无 socket。
