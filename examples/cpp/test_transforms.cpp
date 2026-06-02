// FluxUI CSS Transforms (Level 1 + 2) Parity Test
// Mirrors W3C CSS Transforms examples and Blink's transform_operations_test.cc.
//
// Reference: https://www.w3.org/TR/css-transforms-1/
//            https://www.w3.org/TR/css-transforms-2/
//            chromium/src/third_party/blink/renderer/core/css/transform_operations_test.cc
//            chromium/src/third_party/blink/renderer/platform/graphics/transforms/transformation_matrix_test.cc

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace FluxUI;

#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL: " << #cond << " @ line " << __LINE__ << std::endl; return 1; } } while (0)
#define CHECK_NEAR(a, b, eps) do { float aa = (a), bb = (b); if (std::abs(aa - bb) > (eps)) { std::cerr << "FAIL: " << #a << " (" << aa << ") ~= " << #b << " (" << bb << ") @ line " << __LINE__ << std::endl; return 1; } } while (0)

int test_transform2d_basics() {
    std::cout << "[1] Transform2D basics: multiply, mapPoint, inverse..." << std::endl;
    Transform2D a = Transform2D::fromTranslate(10, 20);
    Transform2D b = Transform2D::fromScale(2, 3);
    Transform2D c = a.multiplied(b);
    // c applied to (1,1) → b: (2,3) → a: (12, 23)
    Vec2 p = c.mapPoint({1, 1});
    CHECK_NEAR(p.x, 12.0f, 1e-4f);
    CHECK_NEAR(p.y, 23.0f, 1e-4f);
    Vec2 back = c.inverseMapPoint(p);
    CHECK_NEAR(back.x, 1.0f, 1e-4f);
    CHECK_NEAR(back.y, 1.0f, 1e-4f);
    CHECK(Transform2D::identity().isIdentity());
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_transform2d_rotate() {
    std::cout << "[2] Transform2D: rotate 90deg around origin..." << std::endl;
    // MSVC doesn't define M_PI by default; use a local constant.
    constexpr float kPi = 3.14159265358979323846f;
    Transform2D r = Transform2D::fromRotate(kPi / 2.0f);
    Transform2D r2 = Transform2D::fromRotate(kPi / 2.0f);
    Vec2 p = r2.mapPoint({1, 0});
    CHECK_NEAR(p.x, 0.0f, 1e-4f);
    CHECK_NEAR(p.y, 1.0f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_parse_simple_functions() {
    std::cout << "[3] Parse simple transform functions..." << std::endl;
    auto list = StyleSheet::parseTransformList("translate(10px, 20px) rotate(45deg) scale(1.5)");
    CHECK(list.size() == 3);
    CHECK(list[0].kind == TransformOperation::Translate);
    CHECK_NEAR(list[0].v[0], 10.0f, 1e-4f);
    CHECK_NEAR(list[0].v[1], 20.0f, 1e-4f);
    CHECK(list[1].kind == TransformOperation::Rotate);
    CHECK_NEAR(list[1].v[0], static_cast<float>(3.14159265f / 4.0), 1e-3f);
    CHECK(list[2].kind == TransformOperation::Scale);
    CHECK_NEAR(list[2].v[0], 1.5f, 1e-4f);
    CHECK_NEAR(list[2].v[1], 1.5f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_parse_full_grammar() {
    std::cout << "[4] Parse full CSS Transforms Level 1+2 grammar..." << std::endl;
    // 21 transform functions in a single whitespace-separated list.
    const char* css =
        "translate(10px) translateX(20px) translateY(30px) translateZ(40px) "
        "translate3d(1px, 2px, 3px) "
        "scale(1.5) scaleX(2) scaleY(3) scaleZ(4) scale3d(0.5, 0.6, 0.7) "
        "rotate(45deg) rotateX(30deg) rotateY(60deg) "
        "rotateZ(90deg) "
        "rotate3d(1, 0, 0, 90deg) "
        "skew(10deg, 20deg) skewX(15deg) skewY(25deg) "
        "matrix(1, 0, 0, 1, 0, 0) matrix3d(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1) "
        "perspective(500px)";
    auto list = StyleSheet::parseTransformList(css);
    CHECK(list.size() == 21);
    // Verify every kind is present.
    bool sawTranslate = false, sawScale = false, sawRotate = false, sawSkew = false;
    bool sawMatrix = false, sawPerspective = false;
    bool sawTranslate3D = false, sawScale3D = false, sawRotate3D = false;
    bool sawMatrix3D = false;
    for (const auto& op : list) {
        if (op.kind == TransformOperation::Translate) {
            sawTranslate = true;
            if (op.dim == 3) sawTranslate3D = true;
        }
        if (op.kind == TransformOperation::Scale) {
            sawScale = true;
            if (op.dim == 3) sawScale3D = true;
        }
        if (op.kind == TransformOperation::Rotate) {
            sawRotate = true;
            if (op.dim == 3) sawRotate3D = true;
        }
        if (op.kind == TransformOperation::Skew)   sawSkew = true;
        if (op.kind == TransformOperation::Matrix) {
            sawMatrix = true;
            if (op.dim == 3) sawMatrix3D = true;
        }
        if (op.kind == TransformOperation::Perspective) sawPerspective = true;
    }
    CHECK(sawTranslate);
    CHECK(sawScale);
    CHECK(sawRotate);
    CHECK(sawSkew);
    CHECK(sawMatrix);
    CHECK(sawPerspective);
    CHECK(sawTranslate3D);
    CHECK(sawScale3D);
    CHECK(sawRotate3D);
    CHECK(sawMatrix3D);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_parse_none_and_invalid() {
    std::cout << "[5] Parse 'none' and invalid input gracefully..." << std::endl;
    CHECK(StyleSheet::parseTransformList("none").empty());
    CHECK(StyleSheet::parseTransformList("").empty());
    CHECK(StyleSheet::parseTransformList("garbage").empty());
    // Malformed function (missing close paren) → empty list (Blink drops the whole list).
    CHECK(StyleSheet::parseTransformList("scale(1.5").empty());
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_parse_transform_origin() {
    std::cout << "[6] Parse transform-origin (keywords, %, px)..." << std::endl;
    Vec2 xy; float z;
    CHECK(StyleSheet::parseTransformOrigin("center", xy, z));
    CHECK_NEAR(xy.x, 0.5f, 1e-4f);
    CHECK_NEAR(xy.y, 0.5f, 1e-4f);
    CHECK(StyleSheet::parseTransformOrigin("left top", xy, z));
    CHECK_NEAR(xy.x, 0.0f, 1e-4f);
    CHECK_NEAR(xy.y, 0.0f, 1e-4f);
    CHECK(StyleSheet::parseTransformOrigin("right bottom 10px", xy, z));
    CHECK_NEAR(xy.x, 1.0f, 1e-4f);
    CHECK_NEAR(xy.y, 1.0f, 1e-4f);
    CHECK_NEAR(z, 10.0f, 1e-4f);
    CHECK(StyleSheet::parseTransformOrigin("50% 50%", xy, z));
    CHECK_NEAR(xy.x, 0.5f, 1e-4f);
    CHECK_NEAR(xy.y, 0.5f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_parse_related_enums() {
    std::cout << "[7] Parse transform-style, transform-box, backface-visibility..." << std::endl;
    TransformStyle ts;
    CHECK(StyleSheet::parseTransformStyle("flat", ts));
    CHECK(ts == TransformStyle::Flat);
    CHECK(StyleSheet::parseTransformStyle("preserve-3d", ts));
    CHECK(ts == TransformStyle::Preserve3d);
    TransformBox tb;
    CHECK(StyleSheet::parseTransformBox("border-box", tb));
    CHECK(tb == TransformBox::BorderBox);
    CHECK(StyleSheet::parseTransformBox("fill-box", tb));
    CHECK(tb == TransformBox::FillBox);
    BackfaceVisibility bv;
    CHECK(StyleSheet::parseBackfaceVisibility("hidden", bv));
    CHECK(bv == BackfaceVisibility::Hidden);
    CHECK(StyleSheet::parseBackfaceVisibility("visible", bv));
    CHECK(bv == BackfaceVisibility::Visible);
    float px;
    CHECK(StyleSheet::parsePerspective("none", px));
    CHECK_NEAR(px, 0.0f, 1e-6f);
    CHECK(StyleSheet::parsePerspective("800px", px));
    CHECK_NEAR(px, 800.0f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_full_css_property_parse() {
    std::cout << "[8] Parse transform + related properties in a full StyleSheet..." << std::endl;
    StyleSheet sheet;
    sheet.parse(
        ".x {"
        "  transform: rotate(45deg) scale(1.5);"
        "  transform-origin: 50% 50%;"
        "  transform-style: preserve-3d;"
        "  transform-box: fill-box;"
        "  perspective: 800px;"
        "  perspective-origin: 50% 50%;"
        "  backface-visibility: hidden;"
        "}");
    auto widget = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x", "", "panel", {}, nullptr, widget.get());
    CHECK(s.transform.size() == 2);
    CHECK(s.transform[0].kind == TransformOperation::Rotate);
    CHECK(s.transform[1].kind == TransformOperation::Scale);
    CHECK(s.hasTransformOrigin);
    CHECK(s.hasTransformStyle);
    CHECK(s.transformStyle == TransformStyle::Preserve3d);
    CHECK(s.hasTransformBox);
    CHECK(s.transformBox == TransformBox::FillBox);
    CHECK(s.hasPerspective);
    CHECK_NEAR(s.perspective, 800.0f, 1e-4f);
    CHECK(s.hasPerspectiveOrigin);
    CHECK(s.hasBackfaceVisibility);
    CHECK(s.backfaceVisibility == BackfaceVisibility::Hidden);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_transform_blend_same_kind() {
    std::cout << "[9] Per-operation blend: same-kind ops interpolate linearly..." << std::endl;
    TransformOperation a = TransformOperation::scaleOp(1.0f, 1.0f);
    TransformOperation b = TransformOperation::scaleOp(2.0f, 2.0f);
    TransformOperation mid = TransformOperation::blend(a, b, 0.5f);
    CHECK(mid.kind == TransformOperation::Scale);
    CHECK_NEAR(mid.v[0], 1.5f, 1e-4f);
    CHECK_NEAR(mid.v[1], 1.5f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_transform_blend_rotate_shortest_path() {
    std::cout << "[10] Rotate blend uses shortest angular path..." << std::endl;
    // Going from 350° to 10° should interpolate through 0° (not through 180°).
    constexpr float kPi = 3.14159265358979323846f;
    TransformOperation a = TransformOperation::rotate(350.0f * kPi / 180.0f);
    TransformOperation b = TransformOperation::rotate( 10.0f * kPi / 180.0f);
    TransformOperation mid = TransformOperation::blend(a, b, 0.5f);
    float deg = mid.v[0] * 180.0f / kPi;
    // The shortest path is 20° total (from 350° to 10° via 0°), so the
    // midpoint is 0° (or equivalently 360°).
    CHECK(std::abs(deg) < 0.5f || std::abs(deg - 360.0f) < 0.5f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_interpolate_transform_list_helper() {
    std::cout << "[11] interpolateTransformList: scale(1) → scale(2)..." << std::endl;
    std::string out = StyleSheet::interpolateTransformList("scale(1)", "scale(2)", 0.5f);
    CHECK(out.find("scale") != std::string::npos);
    CHECK(out.find("1.5") != std::string::npos);
    // Different lengths → discrete swap (t < 0.5 keeps the from value).
    std::string out2 = StyleSheet::interpolateTransformList("scale(1)", "scale(1) rotate(45deg)", 0.3f);
    CHECK(out2 == "scale(1)");
    std::cout << "  PASS" << std::endl;
    return 0;
}

int test_pivot_compensation_in_apply() {
    std::cout << "[12] Per-op pivot compensation: scale around origin (5,5)..." << std::endl;
    // Applying scale(2) around the origin (5,5) to the point (10,10) should
    // produce (15,15):  T(5,5) * S(2) * T(-5,-5) * (10,10)
    //                       = T(5,5) * S(2) * (5,5)
    //                       = T(5,5) * (10,10)
    //                       = (15,15)
    Transform2D m = Transform2D::identity();
    TransformOperation s = TransformOperation::scaleOp(2.0f, 2.0f);
    m = s.apply(m, {5, 5});
    Vec2 p = m.mapPoint({10, 10});
    CHECK_NEAR(p.x, 15.0f, 1e-4f);
    CHECK_NEAR(p.y, 15.0f, 1e-4f);
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI CSS Transforms (Level 1+2) Parity Tests ===" << std::endl;
    int rc = 0;
    rc |= test_transform2d_basics();
    rc |= test_transform2d_rotate();
    rc |= test_parse_simple_functions();
    rc |= test_parse_full_grammar();
    rc |= test_parse_none_and_invalid();
    rc |= test_parse_transform_origin();
    rc |= test_parse_related_enums();
    rc |= test_full_css_property_parse();
    rc |= test_transform_blend_same_kind();
    rc |= test_transform_blend_rotate_shortest_path();
    rc |= test_interpolate_transform_list_helper();
    rc |= test_pivot_compensation_in_apply();
    if (rc == 0) {
        std::cout << "All transform tests passed!" << std::endl;
    } else {
        std::cerr << "Some transform tests failed (rc=" << rc << ")" << std::endl;
    }
    return rc;
}
