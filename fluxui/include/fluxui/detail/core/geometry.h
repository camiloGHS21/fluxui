#pragma once
// FluxUI public API - geometry & paint primitives.
// Auto-split from core.h; do not include directly, use <fluxui/core.h>.
#include "fluxui/config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace FluxUI {
inline float parseLocaleIndependentFloat(const char* str, char** endptr) {
    if (!str) {
        if (endptr) *endptr = nullptr;
        return 0.0f;
    }
    const char* p = str;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    const char* start = p;
    bool neg = false;
    if (*p == '-') {
        neg = true;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    double val = 0.0;
    bool has_digits = false;
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        has_digits = true;
        ++p;
    }
    if (*p == '.') {
        const char* dot_ptr = p;
        ++p;
        double dec = 1.0;
        bool has_frac_digits = false;
        while (*p >= '0' && *p <= '9') {
            dec *= 0.1;
            val += (*p - '0') * dec;
            has_frac_digits = true;
            ++p;
        }
        if (has_frac_digits) {
            has_digits = true;
        } else if (!has_digits) {
            p = dot_ptr;
        }
    }
    if (!has_digits) {
        if (endptr) *endptr = const_cast<char*>(str);
        return 0.0f;
    }
    if (*p == 'e' || *p == 'E') {
        const char* e_ptr = p;
        ++p;
        bool e_neg = false;
        if (*p == '-') {
            e_neg = true;
            ++p;
        } else if (*p == '+') {
            ++p;
        }
        int exp = 0;
        bool has_exp_digits = false;
        while (*p >= '0' && *p <= '9') {
            exp = exp * 10 + (*p - '0');
            has_exp_digits = true;
            ++p;
        }
        if (has_exp_digits) {
            val *= std::pow(10.0, e_neg ? -exp : exp);
        } else {
            p = e_ptr;
        }
    }
    if (neg) val = -val;
    if (endptr) {
        *endptr = const_cast<char*>(p);
    }
    return static_cast<float>(val);
}
inline float parseLocaleIndependentFloat(const std::string& str, float fallback = 0.0f) {
    char* end = nullptr;
    float val = parseLocaleIndependentFloat(str.c_str(), &end);
    if (end == str.c_str()) return fallback;
    return val;
}
typedef void* NativeWindowHandle;
typedef void* NativeCursorHandle;
enum ModifierBits {
    MOD_NONE = 0,
    MOD_SHIFT = 1 << 0,
    MOD_CTRL = 1 << 1,
    MOD_ALT = 1 << 2,
    MOD_GUI = 1 << 3
};
struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
};
struct Color {
    float r = 0, g = 0, b = 0, a = 1.0f;
    Color() = default;
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    static Color fromHex(const std::string& hex) {
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        if (h.size() == 3) {
            h = {h[0], h[0], h[1], h[1], h[2], h[2]};
        } else if (h.size() == 4) {
            h = {h[0], h[0], h[1], h[1], h[2], h[2], h[3], h[3]};
        }
        if (h.size() != 6 && h.size() != 8) {
            return Color();
        }
        unsigned int val = 0;
        for (char c : h) {
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        }
        if (h.size() == 8) {
            return {
                ((val >> 24) & 0xFF) / 255.0f,
                ((val >> 16) & 0xFF) / 255.0f,
                ((val >> 8)  & 0xFF) / 255.0f,
                ((val)       & 0xFF) / 255.0f
            };
        }
        return {
            ((val >> 16) & 0xFF) / 255.0f,
            ((val >> 8)  & 0xFF) / 255.0f,
            ((val)       & 0xFF) / 255.0f,
            1.0f
        };
    }
    static Color fromHSL(float h, float s, float l, float a = 1.0f) {
        float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = l - c / 2.0f;
        float r1 = 0, g1 = 0, b1 = 0;
        if (h < 60)       { r1 = c; g1 = x; }
        else if (h < 120) { r1 = x; g1 = c; }
        else if (h < 180) { g1 = c; b1 = x; }
        else if (h < 240) { g1 = x; b1 = c; }
        else if (h < 300) { r1 = x; b1 = c; }
        else              { r1 = c; b1 = x; }
        return {r1 + m, g1 + m, b1 + m, a};
    }
    static Color fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }
    // ── CSS Color 4: OKLab / OKLCH / Lab / LCH / HWB conversions ──
    // Matrices and transfer functions per W3C CSS Color Module Level 4.
    static float srgbLinearToGamma(float c) {
        float a = std::abs(c);
        float s = (c < 0.0f) ? -1.0f : 1.0f;
        return (a > 0.0031308f)
            ? s * (1.055f * std::pow(a, 1.0f / 2.4f) - 0.055f)
            : 12.92f * c;
    }
    static float srgbGammaToLinear(float c) {
        float a = std::abs(c);
        float s = (c < 0.0f) ? -1.0f : 1.0f;
        return (a <= 0.04045f) ? c / 12.92f
                               : s * std::pow((a + 0.055f) / 1.055f, 2.4f);
    }
    // OKLab (L,a,b) → linear sRGB (r,g,b)
    static void oklabToLinearSrgb(float L, float a, float b,
                                  float& r, float& g, float& bl) {
        float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
        float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
        float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
        float l = l_ * l_ * l_;
        float m = m_ * m_ * m_;
        float s = s_ * s_ * s_;
        r  =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
        g  = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
        bl = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    }
    // OKLCH → sRGB Color (L in [0,1], C chroma, H degrees)
    static Color fromOklch(float L, float C, float H, float alpha = 1.0f) {
        float hr = H * 3.14159265358979323846f / 180.0f;
        float a = C * std::cos(hr);
        float b = C * std::sin(hr);
        return fromOklab(L, a, b, alpha);
    }
    static Color fromOklab(float L, float a, float b, float alpha = 1.0f) {
        float rl, gl, bl;
        oklabToLinearSrgb(L, a, b, rl, gl, bl);
        Color c;
        c.r = std::clamp(srgbLinearToGamma(rl), 0.0f, 1.0f);
        c.g = std::clamp(srgbLinearToGamma(gl), 0.0f, 1.0f);
        c.b = std::clamp(srgbLinearToGamma(bl), 0.0f, 1.0f);
        c.a = std::clamp(alpha, 0.0f, 1.0f);
        return c;
    }
    // CIE Lab D50 → XYZ → linear sRGB. L in [0,100], a/b ~[-125,125].
    static Color fromLab(float L, float a, float b, float alpha = 1.0f) {
        // Lab → XYZ (D50 white point)
        const float Xn = 0.3457f / 0.3585f; // D50
        const float Yn = 1.0f;
        const float Zn = (1.0f - 0.3457f - 0.3585f) / 0.3585f;
        float fy = (L + 16.0f) / 116.0f;
        float fx = fy + a / 500.0f;
        float fz = fy - b / 200.0f;
        auto finv = [](float t) {
            float t3 = t * t * t;
            const float eps = 216.0f / 24389.0f;
            const float kappa = 24389.0f / 27.0f;
            return (t3 > eps) ? t3 : (116.0f * t - 16.0f) / kappa;
        };
        float xr = finv(fx), yr = finv(fy), zr = finv(fz);
        float X = xr * Xn, Y = yr * Yn, Z = zr * Zn;
        // D50 → D65 Bradford-adapted XYZ → linear sRGB (combined matrix)
        float rl =  3.1341359569f * X - 1.6173275824f * Y - 0.4906621711f * Z;
        float gl = -0.9787553510f * X + 1.9161606866f * Y + 0.0334540303f * Z;
        float bl =  0.0719452861f * X - 0.2289909952f * Y + 1.4052427547f * Z;
        Color c;
        c.r = std::clamp(srgbLinearToGamma(rl), 0.0f, 1.0f);
        c.g = std::clamp(srgbLinearToGamma(gl), 0.0f, 1.0f);
        c.b = std::clamp(srgbLinearToGamma(bl), 0.0f, 1.0f);
        c.a = std::clamp(alpha, 0.0f, 1.0f);
        return c;
    }
    static Color fromLch(float L, float C, float H, float alpha = 1.0f) {
        float hr = H * 3.14159265358979323846f / 180.0f;
        return fromLab(L, C * std::cos(hr), C * std::sin(hr), alpha);
    }
    // HWB (hue, whiteness, blackness) → sRGB. h in degrees, w/b in [0,1].
    static Color fromHWB(float h, float w, float b, float alpha = 1.0f) {
        if (w + b >= 1.0f) {
            float gray = w / (w + b);
            return {gray, gray, gray, alpha};
        }
        Color base = fromHSL(h, 1.0f, 0.5f, alpha);
        auto apply = [&](float ch) { return ch * (1.0f - w - b) + w; };
        return {apply(base.r), apply(base.g), apply(base.b), alpha};
    }
    // color(<colorspace> c1 c2 c3) — supports srgb / srgb-linear / display-p3.
    static Color fromColorFunction(const std::string& space,
                                   float c1, float c2, float c3, float alpha = 1.0f);
    Color withAlpha(float alpha) const { return {r, g, b, alpha}; }
    static Color lerp(const Color& a, const Color& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return {
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        };
    }
    Vec4 toVec4() const { return {r, g, b, a}; }
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

