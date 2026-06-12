// FluxUI — CPU software rasterizer (no-GPU fallback) implementation.
// Extracted from the monolithic renderer.cpp. Member functions of Renderer that
// implement the Compatibility (software) backend: init/present + rounded-rect,
// box-shadow, text and image rasterization. Pure pixel helpers live in
// software_internal.h (FluxUI::detail).
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "software_internal.h"

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

#include "fluxui/platform.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace FluxUI {

using detail::SoftwareClip;
using detail::softwarePackOpaque;
using detail::softwareColorFromInstance;
using detail::softwareClipFor;
using detail::softwareRoundedCoverage;
using detail::softwareBlendPixelFast;
using detail::softwareSampleFontAlpha;

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

void Renderer::drawSoftwareGradientRect(const Rect& rect,
                                        const Gradient& gradient,
                                        const BorderRadius& radius,
                                        float opacity,
                                        float drawScale,
                                        const Vec2& pivot) {
    if (!softwareFrameActive_ || softwarePixels_.empty() || opacity <= 0.0f) {
        return;
    }
    if (gradient.stops.empty()) return;

    Rect drawRect = transformSoftwareRect(rect, drawScale, pivot);
    float left = std::floor(drawRect.x + 0.5f);
    float right = std::floor(drawRect.x + drawRect.w + 0.5f);
    float top = std::floor(drawRect.y + 0.5f);
    float bottom = std::floor(drawRect.y + drawRect.h + 0.5f);
    drawRect.x = left;
    drawRect.y = top;
    drawRect.w = std::max(1.0f, right - left);
    drawRect.h = std::max(1.0f, bottom - top);

    float scaledRadius = std::max(0.0f, radius.uniform() * drawScale);

    SoftwareClip clip = softwareClipFor(scissorStack_, softwareWidth_,
                                        softwareHeight_, dpiScale_);
    int x0 = std::max(clip.x0, static_cast<int>(std::floor(drawRect.x)));
    int y0 = std::max(clip.y0, static_cast<int>(std::floor(drawRect.y)));
    int x1 = std::min(clip.x1, static_cast<int>(std::ceil(drawRect.x + drawRect.w)));
    int y1 = std::min(clip.y1, static_cast<int>(std::ceil(drawRect.y + drawRect.h)));
    if (x0 >= x1 || y0 >= y1) return;

    float safeMargin = std::max(0.5f, scaledRadius);
    float safeX0 = safeMargin, safeX1 = drawRect.w - safeMargin;
    float safeY0 = safeMargin, safeY1 = drawRect.h - safeMargin;

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
                if (coverage <= 0.0f) continue;
            }

            // Sample the full gradient ramp at this pixel (CSS-correct math).
            Color fill = gradient.sampleAt(px, py, drawRect.x, drawRect.y,
                                           drawRect.w, drawRect.h);
            uint32_t fR = static_cast<uint32_t>(std::clamp(fill.r, 0.0f, 1.0f) * 255.0f);
            uint32_t fG = static_cast<uint32_t>(std::clamp(fill.g, 0.0f, 1.0f) * 255.0f);
            uint32_t fB = static_cast<uint32_t>(std::clamp(fill.b, 0.0f, 1.0f) * 255.0f);
            uint32_t a_int = static_cast<uint32_t>(fill.a * opacity * coverage * 255.0f);
            softwareBlendPixelFast(softwarePixels_[rowOffset + x], fR, fG, fB, a_int);
        }
    }
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
    float boldOffset = (isBoldWeight(weight) && resolvedFontName == fontName)
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


} // namespace FluxUI
