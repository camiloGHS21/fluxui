// FluxUI CSS Filter (filter + backdrop-filter) Parity Test
// Mirrors Blink's filter_operation.h / css_filter_value.h grammar.
//
// Reference: https://www.w3.org/TR/filter-effects-1/
//            https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/style/filter_operation.h

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " @ line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    float _a = static_cast<float>(a), _b = static_cast<float>(b); \
    if (std::abs(_a - _b) > static_cast<float>(eps)) { \
        std::cerr << "FAIL: " << #a << " (" << _a << ") ~= " << #b \
                  << " (" << _b << ") @ line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while (0)

// ============================================================
// [1] none / empty
// ============================================================
int test_none_and_empty() {
    std::cout << "[1] filter: none / empty string..." << std::endl;
    CHECK(StyleSheet::parseFilterOperations("none").empty());
    CHECK(StyleSheet::parseFilterOperations("").empty());
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [2] blur()
// ============================================================
int test_blur() {
    std::cout << "[2] filter: blur()" << std::endl;

    auto ops = StyleSheet::parseFilterOperations("blur(4px)");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::Blur);
    CHECK_NEAR(ops[0].amount, 4.0f, 1e-4f);

    // Default (no arg) → 0px per spec
    auto ops2 = StyleSheet::parseFilterOperations("blur(0)");
    CHECK(ops2.size() == 1);
    CHECK_NEAR(ops2[0].amount, 0.0f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [3] brightness / contrast / opacity
// ============================================================
int test_amount_functions() {
    std::cout << "[3] filter: brightness() contrast() opacity()" << std::endl;

    auto b = StyleSheet::parseFilterOperations("brightness(0.5)");
    CHECK(b.size() == 1);
    CHECK(b[0].type == FilterOperationType::Brightness);
    CHECK_NEAR(b[0].amount, 0.5f, 1e-4f);

    // Percentage form
    auto b2 = StyleSheet::parseFilterOperations("brightness(150%)");
    CHECK(b2.size() == 1);
    CHECK(b2[0].type == FilterOperationType::Brightness);
    CHECK_NEAR(b2[0].amount, 1.5f, 1e-4f);

    auto c = StyleSheet::parseFilterOperations("contrast(200%)");
    CHECK(c.size() == 1);
    CHECK(c[0].type == FilterOperationType::Contrast);
    CHECK_NEAR(c[0].amount, 2.0f, 1e-4f);

    auto o = StyleSheet::parseFilterOperations("opacity(0.8)");
    CHECK(o.size() == 1);
    CHECK(o[0].type == FilterOperationType::Opacity);
    CHECK_NEAR(o[0].amount, 0.8f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [4] grayscale / sepia / invert / saturate
// ============================================================
int test_color_matrix_functions() {
    std::cout << "[4] filter: grayscale() sepia() invert() saturate()" << std::endl;

    // bare keyword without arg → 1 (fully applied)
    auto g = StyleSheet::parseFilterOperations("grayscale(1)");
    CHECK(g.size() == 1);
    CHECK(g[0].type == FilterOperationType::Grayscale);
    CHECK_NEAR(g[0].amount, 1.0f, 1e-4f);

    auto g2 = StyleSheet::parseFilterOperations("grayscale(50%)");
    CHECK(g2.size() == 1);
    CHECK_NEAR(g2[0].amount, 0.5f, 1e-4f);

    auto s = StyleSheet::parseFilterOperations("sepia(0.75)");
    CHECK(s.size() == 1);
    CHECK(s[0].type == FilterOperationType::Sepia);
    CHECK_NEAR(s[0].amount, 0.75f, 1e-4f);

    auto inv = StyleSheet::parseFilterOperations("invert(100%)");
    CHECK(inv.size() == 1);
    CHECK(inv[0].type == FilterOperationType::Invert);
    CHECK_NEAR(inv[0].amount, 1.0f, 1e-4f);

    auto sat = StyleSheet::parseFilterOperations("saturate(2)");
    CHECK(sat.size() == 1);
    CHECK(sat[0].type == FilterOperationType::Saturate);
    CHECK_NEAR(sat[0].amount, 2.0f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [5] hue-rotate
// ============================================================
int test_hue_rotate() {
    std::cout << "[5] filter: hue-rotate()" << std::endl;

    auto h = StyleSheet::parseFilterOperations("hue-rotate(90deg)");
    CHECK(h.size() == 1);
    CHECK(h[0].type == FilterOperationType::HueRotate);
    CHECK_NEAR(h[0].amount, 90.0f, 1e-3f);

    auto h2 = StyleSheet::parseFilterOperations("hue-rotate(0.5turn)");
    CHECK(h2.size() == 1);
    CHECK_NEAR(h2[0].amount, 180.0f, 1e-2f);

    auto h3 = StyleSheet::parseFilterOperations("hue-rotate(1rad)");
    CHECK(h3.size() == 1);
    // 1 rad ≈ 57.296 deg
    CHECK_NEAR(h3[0].amount, 57.296f, 0.01f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [6] drop-shadow — lengths + hex color
// ============================================================
int test_drop_shadow_hex() {
    std::cout << "[6] filter: drop-shadow() with hex color" << std::endl;

    auto ops = StyleSheet::parseFilterOperations("drop-shadow(2px 4px 6px #ff0000)");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::DropShadow);
    CHECK_NEAR(ops[0].shadowOffsetX, 2.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowOffsetY, 4.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowBlur,    6.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowColor.r, 1.0f, 1e-3f);
    CHECK_NEAR(ops[0].shadowColor.g, 0.0f, 1e-3f);
    CHECK_NEAR(ops[0].shadowColor.b, 0.0f, 1e-3f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [7] drop-shadow — rgb() functional color
// ============================================================
int test_drop_shadow_rgb() {
    std::cout << "[7] filter: drop-shadow() with rgb() color" << std::endl;

    auto ops = StyleSheet::parseFilterOperations("drop-shadow(0px 2px 8px rgb(0, 128, 255))");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::DropShadow);
    CHECK_NEAR(ops[0].shadowOffsetX, 0.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowOffsetY, 2.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowBlur,    8.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowColor.r, 0.0f,       1e-3f);
    CHECK_NEAR(ops[0].shadowColor.g, 128.0f/255.0f, 1e-3f);
    CHECK_NEAR(ops[0].shadowColor.b, 1.0f,       1e-3f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [8] drop-shadow — named color, no blur radius
// ============================================================
int test_drop_shadow_named_color_no_blur() {
    std::cout << "[8] filter: drop-shadow() named color, no blur" << std::endl;

    auto ops = StyleSheet::parseFilterOperations("drop-shadow(3px 5px black)");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::DropShadow);
    CHECK_NEAR(ops[0].shadowOffsetX, 3.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowOffsetY, 5.0f, 1e-4f);
    CHECK_NEAR(ops[0].shadowBlur,    0.0f, 1e-4f);
    // black → r=g=b=0
    CHECK_NEAR(ops[0].shadowColor.r, 0.0f, 1e-3f);
    CHECK_NEAR(ops[0].shadowColor.g, 0.0f, 1e-3f);
    CHECK_NEAR(ops[0].shadowColor.b, 0.0f, 1e-3f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [9] url() reference filter
// ============================================================
int test_url_reference() {
    std::cout << "[9] filter: url(#svg-filter)" << std::endl;

    auto ops = StyleSheet::parseFilterOperations("url(#myFilter)");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::Reference);
    CHECK(ops[0].url == "#myFilter");

    // With quotes
    auto ops2 = StyleSheet::parseFilterOperations("url(\"#another\")");
    CHECK(ops2.size() == 1);
    CHECK(ops2[0].url == "#another");

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [10] Multiple functions in one filter: shorthand
// ============================================================
int test_multiple_functions() {
    std::cout << "[10] filter: blur() brightness() contrast() — multiple functions" << std::endl;

    auto ops = StyleSheet::parseFilterOperations("blur(4px) brightness(0.8) contrast(1.2)");
    CHECK(ops.size() == 3);
    CHECK(ops[0].type == FilterOperationType::Blur);
    CHECK_NEAR(ops[0].amount, 4.0f, 1e-4f);
    CHECK(ops[1].type == FilterOperationType::Brightness);
    CHECK_NEAR(ops[1].amount, 0.8f, 1e-4f);
    CHECK(ops[2].type == FilterOperationType::Contrast);
    CHECK_NEAR(ops[2].amount, 1.2f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [11] Complex multi-function with drop-shadow in the middle
// ============================================================
int test_complex_chain() {
    std::cout << "[11] filter: grayscale() drop-shadow() sepia() — complex chain" << std::endl;

    auto ops = StyleSheet::parseFilterOperations(
        "grayscale(50%) drop-shadow(1px 1px 2px #000000) sepia(0.3)");
    CHECK(ops.size() == 3);
    CHECK(ops[0].type == FilterOperationType::Grayscale);
    CHECK_NEAR(ops[0].amount, 0.5f, 1e-4f);
    CHECK(ops[1].type == FilterOperationType::DropShadow);
    CHECK_NEAR(ops[1].shadowBlur, 2.0f, 1e-4f);
    CHECK(ops[2].type == FilterOperationType::Sepia);
    CHECK_NEAR(ops[2].amount, 0.3f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [12] StyleSheet::mergeProperty — filter: parsed into Style
// ============================================================
int test_merge_property_filter() {
    std::cout << "[12] mergeProperty: filter: parses into Style.filterOperations" << std::endl;

    Style s;
    StyleSheet::mergeProperty(s, "filter", "blur(8px) brightness(0.9)");
    CHECK(s.hasFilter);
    CHECK(s.filterOperations.size() == 2);
    CHECK(s.filterOperations[0].type == FilterOperationType::Blur);
    CHECK_NEAR(s.filterOperations[0].amount, 8.0f, 1e-4f);
    CHECK(s.filterOperations[1].type == FilterOperationType::Brightness);
    CHECK_NEAR(s.filterOperations[1].amount, 0.9f, 1e-4f);

    // filter: none clears the list
    StyleSheet::mergeProperty(s, "filter", "none");
    CHECK(!s.hasFilter);
    CHECK(s.filterOperations.empty());

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [13] StyleSheet::mergeProperty — backdrop-filter: parsed + legacy scalar synced
// ============================================================
int test_merge_property_backdrop_filter() {
    std::cout << "[13] mergeProperty: backdrop-filter: syncs legacy blur scalar" << std::endl;

    Style s;
    StyleSheet::mergeProperty(s, "backdrop-filter", "blur(12px)");
    CHECK(s.hasBackdropFilter);
    CHECK(s.backdropFilterOperations.size() == 1);
    CHECK(s.backdropFilterOperations[0].type == FilterOperationType::Blur);
    // Legacy scalar must be synced
    CHECK(s.hasBackdropFilterBlur);
    CHECK_NEAR(s.backdropFilterBlur, 12.0f, 1e-4f);

    // backdrop-filter: none clears everything
    StyleSheet::mergeProperty(s, "backdrop-filter", "none");
    CHECK(!s.hasBackdropFilter);
    CHECK(s.backdropFilterOperations.empty());
    CHECK(!s.hasBackdropFilterBlur);
    CHECK_NEAR(s.backdropFilterBlur, 0.0f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [14] backdrop-filter: multiple functions — blur + brightness
// ============================================================
int test_backdrop_filter_multi() {
    std::cout << "[14] backdrop-filter: blur() brightness() — multi-function" << std::endl;

    Style s;
    StyleSheet::mergeProperty(s, "backdrop-filter", "blur(16px) brightness(0.6)");
    CHECK(s.hasBackdropFilter);
    CHECK(s.backdropFilterOperations.size() == 2);
    CHECK(s.backdropFilterOperations[0].type == FilterOperationType::Blur);
    CHECK_NEAR(s.backdropFilterOperations[0].amount, 16.0f, 1e-4f);
    CHECK(s.backdropFilterOperations[1].type == FilterOperationType::Brightness);
    CHECK_NEAR(s.backdropFilterOperations[1].amount, 0.6f, 1e-4f);
    // Legacy scalar points to the first blur
    CHECK(s.hasBackdropFilterBlur);
    CHECK_NEAR(s.backdropFilterBlur, 16.0f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [15] Full StyleSheet round-trip: filter + backdrop-filter in CSS text
// ============================================================
int test_full_stylesheet_parse() {
    std::cout << "[15] Full CSS parse: filter + backdrop-filter in .glass rule" << std::endl;

    StyleSheet sheet;
    sheet.parse(
        ".glass {"
        "  filter: blur(2px) drop-shadow(0px 4px 8px rgba(0,0,0,0.5));"
        "  backdrop-filter: blur(20px) saturate(180%);"
        "}");

    auto widget = std::make_shared<Panel>("glass");
    Style s = sheet.resolve("glass", "", "panel", {}, nullptr, widget.get());

    // filter:
    CHECK(s.hasFilter);
    CHECK(s.filterOperations.size() == 2);
    CHECK(s.filterOperations[0].type == FilterOperationType::Blur);
    CHECK_NEAR(s.filterOperations[0].amount, 2.0f, 1e-4f);
    CHECK(s.filterOperations[1].type == FilterOperationType::DropShadow);
    CHECK_NEAR(s.filterOperations[1].shadowOffsetY, 4.0f, 1e-4f);
    CHECK_NEAR(s.filterOperations[1].shadowBlur,    8.0f, 1e-4f);

    // backdrop-filter:
    CHECK(s.hasBackdropFilter);
    CHECK(s.backdropFilterOperations.size() == 2);
    CHECK(s.backdropFilterOperations[0].type == FilterOperationType::Blur);
    CHECK_NEAR(s.backdropFilterOperations[0].amount, 20.0f, 1e-4f);
    CHECK(s.backdropFilterOperations[1].type == FilterOperationType::Saturate);
    CHECK_NEAR(s.backdropFilterOperations[1].amount, 1.8f, 1e-3f);
    // Legacy scalar
    CHECK(s.hasBackdropFilterBlur);
    CHECK_NEAR(s.backdropFilterBlur, 20.0f, 1e-4f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// [16] filter: none resets previously set filter
// ============================================================
int test_filter_none_resets() {
    std::cout << "[16] filter: none resets a previously parsed filter" << std::endl;

    StyleSheet sheet;
    // First rule sets a filter
    sheet.parse(
        ".a { filter: blur(5px); }"
        ".b { filter: none; }");

    auto wa = std::make_shared<Panel>("a");
    Style sa = sheet.resolve("a", "", "panel", {}, nullptr, wa.get());
    CHECK(sa.hasFilter);
    CHECK(sa.filterOperations.size() == 1);

    auto wb = std::make_shared<Panel>("b");
    Style sb = sheet.resolve("b", "", "panel", {}, nullptr, wb.get());
    CHECK(!sb.hasFilter);
    CHECK(sb.filterOperations.empty());

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ============================================================
// main
// ============================================================
int main() {
    std::cout << "=== FluxUI CSS Filter (filter + backdrop-filter) Parity Tests ===" << std::endl;
    int rc = 0;
    rc |= test_none_and_empty();
    rc |= test_blur();
    rc |= test_amount_functions();
    rc |= test_color_matrix_functions();
    rc |= test_hue_rotate();
    rc |= test_drop_shadow_hex();
    rc |= test_drop_shadow_rgb();
    rc |= test_drop_shadow_named_color_no_blur();
    rc |= test_url_reference();
    rc |= test_multiple_functions();
    rc |= test_complex_chain();
    rc |= test_merge_property_filter();
    rc |= test_merge_property_backdrop_filter();
    rc |= test_backdrop_filter_multi();
    rc |= test_full_stylesheet_parse();
    rc |= test_filter_none_resets();
    if (rc == 0)
        std::cout << "\nAll filter tests passed!" << std::endl;
    else
        std::cerr << "\nSome filter tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}

// ============================================================
// Tests for the new features: calc(), luminance-to-alpha,
// color-matrix, and UseCounter (Blink parity additions).
// ============================================================

// ── [17] calc() in brightness ──────────────────────────────
int test_calc_brightness() {
    std::cout << "[17] filter: brightness(calc(0.5 + 0.3))" << std::endl;
    auto ops = StyleSheet::parseFilterOperations("brightness(calc(0.5 + 0.3))");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::Brightness);
    CHECK_NEAR(ops[0].amount, 0.8f, 1e-3f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [18] calc() clamped to [0,1] for grayscale ─────────────
int test_calc_clamp_grayscale() {
    std::cout << "[18] filter: grayscale(calc(0.5 + 0.8)) — clamped [0,1]" << std::endl;
    // 0.5 + 0.8 = 1.3 → clamped to 1.0
    auto ops = StyleSheet::parseFilterOperations("grayscale(calc(0.5 + 0.8))");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::Grayscale);
    CHECK_NEAR(ops[0].amount, 1.0f, 1e-4f); // clamped
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [19] calc() NOT clamped for brightness (may exceed 1) ──
int test_calc_no_clamp_brightness() {
    std::cout << "[19] filter: brightness(calc(1 + 0.5)) — NOT clamped" << std::endl;
    auto ops = StyleSheet::parseFilterOperations("brightness(calc(1 + 0.5))");
    CHECK(ops.size() == 1);
    CHECK_NEAR(ops[0].amount, 1.5f, 1e-3f); // NOT clamped to 1
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [20] calc() in blur ─────────────────────────────────────
int test_calc_blur() {
    std::cout << "[20] filter: blur(calc(2px + 2px))" << std::endl;
    auto ops = StyleSheet::parseFilterOperations("blur(calc(2px + 2px))");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::Blur);
    CHECK_NEAR(ops[0].amount, 4.0f, 1e-3f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [21] calc() in opacity clamped [0,1] ───────────────────
int test_calc_clamp_opacity() {
    std::cout << "[21] filter: opacity(calc(-0.5)) — clamped [0,1]" << std::endl;
    auto ops = StyleSheet::parseFilterOperations("opacity(calc(-0.5))");
    CHECK(ops.size() == 1);
    CHECK_NEAR(ops[0].amount, 0.0f, 1e-4f); // clamped to 0
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [22] luminance-to-alpha() ───────────────────────────────
int test_luminance_to_alpha() {
    std::cout << "[22] filter: luminance-to-alpha() — Blink kLuminanceToAlpha" << std::endl;
    auto ops = StyleSheet::parseFilterOperations("luminance-to-alpha()");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::LuminanceToAlpha);
    CHECK_NEAR(ops[0].amount, 0.0f, 1e-6f); // unused, always 0
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [23] color-matrix(<20 values>) ─────────────────────────
int test_color_matrix_basic() {
    std::cout << "[23] filter: color-matrix(identity 20 values)" << std::endl;
    // Identity matrix: 1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 1 0
    auto ops = StyleSheet::parseFilterOperations(
        "color-matrix(1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 1 0)");
    CHECK(ops.size() == 1);
    CHECK(ops[0].type == FilterOperationType::ColorMatrix);
    CHECK(ops[0].colorMatrixValues.size() == 20);
    // Row 0: R' = 1*R + 0*G + 0*B + 0*A + 0
    CHECK_NEAR(ops[0].colorMatrixValues[0],  1.0f, 1e-4f);
    CHECK_NEAR(ops[0].colorMatrixValues[1],  0.0f, 1e-4f);
    // Row 1, col 1: G'= 1*G
    CHECK_NEAR(ops[0].colorMatrixValues[6],  1.0f, 1e-4f);
    // Row 3, col 3: A'= 1*A
    CHECK_NEAR(ops[0].colorMatrixValues[18], 1.0f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [24] color-matrix with fewer than 20 values — padded ───
int test_color_matrix_padded() {
    std::cout << "[24] filter: color-matrix(<5 values>) — padded to 20" << std::endl;
    auto ops = StyleSheet::parseFilterOperations("color-matrix(1 0 0 0 0)");
    CHECK(ops.size() == 1);
    CHECK(ops[0].colorMatrixValues.size() == 20);
    CHECK_NEAR(ops[0].colorMatrixValues[0], 1.0f, 1e-4f);
    CHECK_NEAR(ops[0].colorMatrixValues[5], 0.0f, 1e-4f); // padded
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [25] UseCounter: FilterUseCounter ──────────────────────
int test_use_counter() {
    std::cout << "[25] UseCounter: FilterUseCounter tracks feature usage" << std::endl;

    // Reset the global counter
    FilterUseCounter::instance().reset();

    StyleSheet::parseFilterOperations("blur(4px) brightness(0.8) grayscale(1)");
    CHECK(FilterUseCounter::instance().get(FilterFeature::Blur)       == 1);
    CHECK(FilterUseCounter::instance().get(FilterFeature::Brightness) == 1);
    CHECK(FilterUseCounter::instance().get(FilterFeature::Grayscale)  == 1);
    CHECK(FilterUseCounter::instance().get(FilterFeature::Contrast)   == 0); // not used

    // Parsing again accumulates (same as Blink per-document counting)
    StyleSheet::parseFilterOperations("blur(2px)");
    CHECK(FilterUseCounter::instance().get(FilterFeature::Blur) == 2);

    // luminance-to-alpha and color-matrix
    StyleSheet::parseFilterOperations("luminance-to-alpha() color-matrix(1 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 0 1 0)");
    CHECK(FilterUseCounter::instance().get(FilterFeature::LuminanceToAlpha) == 1);
    CHECK(FilterUseCounter::instance().get(FilterFeature::ColorMatrix)      == 1);

    FilterUseCounter::instance().reset();
    CHECK(FilterUseCounter::instance().get(FilterFeature::Blur) == 0);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── Append new tests to main ────────────────────────────────
// NOTE: the main() above already runs tests [1]-[16].
// This runs the additional tests as a separate entry point
// that is called from a renamed main wrapper below.
int run_extended_tests() {
    int rc = 0;
    rc |= test_calc_brightness();
    rc |= test_calc_clamp_grayscale();
    rc |= test_calc_no_clamp_brightness();
    rc |= test_calc_blur();
    rc |= test_calc_clamp_opacity();
    rc |= test_luminance_to_alpha();
    rc |= test_color_matrix_basic();
    rc |= test_color_matrix_padded();
    rc |= test_use_counter();
    return rc;
}
