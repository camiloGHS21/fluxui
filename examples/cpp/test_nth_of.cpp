// FluxUI :nth-child(An+B of S) test (CSS Selectors L4).
// Counts only siblings matching S, then applies the An+B formula.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// Build a parent with the given child class names; return the parent.
static std::shared_ptr<Panel> makeParent(const std::vector<std::string>& classes) {
    auto parent = std::make_shared<Panel>("list");
    parent->type = "panel";
    for (const auto& cls : classes) {
        auto c = std::make_shared<Panel>(cls);
        c->type = "panel";
        c->parent = parent.get();
        parent->children.push_back(c);
    }
    return parent;
}

// Does `selector` match the child at index `i` of parent?
static bool matchesChild(const std::string& selector, Panel* parent, size_t i) {
    StyleSheet sheet;
    sheet.parse(selector + " { letter-spacing: 42px; }");
    std::vector<CSSSelectorNode> anc = {{"list", "", "panel", parent}};
    Widget* w = parent->children[i].get();
    Style s = sheet.resolve(w->className.getString(), w->id.getString(),
                            w->selectorType(), anc, nullptr, w);
    return s.letterSpacing > 41.0f && s.letterSpacing < 43.0f;
}

// ── [1] :nth-child(odd of .item) — only .item children counted ──
int test_nth_of_odd() {
    std::cout << "[1] :nth-child(odd of .item)" << std::endl;
    // indices:  0=item 1=other 2=item 3=item 4=other 5=item
    // .item sequence: child0(1st), child2(2nd), child3(3rd), child5(4th)
    // odd among items → 1st & 3rd → child0, child3
    auto p = makeParent({"item", "other", "item", "item", "other", "item"});
    CHECK(matchesChild(":nth-child(odd of .item)", p.get(), 0));   // 1st item
    CHECK(!matchesChild(":nth-child(odd of .item)", p.get(), 2));  // 2nd item
    CHECK(matchesChild(":nth-child(odd of .item)", p.get(), 3));   // 3rd item
    CHECK(!matchesChild(":nth-child(odd of .item)", p.get(), 5));  // 4th item
    // a non-.item never matches
    CHECK(!matchesChild(":nth-child(odd of .item)", p.get(), 1));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] :nth-child(2 of .item) — the 2nd matching item ──
int test_nth_of_index() {
    std::cout << "[2] :nth-child(2 of .item)" << std::endl;
    auto p = makeParent({"item", "other", "item", "item"});
    // 2nd .item is child2
    CHECK(!matchesChild(":nth-child(2 of .item)", p.get(), 0));
    CHECK(matchesChild(":nth-child(2 of .item)", p.get(), 2));
    CHECK(!matchesChild(":nth-child(2 of .item)", p.get(), 3));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] :nth-last-child(1 of .item) — last matching item ──
int test_nth_last_of() {
    std::cout << "[3] :nth-last-child(1 of .item)" << std::endl;
    auto p = makeParent({"item", "other", "item", "other"});
    // last .item is child2
    CHECK(matchesChild(":nth-last-child(1 of .item)", p.get(), 2));
    CHECK(!matchesChild(":nth-last-child(1 of .item)", p.get(), 0));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] plain :nth-child still works (no `of`) ──
int test_plain_nth() {
    std::cout << "[4] plain :nth-child(2)" << std::endl;
    auto p = makeParent({"a", "b", "c"});
    CHECK(matchesChild(":nth-child(2)", p.get(), 1));
    CHECK(!matchesChild(":nth-child(2)", p.get(), 0));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI :nth-child(of S) Tests ===" << std::endl;
    int rc = 0;
    rc |= test_nth_of_odd();
    rc |= test_nth_of_index();
    rc |= test_nth_last_of();
    rc |= test_plain_nth();
    if (rc == 0) std::cout << "\nAll nth-of-S tests passed!" << std::endl;
    return rc;
}
