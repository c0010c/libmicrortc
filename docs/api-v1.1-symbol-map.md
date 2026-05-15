# libmicrortc v1.1 Public Symbol Map

## 目标

本文冻结 installed `include/micrortc/*.h` 中当前 public symbols 的旧名到 v1.1 新名映射，供 Phase 8+ 做 public header 机械改名、调用点迁移和 residual scan 校验。

映射范围只覆盖 installed public headers：

| Header | Public 范围 |
|--------|-------------|
| `include/micrortc/micrortc.h` | Umbrella header、status type、core lifecycle functions |
| `include/micrortc/peer_connection.h` | PeerConnection、DataChannel、transceiver、encoded frame public API |

本表不映射 `src/` private helpers，不映射 tests/E2E harness 内部 helper，不改变 CMake package/target namespace。下游仍使用 `micrortc::micrortc`。

## 字段定义

| 字段 | 含义 | 规则 |
|------|------|------|
| `old public symbol` | 当前 v1 public header 中暴露的旧 symbol | 必须来自 installed `include/micrortc/*.h` |
| `new v1.1 symbol` | v1.1 public API 目标名称 | 函数用 `rtc_*`，类型/typedef/enum type 用 `Rtc*`，宏和 enum constants 用 `RTC_*` |
| `category` | symbol 分类 | 必须使用下方允许的 category 值 |
| `action` | Phase 8+ 对旧名的处理 | 必须明确 `rename/delete old; no wrapper/no alias` 或等价删除策略 |
| `source` | 当前旧 symbol 来源 | 使用 repo-relative header path 和行号或行范围 |

## 允许的 category 值

| category | 含义 |
|----------|------|
| `header_guard` | public header guard macro |
| `status_guard` | status duplicate-definition helper macro |
| `enum_type` | public enum typedef/type |
| `enum_constant` | public enum constant |
| `opaque_handle_typedef` | opaque pointer handle typedef |
| `struct_typedef` | public struct typedef |
| `callback_struct_typedef` | public callback/config struct typedef |
| `function` | public C function |

## 允许的 action 值

| action | 含义 |
|--------|------|
| `rename/delete old; no wrapper/no alias` | 旧 public symbol 改为新名，installed headers 删除旧名，不提供 compatibility wrapper、macro alias 或 typedef alias |

所有映射行都遵守 API-04：旧 `mrtc_*`、`MRTC_*`、AWS/KVS 风格 public names 默认删除；本文档和迁移文档可作为 `migration_doc_whitelist` 提及旧名，但 public headers 不保留旧名兼容层。

## Core / Header Helpers / Status

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `MRTC_MICRORTC_H` | `RTC_MICRORTC_H` | `header_guard` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:1` |
| `MRTC_STATUS_DEFINED` | `RTC_STATUS_DEFINED` | `status_guard` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:8`, `include/micrortc/peer_connection.h:7` |
| `MRTC_STATUS` | `RtcStatus` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:10`, `include/micrortc/peer_connection.h:9` |
| `MRTC_STATUS_OK` | `RTC_STATUS_OK` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:11`, `include/micrortc/peer_connection.h:10` |
| `MRTC_STATUS_INVALID_ARG` | `RTC_STATUS_INVALID_ARG` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:12`, `include/micrortc/peer_connection.h:11` |
| `MRTC_STATUS_NOT_IMPLEMENTED` | `RTC_STATUS_NOT_IMPLEMENTED` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:13`, `include/micrortc/peer_connection.h:12` |
| `MRTC_STATUS_INVALID_STATE` | `RTC_STATUS_INVALID_STATE` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:14`, `include/micrortc/peer_connection.h:13` |
| `MRTC_STATUS_PARSE_ERROR` | `RTC_STATUS_PARSE_ERROR` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:15`, `include/micrortc/peer_connection.h:14` |
| `mrtc_version_string` | `rtc_version_string` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:19` |
| `mrtc_initialize` | `rtc_initialize` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:20` |
| `mrtc_shutdown` | `rtc_shutdown` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/micrortc.h:21` |

## PeerConnection Header Helpers

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `MRTC_PEER_CONNECTION_H` | `RTC_PEER_CONNECTION_H` | `header_guard` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:1` |

