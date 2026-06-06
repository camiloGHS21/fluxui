// FluxUI test: :has() with sibling combinators + form-control pseudo-classes.
//   :has(+ x) / :has(~ x)  — CSS Selectors L4 §4.5 (relative selector)
//   :required / :optional / :placeholder-shown / :valid / :invalid — CSS UI L4
//
// Before: :has() only walked descendants (no sibling axis), and none of the
// form pseudo-classes were implemented.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// Resolve a rule's selector against a widget; the rule sets a distinctive
// letter-spacing sentinel (a property the UA sheet never sets on form controls),
// so we can tell whether *this* rule matched without UA background noise.
static bool ruleApplies(const std::string& selectorBody, Widget* w,
                        const std::vector<CSSSelectorNode>& ancestors = {}) {
    // selectorBody is the selector without the declaration block.
    StyleSheet sheet;
    sheet.parse(selectorBody + " { letter-spacing: 42px; }");
    Style s = sheet.resolve(w->className.getString(), w->id.getString(),
                            w->selectorType(), ancestors, nullptr, w);
    return s.letterSpacing > 41.0f && s.letterSpacing < 43.0f;
}

// ── [1] :has(+ .x) adjacent sibling ──
int test_has_adjacent() {
    std::cout << "[1] :has(+ .next) matches when next sibling matches" << std::endl;
    auto parent = std::make_shared<Panel>("list");
    auto a = std::make_shared<Panel>("a"); a->parent = parent.get(); a->type = "panel";
    auto b = std::make_shared<Panel>("next"); b->parent = parent.get(); b->type = "panel";
    parent->children = {a, b};

    // .a:has(+ .next) should match a (its adjacent sibling is .next)
    CHECK(ruleApplies(".a:has(+ .next)", a.get()));
    // .next:has(+ .a) should NOT match (nothing after .next)
    CHECK(!ruleApplies(".next:has(+ .a)", b.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] :has(~ .x) following sibling ──
int test_has_following() {
    std::cout << "[2] :has(~ .later) matches a preceding sibling" << std::endl;
    auto parent = std::make_shared<Panel>("list");
    auto a = std::make_shared<Panel>("a"); a->parent = parent.get(); a->type = "panel";
    auto mid = std::make_shared<Panel>("mid"); mid->parent = parent.get(); mid->type = "panel";
    auto later = std::make_shared<Panel>("later"); later->parent = parent.get(); later->type = "panel";
    parent->children = {a, mid, later};

    CHECK(ruleApplies(".a:has(~ .later)", a.get()));
    CHECK(!ruleApplies(".later:has(~ .a)", later.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] :has(> .child) direct child still works ──
int test_has_child() {
    std::cout << "[3] :has(> .child) direct-child axis" << std::endl;
    auto box = std::make_shared<Panel>("box");
    auto child = std::make_shared<Panel>("child"); child->parent = box.get(); child->type = "panel";
    auto grand = std::make_shared<Panel>("child"); grand->parent = child.get(); grand->type = "panel";
    box->children = {child};
    child->children = {grand};

    CHECK(ruleApplies(".box:has(> .child)", box.get()));
    // direct-child only: .child has a .child grandchild but box's > axis is child
    CHECK(ruleApplies(".box:has(.child)", box.get())); // descendant
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] :required / :optional ──
int test_required_optional() {
    std::cout << "[4] :required / :optional" << std::endl;
    auto inp = std::make_shared<TextInput>();
    inp->className = "f";
    CHECK(!ruleApplies("input:required", inp.get()));
    CHECK(ruleApplies("input:optional", inp.get()));

    inp->setRequired(true);
    CHECK(ruleApplies("input:required", inp.get()));
    CHECK(!ruleApplies("input:optional", inp.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] :placeholder-shown ──
int test_placeholder_shown() {
    std::cout << "[5] :placeholder-shown" << std::endl;
    auto inp = std::make_shared<TextInput>("Enter name");  // has placeholder, empty value
    inp->className = "f";
    CHECK(ruleApplies("input:placeholder-shown", inp.get()));

    inp->value = "typed";
    inp->cachedSelectorType.clear();  // value changed → recompute type
    CHECK(!ruleApplies("input:placeholder-shown", inp.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] :valid / :invalid ──
int test_valid_invalid() {
    std::cout << "[6] :valid / :invalid via required + empty value" << std::endl;
    auto inp = std::make_shared<TextInput>();
    inp->className = "f";
    inp->setRequired(true);                 // required + empty → invalid
    CHECK(ruleApplies("input:invalid", inp.get()));
    CHECK(!ruleApplies("input:valid", inp.get()));

    inp->value = "ok";
    inp->cachedSelectorType.clear();
    CHECK(!ruleApplies("input:invalid", inp.get()));
    CHECK(ruleApplies("input:valid", inp.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI :has() sibling + form pseudo-class Tests ===" << std::endl;
    int rc = 0;
    rc |= test_has_adjacent();
    rc |= test_has_following();
    rc |= test_has_child();
    rc |= test_required_optional();
    rc |= test_placeholder_shown();
    rc |= test_valid_invalid();
    if (rc == 0) std::cout << "\nAll :has()/form pseudo-class tests passed!" << std::endl;
    return rc;
}
