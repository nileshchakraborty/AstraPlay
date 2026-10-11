# Pylux — Design Document

## 1. Design Principles

1. **Mobile-first, controller-second** — Touch UI must be usable without a physical controller. Desktop/TV UI must be fully navigable by controller alone.
2. **Platform parity** — Every streaming feature lives in `lib/` (C). Platform layers only adapt I/O (render, audio, input). No platform gets a "secret" behavior.
3. **Merge safety** — Avoid editing upstream-owned Qt files where possible. New Pylux features go in new files.
4. **Graceful degradation** — Features (Steamworks, Setsu, haptics) are compile-time optional and run-time detected. Missing SDK = silently skip.
5. **Privacy by default** — No analytics, telemetry, or user data sent anywhere beyond what PSN authentication requires.

---

## 2. Visual Design Language

### 2.1 Branding
- **Logo:** `pylux-logo.png` — dark purple/violet mark with a lightning-bolt P
- **App ID (Android):** `com.pylux.stream`
- **Bundle ID (iOS/macOS):** `io.github.ForWard_Technologies_LLC.Pylux`

### 2.2 Color Palette (Qt / QML)
Inherits from chiaki-ng palette with Pylux-specific accent overrides:

| Role | Color |
|------|-------|
| Primary accent | PlayStation blue `#003791` |
| Secondary accent | Pylux purple (TBD — match logo) |
| Background | Dark `#1a1a2e` |
| Surface | `#16213e` |
| On-surface text | `#e0e0e0` |
| Error | `#cf6679` |

### 2.3 Typography
- **Desktop:** System font (Qt default); monospaced log views use `Courier New`
- **Mobile (Android/iOS):** Platform system font (Roboto / SF Pro)

### 2.4 Icon Set
- Material Design Icons (Android)
- SF Symbols (iOS/macOS)
- Custom QML-rendered icons for desktop

---

## 3. Platform UI Designs

### 3.1 Android / Android TV

**Home Screen**
```
┌─────────────────────────────────────────┐
│ [Pylux Logo]          [Settings] [Login]│
├─────────────────────────────────────────┤
│                                         │
│  ┌─────┐  ┌─────┐  ┌─────┐            │
│  │ My  │  │Cloud│  │ Add │            │
│  │ PS5 │  │Games│  │Console│          │
│  └─────┘  └─────┘  └─────┘            │
│                                         │
│  Recent Games ──────────────────────── │
│  [ Game Art ] [ Game Art ] [ Game Art ] │
└─────────────────────────────────────────┘
```

**Stream Overlay (Mobile)**
```
┌─────────────────────────────────────────┐
│                                [✕]     │
│                                        │
│         [VIDEO STREAM AREA]            │
│                                        │
│  [↑]                          [△]      │
│ [←][→]  [PS][OPTIONS]        [○][×]   │
│  [↓]   [L3][SHARE]  [R3]    [□]       │
│ [L2][L1]                    [R1][R2]  │
└─────────────────────────────────────────┘
```

### 3.2 iOS / iPadOS

Mirrors Android flows with platform-native navigation patterns:
- `NavigationStack` / `TabView` for top-level navigation
- `GameController` framework for physical controller binding
- Stream overlay adapts to safe areas (notch, Dynamic Island)

### 3.3 Desktop Qt (macOS, Windows, Linux, Steam Deck)

**Main Window Layout**
```
┌─────────────────────────────────────────────┐
│ File  Settings  Help                        │
├──────────────┬──────────────────────────────┤
│              │                              │
│  Hosts       │   Stream / Game Launcher     │
│  ─────────   │   ────────────────────────  │
│  • My PS5    │   [Console art / Game list]  │
│  + Add Host  │                              │
│              │   Resolution: [1080p ▾]     │
│  Account     │   FPS:        [60    ▾]     │
│  ─────────   │   Codec:      [H265  ▾]     │
│  [Login PSN] │                              │
│              │   [▶ Start Stream]           │
└──────────────┴──────────────────────────────┘
```

---

## 4. User Flows

### 4.1 First-Run PSN Login (Mobile — PSStream relay)
```
App Launch
  │
  ▼
Show login screen
  │
  ▼
User taps "Login to PSN"
  │
  ▼
App calls psstream-server: POST /pylux/create-code
  │
  ▼
Display 6-char code to user: "ENTER AT pylux.stream/login"
  │
  ▼
User opens browser → enters code → PSN OAuth → npsso stored
  │
  ▼
App polls POST /pylux/get-tokens every 3 s
  │
  ▼
Token received → account registered → home screen
```

### 4.2 Remote Play Session Start
```
Home Screen
  │
  ▼
User selects console
  │
  ├──(LAN available)──► Discover via UDP broadcast → connect
  │
  └──(WAN / Internet)──► RUDP hole-punch → connect
             │
             ▼
      Session negotiation (registration PIN if first time)
             │
             ▼
      Video/audio stream started
             │
             ▼
      Input loop: controller → feedback sender → PS console
```

### 4.3 Internet Play
```
Home Screen → "Cloud Games" tab
  │
  ▼
App fetches game catalog (cloudcatalog_fetch → cache)
  │
  ▼
User selects title
  │
  ▼
App resolves launchspec (Kamaji or Gaikai endpoint)
  │
  ▼
Cloud session negotiated → stream starts
```

---

## 5. Accessibility

- All interactive elements labeled for TalkBack (Android) / VoiceOver (iOS/macOS)
- Minimum touch target 44×44 pt (iOS HIG) / 48×48 dp (Material)
- Sufficient contrast ratios (WCAG AA minimum)
- Full keyboard + controller navigation on desktop (Qt::TabFocusAllControls)
- Stream overlay buttons can be repositioned (planned)

---

## 6. Localization

- String resources externalized (Android: `strings.xml`, iOS: `Localizable.strings`, Qt: `*.ts`)
- RTL layout support at platform level (Android/iOS automatic; Qt: `layoutDirection`)
- Date/time formats follow device locale

---

## 7. Error States & Empty States

| State | Treatment |
|-------|-----------|
| No console found (LAN) | Inline message + "Add manually" CTA |
| PSN auth expired | Toast → re-login flow |
| Stream disconnected | Reconnect dialog (3 auto-retry, then manual) |
| No internet | Offline illustration + "Check connection" |
| Cloud game unavailable | "Not available in your region / subscription" |

---

## 8. Design Artifacts

| Artifact | Location |
|----------|----------|
| App logo | `pylux-logo.png` |
| Store badges | `assets/*.svg` |
| Android screenshots | `assets/screenshots/android-phones/` |
| Desktop metadata | `gui/io.github.ForWard_Technologies_LLC.Pylux.metainfo.xml` |
| macOS bundle plist template | `gui/MacOSXBundleInfo.plist.in` |
