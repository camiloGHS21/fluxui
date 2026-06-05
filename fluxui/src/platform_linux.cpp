#if defined(__linux__) && !defined(__ANDROID__)
#include "fluxui/platform.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xresource.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

namespace FluxUI {

static Display* g_display = nullptr;
static Window g_window = 0;
static Atom g_wmDeleteMessage;
static void* g_eventContext = nullptr;
static PlatformEventCallback g_eventCallback = nullptr;
static std::string g_clipboardText;

static unsigned long initialBackgroundPixel() {
    if (!g_display) return 0;
    static bool cached = false;
    static unsigned long pixel = 0;
    if (cached) return pixel;

    int screen = DefaultScreen(g_display);
    XColor color = {};
    color.red = 15 * 257;
    color.green = 15 * 257;
    color.blue = 23 * 257;
    color.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(g_display, DefaultColormap(g_display, screen), &color)) {
        pixel = color.pixel;
    } else {
        pixel = BlackPixel(g_display, screen);
    }
    cached = true;
    return pixel;
}

static void paintInitialBackground(Window window) {
    if (!g_display || !window) return;
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(g_display, window, &attrs)) return;
    GC gc = XCreateGC(g_display, window, 0, nullptr);
    if (!gc) return;
    XSetForeground(g_display, gc, initialBackgroundPixel());
    XFillRectangle(g_display, window, gc, 0, 0,
                   static_cast<unsigned int>(attrs.width),
                   static_cast<unsigned int>(attrs.height));
    XFreeGC(g_display, gc);
}

void Platform::setEventCallback(void* context, PlatformEventCallback callback) {
    g_eventContext = context;
    g_eventCallback = callback;
}

static void dispatchEvent(const PlatformInputEvent& event) {
    if (g_eventCallback && g_eventContext) {
        g_eventCallback(g_eventContext, event);
    }
}

// Convert X11 KeySym to a portable virtual key code compatible with Windows VK_ codes
static int keysymToVK(KeySym sym) {
    if (sym >= XK_a && sym <= XK_z) return 'A' + (int)(sym - XK_a);
    if (sym >= XK_A && sym <= XK_Z) return 'A' + (int)(sym - XK_A);
    if (sym >= XK_0 && sym <= XK_9) return '0' + (int)(sym - XK_0);
    if (sym >= XK_F1 && sym <= XK_F12) return 0x70 + (int)(sym - XK_F1); // VK_F1..VK_F12
    switch (sym) {
        case XK_Return:    return 0x0D; // VK_RETURN
        case XK_Escape:    return 0x1B; // VK_ESCAPE
        case XK_Tab:       return 0x09; // VK_TAB
        case XK_BackSpace: return 0x08; // VK_BACK
        case XK_Delete:    return 0x2E; // VK_DELETE
        case XK_Left:      return 0x25; // VK_LEFT
        case XK_Up:        return 0x26; // VK_UP
        case XK_Right:     return 0x27; // VK_RIGHT
        case XK_Down:      return 0x28; // VK_DOWN
        case XK_Home:      return 0x24; // VK_HOME
        case XK_End:       return 0x23; // VK_END
        case XK_Page_Up:   return 0x21; // VK_PRIOR
        case XK_Page_Down: return 0x22; // VK_NEXT
        case XK_Insert:    return 0x2D; // VK_INSERT
        case XK_space:     return 0x20; // VK_SPACE
        case XK_Shift_L: case XK_Shift_R:     return 0x10; // VK_SHIFT
        case XK_Control_L: case XK_Control_R: return 0x11; // VK_CONTROL
        case XK_Alt_L: case XK_Alt_R:         return 0x12; // VK_MENU
        default: return (int)sym & 0xFF;
    }
}

static int getX11Modifiers(unsigned int state) {
    int mods = MOD_NONE;
    if (state & ShiftMask)   mods |= MOD_SHIFT;
    if (state & ControlMask) mods |= MOD_CTRL;
    if (state & Mod1Mask)    mods |= MOD_ALT;   // Alt
    if (state & Mod4Mask)    mods |= MOD_GUI;    // Super/Meta
    return mods;
}

bool Platform::init() {
    XInitThreads();
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
    g_window = 0;
}

NativeWindowHandle Platform::createWindow(const PlatformWindowConfig& config) {
    if (!g_display) return nullptr;

    int screen = DefaultScreen(g_display);
    Window root = RootWindow(g_display, screen);

    XSetWindowAttributes attributes;
    attributes.background_pixmap = None;
    attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask | 
                            StructureNotifyMask | FocusChangeMask;

    unsigned long mask = CWEventMask | CWBackPixmap;

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

    g_window = window;
    return (NativeWindowHandle)window;
}

void Platform::prepareWindow(NativeWindowHandle window) {
    // X11/Wayland windows take their size from createWindow(); no pre-show
    // geometry step is needed here.
    (void)window;
}

void Platform::showWindow(NativeWindowHandle window) {
    if (g_display && window) {
        XMapRaised(g_display, (Window)window);
        XFlush(g_display);
    }
}

