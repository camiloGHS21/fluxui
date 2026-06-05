// FluxUI Renderer - GPU-accelerated rendering
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "fluxui/property_trees.h"

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
#if FLUXUI_HAS_VULKAN_SDK
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#endif
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

static std::string substituteLigatures(const std::string& text, const FontData& font);

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
    float number = parseLocaleIndependentFloat(s.c_str(), &end);
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
        float v = parseLocaleIndependentFloat(p, &end);
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
            std::string sub = value.substr(start + 1, end - start - 1);
            std::vector<float> nums;
            std::vector<bool> isPercent;
            
            const char* p = sub.c_str();
            while (*p) {
                while (*p && (std::isspace((unsigned char)*p) || *p == ',' || *p == '/')) {
                    ++p;
                }
                if (!*p) break;
                
                char* parseEnd = nullptr;
                float val = parseLocaleIndependentFloat(p, &parseEnd);
                if (parseEnd == p) {
                    ++p;
                    continue;
                }
                p = parseEnd;
                
                bool percent = false;
                if (*p == '%') {
                    percent = true;
                    ++p;
                }
                nums.push_back(val);
                isPercent.push_back(percent);
            }
            
            if (nums.size() >= 3) {
                float r = isPercent[0] ? (nums[0] / 100.0f) * 255.0f : nums[0];
                float g = isPercent[1] ? (nums[1] / 100.0f) * 255.0f : nums[1];
                float b = isPercent[2] ? (nums[2] / 100.0f) * 255.0f : nums[2];
                float a = 1.0f;
                if (nums.size() >= 4) {
                    a = isPercent[3] ? nums[3] / 100.0f : nums[3];
                }
                return Color(r / 255.0f, g / 255.0f, b / 255.0f, a);
            }
        }
    }

    struct Named { const char* name; Color color; };
    static const Named named[] = {
        {"aliceblue", Color::fromHex("#F0F8FF")},
        {"antiquewhite", Color::fromHex("#FAEBD7")},
        {"aqua", Color::fromHex("#00FFFF")},
        {"aquamarine", Color::fromHex("#7FFFD4")},
        {"azure", Color::fromHex("#F0FFFF")},
        {"beige", Color::fromHex("#F5F5DC")},
        {"bisque", Color::fromHex("#FFE4C4")},
        {"black", Color::fromHex("#000000")},
        {"blanchedalmond", Color::fromHex("#FFEBCD")},
        {"blue", Color::fromHex("#0000FF")},
        {"blueviolet", Color::fromHex("#8A2BE2")},
        {"brown", Color::fromHex("#A52A2A")},
        {"burlywood", Color::fromHex("#DEB887")},
        {"cadetblue", Color::fromHex("#5F9EA0")},
        {"chartreuse", Color::fromHex("#7FFF00")},
        {"chocolate", Color::fromHex("#D2691E")},
        {"coral", Color::fromHex("#FF7F50")},
        {"cornflowerblue", Color::fromHex("#6495ED")},
        {"cornsilk", Color::fromHex("#FFF8DC")},
        {"crimson", Color::fromHex("#DC143C")},
        {"cyan", Color::fromHex("#00FFFF")},
        {"darkblue", Color::fromHex("#00008B")},
        {"darkcyan", Color::fromHex("#008B8B")},
        {"darkgoldenrod", Color::fromHex("#B8860B")},
        {"darkgray", Color::fromHex("#A9A9A9")},
        {"darkgreen", Color::fromHex("#006400")},
        {"darkgrey", Color::fromHex("#A9A9A9")},
        {"darkkhaki", Color::fromHex("#BDB76B")},
        {"darkmagenta", Color::fromHex("#8B008B")},
        {"darkolivegreen", Color::fromHex("#556B2F")},
        {"darkorange", Color::fromHex("#FF8C00")},
        {"darkorchid", Color::fromHex("#9932CC")},
        {"darkred", Color::fromHex("#8B0000")},
        {"darksalmon", Color::fromHex("#E9967A")},
        {"darkseagreen", Color::fromHex("#8FBC8F")},
        {"darkslateblue", Color::fromHex("#483D8B")},
        {"darkslategray", Color::fromHex("#2F4F4F")},
        {"darkslategrey", Color::fromHex("#2F4F4F")},
        {"darkturquoise", Color::fromHex("#00CED1")},
        {"darkviolet", Color::fromHex("#9400D3")},
        {"deeppink", Color::fromHex("#FF1493")},
        {"deepskyblue", Color::fromHex("#00BFFF")},
        {"dimgray", Color::fromHex("#696969")},
        {"dimgrey", Color::fromHex("#696969")},
        {"dodgerblue", Color::fromHex("#1E90FF")},
        {"firebrick", Color::fromHex("#B22222")},
        {"floralwhite", Color::fromHex("#FFFAF0")},
        {"forestgreen", Color::fromHex("#228B22")},
        {"fuchsia", Color::fromHex("#FF00FF")},
        {"gainsboro", Color::fromHex("#DCDCDC")},
        {"ghostwhite", Color::fromHex("#F8F8FF")},
        {"gold", Color::fromHex("#FFD700")},
        {"goldenrod", Color::fromHex("#DAA520")},
        {"gray", Color::fromHex("#808080")},
        {"green", Color::fromHex("#008000")},
        {"greenyellow", Color::fromHex("#ADFF2F")},
        {"grey", Color::fromHex("#808080")},
        {"honeydew", Color::fromHex("#F0FFF0")},
        {"hotpink", Color::fromHex("#FF69B4")},
        {"indianred", Color::fromHex("#CD5C5C")},
        {"indigo", Color::fromHex("#4B0082")},
        {"ivory", Color::fromHex("#FFFFF0")},
        {"khaki", Color::fromHex("#F0E68C")},
        {"lavender", Color::fromHex("#E6E6FA")},
        {"lavenderblush", Color::fromHex("#FFF0F5")},
        {"lawngreen", Color::fromHex("#7CFC00")},
        {"lemonchiffon", Color::fromHex("#FFFACD")},
        {"lightblue", Color::fromHex("#ADD8E6")},
        {"lightcoral", Color::fromHex("#F08080")},
        {"lightcyan", Color::fromHex("#E0FFFF")},
        {"lightgoldenrodyellow", Color::fromHex("#FAFAD2")},
        {"lightgray", Color::fromHex("#D3D3D3")},
        {"lightgreen", Color::fromHex("#90EE90")},
        {"lightgrey", Color::fromHex("#D3D3D3")},
        {"lightpink", Color::fromHex("#FFB6C1")},
        {"lightsalmon", Color::fromHex("#FFA07A")},
        {"lightseagreen", Color::fromHex("#20B2AA")},
        {"lightskyblue", Color::fromHex("#87CEFA")},
        {"lightslategray", Color::fromHex("#778899")},
        {"lightslategrey", Color::fromHex("#778899")},
        {"lightsteelblue", Color::fromHex("#B0C4DE")},
        {"lightyellow", Color::fromHex("#FFFFE0")},
        {"lime", Color::fromHex("#00FF00")},
        {"limegreen", Color::fromHex("#32CD32")},
        {"linen", Color::fromHex("#FAF0E6")},
        {"magenta", Color::fromHex("#FF00FF")},
        {"maroon", Color::fromHex("#800000")},
        {"mediumaquamarine", Color::fromHex("#66CDAA")},
        {"mediumblue", Color::fromHex("#0000CD")},
        {"mediumorchid", Color::fromHex("#BA55D3")},
        {"mediumpurple", Color::fromHex("#9370DB")},
        {"mediumseagreen", Color::fromHex("#3CB371")},
        {"mediumslateblue", Color::fromHex("#7B68EE")},
        {"mediumspringgreen", Color::fromHex("#00FA9A")},
        {"mediumturquoise", Color::fromHex("#48D1CC")},
        {"mediumvioletred", Color::fromHex("#C71585")},
        {"midnightblue", Color::fromHex("#191970")},
        {"mintcream", Color::fromHex("#F5FFFA")},
        {"mistyrose", Color::fromHex("#FFE4E1")},
        {"moccasin", Color::fromHex("#FFE4B5")},
        {"navajowhite", Color::fromHex("#FFDEAD")},
        {"navy", Color::fromHex("#000080")},
        {"oldlace", Color::fromHex("#FDF5E6")},
        {"olive", Color::fromHex("#808000")},
        {"olivedrab", Color::fromHex("#6B8E23")},
        {"orange", Color::fromHex("#FFA500")},
        {"orangered", Color::fromHex("#FF4500")},
        {"orchid", Color::fromHex("#DA70D6")},
        {"palegoldenrod", Color::fromHex("#EEE8AA")},
        {"palegreen", Color::fromHex("#98FB98")},
        {"paleturquoise", Color::fromHex("#AFEEEE")},
        {"palevioletred", Color::fromHex("#DB7093")},
        {"papayawhip", Color::fromHex("#FFEFD5")},
        {"peachpuff", Color::fromHex("#FFDAB9")},
        {"peru", Color::fromHex("#CD853F")},
        {"pink", Color::fromHex("#FFC0CB")},
        {"plum", Color::fromHex("#DDA0DD")},
        {"powderblue", Color::fromHex("#B0E0E6")},
        {"purple", Color::fromHex("#800080")},
        {"rebeccapurple", Color::fromHex("#663399")},
        {"red", Color::fromHex("#FF0000")},
        {"rosybrown", Color::fromHex("#BC8F8F")},
        {"royalblue", Color::fromHex("#4169E1")},
        {"saddlebrown", Color::fromHex("#8B4513")},
        {"salmon", Color::fromHex("#FA8072")},
        {"sandybrown", Color::fromHex("#F4A460")},
        {"seagreen", Color::fromHex("#2E8B57")},
        {"seashell", Color::fromHex("#FFF5EE")},
        {"sienna", Color::fromHex("#A0522D")},
        {"silver", Color::fromHex("#C0C0C0")},
        {"skyblue", Color::fromHex("#87CEEB")},
        {"slateblue", Color::fromHex("#6A5ACD")},
        {"slategray", Color::fromHex("#708090")},
        {"slategrey", Color::fromHex("#708090")},
        {"snow", Color::fromHex("#FFFAFA")},
        {"springgreen", Color::fromHex("#00FF7F")},
        {"steelblue", Color::fromHex("#4682B4")},
        {"tan", Color::fromHex("#D2B48C")},
        {"teal", Color::fromHex("#008080")},
        {"thistle", Color::fromHex("#D8BFD8")},
        {"tomato", Color::fromHex("#FF6347")},
        {"turquoise", Color::fromHex("#40E0D0")},
        {"violet", Color::fromHex("#EE82EE")},
        {"wheat", Color::fromHex("#F5DEB3")},
        {"white", Color::fromHex("#FFFFFF")},
        {"whitesmoke", Color::fromHex("#F5F5F5")},
        {"yellow", Color::fromHex("#FFFF00")},
        {"yellowgreen", Color::fromHex("#9ACD32")}
    };

    auto it = std::lower_bound(std::begin(named), std::end(named), value, [](const Named& item, const std::string& val) {
        return std::strcmp(item.name, val.c_str()) < 0;
    });
    if (it != std::end(named) && value == it->name) {
        return it->color;
    }
    return fallback;
}

struct SvgAffine {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, e = 0.0f, f = 0.0f;

    static SvgAffine identity() { return {}; }
    static SvgAffine translate(float tx, float ty) { return {1.0f, 0.0f, 0.0f, 1.0f, tx, ty}; }
    static SvgAffine scale(float sx, float sy) { return {sx, 0.0f, 0.0f, sy, 0.0f, 0.0f}; }
    static SvgAffine rotate(float degrees) {
        float radians = degrees * 3.14159265358979323846f / 180.0f;
        float cs = std::cos(radians);
        float sn = std::sin(radians);
        return {cs, sn, -sn, cs, 0.0f, 0.0f};
    }
    static SvgAffine skewX(float degrees) {
        float radians = degrees * 3.14159265358979323846f / 180.0f;
        return {1.0f, 0.0f, std::tan(radians), 1.0f, 0.0f, 0.0f};
    }
    static SvgAffine skewY(float degrees) {
        float radians = degrees * 3.14159265358979323846f / 180.0f;
        return {1.0f, std::tan(radians), 0.0f, 1.0f, 0.0f, 0.0f};
    }

    Vec2 apply(float x, float y) const {
        return {a * x + c * y + e, b * x + d * y + f};
    }

    SvgAffine multiply(const SvgAffine& other) const {
        return {
            a * other.a + c * other.b,
            b * other.a + d * other.b,
            a * other.c + c * other.d,
            b * other.c + d * other.d,
            a * other.e + c * other.f + e,
            b * other.e + d * other.f + f
        };
    }

    bool axisAligned() const {
        return std::abs(b) < 0.0001f && std::abs(c) < 0.0001f;
    }
};

struct SvgCanvas {
    std::vector<unsigned char>* pixels = nullptr;
    int width = 0;
    int height = 0;
    float viewX = 0.0f;
    float viewY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    SvgAffine transform;
};

Vec2 svgMapPoint(const SvgCanvas& canvas, float x, float y) {
    Vec2 transformed = canvas.transform.apply(x, y);
    return {canvas.offsetX + (transformed.x - canvas.viewX) * canvas.scaleX,
            canvas.offsetY + (transformed.y - canvas.viewY) * canvas.scaleY};
}

struct SvgAspectRatio {
    bool none = false;
    bool slice = false;
    float alignX = 0.5f;
    float alignY = 0.5f;
};

struct SvgPaintState {
    std::string fill = "black";
    std::string stroke = "none";
    std::string opacity = "1";
    std::string fillOpacity = "1";
    std::string strokeOpacity = "1";
    std::string strokeWidth = "1";
    std::string color = "black";
    std::string fillRule = "nonzero"; // SVG default is nonzero, not evenodd
};

SvgAspectRatio parseSvgPreserveAspectRatio(const std::string& rawValue) {
    SvgAspectRatio result;
    std::string value = lowerSvgString(trimSvgString(rawValue));
    if (value.empty()) return result;

    std::istringstream stream(value);
    std::string token;
    while (stream >> token) {
        if (token == "defer") continue;
        if (token == "none") {
            result.none = true;
            result.alignX = 0.0f;
            result.alignY = 0.0f;
            continue;
        }
        if (token == "slice") {
            result.slice = true;
            continue;
        }
        if (token == "meet") {
            result.slice = false;
            continue;
        }
        if (token.find("xmin") != std::string::npos) result.alignX = 0.0f;
        else if (token.find("xmax") != std::string::npos) result.alignX = 1.0f;
        else if (token.find("xmid") != std::string::npos) result.alignX = 0.5f;

        if (token.find("ymin") != std::string::npos) result.alignY = 0.0f;
        else if (token.find("ymax") != std::string::npos) result.alignY = 1.0f;
        else if (token.find("ymid") != std::string::npos) result.alignY = 0.5f;
    }
    return result;
}

SvgAffine parseSvgTransformList(const std::string& rawValue) {
    SvgAffine combined = SvgAffine::identity();
    std::string value = trimSvgString(rawValue);
    size_t pos = 0;
    while (pos < value.size()) {
        while (pos < value.size() && (std::isspace((unsigned char)value[pos]) || value[pos] == ',')) ++pos;
        size_t nameStart = pos;
        while (pos < value.size() && (std::isalpha((unsigned char)value[pos]) || value[pos] == '-')) ++pos;
        if (nameStart == pos) break;
        std::string name = lowerSvgString(value.substr(nameStart, pos - nameStart));
        while (pos < value.size() && std::isspace((unsigned char)value[pos])) ++pos;
        if (pos >= value.size() || value[pos] != '(') break;
        size_t argsStart = ++pos;
        int depth = 1;
        while (pos < value.size() && depth > 0) {
            if (value[pos] == '(') ++depth;
            else if (value[pos] == ')') --depth;
            if (depth > 0) ++pos;
        }
        if (depth != 0) break;
        std::vector<float> args = parseSvgNumberList(value.substr(argsStart, pos - argsStart));
        if (pos < value.size() && value[pos] == ')') ++pos;

        SvgAffine next = SvgAffine::identity();
        if (name == "matrix" && args.size() >= 6) {
            next = {args[0], args[1], args[2], args[3], args[4], args[5]};
        } else if (name == "translate" && !args.empty()) {
            next = SvgAffine::translate(args[0], args.size() >= 2 ? args[1] : 0.0f);
        } else if (name == "scale" && !args.empty()) {
            next = SvgAffine::scale(args[0], args.size() >= 2 ? args[1] : args[0]);
        } else if (name == "rotate" && !args.empty()) {
            next = SvgAffine::rotate(args[0]);
            if (args.size() >= 3) {
                next = SvgAffine::translate(args[1], args[2])
                    .multiply(next)
                    .multiply(SvgAffine::translate(-args[1], -args[2]));
            }
        } else if (name == "skewx" && !args.empty()) {
            next = SvgAffine::skewX(args[0]);
        } else if (name == "skewy" && !args.empty()) {
            next = SvgAffine::skewY(args[0]);
        }
        combined = combined.multiply(next);
    }
    return combined;
}