## Opaque Handles

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `MRTC_PEER_CONNECTION_HANDLE` | `RtcPeerConnection` | `opaque_handle_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:22` |
| `MRTC_DATA_CHANNEL_HANDLE` | `RtcDataChannel` | `opaque_handle_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:23` |
| `MRTC_RTP_TRANSCEIVER_HANDLE` | `RtcRtpTransceiver` | `opaque_handle_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:24` |

Opaque handle 新名不使用 public `_HANDLE` suffix。Phase 8 可选择同步重命名 opaque struct tag，但 public typedef 名以 `RtcPeerConnection`、`RtcDataChannel`、`RtcRtpTransceiver` 为准。

## Struct And Enum Types

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `MRTC_ICE_SERVER` | `RtcIceServer` | `struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:26` |
| `MRTC_PEER_CONNECTION_STATE` | `RtcPeerConnectionState` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:32` |
| `MRTC_DATA_CHANNEL_MESSAGE_TYPE` | `RtcDataChannelMessageType` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:41` |
| `MRTC_MEDIA_KIND` | `RtcMediaKind` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:46` |
| `MRTC_CODEC` | `RtcCodec` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:51` |
| `MRTC_RTP_TRANSCEIVER_DIRECTION` | `RtcRtpTransceiverDirection` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:56` |
| `MRTC_FRAME_FLAG` | `RtcFrameFlag` | `enum_type` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:63` |
| `MRTC_FRAME` | `RtcFrame` | `struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:68` |
| `MRTC_DATA_CHANNEL_INIT` | `RtcDataChannelInit` | `struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:78` |
| `MRTC_DATA_CHANNEL_CALLBACKS` | `RtcDataChannelCallbacks` | `callback_struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:84` |
| `MRTC_TRANSCEIVER_CALLBACKS` | `RtcTransceiverCallbacks` | `callback_struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:94` |
| `MRTC_TRANSCEIVER_INIT` | `RtcTransceiverInit` | `struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:101` |
| `MRTC_PEER_CONNECTION_CONFIG` | `RtcPeerConnectionConfig` | `struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:108` |
| `MRTC_SELECTED_CANDIDATE_PAIR_INFO` | `RtcSelectedCandidatePairInfo` | `struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:114` |
| `MRTC_PEER_CONNECTION_CALLBACKS` | `RtcPeerConnectionCallbacks` | `callback_struct_typedef` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:120` |

## Enum Constants

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `MRTC_PEER_CONNECTION_STATE_NEW` | `RTC_PEER_CONNECTION_STATE_NEW` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:33` |
| `MRTC_PEER_CONNECTION_STATE_CONNECTING` | `RTC_PEER_CONNECTION_STATE_CONNECTING` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:34` |
| `MRTC_PEER_CONNECTION_STATE_CONNECTED` | `RTC_PEER_CONNECTION_STATE_CONNECTED` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:35` |
| `MRTC_PEER_CONNECTION_STATE_DISCONNECTED` | `RTC_PEER_CONNECTION_STATE_DISCONNECTED` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:36` |
| `MRTC_PEER_CONNECTION_STATE_FAILED` | `RTC_PEER_CONNECTION_STATE_FAILED` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:37` |
| `MRTC_PEER_CONNECTION_STATE_CLOSED` | `RTC_PEER_CONNECTION_STATE_CLOSED` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:38` |
| `MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT` | `RTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:42` |
| `MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY` | `RTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:43` |
| `MRTC_MEDIA_KIND_AUDIO` | `RTC_MEDIA_KIND_AUDIO` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:47` |
| `MRTC_MEDIA_KIND_VIDEO` | `RTC_MEDIA_KIND_VIDEO` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:48` |
| `MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1` | `RTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:52` |
| `MRTC_CODEC_OPUS` | `RTC_CODEC_OPUS` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:53` |
| `MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV` | `RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:57` |
| `MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY` | `RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:58` |
| `MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY` | `RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:59` |
| `MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE` | `RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:60` |
| `MRTC_FRAME_FLAG_NONE` | `RTC_FRAME_FLAG_NONE` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:64` |
| `MRTC_FRAME_FLAG_KEY_FRAME` | `RTC_FRAME_FLAG_KEY_FRAME` | `enum_constant` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:65` |

