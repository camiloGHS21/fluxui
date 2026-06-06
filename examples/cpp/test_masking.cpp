// FluxUI Masking, Clip-Path, Blending CSS Properties Test
// Mirrors Blink's CSS Masking Level 1 / Compositing Level 1.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// ── [1] clip-path basic shapes ──────────────────────────────
int test_clip_path() {
    std::cout << "[1] clip-path: circle(), polygon(), url()" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { clip-path: circle(50%); }"
               ".b { clip-path: polygon(0% 0%, 100% 0%, 50% 100%); }"
               ".c { clip-path: url(#myClip); }"
               ".d { clip-path: none; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    auto wd = std::make_shared<Panel>("d");
    Style sa = sheet.resolve("a", "", "panel", {}, nullptr, wa.get());
    Style sb = sheet.resolve("b", "", "panel", {}, nullptr, wb.get());
    Style sc = sheet.resolve("c", "", "panel", {}, nullptr, wc.get());
    Style sd = sheet.resolve("d", "", "panel", {}, nullptr, wd.get());
    CHECK(sa.rare().hasClipPath);
    CHECK(sa.rare().clipPath == "circle(50%)");
    CHECK(sb.rare().hasClipPath);
    CHECK(sb.rare().clipPath.find("polygon") != std::string::npos);
    CHECK(sc.rare().hasClipPath);
    CHECK(sc.rare().clipPath == "url(#myClip)");
    CHECK(!sd.rare().hasClipPath);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] shape-outside ───────────────────────────────────────
int test_shape_outside() {
    std::cout << "[2] shape-outside" << std::endl;
    StyleSheet sheet;
    sheet.parse(".float { shape-outside: circle(50%); }");
    auto w = std::make_shared<Panel>("float");
    Style s = sheet.resolve("float", "", "panel", {}, nullptr, w.get());
    CHECK(s.rare().hasShapeOutside);
    CHECK(s.rare().shapeOutside == "circle(50%)");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] mask-image ──────────────────────────────────────────
int test_mask_image() {
    std::cout << "[3] mask-image / mask shorthand" << std::endl;
    StyleSheet sheet;
    sheet.parse(".masked { mask-image: url(mask.svg); }"
               ".masked2 { mask: url(star.png); }");
    auto w1 = std::make_shared<Panel>("masked");
    auto w2 = std::make_shared<Panel>("masked2");
    Style s1 = sheet.resolve("masked", "", "panel", {}, nullptr, w1.get());
    Style s2 = sheet.resolve("masked2", "", "panel", {}, nullptr, w2.get());
    CHECK(s1.rare().hasMask);
    CHECK(s1.rare().maskImage == "url(mask.svg)");
    CHECK(s2.rare().hasMask);
    CHECK(s2.rare().maskImage == "url(star.png)");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] mask sub-properties ─────────────────────────────────
int test_mask_sub_properties() {
    std::cout << "[4] mask-mode, mask-repeat, mask-position, mask-size, mask-clip, mask-origin, mask-composite" << std::endl;
    StyleSheet sheet;
    sheet.parse(".m { mask-mode: luminance; mask-repeat: no-repeat;"
               "     mask-position: center; mask-size: cover;"
               "     mask-clip: content-box; mask-origin: padding-box;"
               "     mask-composite: subtract; }");
    auto w = std::make_shared<Panel>("m");
    Style s = sheet.resolve("m", "", "panel", {}, nullptr, w.get());
    CHECK(s.rare().maskMode == "luminance");
    CHECK(s.rare().maskRepeat == "no-repeat");
    CHECK(s.rare().maskPosition == "center");
    CHECK(s.rare().maskSize == "cover");
    CHECK(s.rare().maskClip == "content-box");
    CHECK(s.rare().maskOrigin == "padding-box");
    CHECK(s.rare().maskComposite == "subtract");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] mix-blend-mode ──────────────────────────────────────