// color(<colorspace> c1 c2 c3 [/ alpha]) — CSS Color 4.
// Supports: srgb, srgb-linear, display-p3 (mapped into sRGB gamut).
inline Color Color::fromColorFunction(const std::string& space,
                                       float c1, float c2, float c3, float alpha) {
    if (space == "srgb") {
        return {std::clamp(c1, 0.0f, 1.0f), std::clamp(c2, 0.0f, 1.0f),
                std::clamp(c3, 0.0f, 1.0f), std::clamp(alpha, 0.0f, 1.0f)};
    }
    if (space == "srgb-linear") {
        return {std::clamp(srgbLinearToGamma(c1), 0.0f, 1.0f),
                std::clamp(srgbLinearToGamma(c2), 0.0f, 1.0f),
                std::clamp(srgbLinearToGamma(c3), 0.0f, 1.0f),
                std::clamp(alpha, 0.0f, 1.0f)};
    }
    if (space == "display-p3") {
        // Display-P3 (gamma) → linear → XYZ(D65) → linear sRGB → gamma sRGB
        float rl = srgbGammaToLinear(c1);
        float gl = srgbGammaToLinear(c2);
        float bl = srgbGammaToLinear(c3);
        // P3 linear → XYZ D65
        float X = 0.4865709486f * rl + 0.2656676932f * gl + 0.1982172852f * bl;
        float Y = 0.2289745641f * rl + 0.6917385218f * gl + 0.0792869141f * bl;
        float Z = 0.0000000000f * rl + 0.0451133819f * gl + 1.0439443689f * bl;
        // XYZ D65 → linear sRGB
        float r =  3.2409699419f * X - 1.5373831776f * Y - 0.4986107603f * Z;
        float g = -0.9692436363f * X + 1.8759675015f * Y + 0.0415550574f * Z;
        float b =  0.0556300797f * X - 0.2039769589f * Y + 1.0569715142f * Z;
        return {std::clamp(srgbLinearToGamma(r), 0.0f, 1.0f),
                std::clamp(srgbLinearToGamma(g), 0.0f, 1.0f),
                std::clamp(srgbLinearToGamma(b), 0.0f, 1.0f),
                std::clamp(alpha, 0.0f, 1.0f)};
    }
    // Unknown space → treat components as sRGB
    return {std::clamp(c1, 0.0f, 1.0f), std::clamp(c2, 0.0f, 1.0f),
            std::clamp(c3, 0.0f, 1.0f), std::clamp(alpha, 0.0f, 1.0f)};
}

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
    bool contains(Vec2 p) const {
        // An empty (zero/negative area) rect contains no points. This matters
        // because un-laid-out widgets default to {0,0,0,0}; without this guard a
        // mouse resting at the origin (0,0) would be reported as "inside" every
        // such widget, flickering their hover state and pinning the app awake.
        if (w <= 0.0f || h <= 0.0f) return false;
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
    Vec2 center() const { return {x + w / 2, y + h / 2}; }
    Vec2 size() const { return {w, h}; }
    Rect shrink(float amount) const {
        return {x + amount, y + amount, w - 2 * amount, h - 2 * amount};
    }
};
struct EdgeInsets {
    float top = 0, right = 0, bottom = 0, left = 0;
    EdgeInsets() = default;
    EdgeInsets(float all) : top(all), right(all), bottom(all), left(all) {}
    EdgeInsets(float v, float h) : top(v), right(h), bottom(v), left(h) {}
    EdgeInsets(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}
    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
    bool operator==(const EdgeInsets& o) const {
        return top == o.top && right == o.right && bottom == o.bottom && left == o.left;
    }
    bool operator!=(const EdgeInsets& o) const { return !(*this == o); }
};
struct BorderRadius {
    float tl = 0, tr = 0, br = 0, bl = 0;
    BorderRadius() = default;
    BorderRadius(float all) : tl(all), tr(all), br(all), bl(all) {}
    float maxRadius() const { return std::max({tl, tr, br, bl}); }
    float uniform() const { return tl; }
    bool operator==(const BorderRadius& o) const {
        return tl == o.tl && tr == o.tr && br == o.br && bl == o.bl;
    }
    bool operator!=(const BorderRadius& o) const { return !(*this == o); }
};
struct BoxShadow {
    float offsetX = 0, offsetY = 0;
    float blur = 0, spread = 0;
    Color color = Color(0, 0, 0, 0.0f);
    bool inset = false;
    bool operator==(const BoxShadow& o) const {
        return offsetX == o.offsetX && offsetY == o.offsetY &&
               blur == o.blur && spread == o.spread &&
               color == o.color && inset == o.inset;
    }
    bool operator!=(const BoxShadow& o) const { return !(*this == o); }
};
// CSS text-shadow layer (CSS Text Decoration L3). offset-x offset-y [blur] color.
// Unlike box-shadow there is no spread and no inset. text-shadow takes a
// comma-separated list of layers painted back-to-front.
struct TextShadow {
    float offsetX = 0, offsetY = 0;
    float blur = 0;
    Color color = Color(0, 0, 0, 0.0f);
    bool operator==(const TextShadow& o) const {
        return offsetX == o.offsetX && offsetY == o.offsetY &&
               blur == o.blur && color == o.color;
    }
    bool operator!=(const TextShadow& o) const { return !(*this == o); }
};

