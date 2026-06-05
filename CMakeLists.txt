cmake_minimum_required(VERSION 3.20)

# ============================================================
#  vcpkg integration
# ============================================================
option(DATALEAKGUARD_SINGLE_BINARY "Prefer static dependencies and build only the application executable" ON)

if(DATALEAKGUARD_SINGLE_BINARY AND WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg target triplet")
endif()

if(DATALEAKGUARD_SINGLE_BINARY AND WIN32)
    set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "Do not copy vcpkg DLLs next to the executable")
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vendor/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(FluxUI VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckIPOSupported)

option(DATALEAKGUARD_ENABLE_LTO "Enable release link-time optimization for faster startup/runtime" ON)
option(DATALEAKGUARD_AGGRESSIVE_OPTIMIZATIONS "Enable aggressive optimizations for maximum runtime speed" ON)

if(DATALEAKGUARD_ENABLE_LTO)
    check_ipo_supported(RESULT DATALEAKGUARD_IPO_SUPPORTED
                        OUTPUT DATALEAKGUARD_IPO_ERROR)
    if(NOT DATALEAKGUARD_IPO_SUPPORTED)
        message(WARNING "IPO/LTO is not supported by this toolchain: ${DATALEAKGUARD_IPO_ERROR}")
    endif()
endif()

function(enable_release_lto target_name)
    if(DATALEAKGUARD_ENABLE_LTO AND DATALEAKGUARD_IPO_SUPPORTED)
        set_property(TARGET ${target_name} PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()

    if(DATALEAKGUARD_AGGRESSIVE_OPTIMIZATIONS)
        if(MSVC)
            target_compile_options(${target_name} PUBLIC
                $<$<CONFIG:Release>:/O2 /Oi /Ot /GL /Gw /Gy /Gs /Zi>
                $<$<CONFIG:RelWithDebInfo>:/O2 /Oi /Ot /GL /Gw /Gy /Gs /Zi>
            )
            target_link_options(${target_name} PUBLIC
                $<$<CONFIG:Release>:/OPT:REF /OPT:ICF /LTCG /DEBUG>
                $<$<CONFIG:RelWithDebInfo>:/OPT:REF /OPT:ICF /LTCG /DEBUG>
            )
        else()
            target_compile_options(${target_name} PUBLIC
                $<$<CONFIG:Release>:-O3 -flto -ffunction-sections -fdata-sections>
                $<$<CONFIG:RelWithDebInfo>:-O3 -flto -ffunction-sections -fdata-sections>
            )
            target_link_options(${target_name} PUBLIC
                $<$<CONFIG:Release>:-flto -Wl,--gc-sections>
                $<$<CONFIG:RelWithDebInfo>:-flto -Wl,--gc-sections>
            )
        endif()
    endif()
endfunction()

set(DATALEAKGUARD_EMBEDDED_THEME_HEADER "${CMAKE_BINARY_DIR}/generated/embedded_theme.h")
set(DATALEAKGUARD_EMBEDDED_FONT_ATLAS_HEADER "${CMAKE_BINARY_DIR}/generated/embedded_font_atlas.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/styles/theme.css"
)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(READ "${CMAKE_SOURCE_DIR}/assets/styles/theme.css" DATALEAKGUARD_THEME_HEX HEX)
string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," DATALEAKGUARD_THEME_BYTES "${DATALEAKGUARD_THEME_HEX}")
file(WRITE "${DATALEAKGUARD_EMBEDDED_THEME_HEADER}"
    "#pragma once\n\n"
    "static constexpr unsigned char DATALEAKGUARD_EMBEDDED_THEME_CSS_BYTES[] = {\n"
    "${DATALEAKGUARD_THEME_BYTES}0x00\n"
    "};\n\n"
    "inline const char* dataleakguardEmbeddedThemeCss() {\n"
    "    return reinterpret_cast<const char*>(DATALEAKGUARD_EMBEDDED_THEME_CSS_BYTES);\n"
    "}\n"
)
if(NOT EXISTS "${DATALEAKGUARD_EMBEDDED_FONT_ATLAS_HEADER}")
    file(WRITE "${DATALEAKGUARD_EMBEDDED_FONT_ATLAS_HEADER}"
        "#pragma once\n\n"
        "#include \"fluxui/renderer.h\"\n\n"
        "namespace DataLeakGuardEmbeddedFonts {\n"
        "inline bool loadEmbeddedUiFontAtlases(FluxUI::Renderer&) { return false; }\n"
        "}\n"
    )
endif()

if(DATALEAKGUARD_SINGLE_BINARY AND WIN32 AND
   DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET MATCHES "static")
    message(WARNING "DATALEAKGUARD_SINGLE_BINARY is ON, but VCPKG_TARGET_TRIPLET='${VCPKG_TARGET_TRIPLET}' is not a static triplet. Configure with -DVCPKG_TARGET_TRIPLET=x64-windows-static for a Vulkan loader linked into the executable.")
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ============================================================
#  Find packages
# ============================================================
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)
find_package(harfbuzz CONFIG REQUIRED)

