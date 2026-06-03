// FluxUI Renderer - GPU-accelerated rendering
#pragma once

#include "core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <memory>

#ifndef FLUXUI_FONT_GLYPH_LIMIT
#define FLUXUI_FONT_GLYPH_LIMIT 384
#endif

namespace FluxUI {
class Widget;

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
    Skia,
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
    GlyphInfo glyphs[1024]; // Packed range defaults to Latin + Latin Extended-A.
    std::unordered_map<uint32_t, GlyphInfo> extendedGlyphs;
    std::unordered_map<uint64_t, float> kerningPairs;
    std::vector<std::pair<std::string, uint32_t>> supportedLigatures;

    // Added for HarfBuzz and glyph indexing
    std::unordered_map<uint32_t, GlyphInfo> glyphsByIndex;
    mutable void* hbFont = nullptr;
    mutable void* ftLibrary = nullptr;
    mutable void* ftFace = nullptr;

    FontData();
    ~FontData();
    FontData(FontData&& other) noexcept;
    FontData& operator=(FontData&& other) noexcept;
    FontData(const FontData& other);
    FontData& operator=(const FontData& other);

    void cleanupHarfbuzz() const;

    const GlyphInfo& getGlyph(uint32_t c) const {
        if (c < 1024) return glyphs[c];
        auto it = extendedGlyphs.find(c);
        if (it != extendedGlyphs.end()) return it->second;
        static GlyphInfo emptyGlyph = {};
        return emptyGlyph;
    }

    const GlyphInfo& getGlyphByIndex(uint32_t glyphIndex) const {
        auto it = glyphsByIndex.find(glyphIndex);
        if (it != glyphsByIndex.end()) return it->second;
        return getGlyph(glyphIndex); // fallback if not indexed
    }

    float getKerning(uint32_t left, uint32_t right, float scale) const {
        uint64_t key = ((uint64_t)left << 32) | (uint64_t)right;
        auto it = kerningPairs.find(key);
        if (it != kerningPairs.end()) {
            return it->second * scale;
        }
        return 0.0f;
    }

    std::vector<unsigned char> sourceData;
    std::vector<unsigned char> atlasPixels;
    bool loaded = false;

    // RLE prebaked atlas parameters for lazy decompression
    const uint16_t* runLengths = nullptr;
    const unsigned char* runValues = nullptr;
    size_t runCount = 0;
    size_t pixelCount = 0;
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
    ScissorPop,
    Border,
    BoxShadow,
    BackdropFilterBlur,
    // ── Compositing operations (Blink cc::PaintOp parity) ──
    FilterEffect,       // Applies a filter operation list to content below
    BlendModeBegin,     // Push blend mode isolation layer
    BlendModeEnd,       // Pop blend mode layer (composite with backdrop)
    IsolationBegin,     // Push isolation stacking context
    IsolationEnd,       // Pop isolation stacking context
    SaveLayer,          // Save layer with opacity/filter/blend (Skia SaveLayerOp parity)
    RestoreLayer,       // Restore layer composite
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

    // Extra fields for rich paint invalidation/recording
    float blurRadius = 0.0f;
    FontStyle fontStyle = FontStyle::Normal;
    Direction fontDirection = Direction::Ltr;
    UnicodeBidi unicodeBidi = UnicodeBidi::Normal;
    std::string fontName = "default";
    Rect sourceUv;
    Color tint = Color(1, 1, 1, 1);

    // Property Tree node references (cc::PropertyTrees parity)
    int transformNodeId = 0;
    int clipNodeId = 0;
    int effectNodeId = 0;

    // ── Compositing fields (Blink cc::PaintFlags / cc::FilterOperations parity) ──
    // Blend mode for BlendModeBegin/SaveLayer commands
    Style::BlendMode blendMode = Style::BlendMode::Normal;
    // Filter operations for FilterEffect/SaveLayer commands
    std::vector<FilterOperation> filterOps;
    // Isolation flag for the layer
    bool isolate = false;
    // Backdrop filter (separate from element filter — composited behind content)
    std::vector<FilterOperation> backdropFilterOps;
};

struct ImageData {
    int width = 0;
    int height = 0;
    uint32_t textureId = 0;
    bool loaded = false;
    bool svg = false;
    std::vector<unsigned char> pixels; // RGBA8 until the active backend uploads it.
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
    bool needsRepaint() const;

    // Font loading
    bool loadDefaultFont(float size, const std::string& name = "default");
    bool loadFont(const std::string& path, float size, const std::string& name = "default");
    bool loadFontFromMemory(const unsigned char* data, int dataSize, float size, const std::string& name = "default");
    bool loadPrebakedFontAtlas(const std::string& name, float pixelSize,
                               int atlasWidth, int atlasHeight,
                               float ascent, float descent, float lineGap,
                               const GlyphInfo* glyphs, size_t glyphCount,
                               const uint16_t* runLengths,
                               const unsigned char* runValues,
                               size_t runCount, size_t pixelCount);
    void warmFontCache(float size, const std::string& name = "default");
    void warmFontCache(const std::vector<float>& sizes, const std::string& name = "default");
    void releaseFontSources();
    void registerCustomFont(const std::string& family, const std::string& path);

