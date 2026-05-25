#ifdef __APPLE__
#include "fluxui/platform.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#include <dlfcn.h>
#include <cstring>
#include <iostream>

namespace FluxUI {

static void* g_eventContext = nullptr;
static PlatformEventCallback g_eventCallback = nullptr;

static NSColor* initialBackgroundColor() {
    return [NSColor colorWithCalibratedRed:15.0 / 255.0
                                     green:15.0 / 255.0
                                      blue:23.0 / 255.0
                                     alpha:1.0];
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

static int getCocoaModifiers(NSUInteger flags) {
    int mods = MOD_NONE;
    if (flags & NSEventModifierFlagShift)   mods |= MOD_SHIFT;
    if (flags & NSEventModifierFlagControl) mods |= MOD_CTRL;
    if (flags & NSEventModifierFlagOption)  mods |= MOD_ALT;
    if (flags & NSEventModifierFlagCommand) mods |= MOD_GUI;
    return mods;
}

// Convert macOS key code to Windows-compatible VK code
static int cocoaKeyToVK(unsigned short keyCode) {
    switch (keyCode) {
        case 0x00: return 'A'; case 0x01: return 'S'; case 0x02: return 'D';
        case 0x03: return 'F'; case 0x04: return 'H'; case 0x05: return 'G';
        case 0x06: return 'Z'; case 0x07: return 'X'; case 0x08: return 'C';
        case 0x09: return 'V'; case 0x0B: return 'B'; case 0x0C: return 'Q';
        case 0x0D: return 'W'; case 0x0E: return 'E'; case 0x0F: return 'R';
        case 0x10: return 'Y'; case 0x11: return 'T'; case 0x12: return '1';
        case 0x13: return '2'; case 0x14: return '3'; case 0x15: return '4';
        case 0x16: return '6'; case 0x17: return '5'; case 0x19: return '9';
        case 0x1A: return '7'; case 0x1C: return '8'; case 0x1D: return '0';
        case 0x1F: return 'O'; case 0x20: return 'U'; case 0x22: return 'I';
        case 0x23: return 'P'; case 0x25: return 'L'; case 0x26: return 'J';
        case 0x28: return 'K'; case 0x2D: return 'N'; case 0x2E: return 'M';
        case 0x24: return 0x0D; // Return → VK_RETURN
        case 0x30: return 0x09; // Tab → VK_TAB
        case 0x31: return 0x20; // Space → VK_SPACE
        case 0x33: return 0x08; // Delete → VK_BACK
        case 0x35: return 0x1B; // Escape → VK_ESCAPE
        case 0x7B: return 0x25; // Left → VK_LEFT
        case 0x7C: return 0x27; // Right → VK_RIGHT
        case 0x7D: return 0x28; // Down → VK_DOWN
        case 0x7E: return 0x26; // Up → VK_UP
        case 0x73: return 0x24; // Home → VK_HOME
        case 0x77: return 0x23; // End → VK_END
        case 0x74: return 0x21; // PageUp → VK_PRIOR
        case 0x79: return 0x22; // PageDown → VK_NEXT
        case 0x75: return 0x2E; // ForwardDelete → VK_DELETE
        case 0x7A: return 0x70; // F1 → VK_F1
        case 0x78: return 0x71; // F2
        case 0x63: return 0x72; // F3
        case 0x76: return 0x73; // F4
        case 0x60: return 0x74; // F5
        case 0x61: return 0x75; // F6
        case 0x62: return 0x76; // F7
        case 0x64: return 0x77; // F8
        case 0x65: return 0x78; // F9
        case 0x6D: return 0x79; // F10
        case 0x67: return 0x7A; // F11
        case 0x6F: return 0x7B; // F12
        default: return (int)keyCode;
    }
}

@interface FluxUIView : NSView
@property (nonatomic, assign) void* app;
@end

@implementation FluxUIView
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)wantsUpdateLayer { return YES; }
- (BOOL)isFlipped { return YES; } // Match top-left origin like Win32/X11
- (BOOL)isOpaque { return g_eventContext ? NO : YES; }
- (void)updateLayer {
    self.layer.backgroundColor = g_eventContext ? nil : [initialBackgroundColor() CGColor];
}
@end

@interface FluxUIWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) void* app;
@end

@implementation FluxUIWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    PlatformInputEvent e{};
    e.type = PlatformInputEvent::Close;
    dispatchEvent(e);
    return NO; // Let the app handle closing
}

