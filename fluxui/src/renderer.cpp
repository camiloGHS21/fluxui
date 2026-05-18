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

#include <glad/gl.h>
#if FLUXUI_HAS_VULKAN_SDK
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#endif
#include "fluxui/platform.h"

#include <ft2build.h>
#include FT_FREETYPE_H

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
        VmaAllocation allocation = VK_NULL_HANDLE;
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
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        int width = 0;
        int height = 0;
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
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
    VkPipeline imagePipeline = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkSampler fontSampler = VK_NULL_HANDLE;
    std::unordered_map<std::string, FontTexture> fontTextures;
    std::unordered_map<std::string, FontTexture> imageTextures;

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

std::string trimSvgString(std::string value) {
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isWs((unsigned char)value.front())) value.erase(value.begin());
    while (!value.empty() && isWs((unsigned char)value.back())) value.pop_back();
    return value;
}

std::string lowerSvgString(std::string value) {
    for (char& c : value) c = (char)std::tolower((unsigned char)c);
    return value;
}

bool hasSvgSignature(const unsigned char* data, int dataSize) {
    if (!data || dataSize <= 0) return false;
    int n = std::min(dataSize, 1024);
    std::string head(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + n);
    return lowerSvgString(head).find("<svg") != std::string::npos;
}

float parseSvgFloat(const std::string& value, float fallback = 0.0f) {
    std::string s = trimSvgString(value);
    if (s.empty()) return fallback;
    char* end = nullptr;
    float number = std::strtof(s.c_str(), &end);
    if (end == s.c_str()) return fallback;
    return number;
}

std::vector<float> parseSvgNumberList(const std::string& value) {
    std::vector<float> numbers;
    const char* p = value.c_str();
    char* end = nullptr;
    while (*p) {
        while (*p && (std::isspace((unsigned char)*p) || *p == ',')) ++p;
        if (!*p) break;
        float v = std::strtof(p, &end);
        if (end == p) {
            ++p;
            continue;
        }
        numbers.push_back(v);
        p = end;
    }
    return numbers;
}

using SvgAttrs = std::unordered_map<std::string, std::string>;

SvgAttrs parseSvgAttrs(const std::string& tag) {
    SvgAttrs attrs;
    size_t i = 0;
    while (i < tag.size() && tag[i] != '<') ++i;
    if (i < tag.size()) ++i;
    while (i < tag.size() && !std::isspace((unsigned char)tag[i]) && tag[i] != '>' && tag[i] != '/') ++i;

    while (i < tag.size()) {
        while (i < tag.size() && (std::isspace((unsigned char)tag[i]) || tag[i] == '/')) ++i;
        if (i >= tag.size() || tag[i] == '>') break;
        size_t nameStart = i;
        while (i < tag.size() &&
               (std::isalnum((unsigned char)tag[i]) || tag[i] == '-' || tag[i] == '_' || tag[i] == ':')) {
            ++i;
        }
        std::string name = lowerSvgString(tag.substr(nameStart, i - nameStart));
        while (i < tag.size() && std::isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size() || tag[i] != '=') {
            attrs[name] = "";
            continue;
        }
        ++i;
        while (i < tag.size() && std::isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size()) break;
        std::string value;
        char quote = tag[i];
        if (quote == '"' || quote == '\'') {
            ++i;
            size_t valueStart = i;
            while (i < tag.size() && tag[i] != quote) ++i;
            value = tag.substr(valueStart, i - valueStart);
            if (i < tag.size()) ++i;
        } else {
            size_t valueStart = i;
            while (i < tag.size() && !std::isspace((unsigned char)tag[i]) && tag[i] != '>') ++i;
            value = tag.substr(valueStart, i - valueStart);
        }
        if (!name.empty()) attrs[name] = trimSvgString(value);
    }

    auto styleIt = attrs.find("style");
    if (styleIt != attrs.end()) {
        std::istringstream ss(styleIt->second);
        std::string item;
        while (std::getline(ss, item, ';')) {
            size_t colon = item.find(':');
            if (colon == std::string::npos) continue;
            std::string key = lowerSvgString(trimSvgString(item.substr(0, colon)));
            std::string val = trimSvgString(item.substr(colon + 1));
            if (!key.empty()) attrs[key] = val;
        }
    }

    return attrs;
}

std::string svgAttr(const SvgAttrs& attrs, const std::string& name, const std::string& fallback = "") {
    auto it = attrs.find(name);
    return it == attrs.end() ? fallback : it->second;
}

Color parseSvgColor(const std::string& raw, Color fallback, bool* none = nullptr) {
    if (none) *none = false;
    std::string value = lowerSvgString(trimSvgString(raw));
    if (value.empty()) return fallback;
    if (value == "none") {
        if (none) *none = true;
        return fallback.withAlpha(0.0f);
    }
    if (value == "transparent") return Color(0, 0, 0, 0);
    if (value == "currentcolor") return fallback;
    if (!value.empty() && value[0] == '#') return Color::fromHex(value);
    if (value.rfind("rgb(", 0) == 0 || value.rfind("rgba(", 0) == 0) {
        auto start = value.find('(');
        auto end = value.find(')', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::vector<float> nums = parseSvgNumberList(value.substr(start + 1, end - start - 1));
            if (nums.size() >= 3) {
                float a = nums.size() >= 4 ? nums[3] : 1.0f;
                return Color(nums[0] / 255.0f, nums[1] / 255.0f, nums[2] / 255.0f, a);
            }
        }
    }

    struct Named { const char* name; Color color; };
    static const Named named[] = {
        {"black", Color(0, 0, 0, 1)}, {"white", Color(1, 1, 1, 1)},
        {"red", Color(1, 0, 0, 1)}, {"green", Color(0, 0.5f, 0, 1)},
        {"blue", Color(0, 0, 1, 1)}, {"yellow", Color(1, 1, 0, 1)},
        {"cyan", Color(0, 1, 1, 1)}, {"magenta", Color(1, 0, 1, 1)},
        {"gray", Color(0.5f, 0.5f, 0.5f, 1)}, {"grey", Color(0.5f, 0.5f, 0.5f, 1)}
    };
    for (const auto& item : named) {
        if (value == item.name) return item.color;
    }
    return fallback;
}

