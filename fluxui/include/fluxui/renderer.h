// FluxUI Renderer - GPU-accelerated rendering
#pragma once

#include "core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>



namespace FluxUI {

// ============================================================
//  Render Backend Selection
// ============================================================

enum class RenderBackendType {
    Auto,
    Vulkan,
    Direct3D12,
    Direct12 = Direct3D12,
    DirectX12 = Direct3D12,
    Metal,
    Compatibility
};

struct RenderBackendInfo {
    RenderBackendType type = RenderBackendType::Auto;
    const char* name = "Auto";
    bool compiled = false;
    bool selectable = false;
    const char* note = "";
};

struct VulkanProbeResult {
    bool loaderAvailable = false;
    bool windowCreated = false;
    bool instanceCreated = false;
    bool surfaceCreated = false;
    uint32_t physicalDeviceCount = 0;
    std::vector<std::string> deviceNames;
    std::string error;
};

struct VulkanRendererState;

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual RenderBackendInfo info() const = 0;
    virtual uint32_t windowFlags() const = 0;
};

// ============================================================
//  Font Atlas (generated from stb_truetype)
// ============================================================

struct GlyphInfo {
    float x0, y0, x1, y1;         // texcoords in atlas
    float xoff, yoff;             // offset from cursor
    float xadvance;               // cursor advance
    float width, height;          // glyph size in pixels
};

struct FontData {
    uint32_t textureId = 0;
    int atlasWidth = 0, atlasHeight = 0;
    float fontSize = 0;
    float ascent = 0, descent = 0, lineGap = 0;
    GlyphInfo glyphs[1024]; // Expanded Unicode range
    std::vector<unsigned char> sourceData;
    std::vector<unsigned char> atlasPixels;
    bool loaded = false;
};

// ============================================================
//  Render Command (batched rendering)
// ============================================================

struct RenderVertex {
    float x, y;       // position
    float u, v;       // texcoord
    float r, g, b, a; // color
};

enum class RenderCommandType {
    RoundedRect,
    Text,
    TexturedQuad,
    Scissor,
    ScissorPop
};

struct RenderCommand {
    RenderCommandType type;
    Rect rect;
    Color color;
    Color color2;         // for gradients
    BorderRadius radius;
    Border border;
    BoxShadow shadow;
    Gradient gradient;
    float opacity = 1.0f;
    bool hasShadow = false;
    bool hasGradient = false;
    bool hasBorder = false;

    // Text specific
    std::string text;
    float fontSize = 14;
    FontWeight fontWeight = FontWeight::Normal;
    TextAlign textAlign = TextAlign::Left;

    // Scissor
    Rect scissorRect;

    // Texture
    uint32_t textureId = 0;
};

// ============================================================
//  Renderer
// ============================================================

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Backend selection. Call before init(). Auto uses the compiled default preference.
    void setBackend(RenderBackendType backend);
    RenderBackendType backendPreference() const { return backendPreference_; }
    RenderBackendType activeBackend() const { return activeBackend_; }
    const char* activeBackendName() const;
    uint32_t windowFlags() const;

    static RenderBackendInfo getBackendInfo(RenderBackendType backend);
    static const char* backendName(RenderBackendType backend);
    static RenderBackendType defaultBackend();
    static VulkanProbeResult probeVulkanRuntime(const char* appName = "FluxUI Vulkan Probe");

    bool init(void* windowHandle);
    void shutdown();

    // Frame management
    void beginFrame(int windowWidth, int windowHeight);
    void endFrame();

    // Font loading
    bool loadFont(const std::string& path, float size, const std::string& name = "default");
    bool loadFontFromMemory(const unsigned char* data, int dataSize, float size, const std::string& name = "default");

    // Drawing primitives
    void drawRoundedRect(const Rect& rect, const Color& color, const BorderRadius& radius,
                         float opacity = 1.0f);
    void drawRoundedRectGradient(const Rect& rect, const Gradient& gradient,
                                 const BorderRadius& radius, float opacity = 1.0f);
    void drawBorder(const Rect& rect, const Border& border, const BorderRadius& radius);
    void drawBoxShadow(const Rect& rect, const BoxShadow& shadow, const BorderRadius& radius);
    void drawText(const std::string& text, const Vec2& pos, const Color& color,
                  float fontSize = 14.0f, FontWeight weight = FontWeight::Normal,
                  const std::string& fontName = "default");
    void drawTextInRect(const std::string& text, const Rect& rect, const Color& color,
                        float fontSize = 14.0f, TextAlign align = TextAlign::Left,
                        FontWeight weight = FontWeight::Normal,
                        const std::string& fontName = "default");

    // Clipping
    void pushScissor(const Rect& rect);
    void popScissor();

    // Transformation
    void pushTranslation(const Vec2& offset);
    void popTranslation();
    void pushScale(float scale, const Vec2& pivot);
    void popScale();
    Vec2 getTranslation() const { return translation_; }

    // Measurement
    Vec2 measureText(const std::string& text, float fontSize,
                     const std::string& fontName = "default") const;

    // Window size
    Vec2 getWindowSize() const { return {(float)windowWidth_, (float)windowHeight_}; }