void Platform::destroyWindow(NativeWindowHandle window) {
    if (g_display && window) {
        XDestroyWindow(g_display, (Window)window);
        XFlush(g_display);
        if ((Window)window == g_window) g_window = 0;
    }
}

void Platform::processEvents(bool& running) {
    if (!g_display) return;
    
    XEvent xev;
    while (XPending(g_display)) {
        XNextEvent(g_display, &xev);
        
        switch (xev.type) {
        case SelectionRequest: {
            XSelectionRequestEvent* req = &xev.xselectionrequest;
            XEvent respond;
            respond.xselection.type = SelectionNotify;
            respond.xselection.requestor = req->requestor;
            respond.xselection.selection = req->selection;
            respond.xselection.target = req->target;
            respond.xselection.property = None; // Default to reject
            respond.xselection.time = req->time;

            Atom targets = XInternAtom(g_display, "TARGETS", False);
            Atom utf8_string = XInternAtom(g_display, "UTF8_STRING", False);

            if (req->target == targets) {
                Atom supported[] = { targets, utf8_string, XA_STRING };
                XChangeProperty(g_display, req->requestor, req->property, XA_ATOM, 32,
                                PropModeReplace, (unsigned char*)supported, 3);
                respond.xselection.property = req->property;
            } else if (req->target == utf8_string || req->target == XA_STRING) {
                XChangeProperty(g_display, req->requestor, req->property, req->target, 8,
                                PropModeReplace, (unsigned char*)g_clipboardText.c_str(),
                                g_clipboardText.length());
                respond.xselection.property = req->property;
            }

            XSendEvent(g_display, req->requestor, True, NoEventMask, &respond);
            XFlush(g_display);
            break;
        }
        case SelectionClear:
            break;
        case ClientMessage: {
            if ((Atom)xev.xclient.data.l[0] == g_wmDeleteMessage) {
                running = false;
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::Close;
                dispatchEvent(e);
            }
            break;
        }
        case MotionNotify: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::MouseMove;
            e.x = (float)xev.xmotion.x;
            e.y = (float)xev.xmotion.y;
            e.modifiers = getX11Modifiers(xev.xmotion.state);
            dispatchEvent(e);
            break;
        }
        case ButtonPress: {
            // X11: Button 1=left, 2=middle, 3=right, 4=scroll up, 5=scroll down
            if (xev.xbutton.button == 4 || xev.xbutton.button == 5) {
                // Scroll wheel
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::Scroll;
                e.y = (xev.xbutton.button == 4) ? 1.0f : -1.0f;
                dispatchEvent(e);
            } else if (xev.xbutton.button >= 1 && xev.xbutton.button <= 3) {
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::MouseDown;
                e.x = (float)xev.xbutton.x;
                e.y = (float)xev.xbutton.y;
                // Map: X11 1→0(left), 2→2(middle), 3→1(right)
                int btn = 0;
                if (xev.xbutton.button == 1) btn = 0;
                else if (xev.xbutton.button == 3) btn = 1;
                else if (xev.xbutton.button == 2) btn = 2;
                e.button = btn;
                e.modifiers = getX11Modifiers(xev.xbutton.state);
                dispatchEvent(e);
            }
            break;
        }
        case ButtonRelease: {
            if (xev.xbutton.button >= 1 && xev.xbutton.button <= 3) {
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::MouseUp;
                e.x = (float)xev.xbutton.x;
                e.y = (float)xev.xbutton.y;
                int btn = 0;
                if (xev.xbutton.button == 1) btn = 0;
                else if (xev.xbutton.button == 3) btn = 1;
                else if (xev.xbutton.button == 2) btn = 2;
                e.button = btn;
                e.modifiers = getX11Modifiers(xev.xbutton.state);
                dispatchEvent(e);
            }
            break;
        }
        case KeyPress: {
            KeySym sym = XkbKeycodeToKeysym(g_display, xev.xkey.keycode, 0, 0);

            // Dispatch key event
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::KeyDown;
            e.button = keysymToVK(sym);
            e.modifiers = getX11Modifiers(xev.xkey.state);
            dispatchEvent(e);

            // Also dispatch text input for printable characters
            char buf[32] = {};
            KeySym keysym;
            int len = XLookupString(&xev.xkey, buf, sizeof(buf) - 1, &keysym, nullptr);
            if (len > 0 && (unsigned char)buf[0] >= 32) {
                PlatformInputEvent te{};
                te.type = PlatformInputEvent::TextInput;
                std::memcpy(te.text, buf, std::min(len, 31));
                te.text[std::min(len, 31)] = '\0';
                dispatchEvent(te);
            }
            break;
        }
        case KeyRelease: {
            // Check for auto-repeat (X11 sends KeyRelease+KeyPress pairs)
            if (XEventsQueued(g_display, QueuedAfterReading)) {
                XEvent next;
                XPeekEvent(g_display, &next);
                if (next.type == KeyPress && next.xkey.time == xev.xkey.time &&
                    next.xkey.keycode == xev.xkey.keycode) {
                    // Auto-repeat — skip the release, the press will be handled next
                    break;
                }
            }
            KeySym sym = XkbKeycodeToKeysym(g_display, xev.xkey.keycode, 0, 0);
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::KeyUp;
            e.button = keysymToVK(sym);
            e.modifiers = getX11Modifiers(xev.xkey.state);
            dispatchEvent(e);
            break;
        }
        case ConfigureNotify: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::Resize;
            e.x = (float)xev.xconfigure.width;
            e.y = (float)xev.xconfigure.height;
            dispatchEvent(e);
            break;
        }
        case Expose: {
            if (xev.xexpose.count == 0) {
                if (!g_eventContext) {
                    paintInitialBackground(xev.xexpose.window);
                }
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::Expose;
                dispatchEvent(e);
            }
            break;
        }
        default:
            break;
        }
    }
}