struct SvgCanvas {
    std::vector<unsigned char>* pixels = nullptr;
    int width = 0;
    int height = 0;
    float viewX = 0.0f;
    float viewY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

Vec2 svgMapPoint(const SvgCanvas& canvas, float x, float y) {
    return {(x - canvas.viewX) * canvas.scaleX,
            (y - canvas.viewY) * canvas.scaleY};
}

void svgBlendPixel(SvgCanvas& canvas, int x, int y, Color color, float coverage = 1.0f) {
    if (!canvas.pixels || x < 0 || y < 0 || x >= canvas.width || y >= canvas.height) return;
    float srcA = std::clamp(color.a * coverage, 0.0f, 1.0f);
    if (srcA <= 0.0f) return;
    size_t index = ((size_t)y * (size_t)canvas.width + (size_t)x) * 4;
    float dstR = (*canvas.pixels)[index + 0] / 255.0f;
    float dstG = (*canvas.pixels)[index + 1] / 255.0f;
    float dstB = (*canvas.pixels)[index + 2] / 255.0f;
    float dstA = (*canvas.pixels)[index + 3] / 255.0f;
    float outA = srcA + dstA * (1.0f - srcA);
    if (outA <= 0.0f) return;
    float outR = (color.r * srcA + dstR * dstA * (1.0f - srcA)) / outA;
    float outG = (color.g * srcA + dstG * dstA * (1.0f - srcA)) / outA;
    float outB = (color.b * srcA + dstB * dstA * (1.0f - srcA)) / outA;
    (*canvas.pixels)[index + 0] = (unsigned char)std::round(std::clamp(outR, 0.0f, 1.0f) * 255.0f);
    (*canvas.pixels)[index + 1] = (unsigned char)std::round(std::clamp(outG, 0.0f, 1.0f) * 255.0f);
    (*canvas.pixels)[index + 2] = (unsigned char)std::round(std::clamp(outB, 0.0f, 1.0f) * 255.0f);
    (*canvas.pixels)[index + 3] = (unsigned char)std::round(std::clamp(outA, 0.0f, 1.0f) * 255.0f);
}

float pointSegmentDistance(float px, float py, Vec2 a, Vec2 b) {
    float vx = b.x - a.x;
    float vy = b.y - a.y;
    float wx = px - a.x;
    float wy = py - a.y;
    float len2 = vx * vx + vy * vy;
    float t = len2 > 0.0f ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
    float dx = px - (a.x + vx * t);
    float dy = py - (a.y + vy * t);
    return std::sqrt(dx * dx + dy * dy);
}

void strokePolyline(SvgCanvas& canvas, const std::vector<Vec2>& points, Color color, float strokeWidth, bool close) {
    if (points.size() < 2 || color.a <= 0.0f || strokeWidth <= 0.0f) return;
    float half = std::max(0.5f, strokeWidth * 0.5f);
    for (size_t i = 1; i < points.size() + (close ? 1 : 0); ++i) {
        Vec2 a = points[i - 1];
        Vec2 b = points[i % points.size()];
        int minX = (int)std::floor(std::min(a.x, b.x) - half - 1.0f);
        int maxX = (int)std::ceil(std::max(a.x, b.x) + half + 1.0f);
        int minY = (int)std::floor(std::min(a.y, b.y) - half - 1.0f);
        int maxY = (int)std::ceil(std::max(a.y, b.y) + half + 1.0f);
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float d = pointSegmentDistance(x + 0.5f, y + 0.5f, a, b);
                if (d <= half + 0.75f) {
                    float coverage = std::clamp(half + 0.75f - d, 0.0f, 1.0f);
                    svgBlendPixel(canvas, x, y, color, coverage);
                }
            }
        }
    }
}

bool pointInPolygon(float x, float y, const std::vector<Vec2>& points) {
    bool inside = false;
    if (points.size() < 3) return false;
    for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
        const Vec2& pi = points[i];
        const Vec2& pj = points[j];
        bool crosses = ((pi.y > y) != (pj.y > y)) &&
                       (x < (pj.x - pi.x) * (y - pi.y) / ((pj.y - pi.y) + 0.000001f) + pi.x);
        if (crosses) inside = !inside;
    }
    return inside;
}

void fillPolygon(SvgCanvas& canvas, const std::vector<Vec2>& points, Color color) {
    if (points.size() < 3 || color.a <= 0.0f) return;
    float minX = points[0].x, maxX = points[0].x, minY = points[0].y, maxY = points[0].y;
    for (const Vec2& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    for (int y = (int)std::floor(minY); y <= (int)std::ceil(maxY); ++y) {
        for (int x = (int)std::floor(minX); x <= (int)std::ceil(maxX); ++x) {
            if (pointInPolygon(x + 0.5f, y + 0.5f, points)) {
                svgBlendPixel(canvas, x, y, color);
            }
        }
    }
}

void drawSvgRect(SvgCanvas& canvas, float x, float y, float w, float h,
                 float rx, float ry, Color fill, Color stroke, float strokeWidth) {
    if (w <= 0.0f || h <= 0.0f) return;
    Vec2 p0 = svgMapPoint(canvas, x, y);
    Vec2 p1 = svgMapPoint(canvas, x + w, y + h);
    float left = std::min(p0.x, p1.x);
    float right = std::max(p0.x, p1.x);
    float top = std::min(p0.y, p1.y);
    float bottom = std::max(p0.y, p1.y);
    float rr = std::max(rx * canvas.scaleX, ry * canvas.scaleY);
    for (int py = (int)std::floor(top); py <= (int)std::ceil(bottom); ++py) {
        for (int px = (int)std::floor(left); px <= (int)std::ceil(right); ++px) {
            float cx = px + 0.5f;
            float cy = py + 0.5f;
            bool inside = cx >= left && cx <= right && cy >= top && cy <= bottom;
            if (inside && rr > 0.0f) {
                float qx = std::max(std::max(left + rr - cx, 0.0f), cx - (right - rr));
                float qy = std::max(std::max(top + rr - cy, 0.0f), cy - (bottom - rr));
                inside = qx * qx + qy * qy <= rr * rr;
            }
            if (inside && fill.a > 0.0f) svgBlendPixel(canvas, px, py, fill);
        }
    }
    if (stroke.a > 0.0f && strokeWidth > 0.0f) {
        std::vector<Vec2> points = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
        strokePolyline(canvas, points, stroke, strokeWidth, true);
    }
}

void drawSvgEllipse(SvgCanvas& canvas, float cx, float cy, float rx, float ry,
                    Color fill, Color stroke, float strokeWidth) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    Vec2 c = svgMapPoint(canvas, cx, cy);
    float prx = std::abs(rx * canvas.scaleX);
    float pry = std::abs(ry * canvas.scaleY);
    int minX = (int)std::floor(c.x - prx - strokeWidth - 1.0f);
    int maxX = (int)std::ceil(c.x + prx + strokeWidth + 1.0f);
    int minY = (int)std::floor(c.y - pry - strokeWidth - 1.0f);
    int maxY = (int)std::ceil(c.y + pry + strokeWidth + 1.0f);
    float strokeNorm = strokeWidth > 0.0f ? strokeWidth / std::max(1.0f, std::min(prx, pry)) : 0.0f;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float nx = (x + 0.5f - c.x) / std::max(1.0f, prx);
            float ny = (y + 0.5f - c.y) / std::max(1.0f, pry);
            float d = std::sqrt(nx * nx + ny * ny);
            if (d <= 1.0f && fill.a > 0.0f) svgBlendPixel(canvas, x, y, fill, std::clamp(1.5f - d, 0.0f, 1.0f));
            if (stroke.a > 0.0f && strokeNorm > 0.0f && std::abs(d - 1.0f) <= strokeNorm + 0.02f) {
                svgBlendPixel(canvas, x, y, stroke);
            }
        }
    }
}

std::vector<Vec2> parseSvgPoints(const std::string& value, const SvgCanvas& canvas) {
    std::vector<Vec2> points;
    std::vector<float> nums = parseSvgNumberList(value);
    for (size_t i = 1; i < nums.size(); i += 2) {
        points.push_back(svgMapPoint(canvas, nums[i - 1], nums[i]));
    }
    return points;
}

struct SvgPathParser {
    const std::string& d;
    size_t pos = 0;
    char command = 0;

    void skip() {
        while (pos < d.size() && (std::isspace((unsigned char)d[pos]) || d[pos] == ',')) ++pos;
    }

