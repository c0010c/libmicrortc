# 第 4 阶段：DTLS-SRTP 安全传输 - 模式映射

**Mapped:** 2026-05-10  
**Files analyzed:** 14  
**Analogs found:** 14 / 14  

## 文件分类

| 新增/修改文件 | 角色 | 数据流 | 最近类比 | 匹配质量 |
|---|---|---|---|---|
| `include/rtc/security.h` | config / backend contract | request-response, transform | `include/rtc/config.h`, `include/rtc/observer.h` | role-match |
| `include/rtc/config.h` | config | request-response | `include/rtc/config.h` | exact |
| `include/rtc/limits.h` | config | fixed-memory | `include/rtc/limits.h` | exact |
| `include/rtc/counters.h` | model / observability | event-driven | `include/rtc/counters.h`, `src/observability/counters.c` | exact |
| `include/rtc/trace.h` | config / observability | event-driven | `include/rtc/trace.h` | exact |
| `src/api/peer_connection.h` | internal model | request-response, fixed-memory | `src/api/peer_connection.h` | exact |
| `src/api/peer_connection.c` | controller / integration | request-response, event-driven | `src/api/peer_connection.c` | exact |
| `src/security/security.h` | internal service header | request-response, transform | `src/ice/ice.h`, `src/net/demux.h` | role-match |
| `src/security/security.c` | service / state machine | request-response, event-driven | `src/ice/ice.c`, `src/api/peer_connection.c` | role-match |
| `src/sdp/sdp_writer.c` | utility | transform | `src/sdp/sdp_writer.c` | exact |
| `src/sdp/sdp_parser.c` | utility | transform | `src/sdp/sdp_parser.c` | exact |
| `tests/test_security.c` | test | event-driven, request-response | `tests/test_datagram.c`, `tests/test_ice.c` | role-match |
| `tests/test_sdp_writer.c` | test | transform | `tests/test_sdp_writer.c` | exact |
| `CMakeLists.txt` | config | build graph | `CMakeLists.txt` | exact |

## 模式分配

### `include/rtc/security.h`（config / backend contract, request-response + transform）

**类比:** `include/rtc/config.h`, `include/rtc/observer.h`, `include/rtc/status.h`

**导入与 C ABI 模式**（`include/rtc/config.h` 行 4-13）:
```c
#include <stddef.h>
#include <stdint.h>

#include "rtc/executor.h"
#include "rtc/limits.h"
#include "rtc/observer.h"

#ifdef __cplusplus
extern "C" {
#endif
```

**vtable 形状模式**（`include/rtc/observer.h` 行 14-27）:
```c
typedef struct rtc_observer_vtable_t {
    void (*on_state)(void *user_data, const char *state);
    void (*on_error)(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code);
    void (*on_trace)(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count);
    void *user_data;
} rtc_observer_vtable_t;
```

**status 边界模式**（`include/rtc/status.h` 行 8-24）:
```c
typedef enum rtc_status_t {
    RTC_STATUS_OK = 0,
    RTC_STATUS_INVALID_ARGUMENT,
    RTC_STATUS_INVALID_STATE,
    RTC_STATUS_PROTOCOL_ERROR,
    RTC_STATUS_BACKEND_ERROR
} rtc_status_t;
```

**规划提示:** `rtc_security_backend_vtable_t` 应保持单一 vtable：DTLS session/start/input/output、local/peer fingerprint、key export、SRTP/SRTCP protect/unprotect。函数返回 `rtc_status_t`，细节不要扩展 public status；用 detail code、trace reason 和 counters 表达。

---

### `include/rtc/config.h`（config, request-response）

**类比:** `include/rtc/config.h`

**现有接入点**（行 56-69）:
```c
typedef struct rtc_peer_connection_config_t {
    rtc_arena_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_sdp_parameters_t sdp;
    rtc_observer_vtable_t observer;
    void *security_backend;
} rtc_peer_connection_config_t;
```

**固定资源枚举模式**（行 20-29）:
```c
typedef enum rtc_capacity_resource_t {
    RTC_CAPACITY_RESOURCE_ARENA = 0,
    RTC_CAPACITY_RESOURCE_SDP_BUFFER,
    RTC_CAPACITY_RESOURCE_ICE_CANDIDATES,
    RTC_CAPACITY_RESOURCE_STUN_TRANSACTIONS,
    RTC_CAPACITY_RESOURCE_PACKET_CACHE,
    RTC_CAPACITY_RESOURCE_TRACE_BUFFER
} rtc_capacity_resource_t;
```

