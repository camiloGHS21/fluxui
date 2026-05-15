// FluxUI Renderer - GPU-accelerated rendering
#include "fluxui/renderer.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifndef FLUXUI_HAS_VULKAN_SDK
#define FLUXUI_HAS_VULKAN_SDK 0
#endif

#if FLUXUI_HAS_VULKAN_SDK && defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

#include <glad/gl.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#if FLUXUI_HAS_VULKAN_SDK
#include <vulkan/vulkan.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <SDL_syswm.h>
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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

#ifndef FLUXUI_ENABLE_MSAA
#define FLUXUI_ENABLE_MSAA 1
#endif

#ifndef FLUXUI_ENABLE_VSYNC
#define FLUXUI_ENABLE_VSYNC 1
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

struct VulkanRendererState {
#if FLUXUI_HAS_VULKAN_SDK
    struct DynamicPage {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        size_t capacityBytes = 0;
        size_t bytesUsed = 0;
        void* mapped = nullptr;
    };

    struct DynamicPool {
        std::vector<DynamicPage> pages;
        size_t currentPage = 0;
    };

    struct Frame {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
        DynamicPool roundedInstances;
        DynamicPool textVertices;
    };

    struct FontTexture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        int width = 0;
        int height = 0;
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    uint32_t presentQueueFamily = std::numeric_limits<uint32_t>::max();

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent = {};
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<Frame> frames;

    VkPipelineLayout roundedPipelineLayout = VK_NULL_HANDLE;
    VkPipeline roundedPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout textDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout textPipelineLayout = VK_NULL_HANDLE;
    VkPipeline textPipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkSampler fontSampler = VK_NULL_HANDLE;
    std::unordered_map<std::string, FontTexture> fontTextures;

    size_t roundedBatchOffsetBytes = 0;
    size_t roundedBatchPageIndex = 0;
    uint32_t roundedBatchCount = 0;

    size_t textBatchOffsetBytes = 0;
    size_t textBatchPageIndex = 0;
    uint32_t textBatchVertexCount = 0;
    VkDescriptorSet textBatchDescriptorSet = VK_NULL_HANDLE;

    uint32_t currentFrame = 0;
    uint32_t currentImage = 0;
    bool frameActive = false;
    bool swapchainDirty = false;
    int logicalWidth = 0;
    int logicalHeight = 0;
#endif
};

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
            "Internal compatibility draw path used while native backends are staged"
        };
    }

    uint32_t sdlWindowFlags() const override {
        return SDL_WINDOW_OPENGL;
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

    uint32_t sdlWindowFlags() const override {
#if defined(_WIN32)
        return 0;
#else
        return SDL_WINDOW_VULKAN;
#endif
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

    uint32_t sdlWindowFlags() const override {
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

    uint32_t sdlWindowFlags() const override {
#ifdef SDL_WINDOW_METAL
        return SDL_WINDOW_METAL;
#else
        return 0;
#endif
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

    return RenderBackendType::Compatibility;
}

RenderBackendType chooseBackend(RenderBackendType preference) {
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

    if (!info.compiled) {
        std::cerr << "FluxUI: " << info.name
                  << " backend was requested but is not compiled in this build. Using compatibility renderer." << std::endl;
    } else {
        std::cerr << "FluxUI: " << info.name
                  << " backend is staged but not selectable yet. Using compatibility renderer." << std::endl;
    }

    return RenderBackendType::Compatibility;
}

} // namespace

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

#if FLUXUI_HAS_VULKAN_SDK
static const char* VULKAN_ROUNDED_VERT = R"(
#version 450
layout(push_constant) uniform Push {
    vec2 framebufferSize;
} pc;
layout(location = 0) in vec4 inRect;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inColor2;
layout(location = 3) in vec4 inBorderColor;
layout(location = 4) in vec4 inRadius;
layout(location = 5) in vec4 inParams;
layout(location = 0) out vec2 vLocalPos;
layout(location = 1) out vec2 vSize;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec4 vColor2;
layout(location = 4) out vec4 vBorderColor;
layout(location = 5) out vec4 vRadius;
layout(location = 6) out vec4 vParams;
void main() {
    vec2 verts[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );
    vec2 local = verts[gl_VertexIndex] * inRect.zw;
    vec2 pos = inRect.xy + local;
    vec2 ndc = vec2(
        (pos.x / pc.framebufferSize.x) * 2.0 - 1.0,
        (pos.y / pc.framebufferSize.y) * 2.0 - 1.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    vLocalPos = local;
    vSize = inRect.zw;
    vColor = inColor;
    vColor2 = inColor2;
    vBorderColor = inBorderColor;
    vRadius = inRadius;
    vParams = inParams;
}
)";

static const char* VULKAN_ROUNDED_FRAG = R"(
#version 450
layout(push_constant) uniform Push {
    vec2 framebufferSize;
} pc;
layout(location = 0) in vec2 vLocalPos;
layout(location = 1) in vec2 vSize;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec4 vColor2;
layout(location = 4) in vec4 vBorderColor;
layout(location = 5) in vec4 vRadius;
layout(location = 6) in vec4 vParams;
layout(location = 0) out vec4 outColor;

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 halfSize = vSize * 0.5;
    vec2 center = vLocalPos - halfSize;
    float r = min(vRadius.x, min(halfSize.x, halfSize.y));
    float borderWidth = vParams.x;
    float opacity = vParams.y;
    float hasGradient = vParams.z;
    float gradientAngle = vParams.w;

    if (hasGradient < -0.5) {
        float blur = max(borderWidth, 0.001);
        vec2 halfBox = max(halfSize - vec2(blur), vec2(0.0));
        float shadowRadius = min(r, min(halfBox.x, halfBox.y));
        float shadowDist = roundedBoxSDF(center, halfBox, shadowRadius);
        float shadowAlpha = 1.0 - smoothstep(0.0, blur, shadowDist);
        outColor = vec4(vColor.rgb, vColor.a * shadowAlpha * opacity);
        return;
    }

    float dist = roundedBoxSDF(center, halfSize, r);
    float aa = max(fwidth(dist), 0.75);
    float alpha = 1.0 - smoothstep(-aa, aa, dist);

    vec4 fillColor = vColor;
    if (hasGradient > 0.5) {
        float angle = radians(gradientAngle);
        vec2 dir = vec2(cos(angle), sin(angle));
        float t = dot((vLocalPos / max(vSize, vec2(1.0))) - vec2(0.5), dir) + 0.5;
        fillColor = mix(vColor, vColor2, clamp(t, 0.0, 1.0));
    }

    if (borderWidth > 0.0) {
        vec2 innerHalf = max(halfSize - vec2(borderWidth), vec2(0.0));
        float innerRadius = max(r - borderWidth, 0.0);
        float innerDist = roundedBoxSDF(center, innerHalf, innerRadius);
        float innerAa = max(fwidth(innerDist), 0.75);
        float innerAlpha = 1.0 - smoothstep(-innerAa, innerAa, innerDist);
        fillColor = mix(vBorderColor, fillColor, innerAlpha);
    }

    outColor = vec4(fillColor.rgb, fillColor.a * alpha * opacity);
}
)";

static const char* VULKAN_TEXT_VERT = R"(
#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(push_constant) uniform Push {
    vec2 framebufferSize;
} pc;
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
void main() {
    vec2 ndc = vec2(
        (inPos.x / pc.framebufferSize.x) * 2.0 - 1.0,
        (inPos.y / pc.framebufferSize.y) * 2.0 - 1.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUv = inUv;
    vColor = inColor;
}
)";

static const char* VULKAN_TEXT_FRAG = R"(
#version 450
layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;
void main() {
    float a = texture(uTexture, vUv).r;
    outColor = vec4(vColor.rgb, vColor.a * a);
}
)";
#endif

// ============================================================
//  Shader Compilation
// ============================================================

Renderer::Renderer() = default;
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

uint32_t Renderer::sdlWindowFlags() const {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        return vulkanBackendInfo().sdlWindowFlags();
    }
    if (activeBackend_ == RenderBackendType::Direct3D12) {
        return direct3D12BackendInfo().sdlWindowFlags();
    }
    if (activeBackend_ == RenderBackendType::Metal) {
        return metalBackendInfo().sdlWindowFlags();
    }
    return compatibilityBackendInfo().sdlWindowFlags();
}

RenderBackendInfo Renderer::getBackendInfo(RenderBackendType backend) {
    if (backend == RenderBackendType::Vulkan) {
        return vulkanBackendInfo().info();
    }
    if (backend == RenderBackendType::Direct3D12) {
        return direct3D12BackendInfo().info();
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
#else
    return RenderBackendType::Auto;
#endif
}

VulkanProbeResult Renderer::probeVulkanRuntime(const char* appName) {
    VulkanProbeResult result;

    const bool videoAlreadyInitialized = (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0;
    bool loadedLibrary = false;
    bool loadedWithSDL = false;
#ifdef _WIN32
    HMODULE vulkanModule = nullptr;
#endif
    SDL_Window* window = nullptr;
    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;

    auto finish = [&]() {
        if (surface && vkDestroySurfaceKHR && instance) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        if (instance && vkDestroyInstance) {
            vkDestroyInstance(instance, nullptr);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }
        if (loadedWithSDL) {
            SDL_Vulkan_UnloadLibrary();
        }
#ifdef _WIN32
        if (vulkanModule) {
            FreeLibrary(vulkanModule);
        }
#endif
        if (!videoAlreadyInitialized) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        return result;
    };

    if (!videoAlreadyInitialized && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        result.error = std::string("SDL video init failed: ") + SDL_GetError();
        return finish();
    }

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    std::string sdlLoadError;
    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        loadedLibrary = true;
        loadedWithSDL = true;
        vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            SDL_Vulkan_GetVkGetInstanceProcAddr());
    } else {
        sdlLoadError = SDL_GetError();
#ifdef _WIN32
        vulkanModule = LoadLibraryA("vulkan-1.dll");
        if (vulkanModule) {
            loadedLibrary = true;
            vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                GetProcAddress(vulkanModule, "vkGetInstanceProcAddr"));
        }
#endif
    }

    if (!loadedLibrary || !vkGetInstanceProcAddr) {
        result.error = "Could not load Vulkan loader";
        if (!sdlLoadError.empty()) {
            result.error += ": " + sdlLoadError;
        }
        return finish();
    }
    result.loaderAvailable = true;

    window = SDL_CreateWindow(appName ? appName : "FluxUI Vulkan Probe",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              64,
                              64,
                              (loadedWithSDL ? SDL_WINDOW_VULKAN : 0) | SDL_WINDOW_HIDDEN);
    if (!window) {
        result.error = std::string("SDL could not create a Vulkan probe window: ") + SDL_GetError();
        return finish();
    }
    result.windowCreated = true;

    unsigned int extensionCount = 0;
    std::vector<const char*> extensions;
    if (loadedWithSDL) {
        if (SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr) != SDL_TRUE) {
            result.error = std::string("SDL could not query Vulkan instance extensions: ") + SDL_GetError();
            return finish();
        }

        extensions.resize(extensionCount);
        if (extensionCount > 0 &&
            SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data()) != SDL_TRUE) {
            result.error = std::string("SDL could not read Vulkan instance extensions: ") + SDL_GetError();
            return finish();
        }
    } else {
#ifdef _WIN32
        extensions = {"VK_KHR_surface", "VK_KHR_win32_surface"};
        extensionCount = (unsigned int)extensions.size();
#else
        result.error = "Vulkan loader fallback is only implemented for Windows";
        return finish();
#endif
    }

    auto loadGlobal = [&](const char* name) {
        return vkGetInstanceProcAddr(nullptr, name);
    };

    auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        loadGlobal("vkCreateInstance"));
    if (!vkCreateInstance) {
        result.error = "Vulkan loader did not expose vkCreateInstance";
        return finish();
    }

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = appName ? appName : "FluxUI Vulkan Probe";
    applicationInfo.applicationVersion = makeVkVersion(1, 0, 0);
    applicationInfo.pEngineName = "FluxUI";
    applicationInfo.engineVersion = makeVkVersion(1, 0, 0);
    applicationInfo.apiVersion = makeVkVersion(1, 0, 0);

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &applicationInfo;
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    VkResult createResult = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (createResult != VK_SUCCESS || !instance) {
        result.error = vkResultMessage("vkCreateInstance", createResult);
        return finish();
    }
    result.instanceCreated = true;

    auto loadInstance = [&](const char* name) {
        return vkGetInstanceProcAddr(instance, name);
    };

    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        loadInstance("vkDestroyInstance"));
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        loadInstance("vkDestroySurfaceKHR"));

    if (loadedWithSDL) {
        if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE || !surface) {
            result.error = std::string("SDL could not create a Vulkan surface: ") + SDL_GetError();
            return finish();
        }
    } else {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo) != SDL_TRUE || !wmInfo.info.win.window) {
            result.error = std::string("SDL could not expose the Win32 window handle: ") + SDL_GetError();
            return finish();
        }

        auto vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
            loadInstance("vkCreateWin32SurfaceKHR"));
        if (!vkCreateWin32SurfaceKHR) {
            result.error = "Vulkan instance did not expose vkCreateWin32SurfaceKHR";
            return finish();
        }

        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hinstance = GetModuleHandleW(nullptr);
        surfaceInfo.hwnd = wmInfo.info.win.window;

        VkResult surfaceResult = vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
        if (surfaceResult != VK_SUCCESS || !surface) {
            result.error = vkResultMessage("vkCreateWin32SurfaceKHR", surfaceResult);
            return finish();
        }
