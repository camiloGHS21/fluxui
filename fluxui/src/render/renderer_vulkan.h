// FluxUI - Vulkan backend shared types & cross-TU entry points (internal).
//
// The Vulkan swapchain/pipeline/device backend lives in renderer_vulkan.cpp.
// renderer.cpp still owns the Renderer object (which holds a
// unique_ptr<VulkanRendererState> member and calls a few Vulkan entry points
// from its backend-agnostic code), so the state struct and those entry points
// are declared here and shared between the two translation units.
//
// Private to fluxui/src/render; never installed.
#pragma once
#include "fluxui/renderer.h"

#ifndef FLUXUI_HAS_VULKAN_SDK
#define FLUXUI_HAS_VULKAN_SDK 0
#endif

#include <glad/gl.h>
#if FLUXUI_HAS_VULKAN_SDK
#include <vk_mem_alloc.h>   // NOTE: VMA_IMPLEMENTATION is defined only in renderer_vulkan.cpp
#endif

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

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

#if FLUXUI_HAS_VULKAN_SDK
// Cross-TU Vulkan entry points called from renderer.cpp's backend-agnostic code.
// (Definitions live in renderer_vulkan.cpp.)
bool ensureVulkanImageTexture(VulkanRendererState& state,
                              const std::string& key,
                              ImageData& image);
bool ensureVulkanFontTexture(VulkanRendererState& state,
                             const std::string& key,
                             FontData& font);
void setVulkanScissor(VulkanRendererState& state,
                      const std::vector<Rect>& scissorStack,
                      float dpiScale);
void flushVulkanBatches(VulkanRendererState& state,
                        const std::vector<Rect>& scissorStack,
                        float dpiScale);

// Font atlas decompression lives in renderer.cpp (font subsystem) but is needed
// by the Vulkan font-texture upload path.
bool decompressFontAtlasShared(FontData& font);
#endif

} // namespace FluxUI