**规划提示:** 将 `void *security_backend` 演进为指向稳定 security backend config/vtable 的入口，新增 backend storage 资源时同步扩展 `rtc_capacity_resource_t`，保持 create-time 诊断。

---

### `include/rtc/limits.h`（config, fixed-memory）

**类比:** `include/rtc/limits.h`

**现有 DTLS limit 起点**（行 21-23）:
```c
typedef struct rtc_dtls_limits_t {
    size_t max_sessions;
} rtc_dtls_limits_t;
```

**聚合模式**（行 37-44）:
```c
typedef struct rtc_peer_connection_limits_t {
    rtc_sdp_limits_t sdp;
    rtc_ice_limits_t ice;
    rtc_dtls_limits_t dtls;
    rtc_rtp_limits_t rtp;
    rtc_rtcp_limits_t rtcp;
    rtc_trace_limits_t trace;
} rtc_peer_connection_limits_t;
```

**规划提示:** 扩展 `dtls` 或新增 `security/srtp` limits 时保持子结构聚合风格。第 4 阶段首版只有 1 个 BUNDLE transport context，不要引入动态 m-line 数组。

---

### `include/rtc/counters.h` 与 `src/observability/counters.c`（observability model, event-driven）

**类比:** `include/rtc/counters.h`, `src/observability/counters.c`

**counter 分组模式**（`include/rtc/counters.h` 行 24-59）:
```c
typedef struct rtc_ice_counters_t {
    uint64_t local_candidates;
    uint64_t remote_candidates;
    uint64_t selected_pairs;
    uint64_t checks_failed;
} rtc_ice_counters_t;

typedef struct rtc_peer_connection_counters_t {
    rtc_ice_counters_t ice;
    rtc_stun_counters_t stun;
    rtc_net_counters_t net;
    rtc_trace_counters_t trace;
} rtc_peer_connection_counters_t;
```

**初始化模式**（`src/observability/counters.c` 行 3-29）:
```c
void rtc_counters_init(rtc_peer_connection_counters_t *counters)
{
    if (counters == 0) {
        return;
    }
    counters->ice.selected_pairs = 0;
    counters->net.demux_dtls = 0;
    counters->trace.trace_events = 0;
}
```

**规划提示:** 新增 `rtc_dtls_counters_t` / `rtc_srtp_counters_t`，覆盖 handshake started/completed/failed、fingerprint_mismatch、key_export_failed、protect_failed、unprotect_failed、replay_failed。必须在 `rtc_counters_init` 中显式清零。

---

### `include/rtc/trace.h`（observability config, event-driven）

**类比:** `include/rtc/trace.h`

**事件与字段常量模式**（行 11-48）:
```c
#define RTC_TRACE_ICE_STATE "ice.state"
#define RTC_TRACE_STUN_TRANSACTION "stun.transaction"
#define RTC_TRACE_NET_DEMUX "net.demux"

#define RTC_TRACE_FIELD_SUBSYSTEM "subsystem"
#define RTC_TRACE_FIELD_OPERATION "operation"
#define RTC_TRACE_FIELD_STATUS "status"
#define RTC_TRACE_FIELD_REASON "reason"
#define RTC_TRACE_FIELD_ROLE "role"
#define RTC_TRACE_FIELD_PROTOCOL "protocol"
```

**字段结构模式**（行 49-53）:
```c
typedef struct rtc_trace_field_t {
    const char *key;
    const char *value;
    uint64_t number;
} rtc_trace_field_t;
```

**规划提示:** 安全层 trace 应复用 `subsystem/operation/status/reason/role`。新增事件建议类似 `RTC_TRACE_DTLS_STATE`、`RTC_TRACE_DTLS_HANDSHAKE`、`RTC_TRACE_SRTP_STATE`、`RTC_TRACE_SRTP_PROTECT`。

---

### `src/api/peer_connection.h`（internal model, request-response + fixed-memory）

**类比:** `src/api/peer_connection.h`

**内部状态聚合模式**（行 61-97）:
```c
struct rtc_peer_connection_t {
    rtc_arena_view_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_sdp_parameters_t sdp;
    rtc_observer_vtable_t observer;
    rtc_peer_connection_counters_t counters;
    rtc_sdp_description_t local_summary;
    rtc_sdp_description_t remote_summary;
    rtc_ice_state_t ice_state;
    rtc_ice_role_t ice_role;
    int is_closed;
};
```