private:
    void* window_ = nullptr;
    void* glContext_ = nullptr;
    RenderBackendType backendPreference_ = defaultBackend();
    RenderBackendType activeBackend_ = defaultBackend();
    bool backendResolved_ = false;
    bool backendInitialized_ = false;
    int windowWidth_ = 0, windowHeight_ = 0;
    float dpiScale_ = 1.0f;

    // Shader programs
    uint32_t roundedRectShader_ = 0;
    uint32_t textShader_ = 0;
    uint32_t shadowShader_ = 0;
    uint32_t activeShader_ = 0;

    // Geometry
    uint32_t quadVAO_ = 0, quadVBO_ = 0;
    uint32_t textVAO_ = 0, textVBO_ = 0;
    size_t textVBOCapacity_ = 0;
    std::vector<float> textVertexScratch_;

    struct RoundedRectUniforms {
        int projection = -1;
        int rect = -1;
        int color = -1;
        int color2 = -1;
        int borderColor = -1;
        int radius = -1;
        int borderWidth = -1;
        int opacity = -1;
        int hasGradient = -1;
        int gradientAngle = -1;
    } roundedUniforms_;

    struct ShadowUniforms {
        int projection = -1;
        int rect = -1;
        int color = -1;
        int radius = -1;
        int blur = -1;
        int opacity = -1;
        int boxSize = -1;
    } shadowUniforms_;

    struct TextUniforms {
        int projection = -1;
        int texture = -1;
    } textUniforms_;

    // Font data
    std::unordered_map<std::string, FontData> fonts_;
    mutable std::unordered_map<std::string, Vec2> textMeasureCache_;
    FontData* currentFont_ = nullptr;

    // Scissor stack
    std::vector<Rect> scissorStack_;

    // Translation
    Vec2 translation_ = {0, 0};
    std::vector<Vec2> translationStack_;

    // Scale
    float scale_ = 1.0f;
    std::vector<float> scaleStack_;
    std::vector<Vec2> scalePivotStack_;

    // Internal methods
    uint32_t compileShader(const char* vertSrc, const char* fragSrc);
    void setupQuad();
    void setupTextBuffer();
    void cacheUniformLocations();
    void useShader(uint32_t shader);
    void drawQuadWithShader(uint32_t shader, const Rect& rect);
    bool buildFontAtlas(FontData& font, const unsigned char* data, int dataSize, float size);
    FontData* getFontForSize(const std::string& fontName, float fontSize);
    const FontData* findFontForMeasure(const std::string& fontName, float fontSize) const;
    bool initVulkan(void* windowHandle);
    void shutdownVulkan();
    bool beginVulkanFrame(int windowWidth, int windowHeight);
    void endVulkanFrame();
    void drawVulkanRect(const Rect& rect, const Color& color, float opacity = 1.0f);
    void drawVulkanRoundedRect(const Rect& rect, const Color& color, const Color& color2,
                               const Color& borderColor, const BorderRadius& radius,
                               float borderWidth, float opacity, bool hasGradient,
                               float gradientAngle);
    void drawVulkanBoxShadow(const Rect& rect, const BoxShadow& shadow,
                             const BorderRadius& radius);
    void drawVulkanText(const std::string& text, const Vec2& pos, const Color& color,
                        float fontSize, FontWeight weight, const std::string& fontName);

    std::unique_ptr<VulkanRendererState> vulkan_;
};

} // namespace FluxUI
