// FluxUI text-decoration sub-properties test (CSS Text Decoration L4).
// Covers text-decoration-style/-thickness, text-underline-offset and the
// shorthand carrying line + style.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
static bool nearEq(float a, float b){ return std::abs(a-b) < 0.01f; }

static Style resolveBox(const std::string& css) {
    StyleSheet sheet;
    sheet.parse(css);
    auto w = std::make_shared<Panel>("box");
    return sheet.resolve("box", "", "panel", {}, nullptr, w.get());
}

// ── [1] text-decoration-style ──
int test_style() {
    std::cout << "[1] text-decoration-style" << std::endl;
    Style s = resolveBox(".box { text-decoration: underline; text-decoration-style: wavy; }");
    CHECK(s.textDecoration == TextDecoration::Underline);
    CHECK(s.rare().textDecorationStyle == "wavy");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] thickness + offset ──
int test_thickness_offset() {
    std::cout << "[2] thickness + underline-offset" << std::endl;
    Style s = resolveBox(".box { text-decoration-thickness: 3px; text-underline-offset: 4px; }");
    CHECK(nearEq(s.rare().textDecorationThickness, 3.0f));
    CHECK(nearEq(s.rare().textUnderlineOffset, 4.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] auto values leave them unset (-1) ──
int test_auto() {
    std::cout << "[3] auto thickness/offset → unset" << std::endl;
    Style s = resolveBox(".box { text-decoration-thickness: auto; text-underline-offset: auto; }");
    CHECK(s.rare().textDecorationThickness < 0.0f);
    CHECK(s.rare().textUnderlineOffset < 0.0f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] shorthand carries line + style ──
int test_shorthand() {
    std::cout << "[4] shorthand: underline dotted" << std::endl;
    Style s = resolveBox(".box { text-decoration: underline dotted; }");
    CHECK(s.textDecoration == TextDecoration::Underline);
    CHECK(s.rare().textDecorationStyle == "dotted");

    Style s2 = resolveBox(".box { text-decoration: line-through; }");
    CHECK(s2.textDecoration == TextDecoration::LineThrough);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI text-decoration Tests ===" << std::endl;
    int rc = 0;
    rc |= test_style();
    rc |= test_thickness_offset();
    rc |= test_auto();
    rc |= test_shorthand();
    if (rc == 0) std::cout << "\nAll text-decoration tests passed!" << std::endl;
    return rc;
}
