#ifndef _WIN32
#include "fluxui/platform.h"
#include <dlfcn.h>
#include <iostream>

namespace FluxUI {

// This is a minimal stub for Linux. 
// A real implementation would use X11 (libX11) or Wayland.

bool Platform::init() {
    std::cerr << "FluxUI: Linux platform not fully implemented yet." << std::endl;
    return true; 
}

void Platform::shutdown() {}

NativeWindowHandle Platform::createWindow(const PlatformWindowConfig& config) {
    (void)config;
    return nullptr;
}

void Platform::destroyWindow(NativeWindowHandle window) {
    (void)window;
}

void Platform::processEvents(bool& running) {
    (void)running;
}

void Platform::setClipboardText(const char* text) {
    (void)text;
}

std::string Platform::getClipboardText() { return ""; }

NativeCursorHandle Platform::createSystemCursor(CursorType type) {
    (void)type;
    return nullptr;
}

void Platform::setCursor(NativeCursorHandle cursor) {
    (void)cursor;
}

void Platform::getWindowSize(NativeWindowHandle window, int& w, int& h) {
    (void)window;
    w = 800; h = 600;
}

void* Platform::loadVulkanLibrary() {
#if defined(__APPLE__)
    const char* names[] = {
        "libvulkan.1.dylib",
        "libvulkan.dylib",
        "libMoltenVK.dylib",
        nullptr
    };
#elif defined(__ANDROID__)
    const char* names[] = {
        "libvulkan.so",
        nullptr
    };
#else
    const char* names[] = {
        "libvulkan.so.1",
        "libvulkan.so",
        nullptr
    };
#endif
    for (int i = 0; names[i]; ++i) {
        if (void* library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL)) {
            return library;
        }
    }
    return nullptr;
}

void Platform::unloadVulkanLibrary(void* library) {
    if (library) dlclose(library);
}

void* Platform::getVulkanProc(void* library, const char* name) {
    if (!library || !name) return nullptr;
    return dlsym(library, name);
}

std::vector<const char*> Platform::getVulkanInstanceExtensions() {
#if defined(__APPLE__)
    return {"VK_KHR_surface", "VK_EXT_metal_surface"};
#elif defined(__ANDROID__)
    return {"VK_KHR_surface", "VK_KHR_android_surface"};
#else
    return {"VK_KHR_surface", "VK_KHR_xlib_surface"};
#endif
}

bool Platform::createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface) {
    (void)instance;
    (void)window;
    (void)surface;
    return false;
}

} // namespace FluxUI
#endif
