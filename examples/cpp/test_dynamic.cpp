#include <windows.h>
#include <iostream>
#include <iomanip>

typedef void* (*fluxui_app_create_t)();
typedef void (*fluxui_app_destroy_t)(void*);
typedef int (*fluxui_app_init_t)(void*, const char*, int, int);
typedef void (*fluxui_app_run_t)(void*);
typedef void (*fluxui_app_stop_t)(void*);
typedef void* (*fluxui_app_root_t)(void*);
typedef void* (*fluxui_widget_add_element_t)(void*, const char*, const char*, const char*);
typedef void (*fluxui_style_width_px_t)(void*, float);
typedef void (*fluxui_style_height_px_t)(void*, float);
typedef int (*fluxui_app_set_backend_t)(void*, int);

HMODULE hDll = NULL;

LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    std::cerr << "\n========== CRASH REPORT ==========\n";
    std::cerr << "Exception Code: 0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << "\n";
    std::cerr << "Faulting Address: 0x" << std::hex << (DWORD64)ExceptionInfo->ExceptionRecord->ExceptionAddress << std::dec << "\n";
    std::cerr << "DLL Base Address: 0x" << std::hex << (DWORD64)hDll << std::dec << "\n";
    std::cerr << "Relative Offset: 0x" << std::hex << ((DWORD64)ExceptionInfo->ExceptionRecord->ExceptionAddress - (DWORD64)hDll) << std::dec << "\n";
    std::cerr << "==================================\n\n";

    ExitProcess(1);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main() {
    AddVectoredExceptionHandler(1, VectoredExceptionHandler);

    std::cout << "Loading fluxui_shared.dll..." << std::endl;
    hDll = LoadLibraryA("build/Release/fluxui_shared.dll");
    if (!hDll) {
        std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
        return 1;
    }

    auto create = (fluxui_app_create_t)GetProcAddress(hDll, "fluxui_app_create");
    auto destroy = (fluxui_app_destroy_t)GetProcAddress(hDll, "fluxui_app_destroy");
    auto init = (fluxui_app_init_t)GetProcAddress(hDll, "fluxui_app_init");
    auto run = (fluxui_app_run_t)GetProcAddress(hDll, "fluxui_app_run");
    auto set_backend = (fluxui_app_set_backend_t)GetProcAddress(hDll, "fluxui_app_set_backend");
    auto root_fn = (fluxui_app_root_t)GetProcAddress(hDll, "fluxui_app_root");
    auto add_element = (fluxui_widget_add_element_t)GetProcAddress(hDll, "fluxui_widget_add_element");
    auto width_px = (fluxui_style_width_px_t)GetProcAddress(hDll, "fluxui_style_width_px");
    auto height_px = (fluxui_style_height_px_t)GetProcAddress(hDll, "fluxui_style_height_px");

    if (!create || !destroy || !init || !run || !set_backend) {
        std::cerr << "Failed to get proc addresses" << std::endl;
        return 1;
    }

    std::cout << "Creating app..." << std::endl;
    void* app = create();
    if (!app) {
        std::cerr << "Failed to create app" << std::endl;
        return 1;
    }

    std::cout << "Setting backend to Compatibility..." << std::endl;
    set_backend(app, 100);

    std::cout << "Initializing app..." << std::endl;
    if (!init(app, "FluxUI Dynamic C++ Test", 800, 600)) {
        std::cerr << "Failed to init app" << std::endl;
        return 1;
    }

    std::cout << "Adding widgets..." << std::endl;
    void* root = root_fn(app);
    if (root) {
        // Use HTML element names via the C ABI add_element factory.
        void* container = add_element(root, "div", "", "container");
        if (container) {
            width_px(container, 600);
            height_px(container, 400);
            add_element(container, "h1", "Hello from Dynamic C++!", "");
        }
    }

    std::cout << "Running app..." << std::endl;
    run(app);

    std::cout << "Destroying app..." << std::endl;
    destroy(app);
    return 0;
}
