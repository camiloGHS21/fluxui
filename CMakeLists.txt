cmake_minimum_required(VERSION 3.20)

# ============================================================
#  vcpkg integration
# ============================================================
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vendor/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(DataLeakGuard VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
#  Find packages
# ============================================================
find_package(SDL2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

option(FLUXUI_ENABLE_MSAA "Enable 4x multisample antialiasing for smoother UI edges" ON)
option(FLUXUI_ENABLE_VSYNC "Enable swap interval vsync by default" ON)
option(FLUXUI_ENABLE_VULKAN "Compile optional Vulkan backend metadata/probing support" ON)
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
set(FLUXUI_DEFAULT_BACKEND "VULKAN" CACHE STRING "Default FluxUI backend preference: AUTO, VULKAN, D3D12, DIRECT12, DIRECTX12, DIRECT3D12, or METAL")
set_property(CACHE FLUXUI_DEFAULT_BACKEND PROPERTY STRINGS AUTO VULKAN D3D12 DIRECT12 DIRECTX12 DIRECT3D12 METAL)

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
else()
    message(FATAL_ERROR "FLUXUI_DEFAULT_BACKEND must be AUTO, VULKAN, D3D12, DIRECT12, DIRECTX12, DIRECT3D12, or METAL")
endif()

set(FLUXUI_VULKAN_AVAILABLE OFF)
set(FLUXUI_VULKAN_SDK_AVAILABLE OFF)
if(FLUXUI_ENABLE_VULKAN)
    set(FLUXUI_VULKAN_AVAILABLE ON)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        set(FLUXUI_VULKAN_SDK_AVAILABLE ON)
        find_package(glslang CONFIG QUIET)
        if(NOT glslang_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/glslang/glslang-config.cmake")
            set(glslang_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/glslang"
                CACHE PATH "glslang package directory" FORCE)
            find_package(glslang CONFIG QUIET)
        endif()
        if(NOT glslang_FOUND)
            message(FATAL_ERROR "FLUXUI_ENABLE_VULKAN requires glslang for Vulkan shader compilation. Run vendor/vcpkg/vcpkg.exe install --triplet ${VCPKG_TARGET_TRIPLET} or install glslang in the active vcpkg tree.")
        endif()
    else()
        message(STATUS "FLUXUI_ENABLE_VULKAN is ON, but Vulkan SDK was not found. Vulkan runtime probing will use SDL dynamic loading.")
    endif()
endif()

set(FLUXUI_D3D12_AVAILABLE OFF)
if(FLUXUI_ENABLE_D3D12 AND WIN32)
    set(FLUXUI_D3D12_AVAILABLE ON)
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

# ============================================================
#  FluxUI Framework Library
# ============================================================
set(FLUXUI_SOURCES
    fluxui/src/css_parser.cpp
    fluxui/src/renderer.cpp
    fluxui/src/widgets.cpp
    fluxui/src/fluxui_c.cpp
)

add_library(fluxui STATIC
    ${FLUXUI_SOURCES}
)

function(configure_fluxui_target target_name)
    target_include_directories(${target_name} PUBLIC
        fluxui/include
        vendor/stb
        vendor/glad/include
    )

    target_link_libraries(${target_name} PUBLIC
        glad
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    )

    if(TARGET OpenGL::GL)
        target_link_libraries(${target_name} PUBLIC OpenGL::GL)
    elseif(WIN32)
        target_link_libraries(${target_name} PUBLIC opengl32)
    else()
        target_link_libraries(${target_name} PUBLIC GL)
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

    if(FLUXUI_VULKAN_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN=0)
    endif()

    if(FLUXUI_VULKAN_SDK_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN_SDK=1)
        target_link_libraries(${target_name} PUBLIC
            Vulkan::Vulkan
            glslang::glslang
            glslang::glslang-default-resource-limits
            glslang::SPIRV
            glslang::MachineIndependent
            glslang::GenericCodeGen
        )
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_VULKAN_SDK=0)
    endif()

    if(FLUXUI_D3D12_AVAILABLE)
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_D3D12=1)
    else()
        target_compile_definitions(${target_name} PUBLIC FLUXUI_HAS_D3D12=0)
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
target_compile_definitions(fluxui PUBLIC FLUXUI_STATIC)

option(FLUXUI_BUILD_SHARED "Build FluxUI shared library for C/Rust/Zig FFI" ON)
if(FLUXUI_BUILD_SHARED)
    add_library(fluxui_shared SHARED
        ${FLUXUI_SOURCES}
    )
    configure_fluxui_target(fluxui_shared)
    target_compile_definitions(fluxui_shared PRIVATE FLUXUI_SHARED FLUXUI_BUILDING_DLL)
endif()

# ============================================================
#  DataLeak Guard Application
# ============================================================
add_executable(DataLeakGuard
    src/main.cpp
)

target_link_libraries(DataLeakGuard PRIVATE
    fluxui
    nlohmann_json::nlohmann_json
)

# Copy assets to build directory
add_custom_command(TARGET DataLeakGuard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:DataLeakGuard>/assets"
)

# Copy SDL2 DLL on Windows
if(WIN32)
    add_custom_command(TARGET DataLeakGuard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:DataLeakGuard>
    )
endif()
