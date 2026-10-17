# FPS Overlay — Design Spec

**Date:** 2026-10-16
**Status:** Approved (consolidated in chat)

## Goal

A lightweight Windows desktop overlay showing the current compositor FPS as a single number. No graph, no settings, no extras.

## Architecture

C++ / Win32 single-process app. Three components in one binary:

1. **Overlay Window** — transparent, always-on-top, click-through, draggable layered window with DWM blur-behind (glassmorphism).
2. **ETW Consumer** — live trace session on `Microsoft-Windows-DxgKrnl` provider; counts presents per second across all processes.
3. **Renderer** — DirectWrite text on the layered window; updates once per second.

## Data Flow

```
DxgKrnl ETW events → count presents in sliding 1s window → FPS integer → DirectWrite → UpdateLayeredWindow
```

## Technical Constraints

- C++ / Win32, zero dependencies, no runtime
- RAM: < 10 MB idle
- CPU: < 0.5% idle
- No settings files, no config, no registry writes
- Right-click to close
- Drag to reposition
- DWM blur-behind enabled (glassmorphism backdrop)
- Single number display, no graph

## Files

| File | Responsibility |
|------|---------------|
| `src/main.cpp` | Entry point, window proc, message loop |
| `src/overlay.h` / `src/overlay.cpp` | Layered window, DWM blur, drag, DirectWrite text |
| `src/etw_consumer.h` / `src/etw_consumer.cpp` | ETW trace session, present counting |
| `CMakeLists.txt` | Build target |

## Testing

- Manual: launch, verify transparent always-on-top window appears
- Manual: drag to reposition
- Manual: right-click closes
- Manual: number updates ~1/s and matches expected refresh rate under load
