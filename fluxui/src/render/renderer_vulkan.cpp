// FluxUI - Vulkan backend (device, swapchain, pipelines, draw, frame loop).
// Extracted from renderer.cpp. Only compiled meaningfully when the Vulkan SDK
// is present; otherwise the Renderer's Vulkan members are no-SDK stubs.
//
// VMA_IMPLEMENTATION lives here (single definition across the whole library).
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "renderer_vulkan.h"
#include "fluxui/platform.h"

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

#if FLUXUI_HAS_VULKAN_SDK
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace FluxUI {

#if FLUXUI_HAS_VULKAN_SDK
namespace {
// Trivial Vk helpers (duplicated from renderer.cpp; anonymous-namespace, no ODR
// concern). Self-contained so this TU does not depend on renderer.cpp internals.
const char* physicalDeviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:                                     return "other";
    }
}
std::string vkResultMessage(const char* operation, VkResult result) {
    return std::string(operation) + " failed with VkResult " + std::to_string(result);
}
uint32_t makeVkVersion(uint32_t major, uint32_t minor, uint32_t patch) {
    return VK_MAKE_VERSION(major, minor, patch);
}
uint32_t vkVersionMajor(uint32_t version) { return version >> 22; }
uint32_t vkVersionMinor(uint32_t version) { return (version >> 12) & 0x3ff; }
uint32_t vkVersionPatch(uint32_t version) { return version & 0xfff; }
} // namespace

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

bool ensureVulkanFontTexture(VulkanRendererState& state,
                             const std::string& key,
                             FontData& font) {
    auto existing = state.fontTextures.find(key);
    if (existing != state.fontTextures.end()) {
        return existing->second.descriptorSet != VK_NULL_HANDLE;
    }
    if (font.atlasPixels.empty()) {
        if (!decompressFontAtlasShared(font)) {
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

// Software (CPU) rasterizer member functions moved to software_raster.cpp.

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

#endif // FLUXUI_HAS_VULKAN_SDK

} // namespace FluxUI