option(DATALEAKGUARD_PREBAKE_UI_FONTS "Bake UI font atlases at build time for faster startup and stable text quality" ON)
option(DATALEAKGUARD_EMBED_FONT_GENERATOR "Embed font atlas generator into build process (required for prebake)" ON)

if(DATALEAKGUARD_PREBAKE_UI_FONTS AND DATALEAKGUARD_EMBED_FONT_GENERATOR)
    add_executable(dataleakguard_font_atlas_generator
        tools/font_atlas_generator.cpp
    )
    target_link_libraries(dataleakguard_font_atlas_generator PRIVATE Freetype::Freetype harfbuzz::harfbuzz)
    target_compile_features(dataleakguard_font_atlas_generator PRIVATE cxx_std_17)
    enable_release_lto(dataleakguard_font_atlas_generator)

    add_custom_target(dataleakguard_prebaked_fonts
        COMMAND "$<TARGET_FILE:dataleakguard_font_atlas_generator>"
                "${DATALEAKGUARD_EMBEDDED_FONT_ATLAS_HEADER}"
        BYPRODUCTS "${DATALEAKGUARD_EMBEDDED_FONT_ATLAS_HEADER}"
        DEPENDS dataleakguard_font_atlas_generator
        COMMENT "Prebaking DataLeakGuard UI font atlases"
        VERBATIM
    )
endif()

option(FLUXUI_ENABLE_MSAA "Enable 4x multisample antialiasing for smoother UI edges" ON)
option(FLUXUI_ENABLE_VSYNC "Enable swap interval vsync by default" ON)
option(FLUXUI_ENABLE_VULKAN "Compile optional Vulkan backend metadata/probing support" ON)
option(FLUXUI_LOW_MEMORY "Favor lower RAM/VRAM usage over extra buffering" OFF)
if(WIN32)
    set(FLUXUI_ENABLE_D3D12_DEFAULT ON)
else()
    set(FLUXUI_ENABLE_D3D12_DEFAULT OFF)
endif()
if(APPLE)
    set(FLUXUI_ENABLE_METAL_DEFAULT ON)
else()
    set(FLUXUI_ENABLE_METAL_DEFAULT OFF)
endif()
option(FLUXUI_ENABLE_D3D12 "Compile optional Direct3D 12 backend metadata/probing support" ${FLUXUI_ENABLE_D3D12_DEFAULT})
option(FLUXUI_ENABLE_METAL "Compile optional Metal backend metadata/probing support" ${FLUXUI_ENABLE_METAL_DEFAULT})
option(FLUXUI_ENABLE_SKIA "Compile optional Skia backend metadata/probing support" ON)
set(FLUXUI_DEFAULT_BACKEND "VULKAN" CACHE STRING "Default FluxUI backend preference: AUTO, VULKAN, D3D12, DIRECT12, DIRECTX12, DIRECT3D12, METAL, or SKIA")
set_property(CACHE FLUXUI_DEFAULT_BACKEND PROPERTY STRINGS AUTO VULKAN D3D12 DIRECT12 DIRECTX12 DIRECT3D12 METAL SKIA)

string(TOUPPER "${FLUXUI_DEFAULT_BACKEND}" FLUXUI_DEFAULT_BACKEND_UPPER)
if(FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "AUTO")
    set(FLUXUI_DEFAULT_BACKEND_VALUE 0)
