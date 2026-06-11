// FluxUI gradient parsing test (CSS Images L3/L4).
// Covers linear / radial / conic gradients and their repeating-* variants,
// direction/angle keywords, position (at ...), shape/size keywords, and
// color-stop position parsing.
//
// NOTE: this test validates the CSS *parsing* into the Gradient model. Full
// multi-stop radial/conic GPU rendering is a separate rendering-pipeline task.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
static bool nearEq(float a, float b){ return std::abs(a-b) < 0.01f; }

static Gradient parse(const std::string& v) { return StyleSheet::parseGradient(v); }

// ── [1] linear-gradient direction keywords ──
int test_linear() {
    std::cout << "[1] linear-gradient" << std::endl;
    Gradient g = parse("linear-gradient(to right, #ff0000, #0000ff)");
    CHECK(g.type == Gradient::Linear);
    CHECK(nearEq(g.angle, 90.0f));
    CHECK(g.stops.size() == 2);
    CHECK(nearEq(g.stops[0].first.r, 1.0f));
    CHECK(nearEq(g.stops[1].first.b, 1.0f));

    Gradient g2 = parse("linear-gradient(45deg, red, green, blue)");
    CHECK(nearEq(g2.angle, 45.0f));
    CHECK(g2.stops.size() == 3);

    Gradient g3 = parse("linear-gradient(to top right, #000, #fff)");
    CHECK(nearEq(g3.angle, 45.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] radial-gradient shape / size / position ──
int test_radial() {
    std::cout << "[2] radial-gradient" << std::endl;
    Gradient g = parse("radial-gradient(circle at center, #ff0000, #0000ff)");
    CHECK(g.type == Gradient::Radial);
    CHECK(g.radialShape == Gradient::Circle);
    CHECK(g.stops.size() == 2);

    Gradient g2 = parse("radial-gradient(ellipse closest-side at top left, red, blue)");
    CHECK(g2.type == Gradient::Radial);
    CHECK(g2.radialShape == Gradient::Ellipse);
    CHECK(g2.radialExtent == Gradient::ClosestSide);
    CHECK(g2.hasCenter);
    CHECK(nearEq(g2.center.x, 0.0f));   // left
    CHECK(nearEq(g2.center.y, 0.0f));   // top

    // No config token → starts at first color
    Gradient g3 = parse("radial-gradient(#ff0000, #00ff00)");
    CHECK(g3.type == Gradient::Radial);
    CHECK(g3.stops.size() == 2);
    CHECK(nearEq(g3.stops[0].first.r, 1.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] conic-gradient from-angle + position ──
int test_conic() {
    std::cout << "[3] conic-gradient" << std::endl;
    Gradient g = parse("conic-gradient(from 90deg at 50% 50%, #ff0000, #0000ff)");
    CHECK(g.type == Gradient::Conic);
    CHECK(nearEq(g.angle, 90.0f));
    CHECK(g.stops.size() == 2);

    // conic color stops as angles
    Gradient g2 = parse("conic-gradient(red 0deg, green 180deg, blue 360deg)");
    CHECK(g2.type == Gradient::Conic);
    CHECK(g2.stops.size() == 3);
    CHECK(nearEq(g2.stops[0].second, 0.0f));
    CHECK(nearEq(g2.stops[1].second, 0.5f));   // 180deg → 0.5
    CHECK(nearEq(g2.stops[2].second, 1.0f));   // 360deg → 1.0
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] repeating-* variants ──
int test_repeating() {
    std::cout << "[4] repeating gradients" << std::endl;
    Gradient g = parse("repeating-linear-gradient(45deg, #000 0%, #fff 10%)");
    CHECK(g.type == Gradient::Linear);
    CHECK(g.repeating);

    Gradient g2 = parse("repeating-radial-gradient(circle, red, blue 20px)");
    CHECK(g2.type == Gradient::Radial);
    CHECK(g2.repeating);

    Gradient g3 = parse("repeating-conic-gradient(red, blue)");
    CHECK(g3.type == Gradient::Conic);
    CHECK(g3.repeating);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] explicit color-stop positions ──
int test_stops() {
    std::cout << "[5] color-stop positions" << std::endl;
    Gradient g = parse("linear-gradient(to right, #ff0000 0%, #00ff00 30%, #0000ff 100%)");
    CHECK(g.stops.size() == 3);
    CHECK(nearEq(g.stops[0].second, 0.0f));
    CHECK(nearEq(g.stops[1].second, 0.3f));
    CHECK(nearEq(g.stops[2].second, 1.0f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [6] applied via background ──
int test_background() {
    std::cout << "[6] background: radial-gradient resolves on a widget" << std::endl;
    StyleSheet sheet;
    sheet.parse(".box { background: radial-gradient(circle, #ff0000, #0000ff); }");
    auto w = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, w.get());
    CHECK(s.backgroundGradient.type == Gradient::Radial);
    CHECK(s.backgroundGradient.stops.size() == 2);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Gradient Parsing Tests ===" << std::endl;
    int rc = 0;
    rc |= test_linear();
    rc |= test_radial();
    rc |= test_conic();
    rc |= test_repeating();
    rc |= test_stops();
    rc |= test_background();
    if (rc == 0) std::cout << "\nAll gradient tests passed!" << std::endl;
    return rc;
}
