---
phase: 6
slug: chrome-end-to-end-acceptance
status: approved
shadcn_initialized: false
preset: none
created: 2026-05-11
reviewed_at: 2026-05-11T13:12:00+08:00
---

# Phase 6 — UI Design Contract

> Chrome 端到端验收页面的视觉与交互契约。该页面是本地诊断工具，不是营销页；第一屏必须直接呈现媒体预览、连接状态和可自动化读取的验收信号。

---

## Design System

| Property | Value |
|----------|-------|
| Tool | none |
| Preset | not applicable |
| Component library | none |
| Icon library | none |
| Font | system UI stack: `ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif` |

**Rationale:** 本阶段页面放在 `examples/chrome_e2e/page/`，使用原生 HTML/CSS/JS；仓库没有 React、Tailwind、shadcn 或既有前端 design system。不得为了验收页面引入 UI framework。

---

## Layout Contract

### Primary Screen

- **Viewport:** 支持 1280x720 desktop 和 390x844 mobile；desktop 为主要验收视口。
- **Focal point:** 左侧或上方的双视频区域是第一视觉锚点，包含 `Local synthetic media` 和 `Remote C sample media` 两个固定比例预览。
- **Secondary focus:** 连接状态条紧贴视频区域，显示 `Signaling`、`ICE`、`DTLS`、`SRTP`、`RTP`、`RTCP`、`Media files` 七个阶段。
- **Tertiary focus:** 右侧或下方为 compact diagnostics 面板，包含 candidate 数量、track 状态、bytes/frames stats、最近 JSONL/summary 状态。
- **No landing page:** 首屏就是验收工具，不出现 hero、marketing copy、feature intro 或装饰性插图。

### Responsive Behavior

- **Desktop >= 960px:** 两列布局：左侧 65% 为媒体和阶段条，右侧 35% 为 diagnostics；页面高度内优先展示核心状态，日志区域可滚动。
- **Tablet 640-959px:** 单列布局：媒体网格在上，阶段条在中，diagnostics 分组在下。
- **Mobile < 640px:** 单列紧凑布局：视频保持 16:9，阶段条换行为两列，diagnostics 使用折叠分组但默认展开当前失败层级。

### Stable Dimensions

- 视频面板使用 `aspect-ratio: 16 / 9`，最小高度 180px，防止 track 加载前后布局跳动。
- 状态 token 高度固定 32px，文本过长时使用省略号，完整值放在 `title`。
- Diagnostics 数值使用 tabular numerals 或等宽 fallback，counter 增长不得推动布局抖动。

---

## Spacing Scale

Declared values (must be multiples of 4):

| Token | Value | Usage |
|-------|-------|-------|
| xs | 4px | Inline status dot gap, table cell micro gaps |
| sm | 8px | Button/icon gaps, compact field padding |
| md | 16px | Default panel padding, media grid gap |
| lg | 24px | Page edge padding on desktop, major groups |
| xl | 32px | Desktop column gap |
| 2xl | 48px | Reserved for rare section separation only |
| 3xl | 64px | Not used in this tool UI |

Exceptions: minimum pointer target is 40px for clickable controls; this is a usability exception and remains a multiple of 4.

---

## Typography

| Role | Size | Weight | Line Height |
|------|------|--------|-------------|
| Body | 14px | 400 | 1.5 |
| Label | 12px | 600 | 1.35 |
| Heading | 18px | 600 | 1.25 |
| Display | 24px | 600 | 1.2 |

**Constraints:**

- Only the four sizes above may be used.
- Only weights `400` and `600` may be used.
- Letter spacing must be `0`.
- Do not scale font size with viewport width.
- Use `font-variant-numeric: tabular-nums` or equivalent for counters, byte totals, frame totals and elapsed time.

---

## Color

| Role | Value | Usage |
|------|-------|-------|
| Dominant (60%) | `#F7F8FA` | Page background and empty media surfaces |
| Secondary (30%) | `#FFFFFF` | Panels, video frames, diagnostics tables |
| Accent (10%) | `#0B6B5E` | Primary CTA, active connected status, focus ring |
| Destructive | `#B42318` | Failed status, stop action, destructive confirmation only |

Accent reserved for: primary CTA `Start Call`, active connected statuses (`connected`, `ready`, `receiving`), keyboard focus ring, selected diagnostics tab if tabs are used.

