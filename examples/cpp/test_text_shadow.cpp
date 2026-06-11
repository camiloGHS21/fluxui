// FluxUI text-shadow test (CSS Text Decoration L3).
// Verifies parsing of single/multi-layer text-shadow (offset, blur, color in
// any order) and inheritance to descendants.

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

// ── [1] single shadow: offset-x offset-y blur color ──
int test_single() {
    std::cout << "[1] single text-shadow" << std::endl;
    Style s = resolveBox(".box { text-shadow: 2px 3px 4px #ff0000; }");
    CHECK(s.hasTextShadow);
    CHECK(s.textShadows.size() == 1);
    CHECK(nearEq(s.textShadows[0].offsetX, 2.0f));
    CHECK(nearEq(s.textShadows[0].offsetY, 3.0f));
    CHECK(nearEq(s.textShadows[0].blur, 4.0f));
    CHECK(nearEq(s.textShadows[0].color.r, 1.0f));
    CHECK(nearEq(s.textShadows[0].color.g, 0.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] no blur (offset only) ──
int test_no_blur() {
    std::cout << "[2] text-shadow without blur" << std::endl;
    Style s = resolveBox(".box { text-shadow: 1px 1px #000000; }");
    CHECK(s.hasTextShadow);
    CHECK(s.textShadows.size() == 1);
    CHECK(nearEq(s.textShadows[0].offsetX, 1.0f));
    CHECK(nearEq(s.textShadows[0].offsetY, 1.0f));
    CHECK(nearEq(s.textShadows[0].blur, 0.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] multiple comma-separated layers ──
int test_multi() {
    std::cout << "[3] multiple text-shadow layers" << std::endl;
    Style s = resolveBox(".box { text-shadow: 1px 1px 2px #ff0000, -1px -1px 2px #00ff00; }");
    CHECK(s.textShadows.size() == 2);
    CHECK(nearEq(s.textShadows[0].offsetX, 1.0f));
    CHECK(nearEq(s.textShadows[0].color.r, 1.0f));
    CHECK(nearEq(s.textShadows[1].offsetX, -1.0f));
    CHECK(nearEq(s.textShadows[1].color.g, 1.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] color-first ordering (rgb) ──
int test_color_first() {
    std::cout << "[4] color-first + rgb()" << std::endl;
    Style s = resolveBox(".box { text-shadow: rgb(0,0,255) 5px 5px 1px; }");
    CHECK(s.hasTextShadow);
    CHECK(s.textShadows.size() == 1);
    CHECK(nearEq(s.textShadows[0].offsetX, 5.0f));
    CHECK(nearEq(s.textShadows[0].color.b, 1.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] none clears ──
int test_none() {
    std::cout << "[5] text-shadow: none" << std::endl;
    Style s = resolveBox(".box { text-shadow: none; }");
    CHECK(!s.hasTextShadow);
    CHECK(s.textShadows.empty());
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] inheritance to descendants ──
int test_inherit() {
    std::cout << "[6] text-shadow inherits" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { text-shadow: 2px 2px 1px #ff0000; }");
    auto parent = std::make_shared<Panel>("box");
    parent->type = "panel";
    auto child = std::make_shared<Panel>("inner");
    child->type = "panel";
    child->parent = parent.get();
    parent->children.push_back(child);
    parent->resolveStyles(sheet);
    child->resolveStyles(sheet);
    CHECK(child->computedStyle->hasTextShadow);
    CHECK(child->computedStyle->textShadows.size() == 1);
    CHECK(nearEq(child->computedStyle->textShadows[0].offsetX, 2.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI text-shadow Tests ===" << std::endl;
    int rc = 0;
    rc |= test_single();
    rc |= test_no_blur();
    rc |= test_multi();
    rc |= test_color_first();
    rc |= test_none();
    rc |= test_inherit();
    if (rc == 0) std::cout << "\nAll text-shadow tests passed!" << std::endl;
    return rc;
}
