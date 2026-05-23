// FluxUI - GPU-Accelerated UI Framework with CSS Styling
// Copyright (c) 2026 - MIT License
#pragma once

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

typedef void* NativeWindowHandle;
typedef void* NativeCursorHandle;

enum ModifierBits {
    MOD_NONE = 0,
    MOD_SHIFT = 1 << 0,
    MOD_CTRL = 1 << 1,
    MOD_ALT = 1 << 2,
    MOD_GUI = 1 << 3
};

// ============================================================
//  Core Math Types
// ============================================================

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

// ============================================================
//  Color
// ============================================================

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
};

// ============================================================
//  Geometry
// ============================================================

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
};

struct BorderRadius {
    float tl = 0, tr = 0, br = 0, bl = 0;
    BorderRadius() = default;
    BorderRadius(float all) : tl(all), tr(all), br(all), bl(all) {}
    float maxRadius() const { return std::max({tl, tr, br, bl}); }
    float uniform() const { return tl; } // assumes all same
};

struct BoxShadow {
    float offsetX = 0, offsetY = 0;
    float blur = 0, spread = 0;
    Color color = Color(0, 0, 0, 0.0f);
    bool inset = false;
};

struct Border {
    float width = 0;
    Color color;
    Border() = default;
    Border(float w, Color c) : width(w), color(c) {}
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
};

// ============================================================
//  CSS Enums
// ============================================================

enum class Display { Block, Flex, InlineBlock, Inline, None };
enum class FlexDirection { Row, Column, RowReverse, ColumnReverse };
enum class FlexWrap { NoWrap, Wrap, WrapReverse };
enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignItems { FlexStart, FlexEnd, Center, Stretch, Baseline };
enum class AlignContent { FlexStart, FlexEnd, Center, Stretch, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignSelf { Auto, FlexStart, FlexEnd, Center, Stretch, Baseline };
enum class Position { Static, Relative, Absolute, Fixed, Sticky };
enum class Overflow { Visible, Hidden, Scroll, Auto, Clip };
enum class TextAlign { Left, Center, Right, Justify };
enum class FontWeight { Normal, Bold };
enum class FontStyle { Normal, Italic, Oblique };
enum class CursorType { Default, Pointer, Text, Grab, Grabbing, NotAllowed, Crosshair };
enum class BoxSizing { ContentBox, BorderBox };
enum class Visibility { Visible, Hidden, Collapse };
enum class TextOverflow { Clip, Ellipsis };
enum class WhiteSpace { Normal, NoWrap, Pre, PreWrap, PreLine };
enum class TextDecoration { None, Underline, LineThrough, Overline };
enum class TextTransform { None, Uppercase, Lowercase, Capitalize };
enum class PointerEvents { Auto, None };
enum class WordBreak { Normal, BreakAll, KeepAll, BreakWord };
enum class ObjectFit { Fill, Contain, Cover, None, ScaleDown };
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

// ============================================================
//  CSS Value (supports px, %, auto)
// ============================================================

struct CSSValue {
    enum Unit { Px, Percent, Auto, None, Vw, Vh, Em, Rem, MinContent, MaxContent, FitContent };
    float value = 0;
    Unit unit = None;

    // For calc() support: stores calc operands
    enum CalcOp { CalcNone, CalcAdd, CalcSub, CalcMul, CalcDiv };
    CalcOp calcOp = CalcNone;
    float calcValue2 = 0;
    Unit calcUnit2 = None;

    CSSValue() = default;
    CSSValue(float v, Unit u = Px) : value(v), unit(u) {}

    static CSSValue px(float v) { return {v, Px}; }
    static CSSValue pct(float v) { return {v, Percent}; }
    static CSSValue autoVal() { return {0, Auto}; }
    static CSSValue vw(float v) { return {v, Vw}; }
    static CSSValue vh(float v) { return {v, Vh}; }
    static CSSValue em(float v) { return {v, Em}; }
    static CSSValue rem(float v) { return {v, Rem}; }
    static CSSValue minContent() { return {0, MinContent}; }
    static CSSValue maxContent() { return {0, MaxContent}; }
    static CSSValue fitContent() { return {0, FitContent}; }

    float resolve(float parentSize, float viewportW = 1920.0f, float viewportH = 1080.0f, float emBase = 16.0f) const {
        float primary = resolveUnit(value, unit, parentSize, viewportW, viewportH, emBase);
        if (calcOp == CalcNone) return primary;
        float secondary = resolveUnit(calcValue2, calcUnit2, parentSize, viewportW, viewportH, emBase);
        switch (calcOp) {
            case CalcAdd: return primary + secondary;
            case CalcSub: return primary - secondary;
            case CalcMul: return primary * secondary;
            case CalcDiv: return secondary != 0.0f ? primary / secondary : 0.0f;
            default: return primary;
        }
    }


    bool isAuto() const { return unit == Auto; }
    bool isSet() const { return unit != None; }
    bool isIntrinsic() const { return unit == MinContent || unit == MaxContent || unit == FitContent; }

private:
    static float resolveUnit(float val, Unit u, float parentSize, float vpW, float vpH, float emBase) {
        switch (u) {
            case Percent: return val * parentSize / 100.0f;
            case Px: return val;
            case Vw: return val * vpW / 100.0f;
            case Vh: return val * vpH / 100.0f;
            case Em: return val * emBase;
            case Rem: return val * 16.0f;
            default: return 0;
        }
    }
};

struct FastCustomProperties {
    std::unique_ptr<std::unordered_map<std::string, std::string>> map;

