# 第 2 阶段：代码模式映射

## 结论

第 1 阶段已经建立公共 API、固定 arena、执行器亲和、observer、trace、counter 和 CTest 测试模式。第 2 阶段应复用这些边界，在 `src/sdp` 和 `src/jsep` 下新增协议层，`src/api/peer_connection.c` 只保留公开 API glue、亲和检查和可观测性输出。

## 现有模式

| 角色 | 现有路径 | 可复用模式 |
|------|----------|------------|
| 公开 API | `include/rtc/peer_connection.h` | 不透明 `rtc_peer_connection_t`，所有 API 返回 `rtc_status_t` |
| create-time 配置 | `include/rtc/config.h` | 用户传入 arena、limits、executor、observer、backend |
| 固定容量 | `include/rtc/limits.h` | `limits.sdp.max_description_bytes` 和 `limits.ice.max_candidates` 是第 2 阶段核心容量入口 |
| API glue | `src/api/peer_connection.c` | `rtc_require_pc`、signaling executor 亲和、observer error、trace |
| 内部状态 | `src/api/peer_connection.h` | 不透明句柄内部结构可增加 SDP/JSEP 子结构和 arena 切分结果 |
| 测试 | `tests/test_peer_connection.c`、`tests/test_observability.c` | 测试 executor、固定 config helper、CTest 单入口 |
| 构建 | `CMakeLists.txt` | 新增 C 文件、测试文件和 fixture 读取无需第三方依赖 |

## 推荐新增模式

| 角色 | 目标路径 | 模式说明 |
|------|----------|----------|
| SDP profile 类型 | `src/sdp/sdp.h` | 内部 description、media、codec、ICE/DTLS 参数结构 |
| SDP parser | `src/sdp/sdp_parser.c` | 长度有界 line scanner，填充固定 profile summary |
| SDP writer | `src/sdp/sdp_writer.c` | 固定 Chrome 1v1 offer/answer serializer，输出 CRLF SDP |
| JSEP 状态机 | `src/jsep/jsep.h`、`src/jsep/jsep.c` | `stable`、`have-local-offer`、`have-remote-offer` 转换与拒绝原因 |
| SDP tests | `tests/test_sdp.c` | golden 生成、Chrome fixture 解析、错误字段测试 |
| JSEP tests | `tests/test_jsep.c` | caller/callee 顺序、非法顺序、candidate 容量和可观测性 |
| Fixtures | `tests/fixtures/*.sdp` | 纯 SDP 文件，不要求 C 测试解析 JSON |

## 对执行者的约束

- 不要绕过 `rtc_core_alloc` 或 arena 直接使用动态内存。
- 不要把 ICE connectivity、STUN、DTLS 握手、SRTP 或 RTP/RTCP 放入第 2 阶段。
- `addIceCandidate` 只能解析、校验、复制保存远端 candidate 字符串。
- `on_local_candidate` 不应在第 2 阶段主动触发。
- 所有新增公开字段和 trace/detail code 需要在中文 API 文档中说明。
- 所有测试继续通过 `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` 运行。

## 风险提示

- 当前 `rtc_peer_connection_config_t` 没有 SDP 参数字段；计划需要先补 create-time SDP 参数契约。
- 当前 `rtc_peer_connection_t` 只保存基础字段；计划需要在创建阶段切分 local/remote SDP buffer 和 candidate slots。
- 仓库根目录 `chrome_offer.sdp` 是 JSON 包装；执行时建议新增纯 SDP fixture，避免引入 JSON parser。
