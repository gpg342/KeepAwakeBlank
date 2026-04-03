/*
 * KeepAwake Plus - A Windows application to prevent system sleep and display overlays
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For details of the GNU General Public License, see the LICENSE file
 * available at <https://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

// This application provides functionality to keep the Windows system awake and optionally display a colored overlay on a secondary monitor.

namespace {
    // Window class names and application title
    constexpr wchar_t kMainClassName[] = L"KeepAwakePlusMainWindow";
    constexpr wchar_t kOverlayClassName[] = L"KeepAwakePlusOverlayWindow";
    constexpr wchar_t kAppTitle[] = L"KeepAwake Plus";

    // Custom message for tray icon notifications
    constexpr UINT WMAPP_TRAY = WM_APP + 1;
    constexpr UINT ID_TRAY_ICON = 1001;

    // Hotkey identifiers
    constexpr UINT ID_HOTKEY_OVERLAY = 2001;
    constexpr UINT ID_HOTKEY_AWAKE = 2002;

    // Tray menu item identifiers
    constexpr UINT ID_TRAY_TOGGLE_AWAKE = 3001;
    constexpr UINT ID_TRAY_TOGGLE_OVERLAY = 3002;
    constexpr UINT ID_TRAY_SHOW_WINDOW = 3003;
    constexpr UINT ID_TRAY_EXIT = 3004;
    constexpr UINT ID_TRAY_COLOR_WHITE = 3005;
    constexpr UINT ID_TRAY_COLOR_BLACK = 3006;
    constexpr UINT ID_TRAY_COLOR_CUSTOM = 3007;

    // Timer identifier for periodic keep awake refresh
    constexpr UINT ID_TIMER_KEEP_AWAKE = 4001;

    // Global variables for application state
    HINSTANCE gInstance = nullptr;
    HWND gMainWindow = nullptr;
    HWND gOverlayWindow = nullptr;
    NOTIFYICONDATAW gTrayData{};

    bool gKeepAwakeEnabled = true;
    bool gOverlayVisible = false;
    COLORREF gOverlayColor = RGB(255, 255, 255);
    RECT gOverlayRect{};
    bool gHasSecondaryMonitor = false;

    // Structure to hold data during monitor enumeration
    struct MonitorSearchData {
        bool foundSecondary = false;
        RECT secondaryRect{};
    };

    // Enum for overlay color modes
    enum class OverlayColorMode {
        White,
        Black,
        Custom
    };

    OverlayColorMode gColorMode = OverlayColorMode::White;

    // Forward declarations of utility functions
    void UpdateWindowTitle();
    void UpdateTrayTooltip();
    void ApplyKeepAwakeState();
    void ToggleKeepAwake();
    void ToggleOverlay();
    void HideOverlay();
    void ShowOverlay();
    bool RefreshSecondaryMonitorRect();
    void RepositionOverlayWindow();
    void ShowContextMenu(HWND hwnd);
    bool ChooseCustomOverlayColor(HWND owner);

    // Callback function to enumerate monitors and find the secondary monitor
    BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
        auto* data = reinterpret_cast<MonitorSearchData*>(lParam);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(hMonitor, &mi)) {
            return TRUE;
        }

        if (!(mi.dwFlags & MONITORINFOF_PRIMARY)) {
            data->foundSecondary = true;
            data->secondaryRect = mi.rcMonitor;
            return FALSE;
        }
        return TRUE;
    }

    // Refreshes the rectangle of the secondary monitor and updates global state
    bool RefreshSecondaryMonitorRect() {
        MonitorSearchData data{};
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&data));
        gHasSecondaryMonitor = data.foundSecondary;
        if (data.foundSecondary) {
            gOverlayRect = data.secondaryRect;
        } else {
            SetRectEmpty(&gOverlayRect);
        }
        return gHasSecondaryMonitor;
    }

    // Applies the current keep awake state by setting thread execution state and updating UI
    void ApplyKeepAwakeState() {
        if (gKeepAwakeEnabled) {
            SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        } else {
            SetThreadExecutionState(ES_CONTINUOUS);
        }
        UpdateWindowTitle();
        UpdateTrayTooltip();
    }

    // Toggles the keep awake state
    void ToggleKeepAwake() {
        gKeepAwakeEnabled = !gKeepAwakeEnabled;
        ApplyKeepAwakeState();
    }

    // Repositions the overlay window to cover the secondary monitor
    void RepositionOverlayWindow() {
        if (!gOverlayWindow || !gHasSecondaryMonitor) {
            return;
        }

        const int width = gOverlayRect.right - gOverlayRect.left;
        const int height = gOverlayRect.bottom - gOverlayRect.top;
        SetWindowPos(
            gOverlayWindow,
            HWND_TOPMOST,
            gOverlayRect.left,
            gOverlayRect.top,
            width,
            height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
        InvalidateRect(gOverlayWindow, nullptr, TRUE);
    }

    // Hides the overlay window
    void HideOverlay() {
        if (gOverlayWindow) {
            ShowWindow(gOverlayWindow, SW_HIDE);
        }
        gOverlayVisible = false;
        UpdateWindowTitle();
        UpdateTrayTooltip();
    }

    // Shows the overlay window on the secondary monitor
    void ShowOverlay() {
        if (!RefreshSecondaryMonitorRect()) {
            MessageBoxW(
                gMainWindow,
                L"No secondary monitor was detected in Extend mode.\n\nConnect a second display and set Windows to Extend these displays.",
                kAppTitle,
                MB_OK | MB_ICONINFORMATION
            );
            gOverlayVisible = false;
            UpdateWindowTitle();
            UpdateTrayTooltip();
            return;
        }

        RepositionOverlayWindow();
        ShowWindow(gOverlayWindow, SW_SHOWNOACTIVATE);
        UpdateWindow(gOverlayWindow);
        gOverlayVisible = true;
        UpdateWindowTitle();
        UpdateTrayTooltip();
    }

    // Toggles the visibility of the overlay
    void ToggleOverlay() {
        if (gOverlayVisible) {
            HideOverlay();
        } else {
            ShowOverlay();
        }
    }

    // Updates the main window title to reflect current state
    void UpdateWindowTitle() {
        if (!gMainWindow) {
            return;
        }

        std::wstring colorText = L"White";
        if (gColorMode == OverlayColorMode::Black) {
            colorText = L"Black";
        } else if (gColorMode == OverlayColorMode::Custom) {
            colorText = L"Custom";
        }

        std::wstring title = std::wstring(kAppTitle)
            + L"  |  Keep Awake: " + (gKeepAwakeEnabled ? std::wstring(L"ON") : std::wstring(L"OFF"))
            + L"  |  Overlay: " + (gOverlayVisible ? std::wstring(L"ON") : std::wstring(L"OFF"))
            + L"  |  Colour: " + colorText;

        SetWindowTextW(gMainWindow, title.c_str());
    }

    // Updates the tray icon tooltip to reflect current state
    void UpdateTrayTooltip() {
        std::wstring tip = std::wstring(kAppTitle)
            + L"\nKeep Awake: " + (gKeepAwakeEnabled ? std::wstring(L"ON") : std::wstring(L"OFF"))
            + L"\nOverlay: " + (gOverlayVisible ? std::wstring(L"ON") : std::wstring(L"OFF"));

        wcsncpy_s(gTrayData.szTip, tip.c_str(), _TRUNCATE);
        gTrayData.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &gTrayData);
    }

    // Opens a color chooser dialog to select a custom overlay color
    bool ChooseCustomOverlayColor(HWND owner) {
        static COLORREF customColors[16]{};
        CHOOSECOLORW cc{};
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = owner;
        cc.lpCustColors = customColors;
        cc.rgbResult = gOverlayColor;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;

        if (ChooseColorW(&cc)) {
            gOverlayColor = cc.rgbResult;
            gColorMode = OverlayColorMode::Custom;
            if (gOverlayWindow) {
                InvalidateRect(gOverlayWindow, nullptr, TRUE);
            }
            UpdateWindowTitle();
            return true;
        }
        return false;
    }

    // Adds the tray icon to the system tray
    void AddTrayIcon(HWND hwnd) {
        ZeroMemory(&gTrayData, sizeof(gTrayData));
        gTrayData.cbSize = sizeof(gTrayData);
        gTrayData.hWnd = hwnd;
        gTrayData.uID = ID_TRAY_ICON;
        gTrayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        gTrayData.uCallbackMessage = WMAPP_TRAY;
        gTrayData.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcsncpy_s(gTrayData.szTip, L"KeepAwake Plus", _TRUNCATE);
        Shell_NotifyIconW(NIM_ADD, &gTrayData);
        UpdateTrayTooltip();
    }

    // Removes the tray icon from the system tray
    void RemoveTrayIcon() {
        Shell_NotifyIconW(NIM_DELETE, &gTrayData);
    }

    // Displays the context menu for the tray icon
    void ShowContextMenu(HWND hwnd) {
        HMENU menu = CreatePopupMenu();
        HMENU colorMenu = CreatePopupMenu();
        if (!menu || !colorMenu) {
            if (colorMenu) DestroyMenu(colorMenu);
            if (menu) DestroyMenu(menu);
            return;
        }

        AppendMenuW(menu, MF_STRING | (gKeepAwakeEnabled ? MF_CHECKED : 0), ID_TRAY_TOGGLE_AWAKE, L"Keep Awake ON / OFF\tCtrl+Alt+K");
        AppendMenuW(menu, MF_STRING | (gOverlayVisible ? MF_CHECKED : 0), ID_TRAY_TOGGLE_OVERLAY, L"Overlay ON / OFF\tCtrl+Alt+W");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(colorMenu, MF_STRING | (gColorMode == OverlayColorMode::White ? MF_CHECKED : 0), ID_TRAY_COLOR_WHITE, L"White");
        AppendMenuW(colorMenu, MF_STRING | (gColorMode == OverlayColorMode::Black ? MF_CHECKED : 0), ID_TRAY_COLOR_BLACK, L"Black");
        AppendMenuW(colorMenu, MF_STRING | (gColorMode == OverlayColorMode::Custom ? MF_CHECKED : 0), ID_TRAY_COLOR_CUSTOM, L"Choose Custom...");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(colorMenu), L"Overlay Colour");

        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW_WINDOW, L"Show Window");
        AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

        POINT pt{};
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
    }

    // Window procedure for the overlay window
    LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH brush = CreateSolidBrush(gOverlayColor);
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(brush);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DISPLAYCHANGE:
            if (gOverlayVisible) {
                ShowOverlay();
            } else {
                RefreshSecondaryMonitorRect();
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // Updates the status text in the main window
    void UpdateStatusText(HWND hwnd) {
        std::wstring line1 = L"Ctrl+Alt+K toggles Keep Awake";
        std::wstring line2 = L"Ctrl+Alt+W toggles the second monitor overlay";
        std::wstring line3 = L"Overlay colour can be White, Black, or Custom";
        std::wstring line4 = gHasSecondaryMonitor
            ? L"Secondary monitor detected"
            : L"No secondary monitor detected in Extend mode";

        SetWindowTextW(GetDlgItem(hwnd, 101), line1.c_str());
        SetWindowTextW(GetDlgItem(hwnd, 102), line2.c_str());
        SetWindowTextW(GetDlgItem(hwnd, 103), line3.c_str());
        SetWindowTextW(GetDlgItem(hwnd, 104), line4.c_str());
    }

    // Window procedure for the main window
    LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_CREATE: {
            // Create static text controls for status information
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 20, 20, 460, 24, hwnd, reinterpret_cast<HMENU>(101), gInstance, nullptr);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 20, 50, 460, 24, hwnd, reinterpret_cast<HMENU>(102), gInstance, nullptr);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 20, 80, 460, 24, hwnd, reinterpret_cast<HMENU>(103), gInstance, nullptr);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 20, 120, 460, 24, hwnd, reinterpret_cast<HMENU>(104), gInstance, nullptr);
            // Create buttons for toggling features and changing colors
            CreateWindowW(L"BUTTON", L"Keep Awake ON / OFF", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 170, 200, 32, hwnd, reinterpret_cast<HMENU>(ID_TRAY_TOGGLE_AWAKE), gInstance, nullptr);
            CreateWindowW(L"BUTTON", L"Overlay ON / OFF", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 240, 170, 200, 32, hwnd, reinterpret_cast<HMENU>(ID_TRAY_TOGGLE_OVERLAY), gInstance, nullptr);
            CreateWindowW(L"BUTTON", L"White", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 220, 100, 32, hwnd, reinterpret_cast<HMENU>(ID_TRAY_COLOR_WHITE), gInstance, nullptr);
            CreateWindowW(L"BUTTON", L"Black", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 220, 100, 32, hwnd, reinterpret_cast<HMENU>(ID_TRAY_COLOR_BLACK), gInstance, nullptr);
            CreateWindowW(L"BUTTON", L"Custom...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 240, 220, 100, 32, hwnd, reinterpret_cast<HMENU>(ID_TRAY_COLOR_CUSTOM), gInstance, nullptr);
            CreateWindowW(L"BUTTON", L"Hide to Tray", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 350, 220, 100, 32, hwnd, reinterpret_cast<HMENU>(ID_TRAY_SHOW_WINDOW), gInstance, nullptr);

            // Initialize monitor detection and UI
            RefreshSecondaryMonitorRect();
            UpdateWindowTitle();
            UpdateStatusText(hwnd);
            AddTrayIcon(hwnd);
            // Register global hotkeys
            RegisterHotKey(hwnd, ID_HOTKEY_OVERLAY, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W');
            RegisterHotKey(hwnd, ID_HOTKEY_AWAKE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'K');
            // Set a timer to periodically refresh keep awake state
            SetTimer(hwnd, ID_TIMER_KEEP_AWAKE, 30000, nullptr);
            ApplyKeepAwakeState();
            return 0;
        }
        case WM_TIMER:
            // Refresh keep awake state periodically
            if (wParam == ID_TIMER_KEEP_AWAKE && gKeepAwakeEnabled) {
                ApplyKeepAwakeState();
            }
            return 0;
        case WM_COMMAND: {
            // Handle button clicks and menu commands
            switch (LOWORD(wParam)) {
            case ID_TRAY_TOGGLE_AWAKE:
                ToggleKeepAwake();
                return 0;
            case ID_TRAY_TOGGLE_OVERLAY:
                ToggleOverlay();
                UpdateStatusText(hwnd);
                return 0;
            case ID_TRAY_COLOR_WHITE:
                gOverlayColor = RGB(255, 255, 255);
                gColorMode = OverlayColorMode::White;
                InvalidateRect(gOverlayWindow, nullptr, TRUE);
                UpdateWindowTitle();
                return 0;
            case ID_TRAY_COLOR_BLACK:
                gOverlayColor = RGB(0, 0, 0);
                gColorMode = OverlayColorMode::Black;
                InvalidateRect(gOverlayWindow, nullptr, TRUE);
                UpdateWindowTitle();
                return 0;
            case ID_TRAY_COLOR_CUSTOM:
                ChooseCustomOverlayColor(hwnd);
                return 0;
            case ID_TRAY_SHOW_WINDOW:
                ShowWindow(hwnd, SW_SHOW);
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                return 0;
            case ID_TRAY_EXIT:
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_HOTKEY:
            // Handle global hotkeys
            if (wParam == ID_HOTKEY_OVERLAY) {
                ToggleOverlay();
                UpdateStatusText(hwnd);
                return 0;
            }
            if (wParam == ID_HOTKEY_AWAKE) {
                ToggleKeepAwake();
                return 0;
            }
            break;
        case WMAPP_TRAY:
            // Handle tray icon messages
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowContextMenu(hwnd);
                return 0;
            }
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hwnd, SW_SHOW);
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
            // Handle display configuration changes
            RefreshSecondaryMonitorRect();
            if (gOverlayVisible) {
                ShowOverlay();
            }
            UpdateStatusText(hwnd);
            return 0;
        case WM_CLOSE:
            // Hide window instead of closing to keep app running in tray
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            // Clean up resources on application exit
            KillTimer(hwnd, ID_TIMER_KEEP_AWAKE);
            UnregisterHotKey(hwnd, ID_HOTKEY_OVERLAY);
            UnregisterHotKey(hwnd, ID_HOTKEY_AWAKE);
            RemoveTrayIcon();
            HideOverlay();
            gKeepAwakeEnabled = false;
            ApplyKeepAwakeState();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int nCmdShow) {
    // Set global instance handle
    gInstance = instance;

    // Register main window class
    WNDCLASSW mainClass{};
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.hInstance = instance;
    mainClass.lpszClassName = kMainClassName;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    mainClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&mainClass);

    // Register overlay window class
    WNDCLASSW overlayClass{};
    overlayClass.lpfnWndProc = OverlayWindowProc;
    overlayClass.hInstance = instance;
    overlayClass.lpszClassName = kOverlayClassName;
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    overlayClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&overlayClass);

    // Create main window
    gMainWindow = CreateWindowExW(
        0,
        kMainClassName,
        kAppTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        500,
        320,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!gMainWindow) {
        return 1;
    }

    // Create overlay window
    gOverlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kOverlayClassName,
        L"",
        WS_POPUP,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!gOverlayWindow) {
        DestroyWindow(gMainWindow);
        return 1;
    }

    // Show main window and start message loop
    ShowWindow(gMainWindow, nCmdShow);
    UpdateWindow(gMainWindow);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
