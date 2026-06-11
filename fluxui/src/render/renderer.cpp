// FluxUI Renderer - GPU-accelerated rendering
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "fluxui/property_trees.h"
#include "software_internal.h"
#include "renderer_svg.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__ANDROID__)
#include <android/native_window.h>
#endif

#ifndef FLUXUI_HAS_VULKAN_SDK
#define FLUXUI_HAS_VULKAN_SDK 0
#endif

#include <glad/gl.h>
#include "renderer_vulkan.h"   // VulkanRendererState + Vulkan entry points (impl in renderer_vulkan.cpp)
#include "fluxui/platform.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <utility>
#include <cstdio>
#include <array>
#include <limits>
#include <set>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <cctype>
#include <unordered_map>
#include <thread>
#include "fluxui/widgets.h"

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifndef FLUXUI_ENABLE_MSAA
#define FLUXUI_ENABLE_MSAA 1
#endif

#ifndef FLUXUI_ENABLE_VSYNC
#define FLUXUI_ENABLE_VSYNC 1
#endif

#ifndef FLUXUI_LOW_MEMORY
#define FLUXUI_LOW_MEMORY 1
#endif

#ifndef FLUXUI_HAS_VULKAN
#define FLUXUI_HAS_VULKAN 0
#endif

#ifndef FLUXUI_HAS_VULKAN_SDK
#define FLUXUI_HAS_VULKAN_SDK 0
#endif

#ifndef FLUXUI_HAS_D3D12
#define FLUXUI_HAS_D3D12 0
#endif

#ifndef FLUXUI_HAS_SKIA
#define FLUXUI_HAS_SKIA 0
#endif

#ifndef FLUXUI_HAS_METAL
#define FLUXUI_HAS_METAL 0
#endif

#ifndef FLUXUI_DEFAULT_BACKEND
#define FLUXUI_DEFAULT_BACKEND 1
#endif

#if defined(_MSC_VER) && _MSC_VER < 1945
extern "C" size_t __stdcall __std_find_first_not_of_trivial_pos_1(
    const void* haystack,
    size_t haystackLength,
    const void* needle,
    size_t needleLength) noexcept {
    const auto* h = static_cast<const unsigned char*>(haystack);
    const auto* n = static_cast<const unsigned char*>(needle);
    for (size_t i = 0; i < haystackLength; ++i) {
        bool found = false;
        for (size_t j = 0; j < needleLength; ++j) {
            if (h[i] == n[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return i;
        }
    }
    return static_cast<size_t>(-1);
}

extern "C" size_t __stdcall __std_find_last_not_of_trivial_pos_1(
    const void* haystack,
    size_t haystackLength,
    const void* needle,
    size_t needleLength) noexcept {
    const auto* h = static_cast<const unsigned char*>(haystack);
    const auto* n = static_cast<const unsigned char*>(needle);
    for (size_t i = haystackLength; i > 0; --i) {
        const size_t pos = i - 1;
        bool found = false;
        for (size_t j = 0; j < needleLength; ++j) {
            if (h[pos] == n[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return pos;
        }
    }
    return static_cast<size_t>(-1);
}
#endif

namespace FluxUI {


namespace {

#if !FLUXUI_HAS_VULKAN_SDK
using VkFlags = uint32_t;
using VkResult = int32_t;

#define FLUXUI_VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
FLUXUI_VK_DEFINE_HANDLE(VkPhysicalDevice)
#undef FLUXUI_VK_DEFINE_HANDLE

struct VkAllocationCallbacks;

enum VkStructureType {
    VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1
};

enum VkPhysicalDeviceType {
    VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
    VK_PHYSICAL_DEVICE_TYPE_CPU = 4
};

struct VkApplicationInfo {
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

#ifdef _WIN32
struct VkWin32SurfaceCreateInfoKHR {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    HINSTANCE hinstance;
    HWND hwnd;
};
#endif

struct alignas(8) VkPhysicalDevicePropertiesLite {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    VkPhysicalDeviceType deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
    uint8_t reserved[4096];
};

using PFN_vkVoidFunction = void (*)();
using PFN_vkGetInstanceProcAddr = PFN_vkVoidFunction (*)(VkInstance instance, const char* pName);
using PFN_vkCreateInstance = VkResult (*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
using PFN_vkDestroyInstance = void (*)(VkInstance, const VkAllocationCallbacks*);
using PFN_vkEnumeratePhysicalDevices = VkResult (*)(VkInstance, uint32_t*, VkPhysicalDevice*);
using PFN_vkGetPhysicalDeviceProperties = void (*)(VkPhysicalDevice, VkPhysicalDevicePropertiesLite*);
using PFN_vkDestroySurfaceKHR = void (*)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
#ifdef _WIN32
using PFN_vkCreateWin32SurfaceKHR = VkResult (*)(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*);
#endif

constexpr VkResult VK_SUCCESS = 0;
constexpr VkStructureType VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR =
    static_cast<VkStructureType>(1000009000);

constexpr uint32_t vkMakeVersion(uint32_t major, uint32_t minor, uint32_t patch) {
    return (major << 22) | (minor << 12) | patch;
}
#endif

uint32_t makeVkVersion(uint32_t major, uint32_t minor, uint32_t patch) {
#if FLUXUI_HAS_VULKAN_SDK
    return VK_MAKE_VERSION(major, minor, patch);
#else
    return vkMakeVersion(major, minor, patch);
#endif
}

uint32_t vkVersionMajor(uint32_t version) {
    return version >> 22;
}

uint32_t vkVersionMinor(uint32_t version) {
    return (version >> 12) & 0x3ff;
}

uint32_t vkVersionPatch(uint32_t version) {
    return version & 0xfff;
}

#if FLUXUI_HAS_VULKAN_SDK
using FluxUIVkPhysicalDeviceProperties = VkPhysicalDeviceProperties;
#else
using FluxUIVkPhysicalDeviceProperties = VkPhysicalDevicePropertiesLite;
#endif

const char* physicalDeviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:
        return "other";
    }
}

std::string vkResultMessage(const char* operation, VkResult result) {
    return std::string(operation) + " failed with VkResult " + std::to_string(result);
}

class CompatibilityBackendInfo final : public IRenderBackend {
public:
    RenderBackendInfo info() const override {
        return {
            RenderBackendType::Compatibility,
            "Compatibility",
            true,
            true,
            "CPU software fallback for systems without a usable GPU backend"
        };
    }

    uint32_t windowFlags() const override {
        return 0;
    }
};

class VulkanBackendInfo final : public IRenderBackend {
public:
    RenderBackendInfo info() const override {
        return {
            RenderBackendType::Vulkan,
            "Vulkan",
            FLUXUI_HAS_VULKAN_SDK != 0,
            FLUXUI_HAS_VULKAN_SDK != 0,
            FLUXUI_HAS_VULKAN_SDK != 0
                ? "Native Vulkan swapchain renderer is compiled"
                : "Not compiled in this build"
        };
    }

    uint32_t windowFlags() const override {
        return 0;
    }
};

class Direct3D12BackendInfo final : public IRenderBackend {
public:
    RenderBackendInfo info() const override {
        return {
            RenderBackendType::Direct3D12,
            "Direct3D 12",
            FLUXUI_HAS_D3D12 != 0,
            false,
            FLUXUI_HAS_D3D12 != 0
                ? "Windows backend target is compiled; draw implementation is staged"
                : "Not compiled in this build"
        };
    }

    uint32_t windowFlags() const override {
        return 0;
    }
};

class SkiaBackendInfo final : public IRenderBackend {
public:
    RenderBackendInfo info() const override {
        return {
            RenderBackendType::Skia,
            "Skia",
            FLUXUI_HAS_SKIA != 0,
            false,
            FLUXUI_HAS_SKIA != 0
                ? "Skia backend target is compiled; draw implementation is staged"
                : "Not compiled in this build"
        };
    }

    uint32_t windowFlags() const override {
        return 0;
    }
};

class MetalBackendInfo final : public IRenderBackend {
public:
    RenderBackendInfo info() const override {
        return {
            RenderBackendType::Metal,
            "Metal",
            FLUXUI_HAS_METAL != 0,
            false,
            FLUXUI_HAS_METAL != 0
                ? "Apple backend target is compiled; draw implementation is staged"
                : "Not compiled in this build"
        };
    }

    uint32_t windowFlags() const override {
        return 0;
    }
};

const CompatibilityBackendInfo& compatibilityBackendInfo() {
    static CompatibilityBackendInfo backend;
    return backend;
}

const VulkanBackendInfo& vulkanBackendInfo() {
    static VulkanBackendInfo backend;
    return backend;
}

const Direct3D12BackendInfo& direct3D12BackendInfo() {
    static Direct3D12BackendInfo backend;
    return backend;
}

const SkiaBackendInfo& skiaBackendInfo() {
    static SkiaBackendInfo backend;
    return backend;
}

const MetalBackendInfo& metalBackendInfo() {
    static MetalBackendInfo backend;
    return backend;
}

RenderBackendType chooseAutoBackend() {
    const RenderBackendType candidates[] = {
        Renderer::defaultBackend(),
        RenderBackendType::Vulkan,
#if defined(_WIN32)
        RenderBackendType::Direct3D12,
#endif
#if defined(__APPLE__)
        RenderBackendType::Metal,
#endif
        RenderBackendType::Direct3D12,
        RenderBackendType::Skia,
        RenderBackendType::Metal,
    };

    for (RenderBackendType candidate : candidates) {
        if (candidate == RenderBackendType::Auto ||
            candidate == RenderBackendType::Compatibility) {
            continue;
        }

        const auto info = Renderer::getBackendInfo(candidate);
        if (info.compiled && info.selectable) {
            return candidate;
        }
    }

    const auto vkInfo = Renderer::getBackendInfo(RenderBackendType::Vulkan);
    if (vkInfo.compiled && vkInfo.selectable) {
        return RenderBackendType::Vulkan;
    }

    return RenderBackendType::Compatibility;
}

RenderBackendType chooseBackend(RenderBackendType preference) {
    if (const char* envBackend = std::getenv("FLUXUI_BACKEND")) {
        std::string envStr(envBackend);
        for (char& c : envStr) c = std::tolower(c);
        if (envStr == "compatibility" || envStr == "software" || envStr == "cpu") {
            return RenderBackendType::Compatibility;
        } else if (envStr == "vulkan" || envStr == "gpu") {
            return RenderBackendType::Vulkan;
        }
    }

    if (preference == RenderBackendType::Auto) {
        return chooseAutoBackend();
    }

    if (preference == RenderBackendType::Compatibility) {
        return RenderBackendType::Compatibility;
    }

    const auto info = Renderer::getBackendInfo(preference);
    if (info.selectable) {
        return preference;
    }

    const auto vkInfo = Renderer::getBackendInfo(RenderBackendType::Vulkan);
    const bool fallbackToVulkan = vkInfo.compiled && vkInfo.selectable;

    if (!info.compiled) {
        std::cerr << "FluxUI: " << info.name
                  << " backend was requested but is not compiled in this build. Using "
                  << (fallbackToVulkan ? "Vulkan" : "compatibility") << " renderer." << std::endl;
    } else {
        std::cerr << "FluxUI: " << info.name
                  << " backend is staged but not selectable yet. Using "
                  << (fallbackToVulkan ? "Vulkan" : "compatibility") << " renderer." << std::endl;
    }

    if (fallbackToVulkan) {
        return RenderBackendType::Vulkan;
    }

    return RenderBackendType::Compatibility;
}

} // namespace

#if FLUXUI_HAS_VULKAN_SDK
// decompressFontAtlas lives below (font subsystem); expose a shared wrapper so
// renderer_vulkan.cpp's font-texture upload path can reach it.
bool decompressFontAtlasShared(FontData& font);
#endif

bool Renderer::updateDynamicTexture(const std::string& name, const ImageData& image) {
    if (name.empty()) return false;
    
    auto it = images_.find(name);
    uint32_t oldTexId = 0;
    if (it != images_.end()) {
        oldTexId = it->second.textureId;
        if (oldTexId != 0 && activeBackend_ == RenderBackendType::Auto) {
            glDeleteTextures(1, &oldTexId);
        }
    }
    
    ImageData newImg = image;
    newImg.textureId = 0;
    images_[name] = std::move(newImg);
    
    if (activeBackend_ == RenderBackendType::Vulkan && vulkan_ && vulkan_->device) {
#if FLUXUI_HAS_VULKAN_SDK
        ensureVulkanImageTexture(*vulkan_, name, images_[name]);
#endif
    } else if (backendInitialized_) {
        ensureImageTexture(name, images_[name]);
    }
    return true;
}


// ============================================================
//  GLSL Shaders
// ============================================================

static const char* ROUNDED_RECT_VERT = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform vec4 uRect;       // x, y, w, h
uniform mat4 uProjection;
out vec2 vLocalPos;
out vec2 vSize;
void main() {
    vec2 pos = uRect.xy + aPos * uRect.zw;
    vLocalPos = aPos * uRect.zw;
    vSize = uRect.zw;
    gl_Position = uProjection * vec4(pos, 0.0, 1.0);
}
)";

static const char* ROUNDED_RECT_FRAG = R"(
#version 330 core
in vec2 vLocalPos;
in vec2 vSize;
out vec4 FragColor;

uniform vec4 uColor;
uniform vec4 uColor2;
uniform vec4 uBorderColor;
uniform float uRadius;
uniform float uBorderWidth;
uniform float uOpacity;
uniform int uHasGradient;
uniform float uGradientAngle;

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 halfSize = vSize * 0.5;
    vec2 center = vLocalPos - halfSize;
    float r = min(uRadius, min(halfSize.x, halfSize.y));
    float dist = roundedBoxSDF(center, halfSize, r);

    float aa = max(fwidth(dist), 0.55);
    float alpha = 1.0 - smoothstep(-aa, aa, dist);

    // Fill color (with optional gradient)
    vec4 fillColor = uColor;
    if (uHasGradient == 1) {
        float angle = radians(uGradientAngle);
        vec2 dir = vec2(cos(angle), sin(angle));
        float t = dot(vLocalPos / vSize, dir) * 0.5 + 0.5;
        t = clamp(t, 0.0, 1.0);
        fillColor = mix(uColor, uColor2, t);
    }

    // Border
    if (uBorderWidth > 0.0) {
        float innerDist = roundedBoxSDF(center, halfSize - vec2(uBorderWidth), max(r - uBorderWidth, 0.0));
        float borderAa = max(fwidth(innerDist), 0.55);
        float borderAlpha = 1.0 - smoothstep(-borderAa, borderAa, innerDist);
        fillColor = mix(uBorderColor, fillColor, borderAlpha);
    }

    FragColor = vec4(fillColor.rgb, fillColor.a * alpha * uOpacity);
}
)";

static const char* SHADOW_FRAG = R"(
#version 330 core
in vec2 vLocalPos;
in vec2 vSize;
out vec4 FragColor;

uniform vec4 uColor;
uniform float uRadius;
uniform float uBlur;
uniform float uOpacity;
uniform vec2 uBoxSize;

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 center = vLocalPos - vSize * 0.5;
    vec2 halfBox = uBoxSize * 0.5;
    float r = min(uRadius, min(halfBox.x, halfBox.y));
    float dist = roundedBoxSDF(center, halfBox, r);

    // shadow fades outwards from the border
    float shadowAlpha = 1.0 - smoothstep(0.0, uBlur, dist);
    FragColor = vec4(uColor.rgb, uColor.a * shadowAlpha * uOpacity);
}
)";

