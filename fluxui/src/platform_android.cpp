#ifdef __ANDROID__
#include "fluxui/platform.h"
#include <android_native_app_glue.h>
#include <android/window.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <dlfcn.h>
#include <cstring>
#include <string>
#include <vector>

#define FLUXUI_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FluxUI", __VA_ARGS__)

namespace FluxUI {

static struct android_app* g_app = nullptr;
static void* g_eventContext = nullptr;
static PlatformEventCallback g_eventCallback = nullptr;

void Platform::setEventCallback(void* context, PlatformEventCallback callback) {
    g_eventContext = context;
    g_eventCallback = callback;
}

static void dispatchEvent(const PlatformInputEvent& event) {
    if (g_eventCallback && g_eventContext) {
        g_eventCallback(g_eventContext, event);
    }
}

// Convert Android AKEYCODE to Windows-compatible VK code
static int androidKeyToVK(int32_t keycode) {
    if (keycode >= AKEYCODE_A && keycode <= AKEYCODE_Z)
        return 'A' + (keycode - AKEYCODE_A);
    if (keycode >= AKEYCODE_0 && keycode <= AKEYCODE_9)
        return '0' + (keycode - AKEYCODE_0);
    if (keycode >= AKEYCODE_F1 && keycode <= AKEYCODE_F12)
        return 0x70 + (keycode - AKEYCODE_F1);
    switch (keycode) {
        case AKEYCODE_ENTER:       return 0x0D;
        case AKEYCODE_ESCAPE:      return 0x1B;
        case AKEYCODE_TAB:         return 0x09;
        case AKEYCODE_DEL:         return 0x08; // Backspace
        case AKEYCODE_FORWARD_DEL: return 0x2E; // Delete
        case AKEYCODE_DPAD_LEFT:   return 0x25;
        case AKEYCODE_DPAD_UP:     return 0x26;
        case AKEYCODE_DPAD_RIGHT:  return 0x27;
        case AKEYCODE_DPAD_DOWN:   return 0x28;
        case AKEYCODE_MOVE_HOME:   return 0x24;
        case AKEYCODE_MOVE_END:    return 0x23;
        case AKEYCODE_PAGE_UP:     return 0x21;
        case AKEYCODE_PAGE_DOWN:   return 0x22;
        case AKEYCODE_SPACE:       return 0x20;
        case AKEYCODE_SHIFT_LEFT:
        case AKEYCODE_SHIFT_RIGHT: return 0x10;
        case AKEYCODE_CTRL_LEFT:
        case AKEYCODE_CTRL_RIGHT:  return 0x11;
        case AKEYCODE_ALT_LEFT:
        case AKEYCODE_ALT_RIGHT:   return 0x12;
        default: return keycode;
    }
}

static int getAndroidModifiers(int32_t meta) {
    int mods = MOD_NONE;
    if (meta & AMETA_SHIFT_ON)  mods |= MOD_SHIFT;
    if (meta & AMETA_CTRL_ON)   mods |= MOD_CTRL;
    if (meta & AMETA_ALT_ON)    mods |= MOD_ALT;
    if (meta & AMETA_META_ON)   mods |= MOD_GUI;
    return mods;
}

static int32_t handleInputEvent(struct android_app* app, AInputEvent* inputEvent) {
    int32_t eventType = AInputEvent_getType(inputEvent);

    if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(inputEvent) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(inputEvent, 0);
        float y = AMotionEvent_getY(inputEvent, 0);

        switch (action) {
        case AMOTION_EVENT_ACTION_DOWN: {
            // Touch down = mouse move + mouse down (left button)
            PlatformInputEvent me{};
            me.type = PlatformInputEvent::MouseMove;
            me.x = x;
            me.y = y;
            dispatchEvent(me);

            PlatformInputEvent e{};
            e.type = PlatformInputEvent::MouseDown;
            e.x = x;
            e.y = y;
            e.button = 0; // Left button
            dispatchEvent(e);
            return 1;
        }
        case AMOTION_EVENT_ACTION_UP: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::MouseUp;
            e.x = x;
            e.y = y;
            e.button = 0;
            dispatchEvent(e);
            return 1;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::MouseMove;
            e.x = x;
            e.y = y;
            dispatchEvent(e);
            return 1;
        }
        case AMOTION_EVENT_ACTION_SCROLL: {
            float scrollY = AMotionEvent_getAxisValue(inputEvent, AMOTION_EVENT_AXIS_VSCROLL, 0);
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::Scroll;
            e.y = scrollY;
            dispatchEvent(e);
            return 1;
        }
        }
    } else if (eventType == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(inputEvent);
        int32_t keycode = AKeyEvent_getKeyCode(inputEvent);
        int32_t meta = AKeyEvent_getMetaState(inputEvent);

        if (action == AKEY_EVENT_ACTION_DOWN) {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::KeyDown;
            e.button = androidKeyToVK(keycode);
            e.modifiers = getAndroidModifiers(meta);
            dispatchEvent(e);
            return 1;
        } else if (action == AKEY_EVENT_ACTION_UP) {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::KeyUp;
            e.button = androidKeyToVK(keycode);
            e.modifiers = getAndroidModifiers(meta);
            dispatchEvent(e);
            return 1;
        }
    }
    return 0;
}

