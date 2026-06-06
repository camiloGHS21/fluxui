// FluxUI Scroll API CSS Properties Test
// CSS Scroll Snap L1, Overscroll Behavior L1, CSS Scrollbars L1

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
#define CHECK_NEAR(a,b,eps) do{float _a=(float)(a),_b=(float)(b);if(std::abs(_a-_b)>(float)(eps)){std::cerr<<"FAIL @ line "<<__LINE__<<std::endl;return 1;}}while(0)

int test_scroll_snap_type() {
    std::cout << "[1] scroll-snap-type" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { scroll-snap-type: x mandatory; }"
               ".b { scroll-snap-type: y proximity; }"
               ".c { scroll-snap-type: both mandatory; }"
               ".d { scroll-snap-type: none; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    auto wd = std::make_shared<Panel>("d");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    Style sc = sheet.resolve("c","","panel",{},nullptr,wc.get());
    Style sd = sheet.resolve("d","","panel",{},nullptr,wd.get());
    CHECK(sa.rare().hasScrollSnapType); CHECK(sa.rare().scrollSnapType == "x mandatory");
    CHECK(sb.rare().hasScrollSnapType); CHECK(sb.rare().scrollSnapType == "y proximity");
    CHECK(sc.rare().hasScrollSnapType); CHECK(sc.rare().scrollSnapType == "both mandatory");
    CHECK(!sd.rare().hasScrollSnapType);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scroll_snap_align() {
    std::cout << "[2] scroll-snap-align" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scroll-snap-align: start; }"
               ".y { scroll-snap-align: center end; }");
    auto wx = std::make_shared<Panel>("x");
    auto wy = std::make_shared<Panel>("y");
    Style sx = sheet.resolve("x","","panel",{},nullptr,wx.get());
    Style sy = sheet.resolve("y","","panel",{},nullptr,wy.get());
    CHECK(sx.rare().hasScrollSnapAlign); CHECK(sx.rare().scrollSnapAlign == "start");
    CHECK(sy.rare().hasScrollSnapAlign); CHECK(sy.rare().scrollSnapAlign == "center end");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scroll_snap_stop() {
    std::cout << "[3] scroll-snap-stop" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scroll-snap-stop: always; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().scrollSnapStop == "always");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scroll_padding() {
    std::cout << "[4] scroll-padding" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scroll-padding: 10px 20px 30px 40px; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasScrollPadding);
    CHECK_NEAR(s.rare().scrollPadding.top, 10.0f, 1e-2f);
    CHECK_NEAR(s.rare().scrollPadding.right, 20.0f, 1e-2f);
    CHECK_NEAR(s.rare().scrollPadding.bottom, 30.0f, 1e-2f);
    CHECK_NEAR(s.rare().scrollPadding.left, 40.0f, 1e-2f);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scroll_margin() {
    std::cout << "[5] scroll-margin" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scroll-margin: 5px; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasScrollMargin);
    CHECK_NEAR(s.rare().scrollMargin.top, 5.0f, 1e-2f);
    CHECK_NEAR(s.rare().scrollMargin.left, 5.0f, 1e-2f);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_overscroll_behavior() {
    std::cout << "[6] overscroll-behavior" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { overscroll-behavior: contain; }"
               ".b { overscroll-behavior: none auto; }"
               ".c { overscroll-behavior-x: none; overscroll-behavior-y: contain; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    Style sc = sheet.resolve("c","","panel",{},nullptr,wc.get());
    CHECK(sa.rare().hasOverscrollBehavior);
    CHECK(sa.rare().overscrollBehaviorX == Style::OverscrollBehavior::Contain);
    CHECK(sa.rare().overscrollBehaviorY == Style::OverscrollBehavior::Contain);
    CHECK(sb.rare().overscrollBehaviorX == Style::OverscrollBehavior::None);
    CHECK(sb.rare().overscrollBehaviorY == Style::OverscrollBehavior::Auto);
    CHECK(sc.rare().overscrollBehaviorX == Style::OverscrollBehavior::None);
    CHECK(sc.rare().overscrollBehaviorY == Style::OverscrollBehavior::Contain);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scrollbar_color() {
    std::cout << "[7] scrollbar-color" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scrollbar-color: #ff0000 #00ff00; }"
               ".y { scrollbar-color: auto; }");
    auto wx = std::make_shared<Panel>("x");
    auto wy = std::make_shared<Panel>("y");
    Style sx = sheet.resolve("x","","panel",{},nullptr,wx.get());
    Style sy = sheet.resolve("y","","panel",{},nullptr,wy.get());
    CHECK(sx.rare().hasScrollbarColor);
    CHECK_NEAR(sx.rare().scrollbarThumbColor.r, 1.0f, 1e-2f);
    CHECK_NEAR(sx.rare().scrollbarTrackColor.g, 1.0f, 1e-2f);
    CHECK(!sy.rare().hasScrollbarColor);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scrollbar_width() {
    std::cout << "[8] scrollbar-width" << std::endl;
    StyleSheet sheet;
    sheet.parse(".a { scrollbar-width: thin; }"
               ".b { scrollbar-width: none; }"
               ".c { scrollbar-width: auto; }");
    auto wa = std::make_shared<Panel>("a");
    auto wb = std::make_shared<Panel>("b");
    auto wc = std::make_shared<Panel>("c");
    Style sa = sheet.resolve("a","","panel",{},nullptr,wa.get());
    Style sb = sheet.resolve("b","","panel",{},nullptr,wb.get());
    Style sc = sheet.resolve("c","","panel",{},nullptr,wc.get());
    CHECK(sa.rare().scrollbarWidth == Style::ScrollbarWidth::Thin);
    CHECK(sb.rare().scrollbarWidth == Style::ScrollbarWidth::None);
    CHECK(sc.rare().scrollbarWidth == Style::ScrollbarWidth::Auto);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_overflow_anchor() {
    std::cout << "[9] overflow-anchor" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { overflow-anchor: none; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasOverflowAnchor);
    CHECK(s.rare().overflowAnchor == Style::OverflowAnchor::None);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scrollbar_gutter() {
    std::cout << "[10] scrollbar-gutter" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scrollbar-gutter: stable both-edges; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasScrollbarGutter);
    CHECK(s.rare().scrollbarGutter == "stable both-edges");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_scroll_behavior() {
    std::cout << "[11] scroll-behavior" << std::endl;
    StyleSheet sheet;
    sheet.parse(".x { scroll-behavior: smooth; }");
    auto w = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x","","panel",{},nullptr,w.get());
    CHECK(s.rare().hasScrollBehavior);
    CHECK(s.rare().scrollBehavior == Style::ScrollBehavior::Smooth);
    std::cout << "  PASS" << std::endl; return 0;
}

int main() {
    std::cout << "=== FluxUI Scroll API Tests ===" << std::endl;
    int rc = 0;
    rc |= test_scroll_snap_type();
    rc |= test_scroll_snap_align();
    rc |= test_scroll_snap_stop();
    rc |= test_scroll_padding();
    rc |= test_scroll_margin();
    rc |= test_overscroll_behavior();
    rc |= test_scrollbar_color();
    rc |= test_scrollbar_width();
    rc |= test_overflow_anchor();
    rc |= test_scrollbar_gutter();
    rc |= test_scroll_behavior();
    if (rc == 0) std::cout << "\nAll scroll API tests passed!" << std::endl;
    else std::cerr << "\nSome tests FAILED" << std::endl;
    return rc;
}