- (void)windowDidResize:(NSNotification *)notification {
    NSWindow* win = [notification object];
    NSRect rect = [win.contentView bounds];
    NSRect backing = [win.contentView convertRectToBacking:rect];
    PlatformInputEvent e{};
    e.type = PlatformInputEvent::Resize;
    e.x = (float)backing.size.width;
    e.y = (float)backing.size.height;
    dispatchEvent(e);
}
@end

bool Platform::init() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];
    return true; 
}

void Platform::shutdown() {
}

NativeWindowHandle Platform::createWindow(const PlatformWindowConfig& config) {
    NSRect rect = NSMakeRect(0, 0, config.width, config.height);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    if (config.resizable) style |= NSWindowStyleMaskResizable;
    
    NSWindow* window = [[NSWindow alloc] initWithContentRect:rect 
                                                   styleMask:style 
                                                     backing:NSBackingStoreBuffered 
                                                       defer:NO];
    [window setTitle:[NSString stringWithUTF8String:config.title.c_str()]];
    [window center];
    [window setBackgroundColor:initialBackgroundColor()];
    [window setAcceptsMouseMovedEvents:YES]; // Enable mouse tracking
    
    FluxUIView* view = [[FluxUIView alloc] initWithFrame:rect];
    [view setWantsLayer:YES];
    view.layer = [CAMetalLayer layer];
    view.layer.backgroundColor = [initialBackgroundColor() CGColor];
    [window setContentView:view];
    
    FluxUIWindowDelegate* delegate = [[FluxUIWindowDelegate alloc] init];
    objc_setAssociatedObject(window, "FluxUIDelegate", delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [window setDelegate:delegate];

    return (NativeWindowHandle)window;
}

void Platform::showWindow(NativeWindowHandle window) {
    if (!window) return;
    NSWindow* nsWindow = (NSWindow*)window;
    if (g_eventContext) {
        [nsWindow setOpaque:NO];
        [nsWindow setBackgroundColor:[NSColor clearColor]];
        if (nsWindow.contentView.layer) {
            nsWindow.contentView.layer.backgroundColor = nil;
        }
    }
    [nsWindow makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void Platform::destroyWindow(NativeWindowHandle window) {
    if (window) {
        NSWindow* nsWindow = (NSWindow*)window;
        [nsWindow close];
    }
}

void Platform::processEvents(bool& running) {
    NSEvent* event;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny 
                                       untilDate:nil 
                                          inMode:NSDefaultRunLoopMode 
                                         dequeue:YES])) {
        
        NSEventType type = [event type];
        int mods = getCocoaModifiers([event modifierFlags]);

        switch (type) {
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged: {
            NSWindow* win = [event window];
            if (win) {
                NSPoint loc = [event locationInWindow];
                NSRect bounds = [win.contentView bounds];
                // Flip Y since Cocoa is bottom-left origin (FluxUIView isFlipped handles view coords)
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::MouseMove;
                e.x = (float)loc.x;
                e.y = (float)(bounds.size.height - loc.y);
                e.modifiers = mods;
                dispatchEvent(e);
            }
            break;
        }
        case NSEventTypeLeftMouseDown:
        case NSEventTypeRightMouseDown:
        case NSEventTypeOtherMouseDown: {
            NSWindow* win = [event window];
            if (win) {
                NSPoint loc = [event locationInWindow];
                NSRect bounds = [win.contentView bounds];
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::MouseDown;
                e.x = (float)loc.x;
                e.y = (float)(bounds.size.height - loc.y);
                e.button = (type == NSEventTypeLeftMouseDown) ? 0 :
                           (type == NSEventTypeRightMouseDown) ? 1 : 2;
                e.modifiers = mods;
                dispatchEvent(e);
            }
            break;
        }
        case NSEventTypeLeftMouseUp:
        case NSEventTypeRightMouseUp:
        case NSEventTypeOtherMouseUp: {
            NSWindow* win = [event window];
            if (win) {
                NSPoint loc = [event locationInWindow];
                NSRect bounds = [win.contentView bounds];
                PlatformInputEvent e{};
                e.type = PlatformInputEvent::MouseUp;
                e.x = (float)loc.x;
                e.y = (float)(bounds.size.height - loc.y);
                e.button = (type == NSEventTypeLeftMouseUp) ? 0 :
                           (type == NSEventTypeRightMouseUp) ? 1 : 2;
                e.modifiers = mods;
                dispatchEvent(e);
            }
            break;
        }
        case NSEventTypeScrollWheel: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::Scroll;
            e.x = (float)[event scrollingDeltaX];
            e.y = (float)[event scrollingDeltaY];
            // Normalize for non-pixel-precise scroll (mouse wheel)
            if (![event hasPreciseScrollingDeltas]) {
                e.y = (e.y > 0) ? 1.0f : (e.y < 0) ? -1.0f : 0.0f;
                e.x = (e.x > 0) ? 1.0f : (e.x < 0) ? -1.0f : 0.0f;
            } else {
                e.y /= 10.0f; // Scale down trackpad scroll
            }
            dispatchEvent(e);
            break;
        }
        case NSEventTypeKeyDown: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::KeyDown;
            e.button = cocoaKeyToVK([event keyCode]);
            e.modifiers = mods;
            dispatchEvent(e);

            // Text input
            NSString* chars = [event characters];
            if (chars && [chars length] > 0) {
                unichar ch = [chars characterAtIndex:0];
                if (ch >= 32 && ch != 127) {
                    const char* utf8 = [chars UTF8String];
                    if (utf8) {
                        PlatformInputEvent te{};
                        te.type = PlatformInputEvent::TextInput;
                        std::strncpy(te.text, utf8, 31);
                        te.text[31] = '\0';
                        dispatchEvent(te);
                    }
                }
            }
            break;
        }
        case NSEventTypeKeyUp: {
            PlatformInputEvent e{};
            e.type = PlatformInputEvent::KeyUp;
            e.button = cocoaKeyToVK([event keyCode]);
            e.modifiers = mods;
            dispatchEvent(e);
            break;
        }
        default:
            break;
        }

        [NSApp sendEvent:event];
    }
}

