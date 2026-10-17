#include "overlay.h"
#include <windowsx.h>
#include <dwmapi.h>

static constexpr UINT WM_TRAYICON_ = WM_APP + 1;
static constexpr UINT TRAY_ID_EXIT = 1001;

OverlayWindow::~OverlayWindow() {
    if (nid_.uFlags) Shell_NotifyIconW(NIM_DELETE, &nid_);
}

bool OverlayWindow::Create(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"FpsOverlayWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    g_instance = this;
    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"FpsOverlayWindow", L"FPS Overlay", WS_POPUP,
        winX_, winY_, WIN_W, WIN_H, nullptr, nullptr, hInst, nullptr);
    if (!hwnd_) return false;

    // Glassmorphism backdrop
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, WIN_W, WIN_H);
    DwmEnableBlurBehindWindow(hwnd_, &bb);
    DeleteObject(bb.hRgnBlur);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    // Layered window stays invisible until UpdateLayeredWindow is called at least once
    RenderText(std::to_wstring(fps_));

    // Tray icon
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON_;
    nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"FPS Overlay - right-click to exit");
    Shell_NotifyIconW(NIM_ADD, &nid_);
    return true;
}

void OverlayWindow::Show() {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void OverlayWindow::SetFps(uint32_t fps) {
    fps_ = fps;
    RenderText(std::to_wstring(fps_));
}

void OverlayWindow::RequestFpsUpdate(uint32_t fps) {
    if (hwnd_) PostMessageW(hwnd_, WM_APP + 2, (WPARAM)fps, 0);
}

void OverlayWindow::RenderText(const std::wstring& text) {
    // Set up GDI for alpha-blended layered update
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WIN_W;
    bmi.bmiHeader.biHeight = -WIN_H; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hbm);

    // Clear to transparent with a faint dark tint
    uint32_t* px = (uint32_t*)bits;
    for (int i = 0; i < WIN_W * WIN_H; i++) {
        px[i] = 0x22000000; // ~13% black, glassy
    }

    // White bold text
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));
    // Create font once, cache it
    static HFONT font = nullptr;
    if (!font) {
        font = CreateFontW(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
    HFONT oldFont = (HFONT)SelectObject(memDC, font);

    RECT rc = {0, 0, WIN_W, WIN_H};
    DrawTextW(memDC, text.c_str(), (int)text.length(), &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    GdiFlush();

    SelectObject(memDC, oldFont);

    // GDI doesn't set alpha: rebuild it as premultiplied from the text coverage
    for (int i = 0; i < WIN_W * WIN_H; i++) {
        uint32_t p = px[i];
        uint32_t r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
        uint32_t cov = r > g ? (r > b ? r : b) : (g > b ? g : b);
        uint32_t a = cov > 0x22 ? cov : 0x22;
        px[i] = (a << 24) | (p & 0x00FFFFFF);
    }

    // Push to layered window — nullptr dest keeps current position (no snap-back)
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT ptSrc = {0, 0};
    SIZE size = {WIN_W, WIN_H};
    UpdateLayeredWindow(hwnd_, screenDC, nullptr, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(hbm);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

OverlayWindow* OverlayWindow::g_instance = nullptr;

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = g_instance;
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
    if (!self->hwnd_) self->hwnd_ = hwnd; // valid from the very first message
    return self->HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTCAPTION;
    case WM_NCRBUTTONUP:
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        PostQuitMessage(0);
        return 0;
    case WM_TRAYICON_:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            SetForegroundWindow(hwnd);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, TRAY_ID_EXIT, L"Exit");
            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            PostMessageW(hwnd, WM_NULL, 0, 0); // dismiss menu properly
            DestroyMenu(menu);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == TRAY_ID_EXIT) PostQuitMessage(0);
        return 0;
    case WM_APP + 2:
        SetFps((uint32_t)wParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OverlayWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
