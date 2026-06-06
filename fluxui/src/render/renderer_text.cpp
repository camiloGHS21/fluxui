// FluxUI - text shaping + glyph drawing (HarfBuzz/FreeType).
// Extracted from renderer.cpp: complex-script shaping (shapeTextWithHarfbuzz),
// HarfBuzz font setup, ligature substitution, and the glyph-drawing path.
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "software_internal.h"

#include <glad/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

// Local copies of small renderer.cpp file-static helpers (anonymous-namespace,
// no ODR concern) so this TU is self-contained.
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

static std::string substituteLigatures(const std::string& text, const FontData& font);

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
    float boldOffset = (isBoldWeight(weight) && resolvedFontName == fontName)
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


} // namespace FluxUI