#endif
    }
    result.surfaceCreated = true;

    auto vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        loadInstance("vkEnumeratePhysicalDevices"));
    auto vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        loadInstance("vkGetPhysicalDeviceProperties"));
    if (!vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceProperties) {
        result.error = "Vulkan instance did not expose physical device queries";
        return finish();
    }

    uint32_t deviceCount = 0;
    VkResult enumResult = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (enumResult != VK_SUCCESS) {
        result.error = vkResultMessage("vkEnumeratePhysicalDevices", enumResult);
        return finish();
    }
    result.physicalDeviceCount = deviceCount;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (deviceCount > 0) {
        enumResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        if (enumResult != VK_SUCCESS) {
            result.error = vkResultMessage("vkEnumeratePhysicalDevices", enumResult);
            return finish();
        }
    }

    result.deviceNames.reserve(deviceCount);
    for (uint32_t i = 0; i < deviceCount; ++i) {
        FluxUIVkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        uint32_t major = vkVersionMajor(properties.apiVersion);
        uint32_t minor = vkVersionMinor(properties.apiVersion);
        uint32_t patch = vkVersionPatch(properties.apiVersion);

        char line[384];
        std::snprintf(line,
                      sizeof(line),
                      "%s (%s, Vulkan %u.%u.%u)",
                      properties.deviceName,
                      physicalDeviceTypeName(properties.deviceType),
                      major,
                      minor,
                      patch);
        result.deviceNames.emplace_back(line);
    }

    return finish();
}

#if FLUXUI_HAS_VULKAN_SDK
struct VulkanQueueSelection {
    uint32_t graphics = std::numeric_limits<uint32_t>::max();
    uint32_t present = std::numeric_limits<uint32_t>::max();

    bool complete() const {
        return graphics != std::numeric_limits<uint32_t>::max() &&
               present != std::numeric_limits<uint32_t>::max();
    }
};

struct VulkanSwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

VulkanQueueSelection findVulkanQueues(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VulkanQueueSelection selection;
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueCount);
    if (queueCount > 0) {
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, families.data());
    }

    for (uint32_t i = 0; i < queueCount; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            selection.graphics == std::numeric_limits<uint32_t>::max()) {
            selection.graphics = i;
        }

        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported);
        if (presentSupported && selection.present == std::numeric_limits<uint32_t>::max()) {
            selection.present = i;
        }

        if (selection.complete()) {
            break;
        }
    }
    return selection;
}

bool deviceSupportsVulkanSwapchain(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (extensionCount > 0) {
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
    }

    for (const auto& extension : extensions) {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

VulkanSwapchainSupport queryVulkanSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VulkanSwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    support.presentModes.resize(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, support.presentModes.data());
    }

    return support;
}

bool physicalDeviceUsableForVulkan(VkPhysicalDevice device, VkSurfaceKHR surface) {
    auto queues = findVulkanQueues(device, surface);
    if (!queues.complete() || !deviceSupportsVulkanSwapchain(device)) {
        return false;
    }

    auto support = queryVulkanSwapchainSupport(device, surface);
    return !support.formats.empty() && !support.presentModes.empty();
}

int physicalDeviceScore(VkPhysicalDevice device, VkSurfaceKHR surface) {
    if (!physicalDeviceUsableForVulkan(device, surface)) {
        return -1;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(device, &properties);

    int score = 100;
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 500;
    }
    score += static_cast<int>(properties.limits.maxImageDimension2D / 1024);
    return score;
}

VkSurfaceFormatKHR chooseVulkanSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR chooseVulkanPresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
#if FLUXUI_ENABLE_VSYNC
    (void)presentModes;
    return VK_PRESENT_MODE_FIFO_KHR;
#else
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
#endif
}

VkExtent2D chooseVulkanExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                              int width,
                              int height) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {
        static_cast<uint32_t>(std::max(1, width)),
        static_cast<uint32_t>(std::max(1, height))
    };
    extent.width = std::clamp(extent.width,
                              capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
}

Color compositedVulkanColor(const Color& color, float opacity) {
    float alpha = std::clamp(color.a * opacity, 0.0f, 1.0f);
    constexpr float bgR = 0.06f;
    constexpr float bgG = 0.06f;
    constexpr float bgB = 0.09f;
    return {
        color.r * alpha + bgR * (1.0f - alpha),
        color.g * alpha + bgG * (1.0f - alpha),
        color.b * alpha + bgB * (1.0f - alpha),
        1.0f
    };
}

void destroyVulkanSwapchain(VulkanRendererState& state) {
    if (!state.device) {
        return;
    }

    auto destroyDynamicPool = [&](VulkanRendererState::DynamicPool& pool) {
        for (auto& page : pool.pages) {
            if (page.mapped) {
                vkUnmapMemory(state.device, page.memory);
                page.mapped = nullptr;
            }
            if (page.buffer) {
                vkDestroyBuffer(state.device, page.buffer, nullptr);
                page.buffer = VK_NULL_HANDLE;
            }
            if (page.memory) {
                vkFreeMemory(state.device, page.memory, nullptr);
                page.memory = VK_NULL_HANDLE;
            }
            page.capacityBytes = 0;
            page.bytesUsed = 0;
        }
        pool.pages.clear();
        pool.currentPage = 0;
    };

    std::vector<VkCommandBuffer> commandBuffers;
    commandBuffers.reserve(state.frames.size());
    for (auto& frame : state.frames) {
        destroyDynamicPool(frame.roundedInstances);
        destroyDynamicPool(frame.textVertices);
        if (frame.commandBuffer) {
            commandBuffers.push_back(frame.commandBuffer);
            frame.commandBuffer = VK_NULL_HANDLE;
        }
        if (frame.framebuffer) {
            vkDestroyFramebuffer(state.device, frame.framebuffer, nullptr);
            frame.framebuffer = VK_NULL_HANDLE;
        }
        if (frame.imageView) {
            vkDestroyImageView(state.device, frame.imageView, nullptr);
            frame.imageView = VK_NULL_HANDLE;
        }
        if (frame.imageAvailable) {
            vkDestroySemaphore(state.device, frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }
        if (frame.renderFinished) {
            vkDestroySemaphore(state.device, frame.renderFinished, nullptr);
            frame.renderFinished = VK_NULL_HANDLE;
        }
        if (frame.inFlight) {
            vkDestroyFence(state.device, frame.inFlight, nullptr);
            frame.inFlight = VK_NULL_HANDLE;
        }
    }

    if (!commandBuffers.empty() && state.commandPool) {
        vkFreeCommandBuffers(state.device,
                             state.commandPool,
                             static_cast<uint32_t>(commandBuffers.size()),
                             commandBuffers.data());
    }
    state.frames.clear();

    if (state.renderPass) {
        vkDestroyRenderPass(state.device, state.renderPass, nullptr);
        state.renderPass = VK_NULL_HANDLE;
    }
    if (state.swapchain) {
        vkDestroySwapchainKHR(state.device, state.swapchain, nullptr);
        state.swapchain = VK_NULL_HANDLE;
    }
}

bool createVulkanSwapchain(VulkanRendererState& state, int width, int height) {
    auto support = queryVulkanSwapchainSupport(state.physicalDevice, state.surface);
    if (support.formats.empty() || support.presentModes.empty()) {
        std::cerr << "FluxUI: Vulkan surface does not support a usable swapchain." << std::endl;
        return false;
    }

    auto surfaceFormat = chooseVulkanSurfaceFormat(support.formats);
    auto presentMode = chooseVulkanPresentMode(support.presentModes);
    auto extent = chooseVulkanExtent(support.capabilities, width, height);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = state.surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilies[] = {state.graphicsQueueFamily, state.presentQueueFamily};
    if (state.graphicsQueueFamily != state.presentQueueFamily) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilies;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainInfo.preTransform = support.capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;

    VkResult result = vkCreateSwapchainKHR(state.device, &swapchainInfo, nullptr, &state.swapchain);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateSwapchainKHR", result) << std::endl;
        return false;
    }

    state.swapchainFormat = surfaceFormat.format;
    state.swapchainExtent = extent;

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(state.device, state.swapchain, &actualImageCount, nullptr);
    std::vector<VkImage> images(actualImageCount);
    vkGetSwapchainImagesKHR(state.device, state.swapchain, &actualImageCount, images.data());

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = state.swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    result = vkCreateRenderPass(state.device, &renderPassInfo, nullptr, &state.renderPass);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateRenderPass", result) << std::endl;
        return false;
    }

    state.frames.resize(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        auto& frame = state.frames[i];
        frame.image = images[i];

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = frame.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = state.swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(state.device, &viewInfo, nullptr, &frame.imageView);
        if (result != VK_SUCCESS) {
            std::cerr << vkResultMessage("vkCreateImageView", result) << std::endl;
            return false;
        }

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = state.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &frame.imageView;
        framebufferInfo.width = state.swapchainExtent.width;
        framebufferInfo.height = state.swapchainExtent.height;
        framebufferInfo.layers = 1;

        result = vkCreateFramebuffer(state.device, &framebufferInfo, nullptr, &frame.framebuffer);
        if (result != VK_SUCCESS) {
            std::cerr << vkResultMessage("vkCreateFramebuffer", result) << std::endl;
            return false;
        }
    }

    std::vector<VkCommandBuffer> commandBuffers(state.frames.size());
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = state.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    result = vkAllocateCommandBuffers(state.device, &allocInfo, commandBuffers.data());
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkAllocateCommandBuffers", result) << std::endl;
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < state.frames.size(); ++i) {
        auto& frame = state.frames[i];
        frame.commandBuffer = commandBuffers[i];

        result = vkCreateSemaphore(state.device, &semaphoreInfo, nullptr, &frame.imageAvailable);
        if (result != VK_SUCCESS) {
            std::cerr << vkResultMessage("vkCreateSemaphore(imageAvailable)", result) << std::endl;
            return false;
        }
        result = vkCreateSemaphore(state.device, &semaphoreInfo, nullptr, &frame.renderFinished);
        if (result != VK_SUCCESS) {
            std::cerr << vkResultMessage("vkCreateSemaphore(renderFinished)", result) << std::endl;
            return false;
        }
        result = vkCreateFence(state.device, &fenceInfo, nullptr, &frame.inFlight);
        if (result != VK_SUCCESS) {
            std::cerr << vkResultMessage("vkCreateFence", result) << std::endl;
            return false;
        }
    }

    state.logicalWidth = width;
    state.logicalHeight = height;
    state.swapchainDirty = false;
    return true;
}