**规划提示:** 在此处保存 `security_backend` config/vtable、backend session storage 指针、安全状态、DTLS role、local/remote fingerprint 快照、keying material、SRTP ready 标志。不要把可变长度数据放到运行期 malloc。

---

### `src/api/peer_connection.c`（controller / integration, request-response + event-driven）

**类比:** `src/api/peer_connection.c`

**导入模式**（行 1-13）:
```c
#include "api/peer_connection.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "executor/executor.h"
#include "ice/ice.h"
#include "memory/allocator.h"
#include "net/demux.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"
```

**配置校验模式**（行 176-199）:
```c
static rtc_status_t rtc_validate_config(const rtc_peer_connection_config_t *config)
{
    if (config == 0 || config->arena.data == 0 || config->arena.size == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (config->limits.sdp.max_description_bytes == 0 ||
        config->limits.ice.max_candidates == 0 ||
        config->sdp.dtls_fingerprint == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    return rtc_validate_stun_config(config);
}
```

**create-time arena 切分模式**（行 385-475）:
```c
rtc_arena_init(&arena, config->arena);
pc = (rtc_peer_connection_t *)rtc_core_alloc(
    &arena, sizeof(*pc), sizeof(void *), RTC_CAPACITY_RESOURCE_ARENA, diag);
pc->arena = arena;
pc->limits = config->limits;
pc->observer = config->observer;
rtc_counters_init(&pc->counters);

status = rtc_pc_alloc_sdp_buffer(&pc->arena,
                                 config->limits.sdp.max_description_bytes,
                                 &pc->local_description, diag);
```

**DTLS demux 集成点**（行 942-999）:
```c
status = rtc_net_demux_datagram(data, data_len, &protocol);
rtc_pc_note_demux_counter(pc, protocol);
rtc_pc_trace_demux(pc, protocol, RTC_STATUS_OK, reason);

if (protocol == RTC_NET_PROTOCOL_STUN) {
    status = rtc_ice_handle_stun_response(pc, data, data_len);
    if (status != RTC_STATUS_OK) {
        rtc_pc_trace_demux(pc, protocol, status, reason);
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    return RTC_STATUS_OK;
}
```

**规划提示:** 第 4 阶段把 `RTC_NET_PROTOCOL_DTLS` 从“只 demux 后 OK”改为调用 `rtc_security_handle_dtls_datagram(pc, data, data_len)`。如果 ICE 未 connected/selected，返回 `RTC_STATUS_INVALID_STATE`，输出 observer error + trace reason `ice_not_connected`，不缓存 datagram。

---

### `src/security/security.h`（internal service header, request-response + transform）

**类比:** `src/net/demux.h`, `src/ice/ice.h`

**小型内部 header 模式**（`src/net/demux.h` 行 1-19）:
```c
#ifndef RTC_NET_DEMUX_H
#define RTC_NET_DEMUX_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"

rtc_status_t rtc_net_demux_datagram(const uint8_t *data, size_t data_len,
                                    rtc_net_protocol_t *out_protocol);

#endif
```

**规划提示:** `src/security/security.h` 只暴露内部编排入口，例如 `rtc_security_init`、`rtc_security_on_ice_connected`、`rtc_security_handle_dtls_datagram`、`rtc_security_protect_rtp/rtcp`、`rtc_security_unprotect_rtp/rtcp`。不要暴露为 public user API。

---

### `src/security/security.c`（service / state machine, request-response + event-driven）

**类比:** `src/ice/ice.c`, `src/api/peer_connection.c`

**状态名与状态事件模式**（`src/ice/ice.c` 行 36-53、65-89）:
```c
static const char *rtc_ice_state_name(rtc_ice_state_t state)
{
    switch (state) {
    case RTC_ICE_CONNECTED:
        return "ice.connected";
    case RTC_ICE_FAILED:
        return "ice.failed";
    default:
        return "ice.new";
    }
}

pc->ice_state = state;
rtc_observer_emit_state(&pc->observer, rtc_ice_state_name(state));
rtc_counters_note_trace(&pc->counters);
rtc_trace_emit(&pc->observer, RTC_TRACE_ICE_STATE, fields, field_count);
```