void Platform::setClipboardText(const char* text) {
    if (!g_display || !g_window) return;
    g_clipboardText = text ? text : "";
    Atom clipboard = XInternAtom(g_display, "CLIPBOARD", False);
    XSetSelectionOwner(g_display, clipboard, g_window, CurrentTime);
    XFlush(g_display);
}

std::string Platform::getClipboardText() {
    if (!g_display || !g_window) return "";

    Atom clipboard = XInternAtom(g_display, "CLIPBOARD", False);
    
    // If we are already the owner of the selection, return our local cache
    if (XGetSelectionOwner(g_display, clipboard) == g_window) {
        return g_clipboardText;
    }

    Atom utf8_string = XInternAtom(g_display, "UTF8_STRING", False);
    Atom prop = XInternAtom(g_display, "FLUXUI_CLIPBOARD", False);

    // Clear any pending selection notify events on our window to avoid stale state
    XEvent dummy;
    while (XCheckTypedWindowEvent(g_display, g_window, SelectionNotify, &dummy)) {}

    XConvertSelection(g_display, clipboard, utf8_string, prop, g_window, CurrentTime);
    XFlush(g_display);

    // Synchronously check for SelectionNotify with a 200ms timeout, leaving other events in the queue
    auto startTime = std::chrono::steady_clock::now();
    XEvent xev;
    bool received = false;

    while (true) {
        if (XCheckTypedWindowEvent(g_display, g_window, SelectionNotify, &xev)) {
            if (xev.xselection.property != None) {
                received = true;
            }
            break;
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count() > 200) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::string result = "";
    if (received) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char* prop_data = nullptr;

        if (XGetWindowProperty(g_display, g_window, prop, 0, 65536, True, AnyPropertyType,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop_data) == Success) {
            if (prop_data) {
                result = (char*)prop_data;
                XFree(prop_data);
            }
        }
    }

    return result;
}

void Platform::openSystemURL(const std::string& url) {
    if (url.empty()) return;
    std::string cmd = "xdg-open \"" + url + "\" &";
    int ret = std::system(cmd.c_str());
    (void)ret;
}


NativeCursorHandle Platform::createSystemCursor(CursorType type) {
    if (!g_display) return nullptr;
    unsigned int shape = XC_left_ptr;
    switch (type) {
        case CursorType::Pointer: shape = XC_hand2; break;
        case CursorType::Text: shape = XC_xterm; break;
        case CursorType::ResizeNWSE: shape = 14; break; // XC_bottom_right_corner
        default: shape = XC_left_ptr; break;
    }
    return (NativeCursorHandle)XCreateFontCursor(g_display, shape);
}

void Platform::setCursor(NativeCursorHandle cursor) {
    if (g_display && g_window && cursor) {
        XDefineCursor(g_display, g_window, (Cursor)(uintptr_t)cursor);
        XFlush(g_display);
    }
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

bool Platform::isWindowActive(NativeWindowHandle window) {
    // Best-effort: treat as active when we can't cheaply determine focus.
    if (!g_display || !window) return true;
    Window focused = 0;
    int revert = 0;
    XGetInputFocus(g_display, &focused, &revert);
    if (focused == 0 || focused == PointerRoot) return true;
    return focused == (Window)window;
}

PowerStatus Platform::getPowerStatus() {
    PowerStatus status;
    // Read /sys/class/power_supply: AC "online" + battery capacity. No external
    // deps; if the files don't exist (desktop), source stays Unknown (=> AC).
    auto readFirstLine = [](const char* path, std::string& out) -> bool {
        FILE* f = std::fopen(path, "r");
        if (!f) return false;
        char buf[64] = {0};
        bool ok = std::fgets(buf, sizeof(buf), f) != nullptr;
        std::fclose(f);
        if (ok) { out = buf; while (!out.empty() && (out.back()=='\n'||out.back()=='\r')) out.pop_back(); }
        return ok;
    };
    std::string val;
    if (readFirstLine("/sys/class/power_supply/AC/online", val) ||
        readFirstLine("/sys/class/power_supply/AC0/online", val) ||
        readFirstLine("/sys/class/power_supply/ADP1/online", val)) {
        status.source = (val == "1") ? PowerSource::AC : PowerSource::Battery;
    }
    if (readFirstLine("/sys/class/power_supply/BAT0/capacity", val) ||
        readFirstLine("/sys/class/power_supply/BAT1/capacity", val)) {
        status.batteryPercent = std::atoi(val.c_str());
    }
    return status;
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