struct VulkanRoundedPush {
    float framebufferSize[2];
};

struct VulkanRoundedInstance {
    float rect[4];
    float color[4];
    float color2[4];
    float borderColor[4];
    float radius[4];
    float params[4];
};

struct VulkanTextPush {
    float framebufferSize[2];
};

struct VulkanTextVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

std::vector<uint32_t> compileVulkanShader(const char* code,
                                          glslang_stage_t stage,
                                          const char* label) {
    std::string safeLabel = label;
    for (char& c : safeLabel) { if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_'; }
    std::string cachePath = safeLabel + ".spv";

    std::ifstream cacheIn(cachePath, std::ios::binary);
    if (cacheIn) {
        cacheIn.seekg(0, std::ios::end);
        size_t size = cacheIn.tellg();
        cacheIn.seekg(0, std::ios::beg);
        std::vector<uint32_t> spirv(size / sizeof(uint32_t));
        cacheIn.read(reinterpret_cast<char*>(spirv.data()), size);
        return spirv;
    }

    static bool glslangInitialized = false;
    if (!glslangInitialized) {
        if (!glslang_initialize_process()) {
            std::cerr << "FluxUI: glslang initialization failed." << std::endl;
            return {};
        }
        glslangInitialized = true;
    }

    glslang_input_t input = {};
    input.language = GLSLANG_SOURCE_GLSL;
    input.stage = stage;
    input.client = GLSLANG_CLIENT_VULKAN;
    input.client_version = GLSLANG_TARGET_VULKAN_1_0;
    input.target_language = GLSLANG_TARGET_SPV;
    input.target_language_version = GLSLANG_TARGET_SPV_1_0;
    input.code = code;
    input.default_version = 450;
    input.default_profile = GLSLANG_CORE_PROFILE;
    input.force_default_version_and_profile = 0;
    input.forward_compatible = 0;
    input.messages = static_cast<glslang_messages_t>(
        GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT);
    input.resource = glslang_default_resource();

    glslang_shader_t* shader = glslang_shader_create(&input);
    if (!shader) {
        std::cerr << "FluxUI: could not create glslang shader for " << label << "." << std::endl;
        return {};
    }
    glslang_shader_set_entry_point(shader, "main");

    if (!glslang_shader_preprocess(shader, &input)) {
        std::cerr << "FluxUI: GLSL preprocess failed for " << label << ": "
                  << glslang_shader_get_info_log(shader)
                  << glslang_shader_get_info_debug_log(shader) << std::endl;
        glslang_shader_delete(shader);
        return {};
    }

    if (!glslang_shader_parse(shader, &input)) {
        std::cerr << "FluxUI: GLSL parse failed for " << label << ": "
                  << glslang_shader_get_info_log(shader)
                  << glslang_shader_get_info_debug_log(shader) << std::endl;
        glslang_shader_delete(shader);
        return {};
    }
    glslang_shader_set_entry_point(shader, "main");

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);
    if (!glslang_program_link(program, input.messages)) {
        std::cerr << "FluxUI: GLSL link failed for " << label << ": "
                  << glslang_program_get_info_log(program)
                  << glslang_program_get_info_debug_log(program) << std::endl;
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return {};
    }

    glslang_program_SPIRV_generate(program, stage);
    const char* messages = glslang_program_SPIRV_get_messages(program);
    if (messages && messages[0] != '\0') {
        std::cerr << "FluxUI: SPIR-V messages for " << label << ": " << messages << std::endl;
    }

    size_t wordCount = glslang_program_SPIRV_get_size(program);
    std::vector<uint32_t> spirv(wordCount);
    if (wordCount > 0) {
        glslang_program_SPIRV_get(program, spirv.data());
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

    std::ofstream cacheOut(cachePath, std::ios::binary);
    if (cacheOut && !spirv.empty()) {
        cacheOut.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));
    }

    return spirv;
}

VkShaderModule createVulkanShaderModule(VkDevice device,
                                        const std::vector<uint32_t>& spirv,
                                        const char* label) {
    if (spirv.empty()) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size() * sizeof(uint32_t);
    info.pCode = spirv.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(device, &info, nullptr, &module);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage(label, result) << std::endl;
        return VK_NULL_HANDLE;
    }
    return module;
}

uint32_t findVulkanMemoryType(VkPhysicalDevice physicalDevice,
                              uint32_t typeFilter,
                              VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0 &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return std::numeric_limits<uint32_t>::max();
}

bool createVulkanBuffer(VulkanRendererState& state,
                        VkDeviceSize size,
                        VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties,
                        VkBuffer& buffer,
                        VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(state.device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateBuffer", result) << std::endl;
        return false;
    }

    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(state.device, buffer, &requirements);
    uint32_t typeIndex = findVulkanMemoryType(state.physicalDevice,
                                              requirements.memoryTypeBits,
                                              properties);
    if (typeIndex == std::numeric_limits<uint32_t>::max()) {
        std::cerr << "FluxUI: no compatible Vulkan memory type for buffer." << std::endl;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = typeIndex;

    result = vkAllocateMemory(state.device, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkAllocateMemory(buffer)", result) << std::endl;
        return false;
    }

    vkBindBufferMemory(state.device, buffer, memory, 0);
    return true;
}

VkCommandBuffer beginVulkanOneTimeCommands(VulkanRendererState& state) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = state.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(state.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void endVulkanOneTimeCommands(VulkanRendererState& state, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(state.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(state.graphicsQueue);
    vkFreeCommandBuffers(state.device, state.commandPool, 1, &commandBuffer);
}

void transitionVulkanImage(VulkanRendererState& state,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginVulkanOneTimeCommands(state);
    if (!commandBuffer) {
        return;
    }

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer,
                         srcStage,
                         dstStage,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    endVulkanOneTimeCommands(state, commandBuffer);
}

void copyVulkanBufferToImage(VulkanRendererState& state,
                             VkBuffer buffer,
                             VkImage image,
                             uint32_t width,
                             uint32_t height) {
    VkCommandBuffer commandBuffer = beginVulkanOneTimeCommands(state);
    if (!commandBuffer) {
        return;
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer,
                           buffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    endVulkanOneTimeCommands(state, commandBuffer);
}

bool createVulkanPipelines(VulkanRendererState& state) {
    auto roundedVert = compileVulkanShader(VULKAN_ROUNDED_VERT,
                                           GLSLANG_STAGE_VERTEX,
                                           "vulkan rounded vertex");
    auto roundedFrag = compileVulkanShader(VULKAN_ROUNDED_FRAG,
                                           GLSLANG_STAGE_FRAGMENT,
                                           "vulkan rounded fragment");
    auto textVert = compileVulkanShader(VULKAN_TEXT_VERT,
                                        GLSLANG_STAGE_VERTEX,
                                        "vulkan text vertex");
    auto textFrag = compileVulkanShader(VULKAN_TEXT_FRAG,
                                        GLSLANG_STAGE_FRAGMENT,
                                        "vulkan text fragment");

    VkShaderModule roundedVertModule = createVulkanShaderModule(state.device, roundedVert, "rounded vertex module");
    VkShaderModule roundedFragModule = createVulkanShaderModule(state.device, roundedFrag, "rounded fragment module");
    VkShaderModule textVertModule = createVulkanShaderModule(state.device, textVert, "text vertex module");
    VkShaderModule textFragModule = createVulkanShaderModule(state.device, textFrag, "text fragment module");
    if (!roundedVertModule || !roundedFragModule || !textVertModule || !textFragModule) {
        if (roundedVertModule) vkDestroyShaderModule(state.device, roundedVertModule, nullptr);
        if (roundedFragModule) vkDestroyShaderModule(state.device, roundedFragModule, nullptr);
        if (textVertModule) vkDestroyShaderModule(state.device, textVertModule, nullptr);
        if (textFragModule) vkDestroyShaderModule(state.device, textFragModule, nullptr);
        return false;
    }

    auto makeStage = [](VkShaderStageFlagBits stage, VkShaderModule module) {
        VkPipelineShaderStageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = stage;
        info.module = module;
        info.pName = "main";
        return info;
    };

    VkPushConstantRange roundedPushRange = {};
    roundedPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    roundedPushRange.offset = 0;
    roundedPushRange.size = sizeof(VulkanRoundedPush);

    VkPipelineLayoutCreateInfo roundedLayoutInfo = {};
    roundedLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    roundedLayoutInfo.pushConstantRangeCount = 1;
    roundedLayoutInfo.pPushConstantRanges = &roundedPushRange;
    VkResult result = vkCreatePipelineLayout(state.device,
                                             &roundedLayoutInfo,
                                             nullptr,
                                             &state.roundedPipelineLayout);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreatePipelineLayout(rounded)", result) << std::endl;
        return false;
    }

    VkDescriptorSetLayoutBinding samplerBinding = {};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &samplerBinding;
    result = vkCreateDescriptorSetLayout(state.device,
                                         &descriptorLayoutInfo,
                                         nullptr,
                                         &state.textDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateDescriptorSetLayout(text)", result) << std::endl;
        return false;
    }

    VkPushConstantRange textPushRange = {};
    textPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    textPushRange.offset = 0;
    textPushRange.size = sizeof(VulkanTextPush);

    VkPipelineLayoutCreateInfo textLayoutInfo = {};
    textLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    textLayoutInfo.setLayoutCount = 1;
    textLayoutInfo.pSetLayouts = &state.textDescriptorSetLayout;
    textLayoutInfo.pushConstantRangeCount = 1;
    textLayoutInfo.pPushConstantRanges = &textPushRange;
    result = vkCreatePipelineLayout(state.device,
                                    &textLayoutInfo,
                                    nullptr,
                                    &state.textPipelineLayout);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreatePipelineLayout(text)", result) << std::endl;
        return false;
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.width = 1.0f;
    viewport.height = 1.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent = {1, 1};

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                     VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT |
                                     VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkVertexInputBindingDescription roundedBinding = {};
    roundedBinding.binding = 0;
    roundedBinding.stride = sizeof(VulkanRoundedInstance);
    roundedBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription roundedAttributes[6] = {};
    roundedAttributes[0].binding = 0;
    roundedAttributes[0].location = 0;
    roundedAttributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    roundedAttributes[0].offset = offsetof(VulkanRoundedInstance, rect);
    roundedAttributes[1].binding = 0;
    roundedAttributes[1].location = 1;
    roundedAttributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    roundedAttributes[1].offset = offsetof(VulkanRoundedInstance, color);
    roundedAttributes[2].binding = 0;
    roundedAttributes[2].location = 2;
    roundedAttributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    roundedAttributes[2].offset = offsetof(VulkanRoundedInstance, color2);
    roundedAttributes[3].binding = 0;
    roundedAttributes[3].location = 3;
    roundedAttributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    roundedAttributes[3].offset = offsetof(VulkanRoundedInstance, borderColor);
    roundedAttributes[4].binding = 0;
    roundedAttributes[4].location = 4;
    roundedAttributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    roundedAttributes[4].offset = offsetof(VulkanRoundedInstance, radius);
    roundedAttributes[5].binding = 0;
    roundedAttributes[5].location = 5;
    roundedAttributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    roundedAttributes[5].offset = offsetof(VulkanRoundedInstance, params);

    VkPipelineVertexInputStateCreateInfo roundedVertexInput = {};
    roundedVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    roundedVertexInput.vertexBindingDescriptionCount = 1;
    roundedVertexInput.pVertexBindingDescriptions = &roundedBinding;
    roundedVertexInput.vertexAttributeDescriptionCount = 6;
    roundedVertexInput.pVertexAttributeDescriptions = roundedAttributes;

    VkPipelineShaderStageCreateInfo roundedStages[] = {
        makeStage(VK_SHADER_STAGE_VERTEX_BIT, roundedVertModule),
        makeStage(VK_SHADER_STAGE_FRAGMENT_BIT, roundedFragModule)
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = roundedStages;
    pipelineInfo.pVertexInputState = &roundedVertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = state.roundedPipelineLayout;
    pipelineInfo.renderPass = state.renderPass;
    pipelineInfo.subpass = 0;

    result = vkCreateGraphicsPipelines(state.device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &pipelineInfo,
                                       nullptr,
                                       &state.roundedPipeline);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateGraphicsPipelines(rounded)", result) << std::endl;
        return false;
    }

    VkVertexInputBindingDescription textBinding = {};
    textBinding.binding = 0;
    textBinding.stride = sizeof(VulkanTextVertex);
    textBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription textAttributes[3] = {};
    textAttributes[0].binding = 0;
    textAttributes[0].location = 0;
    textAttributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    textAttributes[0].offset = offsetof(VulkanTextVertex, x);
    textAttributes[1].binding = 0;
    textAttributes[1].location = 1;
    textAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    textAttributes[1].offset = offsetof(VulkanTextVertex, u);
    textAttributes[2].binding = 0;
    textAttributes[2].location = 2;
    textAttributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    textAttributes[2].offset = offsetof(VulkanTextVertex, r);

    VkPipelineVertexInputStateCreateInfo textVertexInput = {};
    textVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    textVertexInput.vertexBindingDescriptionCount = 1;
    textVertexInput.pVertexBindingDescriptions = &textBinding;
    textVertexInput.vertexAttributeDescriptionCount = 3;
    textVertexInput.pVertexAttributeDescriptions = textAttributes;

    VkPipelineShaderStageCreateInfo textStages[] = {
        makeStage(VK_SHADER_STAGE_VERTEX_BIT, textVertModule),
        makeStage(VK_SHADER_STAGE_FRAGMENT_BIT, textFragModule)
    };

    pipelineInfo.pStages = textStages;
    pipelineInfo.pVertexInputState = &textVertexInput;
    pipelineInfo.layout = state.textPipelineLayout;
    result = vkCreateGraphicsPipelines(state.device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &pipelineInfo,
                                       nullptr,
                                       &state.textPipeline);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateGraphicsPipelines(text)", result) << std::endl;
        return false;
    }

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 256;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 256;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    result = vkCreateDescriptorPool(state.device, &poolInfo, nullptr, &state.descriptorPool);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateDescriptorPool", result) << std::endl;
        return false;
    }

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    result = vkCreateSampler(state.device, &samplerInfo, nullptr, &state.fontSampler);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateSampler", result) << std::endl;
        return false;
    }

    vkDestroyShaderModule(state.device, roundedVertModule, nullptr);
    vkDestroyShaderModule(state.device, roundedFragModule, nullptr);
    vkDestroyShaderModule(state.device, textVertModule, nullptr);
    vkDestroyShaderModule(state.device, textFragModule, nullptr);
    return true;
}

