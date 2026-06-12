// FluxUI gradient sampling test — verifies the reference rasterization math
// (Gradient::sampleColor / sampleAt) used by the software renderer for
// multi-stop linear, radial and conic gradients.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include <iostream>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)
static bool nearEq(float a, float b){ return std::abs(a-b) < 0.02f; }

// ── [1] multi-stop ramp interpolation ──
int test_ramp() {
    std::cout << "[1] sampleColor multi-stop ramp" << std::endl;
    Gradient g = StyleSheet::parseGradient(
        "linear-gradient(to right, #ff0000 0%, #00ff00 50%, #0000ff 100%)");
    Color c0 = g.sampleColor(0.0f);
    Color cMid = g.sampleColor(0.5f);
    Color c1 = g.sampleColor(1.0f);
    CHECK(nearEq(c0.r, 1.0f));                 // red at 0
    CHECK(nearEq(cMid.g, 1.0f));               // green at 0.5
    CHECK(nearEq(c1.b, 1.0f));                 // blue at 1
    // quarter point: halfway between red and green
    Color cq = g.sampleColor(0.25f);
    CHECK(nearEq(cq.r, 0.5f) && nearEq(cq.g, 0.5f));
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] linear sampleAt across the box ──
int test_linear_at() {
    std::cout << "[2] linear sampleAt (to right)" << std::endl;
    Gradient g = StyleSheet::parseGradient("linear-gradient(to right, #ff0000, #0000ff)");
    // box 0,0,100,100. Left edge ~red, right edge ~blue.
    Color cl = g.sampleAt(2.0f, 50.0f, 0, 0, 100, 100);
    Color cr = g.sampleAt(98.0f, 50.0f, 0, 0, 100, 100);
    CHECK(cl.r > 0.8f);
    CHECK(cr.b > 0.8f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] radial sampleAt — center vs edge ──
int test_radial_at() {
    std::cout << "[3] radial sampleAt (center→edge)" << std::endl;
    Gradient g = StyleSheet::parseGradient("radial-gradient(circle, #ff0000, #0000ff)");
    // center red (t=0), corner blue (t≈1)
    Color cc = g.sampleAt(50.0f, 50.0f, 0, 0, 100, 100);
    Color ce = g.sampleAt(99.0f, 99.0f, 0, 0, 100, 100);
    CHECK(cc.r > 0.8f);     // center
    CHECK(ce.b > 0.5f);     // far corner toward blue
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] conic sampleAt — angle sweep ──
int test_conic_at() {
    std::cout << "[4] conic sampleAt (angle sweep)" << std::endl;
    Gradient g = StyleSheet::parseGradient("conic-gradient(#ff0000, #0000ff)");
    // top (12 o'clock) is t≈0 (red); bottom is t≈0.5.
    Color top = g.sampleAt(50.0f, 1.0f, 0, 0, 100, 100);
    CHECK(top.r > 0.8f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] repeating wrap ──
int test_repeating() {
    std::cout << "[5] repeating ramp wraps" << std::endl;
    Gradient g = StyleSheet::parseGradient(
        "repeating-linear-gradient(to right, #ff0000 0%, #0000ff 50%)");
    // t=0.0 red, t=0.5 blue, t=1.0 wraps back to red, t=1.5 → blue
    Color a = g.sampleColor(0.0f);
    Color b = g.sampleColor(1.0f);
    CHECK(nearEq(a.r, 1.0f));
    CHECK(nearEq(b.r, 1.0f));     // wrapped to red
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI Gradient Sampling Tests ===" << std::endl;
    int rc = 0;
    rc |= test_ramp();
    rc |= test_linear_at();
    rc |= test_radial_at();
    rc |= test_conic_at();
    rc |= test_repeating();
    if (rc == 0) std::cout << "\nAll gradient sampling tests passed!" << std::endl;
    return rc;
}
