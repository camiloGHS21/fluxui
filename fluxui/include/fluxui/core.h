#pragma once
#include "fluxui/config.h"
#include <string>
#include "fluxui/atomic_string.h"
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
    enum Type { None, Linear, Radial };
    Type type = None;
    float angle = 180.0f;
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
        Gradient result;
        result.type = a.type;
        result.angle = a.angle + (b.angle - a.angle) * t;
        size_t size = std::min(a.stops.size(), b.stops.size());
        result.stops.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            Color c = Color::lerp(a.stops[i].first, b.stops[i].first, t);
            float pos = a.stops[i].second + (b.stops[i].second - a.stops[i].second) * t;
            result.stops.emplace_back(c, pos);
        }
        return result;
    }
};
enum class Display {
    Block,
    Flex,
    Grid,
    InlineBlock,
    Inline,
    None,
    ListItem,
    Table,
    TableRowGroup,
    TableHeaderGroup,
    TableFooterGroup,
    TableRow,
    TableCell,
    TableColumn,
    TableColumnGroup,
    TableCaption,
    Contents
};
enum class Direction { Ltr, Rtl };
enum class UnicodeBidi { Normal, Embed, BidiOverride, Isolate, IsolateOverride, Plaintext };
enum class WritingMode { HorizontalTb, VerticalRl, VerticalLr };
enum class FlexDirection { Row, Column, RowReverse, ColumnReverse };
enum class FlexWrap { NoWrap, Wrap, WrapReverse };
enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignItems { FlexStart, FlexEnd, Center, Stretch, Baseline };
enum class AlignContent { FlexStart, FlexEnd, Center, Stretch, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignSelf { Auto, FlexStart, FlexEnd, Center, Stretch, Baseline };
// ── CSS Grid enums (Blink parity) ──────────────────────────
enum class JustifyItems { Auto, Normal, Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class JustifySelf  { Auto, Normal, Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class GridAutoFlow { Row, Column, RowDense, ColumnDense };
// ── Grid track sizing function type (Blink GridTrackSize parity) ──
enum class GridTrackSizeType {
    Fixed,        // <length> or <percentage>
    Flex,         // <number>fr
    MinMax,       // minmax(<min>, <max>)
    FitContent,   // fit-content(<length>)
    Auto,         // auto
    MinContent,   // min-content
    MaxContent,   // max-content
    Subgrid,      // subgrid keyword
};
// ── Single grid track definition (Blink GridLength/GridTrackSize parity) ──
struct GridTrackSize {
    GridTrackSizeType type = GridTrackSizeType::Auto;
    float value   = 0.0f;   // resolved px for Fixed; fr value for Flex
    float minValue= 0.0f;   // minmax: min side (px after resolve)
    float maxValue= 0.0f;   // minmax: max side (px after resolve)
    GridTrackSizeType minType = GridTrackSizeType::Auto;
    GridTrackSizeType maxType = GridTrackSizeType::Auto;
    std::string namedLine;  // optional [name] for named grid lines

    bool isFlex()     const { return type == GridTrackSizeType::Flex; }
    bool isAuto()     const { return type == GridTrackSizeType::Auto; }
    bool isFixed()    const { return type == GridTrackSizeType::Fixed; }
    bool isSubgrid()  const { return type == GridTrackSizeType::Subgrid; }

    bool operator==(const GridTrackSize& o) const {
        return type == o.type && value == o.value &&
               minValue == o.minValue && maxValue == o.maxValue &&
               minType == o.minType && maxType == o.maxType &&
               namedLine == o.namedLine;
    }
    bool operator!=(const GridTrackSize& o) const { return !(*this == o); }
};
// ── Grid placement value (Blink GridPosition parity) ──────
// Covers: auto | <integer> | <integer> span | span <integer> | [name]
struct GridPlacement {
    enum class PlacementType { Auto, Line, Span, NamedLine };
    PlacementType type = PlacementType::Auto;
    int          line  = 0;   // 1-based; negative counts from end
    int          span  = 1;   // span count
    std::string  name;        // named line reference

    bool isAuto() const { return type == PlacementType::Auto; }