void destroyVulkanPipelines(VulkanRendererState& state) {
    if (!state.device) {
        return;
    }

    state.roundedBatchOffsetBytes = 0;
    state.roundedBatchPageIndex = 0;
    state.roundedBatchCount = 0;

    state.textBatchOffsetBytes = 0;
    state.textBatchPageIndex = 0;
    state.textBatchVertexCount = 0;
    state.textBatchDescriptorSet = VK_NULL_HANDLE;

    for (auto& item : state.fontTextures) {
        auto& texture = item.second;
        if (texture.view) vkDestroyImageView(state.device, texture.view, nullptr);
        if (texture.image) vkDestroyImage(state.device, texture.image, nullptr);
        if (texture.memory) vkFreeMemory(state.device, texture.memory, nullptr);
    }
    state.fontTextures.clear();

    if (state.fontSampler) {
        vkDestroySampler(state.device, state.fontSampler, nullptr);
        state.fontSampler = VK_NULL_HANDLE;
    }
    if (state.descriptorPool) {
        vkDestroyDescriptorPool(state.device, state.descriptorPool, nullptr);
        state.descriptorPool = VK_NULL_HANDLE;
    }
    if (state.roundedPipeline) {
        vkDestroyPipeline(state.device, state.roundedPipeline, nullptr);
        state.roundedPipeline = VK_NULL_HANDLE;
    }
    if (state.textPipeline) {
        vkDestroyPipeline(state.device, state.textPipeline, nullptr);
        state.textPipeline = VK_NULL_HANDLE;
    }
    if (state.roundedPipelineLayout) {
        vkDestroyPipelineLayout(state.device, state.roundedPipelineLayout, nullptr);
        state.roundedPipelineLayout = VK_NULL_HANDLE;
    }
    if (state.textPipelineLayout) {
        vkDestroyPipelineLayout(state.device, state.textPipelineLayout, nullptr);
        state.textPipelineLayout = VK_NULL_HANDLE;
    }
    if (state.textDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(state.device, state.textDescriptorSetLayout, nullptr);
        state.textDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

size_t alignVulkanDynamicOffset(size_t value, size_t alignment) {
    if (alignment <= 1) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

bool createVulkanDynamicPage(VulkanRendererState& state,
                             VulkanRendererState::DynamicPage& page,
                             VkDeviceSize requiredBytes,
                             VkDeviceSize minCapacityBytes,
                             const char* label) {
    VkDeviceSize nextSize = std::max(requiredBytes, minCapacityBytes);
    if (!createVulkanBuffer(state,
                            nextSize,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            page.buffer,
                            page.memory)) {
        return false;
    }

    VkResult result = vkMapMemory(state.device,
                                  page.memory,
                                  0,
                                  nextSize,
                                  0,
                                  &page.mapped);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage(label, result) << std::endl;
        return false;
    }
    page.capacityBytes = static_cast<size_t>(nextSize);
    page.bytesUsed = 0;
    return true;
}

bool vulkanDynamicPoolHasRoom(const VulkanRendererState::DynamicPool& pool,
                              size_t bytes,
                              size_t alignment) {
    if (pool.pages.empty() || pool.currentPage >= pool.pages.size()) {
        return false;
    }
    const auto& page = pool.pages[pool.currentPage];
    size_t offset = alignVulkanDynamicOffset(page.bytesUsed, alignment);
    return offset + bytes <= page.capacityBytes;
}

struct VulkanDynamicAllocation {
    VulkanRendererState::DynamicPage* page = nullptr;
    size_t pageIndex = 0;
    size_t offset = 0;
    void* mapped = nullptr;
};

bool allocateVulkanDynamicBytes(VulkanRendererState& state,
                                VulkanRendererState::DynamicPool& pool,
                                size_t bytes,
                                size_t alignment,
                                VkDeviceSize minPageBytes,
                                const char* label,
                                VulkanDynamicAllocation& allocation) {
    if (bytes == 0) {
        return false;
    }

    if (pool.pages.empty()) {
        pool.pages.emplace_back();
        pool.currentPage = 0;
        if (!createVulkanDynamicPage(state,
                                     pool.pages.back(),
                                     static_cast<VkDeviceSize>(bytes),
                                     minPageBytes,
                                     label)) {
            return false;
        }
    }

    size_t attempts = 0;
    const size_t maxAttempts = pool.pages.size() + 2;
    while (attempts++ < maxAttempts) {
        if (pool.currentPage >= pool.pages.size()) {
            pool.currentPage = pool.pages.size();
            pool.pages.emplace_back();
            if (!createVulkanDynamicPage(state,
                                         pool.pages.back(),
                                         static_cast<VkDeviceSize>(bytes),
                                         minPageBytes,
                                         label)) {
                return false;
            }
        }

        auto& page = pool.pages[pool.currentPage];
        size_t offset = alignVulkanDynamicOffset(page.bytesUsed, alignment);
        if (offset + bytes <= page.capacityBytes) {
            allocation.page = &page;
            allocation.pageIndex = pool.currentPage;
            allocation.offset = offset;
            allocation.mapped = static_cast<unsigned char*>(page.mapped) + offset;
            page.bytesUsed = offset + bytes;
            return true;
        }

        pool.currentPage++;
    }

    return false;
}

void resetVulkanDynamicPool(VulkanRendererState::DynamicPool& pool) {
    pool.currentPage = 0;
    for (auto& page : pool.pages) {
        page.bytesUsed = 0;
    }
}

bool ensureVulkanFontTexture(VulkanRendererState& state,
                             const std::string& key,
                             FontData& font) {
    auto existing = state.fontTextures.find(key);
    if (existing != state.fontTextures.end()) {
        return existing->second.descriptorSet != VK_NULL_HANDLE;
    }
    if (font.atlasPixels.empty() || font.atlasWidth <= 0 || font.atlasHeight <= 0) {
        return false;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(font.atlasPixels.size());
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!createVulkanBuffer(state,
                            imageSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            stagingBuffer,
                            stagingMemory)) {
        return false;
    }

    void* mapped = nullptr;
    VkResult result = vkMapMemory(state.device, stagingMemory, 0, imageSize, 0, &mapped);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkMapMemory(font staging)", result) << std::endl;
        return false;
    }
    std::memcpy(mapped, font.atlasPixels.data(), font.atlasPixels.size());
    vkUnmapMemory(state.device, stagingMemory);

    VulkanRendererState::FontTexture texture;
    texture.width = font.atlasWidth;
    texture.height = font.atlasHeight;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {
        static_cast<uint32_t>(font.atlasWidth),
        static_cast<uint32_t>(font.atlasHeight),
        1
    };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateImage(state.device, &imageInfo, nullptr, &texture.image);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateImage(font atlas)", result) << std::endl;
        return false;
    }

    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(state.device, texture.image, &requirements);
    uint32_t typeIndex = findVulkanMemoryType(state.physicalDevice,
                                              requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (typeIndex == std::numeric_limits<uint32_t>::max()) {
        std::cerr << "FluxUI: no compatible Vulkan memory type for font atlas." << std::endl;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = typeIndex;
    result = vkAllocateMemory(state.device, &allocInfo, nullptr, &texture.memory);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkAllocateMemory(font atlas)", result) << std::endl;
        return false;
    }
    vkBindImageMemory(state.device, texture.image, texture.memory, 0);

    transitionVulkanImage(state,
                          texture.image,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyVulkanBufferToImage(state,
                            stagingBuffer,
                            texture.image,
                            static_cast<uint32_t>(font.atlasWidth),
                            static_cast<uint32_t>(font.atlasHeight));
    transitionVulkanImage(state,
                          texture.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(state.device, stagingBuffer, nullptr);
    vkFreeMemory(state.device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    result = vkCreateImageView(state.device, &viewInfo, nullptr, &texture.view);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateImageView(font atlas)", result) << std::endl;
        return false;
    }

    VkDescriptorSetAllocateInfo descriptorInfo = {};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorInfo.descriptorPool = state.descriptorPool;
    descriptorInfo.descriptorSetCount = 1;
    descriptorInfo.pSetLayouts = &state.textDescriptorSetLayout;
    result = vkAllocateDescriptorSets(state.device, &descriptorInfo, &texture.descriptorSet);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkAllocateDescriptorSets(font atlas)", result) << std::endl;
        return false;
    }

    VkDescriptorImageInfo imageDescriptor = {};
    imageDescriptor.sampler = state.fontSampler;
    imageDescriptor.imageView = texture.view;
    imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = texture.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescriptor;
    vkUpdateDescriptorSets(state.device, 1, &write, 0, nullptr);

    state.fontTextures.emplace(key, texture);
    font.atlasPixels.clear();
    font.atlasPixels.shrink_to_fit();
    return true;
}

void setVulkanScissor(VulkanRendererState& state,
                      const std::vector<Rect>& scissorStack,
                      float dpiScale) {
    if (!state.frameActive || state.frames.empty()) {
        return;
    }

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = state.swapchainExtent;
    if (!scissorStack.empty()) {
        const Rect& clip = scissorStack.back();
        int fbW = static_cast<int>(state.swapchainExtent.width);
        int fbH = static_cast<int>(state.swapchainExtent.height);
        int x0 = std::clamp(static_cast<int>(std::floor(clip.x * dpiScale)), 0, fbW);
        int y0 = std::clamp(static_cast<int>(std::floor(clip.y * dpiScale)), 0, fbH);
        int x1 = std::clamp(static_cast<int>(std::ceil((clip.x + clip.w) * dpiScale)), 0, fbW);
        int y1 = std::clamp(static_cast<int>(std::ceil((clip.y + clip.h) * dpiScale)), 0, fbH);
        scissor.offset = {x0, y0};
        scissor.extent = {
            static_cast<uint32_t>(std::max(0, x1 - x0)),
            static_cast<uint32_t>(std::max(0, y1 - y0))
        };
    }

    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);
}

void flushVulkanRoundedBatch(VulkanRendererState& state,
                             const std::vector<Rect>& scissorStack,
                             float dpiScale) {
    if (!state.frameActive || state.roundedBatchCount == 0 ||
        !state.roundedPipeline || state.frames.empty()) {
        return;
    }

    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    if (state.roundedBatchPageIndex >= frame.roundedInstances.pages.size()) {
        state.roundedBatchCount = 0;
        return;
    }
    auto& page = frame.roundedInstances.pages[state.roundedBatchPageIndex];
    if (!page.buffer) {
        state.roundedBatchCount = 0;
        return;
    }
    VkDeviceSize offset = static_cast<VkDeviceSize>(state.roundedBatchOffsetBytes);
    VulkanRoundedPush push = {};
    push.framebufferSize[0] = static_cast<float>(state.swapchainExtent.width);
    push.framebufferSize[1] = static_cast<float>(state.swapchainExtent.height);

    setVulkanScissor(state, scissorStack, dpiScale);
    vkCmdBindPipeline(frame.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      state.roundedPipeline);
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &page.buffer, &offset);
    vkCmdPushConstants(frame.commandBuffer,
                       state.roundedPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(push),
                       &push);
    vkCmdDraw(frame.commandBuffer, 6, state.roundedBatchCount, 0, 0);

    state.roundedBatchCount = 0;
    state.roundedBatchOffsetBytes = page.bytesUsed;
}

