# Phase 7: API 盘点与命名契约冻结 - Pattern Map

**Mapped:** 2026-05-15  
**Files analyzed:** 4 个新建/产出文件  
**Analogs found:** 4 / 4

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `docs/api-v1.1-boundary.md` | documentation | transform | `.planning/milestones/v1.0-phases/01-/COMPLIANCE.md` + `cmake/MicroRtcInstall.cmake` + `cmake/MicroRtcTests.cmake` | role-match |
| `docs/api-v1.1-symbol-map.md` | documentation | transform | `.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md` + `include/micrortc/*.h` | role-match |
| `scripts/scan-api-residuals.sh` | utility | batch/file-I/O | `scripts/verify-v1.sh` | exact role |
| `build/reports/api-residuals.json` | report artifact | batch/file-I/O | `build/reports/mrtc-v1-summary.json` + `tests/e2e/summary.js` | exact data-flow |

## Pattern Assignments

### `docs/api-v1.1-boundary.md` (documentation, transform)

**Analog:** `.planning/milestones/v1.0-phases/01-/COMPLIANCE.md`  
**Boundary fact sources:** `cmake/MicroRtcInstall.cmake`, `CMakeLists.txt`, `cmake/MicroRtcTests.cmake`, `include/micrortc/*.h`

**中文策略文档结构** (`.planning/milestones/v1.0-phases/01-/COMPLIANCE.md` lines 1-17):
```markdown
# Phase 1 Compliance: 许可证、NOTICE 与第三方依赖策略

## 目标

`libmicrortc` 派生自 Apache-2.0 许可的 AWS KVS WebRTC C SDK。Phase 1 的目标是先建立可执行的合规策略，确保后续搬迁、裁剪或重写派生文件时不会丢失 LICENSE、NOTICE、来源说明和第三方依赖声明。

## Canonical Sources

| 来源 | 用途 |
|------|------|
| `reflib/kvs-webrtc-sdk/LICENSE` | Apache-2.0 许可证文本来源 |
```

**Public installed header 边界** (`cmake/MicroRtcInstall.cmake` lines 3-16):
```cmake
function(mrtc_install_package target)
    install(
        TARGETS ${target}
        EXPORT micrortcTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(
        DIRECTORY include/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
```

**Build include 边界：public `include/` vs private `src/`** (`CMakeLists.txt` lines 35-41):
```cmake
target_include_directories(micrortc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
```

**Test-only private include 边界** (`cmake/MicroRtcTests.cmake` lines 1-18):
```cmake
function(mrtc_add_test target source labels)
    set(options WITH_PRIVATE_INCLUDES)
    cmake_parse_arguments(MRTC_TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_executable(${target} "${source}")
    if(MRTC_TEST_WITH_PRIVATE_INCLUDES)
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()
    target_link_libraries(${target} PRIVATE micrortc::micrortc)

    add_test(NAME ${target} COMMAND ${target} ${MRTC_TEST_COMMAND_ARGS})
```

**Pattern to copy:** 文档使用“目标 / Canonical Sources / 边界分类 / 检查清单 / 后续维护”结构。`public` 定义来自 installed `include/`；`private` 定义来自 library target 的 `PRIVATE src`；`test-only` 定义来自 `WITH_PRIVATE_INCLUDES`、E2E answerer 的 private include 和测试 harness。

---

### `docs/api-v1.1-symbol-map.md` (documentation, transform)

**Analog:** `.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md`  
**Symbol fact sources:** `include/micrortc/micrortc.h`, `include/micrortc/peer_connection.h`

**表格契约与规则先行** (`.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md` lines 18-27):
```markdown
## 必填字段

| 字段 | 含义 | 规则 |
|------|------|------|
| `source_path` | 来源文件或目录在本地参考库中的路径 | 必须以 `reflib/kvs-webrtc-sdk/` 开头 |
| `target_path` | 目标文件或目标目录路径 | 尚无目标文件时填 `n/a` |
| `origin_commit` | 来源提交 | Phase 1 基线为 `9eebcc4` |
| `derivation` | 派生方式 | 必须使用下方允许值 |
| `notes` | 裁剪、保留、排除或后续确认说明 | 必须写明边界或原因 |
```

**允许值/分类枚举模式** (`.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md` lines 37-47):
```markdown
## 允许的 derivation 值

| derivation | 含义 |
|------------|------|
| `copied` | 基本按来源文件复制，仅做路径或 include 调整 |
| `trimmed` | 从来源文件裁剪掉 AWS/KVS、sample 或平台无关部分 |
| `renamed` | 以来源文件为基础改名或移动，语义基本保留 |
| `rewritten-derived` | 参考来源实现重新组织或重写，仍属于派生实现 |
| `reference-only` | 只作为行为、测试或设计参考，没有直接派生代码进入目标 |
| `excluded` | 明确不进入核心库或目标交付 |
```

