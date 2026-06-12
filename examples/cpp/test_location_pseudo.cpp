// FluxUI structural/location pseudo-class test (CSS Selectors L4):
// :root, :scope, :link, :any-link, :visited.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// Resolve a selector against a widget using a distinctive letter-spacing probe
// (a property the UA sheet never sets), so matching is unambiguous.
static bool applies(const std::string& selector, Widget* w,
                    const std::vector<CSSSelectorNode>& ancestors = {}) {
    StyleSheet sheet;
    sheet.parse(selector + " { letter-spacing: 42px; }");
    Style s = sheet.resolve(w->className.getString(), w->id.getString(),
                            w->selectorType(), ancestors, nullptr, w);
    return s.letterSpacing > 41.0f && s.letterSpacing < 43.0f;
}

// ── [1] :root matches the parentless element ──
int test_root() {
    std::cout << "[1] :root" << std::endl;
    auto root = std::make_shared<Panel>("app");
    auto child = std::make_shared<Panel>("child");
    child->parent = root.get();
    root->children.push_back(child);

    CHECK(applies(":root", root.get()));
    std::vector<CSSSelectorNode> anc = {{"app", "", "panel", root.get()}};
    CHECK(!applies(":root", child.get(), anc));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] :scope behaves like :root without an explicit scope ──
int test_scope() {
    std::cout << "[2] :scope" << std::endl;
    auto root = std::make_shared<Panel>("app");
    CHECK(applies(":scope", root.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] :link / :any-link match an <a> with href ──
int test_link() {
    std::cout << "[3] :link / :any-link" << std::endl;
    auto a = std::make_shared<Anchor>("Home", "https://example.com");
    a->className = "nav";
    CHECK(applies("a:link", a.get()));
    CHECK(applies("a:any-link", a.get()));
    CHECK(applies(".nav:link", a.get()));

    auto noHref = std::make_shared<Anchor>("placeholder", "");
    CHECK(!applies("a:link", noHref.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] :visited never matches (privacy parity) ──
int test_visited() {
    std::cout << "[4] :visited never matches" << std::endl;
    auto a = std::make_shared<Anchor>("Home", "https://example.com");
    CHECK(!applies("a:visited", a.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI location pseudo-class Tests ===" << std::endl;
    int rc = 0;
    rc |= test_root();
    rc |= test_scope();
    rc |= test_link();
    rc |= test_visited();
    if (rc == 0) std::cout << "\nAll location pseudo-class tests passed!" << std::endl;
    return rc;
}
