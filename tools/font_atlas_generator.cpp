// Build-time UI font atlas generator for DataLeakGuard.
// Produces a C++ header with RLE-compressed FreeType atlases.

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct GlyphInfo {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float xoff = 0.0f;
    float yoff = 0.0f;
    float xadvance = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct Atlas {
    std::string symbol;
    std::string fontName;
    float pixelSize = 0.0f;
    int width = 0;
    int height = 0;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineGap = 0.0f;
    std::vector<GlyphInfo> glyphs;
    std::vector<unsigned char> pixels;
    std::vector<uint16_t> runLengths;
    std::vector<unsigned char> runValues;
};

constexpr int kGlyphLimit = 384;
constexpr int kFirstGlyph = 32;

#if defined(_WIN32)
constexpr FT_Int32 kLoadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL;
#elif defined(__APPLE__)
constexpr FT_Int32 kLoadFlags = FT_LOAD_RENDER | FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;
#elif defined(__ANDROID__)
constexpr FT_Int32 kLoadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT;
#else
constexpr FT_Int32 kLoadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT;
#endif

bool readFile(const std::string& path, std::vector<unsigned char>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) return false;
    file.seekg(0, std::ios::beg);
    data.resize(static_cast<size_t>(fileSize));
    return file.read(reinterpret_cast<char*>(data.data()), fileSize).good();
}

std::vector<std::string> regularFontPaths() {
    std::vector<std::string> paths;
#if defined(_WIN32)
    if (const char* windir = std::getenv("WINDIR")) {
        std::string dir = std::string(windir) + "/Fonts/";
        paths.push_back(dir + "segoeui.ttf");
        paths.push_back(dir + "arial.ttf");
    }
#endif
    paths.push_back("C:/Windows/Fonts/segoeui.ttf");
    paths.push_back("C:/Windows/Fonts/arial.ttf");
    paths.push_back("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    paths.push_back("/System/Library/Fonts/SFNS.ttf");
    paths.push_back("/System/Library/Fonts/SFPro.ttf");
    paths.push_back("/System/Library/Fonts/Helvetica.ttc");
    return paths;
}

std::vector<std::string> boldFontPaths() {
    std::vector<std::string> paths;
#if defined(_WIN32)
    if (const char* windir = std::getenv("WINDIR")) {
        std::string dir = std::string(windir) + "/Fonts/";
        paths.push_back(dir + "seguisb.ttf");
        paths.push_back(dir + "segoeuib.ttf");
        paths.push_back(dir + "arialbd.ttf");
    }
#endif
    paths.push_back("C:/Windows/Fonts/seguisb.ttf");
    paths.push_back("C:/Windows/Fonts/segoeuib.ttf");
    paths.push_back("C:/Windows/Fonts/arialbd.ttf");
    paths.push_back("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf");
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    paths.push_back("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf");
    paths.push_back("/System/Library/Fonts/SFNS.ttf");
    paths.push_back("/System/Library/Fonts/SFPro.ttf");
    paths.push_back("/System/Library/Fonts/Helvetica.ttc");
    return paths;
}

bool loadFirstFont(const std::vector<std::string>& paths,
                   std::vector<unsigned char>& data,
                   std::string& pathUsed) {
    for (const auto& path : paths) {
        if (readFile(path, data)) {
            pathUsed = path;
            return true;
        }
    }
    return false;
}

bool packGlyphs(FT_Face face,
                int atlasSize,
                std::vector<unsigned char>& atlas,
                std::vector<GlyphInfo>& glyphs) {
    atlas.assign(static_cast<size_t>(atlasSize) * atlasSize, 0);
    glyphs.assign(kGlyphLimit, {});

    int currentX = 1;
    int currentY = 1;
    int rowHeight = 0;

    for (int i = kFirstGlyph; i < kGlyphLimit; ++i) {
        FT_UInt glyphIndex = FT_Get_Char_Index(face, i);
        if (FT_Load_Glyph(face, glyphIndex, kLoadFlags)) {
            continue;
        }

        FT_GlyphSlot slot = face->glyph;
        int w = static_cast<int>(slot->bitmap.width);
        int h = static_cast<int>(slot->bitmap.rows);

        if (currentX + w + 1 >= atlasSize) {
            currentX = 1;
            currentY += rowHeight + 1;
            rowHeight = 0;
        }
        if (currentY + h + 1 >= atlasSize) {
            return false;
        }
        rowHeight = std::max(rowHeight, h);

        for (int r = 0; r < h; ++r) {
            const unsigned char* src = slot->bitmap.buffer +
                                       static_cast<size_t>(r) * slot->bitmap.pitch;
            unsigned char* dst = atlas.data() +
                                 static_cast<size_t>(currentY + r) * atlasSize +
                                 currentX;
            std::copy(src, src + w, dst);
        }

        GlyphInfo& glyph = glyphs[static_cast<size_t>(i)];
        glyph.x0 = static_cast<float>(currentX) / atlasSize;
        glyph.y0 = static_cast<float>(currentY) / atlasSize;
        glyph.x1 = static_cast<float>(currentX + w) / atlasSize;
        glyph.y1 = static_cast<float>(currentY + h) / atlasSize;
        glyph.xoff = static_cast<float>(slot->bitmap_left);
        glyph.yoff = static_cast<float>(-slot->bitmap_top);
        glyph.xadvance = static_cast<float>(slot->advance.x >> 6);
        glyph.width = static_cast<float>(w);
        glyph.height = static_cast<float>(h);

        currentX += w + 1;
    }

    return true;
}

void sharpenSmallText(std::vector<unsigned char>& atlas, float pixelSize) {
    float sharpenStrength = std::clamp((24.0f - pixelSize) / (24.0f - 13.0f),
                                       0.0f,
                                       1.0f);
    if (sharpenStrength <= 0.0f) return;

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
        alpha = static_cast<unsigned char>(std::round(a * 255.0f));
    }
}