elseif(FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "VULKAN")
    set(FLUXUI_DEFAULT_BACKEND_VALUE 1)
elseif(FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "D3D12" OR
       FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "DIRECT12" OR
       FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "DIRECTX12" OR
       FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "DIRECT3D12")
    set(FLUXUI_DEFAULT_BACKEND_VALUE 2)
elseif(FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "METAL")
    set(FLUXUI_DEFAULT_BACKEND_VALUE 3)
elseif(FLUXUI_DEFAULT_BACKEND_UPPER STREQUAL "SKIA")
    set(FLUXUI_DEFAULT_BACKEND_VALUE 4)
else()
    message(FATAL_ERROR "FLUXUI_DEFAULT_BACKEND must be AUTO, VULKAN, D3D12, DIRECT12, DIRECTX12, DIRECT3D12, METAL, or SKIA")
endif()

set(FLUXUI_VULKAN_AVAILABLE OFF)
set(FLUXUI_VULKAN_SDK_AVAILABLE OFF)
if(FLUXUI_ENABLE_VULKAN)
    set(FLUXUI_VULKAN_AVAILABLE ON)
    find_package(Vulkan REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    set(FLUXUI_VULKAN_SDK_AVAILABLE ON)
endif()

set(FLUXUI_D3D12_AVAILABLE OFF)
if(FLUXUI_ENABLE_D3D12 AND WIN32)
    set(FLUXUI_D3D12_AVAILABLE ON)
endif()

set(FLUXUI_SKIA_AVAILABLE OFF)
if(FLUXUI_ENABLE_SKIA)
    set(FLUXUI_SKIA_AVAILABLE ON)
endif()

set(FLUXUI_METAL_AVAILABLE OFF)
if(FLUXUI_ENABLE_METAL AND APPLE)
    set(FLUXUI_METAL_AVAILABLE ON)
endif()

# ============================================================
#  GLAD (legacy compatibility renderer loader)
# ============================================================
add_library(glad STATIC
    vendor/glad/src/gl.c
)
target_include_directories(glad PUBLIC vendor/glad/include)
set_target_properties(glad PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
enable_release_lto(glad)

# ============================================================
#  FluxUI Framework Library
# ============================================================
set(FLUXUI_SOURCES
    # core: application loop, widgets, GC, a11y, compositor
    fluxui/src/core/application.cpp
    fluxui/src/core/widget_style.cpp
    fluxui/src/core/widget_cascade.cpp
    fluxui/src/core/widgets/video.cpp
    fluxui/src/core/widgets/svg.cpp
    fluxui/src/core/widgets/controls.cpp
    fluxui/src/core/widgets/text.cpp
    fluxui/src/core/widgets/textarea.cpp
    fluxui/src/core/widgets/media.cpp
    fluxui/src/core/widgets/interactive.cpp
    fluxui/src/core/gc.cpp
    fluxui/src/core/accessibility.cpp
    fluxui/src/core/compositor.cpp
    # style: CSS parsing + cascade
    fluxui/src/style/css_parser.cpp
    fluxui/src/style/css_color.cpp
    # layout: Blink-style layout solver + objects
    fluxui/src/layout/layout.cpp
    fluxui/src/layout/layout_object.cpp
    # render: GPU/software renderer + image resources
    fluxui/src/render/renderer.cpp
    fluxui/src/render/software_raster.cpp
    fluxui/src/render/image_resource.cpp
    # ffi: C ABI for language bindings
    fluxui/src/ffi/fluxui_c.cpp
)

if(WIN32)
    list(APPEND FLUXUI_SOURCES fluxui/src/platform/platform_win32.cpp)
elseif(APPLE)
    list(APPEND FLUXUI_SOURCES fluxui/src/platform/platform_macos.mm)
elseif(ANDROID)
    list(APPEND FLUXUI_SOURCES fluxui/src/platform/platform_android.cpp)
else()
    list(APPEND FLUXUI_SOURCES fluxui/src/platform/platform_linux.cpp)
endif()

add_library(fluxui STATIC
    ${FLUXUI_SOURCES}
)
set_target_properties(fluxui PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

function(configure_fluxui_target target_name)
    target_include_directories(${target_name} PUBLIC
        fluxui/include
        vendor/stb
        vendor/glad/include
        "${CMAKE_SOURCE_DIR}"
    )

    # Include optimization configurations
    target_compile_definitions(${target_name} PUBLIC
        FLUXUI_FAST_STARTUP=1
        FLUXUI_SHOW_WINDOW_ON_INIT=0
        FLUXUI_REVEAL_WINDOW_ON_FIRST_FRAME=1
        FLUXUI_PREALLOC_WIDGETS=512
        FLUXUI_PREALLOC_TEXT_NODES=1024
        FLUXUI_PREALLOC_PANELS=256
        FLUXUI_PREALLOC_ROOT_CHILDREN=16
        FLUXUI_PREALLOC_ROUTES=24
        FLUXUI_PREALLOC_EVENT_LISTENERS=32
        FLUXUI_PREALLOC_FONTS=32
        FLUXUI_PREALLOC_IMAGES=96
        FLUXUI_PREALLOC_SCISSORS=16
        FLUXUI_PREALLOC_TRANSFORMS=16
        FLUXUI_PREALLOC_RECT_BATCH=512
        FLUXUI_PREALLOC_TEXT_FLOATS=16384
        FLUXUI_IMMEDIATE_RENDER=1
        FLUXUI_STYLE_CACHE_SIZE=4096
        FLUXUI_TEXT_MEASURE_CACHE_SIZE=8192
        FLUXUI_OPTIMIZE_LAYOUT=1
        FLUXUI_MINIMAL_ALLOCS=1
        FLUXUI_GPU_OPTIMIZE=1
        FLUXUI_TARGET_FPS=120
    )

    target_compile_definitions(${target_name} PUBLIC
        $<$<NOT:$<CONFIG:Debug>>:FLUXUI_SILENT_STARTUP=1>
    )

    target_link_libraries(${target_name} PUBLIC
        glad
        Freetype::Freetype
        harfbuzz::harfbuzz
    )

    if(WIN32)
        target_compile_definitions(${target_name} PUBLIC
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    endif()

    if(TARGET OpenGL::GL)
        target_link_libraries(${target_name} PUBLIC OpenGL::GL)
    elseif(WIN32)
        target_link_libraries(${target_name} PUBLIC opengl32)
    else()
        target_link_libraries(${target_name} PUBLIC GL X11)
    endif()

    if(FLUXUI_ENABLE_MSAA)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_ENABLE_MSAA=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_ENABLE_MSAA=0)
    endif()

    if(FLUXUI_ENABLE_VSYNC)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_ENABLE_VSYNC=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_ENABLE_VSYNC=0)
    endif()

    if(FLUXUI_LOW_MEMORY)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_LOW_MEMORY=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_LOW_MEMORY=0)
    endif()

    if(FLUXUI_VULKAN_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN=0)
    endif()

    if(FLUXUI_VULKAN_SDK_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN_SDK=1)
        target_link_libraries(${target_name} PUBLIC
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
        )
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN_SDK=0)
    endif()

    if(FLUXUI_D3D12_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_D3D12=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_D3D12=0)
    endif()

    if(FLUXUI_SKIA_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_SKIA=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_SKIA=0)
    endif()

    if(FLUXUI_METAL_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_METAL=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_METAL=0)
    endif()

    target_compile_definitions(${target_name} PUBLIC
        FLUXUI_DEFAULT_BACKEND=${FLUXUI_DEFAULT_BACKEND_VALUE}
    )