**失败映射模式**（`src/ice/ice.c` 行 96-103）:
```c
static void rtc_ice_fail(rtc_peer_connection_t *pc, const char *operation,
                         const char *reason)
{
    pc->counters.ice.checks_failed++;
    rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR, "ice",
                            operation, 0);
    rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED, reason);
}
```

**datagram 输出模式**（`src/ice/ice.c` 行 518-524）:
```c
if (pc->observer.on_datagram != 0) {
    pc->observer.on_datagram(pc->observer.user_data, request, request_len);
}
pc->counters.stun.transactions_sent++;
rtc_ice_trace_stun(pc, use_candidate ? "nomination_request" : "pair_check",
                   transaction_index, RTC_STATUS_OK);
```

**ICE connected 触发点**（`src/ice/ice.c` 行 544-551）:
```c
static void rtc_ice_select_pair(rtc_peer_connection_t *pc, size_t pair_id)
{
    pc->candidate_pairs[pair_id].selected = 1;
    pc->candidate_pairs[pair_id].state = RTC_ICE_PAIR_SELECTED;
    pc->counters.ice.selected_pairs++;
    rtc_ice_trace_selected_pair(pc, pair_id);
    rtc_ice_emit_state(pc, RTC_ICE_CONNECTED);
}
```

**规划提示:** security state machine 应复制 ICE 的“状态字符串 + observer state + trace reason + counter”模式。`rtc_ice_select_pair` 后调用 `rtc_security_on_ice_connected(pc)` 自动启动 DTLS；不要增加用户手动 `start_dtls` 常规路径。

---

### `src/sdp/sdp_writer.c`（utility, transform）

**类比:** `src/sdp/sdp_writer.c`

**输入校验与容量模式**（行 49-64、119-124）:
```c
if (params == 0 || inout_sdp_len == 0 ||
    !rtc_sdp_param_valid(params->ice_ufrag, params->ice_ufrag_len) ||
    !rtc_sdp_param_valid(params->dtls_fingerprint,
                         params->dtls_fingerprint_len)) {
    return RTC_STATUS_INVALID_ARGUMENT;
}

*inout_sdp_len = writer.required;
if (out_sdp == 0 || capacity <= writer.required) {
    return RTC_STATUS_CAPACITY_SDP_BUFFER;
}
```

**fingerprint/setup 写入点**（行 87-90、105-108）:
```c
rtc_sdp_append(&writer, "a=fingerprint:%.*s\r\n",
               (int)params->dtls_fingerprint_len,
               params->dtls_fingerprint);
rtc_sdp_append(&writer, "a=setup:%s\r\n", setup);
```

**规划提示:** writer 本身可以继续消费 `rtc_sdp_parameters_t`，但 `createOffer/createAnswer` 前必须让 API 层从 backend 查询本地 `sha-256` fingerprint 快照，再传给 writer；不要继续信任 create-time 临时 fingerprint 字符串。

---

### `src/sdp/sdp_parser.c`（utility, transform）

**类比:** `src/sdp/sdp_parser.c`

**必需字段校验模式**（行 87-104）:
```c
if (!description->has_bundle ||
    description->ice_ufrag[0] == '\0' || description->ice_pwd[0] == '\0' ||
    description->dtls_fingerprint[0] == '\0' ||
    description->dtls_setup[0] == '\0') {
    return RTC_STATUS_PROTOCOL_ERROR;
}
```

**远端 fingerprint/setup 保存模式**（行 187-197）:
```c
} else if (rtc_sdp_line_starts(line, line_len, "a=fingerprint:")) {
    rtc_sdp_copy(out_description->dtls_fingerprint,
                 sizeof(out_description->dtls_fingerprint), line + 14,
                 line_len - 14u);
} else if (rtc_sdp_line_starts(line, line_len, "a=setup:")) {
    rtc_sdp_copy(out_description->dtls_setup,
                 sizeof(out_description->dtls_setup), line + 8,
                 line_len - 8u);
    if (rtc_sdp_line_eq(line, line_len, "a=setup:actpass")) {
        out_description->type = RTC_SDP_TYPE_OFFER;
    }
}
```

**规划提示:** parser 已经保存远端 fingerprint/setup。第 4 阶段应增加 `sha-256` 算法限制和 role helper 的 table-driven 测试，而不是把 role 推导散落在 SDP parser 中。

---

### `tests/test_security.c`（test, event-driven + request-response）