void encodeRle(Atlas& atlas) {
    atlas.runLengths.clear();
    atlas.runValues.clear();
    if (atlas.pixels.empty()) return;

    unsigned char value = atlas.pixels[0];
    uint32_t count = 0;
    auto flush = [&]() {
        while (count > 0) {
            uint16_t chunk = static_cast<uint16_t>(std::min<uint32_t>(count, 65535));
            atlas.runLengths.push_back(chunk);
            atlas.runValues.push_back(value);
            count -= chunk;
        }
    };

    for (unsigned char pixel : atlas.pixels) {
        if (pixel == value && count < 65535) {
            ++count;
        } else {
            flush();
            value = pixel;
            count = 1;
        }
    }
    flush();
}

bool buildAtlas(const std::vector<unsigned char>& fontData,
                const std::string& symbol,
                const std::string& fontName,
                float size,
                Atlas& atlas) {
    FT_Library library;
    if (FT_Init_FreeType(&library)) return false;

    FT_Face face;
    if (FT_New_Memory_Face(library,
                           fontData.data(),
                           static_cast<FT_Long>(fontData.size()),
                           0,
                           &face)) {
        FT_Done_FreeType(library);
        return false;
    }

    float pixelSize = static_cast<float>(std::max(8, static_cast<int>(std::round(size))));
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelSize))) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    int initialSize = pixelSize > 48.0f ? 2048 : (pixelSize > 20.0f ? 1024 : 512);
    std::array<int, 3> candidates = {initialSize, 1024, 2048};
    bool packed = false;
    for (int atlasSize : candidates) {
        if (atlasSize < initialSize) continue;
        if (packGlyphs(face, atlasSize, atlas.pixels, atlas.glyphs)) {
            atlas.width = atlasSize;
            atlas.height = atlasSize;
            packed = true;
            break;
        }
    }

    if (!packed) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    sharpenSmallText(atlas.pixels, pixelSize);
    atlas.symbol = symbol;
    atlas.fontName = fontName;
    atlas.pixelSize = pixelSize;
    atlas.ascent = static_cast<float>(face->size->metrics.ascender) / 64.0f;
    atlas.descent = static_cast<float>(face->size->metrics.descender) / 64.0f;
    atlas.lineGap = (static_cast<float>(face->size->metrics.height) -
                     (atlas.ascent - atlas.descent)) / 64.0f;
    encodeRle(atlas);

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return true;
}

std::string sizeToken(float size) {
    int rounded = static_cast<int>(std::round(size));
    return std::to_string(rounded);
}

std::string atlasName(const std::string& baseName, float size) {
    int rounded = static_cast<int>(std::round(size));
    if (rounded == 13) return baseName;
    return baseName + "@" + std::to_string(rounded);
}

std::string atlasSymbol(const std::string& prefix, float size) {
    return prefix + "_" + sizeToken(size);
}

void emitFloat(std::ostream& out, float value) {
    out << std::fixed << std::setprecision(8) << value << "f";
}

void emitGlyphArray(std::ostream& out, const Atlas& atlas) {
    out << "static constexpr FluxUI::GlyphInfo " << atlas.symbol << "_glyphs[] = {\n";
    for (const auto& glyph : atlas.glyphs) {
        out << "    {";
        emitFloat(out, glyph.x0); out << ", ";
        emitFloat(out, glyph.y0); out << ", ";
        emitFloat(out, glyph.x1); out << ", ";
        emitFloat(out, glyph.y1); out << ", ";
        emitFloat(out, glyph.xoff); out << ", ";
        emitFloat(out, glyph.yoff); out << ", ";
        emitFloat(out, glyph.xadvance); out << ", ";
        emitFloat(out, glyph.width); out << ", ";
        emitFloat(out, glyph.height); out << "},\n";
    }
    out << "};\n\n";
}