SvgPaintState mergeSvgPaintState(const SvgPaintState& parent, const SvgAttrs& attrs) {
    SvgPaintState state = parent;
    auto inheritAttr = [&](std::string& field, const char* name) {
        auto it = attrs.find(name);
        if (it != attrs.end()) field = it->second;
    };
    inheritAttr(state.fill, "fill");
    inheritAttr(state.stroke, "stroke");
    inheritAttr(state.opacity, "opacity");
    inheritAttr(state.fillOpacity, "fill-opacity");
    inheritAttr(state.strokeOpacity, "stroke-opacity");
    inheritAttr(state.strokeWidth, "stroke-width");
    inheritAttr(state.color, "color");
    inheritAttr(state.fillRule, "fill-rule");
    return state;
}

std::string svgTagName(const std::string& tag, size_t offset = 1) {
    size_t i = offset;
    while (i < tag.size() && (std::isspace((unsigned char)tag[i]) || tag[i] == '/')) ++i;
    size_t start = i;
    while (i < tag.size() && (std::isalnum((unsigned char)tag[i]) ||
           tag[i] == '-' || tag[i] == '_' || tag[i] == ':')) ++i;
    return lowerSvgString(tag.substr(start, i - start));
}

bool isSvgRenderableTag(const std::string& name) {
    return name == "rect" || name == "circle" || name == "ellipse" ||
           name == "line" || name == "polyline" || name == "polygon" ||
           name == "path";
}

bool isSvgDefinitionContainer(const std::string& name) {
    return name == "defs" || name == "symbol" || name == "clippath" ||
           name == "mask" || name == "pattern" || name == "marker" ||
           name == "lineargradient" || name == "radialgradient" ||
           name == "filter";
}

bool svgTagIsSelfClosing(const std::string& tag) {
    for (size_t i = tag.size(); i > 0; --i) {
        char c = tag[i - 1];
        if (std::isspace((unsigned char)c) || c == '>') continue;
        return c == '/';
    }
    return false;
}

void appendCubic(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3);
void appendQuad(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2);
void appendArc(std::vector<Vec2>& out, Vec2 p0, float rx, float ry, float angle, bool largeArcFlag, bool sweepFlag, Vec2 p1, SvgCanvas& canvas);

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
    int startY = std::max(0, (int)std::floor(minY));
    int endY = std::min(canvas.height - 1, (int)std::ceil(maxY));
    int startX = std::max(0, (int)std::floor(minX));
    int endX = std::min(canvas.width - 1, (int)std::ceil(maxX));
    if (startY > endY || startX > endX) return;

    struct Edge {
        float x1, y1, x2, y2;
        int dir; // +1 if y1<y2 (upward), -1 if y1>y2 (downward) — for nonzero winding
    };
    std::vector<Edge> edges;
    edges.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        Vec2 p1 = points[i];
        Vec2 p2 = points[(i + 1) % points.size()];
        if (std::abs(p1.y - p2.y) > 0.0001f) {
            int dir = (p1.y < p2.y) ? 1 : -1;
            edges.push_back({p1.x, p1.y, p2.x, p2.y, dir});
        }
    }

    std::vector<float> intersections1;
    std::vector<float> intersections2;
    intersections1.reserve(16);
    intersections2.reserve(16);

    for (int y = startY; y <= endY; ++y) {
        float y_sub1 = y + 0.25f;
        float y_sub2 = y + 0.75f;
        intersections1.clear();
        intersections2.clear();

        for (const auto& edge : edges) {
            if ((edge.y1 <= y_sub1 && edge.y2 > y_sub1) || (edge.y2 <= y_sub1 && edge.y1 > y_sub1)) {
                float t = (y_sub1 - edge.y1) / (edge.y2 - edge.y1);
                intersections1.push_back(edge.x1 + t * (edge.x2 - edge.x1));
            }
            if ((edge.y1 <= y_sub2 && edge.y2 > y_sub2) || (edge.y2 <= y_sub2 && edge.y1 > y_sub2)) {
                float t = (y_sub2 - edge.y1) / (edge.y2 - edge.y1);
                intersections2.push_back(edge.x1 + t * (edge.x2 - edge.x1));
            }
        }

        std::sort(intersections1.begin(), intersections1.end());
        std::sort(intersections2.begin(), intersections2.end());

        auto isInside = [](float x_sub, const std::vector<float>& intersections) -> bool {
            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                if (x_sub >= intersections[i] && x_sub <= intersections[i + 1]) {
                    return true;
                }
            }
            return false;
        };

        for (int x = startX; x <= endX; ++x) {
            float x_sub1 = x + 0.25f;
            float x_sub2 = x + 0.75f;
            int insideCount = 0;

            if (isInside(x_sub1, intersections1)) insideCount++;
            if (isInside(x_sub2, intersections1)) insideCount++;
            if (isInside(x_sub1, intersections2)) insideCount++;
            if (isInside(x_sub2, intersections2)) insideCount++;

            if (insideCount > 0) {
                svgBlendPixel(canvas, x, y, color, insideCount / 4.0f);
            }
        }
    }
}

// Multi-path fill with fill-rule support (nonzero winding / evenodd)
// This is the Blink-parity critical function: it collects edges from ALL subpaths
// so that holes (inner subpaths with opposite winding) are correctly subtracted.
void fillMultiPolygon(SvgCanvas& canvas, const std::vector<std::vector<Vec2>>& subpaths,
                     Color color, bool useEvenOdd) {
    if (color.a <= 0.0f) return;

    struct Edge {
        float x1, y1, x2, y2;
        int dir; // +1 going down, -1 going up (for nonzero winding)
    };

    // Collect edges from ALL subpaths and compute global bounding box
    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    std::vector<Edge> edges;
    size_t totalPts = 0;
    for (const auto& path : subpaths) totalPts += path.size();
    edges.reserve(totalPts);

    for (const auto& path : subpaths) {
        if (path.size() < 3) continue;
        for (size_t i = 0; i < path.size(); ++i) {
            const Vec2& p1 = path[i];
            const Vec2& p2 = path[(i + 1) % path.size()];
            minX = std::min(minX, std::min(p1.x, p2.x));
            maxX = std::max(maxX, std::max(p1.x, p2.x));
            minY = std::min(minY, std::min(p1.y, p2.y));
            maxY = std::max(maxY, std::max(p1.y, p2.y));
            if (std::abs(p1.y - p2.y) > 0.0001f) {
                int dir = (p1.y < p2.y) ? 1 : -1;
                edges.push_back({p1.x, p1.y, p2.x, p2.y, dir});
            }
        }
    }

    if (edges.empty()) return;

    int startY = std::max(0, (int)std::floor(minY));
    int endY = std::min(canvas.height - 1, (int)std::ceil(maxY));
    int startX = std::max(0, (int)std::floor(minX));
    int endX = std::min(canvas.width - 1, (int)std::ceil(maxX));
    if (startY > endY || startX > endX) return;

    // For nonzero winding, store intersection x-values paired with direction
    struct Crossing {
        float x;
        int dir;
    };
    std::vector<Crossing> crossings1, crossings2;
    crossings1.reserve(32);
    crossings2.reserve(32);

    for (int y = startY; y <= endY; ++y) {
        float y_sub1 = y + 0.25f;
        float y_sub2 = y + 0.75f;
        crossings1.clear();
        crossings2.clear();

        for (const auto& edge : edges) {
            if ((edge.y1 <= y_sub1 && edge.y2 > y_sub1) || (edge.y2 <= y_sub1 && edge.y1 > y_sub1)) {
                float t = (y_sub1 - edge.y1) / (edge.y2 - edge.y1);
                crossings1.push_back({edge.x1 + t * (edge.x2 - edge.x1), edge.dir});
            }
            if ((edge.y1 <= y_sub2 && edge.y2 > y_sub2) || (edge.y2 <= y_sub2 && edge.y1 > y_sub2)) {
                float t = (y_sub2 - edge.y1) / (edge.y2 - edge.y1);
                crossings2.push_back({edge.x1 + t * (edge.x2 - edge.x1), edge.dir});
            }
        }

        // Sort crossings by x position
        auto cmpCrossing = [](const Crossing& a, const Crossing& b) { return a.x < b.x; };
        std::sort(crossings1.begin(), crossings1.end(), cmpCrossing);
        std::sort(crossings2.begin(), crossings2.end(), cmpCrossing);

        // Lambda to check if subpixel X is inside using the selected fill rule
        auto isInsideFn = [useEvenOdd](float x_sub, const std::vector<Crossing>& crossings) -> bool {
            if (useEvenOdd) {
                // Even-odd: count crossings to the left; odd = inside
                int count = 0;
                for (const auto& c : crossings) {
                    if (c.x > x_sub) break;
                    ++count;
                }
                return (count % 2) != 0;
            } else {
                // Nonzero winding: sum directions of crossings to the left; nonzero = inside
                int winding = 0;
                for (const auto& c : crossings) {
                    if (c.x > x_sub) break;
                    winding += c.dir;
                }
                return winding != 0;
            }
        };

        for (int x = startX; x <= endX; ++x) {
            float x_sub1 = x + 0.25f;
            float x_sub2 = x + 0.75f;
            int insideCount = 0;

            if (isInsideFn(x_sub1, crossings1)) insideCount++;
            if (isInsideFn(x_sub2, crossings1)) insideCount++;
            if (isInsideFn(x_sub1, crossings2)) insideCount++;
            if (isInsideFn(x_sub2, crossings2)) insideCount++;

            if (insideCount > 0) {
                svgBlendPixel(canvas, x, y, color, insideCount / 4.0f);
            }
        }
    }
}