void flushVulkanTextBatch(VulkanRendererState& state,
                          const std::vector<Rect>& scissorStack,
                          float dpiScale) {
    if (!state.frameActive || state.textBatchVertexCount == 0 ||
        !state.textPipeline || state.frames.empty() ||
        state.textBatchDescriptorSet == VK_NULL_HANDLE) {
        return;
    }

    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    if (state.textBatchPageIndex >= frame.textVertices.pages.size()) {
        state.textBatchVertexCount = 0;
        state.textBatchDescriptorSet = VK_NULL_HANDLE;
        return;
    }
    auto& page = frame.textVertices.pages[state.textBatchPageIndex];
    if (!page.buffer) {
        state.textBatchVertexCount = 0;
        state.textBatchDescriptorSet = VK_NULL_HANDLE;
        return;
    }
    VkDeviceSize offset = static_cast<VkDeviceSize>(state.textBatchOffsetBytes);
    VulkanTextPush push = {};
    push.framebufferSize[0] = static_cast<float>(state.swapchainExtent.width);
    push.framebufferSize[1] = static_cast<float>(state.swapchainExtent.height);

    setVulkanScissor(state, scissorStack, dpiScale);
    vkCmdBindPipeline(frame.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      state.textPipeline);
    vkCmdBindDescriptorSets(frame.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            state.textPipelineLayout,
                            0,
                            1,
                            &state.textBatchDescriptorSet,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &page.buffer, &offset);
    vkCmdPushConstants(frame.commandBuffer,
                       state.textPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(push),
                       &push);
    vkCmdDraw(frame.commandBuffer, state.textBatchVertexCount, 1, 0, 0);

    state.textBatchOffsetBytes = page.bytesUsed;
    state.textBatchVertexCount = 0;
    state.textBatchDescriptorSet = VK_NULL_HANDLE;
}

void flushVulkanBatches(VulkanRendererState& state,
                        const std::vector<Rect>& scissorStack,
                        float dpiScale) {
    flushVulkanRoundedBatch(state, scissorStack, dpiScale);
    flushVulkanTextBatch(state, scissorStack, dpiScale);
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
    constexpr size_t initialFloatCapacity = 4096;

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
}

void Renderer::useShader(uint32_t shader) {
    if (activeShader_ == shader) return;
    glUseProgram(shader);
    activeShader_ = shader;
}

bool Renderer::initVulkan(SDL_Window* window) {
#if !FLUXUI_HAS_VULKAN_SDK
    (void)window;
    std::cerr << "FluxUI: Vulkan backend needs the Vulkan SDK/loader at build time." << std::endl;
    return false;
#else
    vulkan_ = std::make_unique<VulkanRendererState>();
    auto& state = *vulkan_;

    int w = 1, h = 1;
    SDL_GetWindowSize(window, &w, &h);
    windowWidth_ = std::max(1, w);
    windowHeight_ = std::max(1, h);

    std::vector<const char*> extensions;
#if defined(_WIN32)
    extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
#else
    unsigned int extensionCount = 0;
    if (SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr) != SDL_TRUE) {
        std::cerr << "FluxUI: SDL could not query Vulkan extensions: "
                  << SDL_GetError() << std::endl;
        shutdownVulkan();
        return false;
    }
    extensions.resize(extensionCount);
    if (extensionCount > 0 &&
        SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data()) != SDL_TRUE) {
        std::cerr << "FluxUI: SDL could not read Vulkan extensions: "
                  << SDL_GetError() << std::endl;
        shutdownVulkan();
        return false;
    }
#endif

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "FluxUI";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "FluxUI";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &state.instance);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateInstance", result) << std::endl;
        shutdownVulkan();
        return false;
    }

#if defined(_WIN32)
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo) != SDL_TRUE || !wmInfo.info.win.window) {
        std::cerr << "FluxUI: SDL could not expose the Win32 window handle: "
                  << SDL_GetError() << std::endl;
        shutdownVulkan();
        return false;
    }

    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandleW(nullptr);
    surfaceInfo.hwnd = wmInfo.info.win.window;

    result = vkCreateWin32SurfaceKHR(state.instance, &surfaceInfo, nullptr, &state.surface);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateWin32SurfaceKHR", result) << std::endl;
        shutdownVulkan();
        return false;
    }
#else
    if (SDL_Vulkan_CreateSurface(window, state.instance, &state.surface) != SDL_TRUE ||
        !state.surface) {
        std::cerr << "FluxUI: SDL could not create Vulkan surface: "
                  << SDL_GetError() << std::endl;
        shutdownVulkan();
        return false;
    }
#endif

    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(state.instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        std::cerr << "FluxUI: no Vulkan physical devices are available." << std::endl;
        shutdownVulkan();
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(state.instance, &deviceCount, devices.data());

    int bestScore = -1;
    for (auto device : devices) {
        int score = physicalDeviceScore(device, state.surface);
        if (score > bestScore) {
            bestScore = score;
            state.physicalDevice = device;
        }
    }

    if (!state.physicalDevice) {
        std::cerr << "FluxUI: no Vulkan device supports graphics, present, and swapchain." << std::endl;
        shutdownVulkan();
        return false;
    }

    auto queues = findVulkanQueues(state.physicalDevice, state.surface);
    state.graphicsQueueFamily = queues.graphics;
    state.presentQueueFamily = queues.present;

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueQueues = {state.graphicsQueueFamily, state.presentQueueFamily};
    for (uint32_t queueFamily : uniqueQueues) {
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    result = vkCreateDevice(state.physicalDevice, &deviceInfo, nullptr, &state.device);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateDevice", result) << std::endl;
        shutdownVulkan();
        return false;
    }

    vkGetDeviceQueue(state.device, state.graphicsQueueFamily, 0, &state.graphicsQueue);
    vkGetDeviceQueue(state.device, state.presentQueueFamily, 0, &state.presentQueue);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = state.graphicsQueueFamily;

    result = vkCreateCommandPool(state.device, &poolInfo, nullptr, &state.commandPool);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateCommandPool", result) << std::endl;
        shutdownVulkan();
        return false;
    }

    if (!createVulkanSwapchain(state, windowWidth_, windowHeight_)) {
        shutdownVulkan();
        return false;
    }

    if (!createVulkanPipelines(state)) {
        shutdownVulkan();
        return false;
    }

    dpiScale_ = static_cast<float>(state.swapchainExtent.width) /
                static_cast<float>(std::max(1, windowWidth_));

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(state.physicalDevice, &properties);
    std::cout << "FluxUI Renderer initialized (Vulkan "
              << vkVersionMajor(properties.apiVersion) << "."
              << vkVersionMinor(properties.apiVersion) << "."
              << vkVersionPatch(properties.apiVersion)
              << ", device " << properties.deviceName << ")" << std::endl;

    backendInitialized_ = true;
    return true;
#endif
}

void Renderer::shutdownVulkan() {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_) {
        return;
    }
    auto& state = *vulkan_;
    if (state.device) {
        vkDeviceWaitIdle(state.device);
        destroyVulkanPipelines(state);
        destroyVulkanSwapchain(state);
        if (state.commandPool) {
            vkDestroyCommandPool(state.device, state.commandPool, nullptr);
            state.commandPool = VK_NULL_HANDLE;
        }
        vkDestroyDevice(state.device, nullptr);
        state.device = VK_NULL_HANDLE;
    }
    if (state.surface && state.instance) {
        vkDestroySurfaceKHR(state.instance, state.surface, nullptr);
        state.surface = VK_NULL_HANDLE;
    }
    if (state.instance) {
        vkDestroyInstance(state.instance, nullptr);
        state.instance = VK_NULL_HANDLE;
    }