template <typename T>
void emitNumericArray(std::ostream& out,
                      const std::string& typeName,
                      const std::string& name,
                      const std::vector<T>& values) {
    out << "static constexpr " << typeName << " " << name << "[] = {\n    ";
    int column = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        if (column >= 24) {
            out << "\n    ";
            column = 0;
        }
        out << static_cast<uint32_t>(values[i]);
        if (i + 1 < values.size()) out << ", ";
        ++column;
    }
    out << "\n};\n\n";
}

std::string makeHeader(const std::vector<Atlas>& atlases,
                       const std::string& regularPath,
                       const std::string& boldPath) {
    std::ostringstream out;
    out << "#pragma once\n\n";
    out << "#include \"fluxui/renderer.h\"\n";
    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n\n";
    out << "namespace DataLeakGuardEmbeddedFonts {\n\n";
    out << "// Generated by tools/font_atlas_generator.cpp during CMake build.\n";
    out << "// Regular: " << regularPath << "\n";
    out << "// Bold: " << boldPath << "\n\n";

    for (const auto& atlas : atlases) {
        emitGlyphArray(out, atlas);
        emitNumericArray(out,
                         "uint16_t",
                         atlas.symbol + "_run_lengths",
                         atlas.runLengths);
        emitNumericArray(out,
                         "unsigned char",
                         atlas.symbol + "_run_values",
                         atlas.runValues);
    }

    out << "inline bool loadEmbeddedUiFontAtlases(FluxUI::Renderer& renderer) {\n";
    out << "    bool ok = true;\n";
    for (const auto& atlas : atlases) {
        out << "    ok = renderer.loadPrebakedFontAtlas(\"" << atlas.fontName << "\", ";
        emitFloat(out, atlas.pixelSize);
        out << ", " << atlas.width << ", " << atlas.height << ", ";
        emitFloat(out, atlas.ascent);
        out << ", ";
        emitFloat(out, atlas.descent);
        out << ", ";
        emitFloat(out, atlas.lineGap);
        out << ",\n";
        out << "        " << atlas.symbol << "_glyphs, "
            << "sizeof(" << atlas.symbol << "_glyphs) / sizeof("
            << atlas.symbol << "_glyphs[0]), "
            << atlas.symbol << "_run_lengths, "
            << atlas.symbol << "_run_values, "
            << "sizeof(" << atlas.symbol << "_run_lengths) / sizeof("
            << atlas.symbol << "_run_lengths[0]), "
            << atlas.pixels.size() << ") && ok;\n";
    }
    out << "    return ok;\n";
    out << "}\n\n";

    out << "\n} // namespace DataLeakGuardEmbeddedFonts\n";
    return out.str();
}

bool writeIfChanged(const std::string& path, const std::string& content) {
    {
        std::ifstream existing(path, std::ios::binary);
        if (existing.is_open()) {
            std::ostringstream buffer;
            buffer << existing.rdbuf();
            if (buffer.str() == content) {
                return true;
            }
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return file.good();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: font_atlas_generator <output-header>\n";
        return 2;
    }

    std::vector<unsigned char> regularData;
    std::vector<unsigned char> boldData;
    std::string regularPath;
    std::string boldPath;
    if (!loadFirstFont(regularFontPaths(), regularData, regularPath)) {
        std::cerr << "Could not find a regular UI font\n";
        return 1;
    }
    if (!loadFirstFont(boldFontPaths(), boldData, boldPath)) {
        std::cerr << "Could not find a bold UI font\n";
        return 1;
    }

    constexpr std::array<float, 9> sizes = {
        11.0f, 12.0f, 13.0f, 14.0f, 16.0f, 20.0f, 28.0f, 29.0f, 32.0f
    };

    std::vector<Atlas> atlases;
    atlases.reserve(sizes.size() * 2);
    for (float size : sizes) {
        Atlas regular;
        if (!buildAtlas(regularData,
                        atlasSymbol("default", size),
                        atlasName("default", size),
                        size,
                        regular)) {
            std::cerr << "Could not build regular atlas at " << size << "px\n";
            return 1;
        }
        atlases.push_back(std::move(regular));

        Atlas bold;
        if (!buildAtlas(boldData,
                        atlasSymbol("default_bold", size),
                        atlasName("default-bold", size),
                        size,
                        bold)) {
            std::cerr << "Could not build bold atlas at " << size << "px\n";
            return 1;
        }
        atlases.push_back(std::move(bold));
    }

    std::string header = makeHeader(atlases, regularPath, boldPath);
    if (!writeIfChanged(argv[1], header)) {
        std::cerr << "Could not write " << argv[1] << "\n";
        return 1;
    }

    std::cout << "Generated UI font atlases: " << argv[1] << "\n";
    return 0;
}
