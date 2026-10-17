#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

class OverlayWindow {
public:
    OverlayWindow() = default;
    ~OverlayWindow();

    bool Create(HINSTANCE hInst);
    void Show();
    void SetFps(uint32_t fps);
    void RequestFpsUpdate(uint32_t fps);  // thread-safe: posts to window
    void RunMessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static OverlayWindow* g_instance;
    NOTIFYICONDATAW nid_ = {};
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RenderText(const std::wstring& text);

    HWND hwnd_ = nullptr;
    int dragStartX_ = 0;
    int dragStartY_ = 0;
    int winX_ = 100;
    int winY_ = 100;
    uint32_t fps_ = 0;

    static constexpr int WIN_W = 120;
    static constexpr int WIN_H = 60;
};