    // Image loading. Raster formats use stb_image; SVG is rasterized by FluxUI.
    bool loadImage(const std::string& path, const std::string& name = "");
    bool loadImageFromMemory(const unsigned char* data, int dataSize,
                             const std::string& name, bool svg = false);
    Vec2 imageSize(const std::string& nameOrPath);
    const std::vector<unsigned char>* imagePixels(const std::string& nameOrPath);
    bool updateDynamicTexture(const std::string& name, const ImageData& image);
    bool rasterizeSvgWidget(Widget* svgWidget, ImageData& image);

    // Drawing primitives
    void drawRoundedRect(const Rect& rect, const Color& color, const BorderRadius& radius,
                         float opacity = 1.0f);
    void drawRoundedRectGradient(const Rect& rect, const Gradient& gradient,
                                 const BorderRadius& radius, float opacity = 1.0f);
    void drawBorder(const Rect& rect, const Border& border, const BorderRadius& radius);
    void drawBoxShadow(const Rect& rect, const BoxShadow& shadow, const BorderRadius& radius);
    void drawBackdropFilterBlur(const Rect& rect, float blurRadius, const BorderRadius& radius);
    void drawText(const std::string& text, const Vec2& pos, const Color& color,
                  float fontSize = 14.0f, FontWeight weight = FontWeight::Normal,
                  const std::string& fontName = "default",
                  FontStyle style = FontStyle::Normal,
                  Direction direction = Direction::Ltr,
                  UnicodeBidi unicodeBidi = UnicodeBidi::Normal);
    void drawTextInRect(const std::string& text, const Rect& rect, const Color& color,
                        float fontSize = 14.0f, TextAlign align = TextAlign::Left,
                        FontWeight weight = FontWeight::Normal,
                        const std::string& fontName = "default",
                        FontStyle style = FontStyle::Normal,
                        Direction direction = Direction::Ltr,
                        UnicodeBidi unicodeBidi = UnicodeBidi::Normal);
    void drawImage(const std::string& nameOrPath, const Rect& rect,
                   float opacity = 1.0f,
                   const Color& tint = Color(1, 1, 1, 1));
    void drawImage(const std::string& nameOrPath, const Rect& rect,
                   const Rect& sourceUv,
                   float opacity = 1.0f,
                   const Color& tint = Color(1, 1, 1, 1));

    // Clipping
    void pushScissor(const Rect& rect);
    void popScissor();

    // Transformation
    void pushTranslation(const Vec2& offset);
    void popTranslation();
    void pushScale(float scale, const Vec2& pivot);
    void popScale();
    void flush();
    Vec2 getTranslation() const { return translation_; }

    // Active Property Tree node IDs for batching recorded commands
    int activeTransformNodeId = 0;
    int activeClipNodeId = 0;
    int activeEffectNodeId = 0;

    // Measurement
    Vec2 measureText(const std::string& text, float fontSize,
                     const std::string& fontName = "default") const;

    // Recording & Playback for Paint Invalidation
    std::vector<RenderCommand>* recording_ = nullptr;
    Vec2 recordingTranslationStart_ = {0.0f, 0.0f};

    void startRecording(std::vector<RenderCommand>& dest) {
        recording_ = &dest;
        recordingTranslationStart_ = translation_;
    }

    void stopRecording() {
        recording_ = nullptr;
    }

    bool isRecording() const {
        return recording_ != nullptr;
    }

    void recordCommand(RenderCommand cmd) {
        if (recording_) {
            cmd.transformNodeId = activeTransformNodeId;
            cmd.clipNodeId = activeClipNodeId;
            cmd.effectNodeId = activeEffectNodeId;
            recording_->push_back(std::move(cmd));
        }
    }

    void playback(const std::vector<RenderCommand>& commands);
    void playback(const std::vector<RenderCommand>& commands, float opacityScale);

    struct RenderTargetState {
        uint32_t fbo;
        int width;
        int height;
    };
    std::vector<RenderTargetState> renderTargetStack_;

    void pushRenderTarget(uint32_t fbo, int width, int height);
    void popRenderTarget();
    void drawTexture(uint32_t textureId, const Rect& rect, float opacity = 1.0f);

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
    uint32_t imageShader_ = 0;
    uint32_t shadowShader_ = 0;
    uint32_t activeShader_ = 0;

    // Geometry
    uint32_t quadVAO_ = 0, quadVBO_ = 0;
    uint32_t instanceVBO_ = 0;  // for batched rounded rect instances
    uint32_t textVAO_ = 0, textVBO_ = 0;
    size_t textVBOCapacity_ = 0;
    std::vector<float> textVertexScratch_;

