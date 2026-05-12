# Phase 1 Baseline: 本地 KVS WebRTC SDK 基线

## 基线结论

`libmicrortc` 的剥离基线固定为本地目录 `reflib/kvs-webrtc-sdk`。后续研究、搬迁、裁剪、重写派生和差异讨论都必须先读取这个本地目录；GitHub upstream latest 不是默认参考，也不能替代本地快照。

## 复查命令

| 检查项 | 命令 | 当前结果 |
|--------|------|----------|
| 工作区状态 | `git -C reflib/kvs-webrtc-sdk status --short` | 无输出，干净 |
| 当前提交 | `git -C reflib/kvs-webrtc-sdk rev-parse --short HEAD` | `9eebcc4` |
| 版本描述 | `git -C reflib/kvs-webrtc-sdk describe --tags --always --dirty` | `v1.18.1` |

## 本地来源事实

| 字段 | 值 |
|------|----|
| source root | `reflib/kvs-webrtc-sdk` |
| project version | `KinesisVideoWebRTCClient VERSION 1.18.1` |
| tag/version | `v1.18.1` |
| origin commit | `9eebcc4` |
| dirty status | clean |

## 初始源码模块

`reflib/kvs-webrtc-sdk/src/source` 当前包含 `Crypto`、`Ice`、`Metrics`、`PeerConnection`、`Rtcp`、`Rtp`、`Sctp`、`Sdp`、`Signaling`、`Srtp`、`Stun`、`Threadpool`。
