# Phase 3: AWS 风格 API 与 signaling-free PeerConnection - Patterns

## Existing Patterns To Reuse

| 目标文件 | 最近模式 | 复用方式 |
|----------|----------|----------|
| `include/micrortc/micrortc.h` | Phase 2 public umbrella header | 保留 include guard、`extern "C"`、`MRTC_STATUS`、`mrtc_*` 命名；新增 PeerConnection API 时继续保持 C ABI。 |
| `src/micrortc.c` | Phase 2 无依赖实现 | 继续让 init/version/shutdown 无第三方依赖；PeerConnection 不应把基础 lifecycle 绑到 AWS/KVS 依赖。 |
| `CMakeLists.txt` | `micrortc` 静态库 target + CTest | 新增 `src/peer_connection.c`、`src/sdp.c` 到同一个 `micrortc` target；新增测试 executable 并用 `add_test` 注册。 |
| `tests/smoke/test_link.c` | 最小 C 可执行 smoke test | Phase 3 新测试继续用 `int main(void)` + return code 断言，不引入额外测试框架。 |
| `tests/package-consumer/package_consumer.c` | install-tree public API consumer | 扩展 consumer 以 include umbrella header 并至少编译 PeerConnection 类型/状态码，证明安装头可消费。 |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | 来源追溯表格 | Phase 3 若派生 AWS public header、SDP 或 PeerConnection 代码，追加具体 source/target 行。 |

## Planned Files and Closest Analogs

| Planned file | Role | Closest analog | Notes |
|--------------|------|----------------|-------|
| `include/micrortc/peer_connection.h` | Public PeerConnection API | `include/micrortc/micrortc.h` | 使用同一 include guard/extern C 风格，不暴露 private struct 字段。 |
| `src/peer_connection.c` | Lifecycle + state machine + API glue | `src/micrortc.c` | 保持纯 C99；不要引入 AWS/PIC allocator/logging。 |
| `src/sdp.h` | Private SDP helper interface | `src/micrortc.c` pattern extended | 不安装到 public include；只供 core source/test 使用。 |
| `src/sdp.c` | SDP parse/serialize/answer helper | `reflib/kvs-webrtc-sdk/src/source/Sdp/` after baseline restored | rewritten-derived 或 reference-informed；更新 manifest。 |
| `tests/sdp/test_sdp_roundtrip.c` | SDP behavior test | `tests/smoke/test_link.c` | 无框架 C test，失败时返回非零。 |
| `tests/peer_connection/test_peer_connection_api.c` | Public API flow test | `tests/package-consumer/package_consumer.c` | 覆盖 invalid args/state、answerer flow、offerer placeholder。 |
| `tests/fixtures/minimal_offer.sdp` | Test fixture | none | 可从本地 KVS 测试/样本 rewritten-derived 或手写 original；若派生则更新 manifest。 |

## CMake Pattern

- 当前 `add_library(micrortc STATIC src/micrortc.c)` 可改为多行 source list，但 target 名和 alias 不变。
- 新测试继续放在 `if(MRTC_BUILD_TESTS)` 块内。
- 不添加 `add_subdirectory(reflib/kvs-webrtc-sdk)`。
- 不链接 `libwebsockets`、AWS SDK C++、`kvsCommonLws`、`kvspicUtils` 或 `kvspicState`。

## API Pattern

建议 public signatures 使用调用者 buffer 输出，避免 Phase 3 提前引入 allocator：

- `MRTC_STATUS mrtc_peer_connection_create(const MRTC_PEER_CONNECTION_CONFIG *config, const MRTC_PEER_CONNECTION_CALLBACKS *callbacks, void *user_data, MRTC_PEER_CONNECTION_HANDLE *handle);`
- `void mrtc_peer_connection_free(MRTC_PEER_CONNECTION_HANDLE handle);`
- `MRTC_STATUS mrtc_peer_connection_set_remote_description(MRTC_PEER_CONNECTION_HANDLE handle, const char *type, const char *sdp);`
- `MRTC_STATUS mrtc_peer_connection_create_answer(MRTC_PEER_CONNECTION_HANDLE handle, char *buffer, size_t buffer_len, size_t *required_len);`
- `MRTC_STATUS mrtc_peer_connection_set_local_description(MRTC_PEER_CONNECTION_HANDLE handle, const char *type, const char *sdp);`
- `MRTC_STATUS mrtc_peer_connection_create_offer(MRTC_PEER_CONNECTION_HANDLE handle, char *buffer, size_t buffer_len, size_t *required_len);`
- `MRTC_STATUS mrtc_peer_connection_add_ice_candidate(MRTC_PEER_CONNECTION_HANDLE handle, const char *candidate);`

Planner/executor 可微调函数名，但必须保留以上语义和 D-01 到 D-18 的边界。

---
*Pattern map created: 2026-05-12*
