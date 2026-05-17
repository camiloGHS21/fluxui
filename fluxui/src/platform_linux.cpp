#if defined(__linux__) && !defined(__ANDROID__)
#include "fluxui/platform.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <dlfcn.h>
#include <iostream>

namespace FluxUI {

static Display* g_display = nullptr;
static Atom g_wmDeleteMessage;

bool Platform::init() {
    g_display = XOpenDisplay(nullptr);
    if (!g_display) {
        std::cerr << "FluxUI: Failed to open X11 display." << std::endl;
        return false;
    }
    g_wmDeleteMessage = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    return true; 
}

void Platform::shutdown() {
    if (g_display) {
        XCloseDisplay(g_display);
        g_display = nullptr;
    }
}

NativeWindowHandle Platform::createWindow(const PlatformWindowConfig& config) {
    if (!g_display) return nullptr;

    int screen = DefaultScreen(g_display);
    Window root = RootWindow(g_display, screen);

    XSetWindowAttributes attributes;
    attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask | 
                            StructureNotifyMask;

    unsigned long mask = CWEventMask;

    Window window = XCreateWindow(g_display, root, 0, 0, 
                                  config.width, config.height, 0, 
                                  CopyFromParent, InputOutput, 
                                  CopyFromParent, mask, &attributes);

    XStoreName(g_display, window, config.title.c_str());
    
    // Set fixed size if not resizable
    if (!config.resizable) {
        XSizeHints* hints = XAllocSizeHints();
        hints->flags = PMinSize | PMaxSize;
        hints->min_width = config.width;
        hints->max_width = config.width;
        hints->min_height = config.height;
        hints->max_height = config.height;
        XSetWMNormalHints(g_display, window, hints);
        XFree(hints);
    }

    XSetWMProtocols(g_display, window, &g_wmDeleteMessage, 1);
    XMapWindow(g_display, window);
    XFlush(g_display);

    return (NativeWindowHandle)window;
}

void Platform::destroyWindow(NativeWindowHandle window) {
    if (g_display && window) {
        XDestroyWindow(g_display, (Window)window);
        XFlush(g_display);
    }
}

void Platform::processEvents(bool& running) {
    if (!g_display) return;
    
    XEvent event;
    while (XPending(g_display)) {
        XNextEvent(g_display, &event);
        
        if (event.type == ClientMessage) {
            if ((Atom)event.xclient.data.l[0] == g_wmDeleteMessage) {
                running = false;
            }
        }
        // TODO: Map other X11 events to internal events
    }
}

void Platform::setClipboardText(const char* text) {
    // Basic clipboard implementation requires handling selection events in X11
}

std::string Platform::getClipboardText() {
    return "";
}

NativeCursorHandle Platform::createSystemCursor(CursorType type) {
    if (!g_display) return nullptr;
    unsigned int shape = XC_left_ptr;
    switch (type) {
        case CursorType::Pointer: shape = XC_hand2; break;
        case CursorType::Text: shape = XC_xterm; break;
        default: shape = XC_left_ptr; break;
    }
    return (NativeCursorHandle)XCreateFontCursor(g_display, shape);
}

void Platform::setCursor(NativeCursorHandle cursor) {
    // Requires window context to set cursor effectively in X11
}

void Platform::getWindowSize(NativeWindowHandle window, int& w, int& h) {
    if (!g_display || !window) return;
    Window root;
    int x, y;
    unsigned int width, height, border, depth;
    if (XGetGeometry(g_display, (Window)window, &root, &x, &y, &width, &height, &border, &depth)) {
        w = width;
        h = height;
    }
}

void* Platform::loadVulkanLibrary() {
    const char* names[] = {
        "libvulkan.so.1",
        "libvulkan.so",
        nullptr
    };
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
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
}

bool Platform::createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface) {
    if (!g_display || !window) return false;

    VkXlibSurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.dpy = g_display;
    surfaceInfo.window = (Window)window;

    auto func = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
    if (!func) return false;
    
    return func(instance, &surfaceInfo, nullptr, surface) == VK_SUCCESS;
}

} // namespace FluxUI
#endif
