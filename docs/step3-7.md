### ID 3-7 实施计划：基线开关 + API 骨架 + Smoke 测试

#### Summary
- 本轮范围锁定为：`id=3~6` 并顺带完成 `id=7`。
- 目标是把工程从“仅占位静态库”提升到“有统一开关、有最小公共 API、有可运行 ctest”的可演进基线。
- 不进入协议实现，不引入信令，不引入编解码，不改线程模型。

#### Implementation Changes
1. 构建系统与开关（`id=3` + `id=7`）
- 新增 `cmake/options.cmake`，集中定义并设置默认值：
  - `WEBRTC_ENABLE_AUDIO=ON`
  - `WEBRTC_ENABLE_VIDEO=ON`
  - `WEBRTC_ENABLE_DATA_CHANNEL=ON`
  - `WEBRTC_ENABLE_TURN=OFF`
  - `WEBRTC_USE_MBEDTLS=ON`
  - `WEBRTC_PORT_POSIX=ON`
  - `WEBRTC_PORT_RTOS=OFF`
- 根 `CMakeLists.txt` 只 `include(cmake/options.cmake)`，不再内联 `WEBRTC_*` 的 `option(...)`。
- 增加端口开关校验：`POSIX/RTOS` 不能同时 `ON`，也不能同时 `OFF`。
- 将 `WEBRTC_*` 通过 `target_compile_definitions(webrtc_core PUBLIC ...)` 暴露给库与测试目标。

2. 公共接口与类型（`id=4`，并提前最小状态码）
- 新增 `include/webrtc/webrtc_status.h`（最小引导版）：
  - `WEBRTC_STATUS_OK`
  - `WEBRTC_STATUS_INVALID_ARG`
  - `WEBRTC_STATUS_NO_MEMORY`
  - `WEBRTC_STATUS_INVALID_STATE`
  - `WEBRTC_STATUS_NOT_IMPLEMENTED`
- 新增 `include/webrtc/webrtc.h`，先固定核心 API 轮廓（opaque 类型 + 无平台泄漏）：
```c
typedef struct webrtc_instance webrtc_instance_t;
typedef struct webrtc_peer_connection webrtc_peer_connection_t;
typedef int32_t webrtc_status_t;

webrtc_status_t webrtc_init(webrtc_instance_t** out_instance);
void webrtc_deinit(webrtc_instance_t* instance);

webrtc_status_t webrtc_peer_connection_create(
    webrtc_instance_t* instance,
    webrtc_peer_connection_t** out_pc);
void webrtc_peer_connection_free(webrtc_peer_connection_t* pc);

webrtc_status_t webrtc_set_local_description(
    webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len);
webrtc_status_t webrtc_set_remote_description(
    webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len);
webrtc_status_t webrtc_add_ice_candidate(
    webrtc_peer_connection_t* pc, const char* candidate, size_t candidate_len);

webrtc_status_t webrtc_pump_step(
    webrtc_instance_t* instance, uint64_t now_ms, uint32_t budget);
```

3. 源码接入（`id=5`）
- 新增 `src/api/webrtc_api.c` 并加入 `webrtc_core` 源列表，替代纯生成占位源码路径。
- 行为约定：
  - `webrtc_init`: 参数校验 + 最小实例分配，成功返回 `OK`。
  - `webrtc_deinit`: 空指针安全释放。
  - 其余函数：做参数校验后返回 `NOT_IMPLEMENTED`（`free` 为空指针安全 no-op）。
- 不引入 pthread/socket/file/errno 暴露，不引入后台线程。

4. 测试框架（`id=6`）
- 新增 `tests/CMakeLists.txt` 与 `tests/test_smoke.c`。
- `test_smoke.c`：链接 `webrtc_core`，执行 `webrtc_init`/`webrtc_deinit` 并断言成功；可额外断言一个未实现接口返回 `NOT_IMPLEMENTED`。
- 根 CMake 启用 `enable_testing()` 并 `add_subdirectory(tests)`。

#### Test Plan
1. `cmake -S . -B build/base`
2. `cmake --build build/base -j`
3. `ctest --test-dir build/base --output-on-failure`
4. `cmake -S . -B build/base -LA | rg WEBRTC_`
5. `rg -n "option\\(WEBRTC_" CMakeLists.txt cmake/options.cmake`

验收判定：
- `id=3`：`WEBRTC_*` 默认值可见且正确。
- `id=4`：`include/webrtc/webrtc.h` 存在并可被测试目标包含。
- `id=5`：`webrtc_core` 由真实 `src/api/webrtc_api.c` 构建成功。
- `id=6`：`ctest` 至少 1 项 `PASS`。
- `id=7`：`WEBRTC_*` 选项集中在 `cmake/options.cmake`。

#### Assumptions
- 按你确认的方案执行：本轮范围为 `id=3~7`，不扩展到 `id=8+`。
- `webrtc_status.h` 先做最小集引导，后续 `id=11` 再扩展分段错误码体系。
- 当前仅以 Linux/POSIX 构建与测试为准；RTOS 本轮未做运行验证。