void Platform::setClipboardText(const char* text) {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

std::string Platform::getClipboardText() {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
    if (string) {
        return std::string([string UTF8String]);
    }
    return "";
}

void Platform::openSystemURL(const std::string& url) {
    if (url.empty()) return;
    @autoreleasepool {
        NSString* nsUrlStr = [NSString stringWithUTF8String:url.c_str()];
        NSURL* nsUrl = [NSURL URLWithString:nsUrlStr];
        [[NSWorkspace sharedWorkspace] openURL:nsUrl];
    }
}


NativeCursorHandle Platform::createSystemCursor(CursorType type) {
    NSCursor* cursor = [NSCursor arrowCursor];
    switch (type) {
        case CursorType::Pointer: cursor = [NSCursor pointingHandCursor]; break;
        case CursorType::Text: cursor = [NSCursor IBeamCursor]; break;
        case CursorType::ResizeNWSE: cursor = [NSCursor arrowCursor]; break;
        default: break;
    }
    return (NativeCursorHandle)cursor;
}

void Platform::setCursor(NativeCursorHandle cursor) {
    if (cursor) {
        [(NSCursor*)cursor set];
    }
}

void Platform::getWindowSize(NativeWindowHandle window, int& w, int& h) {
    if (window) {
        NSWindow* nsWindow = (NSWindow*)window;
        NSRect rect = [nsWindow.contentView bounds];
        NSRect backing = [nsWindow.contentView convertRectToBacking:rect];
        w = (int)backing.size.width;
        h = (int)backing.size.height;
    }
}

void* Platform::loadVulkanLibrary() {
    void* handle = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!handle) handle = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!handle) handle = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
    return handle;
}

void Platform::unloadVulkanLibrary(void* library) {
    if (library) dlclose(library);
}

void* Platform::getVulkanProc(void* library, const char* name) {
    if (library) return dlsym(library, name);
    return nullptr;
}

std::vector<const char*> Platform::getVulkanInstanceExtensions() {
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME};
}

bool Platform::createVulkanSurface(VkInstance instance, NativeWindowHandle window, VkSurfaceKHR* surface) {
    if (!window) return false;
    NSWindow* nsWindow = (NSWindow*)window;
    CAMetalLayer* metalLayer = (CAMetalLayer*)nsWindow.contentView.layer;
    
    VkMetalSurfaceCreateInfoEXT surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    surfaceInfo.pLayer = metalLayer;

    auto func = (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT");
    if (!func) return false;
    
    return func(instance, &surfaceInfo, nullptr, surface) == VK_SUCCESS;
}

} // namespace FluxUI
#endif