endfunction()

configure_fluxui_target(fluxui)
target_compile_definitions(fluxui PUBLIC FLUXUI_STATIC FLUXUI_INTERNAL)
enable_release_lto(fluxui)

option(FLUXUI_BUILD_SHARED "Build FluxUI shared library for C/Rust/Zig/Java FFI" ON)
option(FLUXUI_BUILD_JAVA "Build FluxUI Java JNI bindings" ON)
if(FLUXUI_BUILD_SHARED)
    add_library(fluxui_shared SHARED
        ${FLUXUI_SOURCES}
    )
    configure_fluxui_target(fluxui_shared)
    target_compile_definitions(fluxui_shared PRIVATE FLUXUI_SHARED FLUXUI_BUILDING_DLL FLUXUI_INTERNAL)
    enable_release_lto(fluxui_shared)

    if(FLUXUI_BUILD_JAVA)
        find_package(JNI QUIET)
        if(JNI_FOUND)
            add_library(fluxui_java SHARED
                bindings/java/native/fluxui_jni.cpp
            )
            target_include_directories(fluxui_java PRIVATE
                "${CMAKE_SOURCE_DIR}/fluxui/include"
                ${JNI_INCLUDE_DIRS}
            )
            target_link_libraries(fluxui_java PRIVATE
                fluxui_shared
                ${JNI_LIBRARIES}
            )
            target_compile_definitions(fluxui_java PRIVATE FLUXUI_SHARED)
            enable_release_lto(fluxui_java)
        else()
            message(STATUS "JNI was not found; skipping FluxUI Java native binding.")
        endif()
    endif()
