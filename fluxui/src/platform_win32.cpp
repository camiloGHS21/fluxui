#ifdef _WIN32
#include "fluxui/platform.h"
#include <windows.h>
#include <windowsx.h>
#include <iostream>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

namespace FluxUI {

// Cross-platform event callback storage
static void* g_eventContext = nullptr;
static PlatformEventCallback g_eventCallback = nullptr;

void Platform::setEventCallback(void* context, PlatformEventCallback callback) {
    g_eventContext = context;
    g_eventCallback = callback;
}

// Forward declaration of the internal event callback
extern void Internal_OnWindowEvent(void* app, UINT msg, WPARAM wParam, LPARAM lParam);

static HBRUSH initialBackgroundBrush() {
    static HBRUSH brush = CreateSolidBrush(RGB(15, 15, 23));
    return brush;
}

static void paintInitialBackground(HWND hwnd, HDC dc) {
    RECT rect;
    if (GetClientRect(hwnd, &rect)) {
        FillRect(dc, &rect, initialBackgroundBrush());
    }
}

static std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return std::wstring(text.begin(), text.end());
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

static LRESULT CALLBACK FluxUI_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    void* app = (void*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (app) {
        Internal_OnWindowEvent(app, msg, wParam, lParam);
    }
    
    if (msg == WM_ERASEBKGND) {
        if (app) {
            return 1;
        }
        paintInitialBackground(hwnd, (HDC)wParam);
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (!app) {
            paintInitialBackground(hwnd, dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Platform::init() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setDpiAwarenessContext = user32
        ? reinterpret_cast<SetDpiAwarenessContextFn>(
              GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
        : nullptr;
    if (setDpiAwarenessContext) {
        setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        SetProcessDPIAware();
    }
    return true; 
}

void Platform::shutdown() {
}

NativeWindowHandle Platform::createWindow(const PlatformWindowConfig& config) {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = FluxUI_WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"FluxUIWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = initialBackgroundBrush();
    wc.style = CS_OWNDC;
    RegisterClassExW(&wc);

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    
    std::wstring wTitle = utf8ToWide(config.title);
    DWORD style = WS_OVERLAPPEDWINDOW | WS_MAXIMIZE;
    if (!config.resizable) style &= ~WS_THICKFRAME;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"FluxUIWindowClass",
        wTitle.c_str(),
        style,
        x, y, config.width, config.height,
        nullptr, nullptr, hInst, nullptr
    );

    return (NativeWindowHandle)hwnd;
}

void Platform::showWindow(NativeWindowHandle window) {
    if (!window) return;
    HWND hwnd = (HWND)window;
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    if (!GetWindowLongPtr(hwnd, GWLP_USERDATA)) {
        UpdateWindow(hwnd);
    }
}

void Platform::destroyWindow(NativeWindowHandle window) {
    if (window) DestroyWindow((HWND)window);
}

void Platform::processEvents(bool& running) {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            running = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Platform::setClipboardText(const char* text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    size_t len = strlen(text);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (hMem) {
        char* p = (char*)GlobalLock(hMem);
        if (p) {
            memcpy(p, text, len + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }
    CloseClipboard();
}

std::string Platform::getClipboardText() {
    if (!OpenClipboard(nullptr)) return "";
    HANDLE hData = GetClipboardData(CF_TEXT);
    std::string result;
    if (hData) {
        char* p = (char*)GlobalLock(hData);
        if (p) {
            result = p;
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

NativeCursorHandle Platform::createSystemCursor(CursorType type) {
    LPCWSTR id = (LPCWSTR)32512; // IDC_ARROW
    switch (type) {
        case CursorType::Pointer: id = (LPCWSTR)32649; break; // IDC_HAND
        case CursorType::Text: id = (LPCWSTR)32513; break;    // IDC_IBEAM
        default: id = (LPCWSTR)32512; break;                  // IDC_ARROW
    }
    return (NativeCursorHandle)LoadCursorW(nullptr, id);
}

void Platform::setCursor(NativeCursorHandle cursor) {
    SetCursor((HCURSOR)cursor);
}

void Platform::getWindowSize(NativeWindowHandle window, int& w, int& h) {
    RECT rect;
    if (GetClientRect((HWND)window, &rect)) {
        w = rect.right - rect.left;
        h = rect.bottom - rect.top;
    }
}

void* Platform::loadVulkanLibrary() {
    HMODULE module = LoadLibraryExA("vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        module = LoadLibraryA("vulkan-1.dll");
    }
    return (void*)module;
}

void Platform::unloadVulkanLibrary(void* library) {
    if (library) FreeLibrary((HMODULE)library);
}

void* Platform::getVulkanProc(void* library, const char* name) {
    if (library) return (void*)GetProcAddress((HMODULE)library, name);
    return nullptr;
}

std::vector<const char*> Platform::getVulkanInstanceExtensions() {
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
}

bool Platform::createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface) {
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    surfaceInfo.hwnd = (HWND)window;

    return vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, surface) == VK_SUCCESS;
}

} // namespace FluxUI
#endif
