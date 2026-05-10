---
phase: 2
plan: 02-03
subsystem: sdp-parser
tags: [sdp, parser, validator, chrome-profile, fixtures]
key-files:
  created:
    - tests/fixtures/chrome-offer-audio-video.sdp
    - tests/fixtures/chrome-answer-audio-video.sdp
    - tests/fixtures/invalid-missing-bundle.sdp
    - tests/fixtures/invalid-sendonly.sdp
  modified:
    - src/sdp/sdp_parser.c
    - tests/test_sdp_parser.c
metrics:
  tests: 8
  commits: 1
---

# 计划 02-03 总结：Chrome SDP parser、profile validator 与错误路径

## 完成内容

- 实现以 `(const char *, size_t)` 为输入边界的 SDP line scanner，不依赖 NUL 终止。
- 解析并校验 BUNDLE、audio/video mid、rtcp-mux、ICE ufrag/pwd、DTLS fingerprint/setup、方向、Opus 111 和 H264 103。
- 仅接受 `sendrecv` 和 `recvonly`，对 `sendonly`/`inactive` 返回 `RTC_STATUS_UNSUPPORTED`。
- 新增 Chrome offer/answer 成功 fixtures，以及缺失 BUNDLE 和非法方向错误 fixtures。

## 提交

| 提交 | 内容 |
|------|------|
| `a5cc8e7` | `feat(02): implement SDP JSEP offer answer` |

## 验证

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

## 偏差

无。

## Self-Check: PASSED

SDP-03 和 SDP-04 的解析侧成功路径与错误路径已覆盖，Chrome fixture 中的非目标 codec 不扩大本阶段支持范围。
