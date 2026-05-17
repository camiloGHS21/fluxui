// FluxUI - Android Platform Layer
// This file is compiled when ANDROID is defined in CMake.

#include <iostream>

// TODO: Implement android_native_app_glue, ANativeWindow creation, and touch event handling.
// For Vulkan on Android, retrieve the ANativeWindow and create a VkSurfaceKHR.

#if defined(__ANDROID__)

// Placeholder to prevent compilation errors when the platform layer is empty.
void fluxui_android_stub() {
    std::cerr << "Android platform layer is not fully implemented yet." << std::endl;
}

#endif