    bool operator==(const GridPlacement& o) const {
        return type == o.type && line == o.line && span == o.span && name == o.name;
    }
    bool operator!=(const GridPlacement& o) const { return !(*this == o); }
};
// ── Parsed grid-template-areas matrix ─────────────────────
struct GridTemplateAreas {
    std::vector<std::string> areas;   // row-major named areas, "." = anonymous
    int rowCount    = 0;
    int columnCount = 0;
    bool operator==(const GridTemplateAreas& o) const {
        return areas == o.areas && rowCount == o.rowCount && columnCount == o.columnCount;
    }
};
enum class Position { Static, Relative, Absolute, Fixed, Sticky };
enum class CSSFloat { None, Left, Right };
enum class CSSClear { None, Left, Right, Both };
enum class Overflow { Visible, Hidden, Scroll, Auto, Clip };
enum class TextAlign { Left, Center, Right, Justify };
enum class FontWeight { Normal, Bold };
enum class FontStyle { Normal, Italic, Oblique };
enum class CursorType { Default, Pointer, Text, Grab, Grabbing, NotAllowed, Crosshair, ResizeNWSE, ResizeNS };
enum class BoxSizing { ContentBox, BorderBox };
enum class Visibility { Visible, Hidden, Collapse };
enum class TextOverflow { Clip, Ellipsis };
enum class WhiteSpace { Normal, NoWrap, Pre, PreWrap, PreLine };
enum class TextDecoration { None, Underline, LineThrough, Overline };
enum class TextTransform { None, Uppercase, Lowercase, Capitalize };
enum class PointerEvents { Auto, None };
enum class WordBreak { Normal, BreakAll, KeepAll, BreakWord };
enum class ObjectFit { Fill, Contain, Cover, None, ScaleDown };
enum class ListStyleType {
    None,
    Disc,
    Circle,
    Square,
    Decimal,
    DecimalLeadingZero,
    LowerRoman,
    UpperRoman,
    LowerAlpha,
    UpperAlpha
};
enum class VerticalAlign { Baseline, Sub, Super, Middle, Top, Bottom, TextTop, TextBottom };
enum class Appearance {
    Auto,
    None,
    TextField,
    SearchField,
    PushButton,
    Button,
    Checkbox,
    Radio,
    Menulist,
    Textarea,
    SliderHorizontal,
    SquareButton
};
enum ContainmentFlags : uint8_t {
    kContainNone = 0,
    kContainSize = 1 << 0,
    kContainLayout = 1 << 1,
    kContainPaint = 1 << 2,
    kContainStyle = 1 << 3,
    kContainContent = kContainLayout | kContainPaint | kContainStyle,
    kContainStrict = kContainSize | kContainLayout | kContainPaint | kContainStyle
};
enum class AnimationDirection {
    Normal,
    Reverse,
    Alternate,
    AlternateReverse
};
enum class AnimationFillMode {
    None,
    Forwards,
    Backwards,
    Both
};
enum class AnimationPlayState {
    Running,
    Paused
};
enum class AnimationComposition {
    Replace,
    Add,
    Accumulate
};
enum class TransitionBehavior {
    Normal,
    AllowDiscrete
};
struct TimingFunction {
    enum Kind {
        Linear,
        Ease,
        EaseIn,
        EaseOut,
        EaseInOut,
        CubicBezier,
        StepStart,
        StepEnd,
        Steps
    };
    Kind kind = Kind::Ease;
    float params[4] = {0.25f, 0.1f, 0.25f, 1.0f};
    int stepCount = 1;
    enum StepPosition { JumpStart, JumpEnd, JumpNone, JumpBoth, Start, End };
    StepPosition stepPosition = JumpEnd;
    static TimingFunction linear()    { TimingFunction t; t.kind = Linear;    return t; }
    static TimingFunction ease()      { TimingFunction t; t.kind = Ease;      return t; }
    static TimingFunction easeIn()    { TimingFunction t; t.kind = EaseIn;    t.params[0]=0.42f; t.params[1]=0.0f; t.params[2]=1.0f; t.params[3]=1.0f; return t; }
    static TimingFunction easeOut()   { TimingFunction t; t.kind = EaseOut;   t.params[0]=0.0f;  t.params[1]=0.0f; t.params[2]=0.58f; t.params[3]=1.0f; return t; }
    static TimingFunction easeInOut() { TimingFunction t; t.kind = EaseInOut; t.params[0]=0.42f; t.params[1]=0.0f; t.params[2]=0.58f; t.params[3]=1.0f; return t; }
    static TimingFunction stepStart() { TimingFunction t; t.kind = StepStart; return t; }
    static TimingFunction stepEnd()   { TimingFunction t; t.kind = StepEnd;   return t; }
    static TimingFunction steps(int n, StepPosition pos = JumpEnd) {
        TimingFunction t; t.kind = Steps; t.stepCount = std::max(1, n); t.stepPosition = pos; return t;
    }
    static TimingFunction bezier(float x1, float y1, float x2, float y2) {
        TimingFunction t; t.kind = CubicBezier;
        t.params[0] = x1; t.params[1] = y1; t.params[2] = x2; t.params[3] = y2;
        return t;
    }
};
class CSSMathExpressionNode;
struct CSSValue {
    enum Unit {
        Px, Percent, Auto, None, Vw, Vh, Em, Rem, MinContent, MaxContent, FitContent, Ch, Lh, Vi, Vb, Dvw, Dvh,
        Vmin, Vmax, Rlh, Ex, Ic, Cap,
        SvW, SvH, SvMin, SvMax,
        LvW, LvH, LvMin, LvMax,
        DvMin, DvMax,
        Cqw, Cqh, Cqi, Cqb, Cqmin, Cqmax,
        Deg, Rad, Grad, Turn
    };
    float value = 0;
    Unit unit = None;
    enum CalcOp { CalcNone, CalcAdd, CalcSub, CalcMul, CalcDiv, CalcMin, CalcMax, CalcClamp };
    CalcOp calcOp = CalcNone;
    float calcValue2 = 0;
    Unit calcUnit2 = None;
    float calcValue3 = 0;
    Unit calcUnit3 = None;
    std::shared_ptr<CSSMathExpressionNode> mathExpr;
    CSSValue() = default;
    CSSValue(float v, Unit u = Px) : value(v), unit(u) {}
    CSSValue(std::shared_ptr<CSSMathExpressionNode> expr);
    operator float() const { return resolve(1.0f); }
    static CSSValue px(float v) { return {v, Px}; }
    static CSSValue pct(float v) { return {v, Percent}; }
    static CSSValue autoVal() { return {0, Auto}; }
    static CSSValue vw(float v) { return {v, Vw}; }
    static CSSValue vh(float v) { return {v, Vh}; }
    static CSSValue em(float v) { return {v, Em}; }
    static CSSValue rem(float v) { return {v, Rem}; }
    static CSSValue ch(float v) { return {v, Ch}; }
    static CSSValue lh(float v) { return {v, Lh}; }
    static CSSValue vi(float v) { return {v, Vi}; }
    static CSSValue vb(float v) { return {v, Vb}; }
    static CSSValue dvw(float v) { return {v, Dvw}; }
    static CSSValue dvh(float v) { return {v, Dvh}; }
    static CSSValue vmin(float v) { return {v, Vmin}; }
    static CSSValue vmax(float v) { return {v, Vmax}; }
    static CSSValue rlh(float v) { return {v, Rlh}; }
    static CSSValue ex(float v) { return {v, Ex}; }
    static CSSValue ic(float v) { return {v, Ic}; }
    static CSSValue cap(float v) { return {v, Cap}; }
    static CSSValue svw(float v) { return {v, SvW}; }
    static CSSValue svh(float v) { return {v, SvH}; }
    static CSSValue svmin(float v) { return {v, SvMin}; }
    static CSSValue svmax(float v) { return {v, SvMax}; }
    static CSSValue lvw(float v) { return {v, LvW}; }
    static CSSValue lvh(float v) { return {v, LvH}; }
    static CSSValue lvmin(float v) { return {v, LvMin}; }
    static CSSValue lvmax(float v) { return {v, LvMax}; }
    static CSSValue dvmin(float v) { return {v, DvMin}; }
    static CSSValue dvmax(float v) { return {v, DvMax}; }
    static CSSValue cqw(float v) { return {v, Cqw}; }
    static CSSValue cqh(float v) { return {v, Cqh}; }
    static CSSValue cqi(float v) { return {v, Cqi}; }
    static CSSValue cqb(float v) { return {v, Cqb}; }
    static CSSValue cqmin(float v) { return {v, Cqmin}; }
    static CSSValue cqmax(float v) { return {v, Cqmax}; }
    static CSSValue minContent() { return {0, MinContent}; }
    static CSSValue maxContent() { return {0, MaxContent}; }
    static CSSValue fitContent() { return {0, FitContent}; }
    float resolve(float parentSize, float viewportW = 1920.0f, float viewportH = 1080.0f, float emBase = 16.0f) const;
    bool isAuto() const { return unit == Auto; }
    bool isSet() const;
    bool isIntrinsic() const { return unit == MinContent || unit == MaxContent || unit == FitContent; }
    bool operator==(const CSSValue& o) const;
    bool operator!=(const CSSValue& o) const { return !(*this == o); }
    static float resolveUnit(float val, Unit u, float parentSize, float vpW, float vpH, float emBase) {
        float vminVal = std::min(vpW, vpH);
        float vmaxVal = std::max(vpW, vpH);
        switch (u) {
            case Percent: return val * parentSize / 100.0f;
            case Px: return val;
            case Vw: return val * vpW / 100.0f;
            case Vh: return val * vpH / 100.0f;
            case Em: return val * emBase;
            case Rem: return val * 16.0f;
            case Ch: return val * emBase * 0.5f;
            case Lh: return val * emBase * 1.2f;
            case Vi: return val * vpW / 100.0f;
            case Vb: return val * vpH / 100.0f;
            case Dvw: return val * vpW / 100.0f;
            case Dvh: return val * vpH / 100.0f;
            case Vmin: return val * vminVal / 100.0f;
            case Vmax: return val * vmaxVal / 100.0f;
            case Rlh: return val * 16.0f * 1.2f;
            case Ex: return val * emBase * 0.5f;
            case Ic: return val * emBase;
            case Cap: return val * emBase * 0.7f;
            case SvW: return val * vpW / 100.0f;
            case SvH: return val * vpH / 100.0f;
            case SvMin: return val * vminVal / 100.0f;
            case SvMax: return val * vmaxVal / 100.0f;
            case LvW: return val * vpW / 100.0f;
            case LvH: return val * vpH / 100.0f;
            case LvMin: return val * vminVal / 100.0f;
            case LvMax: return val * vmaxVal / 100.0f;
            case DvMin: return val * vminVal / 100.0f;
            case DvMax: return val * vmaxVal / 100.0f;
            case Cqw: return val * (parentSize > 0.0f ? parentSize : vpW) / 100.0f;
            case Cqh: return val * (parentSize > 0.0f ? parentSize : vpH) / 100.0f;
            case Cqi: return val * (parentSize > 0.0f ? parentSize : vpW) / 100.0f;
            case Cqb: return val * (parentSize > 0.0f ? parentSize : vpH) / 100.0f;
            case Cqmin: {
                float cSize = parentSize > 0.0f ? parentSize : vminVal;
                return val * cSize / 100.0f;
            }
            case Cqmax: {
                float cSize = parentSize > 0.0f ? parentSize : vmaxVal;
                return val * cSize / 100.0f;
            }
            default: return val;
        }
    }
};
enum class MathNodeType {
    Value,
    BinaryOp,
    Function
};
class CSSMathExpressionNode {
public:
    virtual ~CSSMathExpressionNode() = default;
    virtual float resolve(float parentSize, float viewportW, float viewportH, float emBase) const = 0;
    virtual MathNodeType type() const = 0;
    virtual bool isEqual(const CSSMathExpressionNode& other) const = 0;
};
class CSSMathExpressionValue : public CSSMathExpressionNode {
public:
    float value;
    CSSValue::Unit unit;
    CSSMathExpressionValue(float v, CSSValue::Unit u) : value(v), unit(u) {}
    float resolve(float parentSize, float viewportW, float viewportH, float emBase) const override {
        return CSSValue::resolveUnit(value, unit, parentSize, viewportW, viewportH, emBase);
    }
    MathNodeType type() const override { return MathNodeType::Value; }
    bool isEqual(const CSSMathExpressionNode& other) const override {
        if (other.type() != MathNodeType::Value) return false;
        auto& o = static_cast<const CSSMathExpressionValue&>(other);
        return value == o.value && unit == o.unit;
    }
};
class CSSMathExpressionBinaryOp : public CSSMathExpressionNode {
public:
    enum Op { Add, Sub, Mul, Div };
    Op op;
    std::shared_ptr<CSSMathExpressionNode> left;
    std::shared_ptr<CSSMathExpressionNode> right;
    CSSMathExpressionBinaryOp(Op o, std::shared_ptr<CSSMathExpressionNode> l, std::shared_ptr<CSSMathExpressionNode> r)
        : op(o), left(l), right(r) {}
    float resolve(float parentSize, float viewportW, float viewportH, float emBase) const override {
        float lVal = left ? left->resolve(parentSize, viewportW, viewportH, emBase) : 0.0f;
        float rVal = right ? right->resolve(parentSize, viewportW, viewportH, emBase) : 0.0f;
        switch (op) {
            case Add: return lVal + rVal;
            case Sub: return lVal - rVal;
            case Mul: return lVal * rVal;
            case Div: return rVal != 0.0f ? lVal / rVal : 0.0f;
            default: return 0.0f;
        }
    }
    MathNodeType type() const override { return MathNodeType::BinaryOp; }
    bool isEqual(const CSSMathExpressionNode& other) const override {
        if (other.type() != MathNodeType::BinaryOp) return false;
        auto& o = static_cast<const CSSMathExpressionBinaryOp&>(other);
        if (op != o.op) return false;
        bool leftEqual = (!left && !o.left) || (left && o.left && left->isEqual(*o.left));
        bool rightEqual = (!right && !o.right) || (right && o.right && right->isEqual(*o.right));
        return leftEqual && rightEqual;
    }
};
class CSSMathExpressionFunction : public CSSMathExpressionNode {
public:
    enum Func { Min, Max, Clamp };
    Func func;
    std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
    CSSMathExpressionFunction(Func f, std::vector<std::shared_ptr<CSSMathExpressionNode>> a)
        : func(f), args(a) {}
    float resolve(float parentSize, float viewportW, float viewportH, float emBase) const override {
        if (args.empty()) return 0.0f;
        if (func == Min) {
            float minVal = args[0]->resolve(parentSize, viewportW, viewportH, emBase);
            for (size_t i = 1; i < args.size(); ++i) {
                minVal = std::min(minVal, args[i]->resolve(parentSize, viewportW, viewportH, emBase));
            }
            return minVal;
        } else if (func == Max) {
            float maxVal = args[0]->resolve(parentSize, viewportW, viewportH, emBase);
            for (size_t i = 1; i < args.size(); ++i) {
                maxVal = std::max(maxVal, args[i]->resolve(parentSize, viewportW, viewportH, emBase));
            }
            return maxVal;
        } else if (func == Clamp) {
            if (args.size() < 3) return 0.0f;
            float minVal = args[0]->resolve(parentSize, viewportW, viewportH, emBase);
            float prefVal = args[1]->resolve(parentSize, viewportW, viewportH, emBase);
            float maxVal = args[2]->resolve(parentSize, viewportW, viewportH, emBase);
            return prefVal < minVal ? minVal : (prefVal > maxVal ? maxVal : prefVal);
        }
        return 0.0f;
    }
    MathNodeType type() const override { return MathNodeType::Function; }
    bool isEqual(const CSSMathExpressionNode& other) const override {
        if (other.type() != MathNodeType::Function) return false;
        auto& o = static_cast<const CSSMathExpressionFunction&>(other);
        if (func != o.func) return false;
        if (args.size() != o.args.size()) return false;
        for (size_t i = 0; i < args.size(); ++i) {
            if (!args[i]->isEqual(*o.args[i])) return false;
        }
        return true;
    }
};
class CSSMathExpressionParser {
public:
    static std::shared_ptr<CSSMathExpressionNode> parse(const std::string& str) {
        CSSMathExpressionParser parser(str);
        return parser.parseExpression();
    }
private:
    std::string src;
    size_t pos = 0;
    CSSMathExpressionParser(const std::string& s) : src(s) {}
    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) {
            pos++;
        }
    }
    char peek() {
        skipWhitespace();
        if (pos >= src.size()) return '\0';
        return src[pos];
    }
    char next() {
        skipWhitespace();
        if (pos >= src.size()) return '\0';
        return src[pos++];
    }
    bool consume(char expected) {
        skipWhitespace();
        if (pos < src.size() && src[pos] == expected) {
            pos++;
            return true;
        }
        return false;
    }
    bool consumeWord(const std::string& word) {
        skipWhitespace();
        if (pos + word.size() <= src.size() && src.compare(pos, word.size(), word) == 0) {
            char nextChar = pos + word.size() < src.size() ? src[pos + word.size()] : '\0';
            if (nextChar == '(' || nextChar == ' ' || nextChar == '\t' || nextChar == '\0' || nextChar == ',' || nextChar == ')') {
                pos += word.size();
                return true;
            }
        }
        return false;
    }
    std::shared_ptr<CSSMathExpressionNode> parseExpression() {
        auto node = parseTerm();
        if (!node) return nullptr;
        while (true) {
            skipWhitespace();
            char opChar = peek();
            if (opChar == '+' || opChar == '-') {
                next();
                CSSMathExpressionBinaryOp::Op op = (opChar == '+') ? CSSMathExpressionBinaryOp::Add : CSSMathExpressionBinaryOp::Sub;
                auto right = parseTerm();
                if (!right) return nullptr;
                node = std::make_shared<CSSMathExpressionBinaryOp>(op, node, right);
            } else {
                break;
            }
        }
        return node;
    }
    std::shared_ptr<CSSMathExpressionNode> parseTerm() {
        auto node = parseFactor();
        if (!node) return nullptr;
        while (true) {
            skipWhitespace();
            char opChar = peek();
            if (opChar == '*' || opChar == '/') {
                next();
                CSSMathExpressionBinaryOp::Op op = (opChar == '*') ? CSSMathExpressionBinaryOp::Mul : CSSMathExpressionBinaryOp::Div;
                auto right = parseFactor();
                if (!right) return nullptr;
                node = std::make_shared<CSSMathExpressionBinaryOp>(op, node, right);
            } else {
                break;
            }
        }
        return node;
    }
    std::shared_ptr<CSSMathExpressionNode> parseFactor() {
        skipWhitespace();
        char c = peek();
        if (c == '\0') return nullptr;
        if (consume('(')) {
            auto node = parseExpression();
            consume(')');
            return node;
        }
        if (consumeWord("calc")) {
            if (consume('(')) {
                auto node = parseExpression();
                consume(')');
                return node;
            }
        }
        if (consumeWord("min")) {
            if (consume('(')) {
                std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
                do {
                    auto arg = parseExpression();
                    if (arg) args.push_back(arg);
                } while (consume(','));
                consume(')');
                return std::make_shared<CSSMathExpressionFunction>(CSSMathExpressionFunction::Min, args);
            }
        }
        if (consumeWord("max")) {
            if (consume('(')) {
                std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
                do {
                    auto arg = parseExpression();
                    if (arg) args.push_back(arg);
                } while (consume(','));
                consume(')');
                return std::make_shared<CSSMathExpressionFunction>(CSSMathExpressionFunction::Max, args);
            }
        }
        if (consumeWord("clamp")) {
            if (consume('(')) {
                std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
                do {
                    auto arg = parseExpression();
                    if (arg) args.push_back(arg);
                } while (consume(','));
                consume(')');
                return std::make_shared<CSSMathExpressionFunction>(CSSMathExpressionFunction::Clamp, args);
            }
        }
        return parseValue();
    }
    std::shared_ptr<CSSMathExpressionNode> parseValue() {
        skipWhitespace();
        if (pos >= src.size()) return nullptr;
        size_t start = pos;
        if (src[pos] == '+' || src[pos] == '-') {
            pos++;
        }
        bool hasDigits = false;
        while (pos < src.size() && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '.')) {
            hasDigits = true;
            pos++;
        }
        if (!hasDigits) {
            return nullptr;
        }
        std::string numStr = src.substr(start, pos - start);
        float val = parseLocaleIndependentFloat(numStr, 0.0f);
        CSSValue::Unit unit = CSSValue::None;
        if (pos < src.size() && src[pos] == '%') {
            unit = CSSValue::Percent;
            pos++;
        } else {
            std::string suffix;
            while (pos < src.size() && ((src[pos] >= 'a' && src[pos] <= 'z') || (src[pos] >= 'A' && src[pos] <= 'Z'))) {
                suffix += src[pos++];
            }
            if (!suffix.empty()) {
                std::string lowerSuffix = lowerAscii(suffix);
                if (lowerSuffix == "px") unit = CSSValue::Px;
                else if (lowerSuffix == "em") unit = CSSValue::Em;
                else if (lowerSuffix == "rem") unit = CSSValue::Rem;
                else if (lowerSuffix == "vw") unit = CSSValue::Vw;
                else if (lowerSuffix == "vh") unit = CSSValue::Vh;
                else if (lowerSuffix == "ch") unit = CSSValue::Ch;
                else if (lowerSuffix == "lh") unit = CSSValue::Lh;
                else if (lowerSuffix == "vi") unit = CSSValue::Vi;
                else if (lowerSuffix == "vb") unit = CSSValue::Vb;
                else if (lowerSuffix == "dvw") unit = CSSValue::Dvw;
                else if (lowerSuffix == "dvh") unit = CSSValue::Dvh;
                else if (lowerSuffix == "vmin") unit = CSSValue::Vmin;
                else if (lowerSuffix == "vmax") unit = CSSValue::Vmax;
                else if (lowerSuffix == "rlh") unit = CSSValue::Rlh;
                else if (lowerSuffix == "ex") unit = CSSValue::Ex;
                else if (lowerSuffix == "ic") unit = CSSValue::Ic;
                else if (lowerSuffix == "cap") unit = CSSValue::Cap;
                else if (lowerSuffix == "svw") unit = CSSValue::SvW;
                else if (lowerSuffix == "svh") unit = CSSValue::SvH;
                else if (lowerSuffix == "svmin") unit = CSSValue::SvMin;
                else if (lowerSuffix == "svmax") unit = CSSValue::SvMax;
                else if (lowerSuffix == "lvw") unit = CSSValue::LvW;
                else if (lowerSuffix == "lvh") unit = CSSValue::LvH;
                else if (lowerSuffix == "lvmin") unit = CSSValue::LvMin;
                else if (lowerSuffix == "lvmax") unit = CSSValue::LvMax;
                else if (lowerSuffix == "dvmin") unit = CSSValue::DvMin;
                else if (lowerSuffix == "dvmax") unit = CSSValue::DvMax;
                else if (lowerSuffix == "cqw") unit = CSSValue::Cqw;
                else if (lowerSuffix == "cqh") unit = CSSValue::Cqh;
                else if (lowerSuffix == "cqi") unit = CSSValue::Cqi;
                else if (lowerSuffix == "cqb") unit = CSSValue::Cqb;
                else if (lowerSuffix == "cqmin") unit = CSSValue::Cqmin;
                else if (lowerSuffix == "cqmax") unit = CSSValue::Cqmax;
                else {
                    unit = CSSValue::Px;
                }
            } else {
                unit = CSSValue::Px;
            }
        }
        return std::make_shared<CSSMathExpressionValue>(val, unit);
    }
    static std::string lowerAscii(const std::string& str) {
        std::string res = str;
        for (char& c : res) {
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }
        return res;
    }
};
inline CSSValue::CSSValue(std::shared_ptr<CSSMathExpressionNode> expr) : value(0), unit(None), mathExpr(expr) {}
inline float CSSValue::resolve(float parentSize, float viewportW, float viewportH, float emBase) const {
    if (mathExpr) {
        return mathExpr->resolve(parentSize, viewportW, viewportH, emBase);
    }
    float primary = resolveUnit(value, unit, parentSize, viewportW, viewportH, emBase);
    if (calcOp == CalcNone) return primary;
    float secondary = resolveUnit(calcValue2, calcUnit2, parentSize, viewportW, viewportH, emBase);
    switch (calcOp) {
        case CalcAdd: return primary + secondary;
        case CalcSub: return primary - secondary;
        case CalcMul: return primary * secondary;
        case CalcDiv: return secondary != 0.0f ? primary / secondary : 0.0f;
        case CalcMin: return std::min(primary, secondary);
        case CalcMax: return std::max(primary, secondary);
        case CalcClamp: {
            float tertiary = resolveUnit(calcValue3, calcUnit3, parentSize, viewportW, viewportH, emBase);
            return primary < secondary ? secondary : (primary > tertiary ? tertiary : primary);
        }
        default: return primary;
    }
}
inline bool CSSValue::isSet() const {
    return unit != None || mathExpr != nullptr;
}
inline bool CSSValue::operator==(const CSSValue& o) const {
    if (unit != o.unit || value != o.value || calcOp != o.calcOp ||
        calcValue2 != o.calcValue2 || calcUnit2 != o.calcUnit2 ||
        calcValue3 != o.calcValue3 || calcUnit3 != o.calcUnit3) {
        return false;
    }
    if (!mathExpr && !o.mathExpr) return true;
    if (!mathExpr || !o.mathExpr) return false;
    return mathExpr->isEqual(*o.mathExpr);
}
struct FastCustomProperties {
    std::shared_ptr<std::unordered_map<std::string, std::string>> map;
    FastCustomProperties() = default;
    FastCustomProperties(const FastCustomProperties& other) : map(other.map) {}
    FastCustomProperties& operator=(const FastCustomProperties& other) {
        if (this != &other) {
            map = other.map;
        }
        return *this;
    }
    FastCustomProperties(FastCustomProperties&& other) noexcept : map(std::move(other.map)) {}
    FastCustomProperties& operator=(FastCustomProperties&& other) noexcept {
        map = std::move(other.map);
        return *this;
    }
    FastCustomProperties(const std::unordered_map<std::string, std::string>& other) {
        if (!other.empty()) {
            map = std::make_shared<std::unordered_map<std::string, std::string>>(other);
        }
    }
    FastCustomProperties& operator=(const std::unordered_map<std::string, std::string>& other) {
        if (other.empty()) {
            map.reset();
        } else {
            map = std::make_shared<std::unordered_map<std::string, std::string>>(other);
        }
        return *this;
    }
    const std::unordered_map<std::string, std::string>* getMapPointer() const {
        return map.get();
    }
    std::unordered_map<std::string, std::string>* getMapPointer() {
        return map.get();
    }
    void ensureUnique() {
        if (!map) {
            map = std::make_shared<std::unordered_map<std::string, std::string>>();
        } else if (map.use_count() > 1) {
            map = std::make_shared<std::unordered_map<std::string, std::string>>(*map);
        }
    }
    std::unordered_map<std::string, std::string>& getOrCreateMap() {
        ensureUnique();
        return *map;
    }
    operator const std::unordered_map<std::string, std::string>&() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? *map : emptyMap;
    }
    auto begin() {
        ensureUnique();
        return map->begin();
    }
    auto begin() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->begin() : emptyMap.begin();
    }
    auto end() {
        ensureUnique();
        return map->end();
    }
    auto end() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->end() : emptyMap.end();
    }
    size_t size() const { return map ? map->size() : 0; }
    bool empty() const { return !map || map->empty(); }
    void clear() { map.reset(); }
    std::string& operator[](const std::string& key) {
        ensureUnique();
        return (*map)[key];
    }
    auto find(const std::string& key) {
        ensureUnique();
        return map->find(key);
    }
    auto find(const std::string& key) const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->find(key) : emptyMap.find(key);
    }
};
struct Transform2D {
    float m00 = 1.0f, m01 = 0.0f, m02 = 0.0f;
    float m10 = 0.0f, m11 = 1.0f, m12 = 0.0f;
    Transform2D() = default;
    Transform2D(float m00, float m01, float m02, float m10, float m11, float m12)
        : m00(m00), m01(m01), m02(m02), m10(m10), m11(m11), m12(m12) {}
    static Transform2D identity() {
        return Transform2D(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    static Transform2D fromTranslate(float tx, float ty) {
        return Transform2D(1.0f, 0.0f, tx, 0.0f, 1.0f, ty);
    }
    static Transform2D fromScale(float sx, float sy) {
        return Transform2D(sx, 0.0f, 0.0f, 0.0f, sy, 0.0f);
    }
    static Transform2D fromRotate(float angleRad) {
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        return Transform2D(c, -s, 0.0f, s, c, 0.0f);
    }
    static Transform2D fromSkew(float axRad, float ayRad) {
        return Transform2D(1.0f, std::tan(axRad), 0.0f, std::tan(ayRad), 1.0f, 0.0f);
    }
    bool isIdentity() const {
        return m00 == 1.0f && m01 == 0.0f && m02 == 0.0f &&
               m10 == 0.0f && m11 == 1.0f && m12 == 0.0f;
    }
    Transform2D multiplied(const Transform2D& o) const {
        return Transform2D(
            m00 * o.m00 + m01 * o.m10,
            m00 * o.m01 + m01 * o.m11,
            m00 * o.m02 + m01 * o.m12 + m02,
            m10 * o.m00 + m11 * o.m10,
            m10 * o.m01 + m11 * o.m11,
            m10 * o.m02 + m11 * o.m12 + m12
        );
    }
    Vec2 mapPoint(const Vec2& p) const {
        return Vec2(
            m00 * p.x + m01 * p.y + m02,
            m10 * p.x + m11 * p.y + m12
        );
    }
    Transform2D inverse() const {
        float det = m00 * m11 - m01 * m10;
        if (std::abs(det) < 1e-9f) {
            return identity();
        }
        float invDet = 1.0f / det;
        return Transform2D(
            m11 * invDet, -m01 * invDet, (m01 * m12 - m11 * m02) * invDet,
            -m10 * invDet, m00 * invDet, (m10 * m02 - m00 * m12) * invDet
        );
    }
    Vec2 inverseMapPoint(const Vec2& p) const {
        return inverse().mapPoint(p);
    }
};
enum class TransformOperationType {
    Translate,
    Translate3d,
    TranslateX,
    TranslateY,
    TranslateZ,
    Scale,
    Scale3d,
    ScaleX,
    ScaleY,
    ScaleZ,
    Rotate,
    Rotate3d,
    RotateX,
    RotateY,
    RotateZ,
    Skew,
    SkewX,
    SkewY,
    Matrix,
    Matrix3d,
    Perspective
};
struct TransformOperation {
    enum Kind {
        Translate,
        Scale,
        Rotate,
        Skew,
        Matrix,
        Perspective
    };
    TransformOperationType type;
    std::vector<CSSValue> args;
    Kind kind = Translate;
    float v[16] = {0};
    int dim = 2;
    TransformOperation() = default;
    bool operator==(const TransformOperation& o) const {
        return type == o.type && args == o.args;
    }
    bool operator!=(const TransformOperation& o) const {
        return !(*this == o);
    }
    void resolveFromArgs() {
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        std::fill(std::begin(v), std::end(v), 0.0f);
        int t = (int)type;
        if (t <= 4) {
            kind = Translate;
            dim = (t == 1 || t == 4) ? 3 : 2;
            if (t == 0) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f);
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f);
            } else if (t == 1) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f);
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f);
                if (args.size() >= 3) v[2] = args[2].resolve(1.0f);
            } else if (t == 2) {
                if (!args.empty()) v[0] = args[0].resolve(1.0f);
            } else if (t == 3) {
                if (!args.empty()) v[1] = args[0].resolve(1.0f);
            } else if (t == 4) {
                if (!args.empty()) v[2] = args[0].resolve(1.0f);
            }
        } else if (t <= 9) {
            kind = Scale;
            dim = (t == 6 || t == 9) ? 3 : 2;
            v[0] = 1.0f; v[1] = 1.0f; v[2] = 1.0f;
            if (t == 5) {
                if (args.size() >= 1) {
                    v[0] = args[0].resolve(1.0f);
                    v[1] = (args.size() >= 2) ? args[1].resolve(1.0f) : args[0].resolve(1.0f);
                }
            } else if (t == 6) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f);
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f);
                if (args.size() >= 3) v[2] = args[2].resolve(1.0f);
            } else if (t == 7) {
                if (!args.empty()) v[0] = args[0].resolve(1.0f);
            } else if (t == 8) {
                if (!args.empty()) v[1] = args[0].resolve(1.0f);
            } else if (t == 9) {
                if (!args.empty()) v[2] = args[0].resolve(1.0f);
            }
        } else if (t <= 14) {
            kind = Rotate;
            dim = (t == 11) ? 3 : 2;
            if (t == 11) {
                if (args.size() >= 4) {
                    v[0] = args[0].resolve(1.0f);
                    v[1] = args[1].resolve(1.0f);
                    v[2] = args[2].resolve(1.0f);
                    v[3] = args[3].resolve(1.0f) * kDegToRad;
                }
            } else {
                if (!args.empty()) v[0] = args[0].resolve(1.0f) * kDegToRad;
            }
        } else if (t <= 17) {
            kind = Skew;
            dim = 2;
            if (t == 15) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f) * kDegToRad;
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f) * kDegToRad;
            } else if (t == 16) {
                if (!args.empty()) v[0] = args[0].resolve(1.0f) * kDegToRad;
            } else if (t == 17) {
                if (!args.empty()) v[1] = args[0].resolve(1.0f) * kDegToRad;
            }
        } else if (t <= 19) {
            kind = Matrix;
            dim = (t == 19) ? 3 : 2;
            for (size_t i = 0; i < args.size() && i < 16; ++i) {
                v[i] = args[i].resolve(1.0f);
            }
        } else {
            kind = Perspective;
            dim = 2;
            if (!args.empty()) v[0] = args[0].resolve(1.0f);
        }
    }
    static TransformOperation scaleOp(float sx, float sy) {
        TransformOperation op;
        op.type = TransformOperationType::Scale;
        op.kind = Scale;
        op.dim = 2;
        op.v[0] = sx;
        op.v[1] = sy;
        op.args = { CSSValue(sx, CSSValue::None), CSSValue(sy, CSSValue::None) };
        return op;
    }
    static TransformOperation rotate(float angleRad) {
        TransformOperation op;
        op.type = TransformOperationType::Rotate;
        op.kind = Rotate;
        op.dim = 2;
        op.v[0] = angleRad;
        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
        op.args = { CSSValue(angleRad * kRadToDeg, CSSValue::Deg) };
        return op;
    }
    static TransformOperation blend(const TransformOperation& a, const TransformOperation& b, float t) {
        if (a.kind != b.kind) {
            return t < 0.5f ? a : b;
        }
        TransformOperation res = a;
        if (a.kind == Rotate) {
            constexpr float kPi = 3.14159265358979323846f;
            float diff = b.v[0] - a.v[0];
            diff = std::remainder(diff, 2.0f * kPi);
            res.v[0] = a.v[0] + diff * t;
            constexpr float kRadToDeg = 180.0f / kPi;
            res.args = { CSSValue(res.v[0] * kRadToDeg, CSSValue::Deg) };
        } else {
            int limit = (a.kind == Matrix) ? 16 : 4;
            for (int i = 0; i < limit; ++i) {
                res.v[i] = a.v[i] + (b.v[i] - a.v[i]) * t;
            }
            res.args.clear();
            if (res.kind == Translate) {
                if (res.dim == 3) {
                    res.args = { CSSValue(res.v[0], CSSValue::Px), CSSValue(res.v[1], CSSValue::Px), CSSValue(res.v[2], CSSValue::Px) };
                } else {
                    res.args = { CSSValue(res.v[0], CSSValue::Px), CSSValue(res.v[1], CSSValue::Px) };
                }
            } else if (res.kind == Scale) {
                if (res.dim == 3) {
                    res.args = { CSSValue(res.v[0], CSSValue::None), CSSValue(res.v[1], CSSValue::None), CSSValue(res.v[2], CSSValue::None) };
                } else {
                    res.args = { CSSValue(res.v[0], CSSValue::None), CSSValue(res.v[1], CSSValue::None) };
                }
            } else if (res.kind == Skew) {
                res.args = { CSSValue(res.v[0] * (180.0f / 3.14159265f), CSSValue::Deg), CSSValue(res.v[1] * (180.0f / 3.14159265f), CSSValue::Deg) };
            } else if (res.kind == Matrix) {
                if (res.dim == 3) {
                    for (int i = 0; i < 16; ++i) res.args.push_back(CSSValue(res.v[i], CSSValue::None));
                } else {
                    for (int i = 0; i < 6; ++i) res.args.push_back(CSSValue(res.v[i], CSSValue::None));
                }
            } else if (res.kind == Perspective) {
                res.args = { CSSValue(res.v[0], CSSValue::Px) };
            }
        }
        return res;
    }
    Transform2D toTransform2D() const {
        if (kind == Translate) {
            return Transform2D::fromTranslate(v[0], v[1]);
        } else if (kind == Scale) {
            return Transform2D::fromScale(v[0], v[1]);
        } else if (kind == Rotate) {
            return Transform2D::fromRotate(v[0]);
        } else if (kind == Skew) {
            return Transform2D::fromSkew(v[0], v[1]);
        } else if (kind == Matrix) {
            return Transform2D(v[0], v[2], v[4], v[1], v[3], v[5]);
        }
        return Transform2D::identity();
    }
    Transform2D apply(const Transform2D& matrix, const Vec2& origin) const {
        Transform2D opMatrix = this->toTransform2D();
        Transform2D t = Transform2D::fromTranslate(origin.x, origin.y)
                          .multiplied(opMatrix)
                          .multiplied(Transform2D::fromTranslate(-origin.x, -origin.y));
        return matrix.multiplied(t);
    }
};
struct TransformOrigin {
    CSSValue x = CSSValue::pct(50.0f);
    CSSValue y = CSSValue::pct(50.0f);
    CSSValue z = CSSValue::px(0.0f);
    bool operator==(const TransformOrigin& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const TransformOrigin& o) const {
        return !(*this == o);
    }
};
enum class TransformStyle {
    Flat,
    Preserve3D,
    Preserve3d = Preserve3D
};
enum class TransformBox {
    ContentBox,
    BorderBox,
    FillBox,
    StrokeBox,
    ViewBox
};
struct PerspectiveOrigin {
    CSSValue x = CSSValue::pct(50.0f);
    CSSValue y = CSSValue::pct(50.0f);
    bool operator==(const PerspectiveOrigin& o) const {
        return x == o.x && y == o.y;
    }
    bool operator!=(const PerspectiveOrigin& o) const {
        return !(*this == o);
    }
};
enum class BackfaceVisibility {
    Visible,
    Hidden
};
struct Style {
    Display display = Display::Block;
    Position position = Position::Static;
    FlexDirection flexDirection = FlexDirection::Column;
    FlexWrap flexWrap = FlexWrap::NoWrap;
    JustifyContent justifyContent = JustifyContent::FlexStart;
    AlignItems alignItems = AlignItems::Stretch;
    AlignContent alignContent = AlignContent::Stretch;
    AlignSelf alignSelf = AlignSelf::Auto;
    float flexGrow = 0;
    float flexShrink = 1;
    CSSValue flexBasis;
    int order = 0;
    float gap = 0;
    float rowGap = 0;
    float columnGap = 0;
    int columnCount = 0;
    float columnWidth = 0.0f;
    Overflow overflow = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;
    Overflow overflowY = Overflow::Visible;
    BoxSizing boxSizing = BoxSizing::ContentBox;
    bool hasBoxSizing = false;
    Visibility visibility = Visibility::Visible;
    PointerEvents pointerEvents = PointerEvents::Auto;
    int zIndex = 0;
    bool hasZIndex = false;
    CSSFloat cssFloat = CSSFloat::None;
    CSSClear cssClear = CSSClear::None;
    // ── CSS Grid Layout (Blink NGGridLayoutAlgorithm parity) ──────────────
    // Parsed track lists — replaces the old raw std::string fields.
    std::vector<GridTrackSize> gridTemplateColumnTracks; // grid-template-columns
    std::vector<GridTrackSize> gridTemplateRowTracks;    // grid-template-rows
    // Raw strings kept for subgrid / complex values that need further resolution
    std::string gridTemplateColumns;
    std::string gridTemplateRows;
    // Item placement (grid-column / grid-row shorthand → start/end)
    GridPlacement gridColumnStart;
    GridPlacement gridColumnEnd;
    GridPlacement gridRowStart;
    GridPlacement gridRowEnd;
    std::string gridColumn;  // raw shorthand (kept for cascade)
    std::string gridRow;
    // grid-area shorthand → fills all four GridPlacement fields above
    std::string gridArea;
    // Template areas
    GridTemplateAreas gridTemplateAreas;
    bool hasGridTemplateAreas = false;
    // Implicit track sizing (grid-auto-rows / grid-auto-columns)
    std::vector<GridTrackSize> gridAutoRowTracks;
    std::vector<GridTrackSize> gridAutoColumnTracks;
    // grid-auto-flow
    GridAutoFlow gridAutoFlow = GridAutoFlow::Row;
    // Per-item alignment
    JustifyItems justifyItems = JustifyItems::Normal;
    JustifySelf  justifySelf  = JustifySelf::Auto;
    bool hasJustifyItems = false;
    bool hasJustifySelf  = false;
    std::string content;
    float aspectRatio = 0;
    ObjectFit objectFit = ObjectFit::Fill;
    bool hasObjectFit = false;
    Appearance appearance = Appearance::Auto;
    bool hasAppearance = false;
    Vec2 objectPosition = {0.5f, 0.5f};
    Vec2 objectPositionOffset = {0.0f, 0.0f};
    bool hasObjectPosition = false;
    ContainmentFlags contain = kContainNone;
    CSSValue width, height;
    CSSValue minWidth, minHeight;
    CSSValue maxWidth, maxHeight;
    EdgeInsets padding;
    EdgeInsets margin;
    Direction direction = Direction::Ltr;
    bool hasDirection = false;
    UnicodeBidi unicodeBidi = UnicodeBidi::Normal;
    bool hasUnicodeBidi = false;
    WritingMode writingMode = WritingMode::HorizontalTb;
    bool hasWritingMode = false;
    float marginInlineStart = 0.0f, marginInlineEnd = 0.0f;
    float marginBlockStart = 0.0f, marginBlockEnd = 0.0f;
    bool hasMarginInlineStart = false, hasMarginInlineEnd = false;
    bool hasMarginBlockStart = false, hasMarginBlockEnd = false;
    float paddingInlineStart = 0.0f, paddingInlineEnd = 0.0f;
    float paddingBlockStart = 0.0f, paddingBlockEnd = 0.0f;
    bool hasPaddingInlineStart = false, hasPaddingInlineEnd = false;
    bool hasPaddingBlockStart = false, hasPaddingBlockEnd = false;
    CSSValue insetInlineStart, insetInlineEnd;
    CSSValue insetBlockStart, insetBlockEnd;
    bool hasInsetInlineStart = false, hasInsetInlineEnd = false;
    bool hasInsetBlockStart = false, hasInsetBlockEnd = false;
    Border borderInlineStart, borderInlineEnd;
    Border borderBlockStart, borderBlockEnd;
    bool hasBorderInlineStart = false, hasBorderInlineEnd = false;
    bool hasBorderBlockStart = false, hasBorderBlockEnd = false;
    CSSValue inlineSize, blockSize;
    CSSValue minInlineSize, minBlockSize;
    CSSValue maxInlineSize, maxBlockSize;
    bool hasInlineSize = false, hasBlockSize = false;
    bool hasMinInlineSize = false, hasMinBlockSize = false;
    bool hasMaxInlineSize = false, hasMaxBlockSize = false;
    float marginT = 0.0f, marginR = 0.0f, marginB = 0.0f, marginL = 0.0f;
    float paddingT = 0.0f, paddingR = 0.0f, paddingB = 0.0f, paddingL = 0.0f;
    bool hasMarginT = false, hasMarginR = false, hasMarginB = false, hasMarginL = false;
    bool hasPaddingT = false, hasPaddingR = false, hasPaddingB = false, hasPaddingL = false;
    CSSValue topVal, rightVal, bottomVal, leftVal;
    bool hasTopVal = false, hasRightVal = false, hasBottomVal = false, hasLeftVal = false;
    Border borderT, borderR, borderB, borderL;
    bool hasBorderT = false, hasBorderR = false, hasBorderB = false, hasBorderL = false;
    bool hasWidthVal = false, hasHeightVal = false;
    bool hasMinWidthVal = false, hasMinHeightVal = false;
    bool hasMaxWidthVal = false, hasMaxHeightVal = false;
    uint32_t propertyOrder = 0;
    uint32_t orderMarginTop = 0, orderMarginRight = 0, orderMarginBottom = 0, orderMarginLeft = 0;
    uint32_t orderMarginBlockStart = 0, orderMarginBlockEnd = 0, orderMarginInlineStart = 0, orderMarginInlineEnd = 0;
    uint32_t orderPaddingTop = 0, orderPaddingRight = 0, orderPaddingBottom = 0, orderPaddingLeft = 0;
    uint32_t orderPaddingBlockStart = 0, orderPaddingBlockEnd = 0, orderPaddingInlineStart = 0, orderPaddingInlineEnd = 0;
    uint32_t orderBorderTop = 0, orderBorderRight = 0, orderBorderBottom = 0, orderBorderLeft = 0;
    uint32_t orderBorderBlockStart = 0, orderBorderBlockEnd = 0, orderBorderInlineStart = 0, orderBorderInlineEnd = 0;
    uint32_t orderTop = 0, orderRight = 0, orderBottom = 0, orderLeft = 0;
    uint32_t orderInsetBlockStart = 0, orderInsetBlockEnd = 0, orderInsetInlineStart = 0, orderInsetInlineEnd = 0;
    uint32_t orderWidth = 0, orderHeight = 0;
    uint32_t orderMinWidth = 0, orderMinHeight = 0;
    uint32_t orderMaxWidth = 0, orderMaxHeight = 0;
    uint32_t orderInlineSize = 0, orderBlockSize = 0;
    uint32_t orderMinInlineSize = 0, orderMinBlockSize = 0;
    uint32_t orderMaxInlineSize = 0, orderMaxBlockSize = 0;
    enum PhysicalSide { PhysTop, PhysRight, PhysBottom, PhysLeft };
    enum LogicalSide { LogBlockStart, LogBlockEnd, LogInlineStart, LogInlineEnd };
    PhysicalSide mapLogicalSide(LogicalSide log) const {
        if (writingMode == WritingMode::HorizontalTb) {
            switch(log) {
                case LogBlockStart: return PhysTop;
                case LogBlockEnd: return PhysBottom;
                case LogInlineStart: return (direction == Direction::Ltr) ? PhysLeft : PhysRight;
                case LogInlineEnd: return (direction == Direction::Ltr) ? PhysRight : PhysLeft;
            }
        } else if (writingMode == WritingMode::VerticalRl) {
            switch(log) {
                case LogBlockStart: return PhysRight;
                case LogBlockEnd: return PhysLeft;
                case LogInlineStart: return (direction == Direction::Ltr) ? PhysTop : PhysBottom;
                case LogInlineEnd: return (direction == Direction::Ltr) ? PhysBottom : PhysTop;
            }
        } else {
            switch(log) {
                case LogBlockStart: return PhysLeft;
                case LogBlockEnd: return PhysRight;
                case LogInlineStart: return (direction == Direction::Ltr) ? PhysTop : PhysBottom;
                case LogInlineEnd: return (direction == Direction::Ltr) ? PhysBottom : PhysTop;
            }
        }
        return PhysTop;
    }
    LogicalSide getLogicalSideMapping(PhysicalSide phys) const {
        for (int l = 0; l < 4; ++l) {
            if (mapLogicalSide((LogicalSide)l) == phys) {
                return (LogicalSide)l;
            }
        }
        return LogBlockStart;
    }
    void resolveLogicalProperties() {
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            float physVal = 0.0f;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = marginT; hasPhys = hasMarginT; physOrder = orderMarginTop; break;
                case PhysRight: physVal = marginR; hasPhys = hasMarginR; physOrder = orderMarginRight; break;
                case PhysBottom: physVal = marginB; hasPhys = hasMarginB; physOrder = orderMarginBottom; break;
                case PhysLeft: physVal = marginL; hasPhys = hasMarginL; physOrder = orderMarginLeft; break;
            }
            float logVal = 0.0f;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = marginBlockStart; hasLog = hasMarginBlockStart; logOrder = orderMarginBlockStart; break;
                case LogBlockEnd: logVal = marginBlockEnd; hasLog = hasMarginBlockEnd; logOrder = orderMarginBlockEnd; break;
                case LogInlineStart: logVal = marginInlineStart; hasLog = hasMarginInlineStart; logOrder = orderMarginInlineStart; break;
                case LogInlineEnd: logVal = marginInlineEnd; hasLog = hasMarginInlineEnd; logOrder = orderMarginInlineEnd; break;
            }
            float finalVal = 0.0f;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                finalVal = logVal;
            } else if (hasPhys) {
                finalVal = physVal;
            } else {
                switch(phys) {
                    case PhysTop: finalVal = margin.top; break;
                    case PhysRight: finalVal = margin.right; break;
                    case PhysBottom: finalVal = margin.bottom; break;
                    case PhysLeft: finalVal = margin.left; break;
                }
            }
            switch(phys) {
                case PhysTop: margin.top = finalVal; break;
                case PhysRight: margin.right = finalVal; break;
                case PhysBottom: margin.bottom = finalVal; break;
                case PhysLeft: margin.left = finalVal; break;
            }
        }
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            float physVal = 0.0f;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = paddingT; hasPhys = hasPaddingT; physOrder = orderPaddingTop; break;
                case PhysRight: physVal = paddingR; hasPhys = hasPaddingR; physOrder = orderPaddingRight; break;
                case PhysBottom: physVal = paddingB; hasPhys = hasPaddingB; physOrder = orderPaddingBottom; break;
                case PhysLeft: physVal = paddingL; hasPhys = hasPaddingL; physOrder = orderPaddingLeft; break;
            }
            float logVal = 0.0f;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = paddingBlockStart; hasLog = hasPaddingBlockStart; logOrder = orderPaddingBlockStart; break;
                case LogBlockEnd: logVal = paddingBlockEnd; hasLog = hasPaddingBlockEnd; logOrder = orderPaddingBlockEnd; break;
                case LogInlineStart: logVal = paddingInlineStart; hasLog = hasPaddingInlineStart; logOrder = orderPaddingInlineStart; break;
                case LogInlineEnd: logVal = paddingInlineEnd; hasLog = hasPaddingInlineEnd; logOrder = orderPaddingInlineEnd; break;
            }
            float finalVal = 0.0f;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                finalVal = logVal;
            } else if (hasPhys) {
                finalVal = physVal;
            } else {
                switch(phys) {
                    case PhysTop: finalVal = padding.top; break;
                    case PhysRight: finalVal = padding.right; break;
                    case PhysBottom: finalVal = padding.bottom; break;
                    case PhysLeft: finalVal = padding.left; break;
                }
            }
            switch(phys) {
                case PhysTop: padding.top = finalVal; break;
                case PhysRight: padding.right = finalVal; break;
                case PhysBottom: padding.bottom = finalVal; break;
                case PhysLeft: padding.left = finalVal; break;
            }
        }
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            CSSValue physVal;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = topVal; hasPhys = hasTopVal; physOrder = orderTop; break;
                case PhysRight: physVal = rightVal; hasPhys = hasRightVal; physOrder = orderRight; break;
                case PhysBottom: physVal = bottomVal; hasPhys = hasBottomVal; physOrder = orderBottom; break;
                case PhysLeft: physVal = leftVal; hasPhys = hasLeftVal; physOrder = orderLeft; break;
            }
            CSSValue logVal;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = insetBlockStart; hasLog = hasInsetBlockStart; logOrder = orderInsetBlockStart; break;
                case LogBlockEnd: logVal = insetBlockEnd; hasLog = hasInsetBlockEnd; logOrder = orderInsetBlockEnd; break;
                case LogInlineStart: logVal = insetInlineStart; hasLog = hasInsetInlineStart; logOrder = orderInsetInlineStart; break;
                case LogInlineEnd: logVal = insetInlineEnd; hasLog = hasInsetInlineEnd; logOrder = orderInsetInlineEnd; break;
            }
            CSSValue finalVal;
            bool finalSet = false;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
                finalSet = true;
            } else if (hasLog) {
                finalVal = logVal;
                finalSet = true;
            } else if (hasPhys) {
                finalVal = physVal;
                finalSet = true;
            }
            if (finalSet) {
                switch(phys) {
                    case PhysTop: top = finalVal; break;
                    case PhysRight: right = finalVal; break;
                    case PhysBottom: bottom = finalVal; break;
                    case PhysLeft: left = finalVal; break;
                }
            }
        }
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            Border physVal;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = borderT; hasPhys = hasBorderT; physOrder = orderBorderTop; break;
                case PhysRight: physVal = borderR; hasPhys = hasBorderR; physOrder = orderBorderRight; break;
                case PhysBottom: physVal = borderB; hasPhys = hasBorderB; physOrder = orderBorderBottom; break;
                case PhysLeft: physVal = borderL; hasPhys = hasBorderL; physOrder = orderBorderLeft; break;
            }
            Border logVal;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = borderBlockStart; hasLog = hasBorderBlockStart; logOrder = orderBorderBlockStart; break;
                case LogBlockEnd: logVal = borderBlockEnd; hasLog = hasBorderBlockEnd; logOrder = orderBorderBlockEnd; break;
                case LogInlineStart: logVal = borderInlineStart; hasLog = hasBorderInlineStart; logOrder = orderBorderInlineStart; break;
                case LogInlineEnd: logVal = borderInlineEnd; hasLog = hasBorderInlineEnd; logOrder = orderBorderInlineEnd; break;
            }
            Border finalVal;
            bool finalSet = false;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
                finalSet = true;
            } else if (hasLog) {
                finalVal = logVal;
                finalSet = true;
            } else if (hasPhys) {
                finalVal = physVal;
                finalSet = true;
            }
            if (finalSet) {
                switch(phys) {
                    case PhysTop: borderTop = finalVal; hasBorderTop = true; break;
                    case PhysRight: borderRight = finalVal; hasBorderRight = true; break;
                    case PhysBottom: borderBottom = finalVal; hasBorderBottom = true; break;
                    case PhysLeft: borderLeft = finalVal; hasBorderLeft = true; break;
                }
            }
        }
        {
            bool isWidthLogical = (writingMode != WritingMode::HorizontalTb);
            CSSValue physVal = width;
            bool hasPhys = hasWidthVal;
            uint32_t physOrder = orderWidth;
            CSSValue logVal = isWidthLogical ? blockSize : inlineSize;
            bool hasLog = isWidthLogical ? hasBlockSize : hasInlineSize;
            uint32_t logOrder = isWidthLogical ? orderBlockSize : orderInlineSize;
            if (hasPhys && hasLog) {
                width = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                width = logVal;
            } else if (hasPhys) {
                width = physVal;
            }
        }
        {
            bool isHeightLogical = (writingMode == WritingMode::HorizontalTb);
            CSSValue physVal = height;
            bool hasPhys = hasHeightVal;
            uint32_t physOrder = orderHeight;
            CSSValue logVal = isHeightLogical ? blockSize : inlineSize;
            bool hasLog = isHeightLogical ? hasBlockSize : hasInlineSize;
            uint32_t logOrder = isHeightLogical ? orderBlockSize : orderInlineSize;
            if (hasPhys && hasLog) {
                height = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                height = logVal;
            } else if (hasPhys) {
                height = physVal;
            }
        }
        {
            bool isWidthLogical = (writingMode != WritingMode::HorizontalTb);
            CSSValue physVal = minWidth;
            bool hasPhys = hasMinWidthVal;
            uint32_t physOrder = orderMinWidth;
            CSSValue logVal = isWidthLogical ? minBlockSize : minInlineSize;
            bool hasLog = isWidthLogical ? hasMinBlockSize : hasMinInlineSize;
            uint32_t logOrder = isWidthLogical ? orderMinBlockSize : orderMinInlineSize;
            if (hasPhys && hasLog) {
                minWidth = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                minWidth = logVal;
            } else if (hasPhys) {
                minWidth = physVal;
            }
        }
        {
            bool isHeightLogical = (writingMode == WritingMode::HorizontalTb);
            CSSValue physVal = minHeight;
            bool hasPhys = hasMinHeightVal;
            uint32_t physOrder = orderMinHeight;
            CSSValue logVal = isHeightLogical ? minBlockSize : minInlineSize;
            bool hasLog = isHeightLogical ? hasMinBlockSize : hasMinInlineSize;
            uint32_t logOrder = isHeightLogical ? orderMinBlockSize : orderMinInlineSize;
            if (hasPhys && hasLog) {
                minHeight = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                minHeight = logVal;
            } else if (hasPhys) {
                minHeight = physVal;
            }
        }
        {
            bool isWidthLogical = (writingMode != WritingMode::HorizontalTb);
            CSSValue physVal = maxWidth;
            bool hasPhys = hasMaxWidthVal;
            uint32_t physOrder = orderMaxWidth;
            CSSValue logVal = isWidthLogical ? maxBlockSize : maxInlineSize;
            bool hasLog = isWidthLogical ? hasMaxBlockSize : hasMaxInlineSize;
            uint32_t logOrder = isWidthLogical ? orderMaxBlockSize : orderMaxInlineSize;
            if (hasPhys && hasLog) {
                maxWidth = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                maxWidth = logVal;
            } else if (hasPhys) {
                maxWidth = physVal;
            }
        }
        {
            bool isHeightLogical = (writingMode == WritingMode::HorizontalTb);
            CSSValue physVal = maxHeight;
            bool hasPhys = hasMaxHeightVal;
            uint32_t physOrder = orderMaxHeight;
            CSSValue logVal = isHeightLogical ? maxBlockSize : maxInlineSize;
            bool hasLog = isHeightLogical ? hasMaxBlockSize : hasMaxInlineSize;
            uint32_t logOrder = isHeightLogical ? orderMaxBlockSize : orderMaxInlineSize;
            if (hasPhys && hasLog) {
                maxHeight = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                maxHeight = logVal;
            } else if (hasPhys) {
                maxHeight = physVal;
            }
        }
    }
    CSSValue top, right, bottom, left;
    Color color = Color(1, 1, 1, 1);
    Color backgroundColor = Color(0, 0, 0, 0);
    Gradient backgroundGradient;
    Border border;
    Border borderTop, borderRight, borderBottom, borderLeft;
    bool hasBorderTop = false, hasBorderRight = false;
    bool hasBorderBottom = false, hasBorderLeft = false;
    Border outline;
    BorderRadius borderRadius;
    BoxShadow boxShadow;
    float opacity = 1.0f;
    float outlineOffset = 0;
    // Legacy scalar kept for the renderer drawBackdropFilterBlur() path.
    // Always synced from backdropFilterOperations when parsed.
    float backdropFilterBlur = 0.0f;
    bool hasBackdropFilterBlur = false;
    // Full filter operation lists (Blink FilterOperations parity).
    // Both properties accept the same function set; backdrop-filter also
    // accepts blur() as the primary GPU-composited path.
    std::vector<FilterOperation> filterOperations;           // filter:
    std::vector<FilterOperation> backdropFilterOperations;  // backdrop-filter:
    bool hasFilter = false;
    bool hasBackdropFilter = false;

    // ── Masking, Clipping, Blending (CSS Masking Level 1 / Compositing Level 1) ──
    // clip-path: <basic-shape> | url(#clip) | none
    std::string clipPath;                     // raw value string (e.g. "circle(50%)", "url(#c)")
    bool hasClipPath = false;
    // shape-outside: <basic-shape> | url() | none
    std::string shapeOutside;
    bool hasShapeOutside = false;
    // mask shorthand sub-properties
    std::string maskImage;                    // mask-image: url(...) | <gradient> | none
    std::string maskMode;                     // mask-mode: alpha | luminance | match-source
    std::string maskRepeat;                   // mask-repeat: repeat | no-repeat | ...
    std::string maskPosition;                 // mask-position: <position>
    std::string maskSize;                     // mask-size: <length> | cover | contain
    std::string maskClip;                     // mask-clip: border-box | content-box | ...
    std::string maskOrigin;                   // mask-origin: border-box | content-box | ...
    std::string maskComposite;                // mask-composite: add | subtract | intersect | exclude
    bool hasMask = false;
    // mix-blend-mode / isolation / background-blend-mode
    enum class BlendMode {
        Normal, Multiply, Screen, Overlay, Darken, Lighten,
        ColorDodge, ColorBurn, HardLight, SoftLight, Difference,
        Exclusion, Hue, Saturation, Color, Luminosity
    };
    enum class Isolation { Auto, Isolate };
    BlendMode mixBlendMode = BlendMode::Normal;
    bool hasMixBlendMode = false;
    Isolation isolation = Isolation::Auto;
    bool hasIsolation = false;
    BlendMode backgroundBlendMode = BlendMode::Normal;
    bool hasBackgroundBlendMode = false;

    float fontSize = 14.0f;
    FontWeight fontWeight = FontWeight::Normal;
    FontStyle fontStyle = FontStyle::Normal;
    TextAlign textAlign = TextAlign::Left;
    float lineHeight = 1.4f;
    std::string fontFamily;
    TextOverflow textOverflow = TextOverflow::Clip;
    WhiteSpace whiteSpace = WhiteSpace::Normal;
    TextDecoration textDecoration = TextDecoration::None;
    TextTransform textTransform = TextTransform::None;
    WordBreak wordBreak = WordBreak::Normal;
    VerticalAlign verticalAlign = VerticalAlign::Baseline;
    ListStyleType listStyleType = ListStyleType::Disc;
    float letterSpacing = 0;
    float wordSpacing = 0;
    Color textDecorationColor;
    bool hasTextDecorationColor = false;
    bool hasColor = false;
    bool hasFontSize = false;
    bool hasFontWeight = false;
    bool hasFontStyle = false;
    bool hasTextAlign = false;
    bool hasLineHeight = false;
    bool hasFontFamily = false;
    bool hasLetterSpacing = false;
    bool hasWordSpacing = false;
    bool hasTextOverflow = false;
    bool hasWhiteSpace = false;
    bool hasTextDecoration = false;
    bool hasTextTransform = false;
    bool hasWordBreak = false;
    bool hasVerticalAlign = false;
    bool hasListStyleType = false;
    FastCustomProperties customProperties;
    FastCustomProperties hoverCustomProperties;
    FastCustomProperties focusCustomProperties;
    FastCustomProperties activeCustomProperties;
    std::string unresolvedBackgroundColor;
    std::string unresolvedColor;
    std::string unresolvedBorderColor;
    std::string unresolvedBackgroundGradient;
    CursorType cursor = CursorType::Default;
    float transitionDuration = 0.15f;
    float scale = 1.0f;
    float springStiffness = 180.0f;
    float springDamping = 18.0f;
    std::vector<std::string> animationName;
    std::vector<float> animationDuration;
    std::vector<float> animationDelay;
    std::vector<float> animationIterationCount;
    std::vector<AnimationDirection> animationDirection;
    std::vector<AnimationFillMode> animationFillMode;
    std::vector<AnimationPlayState> animationPlayState;
    std::vector<TimingFunction> animationTimingFunction;
    std::vector<AnimationComposition> animationComposition;
    std::vector<std::string> transitionProperty;
    std::vector<float> transitionDurations;
    std::vector<float> transitionDelays;
    std::vector<TimingFunction> transitionTimingFunctions;
    std::vector<TransitionBehavior> transitionBehavior;
    Color hoverBackgroundColor;
    Color hoverColor;
    Gradient hoverBackgroundGradient;
    float hoverOpacity = -1;
    bool hasHoverBg = false;
    bool hasHoverColor = false;
    bool hasHoverBorder = false;
    bool hasHoverGradient = false;
    Color hoverBorderColor;
    float hoverScale = -1;
    Color focusBackgroundColor;
    Color focusColor;
    Color focusBorderColor;
    Gradient focusBackgroundGradient;
    Border focusOutline;
    bool hasFocusBg = false;
    bool hasFocusColor = false;
    bool hasFocusBorder = false;
    bool hasFocusOutline = false;
    bool hasFocusGradient = false;
    float focusOpacity = -1;
    float focusScale = -1;
    Color activeBackgroundColor;
    Color activeColor;
    Color activeBorderColor;
    Gradient activeBackgroundGradient;
    Border activeOutline;
    bool hasActiveBg = false;
    bool hasActiveColor = false;
    bool hasActiveBorder = false;
    bool hasActiveOutline = false;
    bool hasActiveGradient = false;
    float activeOpacity = -1;
    float activeScale = -1;
    std::vector<TransformOperation> transform;
    TransformOrigin transformOrigin;
    TransformStyle transformStyle = TransformStyle::Flat;
    TransformBox transformBox = TransformBox::BorderBox;
    CSSValue perspective = CSSValue{0.0f, CSSValue::None};
    PerspectiveOrigin perspectiveOrigin;
    BackfaceVisibility backfaceVisibility = BackfaceVisibility::Visible;
    bool hasTransform = false;
    bool hasTransformOrigin = false;
    bool hasTransformStyle = false;
    bool hasTransformBox = false;
    bool hasPerspective = false;
    bool hasPerspectiveOrigin = false;
    bool hasBackfaceVisibility = false;
    uint64_t inheritedHash = 0;
};
class ComputedStyle {
public:
    ComputedStyle() : m_style(std::make_shared<Style>()) {}
    ComputedStyle(const Style& style) : m_style(std::make_shared<Style>(style)) {}
    ComputedStyle(std::shared_ptr<const Style> style) : m_style(std::move(style)) {}
    ComputedStyle(const ComputedStyle& other) = default;
    ComputedStyle& operator=(const ComputedStyle& other) = default;
    ComputedStyle(ComputedStyle&& other) noexcept = default;
    ComputedStyle& operator=(ComputedStyle&& other) noexcept = default;
    const Style* operator->() const { return m_style.get(); }
    Style* operator->() { return &ensureMutable(); }
    const Style& operator*() const { return *m_style; }
    Style& operator*() { return ensureMutable(); }
    operator const Style&() const { return *m_style; }
    explicit operator bool() const { return m_style != nullptr; }
    const Style* get() const { return m_style.get(); }
    Style& ensureMutable() {
        if (!m_style) {
            m_style = std::make_shared<Style>();
        } else if (m_style.use_count() > 1) {
            m_style = std::make_shared<Style>(*m_style);
        }
        return const_cast<Style&>(*m_style);
    }
    ComputedStyle& operator=(std::shared_ptr<const Style> other) {
        m_style = std::move(other);
        return *this;
    }
    ComputedStyle& operator=(const Style& other) {
        m_style = std::make_shared<Style>(other);
        return *this;
    }
    ComputedStyle& operator=(std::nullptr_t) {
        m_style = nullptr;
        return *this;
    }
    bool operator==(const ComputedStyle& other) const {
        return m_style == other.m_style;
    }
    bool operator!=(const ComputedStyle& other) const {
        return m_style != other.m_style;
    }
    bool operator==(std::nullptr_t) const {
        return m_style == nullptr;
    }
    bool operator!=(std::nullptr_t) const {
        return m_style != nullptr;
    }
private:
    std::shared_ptr<const Style> m_style;
};
enum class EventType {
    MouseMove, MouseDown, MouseUp, MouseScroll,
    KeyDown, KeyUp, TextInput,
    WindowResize, WindowClose
};
struct InputEvent {
    EventType type;
    Vec2 mousePos;
    Vec2 mouseDelta;
    Vec2 scroll;
    int mouseButton = 0;
    int keyCode = 0;
    int modifiers = 0;
    std::string text;
    Vec2 windowSize;
    bool consumed = false;
};
struct InputState {
    Vec2 mousePos;
    Vec2 mouseDelta;
    bool mouseDown[3] = {};
    bool mouseClicked[3] = {};
    bool mouseReleased[3] = {};
    int mouseClickCount[3] = {};
    Vec2 scroll;
    float deltaTime = 0.016f;
    Vec2 windowSize;
    int keyCode = 0;
    int modifiers = 0;
    std::string text;
};
}