// ============================================================
//  CSS Filter Operations (Blink FilterOperation parity)
//  Mirrors Blink's FilterOperation::OperationType enum and
//  the per-function data model from filter_operation.h.
//
//  Standard CSS <filter-function> types:
//    blur / brightness / contrast / drop-shadow / grayscale /
//    hue-rotate / invert / opacity / saturate / sepia / url()
//
//  SVG-parity extras (Blink kLuminanceToAlpha, kColorMatrix):
//    luminance-to-alpha  — feColorMatrix type="luminanceToAlpha"
//    color-matrix        — feColorMatrix with explicit 20-value matrix
// ============================================================
enum class FilterOperationType {
    // ── Standard CSS filter functions ──────────────────────
    Blur,               // blur(<length>)
    Brightness,         // brightness(<number-or-percent>)   [0,∞), clamped ≥0
    Contrast,           // contrast(<number-or-percent>)     [0,∞)
    DropShadow,         // drop-shadow(<shadow>)
    Grayscale,          // grayscale(<number-or-percent>)    [0,1]
    HueRotate,          // hue-rotate(<angle>)               degrees
    Invert,             // invert(<number-or-percent>)       [0,1]
    Opacity,            // opacity(<number-or-percent>)      [0,1]
    Saturate,           // saturate(<number-or-percent>)     [0,∞)
    Sepia,              // sepia(<number-or-percent>)        [0,1]
    Reference,          // url(#svg-filter)
    // ── SVG filter primitive equivalents (Blink parity) ────
    LuminanceToAlpha,   // Blink kLuminanceToAlpha — feColorMatrix type="luminanceToAlpha"
    ColorMatrix,        // Blink kColorMatrix      — feColorMatrix with 20-value matrix
};