static const char* TEXT_VERT = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec4 aColor;
uniform mat4 uProjection;
out vec2 vTexCoord;
out vec4 vColor;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";

static const char* TEXT_FRAG = R"(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    float a = texture(uTexture, vTexCoord).r;
    FragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

static const char* IMAGE_FRAG = R"(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    vec4 tex = texture(uTexture, vTexCoord);
    FragColor = vec4(tex.rgb * vColor.rgb, tex.a * vColor.a);
}
)";

namespace {
// The CPU software-rasterizer helpers now live in software_internal.h
// (FluxUI::detail, included at the top of this file). Alias them here so the
// remaining users in this TU (SVG blur pass) keep their call sites. The Renderer
// member rasterizer functions were moved to software_raster.cpp.
using detail::SoftwareClip;
using detail::softwareToByte;
using detail::softwarePackOpaque;
using detail::softwareColorFromInstance;
using detail::softwareClipFor;
using detail::softwareNextCodepoint;
using detail::softwareRoundedCoverage;
using detail::softwareGradientColor;
using detail::softwareBlendPixel;
using detail::softwareBlendPixelFast;
using detail::softwareSampleFontAlpha;
} // namespace



// ============================================================
//  Shader Compilation
// ============================================================

Renderer::Renderer() {
    blurFBO_ = 0;
    blurTex_ = 0;
    blurTexTemp_ = 0;
    blurShaderHoriz_ = 0;
    blurShaderVert_ = 0;
    activeTransformNodeId = 0;
    activeClipNodeId = 0;
    activeEffectNodeId = 0;
#if FLUXUI_FAST_STARTUP
    fonts_.reserve(FLUXUI_PREALLOC_FONTS);
    images_.reserve(FLUXUI_PREALLOC_IMAGES);
#if FLUXUI_TEXT_MEASURE_CACHE_SIZE > 0
    textMeasureCache_.reserve(FLUXUI_TEXT_MEASURE_CACHE_SIZE);
#endif
    scissorStack_.reserve(FLUXUI_PREALLOC_SCISSORS);
    translationStack_.reserve(FLUXUI_PREALLOC_TRANSFORMS);
    scaleStack_.reserve(FLUXUI_PREALLOC_TRANSFORMS);
    scalePivotStack_.reserve(FLUXUI_PREALLOC_TRANSFORMS);
    rectBatch_.reserve(FLUXUI_PREALLOC_RECT_BATCH);
    textVertexScratch_.reserve(FLUXUI_PREALLOC_TEXT_FLOATS);
#endif
}
Renderer::~Renderer() = default;

void Renderer::setBackend(RenderBackendType backend) {
    if (backendInitialized_) {
        std::cerr << "FluxUI: renderer backend cannot be changed after initialization." << std::endl;
        return;
    }
    if (backendResolved_ && backendPreference_ == backend) {
        return;
    }
    backendPreference_ = backend;
    activeBackend_ = chooseBackend(backendPreference_);
    backendResolved_ = true;
}

const char* Renderer::activeBackendName() const {
    return backendName(activeBackend_);
}

uint32_t Renderer::windowFlags() const {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        return vulkanBackendInfo().windowFlags();
    }
    if (activeBackend_ == RenderBackendType::Direct3D12) {
        return direct3D12BackendInfo().windowFlags();
    }
    if (activeBackend_ == RenderBackendType::Skia) {
        return skiaBackendInfo().windowFlags();
    }
    if (activeBackend_ == RenderBackendType::Metal) {
        return metalBackendInfo().windowFlags();
    }
    return compatibilityBackendInfo().windowFlags();
}

RenderBackendInfo Renderer::getBackendInfo(RenderBackendType backend) {
    if (backend == RenderBackendType::Vulkan) {
        return vulkanBackendInfo().info();
    }
    if (backend == RenderBackendType::Direct3D12) {
        return direct3D12BackendInfo().info();
    }
    if (backend == RenderBackendType::Skia) {
        return skiaBackendInfo().info();
    }
    if (backend == RenderBackendType::Metal) {
        return metalBackendInfo().info();
    }
    if (backend == RenderBackendType::Compatibility) {
        return compatibilityBackendInfo().info();
    }
    RenderBackendInfo info;
    info.type = RenderBackendType::Auto;
    info.name = "Auto";
    info.compiled = true;
    info.selectable = true;
    info.note = "Uses the compiled default backend preference";
    return info;
}

const char* Renderer::backendName(RenderBackendType backend) {
    return getBackendInfo(backend).name;
}

RenderBackendType Renderer::defaultBackend() {
#if FLUXUI_DEFAULT_BACKEND == 1
    return RenderBackendType::Vulkan;
#elif FLUXUI_DEFAULT_BACKEND == 2
    return RenderBackendType::Direct3D12;
#elif FLUXUI_DEFAULT_BACKEND == 3
    return RenderBackendType::Metal;
#elif FLUXUI_DEFAULT_BACKEND == 4
    return RenderBackendType::Skia;
#else
    return RenderBackendType::Auto;
#endif
}

VulkanProbeResult Renderer::probeVulkanRuntime(const char* appName) {
    VulkanProbeResult result;

    void* vulkanModule = nullptr;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#if FLUXUI_HAS_VULKAN_SDK
    vkGetInstanceProcAddr = ::vkGetInstanceProcAddr;
    result.loaderAvailable = vkGetInstanceProcAddr != nullptr;
#else
    vulkanModule = Platform::loadVulkanLibrary();
    if (vulkanModule) {
        vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            Platform::getVulkanProc(vulkanModule, "vkGetInstanceProcAddr"));
    }
    result.loaderAvailable = vulkanModule != nullptr;
#endif

    auto unloadLoader = [&]() {
        if (vulkanModule) {
            Platform::unloadVulkanLibrary(vulkanModule);
            vulkanModule = nullptr;
        }
    };

    if (!result.loaderAvailable) {
        result.error = "Could not load Vulkan library";
        return result;
    }

    if (!vkGetInstanceProcAddr) {
        unloadLoader();
        result.error = "vkGetInstanceProcAddr not found";
        return result;
    }

    auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));

    if (!vkCreateInstance) {
        unloadLoader();
        result.error = "vkCreateInstance not found";
        return result;
    }

    std::vector<const char*> extensions = Platform::getVulkanInstanceExtensions();

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName ? appName : "FluxUI Probe";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = (uint32_t)extensions.size();
    instInfo.ppEnabledExtensionNames = extensions.data();

    VkInstance instance = nullptr;
    VkResult res = vkCreateInstance(&instInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        unloadLoader();
        result.error = "vkCreateInstance failed";
        return result;
    }
    result.instanceCreated = true;

    PlatformWindowConfig config;
    config.title = "Vulkan Probe";
    config.width = 1; config.height = 1;
    NativeWindowHandle tempWindow = Platform::createWindow(config);
    if (tempWindow) {
        result.windowCreated = true;
        VkSurfaceKHR surface = nullptr;
        if (Platform::createVulkanSurface(instance, tempWindow, &surface)) {
            result.surfaceCreated = true;
            auto vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
            if (vkDestroySurfaceKHR) vkDestroySurfaceKHR(instance, surface, nullptr);
        }

        auto vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
        auto vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));

        if (vkEnumeratePhysicalDevices && vkGetPhysicalDeviceProperties) {
            uint32_t count = 0;
            vkEnumeratePhysicalDevices(instance, &count, nullptr);
            result.physicalDeviceCount = count;
            if (count > 0) {
                std::vector<VkPhysicalDevice> devices(count);
                vkEnumeratePhysicalDevices(instance, &count, devices.data());
                for (uint32_t i = 0; i < count; i++) {
                    FluxUIVkPhysicalDeviceProperties props = {};
                    vkGetPhysicalDeviceProperties(devices[i], &props);
                    result.deviceNames.push_back(props.deviceName);
                }
            }
        }
        Platform::destroyWindow(tempWindow);
    }

    if (result.physicalDeviceCount == 0) {
        auto vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
            vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
        auto vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
        if (vkEnumeratePhysicalDevices && vkGetPhysicalDeviceProperties) {
            uint32_t count = 0;
            vkEnumeratePhysicalDevices(instance, &count, nullptr);
            result.physicalDeviceCount = count;
            if (count > 0) {
                std::vector<VkPhysicalDevice> devices(count);
                vkEnumeratePhysicalDevices(instance, &count, devices.data());
                for (uint32_t i = 0; i < count; ++i) {
                    FluxUIVkPhysicalDeviceProperties props = {};
                    vkGetPhysicalDeviceProperties(devices[i], &props);
                    result.deviceNames.push_back(props.deviceName);
                }
            }
        }
    }

    auto vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
    if (vkDestroyInstance) vkDestroyInstance(instance, nullptr);
    
    unloadLoader();
    return result;
}