#endif
    vulkan_.reset();
}

bool Renderer::beginVulkanFrame(int w, int h) {
#if !FLUXUI_HAS_VULKAN_SDK
    (void)w;
    (void)h;
    return false;
#else
    if (!vulkan_ || !vulkan_->device || vulkan_->frames.empty()) {
        return false;
    }

    auto& state = *vulkan_;
    windowWidth_ = std::max(1, w);
    windowHeight_ = std::max(1, h);

    if (state.swapchainDirty ||
        state.logicalWidth != windowWidth_ ||
        state.logicalHeight != windowHeight_) {
        vkDeviceWaitIdle(state.device);
        destroyVulkanSwapchain(state);
        if (!createVulkanSwapchain(state, windowWidth_, windowHeight_)) {
            return false;
        }
    }

    dpiScale_ = static_cast<float>(state.swapchainExtent.width) /
                static_cast<float>(std::max(1, windowWidth_));

    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    vkWaitForFences(state.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(state.device,
                                            state.swapchain,
                                            UINT64_MAX,
                                            frame.imageAvailable,
                                            VK_NULL_HANDLE,
                                            &state.currentImage);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        vkDeviceWaitIdle(state.device);
        destroyVulkanSwapchain(state);
        if (!createVulkanSwapchain(state, windowWidth_, windowHeight_)) {
            return false;
        }
        return beginVulkanFrame(windowWidth_, windowHeight_);
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << vkResultMessage("vkAcquireNextImageKHR", result) << std::endl;
        return false;
    }

    vkResetCommandBuffer(frame.commandBuffer, 0);
    resetVulkanDynamicPool(frame.roundedInstances);
    state.roundedBatchOffsetBytes = 0;
    state.roundedBatchPageIndex = 0;
    state.roundedBatchCount = 0;
    resetVulkanDynamicPool(frame.textVertices);
    state.textBatchOffsetBytes = 0;
    state.textBatchPageIndex = 0;
    state.textBatchVertexCount = 0;
    state.textBatchDescriptorSet = VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkBeginCommandBuffer", result) << std::endl;
        return false;
    }

    VkClearValue clearValue = {};
    clearValue.color.float32[0] = 0.06f;
    clearValue.color.float32[1] = 0.06f;
    clearValue.color.float32[2] = 0.09f;
    clearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = state.renderPass;
    renderPassInfo.framebuffer = state.frames[state.currentImage].framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = state.swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(state.swapchainExtent.width);
    viewport.height = static_cast<float>(state.swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
    setVulkanScissor(state, scissorStack_, dpiScale_);
    state.frameActive = true;
    scissorStack_.clear();
    setVulkanScissor(state, scissorStack_, dpiScale_);
    return true;
#endif
}

void Renderer::endVulkanFrame() {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_ || !vulkan_->frameActive || vulkan_->frames.empty()) {
        return;
    }

    auto& state = *vulkan_;
    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    flushVulkanBatches(state, scissorStack_, dpiScale_);
    state.frameActive = false;

    vkCmdEndRenderPass(frame.commandBuffer);
    VkResult result = vkEndCommandBuffer(frame.commandBuffer);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkEndCommandBuffer", result) << std::endl;
        return;
    }

    VkSemaphore waitSemaphores[] = {frame.imageAvailable};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {frame.renderFinished};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(state.device, 1, &frame.inFlight);
    result = vkQueueSubmit(state.graphicsQueue, 1, &submitInfo, frame.inFlight);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkQueueSubmit", result) << std::endl;
        return;
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &state.swapchain;
    presentInfo.pImageIndices = &state.currentImage;

    result = vkQueuePresentKHR(state.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        state.swapchainDirty = true;
    } else if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkQueuePresentKHR", result) << std::endl;
    }

    state.currentFrame = (state.currentFrame + 1) %
                         static_cast<uint32_t>(state.frames.size());
#endif
}

void Renderer::drawVulkanRect(const Rect& rect, const Color& color, float opacity) {
    drawVulkanRoundedRect(rect,
                          color,
                          color,
                          Color(0, 0, 0, 0),
                          BorderRadius(0),
                          0.0f,
                          opacity,
                          false,
                          0.0f);
}

void Renderer::drawVulkanRoundedRect(const Rect& rect,
                                     const Color& color,
                                     const Color& color2,
                                     const Color& borderColor,
                                     const BorderRadius& radius,
                                     float borderWidth,
                                     float opacity,
                                     bool hasGradient,
                                     float gradientAngle) {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_ || !vulkan_->frameActive || !vulkan_->roundedPipeline ||
        opacity <= 0.0f || (color.a <= 0.0f && borderColor.a <= 0.0f)) {
        return;
    }

    auto& state = *vulkan_;
    Rect drawRect = {
        rect.x + translation_.x,
        rect.y + translation_.y,
        rect.w,
        rect.h
    };

    float geometryScale = 1.0f;
    if (scale_ != 1.0f) {
        geometryScale = scale_;
        Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
        drawRect.x = pivot.x + (drawRect.x - pivot.x) * scale_;
        drawRect.y = pivot.y + (drawRect.y - pivot.y) * scale_;
        drawRect.w *= scale_;
        drawRect.h *= scale_;
    }

    if (drawRect.w <= 0.0f || drawRect.h <= 0.0f) {
        return;
    }

    flushVulkanTextBatch(state, scissorStack_, dpiScale_);

    VulkanRoundedInstance instance = {};
    instance.rect[0] = drawRect.x * dpiScale_;
    instance.rect[1] = drawRect.y * dpiScale_;
    instance.rect[2] = drawRect.w * dpiScale_;
    instance.rect[3] = drawRect.h * dpiScale_;
    instance.color[0] = color.r;
    instance.color[1] = color.g;
    instance.color[2] = color.b;
    instance.color[3] = color.a;
    instance.color2[0] = color2.r;
    instance.color2[1] = color2.g;
    instance.color2[2] = color2.b;
    instance.color2[3] = color2.a;
    instance.borderColor[0] = borderColor.r;
    instance.borderColor[1] = borderColor.g;
    instance.borderColor[2] = borderColor.b;
    instance.borderColor[3] = borderColor.a;
    instance.radius[0] = radius.uniform() * geometryScale * dpiScale_;
    instance.radius[1] = radius.tr * geometryScale * dpiScale_;
    instance.radius[2] = radius.br * geometryScale * dpiScale_;
    instance.radius[3] = radius.bl * geometryScale * dpiScale_;
    instance.params[0] = borderWidth * geometryScale * dpiScale_;
    instance.params[1] = opacity;
    instance.params[2] = hasGradient ? 1.0f : 0.0f;
    instance.params[3] = gradientAngle;

    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    if (!vulkanDynamicPoolHasRoom(frame.roundedInstances,
                                  sizeof(VulkanRoundedInstance),
                                  alignof(VulkanRoundedInstance)) &&
        state.roundedBatchCount > 0) {
        flushVulkanRoundedBatch(state, scissorStack_, dpiScale_);
    }

    VulkanDynamicAllocation allocation;
    if (!allocateVulkanDynamicBytes(state,
                                    frame.roundedInstances,
                                    sizeof(VulkanRoundedInstance),
                                    alignof(VulkanRoundedInstance),
                                    512 * 1024,
                                    "vkMapMemory(rounded dynamic page)",
                                    allocation)) {
        return;
    }

    std::memcpy(allocation.mapped, &instance, sizeof(instance));
    if (state.roundedBatchCount == 0) {
        state.roundedBatchOffsetBytes = allocation.offset;
        state.roundedBatchPageIndex = allocation.pageIndex;
    }
    state.roundedBatchCount++;
#else
    (void)rect;
    (void)color;
    (void)color2;
    (void)borderColor;
    (void)radius;
    (void)borderWidth;
    (void)opacity;
    (void)hasGradient;
    (void)gradientAngle;
#endif
}

void Renderer::drawVulkanBoxShadow(const Rect& rect,
                                   const BoxShadow& shadow,
                                   const BorderRadius& radius) {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_ || !vulkan_->frameActive || !vulkan_->roundedPipeline ||
        shadow.color.a <= 0.0f || (shadow.blur <= 0.0f && shadow.spread <= 0.0f)) {
        return;
    }

    auto& state = *vulkan_;
    float expand = shadow.blur + shadow.spread;
    Rect drawRect = {
        rect.x + shadow.offsetX - expand + translation_.x,
        rect.y + shadow.offsetY - expand + translation_.y,
        rect.w + expand * 2.0f,
        rect.h + expand * 2.0f
    };

    float geometryScale = 1.0f;
    if (scale_ != 1.0f) {
        geometryScale = scale_;
        Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
        drawRect.x = pivot.x + (drawRect.x - pivot.x) * scale_;
        drawRect.y = pivot.y + (drawRect.y - pivot.y) * scale_;
        drawRect.w *= scale_;
        drawRect.h *= scale_;
    }

    if (drawRect.w <= 0.0f || drawRect.h <= 0.0f) {
        return;
    }

    flushVulkanTextBatch(state, scissorStack_, dpiScale_);

    VulkanRoundedInstance instance = {};
    instance.rect[0] = drawRect.x * dpiScale_;
    instance.rect[1] = drawRect.y * dpiScale_;
    instance.rect[2] = drawRect.w * dpiScale_;
    instance.rect[3] = drawRect.h * dpiScale_;
    instance.color[0] = shadow.color.r;
    instance.color[1] = shadow.color.g;
    instance.color[2] = shadow.color.b;
    instance.color[3] = shadow.color.a;
    instance.radius[0] = radius.uniform() * geometryScale * dpiScale_;
    instance.radius[1] = radius.tr * geometryScale * dpiScale_;
    instance.radius[2] = radius.br * geometryScale * dpiScale_;
    instance.radius[3] = radius.bl * geometryScale * dpiScale_;
    instance.params[0] = std::max(0.001f, shadow.blur * geometryScale * dpiScale_);
    instance.params[1] = 1.0f;
    instance.params[2] = -1.0f;
    instance.params[3] = 0.0f;

    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    if (!vulkanDynamicPoolHasRoom(frame.roundedInstances,
                                  sizeof(VulkanRoundedInstance),
                                  alignof(VulkanRoundedInstance)) &&
        state.roundedBatchCount > 0) {
        flushVulkanRoundedBatch(state, scissorStack_, dpiScale_);
    }

    VulkanDynamicAllocation allocation;
    if (!allocateVulkanDynamicBytes(state,
                                    frame.roundedInstances,
                                    sizeof(VulkanRoundedInstance),
                                    alignof(VulkanRoundedInstance),
                                    512 * 1024,
                                    "vkMapMemory(rounded dynamic page)",
                                    allocation)) {
        return;
    }

    std::memcpy(allocation.mapped, &instance, sizeof(instance));
    if (state.roundedBatchCount == 0) {
        state.roundedBatchOffsetBytes = allocation.offset;
        state.roundedBatchPageIndex = allocation.pageIndex;
    }
    state.roundedBatchCount++;
#else
    (void)rect;
    (void)shadow;
    (void)radius;
#endif
}

