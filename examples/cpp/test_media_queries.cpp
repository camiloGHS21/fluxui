// FluxUI @media query test (CSS Media Queries Level 4/5).
// Exercises the extended media-feature evaluator: width/height ranges, the
// modern two-sided range syntax, aspect-ratio, resolution, orientation,
// prefers-* user preferences, pointer/hover capabilities, and and/or/not/comma
// combinators.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
static bool hasBg(const Style& s){ return s.backgroundColor.a > 0.01f; }

// Parse `@media <query> { .box { background } }`, resolve .box, return matched.
static bool mq(const std::string& query, float vw, float vh,
               std::function<void(StyleSheet&)> setup = {}) {
    StyleSheet sheet;
    sheet.setViewportSize(vw, vh);
    if (setup) setup(sheet);
    sheet.parse("@media " + query + " { .box { background-color: #00ffff; } }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    return hasBg(s);
}

// ── [1] width ranges (min/max + modern <=) ──
int test_width() {
    std::cout << "[1] width ranges" << std::endl;
    CHECK(mq("(min-width: 800px)", 1024, 768));
    CHECK(!mq("(min-width: 800px)", 600, 768));
    CHECK(mq("(max-width: 800px)", 600, 768));
    CHECK(mq("(width <= 700px)", 600, 768));
    CHECK(!mq("(width <= 700px)", 900, 768));
    // two-sided range
    CHECK(mq("(400px <= width <= 700px)", 600, 768));
    CHECK(!mq("(400px <= width <= 700px)", 800, 768));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] orientation + aspect-ratio ──
int test_orientation_ratio() {
    std::cout << "[2] orientation + aspect-ratio" << std::endl;
    CHECK(mq("(orientation: landscape)", 1024, 768));
    CHECK(mq("(orientation: portrait)", 768, 1024));
    CHECK(mq("(min-aspect-ratio: 4/3)", 1600, 900));   // 16:9 >= 4:3
    CHECK(!mq("(min-aspect-ratio: 16/9)", 1024, 768)); // 4:3 < 16:9
    CHECK(mq("(max-aspect-ratio: 16/9)", 1024, 768));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] resolution ──
int test_resolution() {
    std::cout << "[3] resolution (dppx / dpi)" << std::endl;
    auto hidpi = [](StyleSheet& s){ s.setResolutionDppx(2.0f); };
    CHECK(mq("(min-resolution: 2dppx)", 800, 600, hidpi));
    CHECK(!mq("(min-resolution: 2dppx)", 800, 600));           // default 1dppx
    CHECK(mq("(min-resolution: 192dpi)", 800, 600, hidpi));    // 192dpi == 2dppx
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] prefers-* user preferences ──
int test_prefers() {
    std::cout << "[4] prefers-color-scheme / reduced-motion / contrast" << std::endl;
    auto dark = [](StyleSheet& s){ s.setColorScheme(StyleSheet::ColorScheme::Dark); };
    CHECK(mq("(prefers-color-scheme: dark)", 800, 600, dark));
    CHECK(!mq("(prefers-color-scheme: dark)", 800, 600));
    CHECK(mq("(prefers-color-scheme: light)", 800, 600));

    auto reduce = [](StyleSheet& s){ s.setPrefersReducedMotion(true); };
    CHECK(mq("(prefers-reduced-motion: reduce)", 800, 600, reduce));
    CHECK(mq("(prefers-reduced-motion: no-preference)", 800, 600));

    auto contrast = [](StyleSheet& s){ s.setPrefersContrast("more"); };
    CHECK(mq("(prefers-contrast: more)", 800, 600, contrast));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] pointer / hover capability ──
int test_pointer_hover() {
    std::cout << "[5] pointer / hover" << std::endl;
    CHECK(mq("(hover: hover)", 800, 600));               // default desktop
    CHECK(mq("(pointer: fine)", 800, 600));
    auto touch = [](StyleSheet& s){ s.setPointerType("coarse"); s.setHoverCapable(false); };
    CHECK(mq("(pointer: coarse)", 800, 600, touch));
    CHECK(mq("(hover: none)", 800, 600, touch));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] and / or / not / comma combinators ──
int test_combinators() {
    std::cout << "[6] and / or / not / comma" << std::endl;
    CHECK(mq("(min-width: 500px) and (max-width: 900px)", 700, 600));
    CHECK(!mq("(min-width: 500px) and (max-width: 900px)", 1000, 600));
    CHECK(mq("(max-width: 400px) or (min-width: 900px)", 1000, 600));
    CHECK(!mq("(max-width: 400px) or (min-width: 900px)", 600, 600));
    CHECK(mq("screen and (min-width: 500px)", 700, 600));
    CHECK(mq("not (min-width: 900px)", 600, 600));       // 600<900 → not matches
    CHECK(!mq("not (min-width: 500px)", 700, 600));
    // comma alternative list (OR)
    CHECK(mq("(max-width: 400px), (min-width: 600px)", 700, 600));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI @media Query Tests ===" << std::endl;
    int rc = 0;
    rc |= test_width();
    rc |= test_orientation_ratio();
    rc |= test_resolution();
    rc |= test_prefers();
    rc |= test_pointer_hover();
    rc |= test_combinators();
    if (rc == 0) std::cout << "\nAll @media query tests passed!" << std::endl;
    return rc;
}