    // Batched rounded rect instances (OpenGL instancing)
    struct RoundedRectInstance {
        float rect[4];        // x, y, w, h
        float color[4];       // r, g, b, a
        float color2[4];      // r, g, b, a
        float borderColor[4]; // r, g, b, a
        float radius;
        float borderWidth;
        float opacity;
        float hasGradient;
        float gradientAngle;
        float _pad[3];        // pad to 96 bytes (24 floats)
    };
    std::vector<RoundedRectInstance> rectBatch_;
    size_t instanceVBOCapacity_ = 0;
    float batchScale_ = 1.0f;
    Vec2 batchScalePivot_ = {0, 0};
    bool batchValid_ = false;
    void flushRectBatch();
    void setupInstanceBuffer();

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

    struct ImageUniforms {
        int projection = -1;
        int texture = -1;
    } imageUniforms_;

    // Font data
    std::unordered_map<std::string, FontData> fonts_;
    mutable std::unordered_map<uint64_t, Vec2> textMeasureCache_;
    FontData* currentFont_ = nullptr;
    std::unordered_map<std::string, ImageData> images_;

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
    bool decodeImageBytes(const unsigned char* data, int dataSize,
                          ImageData& image, bool forceSvg = false);
    bool ensureImageTexture(const std::string& key, ImageData& image);
    bool ensureFontTexture(FontData& font);
    void setupQuad();
    void setupTextBuffer();
    void cacheUniformLocations();
    void useShader(uint32_t shader);
    void drawQuadWithShader(uint32_t shader, const Rect& rect);
    bool buildFontAtlas(FontData& font, const unsigned char* data, int dataSize, float size, bool isBackground = false);
    struct CustomFontFaceInfo {
        std::string family;
        std::string path;
        bool loading = false;
        bool loaded = false;
    };
    std::unordered_map<std::string, CustomFontFaceInfo> customFontRegistry_;
    void triggerCustomFontLoad(const std::string& family);
    FontData* getFontForSize(const std::string& fontName, float fontSize);
    const FontData* findFontForMeasure(const std::string& fontName, float fontSize) const;
    const std::string& resolveFontName(const std::string& fontName, FontWeight weight) const;
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
                        float fontSize, FontWeight weight, const std::string& fontName,
                        FontStyle style);
    void drawVulkanImage(const std::string& key, ImageData& image,
                         const Rect& rect, const Rect& sourceUv,
                         const Color& tint, float opacity);
    bool initSoftware(void* windowHandle);
    void shutdownSoftware();
    void beginSoftwareFrame(int windowWidth, int windowHeight);
    void endSoftwareFrame();
    void presentSoftwareFrame();
    void flushSoftwareRectBatch();
    void drawSoftwareRoundedRect(const Rect& rect, const Color& color,
                                 const Color& color2, const Color& borderColor,
                                 const BorderRadius& radius, float borderWidth,
                                 float opacity, bool hasGradient,
                                 float gradientAngle, float drawScale,
                                 const Vec2& pivot);
    void drawSoftwareBoxShadow(const Rect& rect, const BoxShadow& shadow,
                               const BorderRadius& radius);
    void drawSoftwareText(const std::string& text, const Vec2& pos,
                          const Color& color, float fontSize,
                          FontWeight weight, const std::string& fontName,
                          FontStyle style);
    void drawSoftwareImage(const std::string& key, ImageData& image,
                           const Rect& rect, const Rect& sourceUv,
                           const Color& tint, float opacity);
    void applySoftwareBackdropBlur(const Rect& rect, float blurRadius,
                                   const BorderRadius& radius);
    Rect transformSoftwareRect(const Rect& rect, float drawScale,
                               const Vec2& pivot) const;

    std::unique_ptr<VulkanRendererState> vulkan_;
    std::vector<uint32_t> softwarePixels_;
    int softwareWidth_ = 0;
    int softwareHeight_ = 0;
    bool softwareFrameActive_ = false;

    // Blink-parity GPU Composited Layer / Sub-Render Pass members
    unsigned int blurFBO_ = 0;
    unsigned int blurTex_ = 0;
    unsigned int blurTexTemp_ = 0;
    unsigned int blurShaderHoriz_ = 0;
    unsigned int blurShaderVert_ = 0;

    // Reusable fast CPU blur buffers to avoid heap thrashing
    mutable std::vector<uint32_t> softwareBlurBuffer1_;
    mutable std::vector<uint32_t> softwareBlurBuffer2_;
    mutable std::vector<uint32_t> softwareBlurBuffer3_;

public:
    struct ShapedGlyph {
        uint32_t glyphIndex;
        uint32_t codepoint;
        float xOffset;
        float yOffset;
        float xAdvance;
    };
    using ShapedRun = std::vector<ShapedGlyph>;

    ShapedRun shapeTextWithHarfbuzz(const FontData& font, const std::string& text, Direction direction) const;
    void ensureHarfbuzzFont(const FontData& font) const;
};

} // namespace FluxUI
