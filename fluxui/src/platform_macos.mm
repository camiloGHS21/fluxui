// FluxUI - macOS / iOS Platform Layer (Cocoa / Metal)
// This file is compiled when APPLE is defined in CMake.

#include <iostream>

// TODO: Implement Cocoa NSWindow creation, event loop, and CAMetalLayer integration.
// For Vulkan on macOS, use MoltenVK to render to the CAMetalLayer.

#if defined(__APPLE__)

// Placeholder to prevent compilation errors when the platform layer is empty.
void fluxui_macos_stub() {
    std::cerr << "macOS platform layer is not fully implemented yet." << std::endl;
}

#endif