// ── UseCounter: lightweight Blink WebFeature parity ─────────
// Records which CSS filter functions are used, matching the
// CountFilterUse / WebFeature pattern from Blink's
// filter_operation_resolver.cc.
enum class FilterFeature {
    Blur,
    Brightness,
    Contrast,
    DropShadow,
    Grayscale,
    HueRotate,
    Invert,
    Opacity,
    Saturate,
    Sepia,
    Reference,
    LuminanceToAlpha,
    ColorMatrix,
    COUNT  // keep last
};

// Global filter feature use-counter (Blink WebFeature parity).
// Thread-unsafe by design — same as Blink's per-document counters.
struct FilterUseCounter {
    uint32_t counts[static_cast<int>(FilterFeature::COUNT)] = {};

    void count(FilterFeature f) {
        counts[static_cast<int>(f)]++;
    }
    uint32_t get(FilterFeature f) const {
        return counts[static_cast<int>(f)];
    }
    void reset() {
        std::fill(std::begin(counts), std::end(counts), 0u);
    }
    // Returns the global singleton (Blink document-level parity).
    static FilterUseCounter& instance() {
        static FilterUseCounter inst;
        return inst;
    }
};

struct FilterOperation {
    FilterOperationType type = FilterOperationType::Blur;

    // ── Scalar amount ──────────────────────────────────────
    // For blur: stdDeviation in px (resolved from calc() at parse time).
    // For brightness/contrast/saturate: multiplier [0,∞).
    // For grayscale/invert/opacity/sepia: clamped [0,1] (Blink parity).
    // For hue-rotate: degrees.
    // For LuminanceToAlpha: unused (amount = 0).
    float amount = 0.0f;