void Renderer::drawVulkanText(const std::string& text,
                              const Vec2& pos,
                              const Color& color,
                              float fontSize,
                              FontWeight weight,
                              const std::string& fontName) {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_ || !vulkan_->frameActive || !vulkan_->textPipeline || text.empty()) {
        return;
    }

    FontData* fontPtr = getFontForSize(fontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) {
        return;
    }
    FontData& font = *fontPtr;

    int snappedSize = std::max(8, (int)std::round(fontSize));
    std::string sizedTextureKey = fontName + "@" + std::to_string(snappedSize);
    std::string textureKey = sizedTextureKey;
    auto baseFontIt = fonts_.find(fontName);
    if (baseFontIt != fonts_.end() && fontPtr == &baseFontIt->second) {
        textureKey = fontName;
    }

    auto& state = *vulkan_;
    if (!ensureVulkanFontTexture(state, textureKey, font)) {
        return;
    }
    auto textureIt = state.fontTextures.find(textureKey);
    if (textureIt == state.fontTextures.end()) {
        return;
    }

    float fontScale = fontSize / font.fontSize;
    auto& vertices = textVertexScratch_;
    vertices.clear();
    vertices.reserve(text.size() * 48);
    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };

    float cursorX = snap(pos.x + translation_.x);
    float baselineY = snap(pos.y + font.ascent * fontScale + translation_.y);
    float boldOffset = (weight == FontWeight::Bold) ? std::max(0.35f, fontSize * 0.018f) : 0.0f;

    auto appendGlyph = [&](float x, float y, float w, float h, const GlyphInfo& g) {
        float xp = x * dpiScale_;
        float yp = y * dpiScale_;
        float wp = w * dpiScale_;
        float hp = h * dpiScale_;
        float data[] = {
            xp,    yp,    g.x0, g.y0, color.r, color.g, color.b, color.a,
            xp+wp, yp,    g.x1, g.y0, color.r, color.g, color.b, color.a,
            xp+wp, yp+hp, g.x1, g.y1, color.r, color.g, color.b, color.a,
            xp,    yp,    g.x0, g.y0, color.r, color.g, color.b, color.a,
            xp+wp, yp+hp, g.x1, g.y1, color.r, color.g, color.b, color.a,
            xp,    yp+hp, g.x0, g.y1, color.r, color.g, color.b, color.a,
        };
        vertices.insert(vertices.end(), std::begin(data), std::end(data));
    };

    auto getNextCodepoint = [](const std::string& s, size_t& i) -> uint32_t {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) return (uint32_t)s[i++];
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            uint32_t cp = ((s[i++] & 0x1F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            uint32_t cp = ((s[i++] & 0x0F) << 12) | ((s[i++] & 0x3F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            uint32_t cp = ((s[i++] & 0x07) << 18) | ((s[i++] & 0x3F) << 12) |
                          ((s[i++] & 0x3F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        return (uint32_t)s[i++];
    };

    for (size_t i = 0; i < text.size(); ) {
        uint32_t c = getNextCodepoint(text, i);
        if (c >= 1024) continue;
        const auto& g = font.glyphs[c];
        if (g.xadvance == 0 && c != ' ') continue;

        float w = g.width * fontScale;
        float h = g.height * fontScale;
        if (w > 0 && h > 0) {
            float x = snap(cursorX + g.xoff * fontScale);
            float y = snap(baselineY + g.yoff * fontScale);
            appendGlyph(x, y, w, h, g);
            if (boldOffset > 0.0f) {
                appendGlyph(snap(x + boldOffset), y, w, h, g);
            }
        }
        cursorX += g.xadvance * fontScale;
    }

    if (vertices.empty()) {
        return;
    }

    flushVulkanRoundedBatch(state, scissorStack_, dpiScale_);

    size_t byteSize = vertices.size() * sizeof(float);
    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    if (!vulkanDynamicPoolHasRoom(frame.textVertices,
                                  byteSize,
                                  alignof(VulkanTextVertex)) &&
        state.textBatchVertexCount > 0) {
        flushVulkanTextBatch(state, scissorStack_, dpiScale_);
    }

    VulkanDynamicAllocation allocation;
    if (!allocateVulkanDynamicBytes(state,
                                    frame.textVertices,
                                    byteSize,
                                    alignof(VulkanTextVertex),
                                    4 * 1024 * 1024,
                                    "vkMapMemory(text dynamic page)",
                                    allocation)) {
        return;
    }
    std::memcpy(allocation.mapped, vertices.data(), byteSize);

    uint32_t vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    VkDescriptorSet descriptorSet = textureIt->second.descriptorSet;
    if (state.textBatchVertexCount > 0 &&
        state.textBatchDescriptorSet != descriptorSet) {
        flushVulkanTextBatch(state, scissorStack_, dpiScale_);
    }
    if (state.textBatchVertexCount == 0) {
        state.textBatchOffsetBytes = allocation.offset;
        state.textBatchPageIndex = allocation.pageIndex;
        state.textBatchDescriptorSet = descriptorSet;
    }
    state.textBatchVertexCount += vertexCount;
#else
    (void)text;
    (void)pos;
    (void)color;
    (void)fontSize;
    (void)weight;
    (void)fontName;
#endif
}

bool Renderer::init(SDL_Window* window) {
    window_ = window;

    if (activeBackend_ == RenderBackendType::Vulkan) {
        return initVulkan(window);
    }

    if (activeBackend_ != RenderBackendType::Compatibility) {
        std::cerr << "FluxUI: " << activeBackendName()
                  << " draw path is not implemented yet. Using compatibility renderer." << std::endl;
        activeBackend_ = RenderBackendType::Compatibility;
    }
    
    // Calculate DPI scale
    int w, h, dw, dh;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    dpiScale_ = (float)dw / (float)w;
    windowWidth_ = w;
    windowHeight_ = h;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if FLUXUI_ENABLE_MSAA
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
#else
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
#endif

    glContext_ = SDL_GL_CreateContext(window);
    if (!glContext_) {
        std::cerr << "Failed to create compatibility GPU context: " << SDL_GetError() << std::endl;
        return false;
    }

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (!version) {
        std::cerr << "Failed to initialize compatibility GPU loader" << std::endl;
        return false;
    }

    SDL_GL_SetSwapInterval(FLUXUI_ENABLE_VSYNC ? 1 : 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if FLUXUI_ENABLE_MSAA
    glEnable(GL_MULTISAMPLE);
#else
    glDisable(GL_MULTISAMPLE);
#endif

    // Compile shaders
    roundedRectShader_ = compileShader(ROUNDED_RECT_VERT, ROUNDED_RECT_FRAG);
    shadowShader_ = compileShader(ROUNDED_RECT_VERT, SHADOW_FRAG);
    textShader_ = compileShader(TEXT_VERT, TEXT_FRAG);
    cacheUniformLocations();

    setupQuad();
    setupTextBuffer();

    std::cout << "FluxUI Renderer initialized (compatibility driver "
              << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version)
              << ", backend " << activeBackendName() << ")" << std::endl;
    backendInitialized_ = true;
    return true;
}

void Renderer::shutdown() {
    if (activeBackend_ == RenderBackendType::Vulkan || vulkan_) {
        shutdownVulkan();
        backendInitialized_ = false;
        return;
    }

    if (roundedRectShader_) glDeleteProgram(roundedRectShader_);
    if (shadowShader_) glDeleteProgram(shadowShader_);
    if (textShader_) glDeleteProgram(textShader_);
    if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
    if (quadVBO_) glDeleteBuffers(1, &quadVBO_);
    if (textVAO_) glDeleteVertexArrays(1, &textVAO_);
    if (textVBO_) glDeleteBuffers(1, &textVBO_);
    for (auto& [name, font] : fonts_) {
        if (font.textureId) glDeleteTextures(1, &font.textureId);
    }
    if (glContext_) SDL_GL_DeleteContext(glContext_);
    glContext_ = nullptr;
    backendInitialized_ = false;
    activeShader_ = 0;
}

// ============================================================
//  Frame Management
// ============================================================

void Renderer::beginFrame(int w, int h) {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        beginVulkanFrame(w, h);
        return;
    }

    windowWidth_ = w; windowHeight_ = h;
    
    int drawW = w, drawH = h;
    if (window_) {
        SDL_GL_GetDrawableSize(window_, &drawW, &drawH);
        dpiScale_ = (float)drawW / (float)w;
    }
    glViewport(0, 0, drawW, drawH);
    glClearColor(0.06f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    scissorStack_.clear();
    activeShader_ = 0;
    glDisable(GL_SCISSOR_TEST);
}

void Renderer::endFrame() {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        endVulkanFrame();
        return;
    }

    SDL_GL_SwapWindow(window_);
}

// ============================================================
//  Font Loading
// ============================================================

bool Renderer::loadFont(const std::string& path, float size, const std::string& name) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to load font: " << path << std::endl;
        return false;
    }
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return loadFontFromMemory(buffer.data(), (int)fileSize, size, name);
}

bool Renderer::loadFontFromMemory(const unsigned char* data, int dataSize, float size, const std::string& name) {
    FontData font;
    font.sourceData.assign(data, data + dataSize);
    if (!buildFontAtlas(font, font.sourceData.data(), (int)font.sourceData.size(), size)) {
        return false;
    }

    fonts_[name] = font;
    textMeasureCache_.clear();
    if (!currentFont_) currentFont_ = &fonts_[name];

    return true;
}

bool Renderer::buildFontAtlas(FontData& font, const unsigned char* data, int dataSize, float size) {
    font.fontSize = size;
    float scaledSize = size * dpiScale_;
    font.atlasWidth = (scaledSize > 40.0f) ? 2048 : 1024;
    font.atlasHeight = font.atlasWidth;

    std::vector<unsigned char> atlas(font.atlasWidth * font.atlasHeight, 0);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, data, 0)) return false;

    float scale = stbtt_ScaleForPixelHeight(&info, scaledSize);
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    font.ascent = (ascent * scale) / dpiScale_;
    font.descent = (descent * scale) / dpiScale_;
    font.lineGap = (lineGap * scale) / dpiScale_;
    
    // Pack glyphs into atlas
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, atlas.data(), font.atlasWidth, font.atlasHeight, 0, 2, nullptr);
    stbtt_PackSetOversampling(&pc, 2, 2);
    
    stbtt_packedchar charData[1024] = {};
    int packed = stbtt_PackFontRange(&pc, data, 0, scaledSize, 32, 1024-32, charData+32);
    stbtt_PackEnd(&pc);
    if (!packed) return false;
    
    // Fill glyph info
    for (int i = 32; i < 1024; i++) {
        auto& cd = charData[i];
        auto& gi = font.glyphs[i];

        // Use GetPackedQuad to get correct screen-space dimensions
        stbtt_aligned_quad q;
        float dummyX = 0, dummyY = 0;
        stbtt_GetPackedQuad(charData, font.atlasWidth, font.atlasHeight,
                            i, &dummyX, &dummyY, &q, 0);

        gi.x0 = q.s0;
        gi.y0 = q.t0;
        gi.x1 = q.s1;
        gi.y1 = q.t1;
        gi.xoff = q.x0 / dpiScale_;
        gi.yoff = q.y0 / dpiScale_;
        gi.xadvance = cd.xadvance / dpiScale_;
        gi.width = (q.x1 - q.x0) / dpiScale_;
        gi.height = (q.y1 - q.y0) / dpiScale_;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        font.atlasPixels = std::move(atlas);
        font.textureId = 0;
        font.loaded = true;
        return true;
    }

    // Upload to GPU
    glGenTextures(1, &font.textureId);
    glBindTexture(GL_TEXTURE_2D, font.textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font.atlasWidth, font.atlasHeight,
                 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    font.loaded = true;
    return true;
}

