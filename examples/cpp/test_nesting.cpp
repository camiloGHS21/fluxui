// FluxUI CSS Nesting Level 1 Parity Test
// Mirrors Blink's CSSNestingType / CSS Nesting spec behavior.
//
// Reference: https://www.w3.org/TR/css-nesting-1/
//            chromium/src/third_party/blink/renderer/core/css/parser/

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL: " << #cond << " @ line " << __LINE__ << std::endl; return 1; } } while(0)
#define CHECK_NEAR(a, b, eps) do { float _a=(float)(a), _b=(float)(b); if(std::abs(_a-_b)>(float)(eps)){std::cerr<<"FAIL: "<<#a<<"="<<_a<<" ~= "<<#b<<"="<<_b<<" @ line "<<__LINE__<<std::endl; return 1;}}while(0)

static bool hasBg(const Style& s) { return s.backgroundColor.a > 0.01f; }

// ── [1] Basic & nesting ──────────────────────────────────────
int test_basic_ampersand() {
    std::cout << "[1] .card { &:hover { bg } }" << std::endl;
    StyleSheet sheet;
    sheet.parse(".card { &.active { background-color: #ff0000; } }");
    // .card.active should match
    auto w = std::make_shared<Panel>("card active");
    Style s = sheet.resolve("card active", "", "panel", {}, nullptr, w.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f);
    // .card alone should NOT match
    auto w2 = std::make_shared<Panel>("card");
    Style s2 = sheet.resolve("card", "", "panel", {}, nullptr, w2.get());
    CHECK(!hasBg(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] Implicit descendant nesting (no &) ───────────────────
int test_implicit_descendant() {
    std::cout << "[2] .card { .title { bg } } — implicit descendant" << std::endl;
    StyleSheet sheet;
    sheet.parse(".card { .title { background-color: #00ff00; } }");
    // .title inside .card → .card .title
    auto card = std::make_shared<Panel>("card");
    auto title = std::make_shared<Panel>("title");
    title->parent = card.get();
    card->children.push_back(title);
    std::vector<CSSSelectorNode> ancestors = {{"card", "", "panel", card.get()}};
    Style s = sheet.resolve("title", "", "panel", ancestors, nullptr, title.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f);
    // .title without .card ancestor → no match
    auto orphan = std::make_shared<Panel>("title");
    Style s2 = sheet.resolve("title", "", "panel", {}, nullptr, orphan.get());
    CHECK(!hasBg(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] Multiple & in selector ──────────────────────────────
int test_multiple_ampersand() {
    std::cout << "[3] .a { & + & { bg } } — multiple &" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { & + & { background-color: #0000ff; } }");
    // This generates ".a + .a"
    // Check the rule exists with the right selector
    bool found = false;
    for (const auto& r : sheet.rules) {
        if (r.selector.find(".a + .a") != std::string::npos) {
            found = true; break;
        }
    }
    CHECK(found);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] Deep nesting (3 levels) ─────────────────────────────
int test_deep_nesting() {
    std::cout << "[4] .a { .b { .c { bg } } } — deep nesting" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { .b { .c { background-color: #ff00ff; } } }");
    // Should produce rule: ".a .b .c"
    bool found = false;
    for (const auto& r : sheet.rules) {
        if (r.selector.find(".a .b .c") != std::string::npos) {
            found = true; break;
        }
    }
    CHECK(found);
    // Verify actual matching
    auto a = std::make_shared<Panel>("a");
    auto b = std::make_shared<Panel>("b"); b->parent = a.get(); a->children.push_back(b);
    auto c = std::make_shared<Panel>("c"); c->parent = b.get(); b->children.push_back(c);
    std::vector<CSSSelectorNode> ancestors = {
        {"b", "", "panel", b.get()},
        {"a", "", "panel", a.get()}
    };
    Style s = sheet.resolve("c", "", "panel", ancestors, nullptr, c.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f);
    CHECK_NEAR(s.backgroundColor.b, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] Comma-separated parent + nesting ────────────────────
int test_comma_parent() {
    std::cout << "[5] .a, .b { &.active { bg } }" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a, .b { &.active { background-color: #ffff00; } }");
    // Should produce: ".a.active, .b.active"
    auto wa = std::make_shared<Panel>("a active");
    auto wb = std::make_shared<Panel>("b active");
    auto wc = std::make_shared<Panel>("c active");
    Style sa = sheet.resolve("a active", "", "panel", {}, nullptr, wa.get());
    Style sb = sheet.resolve("b active", "", "panel", {}, nullptr, wb.get());
    Style sc = sheet.resolve("c active", "", "panel", {}, nullptr, wc.get());
    CHECK(hasBg(sa));
    CHECK(hasBg(sb));
    CHECK(!hasBg(sc));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] Nested @media ───────────────────────────────────────
int test_nested_media() {
    std::cout << "[6] .card { @media (min-width: 800px) { bg } } — nested at-rule" << std::endl;
    StyleSheet sheet;
    sheet.setViewportSize(1024, 768);
    sheet.parse(".card { @media (min-width: 800px) { background-color: #00ffff; } }");
    // viewport=1024 >= 800 → should match
    auto w = std::make_shared<Panel>("card");
    Style s = sheet.resolve("card", "", "panel", {}, nullptr, w.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f);
    CHECK_NEAR(s.backgroundColor.b, 1.0f, 1e-2f);

    // Shrink viewport below threshold
    StyleSheet sheet2;
    sheet2.setViewportSize(600, 768);
    sheet2.parse(".card { @media (min-width: 800px) { background-color: #00ffff; } }");
    Style s2 = sheet2.resolve("card", "", "panel", {}, nullptr, w.get());
    CHECK(!hasBg(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] & in pseudo-class position ──────────────────────────
int test_ampersand_pseudo() {
    std::cout << "[7] .btn { &:first-child { bg } }" << std::endl;
    StyleSheet sheet;
    sheet.parse(".btn { &:first-child { background-color: #ff6600; } }");
    // Should produce ".btn:first-child"
    auto parent = std::make_shared<Panel>("list");
    auto first = std::make_shared<Panel>("btn"); first->parent = parent.get();
    auto second = std::make_shared<Panel>("btn"); second->parent = parent.get();
    parent->children = {first, second};
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};
    Style s1 = sheet.resolve("btn", "", "panel", ancestors, nullptr, first.get());
    Style s2 = sheet.resolve("btn", "", "panel", ancestors, nullptr, second.get());
    CHECK(hasBg(s1));
    CHECK(!hasBg(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] Properties before and after nested rules ────────────
int test_properties_alongside_nested() {
    std::cout << "[8] .card { color: ...; .title { bg }; opacity: ... }" << std::endl;
    StyleSheet sheet;
    sheet.parse(".card { background-color: #aabbcc; .title { background-color: #112233; } }");
    // .card should get its own background
    auto card = std::make_shared<Panel>("card");
    Style sc = sheet.resolve("card", "", "panel", {}, nullptr, card.get());
    CHECK(hasBg(sc));
    CHECK_NEAR(sc.backgroundColor.r, 0xaa/255.0f, 1e-2f);
    // .title inside .card should get its own
    auto title = std::make_shared<Panel>("title");
    title->parent = card.get();
    card->children.push_back(title);
    std::vector<CSSSelectorNode> ancestors = {{"card", "", "panel", card.get()}};
    Style st = sheet.resolve("title", "", "panel", ancestors, nullptr, title.get());
    CHECK(hasBg(st));
    CHECK_NEAR(st.backgroundColor.r, 0x11/255.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI CSS Nesting Level 1 Parity Tests ===" << std::endl;
    int rc = 0;
    rc |= test_basic_ampersand();
    rc |= test_implicit_descendant();
    rc |= test_multiple_ampersand();
    rc |= test_deep_nesting();
    rc |= test_comma_parent();
    rc |= test_nested_media();
    rc |= test_ampersand_pseudo();
    rc |= test_properties_alongside_nested();
    if (rc == 0)
        std::cout << "\nAll CSS nesting tests passed!" << std::endl;
    else
        std::cerr << "\nSome tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
