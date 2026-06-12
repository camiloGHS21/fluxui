// FluxUI :in-range / :out-of-range pseudo-class test (CSS Selectors L4).

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

static bool applies(const std::string& selector, Widget* w) {
    StyleSheet sheet;
    sheet.parse(selector + " { letter-spacing: 42px; }");
    Style s = sheet.resolve(w->className.getString(), w->id.getString(),
                            w->selectorType(), {}, nullptr, w);
    return s.letterSpacing > 41.0f && s.letterSpacing < 43.0f;
}

// ── [1] value within [min,max] is :in-range ──
int test_in_range() {
    std::cout << "[1] :in-range" << std::endl;
    auto r = std::make_shared<RangeInput>(0.5f, 0.0f, 1.0f, 0.01f);
    CHECK(applies("input:in-range", r.get()));
    CHECK(!applies("input:out-of-range", r.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] value outside [min,max] is :out-of-range ──
int test_out_of_range() {
    std::cout << "[2] :out-of-range" << std::endl;
    auto r = std::make_shared<RangeInput>(0.5f, 0.0f, 1.0f, 0.01f);
    r->value = 2.0f;                 // forced above max
    r->cachedSelectorType.clear();
    CHECK(applies("input:out-of-range", r.get()));
    CHECK(!applies("input:in-range", r.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] setValue clamps back in range ──
int test_clamp() {
    std::cout << "[3] setValue clamps → :in-range" << std::endl;
    auto r = std::make_shared<RangeInput>(0.5f, 0.0f, 1.0f, 0.01f);
    r->setValue(5.0f);               // clamped to 1.0
    CHECK(applies("input:in-range", r.get()));
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI :in-range / :out-of-range Tests ===" << std::endl;
    int rc = 0;
    rc |= test_in_range();
    rc |= test_out_of_range();
    rc |= test_clamp();
    if (rc == 0) std::cout << "\nAll range pseudo-class tests passed!" << std::endl;
    return rc;
}
