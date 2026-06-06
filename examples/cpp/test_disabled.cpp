// FluxUI :disabled / :enabled selector parity test.
// Mirrors Blink's :enabled / :disabled matching (IsDisabledFormControl):
//   https://www.w3.org/TR/selectors-4/#enableddisabled
//
// Before the fix, :enabled was hard-wired to always match and :disabled to
// never match, regardless of widget state. Now they consult Widget::disabled.

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

static bool nearEq(float a, float b) { return std::abs(a - b) < 1e-3f; }

// Resolve background color for a button widget against a sheet, honoring state.
static Color resolveBg(const StyleSheet& sheet, Widget* w) {
    Style s = sheet.resolve(w->className.getString(), w->id.getString(),
                            w->selectorType(), {}, nullptr, w);
    return s.backgroundColor;
}

// ── [1] :disabled matches only when the widget is disabled ──
int test_disabled_matches() {
    std::cout << "[1] :disabled matches a disabled widget" << std::endl;
    StyleSheet sheet;
    sheet.parse("button { background: #112233; }"
                "button:disabled { background: #aabbcc; }");

    auto btn = std::make_shared<Button>("");
    btn->type = "button";

    Color enabledBg = resolveBg(sheet, btn.get());
    CHECK(nearEq(enabledBg.r, 0x11 / 255.0f));   // base, :disabled NOT applied

    btn->setDisabled(true);
    Color disabledBg = resolveBg(sheet, btn.get());
    CHECK(nearEq(disabledBg.r, 0xaa / 255.0f));  // :disabled applied
    CHECK(nearEq(disabledBg.g, 0xbb / 255.0f));

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] :enabled matches only when the widget is NOT disabled ──
int test_enabled_matches() {
    std::cout << "[2] :enabled matches an enabled widget, not a disabled one" << std::endl;
    StyleSheet sheet;
    sheet.parse("button { background: #000000; }"
                "button:enabled { background: #00ff00; }");

    auto btn = std::make_shared<Button>("");
    btn->type = "button";

    Color bg = resolveBg(sheet, btn.get());
    CHECK(nearEq(bg.g, 1.0f));                    // :enabled applied (green)

    btn->setDisabled(true);
    Color bg2 = resolveBg(sheet, btn.get());
    CHECK(nearEq(bg2.r, 0.0f) && nearEq(bg2.g, 0.0f) && nearEq(bg2.b, 0.0f)); // back to base

    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] toggling disabled flips between :enabled and :disabled ──
int test_toggle() {
    std::cout << "[3] toggling disabled flips :enabled <-> :disabled" << std::endl;
    StyleSheet sheet;
    sheet.parse("button:enabled  { background: #00ff00; }"
                "button:disabled { background: #ff0000; }");

    auto btn = std::make_shared<Button>("");
    btn->type = "button";

    CHECK(nearEq(resolveBg(sheet, btn.get()).g, 1.0f)); // enabled -> green
    btn->setDisabled(true);
    CHECK(nearEq(resolveBg(sheet, btn.get()).r, 1.0f)); // disabled -> red
    btn->setDisabled(false);
    CHECK(nearEq(resolveBg(sheet, btn.get()).g, 1.0f)); // enabled again -> green

    CHECK(btn->isDisabled() == false);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] setDisabled clears interactive state ──
int test_clears_state() {
    std::cout << "[4] setDisabled clears hover/pressed/focus" << std::endl;
    auto btn = std::make_shared<Button>("");
    btn->type = "button";
    btn->hovered = true;
    btn->pressed = true;
    btn->focused = true;

    btn->setDisabled(true);
    CHECK(!btn->hovered);
    CHECK(!btn->pressed);
    CHECK(!btn->focused);
    CHECK(btn->isDisabled());

    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI :disabled / :enabled Tests ===" << std::endl;
    int rc = 0;
    rc |= test_disabled_matches();
    rc |= test_enabled_matches();
    rc |= test_toggle();
    rc |= test_clears_state();
    if (rc == 0) std::cout << "\nAll :disabled/:enabled tests passed!" << std::endl;
    return rc;
}