endif()

# ============================================================
#  DataLeak Guard Showcase Example
# ============================================================
include(cmake/AutoRoutes.cmake)

add_executable(dataleakguard
    examples/dataleakguard/main.cpp
)

fluxui_auto_routes(
    TARGET      dataleakguard
    VIEWS_DIR   "${CMAKE_SOURCE_DIR}/examples/dataleakguard/views"
    OUTPUT      "${CMAKE_BINARY_DIR}/generated/dlg_routes_gen.h"
    NAMESPACE   dlg
)

target_link_libraries(dataleakguard PRIVATE
    fluxui
    nlohmann_json::nlohmann_json
)

target_include_directories(dataleakguard PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
    "${CMAKE_SOURCE_DIR}/examples/dataleakguard"
)
if(DATALEAKGUARD_PREBAKE_UI_FONTS)
    add_dependencies(dataleakguard dataleakguard_prebaked_fonts)
endif()
enable_release_lto(dataleakguard)

# ============================================================
#  Minimal C++ Example
# ============================================================
add_executable(minimal_example
    examples/cpp/minimal.cpp
)

target_link_libraries(minimal_example PRIVATE
    fluxui
)

target_include_directories(minimal_example PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(minimal_example)

# ============================================================
#  Temporary Symbol Resolver
# ============================================================
add_executable(symbol_resolver
    examples/symbol_resolver.cpp
)
target_link_libraries(symbol_resolver PRIVATE dbghelp)

add_executable(test_dynamic
    examples/cpp/test_dynamic.cpp
)

add_executable(svg_test
    examples/cpp/svg_test.cpp
)
target_link_libraries(svg_test PRIVATE
    fluxui
)
target_include_directories(svg_test PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(svg_test)

add_executable(table_test
    examples/cpp/table_test.cpp
)
target_link_libraries(table_test PRIVATE
    fluxui
)
target_include_directories(table_test PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(table_test)

add_executable(test_math
    examples/cpp/test_math.cpp
)
target_link_libraries(test_math PRIVATE
    fluxui
)
target_include_directories(test_math PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_math)

add_executable(test_logical
    examples/cpp/test_logical.cpp
)
target_link_libraries(test_logical PRIVATE
    fluxui
)
target_include_directories(test_logical PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_logical)

add_executable(test_video
    examples/cpp/test_video.cpp
)
target_link_libraries(test_video PRIVATE
    fluxui
)
target_include_directories(test_video PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_video)

add_executable(test_svg_parity
    examples/cpp/test_svg_parity.cpp
)
target_link_libraries(test_svg_parity PRIVATE
    fluxui
)
target_include_directories(test_svg_parity PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_svg_parity)

add_executable(test_selectors
    examples/cpp/test_selectors.cpp
)
target_link_libraries(test_selectors PRIVATE
    fluxui
)
target_include_directories(test_selectors PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_selectors)

add_executable(test_animations
    examples/cpp/test_animations.cpp
)
target_link_libraries(test_animations PRIVATE
    fluxui
)
target_include_directories(test_animations PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_animations)

add_executable(test_transforms
    examples/cpp/test_transforms.cpp
)
target_link_libraries(test_transforms PRIVATE
    fluxui
)
target_include_directories(test_transforms PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_transforms)

add_executable(test_filter
    examples/cpp/test_filter.cpp
)
target_link_libraries(test_filter PRIVATE
    fluxui
)
target_include_directories(test_filter PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_filter)

add_executable(test_color
    examples/cpp/test_color.cpp
)
target_link_libraries(test_color PRIVATE
    fluxui
)
target_include_directories(test_color PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_color)

add_executable(test_pseudo
    examples/cpp/test_pseudo.cpp
)
target_link_libraries(test_pseudo PRIVATE
    fluxui
)
target_include_directories(test_pseudo PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_pseudo)

add_executable(test_structural
    examples/cpp/test_structural.cpp
)
target_link_libraries(test_structural PRIVATE
    fluxui
)
target_include_directories(test_structural PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_structural)

add_executable(test_at_rules
    examples/cpp/test_at_rules.cpp
)
target_link_libraries(test_at_rules PRIVATE
    fluxui
)
target_include_directories(test_at_rules PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_at_rules)

add_executable(test_nesting
    examples/cpp/test_nesting.cpp
)
target_link_libraries(test_nesting PRIVATE
    fluxui
)
target_include_directories(test_nesting PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_nesting)

add_executable(test_var_cycle
    examples/cpp/test_var_cycle.cpp
)
target_link_libraries(test_var_cycle PRIVATE
    fluxui
)
target_include_directories(test_var_cycle PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_var_cycle)

add_executable(test_cascade
    examples/cpp/test_cascade.cpp
)
target_link_libraries(test_cascade PRIVATE
    fluxui
)
target_include_directories(test_cascade PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_cascade)

add_executable(test_masking
    examples/cpp/test_masking.cpp
)
target_link_libraries(test_masking PRIVATE
    fluxui
)
target_include_directories(test_masking PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_masking)

add_executable(test_scroll
    examples/cpp/test_scroll.cpp
)
target_link_libraries(test_scroll PRIVATE
    fluxui
)
target_include_directories(test_scroll PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_scroll)

add_executable(test_scroll_timeline
    examples/cpp/test_scroll_timeline.cpp
)
target_link_libraries(test_scroll_timeline PRIVATE
    fluxui
)
target_include_directories(test_scroll_timeline PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_scroll_timeline)

add_executable(test_typography
    examples/cpp/test_typography.cpp
)
target_link_libraries(test_typography PRIVATE
    fluxui
)
target_include_directories(test_typography PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_typography)

add_executable(test_ui_misc
    examples/cpp/test_ui_misc.cpp
)
target_link_libraries(test_ui_misc PRIVATE
    fluxui
)
target_include_directories(test_ui_misc PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_ui_misc)

add_executable(benchmark
    examples/cpp/benchmark.cpp
)
target_link_libraries(benchmark PRIVATE
    fluxui
)
target_include_directories(benchmark PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(benchmark)

add_executable(benchmark_tree
    examples/cpp/benchmark_tree.cpp
)
target_link_libraries(benchmark_tree PRIVATE
    fluxui
)
target_include_directories(benchmark_tree PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(benchmark_tree)

add_executable(benchmark_frame
    examples/cpp/benchmark_frame.cpp
)
target_link_libraries(benchmark_frame PRIVATE
    fluxui
)
target_include_directories(benchmark_frame PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(benchmark_frame)

add_executable(demo_gpu_modes
    examples/cpp/demo_gpu_modes.cpp
)
target_link_libraries(demo_gpu_modes PRIVATE
    fluxui
)
target_include_directories(demo_gpu_modes PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(demo_gpu_modes)

add_executable(demo_dsl
    examples/cpp/demo_dsl.cpp
)
target_link_libraries(demo_dsl PRIVATE
    fluxui
)
target_include_directories(demo_dsl PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(demo_dsl)

add_executable(demo_ecosystem
    examples/cpp/demo_ecosystem.cpp
)
target_link_libraries(demo_ecosystem PRIVATE
    fluxui
)
target_include_directories(demo_ecosystem PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(demo_ecosystem)

add_executable(demo_hotreload
    examples/cpp/demo_hotreload.cpp
)
target_link_libraries(demo_hotreload PRIVATE
    fluxui
)
target_include_directories(demo_hotreload PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(demo_hotreload)

add_executable(test_dsl
    examples/cpp/test_dsl.cpp
)
target_link_libraries(test_dsl PRIVATE
    fluxui
)
target_include_directories(test_dsl PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)
enable_release_lto(test_dsl)


