// FluxUI CSS inheritance test.
// Verifies the generalized (table-driven) inheritance in Widget::resolveStyles:
// every CSS-inherited property propagates parent -> child when the child does
// not set it. Before this change only ~10 properties were inherited even though
// many more were already part of computeInheritedHash().
//
// Reference: CSS Cascading & Inheritance L5 §3 (inheritance);
//            Blink: properties flagged `inherited: true` in css_properties.json5.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
static bool nearEq(float a, float b){ return std::abs(a-b) < 1e-3f; }

// Build parent(.box) > child(.inner) and resolve the whole tree against a sheet.
static std::shared_ptr<Panel> buildTree(const StyleSheet& sheet,
                                        const std::string& css,
                                        Widget*& outChild) {
    (void)css;
    auto parent = std::make_shared<Panel>("box");
    parent->type = "panel";
    auto child = std::make_shared<Panel>("inner");
    child->type = "panel";
    child->parent = parent.get();
    parent->children.push_back(child);

    // Resolve parent first, then child (so the child sees the parent's computed
    // style — the order resolveStyles relies on for inheritance).
    parent->resolveStyles(sheet);
    child->resolveStyles(sheet);
    outChild = child.get();
    return parent;
}

// ── [1] newly-generalized inherited properties propagate ──
int test_text_props_inherited() {
    std::cout << "[1] letter-spacing / word-spacing / white-space / "
                 "text-transform / word-break inherit" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box {"
                "  letter-spacing: 3px;"
                "  word-spacing: 5px;"
                "  white-space: nowrap;"
                "  text-transform: uppercase;"
                "  word-break: break-all;"
                "}");
    Widget* child = nullptr;
    auto parent = buildTree(sheet, "", child);
    const Style& c = child->computedStyle;

    CHECK(nearEq(c.letterSpacing, 3.0f));
    CHECK(nearEq(c.wordSpacing, 5.0f));
    CHECK(c.whiteSpace == WhiteSpace::NoWrap);
    CHECK(c.textTransform == TextTransform::Uppercase);
    CHECK(c.wordBreak == WordBreak::BreakAll);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] child override wins over inheritance ──
int test_child_override() {
    std::cout << "[2] child's own value overrides the inherited one" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box   { letter-spacing: 3px; text-transform: uppercase; }"
                ".inner { letter-spacing: 9px; }");
    Widget* child = nullptr;
    auto parent = buildTree(sheet, "", child);
    const Style& c = child->computedStyle;

    CHECK(nearEq(c.letterSpacing, 9.0f));               // own value
    CHECK(c.textTransform == TextTransform::Uppercase); // still inherited
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] direction inherits ──
int test_direction_inherited() {
    std::cout << "[3] direction inherits to descendants" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { direction: rtl; }");
    Widget* child = nullptr;
    auto parent = buildTree(sheet, "", child);
    CHECK(child->computedStyle->direction == Direction::Rtl);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] classic inherited props still work (color / font-size) ──
int test_classic_inherited() {
    std::cout << "[4] color / font-size / font-family still inherit" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { color: #00ff00; font-size: 20px; font-family: Inter; }");
    Widget* child = nullptr;
    auto parent = buildTree(sheet, "", child);
    const Style& c = child->computedStyle;

    CHECK(nearEq(c.color.g, 1.0f));
    CHECK(nearEq(c.fontSize, 20.0f));
    CHECK(c.fontFamily == "Inter");
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] non-inherited property does NOT propagate ──
int test_non_inherited() {
    std::cout << "[5] background-color does NOT inherit" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { background-color: #ff0000; }");
    Widget* child = nullptr;
    auto parent = buildTree(sheet, "", child);
    // child has no background → stays transparent (not inherited)
    CHECK(child->computedStyle->backgroundColor.a < 0.01f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Inheritance Tests ===" << std::endl;
    int rc = 0;
    rc |= test_text_props_inherited();
    rc |= test_child_override();
    rc |= test_direction_inherited();
    rc |= test_classic_inherited();
    rc |= test_non_inherited();
    if (rc == 0) std::cout << "\nAll inheritance tests passed!" << std::endl;
    return rc;
}
