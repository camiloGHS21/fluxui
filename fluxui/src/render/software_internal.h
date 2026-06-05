// FluxUI — internal helpers for the CPU software rasterizer.
//
// These pure helpers were a file-local anonymous namespace in the monolithic
// renderer.cpp. They are pulled into a shared header (FluxUI::detail, inline) so
// the software rasterizer member functions can live in their own TU
// (software_raster.cpp) while the few remaining users in renderer.cpp (e.g. the
// SVG blur pass) still see one definition. Private to fluxui/src/render.
#pragma once
#include "fluxui/core.h"
#include "fluxui/widgets.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace FluxUI {
namespace detail {

struct SoftwareClip {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

inline uint8_t softwareToByte(float value) {
    return static_cast<uint8_t>(
        std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

inline uint32_t softwarePackOpaque(const Color& color) {
    return 0xff000000u |
           (static_cast<uint32_t>(softwareToByte(color.r)) << 16) |
           (static_cast<uint32_t>(softwareToByte(color.g)) << 8) |
           static_cast<uint32_t>(softwareToByte(color.b));
}

inline Color softwareColorFromInstance(const float rgba[4]) {
    return Color(rgba[0], rgba[1], rgba[2], rgba[3]);
}

inline SoftwareClip softwareClipFor(const std::vector<Rect>& scissorStack,
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

inline uint32_t softwareNextCodepoint(const std::string& s, size_t& i) {
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

inline float softwareRoundedCoverage(float px, float py, const Rect& rect, float radius) {
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

inline Color softwareGradientColor(const Color& a,
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

inline void softwareBlendPixel(std::vector<uint32_t>& pixels,
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
    uint32_t invA = 255u - a_int;
    uint32_t sumR = srcR * a_int + dstR * invA;
    uint32_t sumG = srcG * a_int + dstG * invA;
    uint32_t sumB = srcB * a_int + dstB * invA;
    uint32_t outR = (sumR + (sumR >> 8) + 1u) >> 8;
    uint32_t outG = (sumG + (sumG >> 8) + 1u) >> 8;
    uint32_t outB = (sumB + (sumB >> 8) + 1u) >> 8;

    dst = 0xff000000u | (outR << 16) | (outG << 8) | outB;
}

inline float softwareSampleFontAlpha(const FontData& font,
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

} // namespace detail
} // namespace FluxUI