    bool hasNumber() {
        skip();
        return pos < d.size() && (d[pos] == '-' || d[pos] == '+' || d[pos] == '.' ||
                                  std::isdigit((unsigned char)d[pos]));
    }

    bool readNumber(float& value) {
        skip();
        if (pos >= d.size()) return false;
        char* end = nullptr;
        value = std::strtof(d.c_str() + pos, &end);
        if (end == d.c_str() + pos) return false;
        pos = (size_t)(end - d.c_str());
        return true;
    }

    bool readCommand() {
        skip();
        if (pos < d.size() && std::isalpha((unsigned char)d[pos])) {
            command = d[pos++];
            return true;
        }
        return command != 0;
    }
};

void appendCubic(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3) {
    for (int i = 1; i <= 16; ++i) {
        float t = i / 16.0f;
        float it = 1.0f - t;
        out.push_back({
            it * it * it * p0.x + 3 * it * it * t * p1.x + 3 * it * t * t * p2.x + t * t * t * p3.x,
            it * it * it * p0.y + 3 * it * it * t * p1.y + 3 * it * t * t * p2.y + t * t * t * p3.y
        });
    }
}

void appendQuad(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2) {
    for (int i = 1; i <= 12; ++i) {
        float t = i / 12.0f;
        float it = 1.0f - t;
        out.push_back({
            it * it * p0.x + 2 * it * t * p1.x + t * t * p2.x,
            it * it * p0.y + 2 * it * t * p1.y + t * t * p2.y
        });
    }
}