**当前 public symbol 来源：core/status** (`include/micrortc/micrortc.h` lines 8-23):
```c
#ifndef MRTC_STATUS_DEFINED
#define MRTC_STATUS_DEFINED
typedef enum MRTC_STATUS {
    MRTC_STATUS_OK = 0,
    MRTC_STATUS_INVALID_ARG = 1,
    MRTC_STATUS_NOT_IMPLEMENTED = 2,
    MRTC_STATUS_INVALID_STATE = 3,
    MRTC_STATUS_PARSE_ERROR = 4
} MRTC_STATUS;
#endif /* MRTC_STATUS_DEFINED */

const char *mrtc_version_string(void);
MRTC_STATUS mrtc_initialize(void);
void mrtc_shutdown(void);

#include <micrortc/peer_connection.h>
```

**当前 public symbol 来源：opaque handles / structs / callbacks** (`include/micrortc/peer_connection.h` lines 22-31, 84-106):
```c
typedef struct MRTC_PEER_CONNECTION *MRTC_PEER_CONNECTION_HANDLE;
typedef struct MRTC_DATA_CHANNEL *MRTC_DATA_CHANNEL_HANDLE;
typedef struct MRTC_RTP_TRANSCEIVER *MRTC_RTP_TRANSCEIVER_HANDLE;

typedef struct MRTC_ICE_SERVER {
    const char *urls;
    const char *username;
    const char *password;
} MRTC_ICE_SERVER;

typedef struct MRTC_DATA_CHANNEL_CALLBACKS {
    void (*on_open)(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel);
    void (*on_message)(void *user_data,
                       MRTC_DATA_CHANNEL_HANDLE channel,
                       MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                       const unsigned char *data,
                       size_t data_len);
    void (*on_close)(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel);
} MRTC_DATA_CHANNEL_CALLBACKS;
```

**当前 public symbol 来源：functions** (`include/micrortc/peer_connection.h` lines 126-166):
```c
MRTC_STATUS mrtc_peer_connection_create(const MRTC_PEER_CONNECTION_CONFIG *config,
                                        const MRTC_PEER_CONNECTION_CALLBACKS *callbacks,
                                        void *user_data,
                                        MRTC_PEER_CONNECTION_HANDLE *peer_connection);

void mrtc_peer_connection_free(MRTC_PEER_CONNECTION_HANDLE peer_connection);

MRTC_STATUS mrtc_peer_connection_set_remote_description(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                        const char *type,
                                                        const char *sdp);

MRTC_STATUS mrtc_peer_connection_create_data_channel(MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                                     const char *label,
                                                     const MRTC_DATA_CHANNEL_INIT *init,
                                                     const MRTC_DATA_CHANNEL_CALLBACKS *callbacks,
                                                     void *user_data,
                                                     MRTC_DATA_CHANNEL_HANDLE *channel);
```

**Pattern to copy:** 先定义字段和允许分类，再用 Markdown 表格列出 `old public symbol`、`new v1.1 symbol`、`category`、`action`、`source`。`action` 必须明确“rename/delete old; no wrapper/no alias”。Symbol inventory 只覆盖 installed `include/micrortc/*.h`，不要把 `src/**/*.h` private helper 写成 public symbol。

---

### `scripts/scan-api-residuals.sh` (utility, batch/file-I/O)

**Analog:** `scripts/verify-v1.sh`

**Bash header and repo-root discovery** (lines 1-10):
```bash
#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
FIXTURES_DIR="$ROOT_DIR/tests/fixtures"
TURN_CONFIG=""
SKIP_NPM_INSTALL=0
BROWSER_CHANNEL="${MRTC_E2E_BROWSER_CHANNEL:-chromium}"
```

**Usage + fail-fast option errors** (lines 12-38):
```bash
usage() {
    cat <<'EOF'
Usage: scripts/verify-v1.sh [options]
...
EOF
}

fail_usage() {
    printf 'verify-v1: %s\n\n' "$1" >&2
    usage >&2
    exit 2
}
```

**Option parsing pattern** (lines 40-89):
```bash
while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ "$#" -ge 2 ] || fail_usage "--build-dir requires a path"
            BUILD_DIR="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail_usage "unknown option: $1"
            ;;
    esac
done
```

**Repo-relative path normalization and report directory** (lines 92-108, 144-146):
```bash
case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$ROOT_DIR/$BUILD_DIR" ;;
esac

REPORT_DIR="$BUILD_DIR/reports"
SUMMARY_PATH="$REPORT_DIR/mrtc-v1-summary.json"

cd "$ROOT_DIR" || exit 1
mkdir -p "$REPORT_DIR"
summary_cli --init-v1 "$SUMMARY_PATH" "$([ -n "$TURN_CONFIG" ] && printf true || printf false)"
```