static void handleAppCmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            int w = ANativeWindow_getWidth(app->window);
            int h = ANativeWindow_getHeight(app->window);
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::Resize;
            e.x = (float)w;
            e.y = (float)h;
            dispatchEvent(e);
        }
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        if (app->window) {
            int w = ANativeWindow_getWidth(app->window);
            int h = ANativeWindow_getHeight(app->window);
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::Resize;
            e.x = (float)w;
            e.y = (float)h;
            dispatchEvent(e);
        }
        break;
    case APP_CMD_GAINED_FOCUS: {
        PlatformInputEvent e{};
        e.type = PlatformInputEvent::Expose;
        dispatchEvent(e);
        break;
    }
    case APP_CMD_TERM_WINDOW:
    case APP_CMD_DESTROY: {
        PlatformInputEvent e{};
        e.type = PlatformInputEvent::Close;
        dispatchEvent(e);
        break;
    }
    default:
        break;
    }
}

bool Platform::init() {
    if (!g_app) {
        FLUXUI_LOGI("FluxUI: android_app not set. Call fluxui_set_android_app() first.");
        return false;
    }
    g_app->onAppCmd = handleAppCmd;
    g_app->onInputEvent = handleInputEvent;
    return true;
}

void Platform::shutdown() {
}

NativeWindowHandle Platform::createWindow(const PlatformWindowConfig& config) {
    if (!g_app) return nullptr;
    // Wait until the native window is actually created by the OS
    while (!g_app->window) {
        int events;
        android_poll_source* source;
        if (ALooper_pollAll(-1, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(g_app, source);
        }
        if (g_app->destroyRequested) return nullptr;
    }
    return (NativeWindowHandle)g_app->window;
}

void Platform::showWindow(NativeWindowHandle window) {
    (void)window;
    // Android shows the surface as part of the Activity lifecycle.
}

void Platform::destroyWindow(NativeWindowHandle window) {
    // Window lifecycle is managed by Android
}

void Platform::processEvents(bool& running) {
    if (!g_app) return;
    int events;
    android_poll_source* source;
    while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
        if (source) source->process(g_app, source);
        if (g_app->destroyRequested) {
            running = false;
        }
    }
}

void Platform::setClipboardText(const char* text) {
    // Full clipboard requires JNI bridge to ClipboardManager
    FLUXUI_LOGI("FluxUI: Clipboard set not implemented (requires JNI)");
}

std::string Platform::getClipboardText() {
    // Full clipboard requires JNI bridge to ClipboardManager
    return "";
}

void Platform::openSystemURL(const std::string& url) {
    // Open system URL on Android requires JNI. We log it for now.
    FLUXUI_LOGI("FluxUI: Open URL '%s' (not implemented, JNI required)", url.c_str());
}


NativeCursorHandle Platform::createSystemCursor(CursorType type) {
    return nullptr; // Android doesn't use hardware cursors for touch
}

void Platform::setCursor(NativeCursorHandle cursor) {
    // No hardware cursor on touch-based Android
}

void Platform::getWindowSize(NativeWindowHandle window, int& w, int& h) {
    if (window) {
        ANativeWindow* androidWindow = (ANativeWindow*)window;
        w = ANativeWindow_getWidth(androidWindow);
        h = ANativeWindow_getHeight(androidWindow);
    }
}

bool Platform::isWindowActive(NativeWindowHandle window) {
    // Android visibility is driven by the activity lifecycle; assume active here.
    (void)window;
    return true;
}

PowerStatus Platform::getPowerStatus() {
    // Battery/charging state on Android comes through the Java BatteryManager,
    // which isn't wired into this native layer; report Unknown (treated as AC).
    return PowerStatus{};
}

void* Platform::loadVulkanLibrary() {
    return dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
}

void Platform::unloadVulkanLibrary(void* library) {
    if (library) dlclose(library);
}

void* Platform::getVulkanProc(void* library, const char* name) {
    if (library) return dlsym(library, name);
    return nullptr;
}

std::vector<const char*> Platform::getVulkanInstanceExtensions() {
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
}

bool Platform::createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface) {
    if (!window) return false;
    
    VkAndroidSurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = (ANativeWindow*)window;

    auto func = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
    if (!func) return false;
    
    return func(instance, &surfaceInfo, nullptr, surface) == VK_SUCCESS;
}

} // namespace FluxUI

// Public C API for setting the android_app from android_main
extern "C" void fluxui_set_android_app(struct android_app* app) {
    FluxUI::g_app = app;
}

#endif