static bool decompressFontAtlas(FontData& font) {
    if (!font.atlasPixels.empty()) {
        return true;
    }
    if (font.runCount == 0 || !font.runLengths || !font.runValues || font.pixelCount == 0) {
        return false;
    }
    font.atlasPixels.resize(font.pixelCount);
    size_t out = 0;
    for (size_t i = 0; i < font.runCount; ++i) {
        size_t count = font.runLengths[i];
        if (count == 0 || out + count > font.pixelCount) {
            font.atlasPixels.clear();
            return false;
        }
        std::fill(font.atlasPixels.begin() + static_cast<std::ptrdiff_t>(out),
                  font.atlasPixels.begin() + static_cast<std::ptrdiff_t>(out + count),
                  font.runValues[i]);
        out += count;
    }
    if (out != font.pixelCount) {
        font.atlasPixels.clear();
        return false;
    }
    return true;
}

#if FLUXUI_HAS_VULKAN_SDK
// Shared entry point for renderer_vulkan.cpp's font-texture upload path.
bool decompressFontAtlasShared(FontData& font) {
    return decompressFontAtlas(font);
}
#endif


uint32_t Renderer::compileShader(const char* vertSrc, const char* fragSrc) {
    auto compile = [](GLenum type, const char* src) -> uint32_t {
        uint32_t s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
            std::cerr << "Shader error: " << log << std::endl;
        }
        return s;
    };
    uint32_t vs = compile(GL_VERTEX_SHADER, vertSrc);
    uint32_t fs = compile(GL_FRAGMENT_SHADER, fragSrc);
    uint32_t prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// ============================================================
//  Initialization
// ============================================================

