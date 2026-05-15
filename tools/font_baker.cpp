#define STB_TRUETYPE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include "../vendor/stb/stb_truetype.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>

struct BakedGlyph {
    float x0, y0, x1, y1;
    float xoff, yoff;
    float xadvance;
    float width, height;
};

int main() {
    std::ifstream file("C:/Windows/Fonts/segoeui.ttf", std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Could not find font!" << std::endl; return 1; }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttf(size);
    file.read(reinterpret_cast<char*>(ttf.data()), size);
    
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf.data(), 0)) { std::cerr << "Failed to init font" << std::endl; return 1; }
    
    float fontSize = 32.0f; // Bake at standard 16px size
    float dpiScale = 1.0f; // Scale 1.0
    float scaledSize = fontSize * dpiScale;
    int atlasWidth = 1024;
    int atlasHeight = 1024;
    
    std::vector<unsigned char> atlas(atlasWidth * atlasHeight, 0);
    
    float scale = stbtt_ScaleForPixelHeight(&info, scaledSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    
    float baked_ascent = (ascent * scale) / dpiScale;
    float baked_descent = (descent * scale) / dpiScale;
    float baked_lineGap = (lineGap * scale) / dpiScale;
    
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, atlas.data(), atlasWidth, atlasHeight, 0, 2, nullptr);
    stbtt_PackSetOversampling(&pc, 2, 2);
    
    stbtt_packedchar charData[1024] = {};
    stbtt_PackFontRange(&pc, ttf.data(), 0, scaledSize, 32, 1024-32, charData+32);
    stbtt_PackEnd(&pc);
    
    BakedGlyph glyphs[1024] = {};
    for (int i = 32; i < 1024; i++) {
        auto& cd = charData[i];
        auto& gi = glyphs[i];

        stbtt_aligned_quad q;
        float dummyX = 0, dummyY = 0;
        stbtt_GetPackedQuad(charData, atlasWidth, atlasHeight, i, &dummyX, &dummyY, &q, 0);

        gi.x0 = q.s0;
        gi.y0 = q.t0;
        gi.x1 = q.s1;
        gi.y1 = q.t1;
        gi.xoff = q.x0 / dpiScale;
        gi.yoff = q.y0 / dpiScale;
        gi.xadvance = cd.xadvance / dpiScale;
        gi.width = (q.x1 - q.x0) / dpiScale;
        gi.height = (q.y1 - q.y0) / dpiScale;
    }
    
    std::ofstream out("fluxui/src/font_atlas.h");
    out << "#pragma once\n\n";
    out << "struct BakedGlyph {\n";
    out << "    float x0, y0, x1, y1;\n";
    out << "    float xoff, yoff;\n";
    out << "    float xadvance;\n";
    out << "    float width, height;\n";
    out << "};\n\n";
    
    out << "static const int font_atlas_width = " << atlasWidth << ";\n";
    out << "static const int font_atlas_height = " << atlasHeight << ";\n";
    out << "static const float font_size = " << fontSize << ";\n";
    out << "static const float font_ascent = " << baked_ascent << ";\n";
    out << "static const float font_descent = " << baked_descent << ";\n";
    out << "static const float font_lineGap = " << baked_lineGap << ";\n\n";
    
    out << "static const BakedGlyph font_glyphs[1024] = {\n";
    for (int i = 0; i < 1024; ++i) {
        auto& pg = glyphs[i];
        out << "    {" << pg.x0 << ", " << pg.y0 << ", " << pg.x1 << ", " << pg.y1 << ", " 
            << pg.xoff << ", " << pg.yoff << ", " << pg.xadvance << ", " << pg.width << ", " << pg.height << "},\n";
    }
    out << "};\n\n";
    
    out << "static const unsigned char font_atlas_pixels[" << atlasWidth * atlasHeight << "] = {\n";
    int count = 0;
    for (unsigned char p : atlas) {
        out << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)p << ",";
        if (++count % 32 == 0) out << "\n";
    }
    out << "\n};\n";
    out.close();
    
    std::cout << "Font atlas baked successfully!" << std::endl;
    return 0;
}