    FastCustomProperties() = default;

    FastCustomProperties(const FastCustomProperties& other) {
        if (other.map && !other.map->empty()) {
            map = std::make_unique<std::unordered_map<std::string, std::string>>(*other.map);
        }
    }

    FastCustomProperties& operator=(const FastCustomProperties& other) {
        if (this == &other) return *this;
        if (!other.map || other.map->empty()) {
            map.reset();
        } else {
            if (map) {
                *map = *other.map;
            } else {
                map = std::make_unique<std::unordered_map<std::string, std::string>>(*other.map);
            }
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
            map = std::make_unique<std::unordered_map<std::string, std::string>>(other);
        }
    }

    FastCustomProperties& operator=(const std::unordered_map<std::string, std::string>& other) {
        if (other.empty()) {
            map.reset();
        } else {
            if (map) {
                *map = other;
            } else {
                map = std::make_unique<std::unordered_map<std::string, std::string>>(other);
            }
        }
        return *this;
    }

    const std::unordered_map<std::string, std::string>* getMapPointer() const {
        return map.get();
    }

    std::unordered_map<std::string, std::string>* getMapPointer() {
        return map.get();
    }

    std::unordered_map<std::string, std::string>& getOrCreateMap() {
        if (!map) {
            map = std::make_unique<std::unordered_map<std::string, std::string>>();
        }
        return *map;
    }

    operator const std::unordered_map<std::string, std::string>&() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? *map : emptyMap;
    }

    auto begin() {
        if (!map) {
            map = std::make_unique<std::unordered_map<std::string, std::string>>();
        }
        return map->begin();
    }

    auto begin() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->begin() : emptyMap.begin();
    }

    auto end() {
        if (!map) {
            map = std::make_unique<std::unordered_map<std::string, std::string>>();
        }
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
        if (!map) {
            map = std::make_unique<std::unordered_map<std::string, std::string>>();
        }
        return (*map)[key];
    }

    auto find(const std::string& key) {
        if (!map) {
            map = std::make_unique<std::unordered_map<std::string, std::string>>();
        }
        return map->find(key);
    }

    auto find(const std::string& key) const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->find(key) : emptyMap.find(key);
    }
};

// ============================================================
//  Style (all CSS properties for a widget)
// ============================================================

struct Style {
    // Display & Layout (matching Blink ComputedStyle)
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
    float rowGap = 0;   // Blink separates row-gap and column-gap
    float columnGap = 0;
    Overflow overflow = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;
    Overflow overflowY = Overflow::Visible;
    BoxSizing boxSizing = BoxSizing::ContentBox;
    bool hasBoxSizing = false;
    Visibility visibility = Visibility::Visible;
    PointerEvents pointerEvents = PointerEvents::Auto;
    int zIndex = 0;
    bool hasZIndex = false;
    float aspectRatio = 0;  // 0 means auto
    ObjectFit objectFit = ObjectFit::Fill;
    bool hasObjectFit = false;
    Appearance appearance = Appearance::Auto;
    bool hasAppearance = false;
    Vec2 objectPosition = {0.5f, 0.5f};
    Vec2 objectPositionOffset = {0.0f, 0.0f};
    bool hasObjectPosition = false;

    // Dimensions
    CSSValue width, height;
    CSSValue minWidth, minHeight;
    CSSValue maxWidth, maxHeight;

    // Spacing
    EdgeInsets padding;
    EdgeInsets margin;

    // Position offsets
    CSSValue top, right, bottom, left;

    // Visual
    Color color = Color(1, 1, 1, 1);             // text color
    Color backgroundColor = Color(0, 0, 0, 0);   // transparent by default
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

    // Typography (matching Blink inherited properties)
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
    float letterSpacing = 0;     // Blink: letter-spacing
    float wordSpacing = 0;       // Blink: word-spacing
    Color textDecorationColor;   // defaults to currentColor
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
    FastCustomProperties customProperties;

    // Interaction
    CursorType cursor = CursorType::Default;
    float transitionDuration = 0.15f; // seconds
    float scale = 1.0f; // 1.0 = normal size
    float springStiffness = 180.0f;  // for physics-based animations
    float springDamping = 18.0f;     // for physics-based animations

    // Hover state overrides
    Color hoverBackgroundColor;
    Color hoverColor;
    float hoverOpacity = -1; // -1 means no override
    bool hasHoverBg = false;
    bool hasHoverColor = false;
    bool hasHoverBorder = false;
    Color hoverBorderColor;
    float hoverScale = -1; // -1 means no override

    // Focus state overrides (:focus)
    Color focusBackgroundColor;
    Color focusColor;
    Color focusBorderColor;
    Border focusOutline;
    bool hasFocusBg = false;
    bool hasFocusColor = false;
    bool hasFocusBorder = false;
    bool hasFocusOutline = false;
    float focusOpacity = -1;
    float focusScale = -1;

    // Active state overrides (:active)
    Color activeBackgroundColor;
    Color activeColor;
    Color activeBorderColor;
    Border activeOutline;
    bool hasActiveBg = false;
    bool hasActiveColor = false;
    bool hasActiveBorder = false;
    bool hasActiveOutline = false;
    float activeOpacity = -1;
    float activeScale = -1;
    uint64_t inheritedHash = 0;
};

// ============================================================
//  Events
// ============================================================

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

// ============================================================
//  Input State (current frame)
// ============================================================

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

} // namespace FluxUI