void Renderer::setupQuad() {
    float verts[] = { 0,0, 1,0, 1,1, 0,0, 1,1, 0,1 };
    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::setupTextBuffer() {
    constexpr size_t initialFloatCapacity = FLUXUI_PREALLOC_TEXT_FLOATS;

    glGenVertexArrays(1, &textVAO_);
    glGenBuffers(1, &textVBO_);
    glBindVertexArray(textVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO_);
    glBufferData(GL_ARRAY_BUFFER, initialFloatCapacity * sizeof(float), nullptr, GL_STREAM_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    textVBOCapacity_ = initialFloatCapacity;
}

void Renderer::setupInstanceBuffer() {
    constexpr size_t initialCapacity = FLUXUI_PREALLOC_RECT_BATCH;
    glGenBuffers(1, &instanceVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, initialCapacity * sizeof(RoundedRectInstance),
                 nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    instanceVBOCapacity_ = initialCapacity;
    rectBatch_.reserve(initialCapacity);
}

// Forward declaration needed by flushRectBatch
static void setProjection(int projectionLocation, int w, int h, float scale, Vec2 pivot);

void Renderer::flush() {
    flushRectBatch();
}

void Renderer::flushRectBatch() {
    if (rectBatch_.empty()) return;

    if (activeBackend_ == RenderBackendType::Compatibility) {
        flushSoftwareRectBatch();
        return;
    }

    useShader(roundedRectShader_);
    Vec2 pivot = batchScalePivot_;
    setProjection(roundedUniforms_.projection, windowWidth_, windowHeight_, batchScale_, pivot);

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    if (rectBatch_.size() > instanceVBOCapacity_) {
        instanceVBOCapacity_ = std::max(rectBatch_.size(), instanceVBOCapacity_ * 2);
        glBufferData(GL_ARRAY_BUFFER, instanceVBOCapacity_ * sizeof(RoundedRectInstance),
                     nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    rectBatch_.size() * sizeof(RoundedRectInstance),
                    rectBatch_.data());

    // Draw each instance individually using the per-instance uniforms
    // (True instancing would require a shader rewrite, so we use a tight
    //  loop with minimal state changes instead — still ~10x faster than
    //  the old path because we skip useShader/setProjection/glBindVertexArray
    //  overhead per rect.)
    glBindVertexArray(quadVAO_);
    for (size_t i = 0; i < rectBatch_.size(); i++) {
        auto& inst = rectBatch_[i];
        glUniform4fv(roundedUniforms_.rect, 1, inst.rect);
        glUniform4fv(roundedUniforms_.color, 1, inst.color);
        glUniform4fv(roundedUniforms_.color2, 1, inst.color2);
        glUniform4fv(roundedUniforms_.borderColor, 1, inst.borderColor);
        glUniform1f(roundedUniforms_.radius, inst.radius);
        glUniform1f(roundedUniforms_.borderWidth, inst.borderWidth);
        glUniform1f(roundedUniforms_.opacity, inst.opacity);
        glUniform1i(roundedUniforms_.hasGradient, (int)inst.hasGradient);
        if (inst.hasGradient > 0.0f)
            glUniform1f(roundedUniforms_.gradientAngle, inst.gradientAngle);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    rectBatch_.clear();
}

void Renderer::cacheUniformLocations() {
    auto loc = [](uint32_t shader, const char* name) {
        return glGetUniformLocation(shader, name);
    };

    roundedUniforms_.projection = loc(roundedRectShader_, "uProjection");
    roundedUniforms_.rect = loc(roundedRectShader_, "uRect");
    roundedUniforms_.color = loc(roundedRectShader_, "uColor");
    roundedUniforms_.color2 = loc(roundedRectShader_, "uColor2");
    roundedUniforms_.borderColor = loc(roundedRectShader_, "uBorderColor");
    roundedUniforms_.radius = loc(roundedRectShader_, "uRadius");
    roundedUniforms_.borderWidth = loc(roundedRectShader_, "uBorderWidth");
    roundedUniforms_.opacity = loc(roundedRectShader_, "uOpacity");
    roundedUniforms_.hasGradient = loc(roundedRectShader_, "uHasGradient");
    roundedUniforms_.gradientAngle = loc(roundedRectShader_, "uGradientAngle");

    shadowUniforms_.projection = loc(shadowShader_, "uProjection");
    shadowUniforms_.rect = loc(shadowShader_, "uRect");
    shadowUniforms_.color = loc(shadowShader_, "uColor");
    shadowUniforms_.radius = loc(shadowShader_, "uRadius");
    shadowUniforms_.blur = loc(shadowShader_, "uBlur");
    shadowUniforms_.opacity = loc(shadowShader_, "uOpacity");
    shadowUniforms_.boxSize = loc(shadowShader_, "uBoxSize");

    textUniforms_.projection = loc(textShader_, "uProjection");
    textUniforms_.texture = loc(textShader_, "uTexture");
    imageUniforms_.projection = loc(imageShader_, "uProjection");
    imageUniforms_.texture = loc(imageShader_, "uTexture");
}

void Renderer::useShader(uint32_t shader) {
    if (activeShader_ == shader) return;
    glUseProgram(shader);
    activeShader_ = shader;
}

bool Renderer::init(void* windowHandle) {
    window_ = windowHandle;

    if (activeBackend_ == RenderBackendType::Vulkan) {
        if (initVulkan(windowHandle)) {
            return true;
        }
        std::cerr << "FluxUI: Vulkan could not start. Switching to CPU "
                  << "software compatibility renderer." << std::endl;
        activeBackend_ = RenderBackendType::Compatibility;
        return initSoftware(windowHandle);
    }

    if (activeBackend_ != RenderBackendType::Compatibility) {
        std::cerr << "FluxUI: " << activeBackendName()
                  << " draw path is not implemented yet. Using compatibility renderer." << std::endl;
        activeBackend_ = RenderBackendType::Compatibility;
    }

    return initSoftware(windowHandle);
}

void Renderer::shutdown() {
    if (activeBackend_ == RenderBackendType::Vulkan || vulkan_) {
        shutdownVulkan();
        backendInitialized_ = false;
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        shutdownSoftware();
        backendInitialized_ = false;
        activeShader_ = 0;
        return;
    }

    if (roundedRectShader_) glDeleteProgram(roundedRectShader_);
    if (shadowShader_) glDeleteProgram(shadowShader_);
    if (textShader_) glDeleteProgram(textShader_);
    if (imageShader_) glDeleteProgram(imageShader_);
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
    if (instanceVBO_) glDeleteBuffers(1, &instanceVBO_);
    if (textVAO_) glDeleteVertexArrays(1, &textVAO_);
    if (textVBO_) glDeleteBuffers(1, &textVBO_);
    for (auto& [name, font] : fonts_) {
        if (font.textureId) glDeleteTextures(1, &font.textureId);
    }
    for (auto& [name, image] : images_) {
        if (image.textureId) glDeleteTextures(1, &image.textureId);
    }
    if (blurFBO_) glDeleteFramebuffers(1, &blurFBO_);
    if (blurTex_) glDeleteTextures(1, &blurTex_);
    if (blurTexTemp_) glDeleteTextures(1, &blurTexTemp_);
    if (blurShaderHoriz_) glDeleteProgram(blurShaderHoriz_);
    if (blurShaderVert_) glDeleteProgram(blurShaderVert_);
    blurFBO_ = 0;
    blurTex_ = 0;
    blurTexTemp_ = 0;
    blurShaderHoriz_ = 0;
    blurShaderVert_ = 0;
    softwareBlurBuffer1_.clear();
    softwareBlurBuffer1_.shrink_to_fit();
    softwareBlurBuffer2_.clear();
    softwareBlurBuffer2_.shrink_to_fit();
    softwareBlurBuffer3_.clear();
    softwareBlurBuffer3_.shrink_to_fit();
    // glContext_ (SDL_GLContext) removed
    glContext_ = nullptr;
    backendInitialized_ = false;
    activeShader_ = 0;
}

// ============================================================
//  Frame Management
// ============================================================

void Renderer::beginFrame(int w, int h) {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        if (beginVulkanFrame(w, h)) {
            return;
        }
        // Vulkan frame begin failed (e.g. during resize/minimize/out-of-date swapchain).
        // Do not permanently switch to software renderer. Just return to skip this frame.
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        beginSoftwareFrame(w, h);
        return;
    }

    windowWidth_ = w; windowHeight_ = h;
    
    int drawW = w, drawH = h;
    if (window_) {
        Platform::getWindowSize(window_, drawW, drawH);
        // Simplified DPI calculation: assume 1:1 if not set
        dpiScale_ = (w > 0) ? (float)drawW / (float)w : 1.0f;
    }
    glViewport(0, 0, drawW, drawH);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    scissorStack_.clear();
    activeShader_ = 0;
    rectBatch_.clear();
    batchValid_ = false;
    glDisable(GL_SCISSOR_TEST);
}

void Renderer::endFrame() {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        endVulkanFrame();
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        endSoftwareFrame();
        return;
    }

    // Flush any remaining batched rects before presenting
    flushRectBatch();

    // In Vulkan, endFrame handles presentation.
    // In Win32/GDI it would be SwapBuffers((HDC)window_);
}

bool Renderer::needsRepaint() const {
#if FLUXUI_HAS_VULKAN_SDK
    if (activeBackend_ == RenderBackendType::Vulkan && vulkan_) {
        return vulkan_->swapchainDirty;
    }
#endif
    return false;
}

// ============================================================
//  Font Loading
// ============================================================

static bool readFontFile(const std::string& path, std::vector<unsigned char>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) return false;

    file.seekg(0, std::ios::beg);
    data.resize((size_t)fileSize);
    return file.read(reinterpret_cast<char*>(data.data()), fileSize).good();
}

static std::vector<std::string> defaultFontPaths() {
    std::vector<std::string> paths = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/SFPro.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };

#ifdef _WIN32
    if (const char* windir = std::getenv("WINDIR")) {
        std::string fontDir = std::string(windir) + "/Fonts/";
        paths.insert(paths.begin(), {
            fontDir + "segoeui.ttf",
            fontDir + "arial.ttf",
        });
    }
#endif

    return paths;
}

static std::vector<std::string> defaultBoldFontPaths() {
    std::vector<std::string> paths = {
        "C:/Windows/Fonts/seguisb.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/SFPro.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };

#ifdef _WIN32
    if (const char* windir = std::getenv("WINDIR")) {
        std::string fontDir = std::string(windir) + "/Fonts/";
        paths.insert(paths.begin(), {
            fontDir + "seguisb.ttf",
            fontDir + "segoeuib.ttf",
            fontDir + "arialbd.ttf",
        });
    }
#endif

    return paths;
}

bool Renderer::loadDefaultFont(float size, const std::string& name) {
    for (const auto& path : defaultFontPaths()) {
        if (loadFont(path, size, name)) {
            if (name == "default") {
                for (const auto& boldPath : defaultBoldFontPaths()) {
                    if (loadFont(boldPath, size, name + "-bold")) {
                        break;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

bool Renderer::loadFont(const std::string& path, float size, const std::string& name) {
    std::vector<unsigned char> data;
    if (!readFontFile(path, data)) return false;

    return loadFontFromMemory(data.data(), (int)data.size(), size, name);
}

bool Renderer::loadFontFromMemory(const unsigned char* data, int dataSize, float size, const std::string& name) {
    FontData font;
    if (data && dataSize > 0) {
        font.sourceData.assign(data, data + dataSize);
        if (!buildFontAtlas(font, data, dataSize, size)) {
            return false;
        }
    } else {
        return loadDefaultFont(size, name);
    }

    fonts_[name] = std::move(font);
    textMeasureCache_.clear();
    if (!currentFont_) currentFont_ = &fonts_[name];

    return true;
}

bool Renderer::loadPrebakedFontAtlas(const std::string& name, float pixelSize,
                                     int atlasWidth, int atlasHeight,
                                     float ascent, float descent, float lineGap,
                                     const GlyphInfo* glyphs, size_t glyphCount,
                                     const uint16_t* runLengths,
                                     const unsigned char* runValues,
                                     size_t runCount, size_t pixelCount) {
    if (name.empty() || pixelSize <= 0.0f || atlasWidth <= 0 || atlasHeight <= 0 ||
        !glyphs || glyphCount == 0 || !runLengths || !runValues || runCount == 0) {
        return false;
    }

    const size_t expectedPixels = static_cast<size_t>(atlasWidth) *
                                  static_cast<size_t>(atlasHeight);
    if (pixelCount != expectedPixels) {
        return false;
    }

    FontData font;
    font.fontSize = pixelSize;
    font.atlasWidth = atlasWidth;
    font.atlasHeight = atlasHeight;
    font.ascent = ascent;
    font.descent = descent;
    font.lineGap = lineGap;
    font.loaded = true;

    const size_t copyGlyphs = std::min<size_t>(glyphCount, 1024);
    for (size_t i = 0; i < copyGlyphs; ++i) {
        font.glyphs[i] = glyphs[i];
    }

    font.runLengths = runLengths;
    font.runValues = runValues;
    font.runCount = runCount;
    font.pixelCount = pixelCount;

    fonts_[name] = std::move(font);
    textMeasureCache_.clear();
    if (!currentFont_) currentFont_ = &fonts_[name];

    return true;
}

bool Renderer::decodeImageBytes(const unsigned char* data, int dataSize,
                                ImageData& image, bool forceSvg) {
    if (!data || dataSize <= 0) return false;

    if (forceSvg || hasSvgSignature(data, dataSize)) {
        return rasterizeSvgToRgba(data, dataSize, image);
    }

    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory(data, dataSize, &w, &h, &comp) ||
        w <= 0 || h <= 0 ||
        w > 16384 || h > 16384 ||
        (size_t)w * (size_t)h > 64u * 1024u * 1024u) {
        return false;
    }

    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(data, dataSize, &w, &h, &channels, 4);
    if (!decoded) return false;

    image.width = w;
    image.height = h;
    image.textureId = 0;
    image.loaded = true;
    image.svg = false;
    image.pixels.assign(decoded, decoded + (size_t)w * (size_t)h * 4u);
    stbi_image_free(decoded);
    return true;
}

bool Renderer::loadImage(const std::string& path, const std::string& name) {
    std::string key = name.empty() ? path : name;
    auto existing = images_.find(key);
    if (existing != images_.end() && existing->second.loaded) {
        return true;
    }

    std::vector<unsigned char> data;
    if (!readFontFile(path, data)) return false;

    std::string lowerPath = lowerSvgString(path);
    bool forceSvg = lowerPath.size() >= 4 &&
                    lowerPath.substr(lowerPath.size() - 4) == ".svg";

    ImageData image;
    if (!decodeImageBytes(data.data(), (int)data.size(), image, forceSvg)) {
        return false;
    }

    auto [it, inserted] = images_.insert_or_assign(key, std::move(image));
    (void)inserted;
    if (activeBackend_ == RenderBackendType::Vulkan && vulkan_ && vulkan_->device) {
#if FLUXUI_HAS_VULKAN_SDK
        ensureVulkanImageTexture(*vulkan_, key, it->second);
#endif
    } else if (backendInitialized_) {
        ensureImageTexture(key, it->second);
    }
    return true;
}

bool Renderer::loadImageFromMemory(const unsigned char* data, int dataSize,
                                   const std::string& name, bool svg) {
    if (name.empty()) return false;
    ImageData image;
    if (!decodeImageBytes(data, dataSize, image, svg)) {
        return false;
    }
    auto [it, inserted] = images_.insert_or_assign(name, std::move(image));
    (void)inserted;
    if (activeBackend_ == RenderBackendType::Vulkan && vulkan_ && vulkan_->device) {
#if FLUXUI_HAS_VULKAN_SDK
        ensureVulkanImageTexture(*vulkan_, name, it->second);
#endif
    } else if (backendInitialized_) {
        ensureImageTexture(name, it->second);
    }
    return true;
}

Vec2 Renderer::imageSize(const std::string& nameOrPath) {
    auto it = images_.find(nameOrPath);
    if (it == images_.end() || !it->second.loaded) {
        loadImage(nameOrPath);
        it = images_.find(nameOrPath);
    }
    if (it == images_.end() || !it->second.loaded) {
        return {0, 0};
    }
    return {(float)it->second.width, (float)it->second.height};
}

const std::vector<unsigned char>* Renderer::imagePixels(const std::string& nameOrPath) {
    auto it = images_.find(nameOrPath);
    if (it == images_.end() || !it->second.loaded) {
        loadImage(nameOrPath);
        it = images_.find(nameOrPath);
    }
    if (it == images_.end() || !it->second.loaded) {
        return nullptr;
    }
    return &it->second.pixels;
}

void Renderer::warmFontCache(float size, const std::string& name) {
    if (size <= 0.0f) return;

    std::string resolvedNames[] = {
        name,
        resolveFontName(name, FontWeight::Bold)
    };

    for (int i = 0; i < 2; ++i) {
        const std::string& resolvedName = resolvedNames[i];
        if (resolvedName.empty()) continue;
        if (i == 1 && resolvedName == resolvedNames[0]) continue;

        FontData* font = getFontForSize(resolvedName, size);
        if (!font || !font->loaded) continue;

#if FLUXUI_HAS_VULKAN_SDK
        if (activeBackend_ == RenderBackendType::Vulkan && vulkan_) {
            int snappedSize = std::max(8, (int)std::round(font->fontSize));
            std::string textureKey = resolvedName + "@" + std::to_string(snappedSize);
            auto baseFontIt = fonts_.find(resolvedName);
            if (baseFontIt != fonts_.end() && font == &baseFontIt->second) {
                textureKey = resolvedName;
            }
            ensureVulkanFontTexture(*vulkan_, textureKey, *font);
        }
#endif
    }
}

void Renderer::warmFontCache(const std::vector<float>& sizes, const std::string& name) {
    for (float size : sizes) {
        warmFontCache(size, name);
    }
}

void Renderer::releaseFontSources() {
    for (auto& entry : fonts_) {
        auto& font = entry.second;
        if (!font.sourceData.empty()) {
            font.sourceData.clear();
            font.sourceData.shrink_to_fit();
        }
    }
}

void Renderer::registerCustomFont(const std::string& family, const std::string& path) {
    if (family.empty() || path.empty()) return;
    auto it = customFontRegistry_.find(family);
    if (it != customFontRegistry_.end()) {
        if (it->second.path != path) {
            it->second.path = path;
            it->second.loaded = false;
            it->second.loading = false;
        }
        return;
    }
    CustomFontFaceInfo info;
    info.family = family;
    info.path = path;
    info.loading = false;
    info.loaded = false;
    customFontRegistry_[family] = info;
}

void Renderer::triggerCustomFontLoad(const std::string& family) {
    auto it = customFontRegistry_.find(family);
    if (it == customFontRegistry_.end() || it->second.loading || it->second.loaded) return;
    
    it->second.loading = true;
    std::string path = it->second.path;
    
    std::thread([this, family, path]() {
        std::vector<unsigned char> data;
        if (!readFontFile(path, data)) {
            Application::instance()->runOnMainThread([this, family]() {
                auto it = customFontRegistry_.find(family);
                if (it != customFontRegistry_.end()) {
                    it->second.loading = false;
                }
            });
            return;
        }
        
        FontData baseFont;
        baseFont.sourceData = data;
        if (!buildFontAtlas(baseFont, data.data(), (int)data.size(), 16.0f, true)) {
            Application::instance()->runOnMainThread([this, family]() {
                auto it = customFontRegistry_.find(family);
                if (it != customFontRegistry_.end()) {
                    it->second.loading = false;
                }
            });
            return;
        }
        
        Application::instance()->runOnMainThread([this, family, baseFont = std::move(baseFont)]() mutable {
            fonts_[family] = std::move(baseFont);
            auto it = customFontRegistry_.find(family);
            if (it != customFontRegistry_.end()) {
                it->second.loaded = true;
                it->second.loading = false;
            }
            textMeasureCache_.clear();
            Application::instance()->requestRedraw();
        });
    }).detach();
}

bool Renderer::ensureImageTexture(const std::string& key, ImageData& image) {
    if (!image.loaded) return false;
    if (activeBackend_ == RenderBackendType::Compatibility) {
        (void)key;
        return !image.pixels.empty() && image.width > 0 && image.height > 0;
    }
    if (activeBackend_ == RenderBackendType::Vulkan) {
#if FLUXUI_HAS_VULKAN_SDK
        return vulkan_ && vulkan_->device && ensureVulkanImageTexture(*vulkan_, key, image);
#else
        (void)key;
        return false;
#endif
    }

    if (image.textureId != 0) return true;
    if (image.pixels.empty() || image.width <= 0 || image.height <= 0) return false;
    glGenTextures(1, &image.textureId);
    glBindTexture(GL_TEXTURE_2D, image.textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return image.textureId != 0;
}

bool Renderer::ensureFontTexture(FontData& font) {
    if (!font.loaded) return false;
    if (activeBackend_ == RenderBackendType::Compatibility) {
        return true;
    }
    if (activeBackend_ == RenderBackendType::Vulkan) {
        return true;
    }

    if (font.textureId != 0) return true;

    if (font.atlasPixels.empty()) {
        if (!decompressFontAtlas(font)) {
            return false;
        }
    }

    if (font.atlasPixels.empty() || font.atlasWidth <= 0 || font.atlasHeight <= 0) {
        return false;
    }

    glGenTextures(1, &font.textureId);
    glBindTexture(GL_TEXTURE_2D, font.textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font.atlasWidth, font.atlasHeight,
                 0, GL_RED, GL_UNSIGNED_BYTE, font.atlasPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Clear CPU-side copy of the atlas pixels once uploaded to OpenGL to free system memory
    font.atlasPixels.clear();
    font.atlasPixels.shrink_to_fit();

    return font.textureId != 0;
}

// Platform-specific FreeType hinting to match Chromium/Blink perfectly
#if defined(_WIN32)
    // Windows: Use native TrueType bytecode hinting for sharp ClearType-like grid fitting
    #define FLUXUI_FT_LOAD_FLAGS (FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)
#elif defined(__APPLE__)
    // macOS/iOS (CoreText): Apple prefers pure vector shapes without grid-fitting (relying on Retina screens)
    #define FLUXUI_FT_LOAD_FLAGS (FT_LOAD_RENDER | FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT)
#elif defined(__ANDROID__)
    // Android: Prefers light auto-hinting
    #define FLUXUI_FT_LOAD_FLAGS (FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT)
#else
    // Linux/Other: Often prefers light hinting or normal, TARGET_LIGHT is a safe modern default
    #define FLUXUI_FT_LOAD_FLAGS (FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT)
#endif

bool Renderer::buildFontAtlas(FontData& font, const unsigned char* data, int dataSize, float size, bool isBackground) {
    if (!data || dataSize <= 0) return false;

    FT_Library library;
    if (FT_Init_FreeType(&library)) {
        return false;
    }

    FT_Face face;
    if (FT_New_Memory_Face(library, data, dataSize, 0, &face)) {
        FT_Done_FreeType(library);
        return false;
    }

    float bakedSize = std::max(8.0f, size);
    float dpiScale = std::max(1.0f, dpiScale_);
    float pixelSize = (float)std::max(8, (int)std::round(bakedSize * dpiScale));

    if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixelSize)) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    int atlasSize = pixelSize > 48.0f ? 2048 : (pixelSize > 20.0f ? 1024 : 512);
    std::vector<unsigned char> atlas((size_t)atlasSize * (size_t)atlasSize, 0);

    constexpr int maxGlyphs = 1024;
    constexpr int firstGlyph = 32;
    const int glyphLimit = std::clamp(FLUXUI_FONT_GLYPH_LIMIT,
                                      firstGlyph + 1,
                                      maxGlyphs);

    for (int i = 0; i < maxGlyphs; ++i) {
        font.glyphs[i] = {};
    }
    font.extendedGlyphs.clear();
    font.kerningPairs.clear();
    font.supportedLigatures.clear();

    const std::vector<std::pair<std::string, uint32_t>> possibleLigatures = {
        {"ffi", 0xFB03}, {"ffl", 0xFB04}, {"ff", 0xFB00},
        {"fi",  0xFB01}, {"fl",  0xFB02}, {"ft", 0xFB05},
        {"st",  0xFB06}
    };

    std::vector<uint32_t> codepointsToLoad;
    for (int i = firstGlyph; i < glyphLimit; ++i) {
        codepointsToLoad.push_back((uint32_t)i);
    }
    for (const auto& lig : possibleLigatures) {
        FT_UInt glyphIndex = FT_Get_Char_Index(face, lig.second);
        if (glyphIndex != 0) {
            codepointsToLoad.push_back(lig.second);
            font.supportedLigatures.push_back(lig);
        }
    }

    int currentX = 1;
    int currentY = 1;
    int rowHeight = 0;
    bool packedOk = true;

    for (size_t idx = 0; idx < codepointsToLoad.size(); ++idx) {
        uint32_t i = codepointsToLoad[idx];
        FT_UInt glyphIndex = FT_Get_Char_Index(face, i);
        if (FT_Load_Glyph(face, glyphIndex, FLUXUI_FT_LOAD_FLAGS)) {
            continue;
        }

        FT_GlyphSlot slot = face->glyph;
        int w = slot->bitmap.width;
        int h = slot->bitmap.rows;

        if (currentX + w + 1 >= atlasSize) {
            currentX = 1;
            currentY += rowHeight + 1;
            rowHeight = 0;
        }

        if (currentY + h + 1 >= atlasSize) {
            if (atlasSize < 2048) {
                atlasSize = atlasSize < 1024 ? 1024 : 2048;
                atlas.assign((size_t)atlasSize * (size_t)atlasSize, 0);
                currentX = 1;
                currentY = 1;
                rowHeight = 0;
                for (size_t j_idx = 0; j_idx <= idx; ++j_idx) {
                    uint32_t j = codepointsToLoad[j_idx];
                    FT_UInt reGlyphIndex = FT_Get_Char_Index(face, j);
                    if (FT_Load_Glyph(face, reGlyphIndex, FLUXUI_FT_LOAD_FLAGS)) {
                        continue;
                    }
                    FT_GlyphSlot reSlot = face->glyph;
                    int reW = reSlot->bitmap.width;
                    int reH = reSlot->bitmap.rows;

                    if (currentX + reW + 1 >= atlasSize) {
                        currentX = 1;
                        currentY += rowHeight + 1;
                        rowHeight = 0;
                    }
                    if (currentY + reH + 1 >= atlasSize) {
                        packedOk = false;
                        break;
                    }
                    if (reH > rowHeight) {
                        rowHeight = reH;
                    }

                    for (int r = 0; r < reH; ++r) {
                        for (int c = 0; c < reW; ++c) {
                            atlas[(currentY + r) * atlasSize + (currentX + c)] = reSlot->bitmap.buffer[r * reSlot->bitmap.pitch + c];
                        }
                    }

                    GlyphInfo& gj = (j < 1024) ? font.glyphs[j] : font.extendedGlyphs[j];
                    gj.x0 = (float)currentX / atlasSize;
                    gj.y0 = (float)currentY / atlasSize;
                    gj.x1 = (float)(currentX + reW) / atlasSize;
                    gj.y1 = (float)(currentY + reH) / atlasSize;
                    gj.xoff = (float)reSlot->bitmap_left;
                    gj.yoff = (float)-reSlot->bitmap_top;
                    gj.xadvance = (float)(reSlot->advance.x >> 6);
                    gj.width = (float)reW;
                    gj.height = (float)reH;

                    font.glyphsByIndex[reGlyphIndex] = gj;

                    currentX += reW + 1;
                }
                if (!packedOk) break;
                slot = face->glyph;
                w = slot->bitmap.width;
                h = slot->bitmap.rows;
                continue;
            } else {
                packedOk = false;
                break;
            }
        }

        if (h > rowHeight) {
            rowHeight = h;
        }

        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                atlas[(currentY + r) * atlasSize + (currentX + c)] = slot->bitmap.buffer[r * slot->bitmap.pitch + c];
            }
        }

        GlyphInfo& gi = (i < 1024) ? font.glyphs[i] : font.extendedGlyphs[i];
        gi.x0 = (float)currentX / atlasSize;
        gi.y0 = (float)currentY / atlasSize;
        gi.x1 = (float)(currentX + w) / atlasSize;
        gi.y1 = (float)(currentY + h) / atlasSize;
        gi.xoff = (float)slot->bitmap_left;
        gi.yoff = (float)-slot->bitmap_top;
        gi.xadvance = (float)(slot->advance.x >> 6);
        gi.width = (float)w;
        gi.height = (float)h;

        font.glyphsByIndex[glyphIndex] = gi;

        currentX += w + 1;
    }

    if (!packedOk) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    // Cache kerning pairs:
    if (FT_HAS_KERNING(face)) {
        for (uint32_t left : codepointsToLoad) {
            FT_UInt left_glyph = FT_Get_Char_Index(face, left);
            if (left_glyph == 0) continue;
            for (uint32_t right : codepointsToLoad) {
                FT_UInt right_glyph = FT_Get_Char_Index(face, right);
                if (right_glyph == 0) continue;
                FT_Vector delta;
                if (FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_DEFAULT, &delta) == 0) {
                    if (delta.x != 0) {
                        float kernX = (float)delta.x / 64.0f;
                        uint64_t key = ((uint64_t)left << 32) | (uint64_t)right;
                        font.kerningPairs[key] = kernX;
                    }
                }
            }
        }
    }

    float sharpenStrength = std::clamp((24.0f - pixelSize) / (24.0f - 13.0f), 0.0f, 1.0f);
    if (sharpenStrength > 0.0f) {
        float sub = 0.10f * sharpenStrength;
        float mul = 1.0f + 0.30f * sharpenStrength;
        float powVal = 1.0f + 0.15f * sharpenStrength;
        for (unsigned char& alpha : atlas) {
            float a = alpha / 255.0f;
            if (a > 0.0f) {
                a = (a - sub) * mul;
                a = std::clamp(a, 0.0f, 1.0f);
                a = std::pow(a, powVal);
            }
            alpha = (unsigned char)std::round(a * 255.0f);
        }
    }

    font.fontSize = pixelSize;
    font.atlasWidth = atlasSize;
    font.atlasHeight = atlasSize;
    font.ascent = (float)face->size->metrics.ascender / 64.0f;
    font.descent = (float)face->size->metrics.descender / 64.0f;
    font.lineGap = ((float)face->size->metrics.height - (font.ascent - font.descent)) / 64.0f;

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    if (isBackground ||
        activeBackend_ == RenderBackendType::Vulkan ||
        activeBackend_ == RenderBackendType::Compatibility) {
        font.atlasPixels = std::move(atlas);
        font.textureId = 0;
    } else {
        if (font.textureId) glDeleteTextures(1, &font.textureId);
        glGenTextures(1, &font.textureId);
        glBindTexture(GL_TEXTURE_2D, font.textureId);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font.atlasWidth, font.atlasHeight,
                     0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    font.loaded = true;
    return true;
}

FontData* Renderer::getFontForSize(const std::string& fontName, float fontSize) {
    auto baseIt = fonts_.find(fontName);
    if (baseIt == fonts_.end() || !baseIt->second.loaded) return nullptr;

    int snappedSize = std::max(8, (int)std::round(fontSize * std::max(1.0f, dpiScale_)));
    int baseSize = std::max(8, (int)std::round(baseIt->second.fontSize));
    if (snappedSize == baseSize) {
        return &baseIt->second;
    }

    std::string sizedName = fontName + "@" + std::to_string(snappedSize);
    auto sizedIt = fonts_.find(sizedName);
    if (sizedIt != fonts_.end() && sizedIt->second.loaded) {
        return &sizedIt->second;
    }

    if (baseIt->second.sourceData.empty()) {
        FontData* closest = &baseIt->second;
        float bestDiff = std::abs(static_cast<float>(baseSize - snappedSize));
        std::string sizedPrefix = fontName + "@";
        for (auto& entry : fonts_) {
            if (!entry.second.loaded || entry.first.rfind(sizedPrefix, 0) != 0) {
                continue;
            }
            float diff = std::abs(entry.second.fontSize - static_cast<float>(snappedSize));
            if (diff < bestDiff) {
                bestDiff = diff;
                closest = &entry.second;
            }
        }
        return closest;
    }

    FontData sizedFont;
    const auto& source = baseIt->second.sourceData;
    if (!buildFontAtlas(sizedFont, source.data(), (int)source.size(),
                        snappedSize / std::max(1.0f, dpiScale_))) {
        return &baseIt->second;
    }

    sizedFont.sourceData.clear();
    auto [it, inserted] = fonts_.emplace(std::move(sizedName), std::move(sizedFont));
    (void)inserted;
    textMeasureCache_.clear();
    return &it->second;
}

#ifdef _WIN32
static std::string getWindowsFontPath(const std::string& filename) {
    const char* windir = std::getenv("WINDIR");
    std::string fontDir = windir ? (std::string(windir) + "\\Fonts\\") : "C:\\Windows\\Fonts\\";
    return fontDir + filename;
}
#endif

const std::string& Renderer::resolveFontName(const std::string& fontName, FontWeight weight) const {
    static const std::string s_defaultFont = "default";
    const std::string* baseName = fontName.empty() ? &s_defaultFont : &fontName;

    std::string lowerName = lowerSvgString(*baseName);
    std::string systemFontName;
    std::string regFile;
    std::string boldFile;

    if (lowerName == "sans-serif" || lowerName == "sans" || lowerName == "segoe ui" || lowerName == "segoeui") {
        systemFontName = "Segoe UI";
        regFile = "segoeui.ttf";
        boldFile = "segoeuib.ttf";
    } else if (lowerName == "monospace" || lowerName == "mono" || lowerName == "consolas") {
        systemFontName = "Consolas";
        regFile = "consola.ttf";
        boldFile = "consolab.ttf";
    } else if (lowerName == "serif" || lowerName == "times new roman" || lowerName == "times") {
        systemFontName = "Times New Roman";
        regFile = "times.ttf";
        boldFile = "timesbd.ttf";
    }

    if (!systemFontName.empty()) {
        Renderer* nonConstThis = const_cast<Renderer*>(this);
        auto it = nonConstThis->fonts_.find(systemFontName);
        if (it == nonConstThis->fonts_.end() || !it->second.loaded) {
#ifdef _WIN32
            std::string regPath = getWindowsFontPath(regFile);
            nonConstThis->loadFont(regPath, 16.0f, systemFontName);
#endif
        }
        
        if (isBoldWeight(weight)) {
            std::string boldName = systemFontName + "-bold";
            auto boldIt = nonConstThis->fonts_.find(boldName);
            if (boldIt == nonConstThis->fonts_.end() || !boldIt->second.loaded) {
#ifdef _WIN32
                std::string boldPath = getWindowsFontPath(boldFile);
                nonConstThis->loadFont(boldPath, 16.0f, boldName);
#endif
            }
            boldIt = nonConstThis->fonts_.find(boldName);
            if (boldIt != nonConstThis->fonts_.end() && boldIt->second.loaded) {
                return boldIt->first;
            }
        }
        
        it = nonConstThis->fonts_.find(systemFontName);
        if (it != nonConstThis->fonts_.end() && it->second.loaded) {
            return it->first;
        }
    }

    auto customIt = const_cast<Renderer*>(this)->customFontRegistry_.find(*baseName);
    if (customIt != const_cast<Renderer*>(this)->customFontRegistry_.end()) {
        if (!customIt->second.loaded && !customIt->second.loading) {
            const_cast<Renderer*>(this)->triggerCustomFontLoad(*baseName);
        }
        if (customIt->second.loaded) {
            if (isBoldWeight(weight)) {
                std::string boldName = *baseName + "-bold";
                auto boldIt = fonts_.find(boldName);
                if (boldIt != fonts_.end() && boldIt->second.loaded) {
                    return boldIt->first;
                }
            }
            return *baseName;
        }
    }

    auto baseIt = fonts_.find(*baseName);
    if ((baseIt == fonts_.end() || !baseIt->second.loaded) && *baseName != "default") {
        baseIt = fonts_.find(s_defaultFont);
    }
    const std::string& resolvedBase = (baseIt != fonts_.end()) ? baseIt->first : s_defaultFont;

    if (!isBoldWeight(weight)) {
        return resolvedBase;
    }

    std::string boldName = resolvedBase + "-bold";
    auto boldIt = fonts_.find(boldName);
    if (boldIt != fonts_.end() && boldIt->second.loaded) {
        return boldIt->first;
    }
    return resolvedBase;
}

const FontData* Renderer::findFontForMeasure(const std::string& fontName, float fontSize) const {
    int snappedSize = std::max(8, (int)std::round(fontSize * std::max(1.0f, dpiScale_)));
    auto baseIt = fonts_.find(fontName);
    std::string sizedName = fontName + "@" + std::to_string(snappedSize);
    auto sizedIt = fonts_.find(sizedName);
    if (sizedIt != fonts_.end() && sizedIt->second.loaded) return &sizedIt->second;

    if (baseIt != fonts_.end() && baseIt->second.loaded) {
        if (baseIt->second.sourceData.empty()) {
            const FontData* closest = &baseIt->second;
            float bestDiff = std::abs(baseIt->second.fontSize - static_cast<float>(snappedSize));
            std::string sizedPrefix = fontName + "@";
            for (const auto& entry : fonts_) {
                if (!entry.second.loaded || entry.first.rfind(sizedPrefix, 0) != 0) {
                    continue;
                }
                float diff = std::abs(entry.second.fontSize - static_cast<float>(snappedSize));
                if (diff < bestDiff) {
                    bestDiff = diff;
                    closest = &entry.second;
                }
            }
            return closest;
        }
        return &baseIt->second;
    }
    return nullptr;
}


// ============================================================
//  Drawing: Helper to set projection uniform
// ============================================================

static void setProjection(int projectionLocation, int w, int h, float scale = 1.0f, Vec2 pivot = {0,0}) {
    // Orthographic projection
    float m[16] = {
        2.0f/w, 0,      0, 0,
        0,     -2.0f/h, 0, 0,
        0,      0,     -1, 0,
       -1,      1,      0, 1
    };

    if (scale != 1.0f) {
        // Simple matrix multiply: Translate(-pivot) * Scale(s) * Translate(pivot) * m
        // We can just adjust the components for 2D
        float sx = scale;
        float sy = scale;
        float tx = pivot.x * (1.0f - sx);
        float ty = pivot.y * (1.0f - sy);

        // Adjust translation components of the projection matrix
        // m[12] is the X translation, m[13] is the Y translation
        // Screen space translation = (tx, ty) -> Projection space = (tx * 2/w, ty * -2/h)
        m[0] *= sx;
        m[5] *= sy;
        m[12] += tx * (2.0f / w);
        m[13] += ty * (-2.0f / h);
    }

    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, m);
}

// ============================================================
//  Drawing: Rounded Rectangle
// ============================================================

void Renderer::drawRoundedRect(const Rect& rect, const Color& color,
                                 const BorderRadius& radius, float opacity) {
    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::RoundedRect;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.color = color;
        cmd.radius = radius;
        cmd.opacity = opacity;
        cmd.hasGradient = false;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanRoundedRect(rect,
                              color,
                              color,
                              Color(0, 0, 0, 0),
                              radius,
                              0.0f,
                              opacity,
                              false,
                              0.0f);
        return;
    }

    // Update batch state (flush if scale/pivot changed)
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    if (batchValid_ && (batchScale_ != scale_ || batchScalePivot_.x != pivot.x || batchScalePivot_.y != pivot.y)) {
        flushRectBatch();
    }
    batchScale_ = scale_;
    batchScalePivot_ = pivot;
    batchValid_ = true;

    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };
    RoundedRectInstance inst{};
    float left = snap(rect.x + translation_.x);
    float right = snap(rect.x + rect.w + translation_.x);
    float top = snap(rect.y + translation_.y);
    float bottom = snap(rect.y + rect.h + translation_.y);
    inst.rect[0] = left;
    inst.rect[1] = top;
    inst.rect[2] = std::max(0.0f, right - left);
    inst.rect[3] = std::max(0.0f, bottom - top);
    inst.color[0] = color.r; inst.color[1] = color.g; inst.color[2] = color.b; inst.color[3] = color.a;
    inst.color2[0] = color.r; inst.color2[1] = color.g; inst.color2[2] = color.b; inst.color2[3] = color.a;
    inst.borderColor[0] = 0; inst.borderColor[1] = 0; inst.borderColor[2] = 0; inst.borderColor[3] = 0;
    inst.radius = radius.uniform();
    inst.borderWidth = 0;
    inst.opacity = opacity;
    inst.hasGradient = 0;
    inst.gradientAngle = 0;
    rectBatch_.push_back(inst);
}

void Renderer::drawRoundedRectGradient(const Rect& rect, const Gradient& gradient,
                                        const BorderRadius& radius, float opacity) {
    if (gradient.stops.size() < 2) return;

    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::RoundedRect;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.radius = radius;
        cmd.opacity = opacity;
        cmd.hasGradient = true;
        cmd.gradient = gradient;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanRoundedRect(rect,
                              gradient.stops.front().first,
                              gradient.stops.back().first,
                              Color(0, 0, 0, 0),
                              radius,
                              0.0f,
                              opacity,
                              true,
                              gradient.angle);
        return;
    }

    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    if (batchValid_ && (batchScale_ != scale_ || batchScalePivot_.x != pivot.x || batchScalePivot_.y != pivot.y)) {
        flushRectBatch();
    }
    batchScale_ = scale_;
    batchScalePivot_ = pivot;
    batchValid_ = true;

    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };
    auto& c1 = gradient.stops[0].first;
    auto& c2 = gradient.stops.back().first;

    RoundedRectInstance inst{};
    float left = snap(rect.x + translation_.x);
    float right = snap(rect.x + rect.w + translation_.x);
    float top = snap(rect.y + translation_.y);
    float bottom = snap(rect.y + rect.h + translation_.y);
    inst.rect[0] = left;
    inst.rect[1] = top;
    inst.rect[2] = std::max(0.0f, right - left);
    inst.rect[3] = std::max(0.0f, bottom - top);
    inst.color[0] = c1.r; inst.color[1] = c1.g; inst.color[2] = c1.b; inst.color[3] = c1.a;
    inst.color2[0] = c2.r; inst.color2[1] = c2.g; inst.color2[2] = c2.b; inst.color2[3] = c2.a;
    inst.borderColor[0] = 0; inst.borderColor[1] = 0; inst.borderColor[2] = 0; inst.borderColor[3] = 0;
    inst.radius = radius.uniform();
    inst.borderWidth = 0;
    inst.opacity = opacity;
    inst.hasGradient = 1.0f;
    inst.gradientAngle = gradient.angle;
    rectBatch_.push_back(inst);
}