FontData* Renderer::getFontForSize(const std::string& fontName, float fontSize) {
    int snappedSize = std::max(8, (int)std::round(fontSize));
    std::string sizedName = fontName + "@" + std::to_string(snappedSize);

    auto sizedIt = fonts_.find(sizedName);
    if (sizedIt != fonts_.end() && sizedIt->second.loaded) return &sizedIt->second;

    auto baseIt = fonts_.find(fontName);
    if (baseIt == fonts_.end() || !baseIt->second.loaded) return nullptr;
    if (baseIt->second.sourceData.empty()) return &baseIt->second;

    FontData sizedFont;
    const auto& source = baseIt->second.sourceData;
    if (!buildFontAtlas(sizedFont, source.data(),
                        (int)source.size(), (float)snappedSize)) {
        return &fonts_[fontName];
    }

    // Derived size atlases reuse the base font bytes instead of duplicating
    // the full source data for every requested font size.
    sizedFont.sourceData.clear();
    sizedFont.sourceData.shrink_to_fit();
    fonts_[sizedName] = sizedFont;
    textMeasureCache_.clear();
    return &fonts_[sizedName];
}

const FontData* Renderer::findFontForMeasure(const std::string& fontName, float fontSize) const {
    int snappedSize = std::max(8, (int)std::round(fontSize));
    std::string sizedName = fontName + "@" + std::to_string(snappedSize);

    auto sizedIt = fonts_.find(sizedName);
    if (sizedIt != fonts_.end() && sizedIt->second.loaded) return &sizedIt->second;

    auto baseIt = fonts_.find(fontName);
    if (baseIt != fonts_.end() && baseIt->second.loaded) return &baseIt->second;

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

    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };
    useShader(roundedRectShader_);
    
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    setProjection(roundedUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    glUniform4f(roundedUniforms_.rect,
                snap(rect.x + translation_.x), snap(rect.y + translation_.y),
                snap(rect.w), snap(rect.h));
    glUniform4f(roundedUniforms_.color,
                color.r, color.g, color.b, color.a);
    glUniform4f(roundedUniforms_.color2,
                color.r, color.g, color.b, color.a);
    glUniform4f(roundedUniforms_.borderColor, 0, 0, 0, 0);
    glUniform1f(roundedUniforms_.radius, radius.uniform());
    glUniform1f(roundedUniforms_.borderWidth, 0);
    glUniform1f(roundedUniforms_.opacity, opacity);
    glUniform1i(roundedUniforms_.hasGradient, 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawRoundedRectGradient(const Rect& rect, const Gradient& gradient,
                                        const BorderRadius& radius, float opacity) {
    if (gradient.stops.size() < 2) return;

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

    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };
    useShader(roundedRectShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    setProjection(roundedUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    glUniform4f(roundedUniforms_.rect,
                snap(rect.x + translation_.x), snap(rect.y + translation_.y),
                snap(rect.w), snap(rect.h));

    auto& c1 = gradient.stops[0].first;
    auto& c2 = gradient.stops.back().first;
    glUniform4f(roundedUniforms_.color, c1.r, c1.g, c1.b, c1.a);
    glUniform4f(roundedUniforms_.color2, c2.r, c2.g, c2.b, c2.a);
    glUniform4f(roundedUniforms_.borderColor, 0, 0, 0, 0);
    glUniform1f(roundedUniforms_.radius, radius.uniform());
    glUniform1f(roundedUniforms_.borderWidth, 0);
    glUniform1f(roundedUniforms_.opacity, opacity);
    glUniform1i(roundedUniforms_.hasGradient, 1);
    glUniform1f(roundedUniforms_.gradientAngle, gradient.angle);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawBorder(const Rect& rect, const Border& border, const BorderRadius& radius) {
    if (border.width <= 0) return;

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
    useShader(roundedRectShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    setProjection(roundedUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    glUniform4f(roundedUniforms_.rect,
                snap(rect.x + translation_.x), snap(rect.y + translation_.y),
                snap(rect.w), snap(rect.h));
    glUniform4f(roundedUniforms_.color, 0, 0, 0, 0);
    glUniform4f(roundedUniforms_.color2, 0, 0, 0, 0);
    glUniform4f(roundedUniforms_.borderColor,
                border.color.r, border.color.g, border.color.b, border.color.a);
    glUniform1f(roundedUniforms_.radius, radius.uniform());
    glUniform1f(roundedUniforms_.borderWidth, border.width);
    glUniform1f(roundedUniforms_.opacity, 1.0f);
    glUniform1i(roundedUniforms_.hasGradient, 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawBoxShadow(const Rect& rect, const BoxShadow& shadow,
                              const BorderRadius& radius) {
    if (shadow.blur <= 0 && shadow.spread <= 0) return;

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanBoxShadow(rect, shadow, radius);
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

// ============================================================
//  Drawing: Text
// ============================================================

void Renderer::drawText(const std::string& text, const Vec2& pos, const Color& color,
                         float fontSize, FontWeight weight, const std::string& fontName) {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanText(text, pos, color, fontSize, weight, fontName);
        return;
    }

    FontData* fontPtr = getFontForSize(fontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) return;
    auto& font = *fontPtr;

    float scale = fontSize / font.fontSize;

    // Build vertex data for all glyphs into reusable scratch memory.
    auto& vertices = textVertexScratch_;
    vertices.clear();
    vertices.reserve(text.size() * 48);
    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };

    useShader(textShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    setProjection(textUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    float cursorX = snap(pos.x + translation_.x);
    float baselineY = snap(pos.y + font.ascent * scale + translation_.y);
    float boldOffset = (weight == FontWeight::Bold) ? std::max(0.35f, fontSize * 0.018f) : 0.0f;

    // Simple UTF-8 decoder
    auto getNextCodepoint = [](const std::string& s, size_t& i) -> uint32_t {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) return (uint32_t)s[i++];
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            uint32_t cp = ((s[i++] & 0x1F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            uint32_t cp = ((s[i++] & 0x0F) << 12) | ((s[i++] & 0x3F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            uint32_t cp = ((s[i++] & 0x07) << 18) | ((s[i++] & 0x3F) << 12) | ((s[i++] & 0x3F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        return (uint32_t)s[i++];
    };

    for (size_t i = 0; i < text.size(); ) {
        uint32_t c = getNextCodepoint(text, i);
        if (c >= 1024) continue;
        auto& g = font.glyphs[c];
        if (g.xadvance == 0 && c != ' ') continue;

        float w = g.width * scale;
        float h = g.height * scale;

        if (w > 0 && h > 0) {
            float x = snap(cursorX + g.xoff * scale);
            float y = snap(baselineY + g.yoff * scale);

            float data[] = {
                x,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                x+w, y,   g.x1, g.y0, color.r, color.g, color.b, color.a,
                x+w, y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                x,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                x+w, y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                x,   y+h, g.x0, g.y1, color.r, color.g, color.b, color.a,
            };
            vertices.insert(vertices.end(), std::begin(data), std::end(data));

            if (boldOffset > 0.0f) {
                float xb = snap(x + boldOffset);
                float boldData[] = {
                    xb,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                    xb+w, y,   g.x1, g.y0, color.r, color.g, color.b, color.a,
                    xb+w, y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                    xb,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                    xb+w, y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                    xb,   y+h, g.x0, g.y1, color.r, color.g, color.b, color.a,
                };
                vertices.insert(vertices.end(), std::begin(boldData), std::end(boldData));
            }
        }
        cursorX += g.xadvance * scale;
    }

    if (vertices.empty()) return;

    glBindVertexArray(textVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO_);
    if (vertices.size() > textVBOCapacity_) {
        textVBOCapacity_ = std::max(vertices.size(), textVBOCapacity_ * 2);
        glBufferData(GL_ARRAY_BUFFER, textVBOCapacity_ * sizeof(float), nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font.textureId);
    glUniform1i(textUniforms_.texture, 0);

    glDrawArrays(GL_TRIANGLES, 0, (int)(vertices.size() / 8));
}

void Renderer::drawTextInRect(const std::string& text, const Rect& rect, const Color& color,
                               float fontSize, TextAlign align, FontWeight weight,
                               const std::string& fontName) {
    Vec2 textSize = measureText(text, fontSize, fontName);

    float x = rect.x;
    if (align == TextAlign::Center) x = rect.x + (rect.w - textSize.x) / 2;
    else if (align == TextAlign::Right) x = rect.x + rect.w - textSize.x;

    // Vertically center: position so that the text midpoint aligns with rect midpoint
    // textSize.y is the fontSize; the actual visual height is ascent+|descent|
    float textH = fontSize; // fallback
    const FontData* font = findFontForMeasure(fontName, fontSize);
    if (font && font->loaded) {
        float scale = fontSize / font->fontSize;
        float asc = font->ascent * scale;
        float desc = std::abs(font->descent * scale);
        textH = asc + desc;
    }
    float y = rect.y + (rect.h - textH) / 2;
    drawText(text, {x, y}, color, fontSize, weight, fontName);
}

Vec2 Renderer::measureText(const std::string& text, float fontSize,
                            const std::string& fontName) const {
    std::string key;
    key.reserve(fontName.size() + text.size() + 24);
    key += fontName;
    key.push_back('\x1f');
    key += std::to_string((int)std::round(fontSize * 100.0f));
    key.push_back('\x1f');
    key += text;

    auto cached = textMeasureCache_.find(key);
    if (cached != textMeasureCache_.end()) {
        return cached->second;
    }

    const FontData* fontPtr = findFontForMeasure(fontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) return {0, fontSize};
    auto& font = *fontPtr;
    float scale = fontSize / font.fontSize;
    float width = 0;
    auto getNextCodepoint = [](const std::string& s, size_t& i) -> uint32_t {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) return (uint32_t)s[i++];
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            uint32_t cp = ((s[i++] & 0x1F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            uint32_t cp = ((s[i++] & 0x0F) << 12) | ((s[i++] & 0x3F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            uint32_t cp = ((s[i++] & 0x07) << 18) | ((s[i++] & 0x3F) << 12) | ((s[i++] & 0x3F) << 6) | (s[i++] & 0x3F);
            return cp;
        }
        return (uint32_t)s[i++];
    };

    for (size_t i = 0; i < text.size(); ) {
        uint32_t c = getNextCodepoint(text, i);
        if (c < 1024) {
            width += font.glyphs[c].xadvance * scale;
        }
    }
    Vec2 measured = {width, fontSize};
    if (textMeasureCache_.size() > 4096) {
        textMeasureCache_.clear();
    }
    textMeasureCache_[std::move(key)] = measured;
    return measured;
}

// ============================================================
//  Scissor (Clipping)
// ============================================================

void Renderer::pushScissor(const Rect& rect) {
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

    scissorStack_.push_back(clip);
    glEnable(GL_SCISSOR_TEST);

    int x = (int)std::floor(clip.x * dpiScale_);
    int y = (int)std::floor((windowHeight_ - clip.y - clip.h) * dpiScale_);
    int w = (int)std::ceil(clip.w * dpiScale_);
    int h = (int)std::ceil(clip.h * dpiScale_);
    glScissor(x, y, std::max(0, w), std::max(0, h));
}

void Renderer::popScissor() {
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

} // namespace FluxUI