int test_mix_blend_mode() {
    std::cout << "[5] mix-blend-mode (all 16 modes)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { mix-blend-mode: multiply; }"
               ".b { mix-blend-mode: screen; }"
               ".c { mix-blend-mode: overlay; }"
               ".d { mix-blend-mode: color-dodge; }"
               ".e { mix-blend-mode: luminosity; }"
               ".f { mix-blend-mode: normal; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    auto wd = std::make_shared<Panel>("d");
    auto we = std::make_shared<Panel>("e");
    auto wf = std::make_shared<Panel>("f");
    Style sa = sheet.resolve("a", "", "panel", {}, nullptr, wa.get());
    Style sb = sheet.resolve("b", "", "panel", {}, nullptr, wb.get());
    Style sc = sheet.resolve("c", "", "panel", {}, nullptr, wc.get());
    Style sd = sheet.resolve("d", "", "panel", {}, nullptr, wd.get());
    Style se = sheet.resolve("e", "", "panel", {}, nullptr, we.get());
    Style sf = sheet.resolve("f", "", "panel", {}, nullptr, wf.get());
    CHECK(sa.rare().hasMixBlendMode && sa.rare().mixBlendMode == Style::BlendMode::Multiply);
    CHECK(sb.rare().hasMixBlendMode && sb.rare().mixBlendMode == Style::BlendMode::Screen);
    CHECK(sc.rare().hasMixBlendMode && sc.rare().mixBlendMode == Style::BlendMode::Overlay);
    CHECK(sd.rare().hasMixBlendMode && sd.rare().mixBlendMode == Style::BlendMode::ColorDodge);
    CHECK(se.rare().hasMixBlendMode && se.rare().mixBlendMode == Style::BlendMode::Luminosity);
    CHECK(!sf.rare().hasMixBlendMode); // "normal" resets
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] isolation ───────────────────────────────────────────
int test_isolation() {
    std::cout << "[6] isolation: auto | isolate" << std::endl;
    StyleSheet sheet;
    sheet.parse(".iso { isolation: isolate; }"
               ".def { isolation: auto; }");
    auto w1 = std::make_shared<Panel>("iso");
    auto w2 = std::make_shared<Panel>("def");
    Style s1 = sheet.resolve("iso", "", "panel", {}, nullptr, w1.get());
    Style s2 = sheet.resolve("def", "", "panel", {}, nullptr, w2.get());
    CHECK(s1.rare().hasIsolation && s1.rare().isolation == Style::Isolation::Isolate);
    CHECK(s2.rare().hasIsolation && s2.rare().isolation == Style::Isolation::Auto);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] background-blend-mode ───────────────────────────────
int test_background_blend_mode() {
    std::cout << "[7] background-blend-mode" << std::endl;
    StyleSheet sheet;
    sheet.parse(".bg { background-blend-mode: overlay; }");
    auto w = std::make_shared<Panel>("bg");
    Style s = sheet.resolve("bg", "", "panel", {}, nullptr, w.get());
    CHECK(s.rare().hasBackgroundBlendMode);
    CHECK(s.rare().backgroundBlendMode == Style::BlendMode::Overlay);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] clip-path with inset() ──────────────────────────────
int test_clip_path_inset() {
    std::cout << "[8] clip-path: inset(...)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { clip-path: inset(10px 20px 30px 40px round 5px); }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x", "", "panel", {}, nullptr, w.get());
    CHECK(s.rare().hasClipPath);
    CHECK(s.rare().clipPath.find("inset") != std::string::npos);
    CHECK(s.rare().clipPath.find("round") != std::string::npos);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Masking / Clip-Path / Blending Tests ===" << std::endl;
    int rc = 0;
    rc |= test_clip_path();
    rc |= test_shape_outside();
    rc |= test_mask_image();
    rc |= test_mask_sub_properties();
    rc |= test_mix_blend_mode();
    rc |= test_isolation();
    rc |= test_background_blend_mode();
    rc |= test_clip_path_inset();
    if (rc == 0)
        std::cout << "\nAll masking/blend tests passed!" << std::endl;
    else
        std::cerr << "\nSome tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
