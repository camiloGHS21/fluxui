// FluxUI numeric font-weight test (CSS Fonts L4 §2.2).
// Before: FontWeight was {Normal, Bold} only — 100..900 collapsed to bold/normal
// at a 600 threshold. Now FontWeight is the full numeric ladder 100–900 with
// Normal==400 / Bold==700 aliases, plus bolder/lighter resolution.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

static FontWeight weightOf(const std::string& css) {
    StyleSheet sheet;
    sheet.parse(".x { " + css + " }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x", "", "panel", {}, nullptr, w.get());
    return s.fontWeight;
}

// ── [1] numeric weights map to the right buckets ──
int test_numeric() {
    std::cout << "[1] numeric weights 100..900" << std::endl;
    CHECK(weightOf("font-weight: 100") == FontWeight::Thin);
    CHECK(weightOf("font-weight: 200") == FontWeight::ExtraLight);
    CHECK(weightOf("font-weight: 300") == FontWeight::Light);
    CHECK(weightOf("font-weight: 400") == FontWeight::Normal);
    CHECK(weightOf("font-weight: 500") == FontWeight::Medium);
    CHECK(weightOf("font-weight: 600") == FontWeight::SemiBold);
    CHECK(weightOf("font-weight: 700") == FontWeight::Bold);
    CHECK(weightOf("font-weight: 800") == FontWeight::ExtraBold);
    CHECK(weightOf("font-weight: 900") == FontWeight::Black);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] keywords normal/bold ──
int test_keywords() {
    std::cout << "[2] normal / bold keywords" << std::endl;
    CHECK(weightOf("font-weight: normal") == FontWeight::Normal);  // 400
    CHECK(weightOf("font-weight: bold") == FontWeight::Bold);      // 700
    CHECK(fontWeightValue(FontWeight::Normal) == 400);
    CHECK(fontWeightValue(FontWeight::Bold) == 700);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] isBoldWeight threshold (>=600) drives bold synthesis ──
int test_bold_threshold() {
    std::cout << "[3] isBoldWeight() >= 600" << std::endl;
    CHECK(!isBoldWeight(FontWeight::Normal));
    CHECK(!isBoldWeight(FontWeight::Medium));     // 500
    CHECK(isBoldWeight(FontWeight::SemiBold));    // 600
    CHECK(isBoldWeight(FontWeight::Bold));        // 700
    CHECK(isBoldWeight(FontWeight::Black));       // 900
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] bolder / lighter relative keywords ──
int test_relative() {
    std::cout << "[4] bolder / lighter relative keywords" << std::endl;
    // Resolved during merge against the weight known at that point (initial 400).
    CHECK(weightOf("font-weight: bolder") == FontWeight::Bold);   // 400 -> 700
    CHECK(weightOf("font-weight: lighter") == FontWeight::Thin);  // 400 -> 100
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] font shorthand carries numeric weight ──
int test_shorthand() {
    std::cout << "[5] font shorthand: weight + size" << std::endl;
    CHECK(weightOf("font: 500 16px sans-serif") == FontWeight::Medium);
    CHECK(weightOf("font: bold 16px/1.4 serif") == FontWeight::Bold);
    // a bare size keyword must not be read as a weight
    CHECK(weightOf("font: medium serif") == FontWeight::Normal);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Numeric font-weight Tests ===" << std::endl;
    int rc = 0;
    rc |= test_numeric();
    rc |= test_keywords();
    rc |= test_bold_threshold();
    rc |= test_relative();
    rc |= test_shorthand();
    if (rc == 0) std::cout << "\nAll font-weight tests passed!" << std::endl;
    return rc;
}
