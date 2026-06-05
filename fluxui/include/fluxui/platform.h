#pragma once
#include <string>
#include <vector>
#include "fluxui/core.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#elif defined(__ANDROID__)
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#elif defined(__APPLE__)
#ifndef VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_METAL_EXT
#endif
#endif

#include <vulkan/vulkan.h>

namespace FluxUI {

struct PlatformWindowConfig {
    std::string title;
    int width;
    int height;
    bool resizable = true;
};

// ============================================================
//  Cross-platform input event (unified format for all OS)
// ============================================================
struct PlatformInputEvent {
    enum Type : int {
        MouseMove = 0, MouseDown, MouseUp,
        Scroll, KeyDown, KeyUp, TextInput,
        Resize, Close, Expose
    };
    Type type;
    float x = 0, y = 0;      // position, scroll delta, or window size
    int button = 0;           // mouse button (0=left,1=right,2=middle) or key code
    int modifiers = 0;        // MOD_SHIFT, MOD_CTRL, etc.
    char text[32] = {};       // UTF-8 text for TextInput events
};

using PlatformEventCallback = void(*)(void* context, const PlatformInputEvent& event);

// ============================================================
//  Power / energy status (for adaptive, battery-friendly pacing)
// ============================================================
enum class PowerSource : int {
    Unknown = 0,  // could not be determined (treated as AC for pacing)
    AC,           // plugged into wall power
    Battery       // running on battery
};

struct PowerStatus {
    PowerSource source = PowerSource::Unknown;
    bool batterySaver = false;   // OS battery-saver / low-power mode is active
    int batteryPercent = -1;     // 0..100, or -1 if unknown
};

class Platform {
public:
    static bool init();
    static void shutdown();

    static NativeWindowHandle createWindow(const PlatformWindowConfig& config);
    static void showWindow(NativeWindowHandle window);
    // Give the window its final on-screen geometry (e.g. maximized) while it is
    // still hidden/cloaked, so the first rendered frame is produced at the
    // correct size. Pairs with showWindow() which then reveals it flicker-free.
    static void prepareWindow(NativeWindowHandle window);
    static void destroyWindow(NativeWindowHandle window);
    
    static void processEvents(bool& running);
    
    static void setClipboardText(const char* text);
    static std::string getClipboardText();
    static void openSystemURL(const std::string& url);
    
    static NativeCursorHandle createSystemCursor(CursorType type);
    static void setCursor(NativeCursorHandle cursor);
    
    static void getWindowSize(NativeWindowHandle window, int& w, int& h);

    // True when the given window is focused/active and not minimized. Used to
    // drop the frame rate when the app is in the background (battery friendly).
    static bool isWindowActive(NativeWindowHandle window);

    // Query the system power source / battery state. Cheap; safe to poll on an
    // interval. Returns Unknown fields on platforms/hardware without batteries.
    static PowerStatus getPowerStatus();

    // Cross-platform input event callback
    static void setEventCallback(void* context, PlatformEventCallback callback);
    
    // Vulkan specific
    static void* loadVulkanLibrary();
    static void unloadVulkanLibrary(void* library);
    static void* getVulkanProc(void* library, const char* name);
    
    static std::vector<const char*> getVulkanInstanceExtensions();
    static bool createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface);
};

} // namespace FluxUI