### Semantic Status Colors

| State | Value | Usage |
|-------|-------|-------|
| Pending | `#6B7280` | Not started, waiting for remote candidate |
| Running | `#B7791F` | Gathering/checking/connecting |
| Success | `#0B6B5E` | Connected, ready, receiving |
| Failure | `#B42318` | Failed layer and error badges |

Semantic status colors are allowed only on status dots, badges, and small labels. They must not become full-page backgrounds.

---

## Component Inventory

| Component | Contract |
|-----------|----------|
| Media preview panel | 16:9 surface with visible label, muted local preview, remote playback element, explicit `no track` empty state |
| Stage status bar | Seven fixed stages: `Signaling`, `ICE`, `DTLS`, `SRTP`, `RTP`, `RTCP`, `Media files`; each stage has dot + label + short state |
| Diagnostics panel | Dense key/value rows for candidates, tracks, bytes, frames, selected pair and latest summary layer |
| Action bar | `Start Call`, `Stop Run`, `Open Output Folder` if available, and `Copy Summary`; buttons have stable 40px height |
| Log strip | Last 5 structured events; scrollable full log link or expandable panel; no giant log dump in first viewport |
| Failure callout | Appears only on failure; includes layer, last error detail, suggested next diagnostic step |

No nested cards. Panels may be bordered surfaces, but individual repeated log rows or media previews should not be wrapped inside additional cards.

---

## Interaction Contract

- `Start Call` initializes synthetic media, connects WebSocket signaling, creates the offer and begins candidate exchange.
- `Stop Run` stops tracks, closes `RTCPeerConnection`, closes WebSocket, and marks current run as stopped.
- `Copy Summary` copies the latest JSON summary object as compact JSON.
- Page must expose stable DOM IDs or `data-testid` values for automation:
  - `data-testid="local-video"`
  - `data-testid="remote-video"`
  - `data-testid="stage-signaling"`
  - `data-testid="stage-ice"`
  - `data-testid="stage-dtls"`
  - `data-testid="stage-srtp"`
  - `data-testid="stage-rtp"`
  - `data-testid="stage-rtcp"`
  - `data-testid="stage-media-files"`
  - `data-testid="summary-layer"`
  - `data-testid="candidate-count"`
  - `data-testid="stats-frames"`
- Page must not request camera or microphone permissions; Chrome media is synthetic via canvas and Web Audio per D-07.
- Keyboard order follows visual order: action bar, local preview, remote preview, status stages, diagnostics, log strip.
- Controls must remain usable without hover.

---

## Copywriting Contract

| Element | Copy |
|---------|------|
| Primary CTA | Start Call |
| Secondary stop action | Stop Run |
| Copy action | Copy Summary |
| Empty state heading | Waiting for Chrome E2E run |
| Empty state body | Start the local signaling service and C example, then start the call to exchange SDP and candidates. |
| Remote media empty state | No remote media from C sample yet |
| Local media empty state | Synthetic canvas and oscillator are not started |
| Error state | {layer} failed. Check the latest JSONL event and rerun after fixing the reported layer. |
| Destructive confirmation | Stop Run: Stop tracks, close signaling, and end the current E2E run? |

Copy must stay operational and concise. Do not include visible instructional prose about how WebRTC works, keyboard shortcuts, or implementation details outside diagnostics.

---

## Accessibility Contract

- Every button has visible text; no icon-only controls.
- Video elements have adjacent visible labels and `aria-label` values.
- Status dots are never the only state carrier; each dot also has state text.
- Failure callout uses text and color together.
- Focus ring uses accent color `#0B6B5E` with 2px outline and 2px offset.
- All text/background pairs must meet WCAG AA contrast for normal text.

---

## Registry Safety

| Registry | Blocks Used | Safety Gate |
|----------|-------------|-------------|
| none | none | not applicable |

No shadcn official blocks and no third-party registries are used.

---

## Checker Sign-Off

- [x] Dimension 1 Copywriting: PASS
- [x] Dimension 2 Visuals: PASS
- [x] Dimension 3 Color: PASS
- [x] Dimension 4 Typography: PASS
- [x] Dimension 5 Spacing: PASS
- [x] Dimension 6 Registry Safety: PASS

**Approval:** approved 2026-05-11