    // ── drop-shadow fields ─────────────────────────────────
    float shadowOffsetX = 0.0f;
    float shadowOffsetY = 0.0f;
    float shadowBlur    = 0.0f;
    Color shadowColor   = Color(0, 0, 0, 1.0f);

    // ── ColorMatrix: 20-value RGBA matrix (Blink kColorMatrix) ──
    // Laid out as 4 rows × 5 columns (feColorMatrix values attribute).
    // Empty for all other types.
    std::vector<float> colorMatrixValues;

    // ── url() reference ────────────────────────────────────
    std::string url;

    bool operator==(const FilterOperation& o) const {
        return type == o.type && amount == o.amount &&
               shadowOffsetX == o.shadowOffsetX && shadowOffsetY == o.shadowOffsetY &&
               shadowBlur == o.shadowBlur && shadowColor == o.shadowColor &&
               colorMatrixValues == o.colorMatrixValues &&
               url == o.url;
    }
    bool operator!=(const FilterOperation& o) const { return !(*this == o); }
};
struct Border {
    float width = 0;
    Color color;
    Border() = default;
    Border(float w, Color c) : width(w), color(c) {}
    bool operator==(const Border& o) const {
        return width == o.width && color == o.color;
    }
    bool operator!=(const Border& o) const { return !(*this == o); }
};
struct Gradient {
    enum Type { None, Linear, Radial, Conic };
    enum RadialShape { Ellipse, Circle };
    enum RadialExtent { FarthestCorner, FarthestSide, ClosestCorner, ClosestSide };
    Type type = None;
    float angle = 180.0f;                 // Linear: direction; Conic: from-angle
    bool repeating = false;               // repeating-*-gradient
    // Radial / Conic shape & position (fractions of the box, 0..1).
    RadialShape radialShape = Ellipse;
    RadialExtent radialExtent = FarthestCorner;
    bool hasExplicitRadius = false;
    Vec2 radius = {0.0f, 0.0f};            // explicit radial radii (px), if set
    Vec2 center = {0.5f, 0.5f};            // position (fraction of box)
    bool hasCenter = false;
    std::vector<std::pair<Color, float>> stops;
    static Gradient linear(float angle, std::initializer_list<std::pair<Color, float>> stops) {
        Gradient g;
        g.type = Linear;
        g.angle = angle;
        g.stops = stops;
        return g;
    }
    static Gradient lerp(const Gradient& a, const Gradient& b, float t) {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        if (a.type == None) return b;
        if (b.type == None) return a;
        Gradient result = a;
        result.type = a.type;
        result.angle = a.angle + (b.angle - a.angle) * t;
        result.center = {a.center.x + (b.center.x - a.center.x) * t,
                         a.center.y + (b.center.y - a.center.y) * t};
        size_t size = std::min(a.stops.size(), b.stops.size());
        result.stops.clear();
        result.stops.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            Color c = Color::lerp(a.stops[i].first, b.stops[i].first, t);
            float pos = a.stops[i].second + (b.stops[i].second - a.stops[i].second) * t;
            result.stops.emplace_back(c, pos);
        }
        return result;
    }
};
}