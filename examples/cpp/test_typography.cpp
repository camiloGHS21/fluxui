// FluxUI Advanced Typography CSS Properties Test
// CSS Fonts L4 / CSS Text L4 / Blink parity

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
#define CHECK_NEAR(a,b,eps) do{float _a=(float)(a),_b=(float)(b);if(std::abs(_a-_b)>(float)(eps)){std::cerr<<"FAIL @ line "<<__LINE__<<std::endl;return 1;}}while(0)

int test_font_variant_caps() {
    std::cout << "[1] font-variant-caps" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-variant-caps: small-caps; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasFontVariantCaps);
    CHECK(s.rare().fontVariantCaps == "small-caps");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_variant_numeric() {
    std::cout << "[2] font-variant-numeric" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-variant-numeric: tabular-nums lining-nums; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasFontVariantNumeric);
    CHECK(s.rare().fontVariantNumeric.find("tabular-nums") != std::string::npos);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_variant_ligatures() {
    std::cout << "[3] font-variant-ligatures" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-variant-ligatures: no-common-ligatures; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasFontVariantLigatures);
    CHECK(s.rare().fontVariantLigatures == "no-common-ligatures");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_feature_settings() {
    std::cout << "[4] font-feature-settings" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-feature-settings: \"smcp\" on, \"liga\" off; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasFontFeatureSettings);
    CHECK(s.rare().fontFeatureSettings.find("smcp") != std::string::npos);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_variation_settings() {
    std::cout << "[5] font-variation-settings" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-variation-settings: \"wght\" 700, \"wdth\" 75; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasFontVariationSettings);
    CHECK(s.rare().fontVariationSettings.find("wght") != std::string::npos);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_optical_sizing() {
    std::cout << "[6] font-optical-sizing" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-optical-sizing: none; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasFontOpticalSizing);
    CHECK(s.rare().fontOpticalSizing == "none");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_stretch() {
    std::cout << "[7] font-stretch" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { font-stretch: condensed; } .b { font-stretch: 75%; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.rare().hasFontStretch); CHECK(sa.rare().fontStretch == "condensed");
    CHECK(sb.rare().hasFontStretch); CHECK(sb.rare().fontStretch == "75%");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_tab_size() {
    std::cout << "[8] tab-size" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { tab-size: 4; } .b { tab-size: 32px; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.rare().hasTabSize); CHECK_NEAR(sa.rare().tabSize, 4.0f, 1e-2f);
    CHECK(sb.rare().hasTabSize); CHECK_NEAR(sb.rare().tabSize, 32.0f, 1e-2f);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_hyphens() {
    std::cout << "[9] hyphens" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { hyphens: auto; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasHyphens); CHECK(s.rare().hyphens == "auto");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_overflow_wrap_and_word_wrap() {
    std::cout << "[10] overflow-wrap / word-wrap alias" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { overflow-wrap: break-word; } .b { word-wrap: anywhere; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.rare().hasOverflowWrap); CHECK(sa.rare().overflowWrap == "break-word");
    CHECK(sb.rare().hasOverflowWrap); CHECK(sb.rare().overflowWrap == "anywhere");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_text_indent() {
    std::cout << "[11] text-indent" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { text-indent: 2em; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasTextIndent); CHECK_NEAR(s.rare().textIndent, 32.0f, 1.0f); // 2em * 16px emBase default
    std::cout << "  PASS" << std::endl; return 0;
}

int test_text_justify() {
    std::cout << "[12] text-justify" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { text-justify: inter-character; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasTextJustify); CHECK(s.rare().textJustify == "inter-character");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_hanging_punctuation() {
    std::cout << "[13] hanging-punctuation" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { hanging-punctuation: first last; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasHangingPunctuation); CHECK(s.rare().hangingPunctuation == "first last");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_line_break() {
    std::cout << "[14] line-break" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { line-break: strict; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasLineBreak); CHECK(s.rare().lineBreak == "strict");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_font_palette() {
    std::cout << "[15] font-palette" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { font-palette: --brand; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().fontPalette == "--brand");
    std::cout << "  PASS" << std::endl; return 0;
}

int main() {
    std::cout << "=== FluxUI Advanced Typography Tests ===" << std::endl;
    int rc = 0;
    rc |= test_font_variant_caps();
    rc |= test_font_variant_numeric();
    rc |= test_font_variant_ligatures();
    rc |= test_font_feature_settings();
    rc |= test_font_variation_settings();
    rc |= test_font_optical_sizing();
    rc |= test_font_stretch();
    rc |= test_tab_size();
    rc |= test_hyphens();
    rc |= test_overflow_wrap_and_word_wrap();
    rc |= test_text_indent();
    rc |= test_text_justify();
    rc |= test_hanging_punctuation();
    rc |= test_line_break();
    rc |= test_font_palette();
    if (rc == 0) std::cout << "\nAll typography tests passed!" << std::endl;
    else std::cerr << "\nSome tests FAILED" << std::endl;
    return rc;
}