void Renderer::drawBorder(const Rect& rect, const Border& border, const BorderRadius& radius) {
    if (border.width <= 0) return;

    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Border;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.border = border;
        cmd.radius = radius;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanRoundedRect(rect,
                              Color(0, 0, 0, 0),
                              Color(0, 0, 0, 0),
                              border.color,
                              radius,
                              border.width,
                              1.0f,
                              false,
                              0.0f);
        return;
    }

    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };

    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    if (batchValid_ && (batchScale_ != scale_ || batchScalePivot_.x != pivot.x || batchScalePivot_.y != pivot.y)) {
        flushRectBatch();
    }
    batchScale_ = scale_;
    batchScalePivot_ = pivot;
    batchValid_ = true;

    RoundedRectInstance inst{};
    float left = snap(rect.x + translation_.x);
    float right = snap(rect.x + rect.w + translation_.x);
    float top = snap(rect.y + translation_.y);
    float bottom = snap(rect.y + rect.h + translation_.y);
    inst.rect[0] = left;
    inst.rect[1] = top;
    inst.rect[2] = std::max(0.0f, right - left);
    inst.rect[3] = std::max(0.0f, bottom - top);
    inst.color[0] = 0; inst.color[1] = 0; inst.color[2] = 0; inst.color[3] = 0;
    inst.color2[0] = 0; inst.color2[1] = 0; inst.color2[2] = 0; inst.color2[3] = 0;
    inst.borderColor[0] = border.color.r; inst.borderColor[1] = border.color.g;
    inst.borderColor[2] = border.color.b; inst.borderColor[3] = border.color.a;
    inst.radius = radius.uniform();
    inst.borderWidth = border.width;
    inst.opacity = 1.0f;
    inst.hasGradient = 0;
    inst.gradientAngle = 0;
    rectBatch_.push_back(inst);
}

