// FluxUI Modern CSS Color Functions Parity Test
// Mirrors CSS Color Module Level 4/5 and Blink's color parsing.
//
// Reference: https://www.w3.org/TR/css-color-4/
//            https://www.w3.org/TR/css-color-5/
//            chromium/src/third_party/blink/renderer/core/css/parser/

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include <iostream>
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

// ── [1] hex + named + rgb + hsl (regression) ───────────────
int test_legacy_colors() {
    std::cout << "[1] legacy: hex / named / rgb() / hsl()" << std::endl;
    Color hex = StyleSheet::parseColor("#ff0000");
    CHECK_NEAR(hex.r, 1.0f, 1e-3f); CHECK_NEAR(hex.g, 0.0f, 1e-3f); CHECK_NEAR(hex.b, 0.0f, 1e-3f);

    Color named = StyleSheet::parseColor("rebeccapurple");
    CHECK_NEAR(named.r, 0x66/255.0f, 1e-3f);
    CHECK_NEAR(named.g, 0x33/255.0f, 1e-3f);
    CHECK_NEAR(named.b, 0x99/255.0f, 1e-3f);

    Color rgb = StyleSheet::parseColor("rgb(0, 128, 255)");
    CHECK_NEAR(rgb.r, 0.0f, 1e-3f);
    CHECK_NEAR(rgb.g, 128/255.0f, 1e-3f);
    CHECK_NEAR(rgb.b, 1.0f, 1e-3f);

    Color hsl = StyleSheet::parseColor("hsl(120, 100%, 50%)");
    CHECK_NEAR(hsl.r, 0.0f, 1e-2f);
    CHECK_NEAR(hsl.g, 1.0f, 1e-2f);
    CHECK_NEAR(hsl.b, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] oklch() ─────────────────────────────────────────────
int test_oklch() {
    std::cout << "[2] oklch()" << std::endl;
    // oklch(0.628 0.2577 29.23) is approximately sRGB red
    Color c = StyleSheet::parseColor("oklch(0.628 0.2577 29.23)");
    CHECK_NEAR(c.r, 1.0f, 0.05f);
    CHECK_NEAR(c.g, 0.0f, 0.08f);
    CHECK_NEAR(c.b, 0.0f, 0.08f);

    // White: oklch(1 0 0)
    Color w = StyleSheet::parseColor("oklch(1 0 0)");
    CHECK_NEAR(w.r, 1.0f, 0.02f);
    CHECK_NEAR(w.g, 1.0f, 0.02f);
    CHECK_NEAR(w.b, 1.0f, 0.02f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] oklab() ─────────────────────────────────────────────
int test_oklab() {
    std::cout << "[3] oklab()" << std::endl;
    // oklab(0 0 0) → black
    Color k = StyleSheet::parseColor("oklab(0 0 0)");
    CHECK_NEAR(k.r, 0.0f, 0.02f);
    CHECK_NEAR(k.g, 0.0f, 0.02f);
    CHECK_NEAR(k.b, 0.0f, 0.02f);

    // oklab(1 0 0) → white
    Color w = StyleSheet::parseColor("oklab(1 0 0)");
    CHECK_NEAR(w.r, 1.0f, 0.02f);
    CHECK_NEAR(w.g, 1.0f, 0.02f);
    CHECK_NEAR(w.b, 1.0f, 0.02f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] lab() ───────────────────────────────────────────────
int test_lab() {
    std::cout << "[4] lab()" << std::endl;
    // lab(100 0 0) → white
    Color w = StyleSheet::parseColor("lab(100 0 0)");
    CHECK_NEAR(w.r, 1.0f, 0.03f);
    CHECK_NEAR(w.g, 1.0f, 0.03f);
    CHECK_NEAR(w.b, 1.0f, 0.03f);

    // lab(0 0 0) → black
    Color k = StyleSheet::parseColor("lab(0 0 0)");
    CHECK_NEAR(k.r, 0.0f, 0.02f);
    CHECK_NEAR(k.g, 0.0f, 0.02f);
    CHECK_NEAR(k.b, 0.0f, 0.02f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] lch() ───────────────────────────────────────────────
int test_lch() {
    std::cout << "[5] lch()" << std::endl;
    Color w = StyleSheet::parseColor("lch(100 0 0)");
    CHECK_NEAR(w.r, 1.0f, 0.03f);
    CHECK_NEAR(w.g, 1.0f, 0.03f);
    CHECK_NEAR(w.b, 1.0f, 0.03f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] hwb() ───────────────────────────────────────────────
int test_hwb() {
    std::cout << "[6] hwb()" << std::endl;
    // hwb(0 0% 0%) → pure red
    Color r = StyleSheet::parseColor("hwb(0 0% 0%)");
    CHECK_NEAR(r.r, 1.0f, 1e-2f);
    CHECK_NEAR(r.g, 0.0f, 1e-2f);
    CHECK_NEAR(r.b, 0.0f, 1e-2f);

    // hwb(0 100% 0%) → white (whiteness maxed)
    Color w = StyleSheet::parseColor("hwb(0 100% 0%)");
    CHECK_NEAR(w.r, 1.0f, 1e-2f);
    CHECK_NEAR(w.g, 1.0f, 1e-2f);
    CHECK_NEAR(w.b, 1.0f, 1e-2f);

    // hwb(0 0% 100%) → black (blackness maxed)
    Color k = StyleSheet::parseColor("hwb(0 0% 100%)");
    CHECK_NEAR(k.r, 0.0f, 1e-2f);
    CHECK_NEAR(k.g, 0.0f, 1e-2f);
    CHECK_NEAR(k.b, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] color(srgb ...) ─────────────────────────────────────
int test_color_function_srgb() {
    std::cout << "[7] color(srgb 1 0 0)" << std::endl;
    Color c = StyleSheet::parseColor("color(srgb 1 0 0)");
    CHECK_NEAR(c.r, 1.0f, 1e-3f);
    CHECK_NEAR(c.g, 0.0f, 1e-3f);
    CHECK_NEAR(c.b, 0.0f, 1e-3f);

    Color half = StyleSheet::parseColor("color(srgb 0.5 0.5 0.5)");
    CHECK_NEAR(half.r, 0.5f, 1e-3f);
    CHECK_NEAR(half.g, 0.5f, 1e-3f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] color(display-p3 ...) ───────────────────────────────
int test_color_function_p3() {
    std::cout << "[8] color(display-p3 1 0 0)" << std::endl;
    // P3 red maps to a slightly clamped sRGB red (out of gamut → clamp to ~1,0,0)
    Color c = StyleSheet::parseColor("color(display-p3 1 0 0)");
    CHECK_NEAR(c.r, 1.0f, 0.05f);
    CHECK(c.g < 0.1f);
    CHECK(c.b < 0.1f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [9] color(srgb-linear ...) ──────────────────────────────
int test_color_function_srgb_linear() {
    std::cout << "[9] color(srgb-linear 1 1 1)" << std::endl;
    Color w = StyleSheet::parseColor("color(srgb-linear 1 1 1)");
    CHECK_NEAR(w.r, 1.0f, 1e-2f);
    CHECK_NEAR(w.g, 1.0f, 1e-2f);
    CHECK_NEAR(w.b, 1.0f, 1e-2f);
    // linear 0 → 0
    Color k = StyleSheet::parseColor("color(srgb-linear 0 0 0)");
    CHECK_NEAR(k.r, 0.0f, 1e-3f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [10] color-mix() ────────────────────────────────────────
int test_color_mix() {
    std::cout << "[10] color-mix(in srgb, white, black)" << std::endl;
    // 50/50 white+black → gray 0.5
    Color g = StyleSheet::parseColor("color-mix(in srgb, white, black)");
    CHECK_NEAR(g.r, 0.5f, 1e-2f);
    CHECK_NEAR(g.g, 0.5f, 1e-2f);
    CHECK_NEAR(g.b, 0.5f, 1e-2f);

    // 25% red + 75% black → 0.25 red
    Color q = StyleSheet::parseColor("color-mix(in srgb, red 25%, black)");
    CHECK_NEAR(q.r, 0.25f, 1e-2f);
    CHECK_NEAR(q.g, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [11] alpha channel via / syntax ─────────────────────────
int test_alpha_slash() {
    std::cout << "[11] oklch / rgb alpha via slash" << std::endl;
    Color c = StyleSheet::parseColor("oklch(0.628 0.2577 29.23 / 0.5)");
    CHECK_NEAR(c.a, 0.5f, 1e-2f);

    Color r = StyleSheet::parseColor("rgb(255 0 0 / 50%)");
    CHECK_NEAR(r.a, 0.5f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [12] system color keywords ──────────────────────────────
int test_system_colors() {
    std::cout << "[12] system colors: Canvas, CanvasText" << std::endl;
    Color canvas = StyleSheet::parseColor("Canvas");
    CHECK_NEAR(canvas.r, 1.0f, 1e-2f);
    Color text = StyleSheet::parseColor("CanvasText");
    CHECK_NEAR(text.r, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [13] Color::fromOklch direct conversion ─────────────────
int test_direct_oklch_conversion() {
    std::cout << "[13] Color::fromOklch direct API" << std::endl;
    Color w = Color::fromOklch(1.0f, 0.0f, 0.0f);
    CHECK_NEAR(w.r, 1.0f, 0.02f);
    CHECK_NEAR(w.g, 1.0f, 0.02f);
    CHECK_NEAR(w.b, 1.0f, 0.02f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Modern CSS Color Functions Parity Tests ===" << std::endl;
    int rc = 0;
    rc |= test_legacy_colors();
    rc |= test_oklch();
    rc |= test_oklab();
    rc |= test_lab();
    rc |= test_lch();
    rc |= test_hwb();
    rc |= test_color_function_srgb();
    rc |= test_color_function_p3();
    rc |= test_color_function_srgb_linear();
    rc |= test_color_mix();
    rc |= test_alpha_slash();
    rc |= test_system_colors();
    rc |= test_direct_oklch_conversion();
    if (rc == 0)
        std::cout << "\nAll color tests passed!" << std::endl;
    else
        std::cerr << "\nSome color tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
