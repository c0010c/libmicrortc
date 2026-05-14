# Phase 6: Chrome 自动化 E2E 与测试收口 - Pattern Map

**Generated:** 2026-05-13

## Existing Patterns To Reuse

| New/Modified Area | Closest Existing Analog | Pattern to Reuse |
|-------------------|-------------------------|------------------|
| `examples/chrome-e2e/mrtc_chrome_answerer.c` | `tests/integration/mrtc_phase5_media_verify.c`, `tests/peer_connection/test_peer_connection_api.c` | 使用 public PeerConnection/DataChannel/transceiver API，结构化 label 输出，固定 fixture 输入，不引入采集/编码。 |
| `examples/chrome-e2e/index.html` | `tests/fixtures/minimal_offer.sdp` and SDP tests | 保持最小页面，只承载 WebRTC control/media elements 和 Playwright 状态 bridge。 |
| `tests/e2e/signaling-server.js` | `tests/integration/mrtc_phase4_network_verify.c` | 缺少配置硬失败、阶段化输出、清晰错误码；不要把 secret 写入日志。 |
| `tests/e2e/chrome-host.spec.js` | `tests/integration/mrtc_phase5_media_verify.c` | success label 分层：connection、datachannel、video、audio、summary。 |
| `tests/e2e/chrome-turn.spec.js` | `tests/integration/mrtc_phase4_network_verify.c` | TURN relay 是显式命令；缺配置/无 relay URL/未选中 relay 都必须失败。 |
| `tests/e2e/summary.js` | Phase 4/5 verifier stdout style | 输出机器可读 JSON，同时保留人类可扫的 stage banner。 |
| `scripts/verify-v1.sh` | README 中 Phase 4/5 命令序列 | 串起 configure/build/CTest/E2E，分阶段失败，不把 Chrome E2E 放入默认 CTest。 |
| `README.md`, `.planning/ROADMAP.md`, `.planning/STATE.md` | Phase 5 docs updates | 完成后只标记真实完成的要求；明确 Chrome host 与 TURN relay 命令边界。 |
| `.planning/phases/01-/SOURCE-MANIFEST.md` | Phase 3-5 manifest rows | 若从 KVS sample/test reference 派生 demo/test 逻辑，记录 `origin_commit: 9eebcc4` 和 derivation。 |

## Code Excerpts To Respect

### Public API Boundary

`include/micrortc/peer_connection.h` exposes only `MRTC_*` and `mrtc_*` names. Phase 6 demo must consume this public header first; private `src/*` headers are acceptable only in tests or explicitly diagnostic helpers.

### ICE Config Boundary

`mrtc-ice-servers.example.json` uses:

```json
{
  "ice_servers": [
    {
      "urls": "turn:<host>:3478?transport=udp",
      "username": "<username>",
      "credential": "<credential>"
    }
  ]
}
```

Do not log or commit real `username`, `credential`, `password` or TURN host values from local config.

### Verifier Output Style

Phase 4/5 verifiers print short deterministic labels such as `datachannel open`, `h264 receive ok`, `phase5 media verifier complete`. Phase 6 should keep the same style, plus a JSON summary for automation.

## Implementation Constraints

- `micrortc` target must not link Node, Playwright, WebSocket libraries, libwebsockets, GStreamer, FFmpeg, AWS SDK or KVS signaling.
- Node/Playwright dependencies stay under `tests/e2e/`.
- C demo and browser page stay under `examples/chrome-e2e/`.
- Default `ctest` remains deterministic; browser E2E uses explicit commands.
- TURN relay command must hard-fail when real local config is missing.