void drawSvgRect(SvgCanvas& canvas, float x, float y, float w, float h,
                 float rx, float ry, Color fill, Color stroke, float strokeWidth) {
    if (w <= 0.0f || h <= 0.0f) return;
    
    // Clamp rx and ry according to SVG spec
    if (rx < 0.0f && ry < 0.0f) rx = ry = 0.0f;
    else if (rx >= 0.0f && ry < 0.0f) ry = rx;
    else if (ry >= 0.0f && rx < 0.0f) rx = ry;
    
    rx = std::min(rx, w / 2.0f);
    ry = std::min(ry, h / 2.0f);
    
    if (rx > 0.0f && ry > 0.0f) {
        // High-fidelity Bezier corner approximation (Chromium/Blink parity)
        std::vector<Vec2> points;
        const float K = 0.55228475f; // Cubic bezier kappa for elliptical arc approximation
        
        // 1. Top edge and top-right corner
        Vec2 tr_start = {x + rx, y};
        Vec2 tr_end = {x + w - rx, y};
        points.push_back(svgMapPoint(canvas, tr_start.x, tr_start.y));
        points.push_back(svgMapPoint(canvas, tr_end.x, tr_end.y));
        
        Vec2 cp1 = {x + w - rx * (1.0f - K), y};
        Vec2 cp2 = {x + w, y + ry * (1.0f - K)};
        Vec2 tr_curve_end = {x + w, y + ry};
        appendCubic(points,
                    svgMapPoint(canvas, tr_end.x, tr_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, tr_curve_end.x, tr_curve_end.y));
                    
        // 2. Right edge and bottom-right corner
        Vec2 br_end = {x + w, y + h - ry};
        points.push_back(svgMapPoint(canvas, br_end.x, br_end.y));
        
        cp1 = {x + w, y + h - ry * (1.0f - K)};
        cp2 = {x + w - rx * (1.0f - K), y + h};
        Vec2 br_curve_end = {x + w - rx, y + h};
        appendCubic(points,
                    svgMapPoint(canvas, br_end.x, br_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, br_curve_end.x, br_curve_end.y));
                    
        // 3. Bottom edge and bottom-left corner
        Vec2 bl_end = {x + rx, y + h};
        points.push_back(svgMapPoint(canvas, bl_end.x, bl_end.y));
        
        cp1 = {x + rx * (1.0f - K), y + h};
        cp2 = {x, y + h - ry * (1.0f - K)};
        Vec2 bl_curve_end = {x, y + h - ry};
        appendCubic(points,
                    svgMapPoint(canvas, bl_end.x, bl_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, bl_curve_end.x, bl_curve_end.y));
                    
        // 4. Left edge and top-left corner
        Vec2 tl_end = {x, y + ry};
        points.push_back(svgMapPoint(canvas, tl_end.x, tl_end.y));
        
        cp1 = {x, y + ry * (1.0f - K)};
        cp2 = {x + rx * (1.0f - K), y};
        Vec2 tl_curve_end = {x + rx, y};
        appendCubic(points,
                    svgMapPoint(canvas, tl_end.x, tl_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, tl_curve_end.x, tl_curve_end.y));
                    
        if (fill.a > 0.0f) fillPolygon(canvas, points, fill);
        if (stroke.a > 0.0f && strokeWidth > 0.0f) {
            strokePolyline(canvas, points, stroke, strokeWidth, true);
        }
        return;
    }
    
    if (!canvas.transform.axisAligned()) {
        std::vector<Vec2> points = {
            svgMapPoint(canvas, x, y),
            svgMapPoint(canvas, x + w, y),
            svgMapPoint(canvas, x + w, y + h),
            svgMapPoint(canvas, x, y + h)
        };
        if (fill.a > 0.0f) fillPolygon(canvas, points, fill);
        if (stroke.a > 0.0f && strokeWidth > 0.0f) {
            strokePolyline(canvas, points, stroke, strokeWidth, true);
        }
        return;
    }
    Vec2 p0 = svgMapPoint(canvas, x, y);
    Vec2 p1 = svgMapPoint(canvas, x + w, y + h);
    float left = std::min(p0.x, p1.x);
    float right = std::max(p0.x, p1.x);
    float top = std::min(p0.y, p1.y);
    float bottom = std::max(p0.y, p1.y);
    for (int py = (int)std::floor(top); py <= (int)std::ceil(bottom); ++py) {
        for (int px = (int)std::floor(left); px <= (int)std::ceil(right); ++px) {
            float xOverlap = std::max(0.0f, std::min((float)px + 1.0f, right) - std::max((float)px, left));
            float yOverlap = std::max(0.0f, std::min((float)py + 1.0f, bottom) - std::max((float)py, top));
            float coverage = xOverlap * yOverlap;
            if (coverage > 0.0f && fill.a > 0.0f) {
                svgBlendPixel(canvas, px, py, fill, coverage);
            }
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
    
    std::vector<Vec2> points;
    const float K = 0.55228475f; // Cubic bezier kappa for elliptical arc approximation
    
    // Quadrant 1: Top-Right (from 0 to 90 deg / from (cx + rx, cy) to (cx, cy + ry))
    Vec2 p0 = {cx + rx, cy};
    Vec2 cp1 = {cx + rx, cy + ry * K};
    Vec2 cp2 = {cx + rx * K, cy + ry};
    Vec2 p1 = {cx, cy + ry};
    
    // Evaluate Q1
    appendCubic(points,
                svgMapPoint(canvas, p0.x, p0.y),
                svgMapPoint(canvas, cp1.x, cp1.y),
                svgMapPoint(canvas, cp2.x, cp2.y),
                svgMapPoint(canvas, p1.x, p1.y));
                
    // Quadrant 2: Bottom-Right (from (cx, cy + ry) to (cx - rx, cy))
    Vec2 cp3 = {cx - rx * K, cy + ry};
    Vec2 cp4 = {cx - rx, cy + ry * K};
    Vec2 p2 = {cx - rx, cy};
    
    // Evaluate Q2
    appendCubic(points,
                svgMapPoint(canvas, p1.x, p1.y),
                svgMapPoint(canvas, cp3.x, cp3.y),
                svgMapPoint(canvas, cp4.x, cp4.y),
                svgMapPoint(canvas, p2.x, p2.y));
                
    // Quadrant 3: Bottom-Left (from (cx - rx, cy) to (cx, cy - ry))
    Vec2 cp5 = {cx - rx, cy - ry * K};
    Vec2 cp6 = {cx - rx * K, cy - ry};
    Vec2 p3 = {cx, cy - ry};
    
    // Evaluate Q3
    appendCubic(points,
                svgMapPoint(canvas, p2.x, p2.y),
                svgMapPoint(canvas, cp5.x, cp5.y),
                svgMapPoint(canvas, cp6.x, cp6.y),
                svgMapPoint(canvas, p3.x, p3.y));
                
    // Quadrant 4: Top-Left (from (cx, cy - ry) to (cx + rx, cy))
    Vec2 cp7 = {cx + rx * K, cy - ry};
    Vec2 cp8 = {cx + rx, cy - ry * K};
    
    // Evaluate Q4
    appendCubic(points,
                svgMapPoint(canvas, p3.x, p3.y),
                svgMapPoint(canvas, cp7.x, cp7.y),
                svgMapPoint(canvas, cp8.x, cp8.y),
                svgMapPoint(canvas, p0.x, p0.y));
                
    if (fill.a > 0.0f) fillPolygon(canvas, points, fill);
    if (stroke.a > 0.0f && strokeWidth > 0.0f) {
        strokePolyline(canvas, points, stroke, strokeWidth, true);
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
        value = parseLocaleIndependentFloat(d.c_str() + pos, &end);
        if (end == d.c_str() + pos) return false;
        pos = (size_t)(end - d.c_str());
        return true;
    }

    bool readFlag(float& value) {
        skip();
        if (pos >= d.size()) return false;
        char c = d[pos];
        if (c == '0' || c == '1') {
            value = (c == '1') ? 1.0f : 0.0f;
            pos++;
            return true;
        }
        return false;
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

void appendArc(std::vector<Vec2>& out, Vec2 p0, float rx, float ry, float angle, bool largeArcFlag, bool sweepFlag, Vec2 p1, SvgCanvas& canvas) {
    double dx2 = (p0.x - p1.x) / 2.0;
    double dy2 = (p0.y - p1.y) / 2.0;
    double angleRad = angle * 3.14159265358979323846 / 180.0;
    double cosAngle = std::cos(angleRad);
    double sinAngle = std::sin(angleRad);
    
    double x1_prime = cosAngle * dx2 + sinAngle * dy2;
    double y1_prime = -sinAngle * dx2 + cosAngle * dy2;
    
    double rx_d = std::abs(rx);
    double ry_d = std::abs(ry);
    if (rx_d == 0.0 || ry_d == 0.0) {
        out.push_back(svgMapPoint(canvas, p1.x, p1.y));
        return;
    }
    
    double lambda = (x1_prime * x1_prime) / (rx_d * rx_d) + (y1_prime * y1_prime) / (ry_d * ry_d);
    if (lambda > 1.0) {
        rx_d *= std::sqrt(lambda);
        ry_d *= std::sqrt(lambda);
    }
    
    double rx_sq = rx_d * rx_d;
    double ry_sq = ry_d * ry_d;
    double x1_prime_sq = x1_prime * x1_prime;
    double y1_prime_sq = y1_prime * y1_prime;
    
    double sq_term = (rx_sq * ry_sq - rx_sq * y1_prime_sq - ry_sq * x1_prime_sq) / (rx_sq * y1_prime_sq + ry_sq * x1_prime_sq);
    if (sq_term < 0.0) sq_term = 0.0;
    double sign = (largeArcFlag == sweepFlag) ? -1.0 : 1.0;
    double coef = sign * std::sqrt(sq_term);
    
    double cx_prime = coef * ((rx_d * y1_prime) / ry_d);
    double cy_prime = coef * -((ry_d * x1_prime) / rx_d);
    
    double cx = cosAngle * cx_prime - sinAngle * cy_prime + (p0.x + p1.x) / 2.0;
    double cy = sinAngle * cx_prime + cosAngle * cy_prime + (p0.y + p1.y) / 2.0;
    
    double ux = (x1_prime - cx_prime) / rx_d;
    double uy = (y1_prime - cy_prime) / ry_d;
    double vx = (-x1_prime - cx_prime) / rx_d;
    double vy = (-y1_prime - cy_prime) / ry_d;
    
    auto vectorAngle = [](double ux_val, double uy_val, double vx_val, double vy_val) {
        double dot = ux_val * vx_val + uy_val * vy_val;
        double len_u = std::sqrt(ux_val * ux_val + uy_val * uy_val);
        double len_v = std::sqrt(vx_val * vx_val + vy_val * vy_val);
        double val = dot / std::max(0.000001, len_u * len_v);
        if (val < -1.0) val = -1.0;
        if (val > 1.0) val = 1.0;
        double a = std::acos(val);
        if ((ux_val * vy_val - uy_val * vx_val) < 0.0) a = -a;
        return a;
    };
    
    double startAngle = vectorAngle(1.0, 0.0, ux, uy);
    double deltaAngle = vectorAngle(ux, uy, vx, vy);
    
    if (!sweepFlag && deltaAngle > 0.0) {
        deltaAngle -= 2.0 * 3.14159265358979323846;
    } else if (sweepFlag && deltaAngle < 0.0) {
        deltaAngle += 2.0 * 3.14159265358979323846;
    }
    
    int segments = std::max(6, static_cast<int>(std::ceil(std::abs(deltaAngle) / (10.0 * 3.14159265358979323846 / 180.0))));
    for (int i = 1; i <= segments; ++i) {
        double t = static_cast<double>(i) / segments;
        double currentAngle = startAngle + deltaAngle * t;
        double x_prime = rx_d * std::cos(currentAngle);
        double y_prime = ry_d * std::sin(currentAngle);
        double px = cosAngle * x_prime - sinAngle * y_prime + cx;
        double py = sinAngle * x_prime + cosAngle * y_prime + cy;
        out.push_back(svgMapPoint(canvas, static_cast<float>(px), static_cast<float>(py)));
    }
}

void drawSvgPath(SvgCanvas& canvas, const std::string& d, Color fill, Color stroke, float strokeWidth, bool fillRuleEvenOdd = false) {
    SvgPathParser parser{d};
    std::vector<std::vector<Vec2>> subpaths;
    std::vector<bool> subpathClosed;
    std::vector<Vec2> currentPath;
    Vec2 current = {0, 0};
    Vec2 start = {0, 0};
    Vec2 lastControl = {0, 0};
    char lastCmd = 0;

    auto flushPath = [&](bool isClosed = false) {
        if (!currentPath.empty()) {
            subpaths.push_back(currentPath);
            subpathClosed.push_back(isClosed);
            currentPath.clear();
        }
    };

    auto ensureStartPoint = [&]() {
        if (currentPath.empty()) {
            currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            start = current;
        }
    };

    while (parser.pos < d.size() && parser.readCommand()) {
        size_t startPos = parser.pos;
        char cmd = parser.command;
        bool rel = std::islower((unsigned char)cmd) != 0;
        char upper = (char)std::toupper((unsigned char)cmd);
        if (upper == 'Z') {
            if (!currentPath.empty()) currentPath.push_back(svgMapPoint(canvas, start.x, start.y));
            current = start;
            flushPath(true);
            parser.command = 0;
            lastControl = current;
            lastCmd = 'Z';
            continue;
        }

        auto map = [&](float x, float y) {
            return rel ? Vec2(current.x + x, current.y + y) : Vec2(x, y);
        };

        if (upper == 'M') {
            float x = 0, y = 0;
            if (!parser.readNumber(x) || !parser.readNumber(y)) break;
            flushPath(false);
            current = map(x, y);
            start = current;
            currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            parser.command = rel ? 'l' : 'L';
            lastControl = current;
            lastCmd = 'M';
        } else if (upper == 'L') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x = 0, y = 0;
                if (!parser.readNumber(x) || !parser.readNumber(y)) break;
                current = map(x, y);
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
            lastControl = current;
            lastCmd = 'L';
        } else if (upper == 'H') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x = 0;
                if (!parser.readNumber(x)) break;
                current.x = rel ? current.x + x : x;
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
            lastControl = current;
            lastCmd = 'H';
        } else if (upper == 'V') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float y = 0;
                if (!parser.readNumber(y)) break;
                current.y = rel ? current.y + y : y;
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
            lastControl = current;
            lastCmd = 'V';
        } else if (upper == 'C') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x1, y1, x2, y2, x, y;
                if (!parser.readNumber(x1) || !parser.readNumber(y1) ||
                    !parser.readNumber(x2) || !parser.readNumber(y2) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = rel ? svgMapPoint(canvas, current.x + x1, current.y + y1) : svgMapPoint(canvas, x1, y1);
                Vec2 c2 = rel ? svgMapPoint(canvas, current.x + x2, current.y + y2) : svgMapPoint(canvas, x2, y2);
                Vec2 unmappedC2 = rel ? Vec2(current.x + x2, current.y + y2) : Vec2(x2, y2);
                current = map(x, y);
                Vec2 p3 = svgMapPoint(canvas, current.x, current.y);
                appendCubic(currentPath, p0, c1, c2, p3);
                lastControl = unmappedC2;
            }
            lastCmd = 'C';
        } else if (upper == 'S') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x2, y2, x, y;
                if (!parser.readNumber(x2) || !parser.readNumber(y2) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 unmappedC1;
                if (lastCmd == 'C' || lastCmd == 'S') {
                    unmappedC1.x = 2.0f * current.x - lastControl.x;
                    unmappedC1.y = 2.0f * current.y - lastControl.y;
                } else {
                    unmappedC1 = current;
                }
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = svgMapPoint(canvas, unmappedC1.x, unmappedC1.y);
                Vec2 c2 = rel ? svgMapPoint(canvas, current.x + x2, current.y + y2) : svgMapPoint(canvas, x2, y2);
                Vec2 unmappedC2 = rel ? Vec2(current.x + x2, current.y + y2) : Vec2(x2, y2);
                current = map(x, y);
                Vec2 p3 = svgMapPoint(canvas, current.x, current.y);
                appendCubic(currentPath, p0, c1, c2, p3);
                lastControl = unmappedC2;
                lastCmd = 'S';
            }
        } else if (upper == 'Q') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x1, y1, x, y;
                if (!parser.readNumber(x1) || !parser.readNumber(y1) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = rel ? svgMapPoint(canvas, current.x + x1, current.y + y1) : svgMapPoint(canvas, x1, y1);
                Vec2 unmappedC1 = rel ? Vec2(current.x + x1, current.y + y1) : Vec2(x1, y1);
                current = map(x, y);
                Vec2 p2 = svgMapPoint(canvas, current.x, current.y);
                appendQuad(currentPath, p0, c1, p2);
                lastControl = unmappedC1;
            }
            lastCmd = 'Q';
        } else if (upper == 'T') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x, y;
                if (!parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 unmappedC1;
                if (lastCmd == 'Q' || lastCmd == 'T') {
                    unmappedC1.x = 2.0f * current.x - lastControl.x;
                    unmappedC1.y = 2.0f * current.y - lastControl.y;
                } else {
                    unmappedC1 = current;
                }
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = svgMapPoint(canvas, unmappedC1.x, unmappedC1.y);
                current = map(x, y);
                Vec2 p2 = svgMapPoint(canvas, current.x, current.y);
                appendQuad(currentPath, p0, c1, p2);
                lastControl = unmappedC1;
                lastCmd = 'T';
            }
        } else if (upper == 'A') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float rx, ry, rot, largeArc, sweep, x, y;
                if (!parser.readNumber(rx) || !parser.readNumber(ry) || !parser.readNumber(rot) ||
                    !parser.readFlag(largeArc) || !parser.readFlag(sweep) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p1 = map(x, y);
                appendArc(currentPath, current, rx, ry, rot, largeArc != 0.0f, sweep != 0.0f, p1, canvas);
                current = p1;
            }
            lastControl = current;
            lastCmd = 'A';
        } else {
            parser.command = 0;
            ++parser.pos;
        }

        if (parser.pos == startPos) {
            parser.command = 0;
            break;
        }
    }
    flushPath(false);

    if (fill.a > 0.0f) {
        fillMultiPolygon(canvas, subpaths, fill, fillRuleEvenOdd);
    }
    for (size_t i = 0; i < subpaths.size(); ++i) {
        bool isClosed = i < subpathClosed.size() ? subpathClosed[i] : false;
        if (stroke.a > 0.0f) strokePolyline(canvas, subpaths[i], stroke, strokeWidth, isClosed);
    }
}

void paintSvgRenderableElement(SvgCanvas& canvas,
                               const std::string& name,
                               const SvgAttrs& attrs,
                               const SvgPaintState& paint) {
    float opacity = parseSvgFloat(paint.opacity, 1.0f);
    bool noFill = false;
    bool noStroke = false;
    Color currentColor = parseSvgColor(paint.color, Color(1, 1, 1, 1));
    Color fill = parseSvgColor(paint.fill, currentColor, &noFill);
    Color stroke = parseSvgColor(paint.stroke, currentColor, &noStroke);
    fill.a *= parseSvgFloat(paint.fillOpacity, 1.0f) * opacity;
    stroke.a *= parseSvgFloat(paint.strokeOpacity, 1.0f) * opacity;
    if (noFill) fill.a = 0.0f;
    if (noStroke) stroke.a = 0.0f;
    float strokeWidth = parseSvgFloat(paint.strokeWidth, 1.0f) *
                        (std::abs(canvas.scaleX) + std::abs(canvas.scaleY)) * 0.5f;

    if (name == "rect") {
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
    } else if (name == "circle") {
        float r = parseSvgFloat(svgAttr(attrs, "r"));
        drawSvgEllipse(canvas, parseSvgFloat(svgAttr(attrs, "cx")),
                       parseSvgFloat(svgAttr(attrs, "cy")), r, r, fill, stroke, strokeWidth);
    } else if (name == "ellipse") {
        drawSvgEllipse(canvas, parseSvgFloat(svgAttr(attrs, "cx")),
                       parseSvgFloat(svgAttr(attrs, "cy")),
                       parseSvgFloat(svgAttr(attrs, "rx")),
                       parseSvgFloat(svgAttr(attrs, "ry")),
                       fill, stroke, strokeWidth);
    } else if (name == "line") {
        std::vector<Vec2> points = {
            svgMapPoint(canvas, parseSvgFloat(svgAttr(attrs, "x1")), parseSvgFloat(svgAttr(attrs, "y1"))),
            svgMapPoint(canvas, parseSvgFloat(svgAttr(attrs, "x2")), parseSvgFloat(svgAttr(attrs, "y2")))
        };
        strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
    } else if (name == "polyline") {
        auto points = parseSvgPoints(svgAttr(attrs, "points"), canvas);
        strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
    } else if (name == "polygon") {
        auto points = parseSvgPoints(svgAttr(attrs, "points"), canvas);
        fillPolygon(canvas, points, fill);
        strokePolyline(canvas, points, stroke, strokeWidth, true);
    } else if (name == "path") {
        std::string fr = lowerSvgString(trimSvgString(paint.fillRule));
        bool isEvenOdd = (fr == "evenodd");
        drawSvgPath(canvas, svgAttr(attrs, "d"), fill, stroke, std::max(1.0f, strokeWidth), isEvenOdd);
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
    SvgAspectRatio aspect = parseSvgPreserveAspectRatio(
        svgAttr(root, "preserveaspectratio", "xMidYMid meet"));
    float rawScaleX = outW / std::max(1.0f, viewW);
    float rawScaleY = outH / std::max(1.0f, viewH);
    if (aspect.none) {
        canvas.scaleX = rawScaleX;
        canvas.scaleY = rawScaleY;
    } else {
        float uniformScale = aspect.slice ? std::max(rawScaleX, rawScaleY)
                                          : std::min(rawScaleX, rawScaleY);
        canvas.scaleX = uniformScale;
        canvas.scaleY = uniformScale;
        canvas.offsetX = (outW - viewW * uniformScale) * aspect.alignX;
        canvas.offsetY = (outH - viewH * uniformScale) * aspect.alignY;
    }
    SvgPaintState rootPaint;
    rootPaint.fill = svgAttr(root, "fill", "currentColor");
    rootPaint.stroke = svgAttr(root, "stroke", "none");
    rootPaint.opacity = svgAttr(root, "opacity", "1");
    rootPaint.fillOpacity = svgAttr(root, "fill-opacity", "1");
    rootPaint.strokeOpacity = svgAttr(root, "stroke-opacity", "1");
    rootPaint.strokeWidth = svgAttr(root, "stroke-width", "1");
    rootPaint.color = svgAttr(root, "color", "white");

    std::vector<SvgAffine> transformStack;
    transformStack.push_back(SvgAffine::identity());
    std::vector<SvgPaintState> paintStack;
    paintStack.push_back(rootPaint);

    std::unordered_map<std::string, std::string> idTags;
    size_t indexPos = 0;
    while ((indexPos = svg.find('<', indexPos)) != std::string::npos) {
        if (indexPos + 1 >= svg.size() || svg[indexPos + 1] == '/' ||
            svg[indexPos + 1] == '!' || svg[indexPos + 1] == '?') {
            ++indexPos;
            continue;
        }
        size_t indexEnd = svg.find('>', indexPos);
        if (indexEnd == std::string::npos) break;
        std::string tag = svg.substr(indexPos, indexEnd - indexPos + 1);
        std::string name = svgTagName(tag);
        if (isSvgRenderableTag(name)) {
            SvgAttrs attrs = parseSvgAttrs(tag);
            std::string id = svgAttr(attrs, "id");
            if (!id.empty() && idTags.find(id) == idTags.end()) {
                idTags[id] = tag;
            }
        }
        indexPos = indexEnd + 1;
    }

    int definitionDepth = 0;
    size_t pos = 0;
    while ((pos = svg.find('<', pos)) != std::string::npos) {
        if (pos + 1 >= svg.size()) break;
        if (svg[pos + 1] == '/') {
            size_t closeEnd = svg.find('>', pos);
            if (closeEnd == std::string::npos) break;
            std::string closeName = svgTagName(svg.substr(pos, closeEnd - pos + 1), 2);
            if (isSvgDefinitionContainer(closeName)) {
                definitionDepth = std::max(0, definitionDepth - 1);
                pos = closeEnd + 1;
                continue;
            }
            if ((closeName == "g" || closeName == "svg") && transformStack.size() > 1) {
                transformStack.pop_back();
                paintStack.pop_back();
            }
            pos = closeEnd + 1;
            continue;
        }
        if (svg[pos + 1] == '!' || svg[pos + 1] == '?') {
            ++pos;
            continue;
        }
        size_t end = svg.find('>', pos);
        if (end == std::string::npos) break;
        std::string tag = svg.substr(pos, end - pos + 1);
        std::string name = svgTagName(tag);
        SvgAttrs attrs = parseSvgAttrs(tag);
        bool selfClosing = svgTagIsSelfClosing(tag);
        if (isSvgDefinitionContainer(name)) {
            if (!selfClosing) ++definitionDepth;
            pos = end + 1;
            continue;
        }
        if (definitionDepth > 0) {
            pos = end + 1;
            continue;
        }
        SvgAffine elementTransform = transformStack.back().multiply(
            parseSvgTransformList(svgAttr(attrs, "transform")));
        SvgPaintState elementPaint = mergeSvgPaintState(paintStack.back(), attrs);
        if (name == "g" || name == "svg") {
            if (!selfClosing) {
                transformStack.push_back(elementTransform);
                paintStack.push_back(elementPaint);
            }
            pos = end + 1;
            continue;
        }

        SvgAffine previousTransform = canvas.transform;
        canvas.transform = elementTransform;
        if (name == "use") {
            std::string href = svgAttr(attrs, "href");
            if (href.empty()) href = svgAttr(attrs, "xlink:href");
            if (!href.empty() && href[0] == '#') {
                auto refIt = idTags.find(href.substr(1));
                if (refIt != idTags.end()) {
                    SvgAttrs refAttrs = parseSvgAttrs(refIt->second);
                    std::string refName = svgTagName(refIt->second);
                    SvgAffine useTransform = canvas.transform
                        .multiply(SvgAffine::translate(parseSvgFloat(svgAttr(attrs, "x")),
                                                       parseSvgFloat(svgAttr(attrs, "y"))))
                        .multiply(parseSvgTransformList(svgAttr(refAttrs, "transform")));
                    SvgPaintState refPaint = mergeSvgPaintState(elementPaint, refAttrs);
                    canvas.transform = useTransform;
                    paintSvgRenderableElement(canvas, refName, refAttrs, refPaint);
                }
            }
        } else if (isSvgRenderableTag(name)) {
            paintSvgRenderableElement(canvas, name, attrs, elementPaint);
        }
        canvas.transform = previousTransform;
        pos = end + 1;
    }

    return true;
}

} // namespace

