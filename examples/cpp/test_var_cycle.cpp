// FluxUI var() Cycle Detection Parity Test
// Mirrors Blink's CSSVariableResolver cycle detection behavior.
//
// Reference: https://www.w3.org/TR/css-variables-1/#cycles
//            chromium/src/third_party/blink/renderer/core/css/resolver/css_variable_resolver.cc

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do { if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
#define CHECK_NEAR(a,b,eps) do{float _a=(float)(a),_b=(float)(b);if(std::abs(_a-_b)>(float)(eps)){std::cerr<<"FAIL: "<<#a<<"="<<_a<<" ~= "<<#b<<"="<<_b<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// ── [1] Normal var() resolution works ───────────────────────
int test_basic_var() {
    std::cout << "[1] Basic var() resolution" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --color: #ff0000; } .box { background-color: var(--color); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] var() with fallback ─────────────────────────────────
int test_var_fallback() {
    std::cout << "[2] var(--undefined, fallback)" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { background-color: var(--nope, #00ff00); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] Direct cycle: --a: var(--a) ─────────────────────────
int test_direct_cycle() {
    std::cout << "[3] Direct cycle: --a references itself" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --a: var(--a); } .box { background-color: var(--a, #0000ff); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // Cycle detected on --a → falls back to #0000ff
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.b, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] Indirect cycle: --a → --b → --a ────────────────────
int test_indirect_cycle() {
    std::cout << "[4] Indirect cycle: --a → --b → --a" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --a: var(--b); --b: var(--a); } .box { background-color: var(--a, #ff00ff); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // Cycle: --a → --b → --a (detected) → fallback #ff00ff
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f);
    CHECK_NEAR(s.backgroundColor.b, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] Three-node cycle: --a → --b → --c → --a ────────────
int test_three_node_cycle() {
    std::cout << "[5] Three-node cycle: --a → --b → --c → --a" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --a: var(--b); --b: var(--c); --c: var(--a); }"
               ".box { background-color: var(--a, #aabbcc); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // Cycle detected → fallback
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.r, 0xaa/255.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] No cycle with shared dependency ─────────────────────
int test_shared_dependency_no_cycle() {
    std::cout << "[6] Shared dep (diamond): --a → --c, --b → --c (no cycle)" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --c: #ff0000; --a: var(--c); --b: var(--c); }"
               ".box { background-color: var(--a); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // No cycle — --a resolves to --c → #ff0000
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.r, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [7] Deep chain (no cycle, depth < 32) ───────────────────
int test_deep_chain_no_cycle() {
    std::cout << "[7] Deep chain: --v1 → --v2 → ... → --v10 (no cycle)" << std::endl;
    StyleSheet sheet;
    // Build a chain of 10 variables
    std::string css = ":root { --v10: #00ff00;";
    for (int i = 9; i >= 1; --i) {
        css += " --v" + std::to_string(i) + ": var(--v" + std::to_string(i+1) + ");";
    }
    css += " } .box { background-color: var(--v1); }";
    sheet.parse(css);
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // Should resolve through the chain to #00ff00
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.g, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [8] Cycle without fallback → invalid ────────────────────
int test_cycle_no_fallback_invalid() {
    std::cout << "[8] Cycle without fallback → property becomes invalid" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --x: var(--x); } .box { background-color: var(--x); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // No fallback, cycle → invalid → background stays default (transparent)
    CHECK(s.backgroundColor.a < 0.01f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [9] Nested var() with cycle in inner ────────────────────
int test_nested_var_cycle() {
    std::cout << "[9] Nested: var(--a, var(--b)) where --b cycles" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --b: var(--b); } .box { background-color: var(--undefined, var(--b, #112233)); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    // --undefined → fallback var(--b, #112233) → --b cycles → fallback #112233
    CHECK(s.backgroundColor.a > 0.01f);
    CHECK_NEAR(s.backgroundColor.r, 0x11/255.0f, 1e-2f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [10] resolveValue API returns valid=false on cycle ──────
int test_resolve_value_api() {
    std::cout << "[10] resolveValue() returns valid=false on cycle" << std::endl;
    StyleSheet sheet;
    sheet.parse(":root { --loop: var(--loop); }");
    std::unordered_map<std::string, std::string> custom = {{"--loop", "var(--loop)"}};
    bool valid = true;
    std::string result = sheet.resolveValue("var(--loop)", custom, &valid);
    CHECK(!valid);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI var() Cycle Detection Tests ===" << std::endl;
    int rc = 0;
    rc |= test_basic_var();
    rc |= test_var_fallback();
    rc |= test_direct_cycle();
    rc |= test_indirect_cycle();
    rc |= test_three_node_cycle();
    rc |= test_shared_dependency_no_cycle();
    rc |= test_deep_chain_no_cycle();
    rc |= test_cycle_no_fallback_invalid();
    rc |= test_nested_var_cycle();
    rc |= test_resolve_value_api();
    if (rc == 0)
        std::cout << "\nAll var() cycle detection tests passed!" << std::endl;
    else
        std::cerr << "\nSome tests FAILED (rc=" << rc << ")" << std::endl;
    return rc;
}