**Stage failure reporting pattern** (lines 127-141):
```bash
run_stage() {
    local stage="$1"
    shift
    local started_ms
    started_ms="$(node -e 'process.stdout.write(String(Date.now()))')"
    banner "$stage"
    if "$@"; then
        stage_update "$(printf '%s' "$stage" | tr '[:upper:] ' '[:lower:]_')" "passed" "$started_ms"
    else
        local status=$?
        stage_update "$(printf '%s' "$stage" | tr '[:upper:] ' '[:lower:]_')" "failed" "$started_ms" "command exited with status $status"
        banner "SUMMARY"
        summary_cli --finalize "$SUMMARY_PATH" "$START_EPOCH_MS"
        exit "$status"
    fi
}
```

**Pattern to copy:** 新脚本使用 Bash、`set -u`、`pipefail`、`ROOT_DIR`、`--format json`、`--output <path>`、`--check-symbol-map <path>`、未知参数 `exit 2`。默认写入 `build/reports/api-residuals.json`，默认排除 `reflib/**`、`build/**`、`build-*/**`、`cmake-build-debug/**`、`tests/e2e/node_modules/**` 和 E2E artifact 目录。不要打印 `mrtc-ice-servers.local.json` 内容。

---

### `build/reports/api-residuals.json` (report artifact, batch/file-I/O)

**Analog:** `build/reports/mrtc-v1-summary.json` and `tests/e2e/summary.js`

**现有 report shape** (`build/reports/mrtc-v1-summary.json` lines 1-18, 66-72):
```json
{
  "phase": "06-chrome-e2e",
  "build": {
    "status": "passed",
    "duration_ms": 539
  },
  "ctest": {
    "status": "passed",
    "duration_ms": 455
  },
  "chrome_turn": {
    "status": "skipped",
    "skipped": true,
    "duration_ms": 0
  },
  "turn_relay": {
    "status": "skipped",
    "skipped": true
  },
  "failure_stage": null,
  "failure_reason": null,
  "duration_ms": 4504
}
```

**Report write + secret redaction pattern** (`tests/e2e/summary.js` lines 104-107):
```javascript
function writeSummary(summaryPath, summary) {
  fs.mkdirSync(path.dirname(summaryPath), { recursive: true });
  fs.writeFileSync(summaryPath, `${JSON.stringify(redactSecrets(summary), null, 2)}\n`);
}
```

**Report normalization/status fields** (`tests/e2e/summary.js` lines 140-154):
```javascript
function normalizeV1Summary(input = {}) {
  const summary = deepMerge(createV1Summary(), input);
  summary.build = { ...(typeof summary.build === "object" ? summary.build : {}), status: statusOf(summary.build) };
  summary.ctest = { ...(typeof summary.ctest === "object" ? summary.ctest : {}), status: statusOf(summary.ctest) };
  summary.duration_ms = Number(summary.duration_ms || 0);
  return redactSecrets(summary);
}
```

**Secret self-test pattern** (`tests/e2e/summary.js` lines 215-231):
```javascript
if (argv.includes("--self-test-redaction")) {
  const tempPath = path.join(__dirname, "artifacts", "summary-redaction-self-test.json");
  writeSummary(tempPath, {
    turn_relay: {
      urls: "turn:secret-turn.example:3478?transport=udp&username=raw-user&credential=raw-credential",
      username: "raw-user",
      credential: "raw-credential",
      password: "raw-password",
    },
  });
  const text = fs.readFileSync(tempPath, "utf8");
  const forbidden = ["secret-turn.example", "raw-user", "raw-credential", "raw-password", "username", "credential", "password"];
```

**Pattern to copy:** `api-residuals.json` 应是 pretty JSON 并带换行，包含 `phase`、`status`、`summary`、`findings[]`、`failure_reason`、`duration_ms`。每条 finding 至少包含 `path`、`line`、`symbol`、`category`、`reason`。分类应支持 `forbidden_public_residual`、`migration_doc_whitelist`、`source_compliance_record`、`test_harness_non_public_api`、`ignored_generated_artifact`。报告不能包含 TURN 用户名、credential 或 password。

## Shared Patterns

### Public vs Private Boundary

**Source:** `CMakeLists.txt` + `cmake/MicroRtcInstall.cmake`  
**Apply to:** `docs/api-v1.1-boundary.md`, `scripts/scan-api-residuals.sh`

```cmake
target_include_directories(micrortc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
```

