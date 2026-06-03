// FluxUI @scope, @starting-style, @view-transition At-Rule Tests
// Mirrors Blink's CSSScopeRule, CSSStartingStyleRule, CSSViewTransitionRule.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
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
    float _a = (float)(a), _b = (float)(b); \
    if (std::abs(_a - _b) > (float)(eps)) { \
        std::cerr << "FAIL: " << #a << "=" << _a << " ~= " << #b << "=" << _b << " @ line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while (0)

static bool hasBg(const Style& s) { return s.backgroundColor.a > 0.01f; }

// ── [1] @scope basic — rules scoped to ancestor ────────────
int test_scope_basic() {
    std::cout << "[1] @scope (.card) { .title { ... } }" << std::endl;
    StyleSheet sheet;
    sheet.parse("@scope (.card) { .title { background-color: #ff0000; } }");

    // .title inside .card → matches
    auto card = std::make_shared<Panel>("card");
    auto title = std::make_shared<Panel>("title");
    title->parent = card.get();
    card->children.push_back(title);
    std::vector<CSSSelectorNode> ancestors = {{"card", "", "panel", card.get()}};
    Style s = sheet.resolve("title", "", "panel", ancestors, nullptr, title.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f);

    // .title WITHOUT .card ancestor → does NOT match
    auto orphan = std::make_shared<Panel>("title");
    Style s2 = sheet.resolve("title", "", "panel", {}, nullptr, orphan.get());
    CHECK(!hasBg(s2));

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] @scope with lower boundary (to) ────────────────────
int test_scope_limit() {
    std::cout << "[2] @scope (.card) to (.content) { ... }" << std::endl;
    StyleSheet sheet;
    sheet.parse("@scope (.card) to (.content) { .title { background-color: #00ff00; } }");

    // .title inside .card but BEFORE .content → matches
    auto card = std::make_shared<Panel>("card");
    auto title = std::make_shared<Panel>("title");
    title->parent = card.get();
    card->children.push_back(title);
    std::vector<CSSSelectorNode> a1 = {{"card", "", "panel", card.get()}};
    Style s1 = sheet.resolve("title", "", "panel", a1, nullptr, title.get());
    CHECK(hasBg(s1));

    // .title inside .content inside .card → scope limit hit, does NOT match
    auto content = std::make_shared<Panel>("content");
    content->parent = card.get();
    auto title2 = std::make_shared<Panel>("title");
    title2->parent = content.get();
    content->children.push_back(title2);
    card->children.push_back(content);
    // ancestors from title2's perspective: content, card
    std::vector<CSSSelectorNode> a2 = {
        {"content", "", "panel", content.get()},
        {"card", "", "panel", card.get()}
    };
    Style s2 = sheet.resolve("title", "", "panel", a2, nullptr, title2.get());
    CHECK(!hasBg(s2));

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] @starting-style rules stored separately ─────────────
int test_starting_style() {
    std::cout << "[3] @starting-style { .box { opacity: 0; } }" << std::endl;
    StyleSheet sheet;
    sheet.parse("@starting-style { .box { opacity: 0; background-color: #0000ff; } }"
               ".box { opacity: 1; }");

    // Normal resolve should NOT include @starting-style rules
    auto box = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, box.get());
    CHECK_NEAR(s.opacity, 1.0f, 1e-3f);
    CHECK(!hasBg(s)); // background from @starting-style should NOT be in main resolve

    // startingStyleRules should contain the rule
    CHECK(sheet.startingStyleRules.size() >= 1);
    bool found = false;
    for (const auto& r : sheet.startingStyleRules) {
        if (r.selector.find("box") != std::string::npos && r.isStartingStyle) {
            found = true;
        }
    }
    CHECK(found);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] @view-transition parsed ─────────────────────────────
int test_view_transition() {
    std::cout << "[4] @view-transition { navigation: auto; types: slide fade; }" << std::endl;
    StyleSheet sheet;
    sheet.parse("@view-transition { navigation: auto; types: slide fade; }");

    CHECK(sheet.viewTransition.navigation == "auto");
    CHECK(sheet.viewTransition.types.size() == 2);
    CHECK(sheet.viewTransition.types[0] == "slide");
    CHECK(sheet.viewTransition.types[1] == "fade");

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] @scope does not leak to unscoped rules ──────────────
int test_scope_no_leak() {
    std::cout << "[5] @scope rules don't affect unscoped elements" << std::endl;
    StyleSheet sheet;
    sheet.parse("@scope (.sidebar) { .link { background-color: #ff0000; } }"
               ".link { background-color: #00ff00; }");

    // .link without .sidebar ancestor → gets green (unscoped rule)
    auto link = std::make_shared<Panel>("link");
    Style s = sheet.resolve("link", "", "panel", {}, nullptr, link.get());
    CHECK(hasBg(s));
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f);
    CHECK_NEAR(s.backgroundColor.r, 0.0f, 1e-2f);

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] @view-transition: none ──────────────────────────────
int test_view_transition_none() {
    std::cout << "[6] @view-transition { navigation: none; }" << std::endl;
    StyleSheet sheet;
    sheet.parse("@view-transition { navigation: none; }");
    CHECK(sheet.viewTransition.navigation == "none");
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI @scope / @starting-style / @view-transition Tests ===" << std::endl;
    int rc = 0;
    rc |= test_scope_basic();
    rc |= test_scope_limit();
    rc |= test_starting_style();
    rc |= test_view_transition();
    rc |= test_scope_no_leak();
    rc |= test_view_transition_none();
    if (rc == 0)
        std::cout << "\nAll at-rule tests passed!" << std::endl;
    else
        std::cerr << "\nSome tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