**类比:** `tests/test_datagram.c`, `tests/test_ice.c`, `tests/test_runner.h`

**测试 fixture 状态模式**（`tests/test_datagram.c` 行 10-24）:
```c
typedef struct datagram_observer_state_t {
    int datagram_count;
    int error_count;
    int state_count;
    int trace_count;
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    const char *last_state;
    const char *last_trace;
    const char *last_protocol;
    const char *last_reason;
} datagram_observer_state_t;
```

**observer 捕获模式**（`tests/test_datagram.c` 行 87-128）:
```c
static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    memcpy(state->last_datagram, data, data_len);
    state->last_datagram_len = data_len;
    state->datagram_count++;
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    state->trace_count++;
    state->last_trace = event;
}
```

**config fixture 模式**（`tests/test_datagram.c` 行 130-170）:
```c
memset(&config, 0, sizeof(config));
config.arena.data = arena;
config.arena.size = arena_size;
config.limits.sdp.max_description_bytes = 2048;
config.limits.dtls.max_sessions = 1;
config.executors.signaling = test_executor(state);
config.executors.network = test_executor(state);
config.observer.on_state = on_state;
config.observer.on_error = on_error;
config.observer.on_trace = on_trace;
config.observer.on_datagram = on_datagram;
```

**ICE connected 驱动模式**（`tests/test_ice.c` 行 430-459）:
```c
rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
RTC_TEST_EQ_INT(RTC_STATUS_OK,
                rtc_peer_connection_create(&config, &diag, &pc));
pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
RTC_TEST_EQ_INT(RTC_STATUS_OK,
                rtc_peer_connection_start_connectivity_checks(pc));
RTC_TEST_ASSERT(strcmp(state.last_state, "ice.connected") == 0);
```

**断言宏模式**（`tests/test_runner.h` 行 11-39）:
```c
#define RTC_TEST_ASSERT(expr) do { if (!(expr)) { return 1; } } while (0)
#define RTC_TEST_EQ_INT(expected, actual) do { \
    if ((int)(expected) != (int)(actual)) { return 1; } \
} while (0)
#define RTC_RUN_TEST(fn, result) do { \
    int rtc_test_status__ = fn(); \
    if (rtc_test_status__ == 0) { (result).passed++; } else { (result).failed++; } \
} while (0)
```

**规划提示:** `tests/test_security.c` 应内置 deterministic backend 结构体和 vtable，覆盖：成功 handshake + key export + `srtp.ready`、fingerprint mismatch、handshake backend error、key export failure、SRTP protect failure、SRTP/SRTCP unprotect failure、ICE 未 connected 早到 DTLS。

---

### `tests/test_sdp_writer.c`（test, transform）

**类比:** `tests/test_sdp_writer.c`

**参数 fixture 模式**（行 8-23）:
```c
static rtc_sdp_parameters_t test_sdp_params(const char *setup)
{
    rtc_sdp_parameters_t params;
    params.ice_ufrag = "testufrag";
    params.ice_pwd = "testpassword1234567890";
    params.dtls_fingerprint =
        "sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    params.dtls_setup = setup;
    return params;
}
```

**golden 与容量测试模式**（行 72-115）:
```c
RTC_TEST_EQ_INT(RTC_STATUS_OK,
                rtc_sdp_write_offer(&params, RTC_SDP_DIRECTION_SENDRECV,
                                    RTC_SDP_DIRECTION_SENDRECV, sdp,
                                    &sdp_len));
RTC_TEST_ASSERT(contains_text(sdp, "a=ice-options:trickle"));
RTC_TEST_ASSERT(memcmp(sdp, fixture, sdp_len) == 0);

sdp_len = 8;
RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_SDP_BUFFER,
                rtc_sdp_write_offer(&params, RTC_SDP_DIRECTION_SENDRECV,
                                    RTC_SDP_DIRECTION_SENDRECV, sdp,
                                    &sdp_len));
```

**规划提示:** 增加一个 API-level 测试，证明 backend fingerprint 覆盖/替代 create-time 临时 fingerprint；保留 writer 的纯 transform 测试。

---

### `CMakeLists.txt`（config, build graph）

**类比:** `CMakeLists.txt`

**库源文件登记模式**（行 12-27）:
```cmake
add_library(rtc STATIC
    src/api/rtc.c
    src/api/peer_connection.c
    src/sdp/sdp_parser.c
    src/sdp/sdp_writer.c
    src/ice/ice.c
    src/net/demux.c
    src/observability/counters.c
)
```

