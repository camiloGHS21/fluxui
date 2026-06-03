// FluxUI Advanced Cascade Tests: revert, revert-layer, @import, !important
// Mirrors Blink's StyleCascade / CSSImportRule / cascade origin ordering.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
#define CHECK_NEAR(a,b,eps) do{float _a=(float)(a),_b=(float)(b);if(std::abs(_a-_b)>(float)(eps)){std::cerr<<"FAIL: "<<#a<<"="<<_a<<" ~= "<<#b<<"="<<_b<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

static bool hasBg(const Style& s) { return s.backgroundColor.a > 0.01f; }

// ── [1] revert keyword — falls back to initial for non-inherited ──
int test_revert_non_inherited() {
    std::cout << "[1] revert: non-inherited property → initial" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { background-color: #ff0000; }"
               ".box.reset { background-color: revert; }");
    auto w = std::make_shared<Panel>("box reset");
    Style s = sheet.resolve("box reset", "", "panel", {}, nullptr, w.get());
    // revert on non-inherited → initial (transparent)
    CHECK(!hasBg(s));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] revert keyword — inherits for inherited property ────
int test_revert_inherited() {
    std::cout << "[2] revert: inherited property → parent value" << std::endl;
    StyleSheet sheet;
    sheet.parse(".parent { font-size: 24px; }"
               ".child { font-size: 12px; }"
               ".child.reset { font-size: revert; }");
    Style parentStyle;
    parentStyle.fontSize = 24.0f;
    auto w = std::make_shared<Panel>("child reset");
    Style s = sheet.resolve("child reset", "", "panel", {}, &parentStyle, w.get());
    // revert on inherited → parent's value (24px)
    CHECK_NEAR(s.fontSize, 24.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] revert-layer keyword ────────────────────────────────
int test_revert_layer() {
    std::cout << "[3] revert-layer: falls back like revert" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { background-color: #ff0000; }"
               ".box.reset { background-color: revert-layer; }");
    auto w = std::make_shared<Panel>("box reset");
    Style s = sheet.resolve("box reset", "", "panel", {}, nullptr, w.get());
    CHECK(!hasBg(s));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] inherit keyword ─────────────────────────────────────
int test_inherit() {
    std::cout << "[4] inherit: color from parent" << std::endl;
    StyleSheet sheet;
    sheet.parse(".child { color: inherit; }");
    Style parentStyle;
    parentStyle.color = Color(1, 0, 0, 1);
    parentStyle.hasColor = true;
    auto w = std::make_shared<Panel>("child");
    Style s = sheet.resolve("child", "", "panel", {}, &parentStyle, w.get());
    CHECK(s.hasColor);
    CHECK_NEAR(s.color.r, 1.0f, 1e-2f);
    CHECK_NEAR(s.color.g, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] initial keyword ─────────────────────────────────────
int test_initial() {
    std::cout << "[5] initial: resets to default" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { background-color: #ff0000; }"
               ".box.reset { background-color: initial; }");
    auto w = std::make_shared<Panel>("box reset");
    Style s = sheet.resolve("box reset", "", "panel", {}, nullptr, w.get());
    CHECK(!hasBg(s)); // initial → transparent
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] unset: inherited → inherits, non-inherited → initial ──
int test_unset() {
    std::cout << "[6] unset: font-size inherits, background resets" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { font-size: 12px; background-color: #ff0000; }"
               ".box.reset { font-size: unset; background-color: unset; }");
    Style parentStyle;
    parentStyle.fontSize = 20.0f;
    auto w = std::make_shared<Panel>("box reset");
    Style s = sheet.resolve("box reset", "", "panel", {}, &parentStyle, w.get());
    CHECK_NEAR(s.fontSize, 20.0f, 1e-2f); // inherited → parent
    CHECK(!hasBg(s)); // non-inherited → initial (transparent)
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] !important beats normal ─────────────────────────────
int test_important() {
    std::cout << "[7] !important beats higher specificity" << std::endl;
    StyleSheet sheet;
    sheet.parse("#mybox { background-color: #ff0000; }"
               ".box { background-color: #00ff00 !important; }");
    auto w = std::make_shared<Panel>("box");
    w->id = "mybox";
    Style s = sheet.resolve("box", "mybox", "panel", {}, nullptr, w.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f); // !important wins
    CHECK_NEAR(s.backgroundColor.r, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] @import is stored in rules ──────────────────────────
int test_import_parsed() {
    std::cout << "[8] @import url(...) parsed into rules" << std::endl;
    StyleSheet sheet;
    sheet.parse("@import url(\"base.css\");\n"
               "@import \"theme.css\" layer(theme);\n"
               "@import url('print.css') print;\n"
               ".box { color: red; }");
    // Check import rules were recorded
    int importCount = 0;
    bool foundBase = false, foundTheme = false, foundPrint = false;
    for (const auto& r : sheet.rules) {
        if (r.selector == "@import") {
            importCount++;
            if (!r.properties.empty()) {
                std::string url = r.properties[0].value;
                if (url == "base.css") foundBase = true;
                if (url == "theme.css") { foundTheme = true; CHECK(!r.layer.empty()); }
                if (url == "print.css") { foundPrint = true; CHECK(r.mediaQuery == "print"); }
            }
        }
    }
    CHECK(importCount == 3);
    CHECK(foundBase);
    CHECK(foundTheme);
    CHECK(foundPrint);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [9] @import ordering — imports come before other rules ──
int test_import_ordering() {
    std::cout << "[9] @import order preserved (relative ordering)" << std::endl;
    StyleSheet sheet;
    sheet.parse("@import \"a.css\";\n@import \"b.css\";\n.x { color: red; }");
    // Find the two import rules and verify their relative order
    int aIdx = -1, bIdx = -1, xIdx = -1;
    for (size_t i = 0; i < sheet.rules.size(); i++) {
        if (sheet.rules[i].selector == "@import" && !sheet.rules[i].properties.empty()) {
            if (sheet.rules[i].properties[0].value == "a.css") aIdx = (int)i;
            if (sheet.rules[i].properties[0].value == "b.css") bIdx = (int)i;
        }
        if (sheet.rules[i].selector.find(".x") != std::string::npos) xIdx = (int)i;
    }
    CHECK(aIdx >= 0);
    CHECK(bIdx >= 0);
    CHECK(xIdx >= 0);
    CHECK(aIdx < bIdx); // a.css before b.css (source order)
    CHECK(bIdx < xIdx); // imports before .x rule
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [10] Layer ordering with !important reversal ────────────
int test_layer_important_reversal() {
    std::cout << "[10] Layers: !important reverses layer order" << std::endl;
    StyleSheet sheet;
    sheet.parse("@layer base, override;\n"
               "@layer base { .box { background-color: #ff0000 !important; } }\n"
               "@layer override { .box { background-color: #00ff00 !important; } }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // In normal: override wins. In !important: layer order is reversed → base wins.
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f); // base !important wins
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Advanced Cascade Tests ===" << std::endl;
    int rc = 0;
    rc |= test_revert_non_inherited();
    rc |= test_revert_inherited();
    rc |= test_revert_layer();
    rc |= test_inherit();
    rc |= test_initial();
    rc |= test_unset();
    rc |= test_important();
    rc |= test_import_parsed();
    rc |= test_import_ordering();
    rc |= test_layer_important_reversal();
    if (rc == 0)
        std::cout << "\nAll cascade tests passed!" << std::endl;
    else
        std::cerr << "\nSome tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
