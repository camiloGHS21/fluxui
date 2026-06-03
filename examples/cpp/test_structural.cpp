// FluxUI Structural/Relational Pseudo-Class Parity Test
// Mirrors Blink's selector matching for :nth-child, :has, :not, :is, :where.
//
// All tests use background-color (not color) because the UA stylesheet
// sets color:white on all widgets, making negative assertions impossible.
// background-color defaults to transparent (a=0), so we can cleanly
// distinguish matched (a>0) from unmatched (a==0).

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

// Helpers: matched = background-color has alpha > 0; unmatched = alpha == 0
static bool matched(const Style& s)   { return s.backgroundColor.a > 0.01f; }
static bool unmatched(const Style& s)  { return s.backgroundColor.a < 0.01f; }

static std::shared_ptr<Panel> makeParent(const std::string& parentCls,
                                         const std::vector<std::string>& childClasses) {
    auto parent = std::make_shared<Panel>(parentCls);
    for (const auto& cls : childClasses) {
        auto child = std::make_shared<Panel>(cls);
        child->parent = parent.get();
        parent->children.push_back(child);
    }
    return parent;
}

// ── [1] :first-child ────────────────────────────────────────
int test_first_child() {
    std::cout << "[1] :first-child" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:first-child { background-color: #ff0000; }");
    auto p = makeParent("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", a, nullptr, p->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    CHECK(matched(s1));
    CHECK(unmatched(s2));
    CHECK(unmatched(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] :last-child ─────────────────────────────────────────
int test_last_child() {
    std::cout << "[2] :last-child" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:last-child { background-color: #00ff00; }");
    auto p = makeParent("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    CHECK(unmatched(s1));
    CHECK(matched(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] :nth-child(2) ───────────────────────────────────────
int test_nth_child_number() {
    std::cout << "[3] :nth-child(2)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-child(2) { background-color: #0000ff; }");
    auto p = makeParent("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", a, nullptr, p->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    CHECK(unmatched(s1));
    CHECK(matched(s2));
    CHECK(unmatched(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] :nth-child(odd) / :nth-child(even) ──────────────────
int test_nth_child_even_odd() {
    std::cout << "[4] :nth-child(odd) / :nth-child(even)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-child(odd) { background-color: #ff0000; }"
               ".item:nth-child(even) { background-color: #00ff00; }");
    auto p = makeParent("list", {"item", "item", "item", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", a, nullptr, p->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    Style s4 = sheet.resolve("item", "", "panel", a, nullptr, p->children[3].get());
    CHECK_NEAR(s1.backgroundColor.r, 1.0f, 1e-2f); // odd → red
    CHECK_NEAR(s2.backgroundColor.g, 1.0f, 1e-2f); // even → green
    CHECK_NEAR(s3.backgroundColor.r, 1.0f, 1e-2f); // odd → red
    CHECK_NEAR(s4.backgroundColor.g, 1.0f, 1e-2f); // even → green
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] :nth-child(2n+1) — same as odd ─────────────────────
int test_nth_child_formula() {
    std::cout << "[5] :nth-child(2n+1)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-child(2n+1) { background-color: #ff0000; }");
    auto p = makeParent("list", {"item", "item", "item", "item", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", a, nullptr, p->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    CHECK(matched(s1));
    CHECK(unmatched(s2));
    CHECK(matched(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] :nth-last-child(1) — same as :last-child ───────────
int test_nth_last_child() {
    std::cout << "[6] :nth-last-child(1)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:nth-last-child(1) { background-color: #0000ff; }");
    auto p = makeParent("list", {"item", "item", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    CHECK(unmatched(s1));
    CHECK(matched(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] :not(.excluded) ─────────────────────────────────────
int test_not() {
    std::cout << "[7] :not(.excluded)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:not(.excluded) { background-color: #ff0000; }");
    auto p = makeParent("list", {"item", "item excluded", "item"});
    std::vector<CSSSelectorNode> a = {{"list", "", "panel", p.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a, nullptr, p->children[0].get());
    Style s2 = sheet.resolve("item excluded", "", "panel", a, nullptr, p->children[1].get());
    Style s3 = sheet.resolve("item", "", "panel", a, nullptr, p->children[2].get());
    CHECK(matched(s1));
    CHECK(unmatched(s2));
    CHECK(matched(s3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] :is(.a, .b) ─────────────────────────────────────────
int test_is() {
    std::cout << "[8] :is(.a, .b)" << std::endl;
    StyleSheet sheet;
    sheet.parse(":is(.a, .b) { background-color: #ff0000; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    Style sa = sheet.resolve("a", "", "panel", {}, nullptr, wa.get());
    Style sb = sheet.resolve("b", "", "panel", {}, nullptr, wb.get());
    Style sc = sheet.resolve("c", "", "panel", {}, nullptr, wc.get());
    CHECK(matched(sa));
    CHECK(matched(sb));
    CHECK(unmatched(sc));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [9] :where() — zero specificity ─────────────────────────
int test_where() {
    std::cout << "[9] :where(.x) has zero specificity" << std::endl;
    StyleSheet sheet;
    sheet.parse(":where(.x) { background-color: #ff0000; }"
               ".x { background-color: #00ff00; }");
    auto wx = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x", "", "panel", {}, nullptr, wx.get());
    // .x (green) wins over :where(.x) (red) because :where has 0 specificity
    CHECK(matched(s));
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f);
    CHECK_NEAR(s.backgroundColor.r, 0.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [10] :has(.child) ───────────────────────────────────────
int test_has() {
    std::cout << "[10] :has(.child)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".container:has(.target) { background-color: #ff0000; }");
    auto container = std::make_shared<Panel>("container");
    auto target = std::make_shared<Panel>("target");
    target->parent = container.get();
    container->children.push_back(target);
    auto empty = std::make_shared<Panel>("container"); // no children
    Style s1 = sheet.resolve("container", "", "panel", {}, nullptr, container.get());
    Style s2 = sheet.resolve("container", "", "panel", {}, nullptr, empty.get());
    CHECK(matched(s1));
    CHECK(unmatched(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [11] :only-child ─────────────────────────────────────────
int test_only_child() {
    std::cout << "[11] :only-child" << std::endl;
    StyleSheet sheet;
    sheet.parse(".item:only-child { background-color: #ff0000; }");
    auto single = makeParent("list", {"item"});
    auto multi = makeParent("list", {"item", "item"});
    std::vector<CSSSelectorNode> a1 = {{"list", "", "panel", single.get()}};
    std::vector<CSSSelectorNode> a2 = {{"list", "", "panel", multi.get()}};
    Style s1 = sheet.resolve("item", "", "panel", a1, nullptr, single->children[0].get());
    Style s2 = sheet.resolve("item", "", "panel", a2, nullptr, multi->children[0].get());
    CHECK(matched(s1));
    CHECK(unmatched(s2));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [12] :empty ──────────────────────────────────────────────
int test_empty() {
    std::cout << "[12] :empty" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box:empty { background-color: #0000ff; }");
    auto empty = std::make_shared<Panel>("box");
    auto full = std::make_shared<Panel>("box");
    full->children.push_back(std::make_shared<Panel>("child"));
    full->children[0]->parent = full.get();
    Style se = sheet.resolve("box", "", "panel", {}, nullptr, empty.get());
    Style sf = sheet.resolve("box", "", "panel", {}, nullptr, full.get());
    CHECK(matched(se));
    CHECK(unmatched(sf));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [13] :nth-of-type(2) ────────────────────────────────────
int test_nth_of_type() {
    std::cout << "[13] :nth-of-type(2)" << std::endl;
    StyleSheet sheet;
    sheet.parse("panel:nth-of-type(2) { background-color: #ff0000; }");
    auto parent = std::make_shared<Panel>("list");
    auto c1 = std::make_shared<Panel>("a"); c1->parent = parent.get(); c1->type = "panel";
    auto c2 = std::make_shared<Panel>("b"); c2->parent = parent.get(); c2->type = "panel";
    auto c3 = std::make_shared<Panel>("c"); c3->parent = parent.get(); c3->type = "panel";
    parent->children = {c1, c2, c3};
    std::vector<CSSSelectorNode> ancestors = {{"list", "", "panel", parent.get()}};
    Style s1 = sheet.resolve("a", "", "panel", ancestors, nullptr, c1.get());
    Style s2 = sheet.resolve("b", "", "panel", ancestors, nullptr, c2.get());
    Style s3 = sheet.resolve("c", "", "panel", ancestors, nullptr, c3.get());
    CHECK(unmatched(s1));
    CHECK(matched(s2));
    CHECK(unmatched(s3));
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