**测试登记模式**（行 37-58）:
```cmake
if(RTC_BUILD_TESTS)
    enable_testing()
    add_executable(rtc_tests
        tests/test_main.c
        tests/test_datagram.c
    )
    target_include_directories(rtc_tests PRIVATE src)
    target_link_libraries(rtc_tests PRIVATE rtc)
    add_test(NAME rtc_tests COMMAND rtc_tests)
endif()
```

**规划提示:** 新增 `src/security/security.c` 到 `rtc`，新增 `tests/test_security.c` 到 `rtc_tests`。默认构建不要强依赖 OpenSSL/libsrtp。

## 共享模式

### 固定内存与容量诊断

**来源:** `src/api/peer_connection.c` 行 210-273、385-475  
**适用:** `src/security/security.c`, `src/api/peer_connection.c`, `include/rtc/limits.h`, `include/rtc/config.h`

```c
*out_transactions = (rtc_stun_transaction_t *)rtc_core_alloc(
    arena, bytes, sizeof(void *),
    RTC_CAPACITY_RESOURCE_STUN_TRANSACTIONS, diag);
return *out_transactions != 0 ? RTC_STATUS_OK
                              : RTC_STATUS_CAPACITY_STUN_TRANSACTIONS;
```

新增 backend session、SRTP context、keying material storage 必须在 create-time 从 `pc->arena` 切分，失败返回容量 status 并保留 `rtc_capacity_diagnostics_t`。

### Observer 错误、状态和 trace

**来源:** `src/ice/ice.c` 行 65-103；`src/observability/observer.c` 行 3-29  
**适用:** security state machine、DTLS/SRTP 错误路径、早到 DTLS 拒绝

```c
rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR, "ice",
                        operation, 0);
rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED, reason);
```

安全层 subsystem 建议统一为 `"security"` 或 `"dtls"` / `"srtp"`，但同一错误链路保持一致。细粒度 reason 放入 trace 字段，不扩展大量 public status。

### UDP/datagram 输出

**来源:** `src/ice/ice.c` 行 518-524；`include/rtc/observer.h` 行 25  
**适用:** backend 产生 DTLS datagram、未来 SRTP protected RTP/RTCP 输出

```c
if (pc->observer.on_datagram != 0) {
    pc->observer.on_datagram(pc->observer.user_data, request, request_len);
}
```

库和 backend 都不得直接发送 socket；DTLS/SRTP 输出必须复用 `observer.on_datagram`。

### Network executor 亲和

**来源:** `src/api/peer_connection.c` 行 950-958  
**适用:** `rtc_security_handle_dtls_datagram`、SRTP/SRTCP datagram 输入输出路径

```c
status = rtc_executor_require(RTC_EXECUTOR_NETWORK);
if (status != RTC_STATUS_OK) {
    return rtc_pc_affinity_violation(pc, "receive_datagram");
}
```

安全 datagram 入口应继承 `receive_datagram` 的 network executor 约束；不要新增线程或后台任务。

### SDP fingerprint 数据流

**来源:** `src/sdp/sdp_parser.c` 行 187-197；`src/sdp/sdp_writer.c` 行 87-108  
**适用:** backend local fingerprint 查询、remote fingerprint 校验

```c
rtc_sdp_copy(out_description->dtls_fingerprint,
             sizeof(out_description->dtls_fingerprint), line + 14,
             line_len - 14u);
rtc_sdp_append(&writer, "a=fingerprint:%.*s\r\n",
               (int)params->dtls_fingerprint_len,
               params->dtls_fingerprint);
```

远端 fingerprint 已在 parser summary 中；本地 fingerprint 从 backend 查询后写入 SDP 参数快照。

## 无强类比文件

| 文件 | 角色 | 数据流 | 原因 |
|---|---|---|---|
| 可选真实 OpenSSL/libsrtp backend | service / adapter | request-response, file-I/O none | 代码库当前没有第三方 backend adapter；第 4 阶段也不以真实库为 gate |

## 元数据

**Analog search scope:** `include/`, `src/`, `tests/`, `CMakeLists.txt`  
**Files scanned:** 43 个源码/测试/配置文件  
**Pattern extraction date:** 2026-05-10  
**约束:** 保持纯 C、固定内存、无线程、用户负责 UDP/socket 收发，不引入 GPL/LGPL 依赖。
