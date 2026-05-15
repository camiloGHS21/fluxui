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

class Platform {
public:
    static bool init();
    static void shutdown();

    static NativeWindowHandle createWindow(const PlatformWindowConfig& config);
    static void destroyWindow(NativeWindowHandle window);
    
    static void processEvents(bool& running);
    
    static void setClipboardText(const char* text);
    static std::string getClipboardText();
    
    static NativeCursorHandle createSystemCursor(CursorType type);
    static void setCursor(NativeCursorHandle cursor);
    
    static void getWindowSize(NativeWindowHandle window, int& w, int& h);
    
    // Vulkan specific
    static void* loadVulkanLibrary();
    static void unloadVulkanLibrary(void* library);
    static void* getVulkanProc(void* library, const char* name);
    
    static std::vector<const char*> getVulkanInstanceExtensions();
    static bool createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface);
};

} // namespace FluxUI