void Renderer::drawBoxShadow(const Rect& rect, const BoxShadow& shadow,
                              const BorderRadius& radius) {
    if (shadow.blur <= 0 && shadow.spread <= 0) return;

    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::BoxShadow;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.shadow = shadow;
        cmd.radius = radius;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanBoxShadow(rect, shadow, radius);
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        drawSoftwareBoxShadow(rect, shadow, radius);
        return;
    }

    float expand = shadow.blur + shadow.spread;
    Rect shadowRect = {
        rect.x + shadow.offsetX - expand + translation_.x,
        rect.y + shadow.offsetY - expand + translation_.y,
        rect.w + expand * 2,
        rect.h + expand * 2
    };

    useShader(shadowShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    setProjection(shadowUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    // Flush rect batch before drawing shadow (different shader)
    flushRectBatch();

    glUniform4f(shadowUniforms_.rect,
                shadowRect.x, shadowRect.y, shadowRect.w, shadowRect.h);
    glUniform4f(shadowUniforms_.color,
                shadow.color.r, shadow.color.g, shadow.color.b, shadow.color.a);
    glUniform1f(shadowUniforms_.radius, radius.uniform());
    glUniform1f(shadowUniforms_.blur, shadow.blur);
    glUniform1f(shadowUniforms_.opacity, 1.0f);
    glUniform2f(shadowUniforms_.boxSize, rect.w + shadow.spread * 2, rect.h + shadow.spread * 2);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

FontData::FontData() = default;

FontData::~FontData() {
    cleanupHarfbuzz();
}

FontData::FontData(FontData&& other) noexcept {
    *this = std::move(other);
}

FontData::FontData(const FontData& other) {
    *this = other;
}

FontData& FontData::operator=(const FontData& other) {
    if (this != &other) {
        cleanupHarfbuzz();

        textureId = other.textureId;
        atlasWidth = other.atlasWidth;
        atlasHeight = other.atlasHeight;
        fontSize = other.fontSize;
        ascent = other.ascent;
        descent = other.descent;
        lineGap = other.lineGap;
        std::copy(std::begin(other.glyphs), std::end(other.glyphs), std::begin(glyphs));
        extendedGlyphs = other.extendedGlyphs;
        kerningPairs = other.kerningPairs;
        supportedLigatures = other.supportedLigatures;
        glyphsByIndex = other.glyphsByIndex;

        hbFont = nullptr;
        ftLibrary = nullptr;
        ftFace = nullptr;

        sourceData = other.sourceData;
        atlasPixels = other.atlasPixels;
        loaded = other.loaded;
        runLengths = other.runLengths;
        runValues = other.runValues;
        runCount = other.runCount;
        pixelCount = other.pixelCount;
    }
    return *this;
}

FontData& FontData::operator=(FontData&& other) noexcept {
    if (this != &other) {
        cleanupHarfbuzz();
        
        textureId = other.textureId;
        atlasWidth = other.atlasWidth;
        atlasHeight = other.atlasHeight;
        fontSize = other.fontSize;
        ascent = other.ascent;
        descent = other.descent;
        lineGap = other.lineGap;
        std::copy(std::begin(other.glyphs), std::end(other.glyphs), std::begin(glyphs));
        extendedGlyphs = std::move(other.extendedGlyphs);
        kerningPairs = std::move(other.kerningPairs);
        supportedLigatures = std::move(other.supportedLigatures);
        glyphsByIndex = std::move(other.glyphsByIndex);
        
        hbFont = other.hbFont;
        ftLibrary = other.ftLibrary;
        ftFace = other.ftFace;
        
        other.hbFont = nullptr;
        other.ftLibrary = nullptr;
        other.ftFace = nullptr;
        
        sourceData = std::move(other.sourceData);
        atlasPixels = std::move(other.atlasPixels);
        loaded = other.loaded;
        runLengths = other.runLengths;
        runValues = other.runValues;
        runCount = other.runCount;
        pixelCount = other.pixelCount;
    }
    return *this;
}

void FontData::cleanupHarfbuzz() const {
    if (hbFont) {
        hb_font_destroy((hb_font_t*)hbFont);
        hbFont = nullptr;
    }
    if (ftFace) {
        FT_Done_Face((FT_Face)ftFace);
        ftFace = nullptr;
    }
    if (ftLibrary) {
        FT_Done_FreeType((FT_Library)ftLibrary);
        ftLibrary = nullptr;
    }
}

void Renderer::drawImage(const std::string& nameOrPath, const Rect& rect,
                         float opacity, const Color& tint) {
    drawImage(nameOrPath, rect, Rect(0.0f, 0.0f, 1.0f, 1.0f), opacity, tint);
}

void Renderer::drawImage(const std::string& nameOrPath, const Rect& rect,
                         const Rect& sourceUv,
                         float opacity, const Color& tint) {
    if (nameOrPath.empty() || rect.w <= 0.0f || rect.h <= 0.0f ||
        opacity <= 0.0f || tint.a <= 0.0f) {
        return;
    }

    auto it = images_.find(nameOrPath);
    if (it == images_.end() || !it->second.loaded) {
        if (!loadImage(nameOrPath)) return;
        it = images_.find(nameOrPath);
        if (it == images_.end() || !it->second.loaded) return;
    }

    Color finalTint = tint;
    if (!it->second.svg) {
        finalTint = Color(1.0f, 1.0f, 1.0f, tint.a);
    }

    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::TexturedQuad;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.text = nameOrPath;
        cmd.sourceUv = sourceUv;
        cmd.opacity = opacity;
        cmd.color = finalTint;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanImage(nameOrPath, it->second, rect, sourceUv, finalTint, opacity);
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        drawSoftwareImage(nameOrPath, it->second, rect, sourceUv, finalTint, opacity);
        return;
    }

    if (!ensureImageTexture(nameOrPath, it->second) || it->second.textureId == 0) {
        return;
    }

    Rect drawRect = {
        rect.x + translation_.x,
        rect.y + translation_.y,
        rect.w,
        rect.h
    };
    if (scale_ != 1.0f) {
        Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
        drawRect.x = pivot.x + (drawRect.x - pivot.x) * scale_;
        drawRect.y = pivot.y + (drawRect.y - pivot.y) * scale_;
        drawRect.w *= scale_;
        drawRect.h *= scale_;
    }

    float a = finalTint.a * opacity;
    float u0 = std::clamp(sourceUv.x, 0.0f, 1.0f);
    float v0 = std::clamp(sourceUv.y, 0.0f, 1.0f);
    float u1 = std::clamp(sourceUv.x + sourceUv.w, 0.0f, 1.0f);
    float v1 = std::clamp(sourceUv.y + sourceUv.h, 0.0f, 1.0f);
    float vertices[] = {
        drawRect.x,              drawRect.y,              u0, v0, finalTint.r, finalTint.g, finalTint.b, a,
        drawRect.x + drawRect.w, drawRect.y,              u1, v0, finalTint.r, finalTint.g, finalTint.b, a,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, u1, v1, finalTint.r, finalTint.g, finalTint.b, a,
        drawRect.x,              drawRect.y,              u0, v0, finalTint.r, finalTint.g, finalTint.b, a,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, u1, v1, finalTint.r, finalTint.g, finalTint.b, a,
        drawRect.x,              drawRect.y + drawRect.h, u0, v1, finalTint.r, finalTint.g, finalTint.b, a,
    };

    flushRectBatch();
    useShader(imageShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
    setProjection(imageUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    glBindVertexArray(textVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO_);
    if (48 > textVBOCapacity_) {
        textVBOCapacity_ = 48;
        glBufferData(GL_ARRAY_BUFFER, textVBOCapacity_ * sizeof(float), nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, it->second.textureId);
    glUniform1i(imageUniforms_.texture, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawTextInRect(const std::string& text, const Rect& rect, const Color& color,
                               float fontSize, TextAlign align, FontWeight weight,
                               const std::string& fontName,
                               FontStyle style,
                               Direction direction,
                               UnicodeBidi unicodeBidi) {
    // Paint CSS text-shadow layers first (back-to-front), then the glyphs.
    if (!activeTextShadows_.empty()) {
        std::vector<TextShadow> shadows;
        shadows.swap(activeTextShadows_);   // disable re-entrancy during layers
        for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
            Rect shadowRect = rect;
            shadowRect.x += it->offsetX;
            shadowRect.y += it->offsetY;
            drawTextInRect(text, shadowRect, it->color, fontSize, align, weight,
                           fontName, style, direction, unicodeBidi);
        }
        activeTextShadows_.swap(shadows);   // restore for any sibling text
    }
    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Text;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.text = text;
        cmd.color = color;
        cmd.fontSize = fontSize;
        cmd.textAlign = align;
        cmd.fontWeight = weight;
        cmd.fontName = fontName;
        cmd.fontStyle = style;
        cmd.fontDirection = direction;
        cmd.unicodeBidi = unicodeBidi;
        recordCommand(std::move(cmd));
        return;
    }

    const std::string& resolvedFontName = resolveFontName(fontName, weight);
    FontData* fontForRect = getFontForSize(resolvedFontName, fontSize);

    TextAlign effectiveAlign = align;
    if (direction == Direction::Rtl && align == TextAlign::Left) {
        effectiveAlign = TextAlign::Right;
    }

    float x = rect.x;
    float measuredW = 0.0f;
    if (effectiveAlign != TextAlign::Left) {
        Vec2 textSize = measureText(text, fontSize, resolvedFontName);
        measuredW = textSize.x;
        if (effectiveAlign == TextAlign::Center) x = rect.x + (rect.w - textSize.x) / 2;
        else if (effectiveAlign == TextAlign::Right) x = rect.x + rect.w - textSize.x;
    }

    // Vertically center: position so that the text midpoint aligns with rect midpoint
    // textSize.y is the fontSize; the actual visual height is ascent+|descent|
    float textH = fontSize; // fallback
    const FontData* font = fontForRect ? fontForRect : findFontForMeasure(resolvedFontName, fontSize);
    if (font && font->loaded) {
        float scale = fontSize / font->fontSize;
        float asc = font->ascent * scale;
        float desc = std::abs(font->descent * scale);
        textH = asc + desc;
    }
    float y = rect.y + (rect.h - textH) / 2;
    drawText(text, {x, y}, color, fontSize, weight, fontName, style, direction, unicodeBidi);
}

Vec2 Renderer::measureText(const std::string& text, float fontSize,
                            const std::string& fontName) const {
    const std::string& resolvedFontName = resolveFontName(fontName, FontWeight::Normal);
#if FLUXUI_TEXT_MEASURE_CACHE_SIZE > 0
    uint64_t key = 14695981039346656037ULL;
    for (char c : resolvedFontName) {
        key ^= static_cast<uint8_t>(c);
        key *= 1099511628211ULL;
    }
    {
        const char* bytes = reinterpret_cast<const char*>(&fontSize);
        for (size_t i = 0; i < sizeof(float); ++i) {
            key ^= static_cast<uint8_t>(bytes[i]);
            key *= 1099511628211ULL;
        }
    }
    for (char c : text) {
        key ^= static_cast<uint8_t>(c);
        key *= 1099511628211ULL;
    }

    auto cached = textMeasureCache_.find(key);
    if (cached != textMeasureCache_.end()) {
        return cached->second;
    }
#endif

    const FontData* fontPtr = findFontForMeasure(resolvedFontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) return {0, fontSize};
    auto& font = *fontPtr;
    float logicalFontHeight = font.fontSize / std::max(1.0f, dpiScale_);
    float scale;
    if (std::abs(fontSize - logicalFontHeight) < 1.01f) {
        scale = 1.0f / std::max(1.0f, dpiScale_);
    } else {
        scale = fontSize / font.fontSize;
    }
    float width = 0.0f;
    ShapedRun run = shapeTextWithHarfbuzz(font, text, Direction::Ltr);
    for (const auto& sg : run) {
        const auto& g = (font.hbFont) ? font.getGlyphByIndex(sg.glyphIndex) : font.getGlyph(sg.codepoint);
        if (g.xadvance == 0 && sg.codepoint != ' ') continue;

        width += sg.xAdvance * scale;
    }
    Vec2 measured = {width, fontSize};
#if FLUXUI_TEXT_MEASURE_CACHE_SIZE > 0
    if (textMeasureCache_.size() >= FLUXUI_TEXT_MEASURE_CACHE_SIZE) {
        textMeasureCache_.clear();
    }
    textMeasureCache_[key] = measured;
#endif
    return measured;
}

// ============================================================
//  Scissor (Clipping)
// ============================================================

void Renderer::pushScissor(const Rect& rect) {
    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Scissor;
        cmd.scissorRect = rect;
        cmd.scissorRect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.scissorRect.y += (translation_.y - recordingTranslationStart_.y);
        recordCommand(std::move(cmd));
        return;
    }

    Rect clip = {
        rect.x + translation_.x,
        rect.y + translation_.y,
        rect.w,
        rect.h
    };

    if (!scissorStack_.empty()) {
        const Rect& parent = scissorStack_.back();
        float x1 = std::max(clip.x, parent.x);
        float y1 = std::max(clip.y, parent.y);
        float x2 = std::min(clip.x + clip.w, parent.x + parent.w);
        float y2 = std::min(clip.y + clip.h, parent.y + parent.h);
        clip = {x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1)};
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
#if FLUXUI_HAS_VULKAN_SDK
        if (vulkan_) {
            flushVulkanBatches(*vulkan_, scissorStack_, dpiScale_);
            scissorStack_.push_back(clip);
            setVulkanScissor(*vulkan_, scissorStack_, dpiScale_);
        }
#endif
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        flushRectBatch();
        scissorStack_.push_back(clip);
        return;
    }

    // Flush rect batch before scissor change
    flushRectBatch();

    scissorStack_.push_back(clip);
    glEnable(GL_SCISSOR_TEST);

    int x = (int)std::floor(clip.x * dpiScale_);
    int y = (int)std::floor((windowHeight_ - clip.y - clip.h) * dpiScale_);
    int w = (int)std::ceil(clip.w * dpiScale_);
    int h = (int)std::ceil(clip.h * dpiScale_);
    glScissor(x, y, std::max(0, w), std::max(0, h));
}

void Renderer::popScissor() {
    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::ScissorPop;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
#if FLUXUI_HAS_VULKAN_SDK
        if (vulkan_) {
            flushVulkanBatches(*vulkan_, scissorStack_, dpiScale_);
            if (!scissorStack_.empty()) scissorStack_.pop_back();
            setVulkanScissor(*vulkan_, scissorStack_, dpiScale_);
        }
#endif
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        flushRectBatch();
        if (!scissorStack_.empty()) scissorStack_.pop_back();
        return;
    }

    // Flush rect batch before scissor change
    flushRectBatch();

    if (!scissorStack_.empty()) scissorStack_.pop_back();
    if (scissorStack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        auto& r = scissorStack_.back();
        int x = (int)std::floor(r.x * dpiScale_);
        int y = (int)std::floor((windowHeight_ - r.y - r.h) * dpiScale_);
        int w = (int)std::ceil(r.w * dpiScale_);
        int h = (int)std::ceil(r.h * dpiScale_);
        glScissor(x, y, std::max(0, w), std::max(0, h));
    }
}
void Renderer::pushTranslation(const Vec2& offset) {
    translationStack_.push_back(translation_);
    translation_ = { translation_.x + offset.x, translation_.y + offset.y };
}

void Renderer::popTranslation() {
    if (!translationStack_.empty()) {
        translation_ = translationStack_.back();
        translationStack_.pop_back();
    }
}

void Renderer::pushScale(float scale, const Vec2& pivot) {
    scaleStack_.push_back(scale_);
    scalePivotStack_.push_back(pivot);
    scale_ = scale_ * scale;
}

void Renderer::popScale() {
    if (!scaleStack_.empty()) {
        scale_ = scaleStack_.back();
        scaleStack_.pop_back();
        scalePivotStack_.pop_back();
    }
}

void Renderer::playback(const std::vector<RenderCommand>& commands) {
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case RenderCommandType::RoundedRect:
                if (cmd.hasGradient) {
                    drawRoundedRectGradient(cmd.rect, cmd.gradient, cmd.radius, cmd.opacity);
                } else {
                    drawRoundedRect(cmd.rect, cmd.color, cmd.radius, cmd.opacity);
                }
                break;
            case RenderCommandType::Border:
                drawBorder(cmd.rect, cmd.border, cmd.radius);
                break;
            case RenderCommandType::BoxShadow:
                drawBoxShadow(cmd.rect, cmd.shadow, cmd.radius);
                break;
            case RenderCommandType::BackdropFilterBlur:
                drawBackdropFilterBlur(cmd.rect, cmd.blurRadius, cmd.radius);
                break;
            case RenderCommandType::Text:
                if (cmd.rect.w > 0.0f || cmd.rect.h > 0.0f) {
                    drawTextInRect(cmd.text, cmd.rect, cmd.color, cmd.fontSize, cmd.textAlign,
                                   cmd.fontWeight, cmd.fontName, cmd.fontStyle, cmd.fontDirection, cmd.unicodeBidi);
                } else {
                    drawText(cmd.text, Vec2(cmd.rect.x, cmd.rect.y), cmd.color, cmd.fontSize, cmd.fontWeight,
                             cmd.fontName, cmd.fontStyle, cmd.fontDirection, cmd.unicodeBidi);
                }
                break;
            case RenderCommandType::TexturedQuad:
                if (cmd.sourceUv.w > 0.0f || cmd.sourceUv.h > 0.0f) {
                    drawImage(cmd.text, cmd.rect, cmd.sourceUv, cmd.opacity, cmd.color);
                } else {
                    drawImage(cmd.text, cmd.rect, cmd.opacity, cmd.color);
                }
                break;
            case RenderCommandType::Scissor:
                pushScissor(cmd.scissorRect);
                break;
            case RenderCommandType::ScissorPop:
                popScissor();
                break;
        }
    }
}