void drawSvgPath(SvgCanvas& canvas, const std::string& d, Color fill, Color stroke, float strokeWidth) {
    SvgPathParser parser{d};
    std::vector<std::vector<Vec2>> subpaths;
    std::vector<Vec2> currentPath;
    Vec2 current = {0, 0};
    Vec2 start = {0, 0};
    bool closed = false;

    auto flushPath = [&]() {
        if (!currentPath.empty()) {
            subpaths.push_back(currentPath);
            currentPath.clear();
        }
    };

    while (parser.pos < d.size() && parser.readCommand()) {
        char cmd = parser.command;
        bool rel = std::islower((unsigned char)cmd) != 0;
        char upper = (char)std::toupper((unsigned char)cmd);
        if (upper == 'Z') {
            if (!currentPath.empty()) currentPath.push_back(start);
            current = start;
            closed = true;
            parser.command = 0;
            continue;
        }

        auto map = [&](float x, float y) {
            return rel ? Vec2(current.x + x, current.y + y) : Vec2(x, y);
        };

        if (upper == 'M') {
            float x = 0, y = 0;
            if (!parser.readNumber(x) || !parser.readNumber(y)) break;
            flushPath();
            current = map(x, y);
            start = current;
            currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            parser.command = rel ? 'l' : 'L';
            closed = false;
        } else if (upper == 'L') {
            while (parser.hasNumber()) {
                float x = 0, y = 0;
                if (!parser.readNumber(x) || !parser.readNumber(y)) break;
                current = map(x, y);
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
        } else if (upper == 'H') {
            while (parser.hasNumber()) {
                float x = 0;
                if (!parser.readNumber(x)) break;
                current.x = rel ? current.x + x : x;
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
        } else if (upper == 'V') {
            while (parser.hasNumber()) {
                float y = 0;
                if (!parser.readNumber(y)) break;
                current.y = rel ? current.y + y : y;
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
        } else if (upper == 'C') {
            while (parser.hasNumber()) {
                float x1, y1, x2, y2, x, y;
                if (!parser.readNumber(x1) || !parser.readNumber(y1) ||
                    !parser.readNumber(x2) || !parser.readNumber(y2) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = rel ? svgMapPoint(canvas, current.x + x1, current.y + y1) : svgMapPoint(canvas, x1, y1);
                Vec2 c2 = rel ? svgMapPoint(canvas, current.x + x2, current.y + y2) : svgMapPoint(canvas, x2, y2);
                current = map(x, y);
                Vec2 p3 = svgMapPoint(canvas, current.x, current.y);
                appendCubic(currentPath, p0, c1, c2, p3);
            }
        } else if (upper == 'Q') {
            while (parser.hasNumber()) {
                float x1, y1, x, y;
                if (!parser.readNumber(x1) || !parser.readNumber(y1) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = rel ? svgMapPoint(canvas, current.x + x1, current.y + y1) : svgMapPoint(canvas, x1, y1);
                current = map(x, y);
                Vec2 p2 = svgMapPoint(canvas, current.x, current.y);
                appendQuad(currentPath, p0, c1, p2);
            }
        } else if (upper == 'A') {
            while (parser.hasNumber()) {
                float rx, ry, rot, largeArc, sweep, x, y;
                if (!parser.readNumber(rx) || !parser.readNumber(ry) || !parser.readNumber(rot) ||
                    !parser.readNumber(largeArc) || !parser.readNumber(sweep) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                (void)rx; (void)ry; (void)rot; (void)largeArc; (void)sweep;
                current = map(x, y);
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
        } else {
            parser.command = 0;
            ++parser.pos;
        }
    }
    flushPath();

    for (const auto& path : subpaths) {
        if (fill.a > 0.0f && path.size() >= 3) fillPolygon(canvas, path, fill);
    }
    for (const auto& path : subpaths) {
        if (stroke.a > 0.0f) strokePolyline(canvas, path, stroke, strokeWidth, closed);
    }
}

bool rasterizeSvgToRgba(const unsigned char* data, int dataSize, ImageData& image) {
    if (!hasSvgSignature(data, dataSize)) return false;
    std::string svg(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + dataSize);
    size_t rootPos = lowerSvgString(svg.substr(0, std::min<size_t>(svg.size(), 4096))).find("<svg");
    if (rootPos == std::string::npos) return false;
    size_t rootEnd = svg.find('>', rootPos);
    std::string rootTag = rootEnd == std::string::npos ? svg.substr(rootPos) : svg.substr(rootPos, rootEnd - rootPos + 1);
    SvgAttrs root = parseSvgAttrs(rootTag);

    std::vector<float> vb = parseSvgNumberList(svgAttr(root, "viewbox"));
    float viewX = vb.size() >= 4 ? vb[0] : 0.0f;
    float viewY = vb.size() >= 4 ? vb[1] : 0.0f;
    float viewW = vb.size() >= 4 ? vb[2] : 0.0f;
    float viewH = vb.size() >= 4 ? vb[3] : 0.0f;
    float width = parseSvgFloat(svgAttr(root, "width"), viewW > 0.0f ? viewW : 300.0f);
    float height = parseSvgFloat(svgAttr(root, "height"), viewH > 0.0f ? viewH : 150.0f);
    if (viewW <= 0.0f) viewW = width;
    if (viewH <= 0.0f) viewH = height;
    int outW = std::clamp((int)std::round(width), 1, 4096);
    int outH = std::clamp((int)std::round(height), 1, 4096);
    if ((size_t)outW * (size_t)outH > 16u * 1024u * 1024u) return false;

    image.width = outW;
    image.height = outH;
    image.svg = true;
    image.textureId = 0;
    image.loaded = true;
    image.pixels.assign((size_t)outW * (size_t)outH * 4u, 0);

    SvgCanvas canvas;
    canvas.pixels = &image.pixels;
    canvas.width = outW;
    canvas.height = outH;
    canvas.viewX = viewX;
    canvas.viewY = viewY;
    canvas.scaleX = outW / std::max(1.0f, viewW);
    canvas.scaleY = outH / std::max(1.0f, viewH);
    std::string inheritedFill = svgAttr(root, "fill", "black");
    std::string inheritedStroke = svgAttr(root, "stroke", "none");
    std::string inheritedOpacity = svgAttr(root, "opacity", "1");
    std::string inheritedFillOpacity = svgAttr(root, "fill-opacity", "1");
    std::string inheritedStrokeOpacity = svgAttr(root, "stroke-opacity", "1");
    std::string inheritedStrokeWidth = svgAttr(root, "stroke-width", "1");

    size_t pos = 0;
    while ((pos = svg.find('<', pos)) != std::string::npos) {
        if (pos + 1 >= svg.size() || svg[pos + 1] == '/' || svg[pos + 1] == '!' || svg[pos + 1] == '?') {
            ++pos;
            continue;
        }
        size_t end = svg.find('>', pos);
        if (end == std::string::npos) break;
        std::string tag = svg.substr(pos, end - pos + 1);
        std::string lower = lowerSvgString(tag.substr(1, std::min<size_t>(tag.size(), 16)));
        SvgAttrs attrs = parseSvgAttrs(tag);
        float opacity = parseSvgFloat(svgAttr(attrs, "opacity", inheritedOpacity), 1.0f);
        bool noFill = false;
        bool noStroke = false;
        Color currentColor = parseSvgColor(svgAttr(attrs, "color", svgAttr(root, "color", "black")),
                                           Color(0, 0, 0, 1));
        Color fill = parseSvgColor(svgAttr(attrs, "fill", inheritedFill), currentColor, &noFill);
        Color stroke = parseSvgColor(svgAttr(attrs, "stroke", inheritedStroke), currentColor, &noStroke);
        fill.a *= parseSvgFloat(svgAttr(attrs, "fill-opacity", inheritedFillOpacity), 1.0f) * opacity;
        stroke.a *= parseSvgFloat(svgAttr(attrs, "stroke-opacity", inheritedStrokeOpacity), 1.0f) * opacity;
        if (noFill) fill.a = 0.0f;
        if (noStroke) stroke.a = 0.0f;
        float strokeWidth = parseSvgFloat(svgAttr(attrs, "stroke-width", inheritedStrokeWidth), 1.0f) *
                            (std::abs(canvas.scaleX) + std::abs(canvas.scaleY)) * 0.5f;

        if (lower.rfind("rect", 0) == 0) {
            drawSvgRect(canvas,
                        parseSvgFloat(svgAttr(attrs, "x")),
                        parseSvgFloat(svgAttr(attrs, "y")),
                        parseSvgFloat(svgAttr(attrs, "width")),
                        parseSvgFloat(svgAttr(attrs, "height")),
                        parseSvgFloat(svgAttr(attrs, "rx")),
                        parseSvgFloat(svgAttr(attrs, "ry")),
                        fill,
                        stroke,
                        strokeWidth);
        } else if (lower.rfind("circle", 0) == 0) {
            float r = parseSvgFloat(svgAttr(attrs, "r"));
            drawSvgEllipse(canvas, parseSvgFloat(svgAttr(attrs, "cx")),
                           parseSvgFloat(svgAttr(attrs, "cy")), r, r, fill, stroke, strokeWidth);
        } else if (lower.rfind("ellipse", 0) == 0) {
            drawSvgEllipse(canvas, parseSvgFloat(svgAttr(attrs, "cx")),
                           parseSvgFloat(svgAttr(attrs, "cy")),
                           parseSvgFloat(svgAttr(attrs, "rx")),
                           parseSvgFloat(svgAttr(attrs, "ry")),
                           fill, stroke, strokeWidth);
        } else if (lower.rfind("line", 0) == 0) {
            std::vector<Vec2> points = {
                svgMapPoint(canvas, parseSvgFloat(svgAttr(attrs, "x1")), parseSvgFloat(svgAttr(attrs, "y1"))),
                svgMapPoint(canvas, parseSvgFloat(svgAttr(attrs, "x2")), parseSvgFloat(svgAttr(attrs, "y2")))
            };
            strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
        } else if (lower.rfind("polyline", 0) == 0) {
            auto points = parseSvgPoints(svgAttr(attrs, "points"), canvas);
            strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
        } else if (lower.rfind("polygon", 0) == 0) {
            auto points = parseSvgPoints(svgAttr(attrs, "points"), canvas);
            fillPolygon(canvas, points, fill);
            strokePolyline(canvas, points, stroke, strokeWidth, true);
        } else if (lower.rfind("path", 0) == 0) {
            drawSvgPath(canvas, svgAttr(attrs, "d"), fill, stroke, std::max(1.0f, strokeWidth));
        }
        pos = end + 1;
    }

    return true;
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

uint32_t Renderer::windowFlags() const {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        return vulkanBackendInfo().windowFlags();
    }
    if (activeBackend_ == RenderBackendType::Direct3D12) {
        return direct3D12BackendInfo().windowFlags();
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

bool deviceSupportsVulkanExtension(VkPhysicalDevice device, const char* extensionName) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (extensionCount > 0) {
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());
    }

    for (const auto& extension : extensions) {
        if (std::strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }
    return false;
}

bool deviceSupportsVulkanSwapchain(VkPhysicalDevice device) {
    return deviceSupportsVulkanExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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

void destroyVulkanDynamicPage(VulkanRendererState& state,
                              VulkanRendererState::DynamicPage& page) {
    if (page.mapped && page.allocation && state.allocator) {
        vmaUnmapMemory(state.allocator, page.allocation);
        page.mapped = nullptr;
    }
    if (page.buffer && page.allocation && state.allocator) {
        vmaDestroyBuffer(state.allocator, page.buffer, page.allocation);
        page.buffer = VK_NULL_HANDLE;
        page.allocation = VK_NULL_HANDLE;
    }
    page.capacityBytes = 0;
    page.bytesUsed = 0;
}

void destroyVulkanSwapchain(VulkanRendererState& state) {
    if (!state.device) {
        return;
    }

    auto destroyDynamicPool = [&](VulkanRendererState::DynamicPool& pool) {
        for (auto& page : pool.pages) {
            destroyVulkanDynamicPage(state, page);
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

    uint32_t imageCount = support.capabilities.minImageCount;
#if !FLUXUI_LOW_MEMORY
    imageCount += 1;
#endif
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

#include "vulkan_shaders.h"
#include "vulkan_pipeline_cache.h"

VkShaderModule createVulkanShaderModule(VkDevice device,
                                        const uint32_t* code,
                                        size_t codeSize,
                                        const char* label) {
    if (!code || codeSize == 0) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = codeSize;
    info.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(device, &info, nullptr, &module);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage(label, result) << std::endl;
        return VK_NULL_HANDLE;
    }
    return module;
}

bool createVulkanBuffer(VulkanRendererState& state,
                        VkDeviceSize size,
                        VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties,
                        VkBuffer& buffer,
                        VmaAllocation& allocation,
                        const char* label = nullptr) {
    if (!state.allocator) {
        std::cerr << "FluxUI: Vulkan memory allocator is not initialized." << std::endl;
        return false;
    }

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = properties;
    if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkResult result = vmaCreateBuffer(state.allocator,
                                      &bufferInfo,
                                      &allocInfo,
                                      &buffer,
                                      &allocation,
                                      nullptr);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vmaCreateBuffer", result) << std::endl;
        return false;
    }

    if (label) {
        vmaSetAllocationName(state.allocator, allocation, label);
    }
    return true;
}

bool createVulkanImage(VulkanRendererState& state,
                       const VkImageCreateInfo& imageInfo,
                       VkMemoryPropertyFlags properties,
                       VkImage& image,
                       VmaAllocation& allocation,
                       const char* label = nullptr) {
    if (!state.allocator) {
        std::cerr << "FluxUI: Vulkan memory allocator is not initialized." << std::endl;
        return false;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = properties;

    VkResult result = vmaCreateImage(state.allocator,
                                     &imageInfo,
                                     &allocInfo,
                                     &image,
                                     &allocation,
                                     nullptr);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vmaCreateImage", result) << std::endl;
        return false;
    }

    if (label) {
        vmaSetAllocationName(state.allocator, allocation, label);
    }
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
    VkShaderModule roundedVertModule = createVulkanShaderModule(state.device, spv_vulkan_rounded_vertex, sizeof(spv_vulkan_rounded_vertex), "rounded vertex module");
    VkShaderModule roundedFragModule = createVulkanShaderModule(state.device, spv_vulkan_rounded_fragment, sizeof(spv_vulkan_rounded_fragment), "rounded fragment module");
    VkShaderModule textVertModule = createVulkanShaderModule(state.device, spv_vulkan_text_vertex, sizeof(spv_vulkan_text_vertex), "text vertex module");
    VkShaderModule textFragModule = createVulkanShaderModule(state.device, spv_vulkan_text_fragment, sizeof(spv_vulkan_text_fragment), "text fragment module");
    VkShaderModule imageFragModule = createVulkanShaderModule(state.device, spv_vulkan_image_fragment, sizeof(spv_vulkan_image_fragment), "image fragment module");

    // Create pipeline cache from embedded data for faster pipeline creation
    if (state.pipelineCache == VK_NULL_HANDLE) {
        VkPipelineCacheCreateInfo cacheInfo = {};
        cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        cacheInfo.initialDataSize = kEmbeddedVulkanPipelineCacheSize;
        cacheInfo.pInitialData = kEmbeddedVulkanPipelineCache;
        VkResult cacheResult = vkCreatePipelineCache(state.device, &cacheInfo, nullptr, &state.pipelineCache);
        if (cacheResult != VK_SUCCESS) {
            // Fall back to empty cache
            std::cerr << "[FluxUI] Embedded pipeline cache rejected (vendor mismatch?), creating empty cache" << std::endl;
            cacheInfo.initialDataSize = 0;
            cacheInfo.pInitialData = nullptr;
            vkCreatePipelineCache(state.device, &cacheInfo, nullptr, &state.pipelineCache);
        }
    }
    if (!roundedVertModule || !roundedFragModule || !textVertModule || !textFragModule || !imageFragModule) {
        if (roundedVertModule) vkDestroyShaderModule(state.device, roundedVertModule, nullptr);
        if (roundedFragModule) vkDestroyShaderModule(state.device, roundedFragModule, nullptr);
        if (textVertModule) vkDestroyShaderModule(state.device, textVertModule, nullptr);
        if (textFragModule) vkDestroyShaderModule(state.device, textFragModule, nullptr);
        if (imageFragModule) vkDestroyShaderModule(state.device, imageFragModule, nullptr);
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
                                       state.pipelineCache,
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
                                       state.pipelineCache,
                                       1,
                                       &pipelineInfo,
                                       nullptr,
                                       &state.textPipeline);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateGraphicsPipelines(text)", result) << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo imageStages[] = {
        makeStage(VK_SHADER_STAGE_VERTEX_BIT, textVertModule),
        makeStage(VK_SHADER_STAGE_FRAGMENT_BIT, imageFragModule)
    };
    pipelineInfo.pStages = imageStages;
    pipelineInfo.pVertexInputState = &textVertexInput;
    pipelineInfo.layout = state.textPipelineLayout;
    result = vkCreateGraphicsPipelines(state.device,
                                       state.pipelineCache,
                                       1,
                                       &pipelineInfo,
                                       nullptr,
                                       &state.imagePipeline);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateGraphicsPipelines(image)", result) << std::endl;
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
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    result = vkCreateSampler(state.device, &samplerInfo, nullptr, &state.fontSampler);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateSampler", result) << std::endl;
        return false;
    }

    vkDestroyShaderModule(state.device, roundedVertModule, nullptr);
    vkDestroyShaderModule(state.device, roundedFragModule, nullptr);
    vkDestroyShaderModule(state.device, textVertModule, nullptr);
    vkDestroyShaderModule(state.device, textFragModule, nullptr);
    vkDestroyShaderModule(state.device, imageFragModule, nullptr);
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
        if (texture.image && texture.allocation && state.allocator) {
            vmaDestroyImage(state.allocator, texture.image, texture.allocation);
        }
    }
    state.fontTextures.clear();
    for (auto& item : state.imageTextures) {
        auto& texture = item.second;
        if (texture.view) vkDestroyImageView(state.device, texture.view, nullptr);
        if (texture.image && texture.allocation && state.allocator) {
            vmaDestroyImage(state.allocator, texture.image, texture.allocation);
        }
    }
    state.imageTextures.clear();

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
    if (state.imagePipeline) {
        vkDestroyPipeline(state.device, state.imagePipeline, nullptr);
        state.imagePipeline = VK_NULL_HANDLE;
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
    if (state.pipelineCache) {
        vkDestroyPipelineCache(state.device, state.pipelineCache, nullptr);
        state.pipelineCache = VK_NULL_HANDLE;
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
                            page.allocation,
                            label)) {
        return false;
    }

    VkResult result = vmaMapMemory(state.allocator, page.allocation, &page.mapped);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage(label, result) << std::endl;
        vmaDestroyBuffer(state.allocator, page.buffer, page.allocation);
        page.buffer = VK_NULL_HANDLE;
        page.allocation = VK_NULL_HANDLE;
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

void trimVulkanDynamicPool(VulkanRendererState& state,
                           VulkanRendererState::DynamicPool& pool,
                           size_t retainedPages) {
    if (pool.pages.size() <= retainedPages) {
        return;
    }
    for (size_t i = retainedPages; i < pool.pages.size(); ++i) {
        destroyVulkanDynamicPage(state, pool.pages[i]);
    }
    pool.pages.resize(retainedPages);
    if (pool.currentPage >= pool.pages.size()) {
        pool.currentPage = 0;
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
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    if (!createVulkanBuffer(state,
                            imageSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            stagingBuffer,
                            stagingAllocation,
                            "font atlas staging")) {
        return false;
    }

    auto destroyStaging = [&]() {
        if (stagingBuffer && stagingAllocation) {
            vmaDestroyBuffer(state.allocator, stagingBuffer, stagingAllocation);
            stagingBuffer = VK_NULL_HANDLE;
            stagingAllocation = VK_NULL_HANDLE;
        }
    };

    void* mapped = nullptr;
    VkResult result = vmaMapMemory(state.allocator, stagingAllocation, &mapped);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vmaMapMemory(font staging)", result) << std::endl;
        destroyStaging();
        return false;
    }
    std::memcpy(mapped, font.atlasPixels.data(), font.atlasPixels.size());
    vmaUnmapMemory(state.allocator, stagingAllocation);

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

    if (!createVulkanImage(state,
                           imageInfo,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           texture.image,
                           texture.allocation,
                           "font atlas image")) {
        destroyStaging();
        return false;
    }

    auto destroyTexture = [&]() {
        if (texture.view) {
            vkDestroyImageView(state.device, texture.view, nullptr);
            texture.view = VK_NULL_HANDLE;
        }
        if (texture.image && texture.allocation) {
            vmaDestroyImage(state.allocator, texture.image, texture.allocation);
            texture.image = VK_NULL_HANDLE;
            texture.allocation = VK_NULL_HANDLE;
        }
    };

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

    destroyStaging();

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
        destroyTexture();
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
        destroyTexture();
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

bool ensureVulkanImageTexture(VulkanRendererState& state,
                              const std::string& key,
                              ImageData& image) {
    auto existing = state.imageTextures.find(key);
    if (existing != state.imageTextures.end()) {
        return existing->second.descriptorSet != VK_NULL_HANDLE;
    }
    if (!image.loaded || image.pixels.empty() || image.width <= 0 || image.height <= 0) {
        return false;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(image.pixels.size());
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    if (!createVulkanBuffer(state,
                            imageSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            stagingBuffer,
                            stagingAllocation,
                            "image staging")) {
        return false;
    }

    auto destroyStaging = [&]() {
        if (stagingBuffer && stagingAllocation) {
            vmaDestroyBuffer(state.allocator, stagingBuffer, stagingAllocation);
            stagingBuffer = VK_NULL_HANDLE;
            stagingAllocation = VK_NULL_HANDLE;
        }
    };

    void* mapped = nullptr;
    VkResult result = vmaMapMemory(state.allocator, stagingAllocation, &mapped);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vmaMapMemory(image staging)", result) << std::endl;
        destroyStaging();
        return false;
    }
    std::memcpy(mapped, image.pixels.data(), image.pixels.size());
    vmaUnmapMemory(state.allocator, stagingAllocation);

    VulkanRendererState::FontTexture texture;
    texture.width = image.width;
    texture.height = image.height;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {
        static_cast<uint32_t>(image.width),
        static_cast<uint32_t>(image.height),
        1
    };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (!createVulkanImage(state,
                           imageInfo,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                           texture.image,
                           texture.allocation,
                           "image texture")) {
        destroyStaging();
        return false;
    }

    auto destroyTexture = [&]() {
        if (texture.view) {
            vkDestroyImageView(state.device, texture.view, nullptr);
            texture.view = VK_NULL_HANDLE;
        }
        if (texture.image && texture.allocation) {
            vmaDestroyImage(state.allocator, texture.image, texture.allocation);
            texture.image = VK_NULL_HANDLE;
            texture.allocation = VK_NULL_HANDLE;
        }
    };

    transitionVulkanImage(state,
                          texture.image,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyVulkanBufferToImage(state,
                            stagingBuffer,
                            texture.image,
                            static_cast<uint32_t>(image.width),
                            static_cast<uint32_t>(image.height));
    transitionVulkanImage(state,
                          texture.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    destroyStaging();

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    result = vkCreateImageView(state.device, &viewInfo, nullptr, &texture.view);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateImageView(image)", result) << std::endl;
        destroyTexture();
        return false;
    }

    VkDescriptorSetAllocateInfo descriptorInfo = {};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorInfo.descriptorPool = state.descriptorPool;
    descriptorInfo.descriptorSetCount = 1;
    descriptorInfo.pSetLayouts = &state.textDescriptorSetLayout;
    result = vkAllocateDescriptorSets(state.device, &descriptorInfo, &texture.descriptorSet);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkAllocateDescriptorSets(image)", result) << std::endl;
        destroyTexture();
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

    state.imageTextures.emplace(key, texture);
    image.pixels.clear();
    image.pixels.shrink_to_fit();
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

void Renderer::setupInstanceBuffer() {
    constexpr size_t initialCapacity = 256; // 256 instances
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

bool Renderer::initVulkan(void* windowHandle) {
#if !FLUXUI_HAS_VULKAN_SDK
    (void)windowHandle;
    std::cerr << "FluxUI: Vulkan backend needs the Vulkan SDK/loader at build time." << std::endl;
    return false;
#else
    vulkan_ = std::make_unique<VulkanRendererState>();
    auto& state = *vulkan_;

    int w = 800, h = 600;
    Platform::getWindowSize(windowHandle, w, h);
    windowWidth_ = std::max(1, w);
    windowHeight_ = std::max(1, h);

    std::vector<const char*> extensions = Platform::getVulkanInstanceExtensions();

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

    if (!Platform::createVulkanSurface(state.instance, windowHandle, &state.surface)) {
        std::cerr << "Failed to create Vulkan surface" << std::endl;
        shutdownVulkan();
        return false;
    }

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

    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const bool memoryBudgetSupported =
        deviceSupportsVulkanExtension(state.physicalDevice, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    if (memoryBudgetSupported) {
        deviceExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }
    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    result = vkCreateDevice(state.physicalDevice, &deviceInfo, nullptr, &state.device);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vkCreateDevice", result) << std::endl;
        shutdownVulkan();
        return false;
    }

    vkGetDeviceQueue(state.device, state.graphicsQueueFamily, 0, &state.graphicsQueue);
    vkGetDeviceQueue(state.device, state.presentQueueFamily, 0, &state.presentQueue);

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.flags = memoryBudgetSupported ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0;
    allocatorInfo.physicalDevice = state.physicalDevice;
    allocatorInfo.device = state.device;
    allocatorInfo.instance = state.instance;
    allocatorInfo.vulkanApiVersion = appInfo.apiVersion;
    result = vmaCreateAllocator(&allocatorInfo, &state.allocator);
    if (result != VK_SUCCESS) {
        std::cerr << vkResultMessage("vmaCreateAllocator", result) << std::endl;
        shutdownVulkan();
        return false;
    }

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
        if (state.allocator) {
            vmaDestroyAllocator(state.allocator);
            state.allocator = VK_NULL_HANDLE;
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
    trimVulkanDynamicPool(state, frame.roundedInstances, 1);
    trimVulkanDynamicPool(state, frame.textVertices, 1);
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
    auto snapPx = [](float value) { return std::floor(value + 0.5f); };
    instance.rect[0] = snapPx(drawRect.x * dpiScale_);
    instance.rect[1] = snapPx(drawRect.y * dpiScale_);
    instance.rect[2] = std::max(1.0f, snapPx(drawRect.w * dpiScale_));
    instance.rect[3] = std::max(1.0f, snapPx(drawRect.h * dpiScale_));
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
                                    128 * 1024,
                                    "vmaMapMemory(rounded dynamic page)",
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
    auto snapPx = [](float value) { return std::floor(value + 0.5f); };
    instance.rect[0] = snapPx(drawRect.x * dpiScale_);
    instance.rect[1] = snapPx(drawRect.y * dpiScale_);
    instance.rect[2] = std::max(1.0f, snapPx(drawRect.w * dpiScale_));
    instance.rect[3] = std::max(1.0f, snapPx(drawRect.h * dpiScale_));
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
                                    128 * 1024,
                                    "vmaMapMemory(rounded dynamic page)",
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

    std::string resolvedFontName = resolveFontName(fontName, weight);
    FontData* fontPtr = getFontForSize(resolvedFontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) {
        return;
    }
    FontData& font = *fontPtr;

    int snappedSize = std::max(8, (int)std::round(font.fontSize));
    std::string sizedTextureKey = resolvedFontName + "@" + std::to_string(snappedSize);
    std::string textureKey = sizedTextureKey;
    auto baseFontIt = fonts_.find(resolvedFontName);
    if (baseFontIt != fonts_.end() && fontPtr == &baseFontIt->second) {
        textureKey = resolvedFontName;
    }

    auto& state = *vulkan_;
    if (!ensureVulkanFontTexture(state, textureKey, font)) {
        return;
    }
    auto textureIt = state.fontTextures.find(textureKey);
    if (textureIt == state.fontTextures.end()) {
        return;
    }

    float logicalFontHeight = font.fontSize / std::max(1.0f, dpiScale_);
    float fontScale;
    if (std::abs(fontSize - logicalFontHeight) < 1.01f) {
        fontScale = 1.0f / std::max(1.0f, dpiScale_);
    } else {
        fontScale = fontSize / font.fontSize;
    }
    auto& vertices = textVertexScratch_;
    vertices.clear();
    vertices.reserve(text.size() * 48);
    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };

    float cursorX = snap(pos.x + translation_.x);
    float baselineY = snap(pos.y + font.ascent * fontScale + translation_.y);
    float boldOffset = (weight == FontWeight::Bold && resolvedFontName == fontName)
        ? std::max(0.35f, fontSize * 0.018f)
        : 0.0f;

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
                                    512 * 1024,
                                    "vmaMapMemory(text dynamic page)",
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

void Renderer::drawVulkanImage(const std::string& key,
                               ImageData& image,
                               const Rect& rect,
                               const Color& tint,
                               float opacity) {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_ || !vulkan_->frameActive || !vulkan_->imagePipeline ||
        !image.loaded || opacity <= 0.0f || tint.a <= 0.0f ||
        rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }

    auto& state = *vulkan_;
    if (!ensureVulkanImageTexture(state, key, image)) {
        return;
    }
    auto textureIt = state.imageTextures.find(key);
    if (textureIt == state.imageTextures.end()) {
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

    auto snapPx = [](float value) { return std::floor(value + 0.5f); };
    float x = snapPx(drawRect.x * dpiScale_);
    float y = snapPx(drawRect.y * dpiScale_);
    float w = std::max(1.0f, snapPx(drawRect.w * dpiScale_));
    float h = std::max(1.0f, snapPx(drawRect.h * dpiScale_));
    float a = tint.a * opacity;
    float data[] = {
        x,     y,     0.0f, 0.0f, tint.r, tint.g, tint.b, a,
        x + w, y,     1.0f, 0.0f, tint.r, tint.g, tint.b, a,
        x + w, y + h, 1.0f, 1.0f, tint.r, tint.g, tint.b, a,
        x,     y,     0.0f, 0.0f, tint.r, tint.g, tint.b, a,
        x + w, y + h, 1.0f, 1.0f, tint.r, tint.g, tint.b, a,
        x,     y + h, 0.0f, 1.0f, tint.r, tint.g, tint.b, a,
    };

    flushVulkanBatches(state, scissorStack_, dpiScale_);
    auto& frame = state.frames[state.currentFrame % state.frames.size()];
    VulkanDynamicAllocation allocation;
    if (!allocateVulkanDynamicBytes(state,
                                    frame.textVertices,
                                    sizeof(data),
                                    alignof(VulkanTextVertex),
                                    512 * 1024,
                                    "vmaMapMemory(image dynamic page)",
                                    allocation)) {
        return;
    }
    std::memcpy(allocation.mapped, data, sizeof(data));

    VkDeviceSize offset = static_cast<VkDeviceSize>(allocation.offset);
    VulkanTextPush push = {};
    push.framebufferSize[0] = static_cast<float>(state.swapchainExtent.width);
    push.framebufferSize[1] = static_cast<float>(state.swapchainExtent.height);

    setVulkanScissor(state, scissorStack_, dpiScale_);
    vkCmdBindPipeline(frame.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      state.imagePipeline);
    VkDescriptorSet descriptorSet = textureIt->second.descriptorSet;
    vkCmdBindDescriptorSets(frame.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            state.textPipelineLayout,
                            0,
                            1,
                            &descriptorSet,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &frame.textVertices.pages[allocation.pageIndex].buffer, &offset);
    vkCmdPushConstants(frame.commandBuffer,
                       state.textPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(push),
                       &push);
    vkCmdDraw(frame.commandBuffer, 6, 1, 0, 0);
#else
    (void)key;
    (void)image;
    (void)rect;
    (void)tint;
    (void)opacity;
#endif
}

bool Renderer::init(void* windowHandle) {
    window_ = windowHandle;

    if (activeBackend_ == RenderBackendType::Vulkan) {
        return initVulkan(windowHandle);
    }

    if (activeBackend_ != RenderBackendType::Compatibility) {
        std::cerr << "FluxUI: " << activeBackendName()
                  << " draw path is not implemented yet. Using compatibility renderer." << std::endl;
        activeBackend_ = RenderBackendType::Compatibility;
    }
    
    // Calculate DPI scale (Win32 specific fallback for compatibility)
    int w = 800, h = 600;
    Platform::getWindowSize(windowHandle, w, h);
    
    // In a pure Win32 app without SDL, we'd use GetDpiForWindow
    dpiScale_ = 1.0f; 
    windowWidth_ = w;
    windowHeight_ = h;

    // OpenGL/Compatibility path removal
    // (If we ever need OpenGL back, we'd need WGL/GLX initialization here)
    std::cerr << "FluxUI: Compatibility (OpenGL) backend requires SDL2 (currently removed) or native WGL/GLX implementation." << std::endl;
    return false;
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
        beginVulkanFrame(w, h);
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
    glClearColor(0.06f, 0.06f, 0.09f, 1.0f);
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

    // Flush any remaining batched rects before presenting
    flushRectBatch();

    // In Vulkan, endFrame handles presentation.
    // In Win32/GDI it would be SwapBuffers((HDC)window_);
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

void Renderer::warmFontCache(const std::vector<float>& sizes, const std::string& name) {
    std::string resolvedNames[] = {
        name,
        resolveFontName(name, FontWeight::Bold)
    };

    for (float size : sizes) {
        if (size <= 0.0f) continue;

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

bool Renderer::ensureImageTexture(const std::string& key, ImageData& image) {
    if (!image.loaded) return false;
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
    image.pixels.clear();
    image.pixels.shrink_to_fit();
    return image.textureId != 0;
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

bool Renderer::buildFontAtlas(FontData& font, const unsigned char* data, int dataSize, float size) {
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

    int currentX = 1;
    int currentY = 1;
    int rowHeight = 0;
    bool packedOk = true;

    for (int i = firstGlyph; i < glyphLimit; ++i) {
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
                for (int j = firstGlyph; j <= i; ++j) {
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

                    font.glyphs[j].x0 = (float)currentX / atlasSize;
                    font.glyphs[j].y0 = (float)currentY / atlasSize;
                    font.glyphs[j].x1 = (float)(currentX + reW) / atlasSize;
                    font.glyphs[j].y1 = (float)(currentY + reH) / atlasSize;
                    font.glyphs[j].xoff = (float)reSlot->bitmap_left;
                    font.glyphs[j].yoff = (float)-reSlot->bitmap_top;
                    font.glyphs[j].xadvance = (float)(reSlot->advance.x >> 6);
                    font.glyphs[j].width = (float)reW;
                    font.glyphs[j].height = (float)reH;

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

        font.glyphs[i].x0 = (float)currentX / atlasSize;
        font.glyphs[i].y0 = (float)currentY / atlasSize;
        font.glyphs[i].x1 = (float)(currentX + w) / atlasSize;
        font.glyphs[i].y1 = (float)(currentY + h) / atlasSize;
        font.glyphs[i].xoff = (float)slot->bitmap_left;
        font.glyphs[i].yoff = (float)-slot->bitmap_top;
        font.glyphs[i].xadvance = (float)(slot->advance.x >> 6);
        font.glyphs[i].width = (float)w;
        font.glyphs[i].height = (float)h;

        currentX += w + 1;
    }

    if (!packedOk) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
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

    if (activeBackend_ == RenderBackendType::Vulkan) {
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
        font.atlasPixels.clear();
    }

    font.loaded = true;
    return true;
}

FontData* Renderer::getFontForSize(const std::string& fontName, float fontSize) {
    auto baseIt = fonts_.find(fontName);
    if (baseIt == fonts_.end() || !baseIt->second.loaded) return nullptr;

    int snappedSize = std::max(8, (int)std::round(fontSize * std::max(1.0f, dpiScale_)));
    int baseSize = std::max(8, (int)std::round(baseIt->second.fontSize));
    if (snappedSize == baseSize || baseIt->second.sourceData.empty()) {
        return &baseIt->second;
    }

    std::string sizedName = fontName + "@" + std::to_string(snappedSize);
    auto sizedIt = fonts_.find(sizedName);
    if (sizedIt != fonts_.end() && sizedIt->second.loaded) {
        return &sizedIt->second;
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

std::string Renderer::resolveFontName(const std::string& fontName, FontWeight weight) const {
    std::string baseName = fontName.empty() ? "default" : fontName;
    auto baseIt = fonts_.find(baseName);
    if ((baseIt == fonts_.end() || !baseIt->second.loaded) && baseName != "default") {
        baseName = "default";
    }

    if (weight != FontWeight::Bold) {
        return baseName;
    }

    std::string boldName = baseName + "-bold";
    auto boldIt = fonts_.find(boldName);
    if (boldIt != fonts_.end() && boldIt->second.loaded) {
        return boldName;
    }
    return baseName;
}

const FontData* Renderer::findFontForMeasure(const std::string& fontName, float fontSize) const {
    int snappedSize = std::max(8, (int)std::round(fontSize * std::max(1.0f, dpiScale_)));
    auto baseIt = fonts_.find(fontName);
    std::string sizedName = fontName + "@" + std::to_string(snappedSize);
    auto sizedIt = fonts_.find(sizedName);
    if (sizedIt != fonts_.end() && sizedIt->second.loaded) return &sizedIt->second;

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
    inst.rect[0] = snap(rect.x + translation_.x);
    inst.rect[1] = snap(rect.y + translation_.y);
    inst.rect[2] = snap(rect.w);
    inst.rect[3] = snap(rect.h);
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
    inst.rect[0] = snap(rect.x + translation_.x);
    inst.rect[1] = snap(rect.y + translation_.y);
    inst.rect[2] = snap(rect.w);
    inst.rect[3] = snap(rect.h);
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
    inst.rect[0] = snap(rect.x + translation_.x);
    inst.rect[1] = snap(rect.y + translation_.y);
    inst.rect[2] = snap(rect.w);
    inst.rect[3] = snap(rect.h);
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

// ============================================================
//  Drawing: Text
// ============================================================

void Renderer::drawText(const std::string& text, const Vec2& pos, const Color& color,
                         float fontSize, FontWeight weight, const std::string& fontName) {
    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanText(text, pos, color, fontSize, weight, fontName);
        return;
    }

    std::string resolvedFontName = resolveFontName(fontName, weight);
    FontData* fontPtr = getFontForSize(resolvedFontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) return;
    auto& font = *fontPtr;

    float logicalFontHeight = font.fontSize / std::max(1.0f, dpiScale_);
    float scale;
    if (std::abs(fontSize - logicalFontHeight) < 1.01f) {
        scale = 1.0f / std::max(1.0f, dpiScale_);
    } else {
        scale = fontSize / font.fontSize;
    }

    // Build vertex data for all glyphs into reusable scratch memory.
    auto& vertices = textVertexScratch_;
    vertices.clear();
    vertices.reserve(text.size() * 48);
    auto snap = [this](float v) { return std::floor(v * dpiScale_ + 0.5f) / dpiScale_; };

    // Flush rect batch before drawing text (different shader)
    flushRectBatch();

    useShader(textShader_);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0,0) : scalePivotStack_.back();
    setProjection(textUniforms_.projection, windowWidth_, windowHeight_, scale_, pivot);

    float cursorX = snap(pos.x + translation_.x);
    float baselineY = snap(pos.y + font.ascent * scale + translation_.y);
    float boldOffset = (weight == FontWeight::Bold && resolvedFontName == fontName)
        ? std::max(0.35f, fontSize * 0.018f)
        : 0.0f;

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

void Renderer::drawImage(const std::string& nameOrPath, const Rect& rect,
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

    if (activeBackend_ == RenderBackendType::Vulkan) {
        drawVulkanImage(nameOrPath, it->second, rect, tint, opacity);
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

    float a = tint.a * opacity;
    float vertices[] = {
        drawRect.x,              drawRect.y,              0.0f, 0.0f, tint.r, tint.g, tint.b, a,
        drawRect.x + drawRect.w, drawRect.y,              1.0f, 0.0f, tint.r, tint.g, tint.b, a,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, 1.0f, 1.0f, tint.r, tint.g, tint.b, a,
        drawRect.x,              drawRect.y,              0.0f, 0.0f, tint.r, tint.g, tint.b, a,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, 1.0f, 1.0f, tint.r, tint.g, tint.b, a,
        drawRect.x,              drawRect.y + drawRect.h, 0.0f, 1.0f, tint.r, tint.g, tint.b, a,
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
                               const std::string& fontName) {
    std::string resolvedFontName = resolveFontName(fontName, weight);
    FontData* fontForRect = getFontForSize(resolvedFontName, fontSize);

    float x = rect.x;
    if (align != TextAlign::Left) {
        Vec2 textSize = measureText(text, fontSize, resolvedFontName);
        if (align == TextAlign::Center) x = rect.x + (rect.w - textSize.x) / 2;
        else if (align == TextAlign::Right) x = rect.x + rect.w - textSize.x;
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
    float logicalFontHeight = font.fontSize / std::max(1.0f, dpiScale_);
    float scale;
    if (std::abs(fontSize - logicalFontHeight) < 1.01f) {
        scale = 1.0f / std::max(1.0f, dpiScale_);
    } else {
        scale = fontSize / font.fontSize;
    }
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

} // namespace FluxUI
