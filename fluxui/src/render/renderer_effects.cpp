// FluxUI - backdrop filter blur (software + GPU separable Gaussian).
// Extracted from renderer.cpp: the backdrop-filter:blur() implementation used
// by glass/acrylic surfaces.
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "software_internal.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

// Local copy of renderer.cpp's file-static projection helper (anonymous
// namespace, no ODR concern) so this TU is self-contained.
namespace {
void setProjection(int projectionLocation, int w, int h, float scale = 1.0f, Vec2 pivot = {0,0}) {
    float m[16] = {
        2.0f/w, 0,      0, 0,
        0,     -2.0f/h, 0, 0,
        0,      0,     -1, 0,
       -1,      1,      0, 1
    };
    if (scale != 1.0f) {
        float sx = scale, sy = scale;
        float tx = pivot.x * (1.0f - sx);
        float ty = pivot.y * (1.0f - sy);
        m[0] *= sx;
        m[5] *= sy;
        m[12] += tx * (2.0f / w);
        m[13] += ty * (-2.0f / h);
    }
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, m);
}
} // namespace

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


} // namespace FluxUI