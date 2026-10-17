# FPS Overlay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a transparent always-on-top Windows overlay showing compositor FPS as a single glassmorphism-styled number.

**Architecture:** Single C++/Win32 binary. Layered window with DWM blur-behind + DirectWrite text, fed by a live ETW trace on Microsoft-Windows-DxgKrnl counting presents per second.

**Tech Stack:** C++17, Win32 API, DWM API, DirectWrite, ETW (TraceLogging), CMake/MSVC.

**Spec:** docs/superpowers/specs/2026-10-16-fps-overlay-design.md

## Global Constraints

- C++17, Win32, zero third-party dependencies
- No runtime dependencies (no .NET, no VC++ redist needed beyond static link)
- Target: Windows 10/11 x64
- RAM < 10 MB idle, CPU < 0.5% idle
- Static link C++ runtime (/MT)

---

### Task 1: Overlay window with DWM blur and DirectWrite

**Files:**
- Create: `src/overlay.h`
- Create: `src/overlay.cpp`
- Create: `src/main.cpp`

**Interfaces:**
- Produces: `OverlayWindow` class with `Create(HINSTANCE)`, `Show()`, `SetFps(uint32_t)`, `RunMessageLoop()`

- [ ] Step 1: Create `overlay.h` — class declaration: layered window, DWM blur, DirectWrite, drag handling
- [ ] Step 2: Create `overlay.cpp` — implement window class registration, `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST`, `DwmEnableBlurBehind`, DirectWrite text rendering, `UpdateLayeredWindow`, drag via `WM_NCHITTEST` returning `HTCAPTION`, close on `WM_RBUTTONUP`
- [ ] Step 3: Create `main.cpp` — entry point, create OverlayWindow, pump messages
- [ ] Step 4: Verify it compiles: `cl /EHsc /std:c++17 /MT src/main.cpp src/overlay.cpp /link dwmapi.lib dwrite.lib`
- [ ] Step 5: Commit

### Task 2: ETW DxgKrnl consumer

**Files:**
- Create: `src/etw_consumer.h`
- Create: `src/etw_consumer.cpp`

**Interfaces:**
- Consumes: nothing (standalone)
- Produces: `EtwConsumer` class with `Start(std::function<void(uint32_t)> onFps)`, `Stop()`. The callback fires once per second with the present count.

- [ ] Step 1: Create `etw_consumer.h` — class declaration
- [ ] Step 2: Create `etw_consumer.cpp` — implement ETW session on `{DxgKrnl_PROVIDER_GUID}` (Microsoft-Windows-DxgKrnl), count `Present` events (event ID for DxgKrnl_Present or DxgKrnl_PresentHistory), fire callback each second with count, stop cleanly
- [ ] Step 3: Verify it compiles
- [ ] Step 4: Commit

### Task 3: Wire ETW into main loop

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `OverlayWindow::SetFps(uint32_t)`, `EtwConsumer::Start(callback)`

- [ ] Step 1: Instantiate EtwConsumer, pass `overlay.SetFps` as callback, call `Start()`
- [ ] Step 2: Verify compile + link
- [ ] Step 3: Commit

### Task 4: Build verification

**Files:**
- Create: `CMakeLists.txt` (optional, for MSVC build)

- [ ] Step 1: Write minimal CMakeLists.txt or build.bat
- [ ] Step 2: Build with MSVC, confirm .exe produced
- [ ] Step 3: Run it — verify transparent window appears, number updates, drag works, right-click closes
- [ ] Step 4: Commit