void Renderer::playback(const std::vector<RenderCommand>& commands, float opacityScale) {
    for (auto cmd : commands) {
        cmd.opacity *= opacityScale;
        cmd.color.a *= opacityScale;
        switch (cmd.type) {
            case RenderCommandType::RoundedRect:
                if (cmd.hasGradient) {
                    drawRoundedRectGradient(cmd.rect, cmd.gradient, cmd.radius, cmd.opacity);
                } else {
                    drawRoundedRect(cmd.rect, cmd.color, cmd.radius, cmd.opacity);
                }
                break;
            case RenderCommandType::Border:
                cmd.border.color.a *= opacityScale;
                drawBorder(cmd.rect, cmd.border, cmd.radius);
                break;
            case RenderCommandType::BoxShadow:
                cmd.shadow.color.a *= opacityScale;
                drawBoxShadow(cmd.rect, cmd.shadow, cmd.radius);
                break;
            case RenderCommandType::BackdropFilterBlur:
                drawBackdropFilterBlur(cmd.rect, cmd.blurRadius, cmd.radius);
                break;
            case RenderCommandType::Text:
                if (cmd.rect.w > 0.0f || cmd.rect.h > 0.0f) {
                    drawTextInRect(cmd.text, cmd.rect, cmd.color, cmd.fontSize, cmd.textAlign,
                                   cmd.fontWeight, cmd.fontName, cmd.fontStyle, cmd.fontDirection, cmd.unicodeBidi);
                } else {
                    drawText(cmd.text, Vec2(cmd.rect.x, cmd.rect.y), cmd.color, cmd.fontSize, cmd.fontWeight,
                             cmd.fontName, cmd.fontStyle, cmd.fontDirection, cmd.unicodeBidi);
                }
                break;
            case RenderCommandType::TexturedQuad:
                if (cmd.sourceUv.w > 0.0f || cmd.sourceUv.h > 0.0f) {
                    drawImage(cmd.text, cmd.rect, cmd.sourceUv, cmd.opacity, cmd.color);
                } else {
                    drawImage(cmd.text, cmd.rect, cmd.opacity, cmd.color);
                }
                break;
            case RenderCommandType::Scissor:
                pushScissor(cmd.scissorRect);
                break;
            case RenderCommandType::ScissorPop:
                popScissor();
                break;
        }
    }
}