## PeerConnection Functions

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `mrtc_peer_connection_create` | `rtc_peer_connection_create` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:126` |
| `mrtc_peer_connection_free` | `rtc_peer_connection_free` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:131` |
| `mrtc_peer_connection_set_remote_description` | `rtc_peer_connection_set_remote_description` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:133` |
| `mrtc_peer_connection_create_answer` | `rtc_peer_connection_create_answer` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:137` |
| `mrtc_peer_connection_set_local_description` | `rtc_peer_connection_set_local_description` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:142` |
| `mrtc_peer_connection_create_offer` | `rtc_peer_connection_create_offer` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:146` |
| `mrtc_peer_connection_add_ice_candidate` | `rtc_peer_connection_add_ice_candidate` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:151` |
| `mrtc_peer_connection_poll_transport` | `rtc_peer_connection_poll_transport` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:154` |
| `mrtc_peer_connection_get_selected_candidate_pair_info` | `rtc_peer_connection_get_selected_candidate_pair_info` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:157` |
| `mrtc_peer_connection_create_data_channel` | `rtc_peer_connection_create_data_channel` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:161` |
| `mrtc_peer_connection_add_transceiver` | `rtc_peer_connection_add_transceiver` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:168` |

## Transceiver Functions

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `mrtc_transceiver_set_callbacks` | `rtc_transceiver_set_callbacks` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:173` |
| `mrtc_transceiver_on_frame` | `rtc_transceiver_on_frame` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:177` |
| `mrtc_transceiver_on_picture_loss` | `rtc_transceiver_on_picture_loss` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:183` |
| `mrtc_transceiver_write_frame` | `rtc_transceiver_write_frame` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:188` |
| `mrtc_transceiver_free` | `rtc_transceiver_free` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:191` |

## DataChannel Functions

| old public symbol | new v1.1 symbol | category | action | source |
|-------------------|-----------------|----------|--------|--------|
| `mrtc_data_channel_set_callbacks` | `rtc_data_channel_set_callbacks` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:193` |
| `mrtc_data_channel_send` | `rtc_data_channel_send` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:197` |
| `mrtc_data_channel_label` | `rtc_data_channel_label` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:202` |
| `mrtc_data_channel_id` | `rtc_data_channel_id` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:204` |
| `mrtc_data_channel_close` | `rtc_data_channel_close` | `function` | `rename/delete old; no wrapper/no alias` | `include/micrortc/peer_connection.h:206` |

## Non-Mapped Boundaries

| 范围 | 处理 |
|------|------|
| `src/` private helpers | 不作为 public symbols 映射；后续 private naming cleanup 由 Phase 10 处理 |
| `tests/` white-box helper usage | 不作为 public symbols 映射；残留分类可用 `test_harness_non_public_api` |
| `examples/chrome-e2e/` private media hook | 不作为 public API；Phase 10 再收敛 demo/private boundary |
| `.planning/milestones/**` source/compliance 记录 | 保留历史 AWS/KVS 和旧名事实；残留分类可用 `source_compliance_record` |
| `docs/api-v1.1-*.md` | 作为迁移和 API 契约白名单；残留分类可用 `migration_doc_whitelist` |

## Phase 8+ 检查清单

| 检查项 | 期望 |
|--------|------|
| 函数新名 | 所有 public functions 使用 `rtc_*` |
| 类型新名 | 所有 public typedef、struct typedef、enum type 使用 `Rtc*` |
| 常量新名 | 所有 public macros 和 enum constants 使用 `RTC_*` |
| 旧名处理 | 每个旧 public symbol 都是 rename/delete old，no wrapper/no alias |
| Public 范围 | 只对 installed `include/micrortc/*.h` public symbols 应用本映射 |
| Package namespace | `micrortc::micrortc` 保持不改 |
