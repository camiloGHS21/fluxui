// FluxUI UI Misc CSS Properties Test
// CSS UI L4, CSS Color Adjust L1, CSS Contain L2 / Blink parity

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
#define CHECK_NEAR(a,b,eps) do{float _a=(float)(a),_b=(float)(b);if(std::abs(_a-_b)>(float)(eps)){std::cerr<<"FAIL @ line "<<__LINE__<<std::endl;return 1;}}while(0)

int test_accent_color() {
    std::cout << "[1] accent-color" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { accent-color: #ff6600; } .y { accent-color: auto; }");
    auto wx = std::make_shared<Panel>("x"); auto wy = std::make_shared<Panel>("y");
    Style sx = sheet.resolve("x","","panel",{},nullptr,wx.get());
    Style sy = sheet.resolve("y","","panel",{},nullptr,wy.get());
    CHECK(sx.hasAccentColor); CHECK_NEAR(sx.accentColor.r, 1.0f, 1e-2f);
    CHECK(!sy.hasAccentColor);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_caret_color() {
    std::cout << "[2] caret-color" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { caret-color: #00ff00; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasCaretColor); CHECK_NEAR(s.caretColor.g, 1.0f, 1e-2f);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_color_scheme() {
    std::cout << "[3] color-scheme" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { color-scheme: light dark; } .b { color-scheme: dark; }");
    auto wa = std::make_shared<Panel>("a"); auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.hasColorScheme); CHECK(sa.colorScheme == "light dark");
    CHECK(sb.hasColorScheme); CHECK(sb.colorScheme == "dark");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_field_sizing() {
    std::cout << "[4] field-sizing" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { field-sizing: content; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasFieldSizing); CHECK(s.fieldSizing == "content");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_image_rendering() {
    std::cout << "[5] image-rendering" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { image-rendering: pixelated; } .b { image-rendering: crisp-edges; }");
    auto wa = std::make_shared<Panel>("a"); auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.hasImageRendering); CHECK(sa.imageRendering == "pixelated");
    CHECK(sb.hasImageRendering); CHECK(sb.imageRendering == "crisp-edges");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_image_orientation() {
    std::cout << "[6] image-orientation" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { image-orientation: none; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasImageOrientation); CHECK(s.imageOrientation == "none");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_object_view_box() {
    std::cout << "[7] object-view-box" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { object-view-box: inset(25% 25% 25% 25%); }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasObjectViewBox); CHECK(s.objectViewBox.find("inset") != std::string::npos);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_touch_action() {
    std::cout << "[8] touch-action" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { touch-action: pan-x pan-y; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasTouchAction); CHECK(s.touchAction == "pan-x pan-y");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_user_select() {
    std::cout << "[9] user-select" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { user-select: none; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasUserSelect); CHECK(s.userSelect == "none");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_will_change() {
    std::cout << "[10] will-change" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { will-change: transform, opacity; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasWillChange); CHECK(s.willChange.find("transform") != std::string::npos);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_content_visibility() {
    std::cout << "[11] content-visibility" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { content-visibility: auto; } .b { content-visibility: hidden; }");
    auto wa = std::make_shared<Panel>("a"); auto wb = std::make_shared<Panel>("b");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    CHECK(sa.hasContentVisibility); CHECK(sa.contentVisibility == "auto");
    CHECK(sb.hasContentVisibility); CHECK(sb.contentVisibility == "hidden");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_contain_intrinsic_size() {
    std::cout << "[12] contain-intrinsic-size" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { contain-intrinsic-size: auto 300px; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.hasContainIntrinsicSize); CHECK(s.containIntrinsicSize == "auto 300px");
    std::cout << "  PASS" << std::endl; return 0;
}

int main() {
    std::cout << "=== FluxUI UI Misc Properties Tests ===" << std::endl;
    int rc = 0;
    rc |= test_accent_color();
    rc |= test_caret_color();
    rc |= test_color_scheme();
    rc |= test_field_sizing();
    rc |= test_image_rendering();
    rc |= test_image_orientation();
    rc |= test_object_view_box();
    rc |= test_touch_action();
    rc |= test_user_select();
    rc |= test_will_change();
    rc |= test_content_visibility();
    rc |= test_contain_intrinsic_size();
    if (rc == 0) std::cout << "\nAll UI misc tests passed!" << std::endl;
    else std::cerr << "\nSome tests FAILED" << std::endl;
    return rc;
}