void Renderer::pushRenderTarget(uint32_t fbo, int width, int height) {
    if (activeBackend_ == RenderBackendType::Compatibility || glad_glBindFramebuffer == nullptr) {
        return;
    }
    flushRectBatch();
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    renderTargetStack_.push_back({(uint32_t)prevFbo, windowWidth_, windowHeight_});
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    windowWidth_ = width;
    windowHeight_ = height;
    glViewport(0, 0, width, height);
}

void Renderer::popRenderTarget() {
    if (activeBackend_ == RenderBackendType::Compatibility || glad_glBindFramebuffer == nullptr) {
        return;
    }
    flushRectBatch();
    if (!renderTargetStack_.empty()) {
        auto prev = renderTargetStack_.back();
        renderTargetStack_.pop_back();
        glBindFramebuffer(GL_FRAMEBUFFER, prev.fbo);
        windowWidth_ = prev.width;
        windowHeight_ = prev.height;
        glViewport(0, 0, prev.width, prev.height);
    }
}

void Renderer::drawTexture(uint32_t textureId, const Rect& rect, float opacity) {
    if (textureId == 0 || rect.w <= 0.0f || rect.h <= 0.0f || opacity <= 0.0f) {
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility || glad_glBindTexture == nullptr) {
        return;
    }

    Rect drawRect = {
        rect.x + translation_.x,
        rect.y + translation_.y,
        rect.w,
        rect.h
    };
    if (scale_ != 1.0f) {
        Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
        drawRect.x = pivot.x + (drawRect.x - pivot.x) * scale_;
        drawRect.y = pivot.y + (drawRect.y - pivot.y) * scale_;
        drawRect.w *= scale_;
        drawRect.h *= scale_;
    }

    float a = opacity;
    float u0 = 0.0f;
    float v0 = 1.0f;
    float u1 = 1.0f;
    float v1 = 0.0f;

    float vertices[] = {
        drawRect.x,              drawRect.y,              u0, v0, 1.0f, 1.0f, 1.0f, a,
        drawRect.x + drawRect.w, drawRect.y,              u1, v0, 1.0f, 1.0f, 1.0f, a,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, u1, v1, 1.0f, 1.0f, 1.0f, a,
        drawRect.x,              drawRect.y,              u0, v0, 1.0f, 1.0f, 1.0f, a,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, u1, v1, 1.0f, 1.0f, 1.0f, a,
        drawRect.x,              drawRect.y + drawRect.h, u0, v1, 1.0f, 1.0f, 1.0f, a,
    };

    flushRectBatch();
    useShader(imageShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
    setProjection(imageUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    glBindVertexArray(textVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO_);
    if (48 > textVBOCapacity_) {
        textVBOCapacity_ = 48;
        glBufferData(GL_ARRAY_BUFFER, textVBOCapacity_ * sizeof(float), nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(imageUniforms_.texture, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

} // namespace FluxUI