Public API 是 installed `include/`，private API 是 `src/`，不能把 tests 通过 `WITH_PRIVATE_INCLUDES` 访问到的 header 升级为 public API。

### Test-Only Boundary

**Source:** `cmake/MicroRtcTests.cmake`  
**Apply to:** `docs/api-v1.1-boundary.md`

```cmake
if(MRTC_TEST_WITH_PRIVATE_INCLUDES)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
target_link_libraries(${target} PRIVATE micrortc::micrortc)
```

Tests 可作为 white-box 验证使用 private header；Phase 7 文档要把这些列为 test-only 或 private usage，不要列入 public C API。

### Package Consumer Public API

**Source:** `tests/package-consumer/CMakeLists.txt`, `tests/package-consumer/package_consumer.c`  
**Apply to:** `docs/api-v1.1-boundary.md`, `docs/api-v1.1-symbol-map.md`

```cmake
find_package(micrortc CONFIG REQUIRED)

add_executable(package_consumer package_consumer.c)
target_link_libraries(package_consumer PRIVATE micrortc::micrortc)
```

```c
#include <micrortc/micrortc.h>
#include <micrortc/peer_connection.h>

const char *version = mrtc_version_string();
MRTC_ICE_SERVER ice_server = {"turn:example.test:3478?transport=udp", "user", "secret"};
MRTC_PEER_CONNECTION_CONFIG config = {0};
```

Package namespace `micrortc::micrortc` 保持不改；Phase 7 只冻结 C symbol rename，不规划 CMake package namespace rename。

### Source Compliance Whitelist

**Source:** `.planning/milestones/v1.0-phases/01-/COMPLIANCE.md`, `.planning/milestones/v1.0-phases/01-/SOURCE-MANIFEST.md`  
**Apply to:** `scripts/scan-api-residuals.sh`, `build/reports/api-residuals.json`

```markdown
| `src/source/Signaling` | `excluded` | AWS/KVS signaling client 属于应用层服务集成，不得成为核心库依赖 |
| `kvsWebrtcSignalingClient` target | `excluded` | 链接 `kvsCommonLws` 和 libwebsockets，服务 KVS signaling |
| AWS credential/storage/KVS service paths | `excluded` | 超出协议栈边界 |
```

```markdown
| `reflib/kvs-webrtc-sdk/src/source/Signaling/` | `n/a` | `9eebcc4` | `excluded` | AWS/KVS signaling client，不能成为核心库依赖。 | `excluded` | `PROTO-07` | `01` | `reviewed` |
```

残留扫描要把合规/溯源文档中的 AWS/KVS 记录标成 `source_compliance_record`，而不是 forbidden residual。

### No Compatibility Wrapper Policy

**Source:** `.planning/ROADMAP.md` lines 13-16, `.planning/REQUIREMENTS.md` lines 8-14  
**Apply to:** `docs/api-v1.1-boundary.md`, `docs/api-v1.1-symbol-map.md`, `scripts/scan-api-residuals.sh`

Roadmap 明确 v1.1 公共 C API 统一为 `rtc_*`、`Rtc*`、`RTC_*`，旧 `mrtc_*`、`MRTC_*` 和 AWS/KVS 风格 public names 默认迁移/删除，不新增兼容 wrapper 层。Requirements API-04 同样要求公共 API 默认不保留旧 typedef、handle 或 wrapper 兼容层。

### Secret Redaction

**Source:** `tests/e2e/turn-config.js`, `tests/e2e/summary.js`  
**Apply to:** `scripts/scan-api-residuals.sh`, `build/reports/api-residuals.json`

```javascript
const SECRET_KEYS = new Set(["username", "credential", "password"]);
const SECRET_VALUE_KEYS = ["username", "credential", "password", "token", "secret"];
```

```javascript
fs.writeFileSync(summaryPath, `${JSON.stringify(redactSecrets(summary), null, 2)}\n`);
```

Phase 7 的扫描报告只记录匹配的 path/line/symbol/category/reason，不读取或输出 `mrtc-ice-servers.local.json` 里的 secret 内容。

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| — | — | — | 4 个 Phase 7 产物都有可复用 analog；两个 docs 没有 `docs/` 目录内精确 analog，但已有 planning 合规文档和 source manifest 可复用表格/策略结构。 |

## Metadata

**Analog search scope:** `scripts/`, `cmake/`, `include/micrortc/`, `tests/package-consumer/`, `tests/peer_connection/`, `tests/e2e/summary.js`, `.planning/milestones/v1.0-phases/01-/`  
**Files scanned:** 175 个 tracked files in docs/scripts/cmake/include/src/tests/examples/planning-milestone scope  
**Strong analogs read:** 11 个文件  
**Pattern extraction date:** 2026-05-15