#if FLUXUI_HAS_VULKAN_SDK
struct VulkanRendererState;
bool ensureVulkanImageTexture(VulkanRendererState& state,
                              const std::string& key,
                              ImageData& image);
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

bool Renderer::rasterizeSvgWidget(Widget* svgWidget, ImageData& image) {
    if (!svgWidget || svgWidget->type != "svg") return false;
    
    Svg* svg = static_cast<Svg*>(svgWidget);
    std::vector<float> vb = parseSvgNumberList(svg->viewBox);
    float viewX = vb.size() >= 4 ? vb[0] : 0.0f;
    float viewY = vb.size() >= 4 ? vb[1] : 0.0f;
    float viewW = vb.size() >= 4 ? vb[2] : 0.0f;
    float viewH = vb.size() >= 4 ? vb[3] : 0.0f;
    
    float width = svg->bounds.w;
    float height = svg->bounds.h;
    if (width <= 0.0f) width = parseSvgFloat(svg->width, viewW > 0.0f ? viewW : 300.0f);
    if (height <= 0.0f) height = parseSvgFloat(svg->height, viewH > 0.0f ? viewH : 150.0f);
    
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
    
    SvgAspectRatio aspect = parseSvgPreserveAspectRatio(svg->preserveAspectRatio);
    float rawScaleX = outW / std::max(1.0f, viewW);
    float rawScaleY = outH / std::max(1.0f, viewH);
    if (aspect.none) {
        canvas.scaleX = rawScaleX;
        canvas.scaleY = rawScaleY;
    } else {
        float uniformScale = aspect.slice ? std::max(rawScaleX, rawScaleY)
                                          : std::min(rawScaleX, rawScaleY);
        canvas.scaleX = uniformScale;
        canvas.scaleY = uniformScale;
        canvas.offsetX = (outW - viewW * uniformScale) * aspect.alignX;
        canvas.offsetY = (outH - viewH * uniformScale) * aspect.alignY;
    }
    
    SvgPaintState rootPaint;
    rootPaint.fill = "currentColor";
    rootPaint.stroke = "none";
    rootPaint.opacity = "1";
    rootPaint.fillOpacity = "1";
    rootPaint.strokeOpacity = "1";
    rootPaint.strokeWidth = "1";
    rootPaint.color = "currentColor";
    
    std::vector<SvgAffine> transformStack;
    transformStack.push_back(SvgAffine::identity());
    std::vector<SvgPaintState> paintStack;
    paintStack.push_back(rootPaint);
    
    std::function<void(Widget*)> traverse = [&](Widget* w) {
        if (!w || !w->visible) return;
        
        std::string type = w->type;
        if (type == "svg" && w != svgWidget) return;
        
        if (type == "g") {
            SvgG* g = static_cast<SvgG*>(w);
            SvgAffine elementTransform = transformStack.back().multiply(
                parseSvgTransformList(g->transformAttr));
                
            SvgAttrs attrs;
            if (!g->fill.empty()) attrs["fill"] = g->fill;
            if (!g->stroke.empty()) attrs["stroke"] = g->stroke;
            if (!g->strokeWidth.empty()) attrs["stroke-width"] = g->strokeWidth;
            if (!g->opacityAttr.empty()) attrs["opacity"] = g->opacityAttr;
            if (!g->fillOpacity.empty()) attrs["fill-opacity"] = g->fillOpacity;
            if (!g->strokeOpacity.empty()) attrs["stroke-opacity"] = g->strokeOpacity;
            if (!g->fillRuleAttr.empty()) attrs["fill-rule"] = g->fillRuleAttr;
            
            SvgPaintState elementPaint = mergeSvgPaintState(paintStack.back(), attrs);
            
            transformStack.push_back(elementTransform);
            paintStack.push_back(elementPaint);
            
            for (auto& child : g->children) {
                traverse(child.get());
            }
            
            transformStack.pop_back();
            paintStack.pop_back();
            return;
        }
        
        if (type == "path" || type == "rect" || type == "circle" || type == "ellipse" ||
            type == "line" || type == "polyline" || type == "polygon") {
            SvgElement* elem = static_cast<SvgElement*>(w);
            SvgAffine elementTransform = transformStack.back().multiply(
                parseSvgTransformList(elem->transformAttr));
                
            SvgAttrs attrs;
            if (!elem->fill.empty()) attrs["fill"] = elem->fill;
            if (!elem->stroke.empty()) attrs["stroke"] = elem->stroke;
            if (!elem->strokeWidth.empty()) attrs["stroke-width"] = elem->strokeWidth;
            if (!elem->opacityAttr.empty()) attrs["opacity"] = elem->opacityAttr;
            if (!elem->fillOpacity.empty()) attrs["fill-opacity"] = elem->fillOpacity;
            if (!elem->strokeOpacity.empty()) attrs["stroke-opacity"] = elem->strokeOpacity;
            if (!elem->fillRuleAttr.empty()) attrs["fill-rule"] = elem->fillRuleAttr;
            
            SvgPaintState paint = mergeSvgPaintState(paintStack.back(), attrs);
            
            bool noFill = false;
            bool noStroke = false;
            Color currentColor = parseSvgColor(paint.color, svgWidget->computedStyle->color);
            Color fill = parseSvgColor(paint.fill, currentColor, &noFill);
            Color stroke = parseSvgColor(paint.stroke, currentColor, &noStroke);
            float opacity = parseSvgFloat(paint.opacity, 1.0f);
            fill.a *= parseSvgFloat(paint.fillOpacity, 1.0f) * opacity;
            stroke.a *= parseSvgFloat(paint.strokeOpacity, 1.0f) * opacity;
            if (noFill) fill.a = 0.0f;
            if (noStroke) stroke.a = 0.0f;
            float strokeWidth = parseSvgFloat(paint.strokeWidth, 1.0f) *
                                (std::abs(canvas.scaleX) + std::abs(canvas.scaleY)) * 0.5f;
                                
            SvgAffine previousTransform = canvas.transform;
            canvas.transform = elementTransform;
            
            if (type == "rect") {
                SvgRect* r = static_cast<SvgRect*>(w);
                drawSvgRect(canvas,
                            parseSvgFloat(r->x),
                            parseSvgFloat(r->y),
                            parseSvgFloat(r->width),
                            parseSvgFloat(r->height),
                            parseSvgFloat(r->rx),
                            parseSvgFloat(r->ry),
                            fill, stroke, strokeWidth);
            } else if (type == "circle") {
                SvgCircle* c = static_cast<SvgCircle*>(w);
                float rad = parseSvgFloat(c->r);
                drawSvgEllipse(canvas,
                               parseSvgFloat(c->cx),
                               parseSvgFloat(c->cy),
                               rad, rad,
                               fill, stroke, strokeWidth);
            } else if (type == "ellipse") {
                SvgEllipse* el = static_cast<SvgEllipse*>(w);
                drawSvgEllipse(canvas,
                               parseSvgFloat(el->cx),
                               parseSvgFloat(el->cy),
                               parseSvgFloat(el->rx),
                               parseSvgFloat(el->ry),
                               fill, stroke, strokeWidth);
            } else if (type == "line") {
                SvgLine* l = static_cast<SvgLine*>(w);
                std::vector<Vec2> points = {
                    svgMapPoint(canvas, parseSvgFloat(l->x1), parseSvgFloat(l->y1)),
                    svgMapPoint(canvas, parseSvgFloat(l->x2), parseSvgFloat(l->y2))
                };
                strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
            } else if (type == "polyline") {
                SvgPolyline* pl = static_cast<SvgPolyline*>(w);
                auto points = parseSvgPoints(pl->points, canvas);
                strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
            } else if (type == "polygon") {
                SvgPolygon* pg = static_cast<SvgPolygon*>(w);
                auto points = parseSvgPoints(pg->points, canvas);
                fillPolygon(canvas, points, fill);
                strokePolyline(canvas, points, stroke, strokeWidth, true);
            } else if (type == "path") {
                SvgPath* p = static_cast<SvgPath*>(w);
                std::string fr = lowerSvgString(trimSvgString(paint.fillRule));
                bool isEvenOdd = (fr == "evenodd");
                drawSvgPath(canvas, p->d, fill, stroke, std::max(1.0f, strokeWidth), isEvenOdd);
            }
            
            canvas.transform = previousTransform;
        }
    };
    
    for (auto& child : svg->children) {
        traverse(child.get());
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

struct SoftwareClip {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

uint8_t softwareToByte(float value) {
    return static_cast<uint8_t>(
        std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

uint32_t softwarePackOpaque(const Color& color) {
    return 0xff000000u |
           (static_cast<uint32_t>(softwareToByte(color.r)) << 16) |
           (static_cast<uint32_t>(softwareToByte(color.g)) << 8) |
           static_cast<uint32_t>(softwareToByte(color.b));
}

Color softwareColorFromInstance(const float rgba[4]) {
    return Color(rgba[0], rgba[1], rgba[2], rgba[3]);
}

SoftwareClip softwareClipFor(const std::vector<Rect>& scissorStack,
                             int width,
                             int height,
                             float dpiScale) {
    SoftwareClip clip{0, 0, std::max(0, width), std::max(0, height)};
    if (scissorStack.empty()) {
        return clip;
    }

    const Rect& r = scissorStack.back();
    clip.x0 = std::clamp(static_cast<int>(std::floor(r.x * dpiScale)), 0, width);
    clip.y0 = std::clamp(static_cast<int>(std::floor(r.y * dpiScale)), 0, height);
    clip.x1 = std::clamp(static_cast<int>(std::ceil((r.x + r.w) * dpiScale)), 0, width);
    clip.y1 = std::clamp(static_cast<int>(std::ceil((r.y + r.h) * dpiScale)), 0, height);
    if (clip.x1 < clip.x0) clip.x1 = clip.x0;
    if (clip.y1 < clip.y0) clip.y1 = clip.y0;
    return clip;
}

uint32_t softwareNextCodepoint(const std::string& s, size_t& i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) return static_cast<uint32_t>(s[i++]);
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        uint32_t cp = ((s[i++] & 0x1F) << 6) | (s[i++] & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        uint32_t cp = ((s[i++] & 0x0F) << 12) |
                      ((s[i++] & 0x3F) << 6) |
                      (s[i++] & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
        uint32_t cp = ((s[i++] & 0x07) << 18) |
                      ((s[i++] & 0x3F) << 12) |
                      ((s[i++] & 0x3F) << 6) |
                      (s[i++] & 0x3F);
        return cp;
    }
    return static_cast<uint32_t>(s[i++]);
}

float softwareRoundedCoverage(float px, float py, const Rect& rect, float radius) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) {
        return 0.0f;
    }
    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    if (radius <= 0.0f) {
        return 1.0f;
    }

    float lx = px - rect.x;
    float ly = py - rect.y;

    if (lx < -0.5f || lx > rect.w + 0.5f || ly < -0.5f || ly > rect.h + 0.5f) {
        return 0.0f;
    }

    float cx = std::clamp(lx, radius, rect.w - radius);
    float cy = std::clamp(ly, radius, rect.h - radius);
    float dx = lx - cx;
    float dy = ly - cy;

    if (dx == 0.0f && dy == 0.0f) {
        return 1.0f;
    }
    if (dx == 0.0f) {
        return std::clamp(radius + 0.5f - std::abs(dy), 0.0f, 1.0f);
    }
    if (dy == 0.0f) {
        return std::clamp(radius + 0.5f - std::abs(dx), 0.0f, 1.0f);
    }

    float distance = std::sqrt(dx * dx + dy * dy);
    return std::clamp(radius + 0.5f - distance, 0.0f, 1.0f);
}

Color softwareGradientColor(const Color& a,
                            const Color& b,
                            const Rect& rect,
                            float px,
                            float py,
                            float angleDegrees) {
    float angle = angleDegrees * 3.14159265358979323846f / 180.0f;
    float vx = std::cos(angle);
    float vy = std::sin(angle);
    float nx = rect.w > 0.0f ? (px - rect.x) / rect.w - 0.5f : 0.0f;
    float ny = rect.h > 0.0f ? (py - rect.y) / rect.h - 0.5f : 0.0f;
    float t = std::clamp(nx * vx + ny * vy + 0.5f, 0.0f, 1.0f);
    return Color::lerp(a, b, t);
}

void softwareBlendPixel(std::vector<uint32_t>& pixels,
                        int width,
                        int height,
                        int x,
                        int y,
                        const Color& color,
                        float alphaScale) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    float srcA = std::clamp(color.a * alphaScale, 0.0f, 1.0f);
    uint32_t a_int = static_cast<uint32_t>(srcA * 255.0f);
    if (a_int == 0) {
        return;
    }

    uint32_t srcR = static_cast<uint32_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    uint32_t srcG = static_cast<uint32_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    uint32_t srcB = static_cast<uint32_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);

    uint32_t& dst = pixels[static_cast<size_t>(y) * width + x];
    uint32_t dstR = (dst >> 16) & 0xffu;
    uint32_t dstG = (dst >> 8) & 0xffu;
    uint32_t dstB = dst & 0xffu;

    uint32_t outR = (srcR * a_int + dstR * (255u - a_int)) / 255u;
    uint32_t outG = (srcG * a_int + dstG * (255u - a_int)) / 255u;
    uint32_t outB = (srcB * a_int + dstB * (255u - a_int)) / 255u;

    dst = 0xff000000u | (outR << 16) | (outG << 8) | outB;
}

inline void softwareBlendPixelFast(uint32_t& dst,
                                   uint32_t srcR,
                                   uint32_t srcG,
                                   uint32_t srcB,
                                   uint32_t a_int) {
    if (a_int == 0) {
        return;
    }
    if (a_int >= 255u) {
        dst = 0xff000000u | (srcR << 16) | (srcG << 8) | srcB;
        return;
    }

    uint32_t dstVal = dst;
    uint32_t dstR = (dstVal >> 16) & 0xffu;
    uint32_t dstG = (dstVal >> 8) & 0xffu;
    uint32_t dstB = dstVal & 0xffu;

    // Division-free /255 approximation: (v * 257 + 257) >> 16 == round(v/255).
    // This is the per-pixel hot path of the CPU rasterizer; replacing three
    // integer divisions with multiply+shift is a measurable software-mode win.
    uint32_t invA = 255u - a_int;
    uint32_t sumR = srcR * a_int + dstR * invA;
    uint32_t sumG = srcG * a_int + dstG * invA;
    uint32_t sumB = srcB * a_int + dstB * invA;
    uint32_t outR = (sumR + (sumR >> 8) + 1u) >> 8;
    uint32_t outG = (sumG + (sumG >> 8) + 1u) >> 8;
    uint32_t outB = (sumB + (sumB >> 8) + 1u) >> 8;

    dst = 0xff000000u | (outR << 16) | (outG << 8) | outB;
}

float softwareSampleFontAlpha(const FontData& font,
                              int atlasX0,
                              int atlasY0,
                              int atlasW,
                              int atlasH,
                              float sourceX,
                              float sourceY) {
    if (font.atlasPixels.empty() || font.atlasWidth <= 0 || font.atlasHeight <= 0 ||
        atlasW <= 0 || atlasH <= 0) {
        return 0.0f;
    }

    sourceX = std::clamp(sourceX, 0.0f, static_cast<float>(atlasW - 1));
    sourceY = std::clamp(sourceY, 0.0f, static_cast<float>(atlasH - 1));

    int x0 = static_cast<int>(std::floor(sourceX));
    int y0 = static_cast<int>(std::floor(sourceY));
    int x1 = std::min(x0 + 1, atlasW - 1);
    int y1 = std::min(y0 + 1, atlasH - 1);
    float tx = sourceX - static_cast<float>(x0);
    float ty = sourceY - static_cast<float>(y0);

    auto sample = [&](int lx, int ly) -> float {
        int ax = std::clamp(atlasX0 + lx, 0, font.atlasWidth - 1);
        int ay = std::clamp(atlasY0 + ly, 0, font.atlasHeight - 1);
        return font.atlasPixels[static_cast<size_t>(ay) * font.atlasWidth + ax] / 255.0f;
    };

    float a00 = sample(x0, y0);
    float a10 = sample(x1, y0);
    float a01 = sample(x0, y1);
    float a11 = sample(x1, y1);
    float ax0 = a00 + (a10 - a00) * tx;
    float ax1 = a01 + (a11 - a01) * tx;
    return ax0 + (ax1 - ax0) * ty;
}

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

// Resolve the effective GPU preference: an explicit FLUXUI_GPU env var wins,
// otherwise fall back to the app-provided preference (Auto => PowerSaving).
GpuPreference resolveGpuPreference(GpuPreference requested) {
    if (const char* env = std::getenv("FLUXUI_GPU")) {
        std::string v(env);
        for (char& c : v) c = (char)std::tolower((unsigned char)c);
        if (v == "integrated" || v == "igpu" || v == "powersaving" ||
            v == "power-saving" || v == "low-power" || v == "battery") {
            return GpuPreference::PowerSaving;
        }
        if (v == "discrete" || v == "dgpu" || v == "performance" ||
            v == "gpu" || v == "high-performance" || v == "game" || v == "gaming") {
            return GpuPreference::Performance;
        }
    }
    if (requested == GpuPreference::Auto) return GpuPreference::PowerSaving;
    return requested;
}

int physicalDeviceScore(VkPhysicalDevice device, VkSurfaceKHR surface, GpuPreference pref) {
    if (!physicalDeviceUsableForVulkan(device, surface)) {
        return -1;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(device, &properties);

    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        return -1;
    }

    // Base score per device class, then bias by the requested preference.
    // PowerSaving (the UI default) ranks the integrated GPU highest so the app
    // doesn't wake / load a discrete RTX just to draw a window; Performance
    // ranks the discrete GPU highest.
    int score = 100;
    const bool discrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    const bool integrated = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    if (pref == GpuPreference::Performance) {
        if (discrete) score += 1000;
        else if (integrated) score += 500;
    } else {
        // Auto + PowerSaving both favor the integrated GPU for UI work.
        if (integrated) score += 1000;
        else if (discrete) score += 500;
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
    if (width <= 0 || height <= 0) {
        return false;
    }
    auto support = queryVulkanSwapchainSupport(state.physicalDevice, state.surface);
    if (support.formats.empty() || support.presentModes.empty()) {
        std::cerr << "FluxUI: Vulkan surface does not support a usable swapchain." << std::endl;
        return false;
    }

    auto surfaceFormat = chooseVulkanSurfaceFormat(support.formats);
    auto presentMode = chooseVulkanPresentMode(support.presentModes);
    auto extent = chooseVulkanExtent(support.capabilities, width, height);
    if (extent.width == 0 || extent.height == 0) {
        return false;
    }

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

    state.logicalWidth = extent.width;
    state.logicalHeight = extent.height;
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

bool ensureVulkanFontTexture(VulkanRendererState& state,
                             const std::string& key,
                             FontData& font) {
    auto existing = state.fontTextures.find(key);
    if (existing != state.fontTextures.end()) {
        return existing->second.descriptorSet != VK_NULL_HANDLE;
    }
    if (font.atlasPixels.empty()) {
        if (!decompressFontAtlas(font)) {
            return false;
        }
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

    GpuPreference effectivePref = resolveGpuPreference(gpuPreference_);
    int bestScore = -1;
    for (auto device : devices) {
        int score = physicalDeviceScore(device, state.surface, effectivePref);
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

    // Record the selected device name (used by activeDeviceName()/diagnostics).
    {
        VkPhysicalDeviceProperties chosenProps = {};
        vkGetPhysicalDeviceProperties(state.physicalDevice, &chosenProps);
        activeDeviceName_ = chosenProps.deviceName;
#if !FLUXUI_SILENT_STARTUP
        std::cout << "FluxUI: GPU '" << chosenProps.deviceName << "' ("
                  << physicalDeviceTypeName(static_cast<VkPhysicalDeviceType>(chosenProps.deviceType))
                  << ", pref=" << (effectivePref == GpuPreference::Performance ? "performance" : "power-saving")
                  << ")" << std::endl;
#endif
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
#if !FLUXUI_SILENT_STARTUP
    std::cout << "FluxUI Renderer initialized (Vulkan "
              << vkVersionMajor(properties.apiVersion) << "."
              << vkVersionMinor(properties.apiVersion) << "."
              << vkVersionPatch(properties.apiVersion)
              << ", device " << properties.deviceName << ")" << std::endl;
#else
    (void)properties;
#endif

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
    if (!vulkan_ || !vulkan_->device) {
        return false;
    }

    auto& state = *vulkan_;
    windowWidth_ = std::max(1, w);
    windowHeight_ = std::max(1, h);

    if (state.swapchainDirty ||
        state.frames.empty() ||
        state.logicalWidth != windowWidth_ ||
        state.logicalHeight != windowHeight_) {
        vkDeviceWaitIdle(state.device);
        destroyVulkanSwapchain(state);
        if (!createVulkanSwapchain(state, windowWidth_, windowHeight_)) {
            return false;
        }
    }

    if (state.frames.empty()) {
        return false;
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
    trimVulkanDynamicPool(state, frame.roundedInstances, FLUXUI_VULKAN_RETAINED_DYNAMIC_PAGES);
    trimVulkanDynamicPool(state, frame.textVertices, FLUXUI_VULKAN_RETAINED_DYNAMIC_PAGES);
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
    clearValue.color.float32[0] = 1.0f;
    clearValue.color.float32[1] = 1.0f;
    clearValue.color.float32[2] = 1.0f;
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

bool Renderer::initSoftware(void* windowHandle) {
#if defined(_WIN32) || defined(__ANDROID__)
    window_ = windowHandle;
    int w = 800;
    int h = 600;
    if (window_) {
        Platform::getWindowSize(window_, w, h);
    }
    softwareWidth_ = std::max(1, w);
    softwareHeight_ = std::max(1, h);
    windowWidth_ = softwareWidth_;
    windowHeight_ = softwareHeight_;
    dpiScale_ = 1.0f;
    softwarePixels_.assign(static_cast<size_t>(softwareWidth_) * softwareHeight_,
                           softwarePackOpaque(Color(0.06f, 0.06f, 0.09f, 1.0f)));
    activeBackend_ = RenderBackendType::Compatibility;
    backendInitialized_ = true;
#if !FLUXUI_SILENT_STARTUP
    std::cout << "FluxUI Renderer initialized (CPU software compatibility)"
              << std::endl;
#endif
    return true;
#else
    (void)windowHandle;
    std::cerr << "FluxUI: CPU software compatibility renderer is implemented "
              << "for Windows and Android in this build." << std::endl;
    return false;
#endif
}

void Renderer::shutdownSoftware() {
    softwarePixels_.clear();
    softwarePixels_.shrink_to_fit();
    softwareWidth_ = 0;
    softwareHeight_ = 0;
    softwareFrameActive_ = false;
}

void Renderer::beginSoftwareFrame(int w, int h) {
    windowWidth_ = std::max(1, w);
    windowHeight_ = std::max(1, h);
    dpiScale_ = 1.0f;

    if (softwareWidth_ != windowWidth_ || softwareHeight_ != windowHeight_) {
        softwareWidth_ = windowWidth_;
        softwareHeight_ = windowHeight_;
        softwarePixels_.resize(static_cast<size_t>(softwareWidth_) * softwareHeight_);
    }

    std::fill(softwarePixels_.begin(), softwarePixels_.end(),
              softwarePackOpaque(Color(1.0f, 1.0f, 1.0f, 1.0f)));
    scissorStack_.clear();
    rectBatch_.clear();
    batchValid_ = false;
    activeShader_ = 0;
    softwareFrameActive_ = true;
}

void Renderer::endSoftwareFrame() {
    if (!softwareFrameActive_) {
        return;
    }
    flushRectBatch();
    presentSoftwareFrame();
    softwareFrameActive_ = false;
}

void Renderer::presentSoftwareFrame() {
    if (softwarePixels_.empty() || softwareWidth_ <= 0 || softwareHeight_ <= 0) {
        return;
    }

#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(window_);
    if (!hwnd) {
        return;
    }

    HDC dc = GetDC(hwnd);
    if (!dc) {
        return;
    }

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = softwareWidth_;
    bitmapInfo.bmiHeader.biHeight = -softwareHeight_;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    // The software buffer is always exactly the client size, so present with a
    // straight 1:1 DIB copy (SetDIBitsToDevice) instead of StretchDIBits. The
    // stretch path forces GDI through a scaling blitter even at 100% — the 1:1
    // copy is markedly cheaper and is the common case every frame.
    SetDIBitsToDevice(dc,
                      0, 0,
                      softwareWidth_, softwareHeight_,
                      0, 0,
                      0, softwareHeight_,
                      softwarePixels_.data(),
                      &bitmapInfo,
                      DIB_RGB_COLORS);
    ReleaseDC(hwnd, dc);
#elif defined(__ANDROID__)
    ANativeWindow* nativeWindow = static_cast<ANativeWindow*>(window_);
    if (!nativeWindow) {
        return;
    }

    ANativeWindow_setBuffersGeometry(nativeWindow,
                                     softwareWidth_,
                                     softwareHeight_,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer buffer = {};
    if (ANativeWindow_lock(nativeWindow, &buffer, nullptr) != 0) {
        return;
    }

    int copyW = std::min(softwareWidth_, static_cast<int>(buffer.width));
    int copyH = std::min(softwareHeight_, static_cast<int>(buffer.height));
    for (int y = 0; y < copyH; ++y) {
        auto* dst = static_cast<unsigned char*>(buffer.bits) +
                    static_cast<size_t>(y) * buffer.stride * 4u;
        const uint32_t* src = softwarePixels_.data() +
                              static_cast<size_t>(y) * softwareWidth_;
        for (int x = 0; x < copyW; ++x) {
            uint32_t pixel = src[x];
            dst[x * 4 + 0] = static_cast<unsigned char>((pixel >> 16) & 0xffu);
            dst[x * 4 + 1] = static_cast<unsigned char>((pixel >> 8) & 0xffu);
            dst[x * 4 + 2] = static_cast<unsigned char>(pixel & 0xffu);
            dst[x * 4 + 3] = 0xff;
        }
    }
    ANativeWindow_unlockAndPost(nativeWindow);
#endif
}

Rect Renderer::transformSoftwareRect(const Rect& rect,
                                     float drawScale,
                                     const Vec2& pivot) const {
    Rect out = rect;
    if (drawScale != 1.0f) {
        out.x = pivot.x + (out.x - pivot.x) * drawScale;
        out.y = pivot.y + (out.y - pivot.y) * drawScale;
        out.w *= drawScale;
        out.h *= drawScale;
    }
    return out;
}

void Renderer::flushSoftwareRectBatch() {
    if (rectBatch_.empty()) {
        return;
    }

    Vec2 pivot = batchScalePivot_;
    for (const auto& inst : rectBatch_) {
        Rect rect(inst.rect[0], inst.rect[1], inst.rect[2], inst.rect[3]);
        drawSoftwareRoundedRect(rect,
                                softwareColorFromInstance(inst.color),
                                softwareColorFromInstance(inst.color2),
                                softwareColorFromInstance(inst.borderColor),
                                BorderRadius(inst.radius),
                                inst.borderWidth,
                                inst.opacity,
                                inst.hasGradient > 0.0f,
                                inst.gradientAngle,
                                batchScale_,
                                pivot);
    }
    rectBatch_.clear();
}

void Renderer::drawSoftwareRoundedRect(const Rect& rect,
                                       const Color& color,
                                       const Color& color2,
                                       const Color& borderColor,
                                       const BorderRadius& radius,
                                       float borderWidth,
                                       float opacity,
                                       bool hasGradient,
                                       float gradientAngle,
                                       float drawScale,
                                       const Vec2& pivot) {
    if (!softwareFrameActive_ || softwarePixels_.empty() || opacity <= 0.0f) {
        return;
    }

    Rect drawRect = transformSoftwareRect(rect, drawScale, pivot);
    float left = std::floor(drawRect.x + 0.5f);
    float right = std::floor(drawRect.x + drawRect.w + 0.5f);
    float top = std::floor(drawRect.y + 0.5f);
    float bottom = std::floor(drawRect.y + drawRect.h + 0.5f);
    drawRect.x = left;
    drawRect.y = top;
    drawRect.w = std::max(1.0f, right - left);
    drawRect.h = std::max(1.0f, bottom - top);
    if (drawRect.w <= 0.0f || drawRect.h <= 0.0f) {
        return;
    }

    float scaledRadius = std::max(0.0f, radius.uniform() * drawScale);
    float scaledBorder = std::max(0.0f, borderWidth * drawScale);
    const bool drawFill = color.a > 0.0f;
    const bool drawBorder = scaledBorder > 0.0f && borderColor.a > 0.0f;
    if (!drawFill && !drawBorder) {
        return;
    }

    SoftwareClip clip = softwareClipFor(scissorStack_,
                                        softwareWidth_,
                                        softwareHeight_,
                                        dpiScale_);
    int x0 = std::max(clip.x0, static_cast<int>(std::floor(drawRect.x)));
    int y0 = std::max(clip.y0, static_cast<int>(std::floor(drawRect.y)));
    int x1 = std::min(clip.x1, static_cast<int>(std::ceil(drawRect.x + drawRect.w)));
    int y1 = std::min(clip.y1, static_cast<int>(std::ceil(drawRect.y + drawRect.h)));
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    Rect inner(drawRect.x + scaledBorder,
               drawRect.y + scaledBorder,
               drawRect.w - scaledBorder * 2.0f,
               drawRect.h - scaledBorder * 2.0f);
    float innerRadius = std::max(0.0f, scaledRadius - scaledBorder);

    uint32_t srcR = 0, srcG = 0, srcB = 0;
    float baseAlpha = 0.0f;
    if (drawFill && !hasGradient) {
        srcR = static_cast<uint32_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
        srcG = static_cast<uint32_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
        srcB = static_cast<uint32_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
        baseAlpha = color.a * opacity;
    }

    uint32_t borderR = 0, borderG = 0, borderB = 0;
    float borderAlpha = 0.0f;
    if (drawBorder) {
        borderR = static_cast<uint32_t>(std::clamp(borderColor.r, 0.0f, 1.0f) * 255.0f);
        borderG = static_cast<uint32_t>(std::clamp(borderColor.g, 0.0f, 1.0f) * 255.0f);
        borderB = static_cast<uint32_t>(std::clamp(borderColor.b, 0.0f, 1.0f) * 255.0f);
        borderAlpha = borderColor.a * opacity;
    }

    float safeMargin = std::max(0.5f, scaledRadius);
    float safeX0 = safeMargin;
    float safeX1 = drawRect.w - safeMargin;
    float safeY0 = safeMargin;
    float safeY1 = drawRect.h - safeMargin;

    float innerSafeX0 = scaledBorder + std::max(0.5f, innerRadius);
    float innerSafeX1 = drawRect.w - scaledBorder - std::max(0.5f, innerRadius);
    float innerSafeY0 = scaledBorder + std::max(0.5f, innerRadius);
    float innerSafeY1 = drawRect.h - scaledBorder - std::max(0.5f, innerRadius);

    float gradVx = 0.0f, gradVy = 0.0f;
    float invW = 0.0f, invH = 0.0f;
    if (drawFill && hasGradient) {
        float angle = gradientAngle * 3.14159265358979323846f / 180.0f;
        gradVx = std::cos(angle);
        gradVy = std::sin(angle);
        invW = drawRect.w > 0.0f ? 1.0f / drawRect.w : 0.0f;
        invH = drawRect.h > 0.0f ? 1.0f / drawRect.h : 0.0f;
    }

    for (int y = y0; y < y1; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        float ly = py - drawRect.y;
        size_t rowOffset = static_cast<size_t>(y) * softwareWidth_;
        for (int x = x0; x < x1; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float lx = px - drawRect.x;

            float coverage;
            if (lx >= safeX0 && lx <= safeX1 && ly >= safeY0 && ly <= safeY1) {
                coverage = 1.0f;
            } else {
                coverage = softwareRoundedCoverage(px, py, drawRect, scaledRadius);
                if (coverage <= 0.0f) {
                    continue;
                }
            }

            uint32_t& dst = softwarePixels_[rowOffset + x];

            if (drawFill) {
                if (hasGradient) {
                    float nx = (px - drawRect.x) * invW - 0.5f;
                    float ny = (py - drawRect.y) * invH - 0.5f;
                    float t = std::clamp(nx * gradVx + ny * gradVy + 0.5f, 0.0f, 1.0f);
                    Color fill = Color::lerp(color, color2, t);
                    uint32_t fR = static_cast<uint32_t>(std::clamp(fill.r, 0.0f, 1.0f) * 255.0f);
                    uint32_t fG = static_cast<uint32_t>(std::clamp(fill.g, 0.0f, 1.0f) * 255.0f);
                    uint32_t fB = static_cast<uint32_t>(std::clamp(fill.b, 0.0f, 1.0f) * 255.0f);
                    uint32_t a_int = static_cast<uint32_t>(fill.a * opacity * coverage * 255.0f);
                    softwareBlendPixelFast(dst, fR, fG, fB, a_int);
                } else {
                    uint32_t a_int = static_cast<uint32_t>(baseAlpha * coverage * 255.0f);
                    softwareBlendPixelFast(dst, srcR, srcG, srcB, a_int);
                }
            }

            if (drawBorder) {
                float innerCoverage = 0.0f;
                bool inSafeCenter = (lx >= safeX0 && lx <= safeX1 && ly >= safeY0 && ly <= safeY1);
                if (inSafeCenter && lx >= innerSafeX0 && lx <= innerSafeX1 && ly >= innerSafeY0 && ly <= innerSafeY1) {
                    innerCoverage = 1.0f;
                } else if (inner.w > 0.0f && inner.h > 0.0f) {
                    innerCoverage = softwareRoundedCoverage(px, py, inner, innerRadius);
                }
                float borderCoverage = coverage * (1.0f - innerCoverage);
                if (borderCoverage > 0.0f) {
                    uint32_t a_int = static_cast<uint32_t>(borderAlpha * borderCoverage * 255.0f);
                    softwareBlendPixelFast(dst, borderR, borderG, borderB, a_int);
                }
            }
        }
    }
}

void Renderer::drawSoftwareBoxShadow(const Rect& rect,
                                     const BoxShadow& shadow,
                                     const BorderRadius& radius) {
    if (shadow.color.a <= 0.0f || (shadow.blur <= 0.0f && shadow.spread <= 0.0f)) {
        return;
    }

    flushRectBatch();
    int layers = std::clamp(static_cast<int>(std::ceil(shadow.blur / 6.0f)), 1, 8);
    for (int i = layers; i >= 1; --i) {
        float t = static_cast<float>(i) / static_cast<float>(layers);
        float expand = shadow.spread + shadow.blur * t;
        Rect shadowRect(rect.x + shadow.offsetX - expand + translation_.x,
                        rect.y + shadow.offsetY - expand + translation_.y,
                        rect.w + expand * 2.0f,
                        rect.h + expand * 2.0f);
        Color layerColor = shadow.color.withAlpha(
            shadow.color.a * (1.0f - t * 0.82f) / static_cast<float>(layers));
        Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
        drawSoftwareRoundedRect(shadowRect,
                                layerColor,
                                layerColor,
                                Color(0, 0, 0, 0),
                                BorderRadius(radius.uniform() + expand),
                                0.0f,
                                1.0f,
                                false,
                                0.0f,
                                scale_,
                                pivot);
    }
}

void Renderer::drawSoftwareText(const std::string& text,
                                const Vec2& pos,
                                const Color& color,
                                float fontSize,
                                FontWeight weight,
                                const std::string& fontName,
                                FontStyle style) {
    if (!softwareFrameActive_ || text.empty() || color.a <= 0.0f) {
        return;
    }

    flushRectBatch();

    float renderFontSize = std::max(1.0f, fontSize * std::max(0.01f, scale_));
    const std::string& resolvedFontName = resolveFontName(fontName, weight);
    FontData* fontPtr = getFontForSize(resolvedFontName, renderFontSize);
    if (!fontPtr || !fontPtr->loaded) {
        return;
    }
    FontData& font = *fontPtr;
    if (font.atlasPixels.empty() && !font.sourceData.empty()) {
        buildFontAtlas(font,
                       font.sourceData.data(),
                       static_cast<int>(font.sourceData.size()),
                       renderFontSize);
    }
    if (font.atlasPixels.empty() || font.atlasWidth <= 0 || font.atlasHeight <= 0) {
        return;
    }

    float logicalFontHeight = font.fontSize / std::max(1.0f, dpiScale_);
    float glyphScale = std::abs(renderFontSize - logicalFontHeight) < 1.01f
        ? 1.0f / std::max(1.0f, dpiScale_)
        : renderFontSize / std::max(1.0f, font.fontSize);

    Vec2 drawPos(pos.x + translation_.x, pos.y + translation_.y);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
    if (scale_ != 1.0f) {
        drawPos.x = pivot.x + (drawPos.x - pivot.x) * scale_;
        drawPos.y = pivot.y + (drawPos.y - pivot.y) * scale_;
    }

    float cursorX = std::floor(drawPos.x + 0.5f);
    float baselineY = std::floor(drawPos.y + font.ascent * glyphScale + 0.5f);
    float boldOffset = (weight == FontWeight::Bold && resolvedFontName == fontName)
        ? std::max(0.35f, renderFontSize * 0.018f)
        : 0.0f;
    float italicSkew = style == FontStyle::Normal ? 0.0f : renderFontSize * 0.18f;

    SoftwareClip clip = softwareClipFor(scissorStack_,
                                        softwareWidth_,
                                        softwareHeight_,
                                        dpiScale_);

    uint32_t srcR = static_cast<uint32_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    uint32_t srcG = static_cast<uint32_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    uint32_t srcB = static_cast<uint32_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
    float baseAlpha = color.a;
    bool isFastPath = (italicSkew == 0.0f && std::abs(glyphScale - 1.0f) < 0.001f);
    uint32_t baseAlphaInt = static_cast<uint32_t>(color.a * 255.0f);

    auto drawGlyph = [&](const GlyphInfo& glyph, float originX, float originY) {
        int drawW = std::max(0, static_cast<int>(std::ceil(glyph.width * glyphScale)));
        int drawH = std::max(0, static_cast<int>(std::ceil(glyph.height * glyphScale)));
        if (drawW <= 0 || drawH <= 0) {
            return;
        }

        int atlasX0 = std::clamp(static_cast<int>(std::floor(glyph.x0 * font.atlasWidth)),
                                 0,
                                 font.atlasWidth - 1);
        int atlasY0 = std::clamp(static_cast<int>(std::floor(glyph.y0 * font.atlasHeight)),
                                 0,
                                 font.atlasHeight - 1);
        int atlasW = std::max(1, static_cast<int>(std::round(glyph.width)));
        int atlasH = std::max(1, static_cast<int>(std::round(glyph.height)));

        if (isFastPath) {
            int iOriginX = static_cast<int>(std::floor(originX + 0.5f));
            int iOriginY = static_cast<int>(std::floor(originY + 0.5f));
            int y0 = std::max(clip.y0, iOriginY);
            int y1 = std::min(clip.y1, iOriginY + atlasH);
            int x0_clip = std::max(clip.x0, iOriginX);
            int x1_clip = std::min(clip.x1, iOriginX + atlasW);

            for (int y = y0; y < y1; ++y) {
                int iy = y - iOriginY;
                int clamp_ay = std::clamp(atlasY0 + iy, 0, font.atlasHeight - 1);
                size_t atlasRowOffset = static_cast<size_t>(clamp_ay) * font.atlasWidth;
                size_t rowOffset = static_cast<size_t>(y) * softwareWidth_;
                for (int x = x0_clip; x < x1_clip; ++x) {
                    int ix = x - iOriginX;
                    int clamp_ax = std::clamp(atlasX0 + ix, 0, font.atlasWidth - 1);
                    uint8_t alphaVal = font.atlasPixels[atlasRowOffset + clamp_ax];
                    if (alphaVal == 0) {
                        continue;
                    }
                    uint32_t a_int = (static_cast<uint32_t>(alphaVal) * baseAlphaInt) / 255u;
                    if (a_int > 0) {
                        softwareBlendPixelFast(softwarePixels_[rowOffset + x], srcR, srcG, srcB, a_int);
                    }
                }
            }
        } else {
            int y0 = std::max(clip.y0, static_cast<int>(std::floor(originY)));
            int y1 = std::min(clip.y1, static_cast<int>(std::ceil(originY + drawH)));
            for (int y = y0; y < y1; ++y) {
                float localY = static_cast<float>(y) + 0.5f - originY;
                float rowSkew = italicSkew * (1.0f - localY / std::max(1.0f, static_cast<float>(drawH)));
                int x0 = std::max(clip.x0, static_cast<int>(std::floor(originX + rowSkew)));
                int x1 = std::min(clip.x1, static_cast<int>(std::ceil(originX + rowSkew + drawW)));
                size_t rowOffset = static_cast<size_t>(y) * softwareWidth_;
                for (int x = x0; x < x1; ++x) {
                    float localX = static_cast<float>(x) + 0.5f - originX - rowSkew;
                    float alpha = softwareSampleFontAlpha(font,
                                                          atlasX0,
                                                          atlasY0,
                                                          atlasW,
                                                          atlasH,
                                                          localX / glyphScale - 0.5f,
                                                          localY / glyphScale - 0.5f);
                    if (alpha <= 0.001f) {
                        continue;
                    }
                    uint32_t a_int = static_cast<uint32_t>(baseAlpha * alpha * 255.0f);
                    softwareBlendPixelFast(softwarePixels_[rowOffset + x], srcR, srcG, srcB, a_int);
                }
            }
        }
    };

    ShapedRun run = shapeTextWithHarfbuzz(font, text, Direction::Ltr);
    for (const auto& sg : run) {
        const GlyphInfo& glyph = (font.hbFont) ? font.getGlyphByIndex(sg.glyphIndex) : font.getGlyph(sg.codepoint);
        if (glyph.xadvance == 0.0f && sg.codepoint != ' ') {
            continue;
        }

        float x = std::floor(cursorX + (glyph.xoff + sg.xOffset) * glyphScale + 0.5f);
        float y = std::floor(baselineY + (glyph.yoff + sg.yOffset) * glyphScale + 0.5f);
        drawGlyph(glyph, x, y);
        if (boldOffset > 0.0f) {
            drawGlyph(glyph, x + boldOffset, y);
        }
        cursorX += sg.xAdvance * glyphScale;
    }
}

void Renderer::drawSoftwareImage(const std::string& key,
                                 ImageData& image,
                                 const Rect& rect,
                                 const Rect& sourceUv,
                                 const Color& tint,
                                 float opacity) {
    (void)key;
    if (!softwareFrameActive_ || !image.loaded || image.pixels.empty() ||
        image.width <= 0 || image.height <= 0 || opacity <= 0.0f ||
        tint.a <= 0.0f) {
        return;
    }

    flushRectBatch();

    Rect drawRect(rect.x + translation_.x, rect.y + translation_.y, rect.w, rect.h);
    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
    drawRect = transformSoftwareRect(drawRect, scale_, pivot);
    if (drawRect.w <= 0.0f || drawRect.h <= 0.0f) {
        return;
    }

    SoftwareClip clip = softwareClipFor(scissorStack_,
                                        softwareWidth_,
                                        softwareHeight_,
                                        dpiScale_);
    int x0 = std::max(clip.x0, static_cast<int>(std::floor(drawRect.x)));
    int y0 = std::max(clip.y0, static_cast<int>(std::floor(drawRect.y)));
    int x1 = std::min(clip.x1, static_cast<int>(std::ceil(drawRect.x + drawRect.w)));
    int y1 = std::min(clip.y1, static_cast<int>(std::ceil(drawRect.y + drawRect.h)));
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    float u0 = std::clamp(sourceUv.x, 0.0f, 1.0f);
    float v0 = std::clamp(sourceUv.y, 0.0f, 1.0f);
    float u1 = std::clamp(sourceUv.x + sourceUv.w, 0.0f, 1.0f);
    float v1 = std::clamp(sourceUv.y + sourceUv.h, 0.0f, 1.0f);

    uint32_t tintR = static_cast<uint32_t>(std::clamp(tint.r, 0.0f, 1.0f) * 255.0f);
    uint32_t tintG = static_cast<uint32_t>(std::clamp(tint.g, 0.0f, 1.0f) * 255.0f);
    uint32_t tintB = static_cast<uint32_t>(std::clamp(tint.b, 0.0f, 1.0f) * 255.0f);
    uint32_t tintA = static_cast<uint32_t>(std::clamp(tint.a, 0.0f, 1.0f) * 255.0f);
    uint32_t opacityInt = static_cast<uint32_t>(std::clamp(opacity, 0.0f, 1.0f) * 255.0f);

    float u_step = (u1 - u0) / drawRect.w;
    float u_start = u0 + (static_cast<float>(x0) + 0.5f - drawRect.x) * u_step;

    float v_step = (v1 - v0) / drawRect.h;
    float v_start = v0 + (static_cast<float>(y0) + 0.5f - drawRect.y) * v_step;

    float imgWidthFloat = static_cast<float>(image.width);
    float imgHeightFloat = static_cast<float>(image.height);

    for (int y = y0; y < y1; ++y) {
        float v_val = v_start + static_cast<float>(y - y0) * v_step;
        int sy = std::clamp(static_cast<int>(v_val * imgHeightFloat),
                            0,
                            image.height - 1);
        size_t rowOffset = static_cast<size_t>(y) * softwareWidth_;
        size_t syOffset = static_cast<size_t>(sy) * image.width;
        for (int x = x0; x < x1; ++x) {
            float u_val = u_start + static_cast<float>(x - x0) * u_step;
            int sx = std::clamp(static_cast<int>(u_val * imgWidthFloat),
                                0,
                                image.width - 1);
            size_t index = (syOffset + sx) * 4u;
            uint32_t imgR = image.pixels[index + 0];
            uint32_t imgG = image.pixels[index + 1];
            uint32_t imgB = image.pixels[index + 2];
            uint32_t imgA = image.pixels[index + 3];

            uint32_t srcR = (imgR * tintR) / 255u;
            uint32_t srcG = (imgG * tintG) / 255u;
            uint32_t srcB = (imgB * tintB) / 255u;
            uint32_t srcA = (imgA * tintA) / 255u;

            uint32_t finalAlpha = (srcA * opacityInt) / 255u;

            softwareBlendPixelFast(softwarePixels_[rowOffset + x], srcR, srcG, srcB, finalAlpha);
        }
    }
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
    float left = snapPx(drawRect.x * dpiScale_);
    float right = snapPx((drawRect.x + drawRect.w) * dpiScale_);
    float top = snapPx(drawRect.y * dpiScale_);
    float bottom = snapPx((drawRect.y + drawRect.h) * dpiScale_);
    instance.rect[0] = left;
    instance.rect[1] = top;
    instance.rect[2] = std::max(1.0f, right - left);
    instance.rect[3] = std::max(1.0f, bottom - top);
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
                              const std::string& fontName,
                              FontStyle style) {
#if FLUXUI_HAS_VULKAN_SDK
    if (!vulkan_ || !vulkan_->frameActive || !vulkan_->textPipeline || text.empty()) {
        return;
    }

    const std::string& resolvedFontName = resolveFontName(fontName, weight);
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
    float italicSkew = style == FontStyle::Normal ? 0.0f : fontSize * 0.18f;

    auto appendGlyph = [&](float x, float y, float w, float h, const GlyphInfo& g) {
        float topSkew = italicSkew * dpiScale_;
        float xp = x * dpiScale_;
        float yp = y * dpiScale_;
        float wp = w * dpiScale_;
        float hp = h * dpiScale_;
        float data[] = {
            xp+topSkew,    yp,    g.x0, g.y0, color.r, color.g, color.b, color.a,
            xp+wp+topSkew, yp,    g.x1, g.y0, color.r, color.g, color.b, color.a,
            xp+wp,         yp+hp, g.x1, g.y1, color.r, color.g, color.b, color.a,
            xp+topSkew,    yp,    g.x0, g.y0, color.r, color.g, color.b, color.a,
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

    ShapedRun run = shapeTextWithHarfbuzz(font, text, Direction::Ltr);
    if (text.find("$291.68") != std::string::npos || text.find("291") != std::string::npos) {
        std::cout << "[DEBUG drawVulkanText] shaping text=\"" << text << "\" fontScale=" << fontScale << " hbFont=" << (font.hbFont != nullptr) << " runSize=" << run.size() << std::endl;
    }
    for (const auto& sg : run) {
        const auto& g = (font.hbFont) ? font.getGlyphByIndex(sg.glyphIndex) : font.getGlyph(sg.codepoint);
        if (text.find("$291.68") != std::string::npos || text.find("291") != std::string::npos) {
            std::cout << "  Glyph: codepoint=" << sg.codepoint << " glyphIndex=" << sg.glyphIndex 
                      << " width=" << g.width << " height=" << g.height << " xadvance=" << g.xadvance << std::endl;
        }
        if (g.xadvance == 0 && sg.codepoint != ' ') continue;

        float w = g.width * fontScale;
        float h = g.height * fontScale;
        if (w > 0 && h > 0) {
            float x = snap(cursorX + (g.xoff + sg.xOffset) * fontScale);
            float y = snap(baselineY + (g.yoff + sg.yOffset) * fontScale);
            appendGlyph(x, y, w, h, g);
            if (boldOffset > 0.0f) {
                appendGlyph(snap(x + boldOffset), y, w, h, g);
            }
        }
        cursorX += sg.xAdvance * fontScale;
    }

    if (text.find("$291.68") != std::string::npos || text.find("291") != std::string::npos) {
        std::cout << "  Vertices count: " << (vertices.size() / 8) << std::endl;
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
    (void)style;
#endif
}

void Renderer::drawVulkanImage(const std::string& key,
                               ImageData& image,
                               const Rect& rect,
                               const Rect& sourceUv,
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
    float u0 = std::clamp(sourceUv.x, 0.0f, 1.0f);
    float v0 = std::clamp(sourceUv.y, 0.0f, 1.0f);
    float u1 = std::clamp(sourceUv.x + sourceUv.w, 0.0f, 1.0f);
    float v1 = std::clamp(sourceUv.y + sourceUv.h, 0.0f, 1.0f);
    float data[] = {
        x,     y,     u0, v0, tint.r, tint.g, tint.b, a,
        x + w, y,     u1, v0, tint.r, tint.g, tint.b, a,
        x + w, y + h, u1, v1, tint.r, tint.g, tint.b, a,
        x,     y,     u0, v0, tint.r, tint.g, tint.b, a,
        x + w, y + h, u1, v1, tint.r, tint.g, tint.b, a,
        x,     y + h, u0, v1, tint.r, tint.g, tint.b, a,
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
    (void)sourceUv;
    (void)tint;
    (void)opacity;
#endif
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
        
        if (weight == FontWeight::Bold) {
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
            if (weight == FontWeight::Bold) {
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

    if (weight != FontWeight::Bold) {
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

void Renderer::ensureHarfbuzzFont(const FontData& font) const {
    if (font.hbFont) return;

    if (!font.ftLibrary) {
        FT_Library library;
        if (FT_Init_FreeType(&library) == 0) {
            font.ftLibrary = library;
        }
    }

    if (!font.ftLibrary) return;

    if (!font.ftFace && !font.sourceData.empty()) {
        FT_Face face;
        if (FT_New_Memory_Face((FT_Library)font.ftLibrary, font.sourceData.data(), (long)font.sourceData.size(), 0, &face) == 0) {
            float bakedSize = std::max(8.0f, font.fontSize);
            float dpiScale = std::max(1.0f, dpiScale_);
            float pixelSize = (float)std::max(8, (int)std::round(bakedSize * dpiScale));
            FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixelSize);
            font.ftFace = face;
        }
    }

    if (!font.ftFace) return;

    FT_Face face = (FT_Face)font.ftFace;
    hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
    if (hbFont) {
        hb_ft_font_set_funcs(hbFont);
        font.hbFont = hbFont;
    }
}

Renderer::ShapedRun Renderer::shapeTextWithHarfbuzz(const FontData& font, const std::string& text, Direction direction) const {
    ShapedRun run;
    if (text.empty()) return run;

    ensureHarfbuzzFont(font);
    if (!font.hbFont) {
        // Fallback to simple manual codepoint-by-codepoint shaping if hbFont is not available
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

        std::string processedText = substituteLigatures(text, font);
        uint32_t prevCp = 0;
        for (size_t i = 0; i < processedText.size(); ) {
            uint32_t c = getNextCodepoint(processedText, i);
            const auto& g = font.getGlyph(c);
            if (g.xadvance == 0 && c != ' ') continue;

            float kern = 0.0f;
            if (prevCp != 0) {
                kern = font.getKerning(prevCp, c, 1.0f);
            }

            ShapedGlyph sg;
            sg.glyphIndex = c;
            sg.codepoint = c;
            sg.xOffset = kern;
            sg.yOffset = 0.0f;
            sg.xAdvance = g.xadvance;
            run.push_back(sg);
            prevCp = c;
        }
        return run;
    }

    // HarfBuzz path!
    hb_font_t* hbFont = (hb_font_t*)font.hbFont;
    hb_buffer_t* buf = hb_buffer_create();

    hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_buffer_set_direction(buf, direction == Direction::Rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);

    // Explicitly enable ligatures and contextual kerning for premium rendering!
    hb_feature_t features[5];
    features[0].tag = HB_TAG('l', 'i', 'g', 'a'); features[0].value = 1; features[0].start = HB_FEATURE_GLOBAL_START; features[0].end = HB_FEATURE_GLOBAL_END;
    features[1].tag = HB_TAG('c', 'l', 'i', 'g'); features[1].value = 1; features[1].start = HB_FEATURE_GLOBAL_START; features[1].end = HB_FEATURE_GLOBAL_END;
    features[2].tag = HB_TAG('d', 'l', 'i', 'g'); features[2].value = 1; features[2].start = HB_FEATURE_GLOBAL_START; features[2].end = HB_FEATURE_GLOBAL_END;
    features[3].tag = HB_TAG('h', 'l', 'i', 'g'); features[3].value = 1; features[3].start = HB_FEATURE_GLOBAL_START; features[3].end = HB_FEATURE_GLOBAL_END;
    features[4].tag = HB_TAG('k', 'e', 'r', 'n'); features[4].value = 1; features[4].start = HB_FEATURE_GLOBAL_START; features[4].end = HB_FEATURE_GLOBAL_END;

    hb_shape(hbFont, buf, features, 5);

    unsigned int glyphCount = 0;
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

    run.reserve(glyphCount);
    for (unsigned int i = 0; i < glyphCount; ++i) {
        ShapedGlyph sg;
        sg.glyphIndex = glyphInfo[i].codepoint;
        sg.codepoint = glyphInfo[i].codepoint;

        // hb-ft uses 26.6 fractional coordinates by default (1/64 of a pixel)
        sg.xOffset = (float)glyphPos[i].x_offset / 64.0f;
        sg.yOffset = (float)glyphPos[i].y_offset / 64.0f;
        sg.xAdvance = (float)glyphPos[i].x_advance / 64.0f;

        run.push_back(sg);
    }

    hb_buffer_destroy(buf);
    return run;
}

static std::string substituteLigatures(const std::string& text, const FontData& font) {
    if (font.supportedLigatures.empty()) return text;
    std::string result = text;
    for (const auto& lig : font.supportedLigatures) {
        size_t pos = 0;
        while ((pos = result.find(lig.first, pos)) != std::string::npos) {
            std::string utf8Lig;
            uint32_t cp = lig.second;
            if (cp < 0x80) {
                utf8Lig.push_back((char)cp);
            } else if (cp < 0x800) {
                utf8Lig.push_back((char)(0xC0 | (cp >> 6)));
                utf8Lig.push_back((char)(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                utf8Lig.push_back((char)(0xE0 | (cp >> 12)));
                utf8Lig.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                utf8Lig.push_back((char)(0x80 | (cp & 0x3F)));
            } else {
                utf8Lig.push_back((char)(0xF0 | (cp >> 18)));
                utf8Lig.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
                utf8Lig.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                utf8Lig.push_back((char)(0x80 | (cp & 0x3F)));
            }
            result.replace(pos, lig.first.length(), utf8Lig);
            pos += utf8Lig.length();
        }
    }
    return result;
}

static bool isRtlCodepoint(uint32_t cp) {
    return (cp >= 0x0590 && cp <= 0x08FF) || 
           (cp >= 0xFB50 && cp <= 0xFDFF) || 
           (cp >= 0xFE70 && cp <= 0xFEFF);
}

static bool isLtrCodepoint(uint32_t cp) {
    if (isRtlCodepoint(cp)) return false;
    return (cp >= 0x0041 && cp <= 0x005A) || // A-Z
           (cp >= 0x0061 && cp <= 0x007A) || // a-z
           (cp >= 0x0030 && cp <= 0x0039) || // 0-9
           (cp >= 0x00C0 && cp <= 0x024F) || // Latin Extended
           (cp >= 0x0370 && cp <= 0x03FF) || // Greek
           (cp >= 0x0400 && cp <= 0x04FF);   // Cyrillic
}

static uint32_t mirrorCodepoint(uint32_t cp) {
    switch (cp) {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        case '<': return '>';
        case '>': return '<';
        default: return cp;
    }
}

static std::string reorderBidiText(const std::string& text, Direction direction, UnicodeBidi bidi) {
    if (text.empty()) return text;

    std::vector<uint32_t> codepoints;
    codepoints.reserve(text.size());
    size_t idx = 0;
    while (idx < text.size()) {
        unsigned char c = (unsigned char)text[idx];
        uint32_t cp = c;
        if (c < 0x80) {
            cp = text[idx++];
        } else if ((c & 0xE0) == 0xC0 && idx + 1 < text.size()) {
            cp = ((text[idx] & 0x1F) << 6) | (text[idx + 1] & 0x3F);
            idx += 2;
        } else if ((c & 0xF0) == 0xE0 && idx + 2 < text.size()) {
            cp = ((text[idx] & 0x0F) << 12) | ((text[idx + 1] & 0x3F) << 6) | (text[idx + 2] & 0x3F);
            idx += 3;
        } else if ((c & 0xF8) == 0xF0 && idx + 3 < text.size()) {
            cp = ((text[idx] & 0x07) << 18) | ((text[idx + 1] & 0x3F) << 12) | ((text[idx + 2] & 0x3F) << 6) | (text[idx + 3] & 0x3F);
            idx += 4;
        } else {
            cp = text[idx++];
        }
        codepoints.push_back(cp);
    }

    std::vector<int> resolvedDirs(codepoints.size(), 0);
    int baseDir = (direction == Direction::Rtl) ? 1 : 0;

    if (bidi == UnicodeBidi::BidiOverride || bidi == UnicodeBidi::IsolateOverride) {
        for (size_t i = 0; i < codepoints.size(); ++i) {
            resolvedDirs[i] = baseDir;
        }
    } else {
        std::vector<int> types(codepoints.size(), -1);
        bool hasRtl = false;
        for (size_t i = 0; i < codepoints.size(); ++i) {
            if (isRtlCodepoint(codepoints[i])) {
                types[i] = 1;
                hasRtl = true;
            } else if (isLtrCodepoint(codepoints[i])) {
                types[i] = 0;
            }
        }

        if (baseDir == 0 && !hasRtl) {
            return text;
        }

        int lastStrong = baseDir;
        for (size_t i = 0; i < codepoints.size(); ++i) {
            if (types[i] != -1) {
                lastStrong = types[i];
            }
            resolvedDirs[i] = lastStrong;
        }
    }

    struct Run {
        int dir;
        size_t start;
        size_t length;
    };
    std::vector<Run> runs;
    if (!resolvedDirs.empty()) {
        int currentDir = resolvedDirs[0];
        size_t start = 0;
        for (size_t i = 1; i < resolvedDirs.size(); ++i) {
            if (resolvedDirs[i] != currentDir) {
                runs.push_back({currentDir, start, i - start});
                currentDir = resolvedDirs[i];
                start = i;
            }
        }
        runs.push_back({currentDir, start, resolvedDirs.size() - start});
    }

    if (baseDir == 1) {
        std::reverse(runs.begin(), runs.end());
    }

    std::vector<uint32_t> visualCodepoints;
    visualCodepoints.reserve(codepoints.size());

    for (const auto& run : runs) {
        if (run.dir == 1) {
            for (size_t i = 0; i < run.length; ++i) {
                size_t srcIdx = run.start + run.length - 1 - i;
                visualCodepoints.push_back(mirrorCodepoint(codepoints[srcIdx]));
            }
        } else {
            for (size_t i = 0; i < run.length; ++i) {
                visualCodepoints.push_back(codepoints[run.start + i]);
            }
        }
    }

    std::string result;
    result.reserve(text.size());
    for (uint32_t cp : visualCodepoints) {
        if (cp < 0x80) {
            result.push_back((char)cp);
        } else if (cp < 0x800) {
            result.push_back((char)(0xC0 | (cp >> 6)));
            result.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            result.push_back((char)(0xE0 | (cp >> 12)));
            result.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp < 0x110000) {
            result.push_back((char)(0xF0 | (cp >> 18)));
            result.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    return result;
}

void Renderer::drawText(const std::string& text, const Vec2& pos, const Color& color,
                         float fontSize, FontWeight weight, const std::string& fontName,
                         FontStyle style,
                         Direction direction,
                         UnicodeBidi unicodeBidi) {
    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Text;
        cmd.rect = Rect(pos.x + (translation_.x - recordingTranslationStart_.x),
                        pos.y + (translation_.y - recordingTranslationStart_.y),
                        0.0f, 0.0f);
        cmd.text = text;
        cmd.color = color;
        cmd.fontSize = fontSize;
        cmd.fontWeight = weight;
        cmd.fontName = fontName;
        cmd.fontStyle = style;
        cmd.fontDirection = direction;
        cmd.unicodeBidi = unicodeBidi;
        recordCommand(std::move(cmd));
        return;
    }
    if (text.find("$291.68") != std::string::npos || text.find("291") != std::string::npos) {
        std::cout << "[DEBUG drawText] text=\"" << text 
                  << "\" activeBackend=" << (int)activeBackend_ 
                  << " pos=[" << pos.x << ", " << pos.y << "]"
                  << " color=[" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << "]" << std::endl;
    }

    std::string processedText = reorderBidiText(text, direction, unicodeBidi);

    if (activeBackend_ == RenderBackendType::Vulkan) {
        if (text.find("$291.68") != std::string::npos || text.find("291") != std::string::npos) {
            std::cout << "[DEBUG drawText] routing to drawVulkanText" << std::endl;
        }
        drawVulkanText(processedText, pos, color, fontSize, weight, fontName, style);
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        if (text.find("$291.68") != std::string::npos || text.find("291") != std::string::npos) {
            std::cout << "[DEBUG drawText] routing to drawSoftwareText" << std::endl;
        }
        drawSoftwareText(processedText, pos, color, fontSize, weight, fontName, style);
        return;
    }

    const std::string& resolvedFontName = resolveFontName(fontName, weight);
    FontData* fontPtr = getFontForSize(resolvedFontName, fontSize);
    if (!fontPtr || !fontPtr->loaded) return;
    if (!ensureFontTexture(*fontPtr)) return;
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
    float italicSkew = style == FontStyle::Normal ? 0.0f : fontSize * 0.18f;

    ShapedRun run = shapeTextWithHarfbuzz(font, processedText, direction);
    for (const auto& sg : run) {
        const auto& g = (font.hbFont) ? font.getGlyphByIndex(sg.glyphIndex) : font.getGlyph(sg.codepoint);
        if (g.xadvance == 0 && sg.codepoint != ' ') continue;

        float w = g.width * scale;
        float h = g.height * scale;

        if (w > 0 && h > 0) {
            float x = snap(cursorX + (g.xoff + sg.xOffset) * scale);
            float y = snap(baselineY + (g.yoff + sg.yOffset) * scale);
            float topSkew = snap(italicSkew);

            float data[] = {
                x+topSkew,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                x+w+topSkew, y,   g.x1, g.y0, color.r, color.g, color.b, color.a,
                x+w,         y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                x+topSkew,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                x+w, y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                x,   y+h, g.x0, g.y1, color.r, color.g, color.b, color.a,
            };
            vertices.insert(vertices.end(), std::begin(data), std::end(data));

            if (boldOffset > 0.0f) {
                float xb = snap(x + boldOffset);
                float boldData[] = {
                    xb+topSkew,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                    xb+w+topSkew, y,   g.x1, g.y0, color.r, color.g, color.b, color.a,
                    xb+w,         y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                    xb+topSkew,   y,   g.x0, g.y0, color.r, color.g, color.b, color.a,
                    xb+w, y+h, g.x1, g.y1, color.r, color.g, color.b, color.a,
                    xb,   y+h, g.x0, g.y1, color.r, color.g, color.b, color.a,
                };
                vertices.insert(vertices.end(), std::begin(boldData), std::end(boldData));
            }
        }
        cursorX += sg.xAdvance * scale;
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
void Renderer::applySoftwareBackdropBlur(const Rect& rect, float blurRadius, const BorderRadius& radius) {
    if (!softwareFrameActive_ || softwarePixels_.empty() || blurRadius <= 0.0f) {
        return;
    }

    // Flush any pending rect batches to ensure the backbuffer is fully updated
    flushRectBatch();

    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
    Rect drawRect = rect;
    drawRect.x += translation_.x;
    drawRect.y += translation_.y;
    drawRect = transformSoftwareRect(drawRect, scale_, pivot);

    int x0 = std::max(0, (int)std::floor(drawRect.x));
    int y0 = std::max(0, (int)std::floor(drawRect.y));
    int x1 = std::min(softwareWidth_, (int)std::ceil(drawRect.x + drawRect.w));
    int y1 = std::min(softwareHeight_, (int)std::ceil(drawRect.y + drawRect.h));
    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0) return;

    // 1. Copy the sub-rectangle pixels from softwarePixels_ without heap thrashing
    size_t reqSize = static_cast<size_t>(w) * h;
    if (softwareBlurBuffer1_.size() < reqSize) {
        softwareBlurBuffer1_.resize(reqSize);
    }
    if (softwareBlurBuffer2_.size() < reqSize) {
        softwareBlurBuffer2_.resize(reqSize);
    }
    if (softwareBlurBuffer3_.size() < reqSize) {
        softwareBlurBuffer3_.resize(reqSize);
    }

    for (int y = 0; y < h; ++y) {
        std::memcpy(&softwareBlurBuffer1_[y * w], &softwarePixels_[static_cast<size_t>(y0 + y) * softwareWidth_ + x0], w * sizeof(uint32_t));
    }

    // 2. Generate 1D Gaussian kernel on the stack
    float sigma = std::max(0.5f, blurRadius / 2.0f);
    int kSize = (int)(ceil(blurRadius * 3.0f));
    if (kSize < 1) kSize = 1;
    if (kSize > 50) kSize = 50;

    float kernel[51];
    float sum = 0.0f;
    for (int i = 0; i <= kSize; ++i) {
        float xVal = (float)i;
        kernel[i] = std::exp(-(xVal * xVal) / (2.0f * sigma * sigma));
        if (i == 0) sum += kernel[i];
        else sum += 2.0f * kernel[i];
    }
    for (int i = 0; i <= kSize; ++i) {
        kernel[i] /= sum;
    }

    // 3. Horizontal Pass
    for (int y = 0; y < h; ++y) {
        size_t rowStart = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            float rSum = 0.0f, gSum = 0.0f, bSum = 0.0f;
            for (int k = -kSize; k <= kSize; ++k) {
                int px = std::clamp(x + k, 0, w - 1);
                uint32_t pixel = softwareBlurBuffer1_[rowStart + px];
                float weight = kernel[std::abs(k)];

                rSum += static_cast<float>((pixel >> 16) & 0xffu) * weight;
                gSum += static_cast<float>((pixel >> 8) & 0xffu) * weight;
                bSum += static_cast<float>(pixel & 0xffu) * weight;
            }
            softwareBlurBuffer2_[rowStart + x] = 0xff000000u |
                (static_cast<uint32_t>(std::clamp(static_cast<int>(std::round(rSum)), 0, 255)) << 16) |
                (static_cast<uint32_t>(std::clamp(static_cast<int>(std::round(gSum)), 0, 255)) << 8) |
                static_cast<uint32_t>(std::clamp(static_cast<int>(std::round(bSum)), 0, 255));
        }
    }

    // 4. Vertical Pass
    for (int y = 0; y < h; ++y) {
        size_t rowStart = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            float rSum = 0.0f, gSum = 0.0f, bSum = 0.0f;
            for (int k = -kSize; k <= kSize; ++k) {
                int py = std::clamp(y + k, 0, h - 1);
                uint32_t pixel = softwareBlurBuffer2_[static_cast<size_t>(py) * w + x];
                float weight = kernel[std::abs(k)];

                rSum += static_cast<float>((pixel >> 16) & 0xffu) * weight;
                gSum += static_cast<float>((pixel >> 8) & 0xffu) * weight;
                bSum += static_cast<float>(pixel & 0xffu) * weight;
            }
            softwareBlurBuffer3_[rowStart + x] = 0xff000000u |
                (static_cast<uint32_t>(std::clamp(static_cast<int>(std::round(rSum)), 0, 255)) << 16) |
                (static_cast<uint32_t>(std::clamp(static_cast<int>(std::round(gSum)), 0, 255)) << 8) |
                static_cast<uint32_t>(std::clamp(static_cast<int>(std::round(bSum)), 0, 255));
        }
    }

    // 5. Blending & Write-back
    float scaledRadius = radius.maxRadius() * scale_;
    float safeMargin = std::max(0.5f, scaledRadius);
    float safeX0 = drawRect.x + safeMargin;
    float safeX1 = drawRect.x + drawRect.w - safeMargin;
    float safeY0 = drawRect.y + safeMargin;
    float safeY1 = drawRect.y + drawRect.h - safeMargin;

    for (int y = 0; y < h; ++y) {
        int dstY = y0 + y;
        float py = (float)dstY;
        size_t dstRowOffset = static_cast<size_t>(dstY) * softwareWidth_;
        size_t srcRowOffset = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            int dstX = x0 + x;
            float px = (float)dstX;

            float coverage = 1.0f;
            if (scaledRadius > 0.05f) {
                if (px >= safeX0 && px <= safeX1 && py >= safeY0 && py <= safeY1) {
                    coverage = 1.0f;
                } else {
                    coverage = softwareRoundedCoverage(px, py, drawRect, scaledRadius);
                    if (coverage <= 0.0f) continue;
                }
            }

            size_t dstIdx = dstRowOffset + dstX;
            uint32_t blurredPixel = softwareBlurBuffer3_[srcRowOffset + x];

            if (coverage >= 0.999f) {
                softwarePixels_[dstIdx] = blurredPixel;
            } else {
                uint32_t bgPixel = softwarePixels_[dstIdx];
                uint32_t bgR = (bgPixel >> 16) & 0xffu;
                uint32_t bgG = (bgPixel >> 8) & 0xffu;
                uint32_t bgB = bgPixel & 0xffu;

                uint32_t fgR = (blurredPixel >> 16) & 0xffu;
                uint32_t fgG = (blurredPixel >> 8) & 0xffu;
                uint32_t fgB = blurredPixel & 0xffu;

                uint32_t covInt = static_cast<uint32_t>(coverage * 255.0f);
                uint32_t outR = (fgR * covInt + bgR * (255u - covInt)) / 255u;
                uint32_t outG = (fgG * covInt + bgG * (255u - covInt)) / 255u;
                uint32_t outB = (fgB * covInt + bgB * (255u - covInt)) / 255u;

                softwarePixels_[dstIdx] = 0xff000000u | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

void Renderer::drawBackdropFilterBlur(const Rect& rect, float blurRadius, const BorderRadius& radius) {
    if (blurRadius <= 0.0f) return;

    if (isRecording()) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::BackdropFilterBlur;
        cmd.rect = rect;
        cmd.rect.x += (translation_.x - recordingTranslationStart_.x);
        cmd.rect.y += (translation_.y - recordingTranslationStart_.y);
        cmd.blurRadius = blurRadius;
        cmd.radius = radius;
        recordCommand(std::move(cmd));
        return;
    }

    if (activeBackend_ == RenderBackendType::Compatibility) {
        applySoftwareBackdropBlur(rect, blurRadius, radius);
        return;
    }

    if (activeBackend_ == RenderBackendType::Vulkan) {
        return;
    }

    // OpenGL FBO support (as requested by the user!)
    flushRectBatch();
    
    // Ensure shaders are compiled
    if (blurShaderHoriz_ == 0) {
        const char* BLUR_VERT = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec4 uRect;       // x, y, w, h
uniform mat4 uProjection;
out vec2 vTexCoord;
void main() {
    vec2 pos = uRect.xy + aPos * uRect.zw;
    vTexCoord = aPos;
    gl_Position = uProjection * vec4(pos, 0.0, 1.0);
}
)";

        const char* BLUR_HORIZ_FRAG = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform float uBlurRadius;
uniform float uWidth;
void main() {
    float sigma = max(0.5, uBlurRadius / 2.0);
    float sum = 0.0;
    vec4 result = vec4(0.0);
    int kSize = int(ceil(uBlurRadius * 3.0));
    kSize = clamp(kSize, 1, 20);
    for (int i = -kSize; i <= kSize; ++i) {
        float weight = exp(-float(i*i) / (2.0 * sigma * sigma));
        result += texture(uTexture, vTexCoord + vec2(float(i) / uWidth, 0.0)) * weight;
        sum += weight;
    }
    FragColor = result / sum;
}
)";

        const char* BLUR_VERT_FRAG = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform float uBlurRadius;
uniform float uHeight;
uniform vec2 uSize;
uniform float uRadius;

float roundedBoxSDF(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    float sigma = max(0.5, uBlurRadius / 2.0);
    float sum = 0.0;
    vec4 result = vec4(0.0);
    int kSize = int(ceil(uBlurRadius * 3.0));
    kSize = clamp(kSize, 1, 20);
    for (int i = -kSize; i <= kSize; ++i) {
        float weight = exp(-float(i*i) / (2.0 * sigma * sigma));
        result += texture(uTexture, vTexCoord + vec2(0.0, float(i) / uHeight)) * weight;
        sum += weight;
    }
    
    // Apply rounded corners clip
    vec2 halfSize = uSize * 0.5;
    vec2 center = vTexCoord * uSize - halfSize;
    float r = min(uRadius, min(halfSize.x, halfSize.y));
    float dist = roundedBoxSDF(center, halfSize, r);
    float aa = max(fwidth(dist), 0.55);
    float alpha = 1.0 - smoothstep(-aa, aa, dist);
    
    vec4 finalColor = result / sum;
    FragColor = vec4(finalColor.rgb, finalColor.a * alpha);
}
)";

        blurShaderHoriz_ = compileShader(BLUR_VERT, BLUR_HORIZ_FRAG);
        blurShaderVert_ = compileShader(BLUR_VERT, BLUR_VERT_FRAG);
    }

    Rect drawRect = rect;
    drawRect.x += translation_.x;
    drawRect.y += translation_.y;
    if (scale_ != 1.0f) {
        Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
        drawRect.x = pivot.x + (drawRect.x - pivot.x) * scale_;
        drawRect.y = pivot.y + (drawRect.y - pivot.y) * scale_;
        drawRect.w *= scale_;
        drawRect.h *= scale_;
    }
    
    int viewportW = (int)(windowWidth_ * dpiScale_);
    int viewportH = (int)(windowHeight_ * dpiScale_);
    if (viewportW <= 0 || viewportH <= 0) return;

    // Lazy allocation matching the viewport size to prevent resizing thrash
    static int allocatedW = 0;
    static int allocatedH = 0;
    if (blurTex_ == 0 || allocatedW != viewportW || allocatedH != viewportH) {
        if (blurTex_) glDeleteTextures(1, &blurTex_);
        if (blurTexTemp_) glDeleteTextures(1, &blurTexTemp_);
        if (blurFBO_ == 0) glGenFramebuffers(1, &blurFBO_);

        glGenTextures(1, &blurTex_);
        glBindTexture(GL_TEXTURE_2D, blurTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, viewportW, viewportH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &blurTexTemp_);
        glBindTexture(GL_TEXTURE_2D, blurTexTemp_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, viewportW, viewportH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        allocatedW = viewportW;
        allocatedH = viewportH;
    }

    int rx = (int)std::floor(drawRect.x * dpiScale_);
    int ry = (int)std::floor((windowHeight_ - drawRect.y - drawRect.h) * dpiScale_);
    int rw = (int)std::ceil(drawRect.w * dpiScale_);
    int rh = (int)std::ceil(drawRect.h * dpiScale_);
    if (rw <= 0 || rh <= 0) return;

    rx = std::clamp(rx, 0, viewportW);
    ry = std::clamp(ry, 0, viewportH);
    rw = std::clamp(rw, 0, viewportW - rx);
    rh = std::clamp(rh, 0, viewportH - ry);
    if (rw <= 0 || rh <= 0) return;

    // Copy backend screen region directly to GPU texture (zero CPU pipeline stall!)
    glBindTexture(GL_TEXTURE_2D, blurTex_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rx, ry, rw, rh);

    // 1st Pass: Horizontal blur to blurTexTemp_
    glBindFramebuffer(GL_FRAMEBUFFER, blurFBO_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTexTemp_, 0);
    glViewport(0, 0, rw, rh);

    useShader(blurShaderHoriz_);
    glUniform1i(glGetUniformLocation(blurShaderHoriz_, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(blurShaderHoriz_, "uBlurRadius"), blurRadius * dpiScale_);
    glUniform1f(glGetUniformLocation(blurShaderHoriz_, "uWidth"), (float)rw);

    float proj[16] = {
        2.0f / rw, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / rh, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(blurShaderHoriz_, "uProjection"), 1, GL_FALSE, proj);

    float quadVerts[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        (float)rw, 0.0f, 1.0f, 0.0f,
        (float)rw, (float)rh, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        (float)rw, (float)rh, 1.0f, 1.0f,
        0.0f, (float)rh, 0.0f, 1.0f
    };

    glBindVertexArray(textVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO_);
    if (48 > textVBOCapacity_) {
        textVBOCapacity_ = 48;
        glBufferData(GL_ARRAY_BUFFER, textVBOCapacity_ * sizeof(float), nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blurTex_);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 2nd Pass: Vertical blur to backbuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportW, viewportH);

    useShader(blurShaderVert_);
    glUniform1i(glGetUniformLocation(blurShaderVert_, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(blurShaderVert_, "uBlurRadius"), blurRadius * dpiScale_);
    glUniform1f(glGetUniformLocation(blurShaderVert_, "uHeight"), (float)rh);
    glUniform2f(glGetUniformLocation(blurShaderVert_, "uSize"), (float)rw, (float)rh);
    glUniform1f(glGetUniformLocation(blurShaderVert_, "uRadius"), radius.maxRadius() * scale_ * dpiScale_);

    Vec2 pivot = scalePivotStack_.empty() ? Vec2(0, 0) : scalePivotStack_.back();
    setProjection(glGetUniformLocation(blurShaderVert_, "uProjection"), windowWidth_, windowHeight_, scale_, pivot);

    float finalVertsPacked[] = {
        drawRect.x,              drawRect.y,              0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        drawRect.x + drawRect.w, drawRect.y,              1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        drawRect.x,              drawRect.y,              0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        drawRect.x + drawRect.w, drawRect.y + drawRect.h, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        drawRect.x,              drawRect.y + drawRect.h, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(finalVertsPacked), finalVertsPacked);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blurTexTemp_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
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
