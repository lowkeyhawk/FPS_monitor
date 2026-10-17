#include "overlay.h"
#include "etw_consumer.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    OverlayWindow overlay;
    if (!overlay.Create(hInst)) {
        DWORD err = GetLastError();
        WCHAR msg[256];
        wsprintfW(msg, L"Failed to create overlay window (err %lu)", err);
        MessageBoxW(nullptr, msg, L"FPS Overlay", MB_OK | MB_ICONERROR);
        return 1;
    }

    EtwConsumer etw;
    if (!etw.Start([&overlay](uint32_t fps) {
        overlay.RequestFpsUpdate(fps);
    })) {
        WCHAR msg[128];
        wsprintfW(msg, L"ETW failed (err %lu). Run as Administrator.", etw.LastError());
        MessageBoxW(nullptr, msg, L"FPS Overlay", MB_OK | MB_ICONWARNING);
    }

    overlay.RunMessageLoop();
    etw.Stop();
    return 0;
}
