// FluxUI Structural/Relational Pseudo-Class Parity Test
// Mirrors Blink's selector matching for :nth-child, :has, :not, :is, :where.
//
// Reference: https://www.w3.org/TR/selectors-4/
//            chromium/src/third_party/blink/renderer/core/css/selector_checker.cc

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
    float _a = static_cast<float>(a), _b = static_cast<float>(b); \
    if (std::abs(_a - _b) > static_cast<float>(eps)) { \
        std::cerr << "FAIL: " << #a << " (" << _a << ") ~= " << #b \
                  << " (" << _b << ") @ line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while (0)

// Helper to build a parent with N children of given classes
static std::shared_ptr<Panel> makeParentWithChildren(
    const std::string& parentCls,
    const std::vector<std::string>& childClasses) {
    auto parent = std::make_shared<Panel>(parentCls);
    for (const auto& cls : childClasses) {
        auto child = std::make_shared<Panel>(cls);
        child->parent = parent.get();
        parent->children.push_back(child);
    }
    return parent;
}

// Helper: check that a color is approximately red (#ff0000)
static bool isRed(const Style& s) { return s.hasColor && s.color.r > 0.9f && s.color.g < 0.1f && s.color.b < 0.1f; }
static bool isGreen(const Style& s) { return s.hasColor && s.color.g > 0.9f && s.color.r < 0.1f && s.color.b < 0.1f; }
static bool isBlue(const Style& s) { return s.hasColor && s.color.b > 0.9f && s.color.r < 0.1f && s.color.g < 0.1f; }
static bool isNotRed(const Style& s) { return !s.hasColor || s.color.g > 0.5f || s.color.r < 0.5f; }
static bool isNotGreen(const Style& s) { return !s.hasColor || s.color.r > 0.5f || s.color.g < 0.5f; }
static bool isNotBlue(const Style& s) { return !s.hasColor || s.color.r > 0.5f || s.color.b < 0.5f; }

// ── [1] :first-child ────────────────────────────────────────
int test_first_child() {
    std::cout << "[1] :first-child" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:first-child { color: #ff0000; }");

    auto parent = makeParentWithChildren("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style s1 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[1].get());
    CHECK(isRed(s1));    // 1st matches :first-child
    CHECK(isNotRed(s2)); // 2nd does not
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] :last-child ─────────────────────────────────────────
int test_last_child() {
    std::cout << "[2] :last-child" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:last-child { color: #00ff00; }");

    auto parent = makeParentWithChildren("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style sl = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[2].get());
    Style sf = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    CHECK(isGreen(sl));    // last matches
    CHECK(isNotGreen(sf)); // first does not
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] :nth-child(2) ───────────────────────────────────────
int test_nth_child_number() {
    std::cout << "[3] :nth-child(2)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-child(2) { color: #0000ff; }");

    auto parent = makeParentWithChildren("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style s1 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[2].get());
    CHECK(isNotBlue(s1));
    CHECK(isBlue(s2)); // blue, 1e-3f);
    CHECK(isNotBlue(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] :nth-child(odd) / :nth-child(even) ──────────────────
int test_nth_child_even_odd() {
    std::cout << "[4] :nth-child(odd) / :nth-child(even)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-child(odd) { color: #ff0000; }"
               ".item:nth-child(even) { color: #00ff00; }");

    auto parent = makeParentWithChildren("list", {"item", "item", "item", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style s1 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[2].get());
    Style s4 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[3].get());
    // 1=odd(red), 2=even(green), 3=odd(red), 4=even(green)
    CHECK(isRed(s1));
    CHECK(isGreen(s2));
    CHECK(isRed(s3));
    CHECK(isGreen(s4));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] :nth-child(2n+1) — same as odd ─────────────────────
int test_nth_child_formula() {
    std::cout << "[5] :nth-child(2n+1)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-child(2n+1) { color: #ff0000; }");

    auto parent = makeParentWithChildren("list", {"item", "item", "item", "item", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style s1 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[2].get());
    CHECK(isRed(s1)); // 1st = match
    CHECK(isNotRed(s2)); // 2nd = no match
    CHECK(isRed(s3)); // 3rd = match
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] :nth-last-child(1) — same as :last-child ───────────
int test_nth_last_child() {
    std::cout << "[6] :nth-last-child(1)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-last-child(1) { color: #0000ff; }");

    auto parent = makeParentWithChildren("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style sl = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[2].get());
    Style sf = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    CHECK(isBlue(sl));
    CHECK(isNotBlue(sf));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] :not(.excluded) ─────────────────────────────────────
int test_not() {
    std::cout << "[7] :not(.excluded)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:not(.excluded) { color: #ff0000; }");

    auto parent = makeParentWithChildren("list", {"item", "item excluded", "item"});
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};

    Style s1 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[0].get());
    Style s2 = sheet.resolve("item excluded", "", "panel", ancestors, nullptr, parent->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", ancestors, nullptr, parent->children[2].get());
    CHECK(s1.hasColor);
    CHECK(isRed(s1));
    // .excluded should NOT match :not(.excluded)
    CHECK(isNotRed(s2));
    CHECK(s3.hasColor);
    CHECK(isRed(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] :is(.a, .b) ─────────────────────────────────────────
int test_is() {
    std::cout << "[8] :is(.a, .b)" << std::endl;
    StyleSheet sheet;
    sheet.parse(":is(.a, .b) { color: #ff0000; }");

    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");

    Style sa = sheet.resolve("a", "", "panel", {}, nullptr, wa.get());
    Style sb = sheet.resolve("b", "", "panel", {}, nullptr, wb.get());
    Style sc = sheet.resolve("c", "", "panel", {}, nullptr, wc.get());
    CHECK(isRed(sa));
    CHECK(isRed(sb));
    CHECK(isNotRed(sc));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [9] :where() — zero specificity ─────────────────────────
int test_where() {
    std::cout << "[9] :where(.x) has zero specificity" << std::endl;
    StyleSheet sheet;
    // :where has zero specificity, .y has specificity 0-1-0.
    // When both match, .y should win.
    sheet.parse(":where(.x) { color: #ff0000; }"
               ".x { color: #00ff00; }");

    auto wx = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x", "", "panel", {}, nullptr, wx.get());
    // .x (green) should win over :where(.x) (red) due to higher specificity
    CHECK(isGreen(s));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [10] :has(.child) — descendant matching ─────────────────
int test_has() {
    std::cout << "[10] :has(.child)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".container:has(.target) { color: #ff0000; }");

    auto container = std::make_shared<Panel>("container");
    auto target = std::make_shared<Panel>("target");
    target->parent = container.get();
    container->children.push_back(target);

    auto empty = std::make_shared<Panel>("container");

    std::vector<CSSSelectorNode> noAncestors;
    Style s1 = sheet.resolve("container", "", "panel", noAncestors, nullptr, container.get());
    Style s2 = sheet.resolve("container", "", "panel", noAncestors, nullptr, empty.get());
    CHECK(s1.hasColor);
    CHECK(isRed(s1));
    // empty container should NOT match :has(.target)
    CHECK(isNotRed(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [11] :only-child ─────────────────────────────────────────
int test_only_child() {
    std::cout << "[11] :only-child" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:only-child { color: #ff0000; }");

    auto single = makeParentWithChildren("list", {"item"});
    auto multi = makeParentWithChildren("list", {"item", "item"});
    std::vector<CSSSelectorNode> a1 = {{"list", "", "panel", single.get()}};
    std::vector<CSSSelectorNode> a2 = {{"list", "", "panel", multi.get()}};

    Style s1 = sheet.resolve("item", "", "panel", a1, nullptr, single->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", a2, nullptr, multi->children[0].get());
    CHECK(s1.hasColor);
    CHECK(isRed(s1));
    CHECK(isNotRed(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [12] :empty ──────────────────────────────────────────────
int test_empty() {
    std::cout << "[12] :empty" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box:empty { color: #0000ff; }");

    auto empty = std::make_shared<Panel>("box"); // no children
    auto full = std::make_shared<Panel>("box");
    full->children.push_back(std::make_shared<Panel>("child"));
    full->children[0]->parent = full.get();

    Style se = sheet.resolve("box", "", "panel", {}, nullptr, empty.get());
    Style sf = sheet.resolve("box", "", "panel", {}, nullptr, full.get());
    CHECK(isBlue(se));
    CHECK(isNotBlue(sf));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [13] :nth-of-type(2) ────────────────────────────────────
int test_nth_of_type() {
    std::cout << "[13] :nth-of-type(2)" << std::endl;
    StyleSheet sheet;
    sheet.parse("panel:nth-of-type(2) { color: #ff0000; }");

    auto parent = std::make_shared<Panel>("list");
    auto c1 = std::make_shared<Panel>("a"); c1->parent = parent.get(); c1->type = "panel";
    auto c2 = std::make_shared<Panel>("b"); c2->parent = parent.get(); c2->type = "panel";
    auto c3 = std::make_shared<Panel>("c"); c3->parent = parent.get(); c3->type = "panel";
    parent->children = {c1, c2, c3};

    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};
    Style s1 = sheet.resolve("a", "", "panel", ancestors, nullptr, c1.get());
    Style s2 = sheet.resolve("b", "", "panel", ancestors, nullptr, c2.get());
    Style s3 = sheet.resolve("c", "", "panel", ancestors, nullptr, c3.get());
    CHECK(isNotRed(s1));
    CHECK(isRed(s2)); // red, 1e-2f);
    CHECK(isNotRed(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Structural/Relational Pseudo-Class Tests ===" << std::endl;
    int rc = 0;
    rc |= test_first_child();
    rc |= test_last_child();
    rc |= test_nth_child_number();
    rc |= test_nth_child_even_odd();
    rc |= test_nth_child_formula();
    rc |= test_nth_last_child();
    rc |= test_not();
    rc |= test_is();
    rc |= test_where();
    rc |= test_has();
    rc |= test_only_child();
    rc |= test_empty();
    rc |= test_nth_of_type();
    if (rc == 0)
        std::cout << "\nAll structural pseudo-class tests passed!" << std::endl;
    else
        std::cerr << "\nSome tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
