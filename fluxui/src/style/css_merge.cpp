// FluxUI - CSS property merge (applies parsed declarations onto a Style).
// Extracted from css_parser.cpp: StyleSheet::mergeProperty and its Part1/2/3
// halves plus the :hover / :focus / :active state-property mergers. This is the
// big property-name dispatch that maps CSS declarations to Style fields.
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include "css_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace FluxUI {
using detail::trimLocal;
using detail::lowerAscii;
using detail::functionInner;
using detail::splitColorTokens;
using detail::splitWhitespace;
using detail::splitWhitespaceTopLevel;

// ── Overflow + object-position helpers (used only by the merge dispatch) ──
static Overflow parseOverflowKeyword(const std::string& value) {
    if (value == "hidden") return Overflow::Hidden;
    if (value == "scroll") return Overflow::Scroll;
    if (value == "auto") return Overflow::Auto;
    if (value == "clip") return Overflow::Clip;
    return Overflow::Visible;
}
static bool isOverflowVisibleOrClip(Overflow overflow) {
    return overflow == Overflow::Visible || overflow == Overflow::Clip;
}
static Overflow normalizeOverflowAxis(Overflow axis, Overflow otherAxis) {
    if (!isOverflowVisibleOrClip(otherAxis)) {
        if (axis == Overflow::Visible) return Overflow::Auto;
        if (axis == Overflow::Clip) return Overflow::Hidden;
    }
    return axis;
}
static void normalizeOverflowAxes(Style& style) {
    Overflow x = style.overflowX;
    Overflow y = style.overflowY;
    style.overflowX = normalizeOverflowAxis(x, y);
    style.overflowY = normalizeOverflowAxis(y, x);
    style.overflow = style.overflowY;
}
struct ObjectPositionAxis {
    float fraction = 0.5f;
    float offset = 0.0f;
    bool set = false;
};
static bool isObjectPositionLength(const std::string& token) {
    if (token.empty()) return false;
    char first = token[0];
    return first == '-' || first == '+' || first == '.' ||
           std::isdigit((unsigned char)first);
}
static bool setObjectPositionKeyword(const std::string& token,
                                     ObjectPositionAxis& x,
                                     ObjectPositionAxis& y) {
    if (token == "left") {
        x.fraction = 0.0f;
        x.offset = 0.0f;
        x.set = true;
        return true;
    }
    if (token == "right") {
        x.fraction = 1.0f;
        x.offset = 0.0f;
        x.set = true;
        return true;
    }
    if (token == "top") {
        y.fraction = 0.0f;
        y.offset = 0.0f;
        y.set = true;
        return true;
    }
    if (token == "bottom") {
        y.fraction = 1.0f;
        y.offset = 0.0f;
        y.set = true;
        return true;
    }
    if (token == "center") {
        if (!x.set) {
            x.fraction = 0.5f;
            x.offset = 0.0f;
            x.set = true;
        } else {
            y.fraction = 0.5f;
            y.offset = 0.0f;
            y.set = true;
        }
        return true;
    }
    return false;
}
static float parseObjectPositionFloat(const std::string& token) {
    char* end = nullptr;
    float value = parseLocaleIndependentFloat(token.c_str(), &end);
    return end == token.c_str() ? 0.0f : value;
}
static float parseObjectPositionLengthPixels(const std::string& token) {
    if (token.size() > 3 && token.substr(token.size() - 3) == "rem") {
        return parseObjectPositionFloat(token) * 16.0f;
    }
    if (token.size() > 2 && token.substr(token.size() - 2) == "em") {
        return parseObjectPositionFloat(token) * 16.0f;
    }
    return parseObjectPositionFloat(token);
}
static void setObjectPositionLength(const std::string& token,
                                    ObjectPositionAxis& axis,
                                    bool fromEnd) {
    if (!token.empty() && token.back() == '%') {
        axis.fraction = std::clamp(parseObjectPositionFloat(token) / 100.0f, -1.0f, 2.0f);
        axis.offset = 0.0f;
    } else {
        float px = parseObjectPositionLengthPixels(token);
        axis.fraction = fromEnd ? 1.0f : 0.0f;
        axis.offset = fromEnd ? -px : px;
    }
    axis.set = true;
}
static bool parseObjectPosition(const std::string& value,
                                Vec2& position,
                                Vec2& offset) {
    std::istringstream ss(value);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) tokens.push_back(lowerAscii(token));
    if (tokens.empty() || tokens.size() > 4) return false;
    ObjectPositionAxis x;
    ObjectPositionAxis y;
    bool sawValidToken = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& current = tokens[i];
        bool horizontalEnd = current == "right";
        bool verticalEnd = current == "bottom";
        bool isEdge = current == "left" || current == "right" ||
                      current == "top" || current == "bottom";
        if (isEdge) {
            setObjectPositionKeyword(current, x, y);
            sawValidToken = true;
            if (i + 1 < tokens.size() && isObjectPositionLength(tokens[i + 1])) {
                if (current == "left" || current == "right") {
                    setObjectPositionLength(tokens[++i], x, horizontalEnd);
                } else {
                    setObjectPositionLength(tokens[++i], y, verticalEnd);
                }
                sawValidToken = true;
            }
            continue;
        }
        if (current == "center") {
            setObjectPositionKeyword(current, x, y);
            sawValidToken = true;
            continue;
        }
        if (isObjectPositionLength(current)) {
            if (!x.set) {
                setObjectPositionLength(current, x, false);
            } else {
                setObjectPositionLength(current, y, false);
            }
            sawValidToken = true;
            continue;
        }
        return false;
    }
    if (!sawValidToken) return false;
    position = {x.set ? x.fraction : 0.5f, y.set ? y.fraction : 0.5f};
    offset = {x.set ? x.offset : 0.0f, y.set ? y.offset : 0.0f};
    return true;
}

void StyleSheet::mergeProperty(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name == "padding") {
        style.hasPaddingInlineStart = false; style.hasPaddingInlineEnd = false;
        style.hasPaddingBlockStart = false; style.hasPaddingBlockEnd = false;
    } else if (name == "padding-top") {
        style.hasPaddingBlockStart = false; style.hasPaddingInlineStart = false;
    } else if (name == "padding-bottom") {
        style.hasPaddingBlockEnd = false; style.hasPaddingInlineEnd = false;
    } else if (name == "padding-left") {
        style.hasPaddingInlineStart = false; style.hasPaddingBlockStart = false;
    } else if (name == "padding-right") {
        style.hasPaddingInlineEnd = false; style.hasPaddingBlockEnd = false;
    } else if (name == "margin") {
        style.hasMarginInlineStart = false; style.hasMarginInlineEnd = false;
        style.hasMarginBlockStart = false; style.hasMarginBlockEnd = false;
    } else if (name == "margin-top") {
        style.hasMarginBlockStart = false; style.hasMarginInlineStart = false;
    } else if (name == "margin-bottom") {
        style.hasMarginBlockEnd = false; style.hasMarginInlineEnd = false;
    } else if (name == "margin-left") {
        style.hasMarginInlineStart = false; style.hasMarginBlockStart = false;
    } else if (name == "margin-right") {
        style.hasMarginInlineEnd = false; style.hasMarginBlockEnd = false;
    } else if (name == "width") {
        style.hasInlineSize = false; style.hasBlockSize = false;
    } else if (name == "height") {
        style.hasInlineSize = false; style.hasBlockSize = false;
    } else if (name == "min-width") {
        style.hasMinInlineSize = false; style.hasMinBlockSize = false;
    } else if (name == "min-height") {
        style.hasMinInlineSize = false; style.hasMinBlockSize = false;
    } else if (name == "max-width") {
        style.hasMaxInlineSize = false; style.hasMaxBlockSize = false;
    } else if (name == "max-height") {
        style.hasMaxInlineSize = false; style.hasMaxBlockSize = false;
    } else if (name == "border") {
        style.hasBorderInlineStart = false; style.hasBorderInlineEnd = false;
        style.hasBorderBlockStart = false; style.hasBorderBlockEnd = false;
    } else if (name == "border-top") {
        style.hasBorderBlockStart = false; style.hasBorderInlineStart = false;
    } else if (name == "border-bottom") {
        style.hasBorderBlockEnd = false; style.hasBorderInlineEnd = false;
    } else if (name == "border-left") {
        style.hasBorderInlineStart = false; style.hasBorderBlockStart = false;
    } else if (name == "border-right") {
        style.hasBorderInlineEnd = false; style.hasBorderBlockEnd = false;
    } else if (name == "top" || name == "bottom" || name == "left" || name == "right") {
        style.hasInsetInlineStart = false; style.hasInsetInlineEnd = false;
        style.hasInsetBlockStart = false; style.hasInsetBlockEnd = false;
    }
    if (mergePropertyPart1(style, name, value, emBase)) return;
    mergePropertyPart2(style, name, value, emBase);
    mergePropertyPart3(style, name, value, emBase);
}
static bool isDynamicValue(const std::string& val) {
    return val.find("var(") != std::string::npos ||
           val.find("min(") != std::string::npos ||
           val.find("max(") != std::string::npos ||
           val.find("clamp(") != std::string::npos ||
           val.find("calc(") != std::string::npos;
}
bool StyleSheet::mergePropertyPart1(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name.rfind("--", 0) == 0) {
        style.customProperties[name] = value;
    } else if (name == "color") {
        if (isDynamicValue(value)) {
            style.unresolvedColor = value;
        } else {
            style.color = parseColor(value);
            style.hasColor = true;
        }
    } else if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            if (isDynamicValue(value)) {
                style.unresolvedBackgroundGradient = value;
            } else {
                style.backgroundGradient = parseGradient(value);
            }
        } else {
            if (isDynamicValue(value)) {
                style.unresolvedBackgroundColor = value;
            } else {
                style.backgroundColor = parseColor(value);
            }
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            if (isDynamicValue(value)) {
                style.unresolvedBackgroundGradient = value;
            } else {
                style.backgroundGradient = parseGradient(value);
            }
        }
    } else if (name == "filter") {
        if (value == "none") {
            style.filterOperations.clear();
            style.hasFilter = false;
        } else {
            style.filterOperations = parseFilterOperations(value, emBase);
            style.hasFilter = !style.filterOperations.empty();
        }
    } else if (name == "backdrop-filter") {
        if (value == "none") {
            style.backdropFilterOperations.clear();
            style.hasBackdropFilter = false;
            style.hasBackdropFilterBlur = false;
            style.backdropFilterBlur = 0.0f;
        } else {
            style.backdropFilterOperations = parseFilterOperations(value, emBase);
            style.hasBackdropFilter = !style.backdropFilterOperations.empty();
            // Sync the legacy scalar for the renderer drawBackdropFilterBlur() path.
            style.backdropFilterBlur = 0.0f;
            style.hasBackdropFilterBlur = false;
            for (const auto& op : style.backdropFilterOperations) {
                if (op.type == FilterOperationType::Blur) {
                    style.backdropFilterBlur = op.amount;
                    style.hasBackdropFilterBlur = true;
                    break;
                }
            }
        }
    } else if (name == "clip-path") {
        style.clipPath = value;
        style.hasClipPath = (value != "none" && !value.empty());
    } else if (name == "shape-outside") {
        style.shapeOutside = value;
        style.hasShapeOutside = (value != "none" && !value.empty());
    } else if (name == "mask" || name == "mask-image") {
        style.maskImage = value;
        style.hasMask = (value != "none" && !value.empty());
    } else if (name == "mask-mode") {
        style.maskMode = value;
    } else if (name == "mask-repeat") {
        style.maskRepeat = value;
    } else if (name == "mask-position") {
        style.maskPosition = value;
    } else if (name == "mask-size") {
        style.maskSize = value;
    } else if (name == "mask-clip") {
        style.maskClip = value;
    } else if (name == "mask-origin") {
        style.maskOrigin = value;
    } else if (name == "mask-composite") {
        style.maskComposite = value;
    } else if (name == "mix-blend-mode") {
        std::string v = lowerAscii(trim(value));
        style.hasMixBlendMode = true;
        if (v == "multiply")        style.mixBlendMode = Style::BlendMode::Multiply;
        else if (v == "screen")     style.mixBlendMode = Style::BlendMode::Screen;
        else if (v == "overlay")    style.mixBlendMode = Style::BlendMode::Overlay;
        else if (v == "darken")     style.mixBlendMode = Style::BlendMode::Darken;
        else if (v == "lighten")    style.mixBlendMode = Style::BlendMode::Lighten;
        else if (v == "color-dodge")style.mixBlendMode = Style::BlendMode::ColorDodge;
        else if (v == "color-burn") style.mixBlendMode = Style::BlendMode::ColorBurn;
        else if (v == "hard-light") style.mixBlendMode = Style::BlendMode::HardLight;
        else if (v == "soft-light") style.mixBlendMode = Style::BlendMode::SoftLight;
        else if (v == "difference") style.mixBlendMode = Style::BlendMode::Difference;
        else if (v == "exclusion")  style.mixBlendMode = Style::BlendMode::Exclusion;
        else if (v == "hue")        style.mixBlendMode = Style::BlendMode::Hue;
        else if (v == "saturation") style.mixBlendMode = Style::BlendMode::Saturation;
        else if (v == "color")      style.mixBlendMode = Style::BlendMode::Color;
        else if (v == "luminosity") style.mixBlendMode = Style::BlendMode::Luminosity;
        else { style.mixBlendMode = Style::BlendMode::Normal; style.hasMixBlendMode = (v != "normal"); }
    } else if (name == "isolation") {
        std::string v = lowerAscii(trim(value));
        style.hasIsolation = true;
        style.isolation = (v == "isolate") ? Style::Isolation::Isolate : Style::Isolation::Auto;
    } else if (name == "background-blend-mode") {
        std::string v = lowerAscii(trim(value));
        style.hasBackgroundBlendMode = true;
        if (v == "multiply")        style.backgroundBlendMode = Style::BlendMode::Multiply;
        else if (v == "screen")     style.backgroundBlendMode = Style::BlendMode::Screen;
        else if (v == "overlay")    style.backgroundBlendMode = Style::BlendMode::Overlay;
        else if (v == "darken")     style.backgroundBlendMode = Style::BlendMode::Darken;
        else if (v == "lighten")    style.backgroundBlendMode = Style::BlendMode::Lighten;
        else if (v == "color-dodge")style.backgroundBlendMode = Style::BlendMode::ColorDodge;
        else if (v == "color-burn") style.backgroundBlendMode = Style::BlendMode::ColorBurn;
        else if (v == "hard-light") style.backgroundBlendMode = Style::BlendMode::HardLight;
        else if (v == "soft-light") style.backgroundBlendMode = Style::BlendMode::SoftLight;
        else if (v == "difference") style.backgroundBlendMode = Style::BlendMode::Difference;
        else if (v == "exclusion")  style.backgroundBlendMode = Style::BlendMode::Exclusion;
        else if (v == "hue")        style.backgroundBlendMode = Style::BlendMode::Hue;
        else if (v == "saturation") style.backgroundBlendMode = Style::BlendMode::Saturation;
        else if (v == "color")      style.backgroundBlendMode = Style::BlendMode::Color;
        else if (v == "luminosity") style.backgroundBlendMode = Style::BlendMode::Luminosity;
        else                        style.backgroundBlendMode = Style::BlendMode::Normal;
    } else if (name == "scroll-snap-type") {
        style.scrollSnapType = value;
        style.hasScrollSnapType = (value != "none" && !value.empty());
    } else if (name == "scroll-snap-align") {
        style.scrollSnapAlign = value;
        style.hasScrollSnapAlign = (value != "none" && !value.empty());
    } else if (name == "scroll-snap-stop") {
        style.scrollSnapStop = lowerAscii(trim(value));
    } else if (name == "scroll-padding") {
        style.scrollPadding = parseEdgeInsets(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-top") {
        style.scrollPadding.top = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-right") {
        style.scrollPadding.right = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-bottom") {
        style.scrollPadding.bottom = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-left") {
        style.scrollPadding.left = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-margin") {
        style.scrollMargin = parseEdgeInsets(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-top") {
        style.scrollMargin.top = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-right") {
        style.scrollMargin.right = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-bottom") {
        style.scrollMargin.bottom = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-left") {
        style.scrollMargin.left = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "overscroll-behavior") {
        std::string v = lowerAscii(trim(value));
        style.hasOverscrollBehavior = true;
        auto parseOB = [](const std::string& s) -> Style::OverscrollBehavior {
            if (s == "contain") return Style::OverscrollBehavior::Contain;
            if (s == "none") return Style::OverscrollBehavior::None;
            return Style::OverscrollBehavior::Auto;
        };
        std::istringstream iss(v); std::string t1, t2;
        iss >> t1; iss >> t2;
        style.overscrollBehaviorX = parseOB(t1);
        style.overscrollBehaviorY = t2.empty() ? style.overscrollBehaviorX : parseOB(t2);
    } else if (name == "overscroll-behavior-x") {
        std::string v = lowerAscii(trim(value));
        style.hasOverscrollBehavior = true;
        if (v == "contain") style.overscrollBehaviorX = Style::OverscrollBehavior::Contain;
        else if (v == "none") style.overscrollBehaviorX = Style::OverscrollBehavior::None;
        else style.overscrollBehaviorX = Style::OverscrollBehavior::Auto;
    } else if (name == "overscroll-behavior-y") {
        std::string v = lowerAscii(trim(value));
        style.hasOverscrollBehavior = true;
        if (v == "contain") style.overscrollBehaviorY = Style::OverscrollBehavior::Contain;
        else if (v == "none") style.overscrollBehaviorY = Style::OverscrollBehavior::None;
        else style.overscrollBehaviorY = Style::OverscrollBehavior::Auto;
    } else if (name == "scrollbar-color") {
        std::string v = trim(value);
        if (lowerAscii(v) == "auto") {
            style.hasScrollbarColor = false;
        } else {
            // scrollbar-color: <thumb-color> <track-color>
            auto tokens = splitColorTokens(functionInner("(" + v + ")"));
            if (tokens.size() >= 2) {
                style.scrollbarThumbColor = parseColor(tokens[0]);
                style.scrollbarTrackColor = parseColor(tokens[1]);
                style.hasScrollbarColor = true;
            } else if (tokens.size() == 1) {
                style.scrollbarThumbColor = parseColor(tokens[0]);
                style.scrollbarTrackColor = parseColor(tokens[0]);
                style.hasScrollbarColor = true;
            }
        }
    } else if (name == "scrollbar-width") {
        std::string v = lowerAscii(trim(value));
        style.hasScrollbarWidth = true;
        if (v == "thin") style.scrollbarWidth = Style::ScrollbarWidth::Thin;
        else if (v == "none") style.scrollbarWidth = Style::ScrollbarWidth::None;
        else style.scrollbarWidth = Style::ScrollbarWidth::Auto;
    } else if (name == "overflow-anchor") {
        std::string v = lowerAscii(trim(value));
        style.hasOverflowAnchor = true;
        style.overflowAnchor = (v == "none") ? Style::OverflowAnchor::None : Style::OverflowAnchor::Auto;
    } else if (name == "scrollbar-gutter") {
        style.scrollbarGutter = lowerAscii(trim(value));
        style.hasScrollbarGutter = !style.scrollbarGutter.empty() && style.scrollbarGutter != "auto";
    } else if (name == "scroll-behavior") {
        std::string v = lowerAscii(trim(value));
        style.hasScrollBehavior = true;
        style.scrollBehavior = (v == "smooth") ? Style::ScrollBehavior::Smooth : Style::ScrollBehavior::Auto;
    } else if (name == "border-radius") {
        style.borderRadius = parseBorderRadius(value, emBase);
    } else if (name == "border") {
        if (isDynamicValue(value)) {
            size_t varPos = value.find("var(");
            if (varPos == std::string::npos) varPos = value.find("min(");
            if (varPos == std::string::npos) varPos = value.find("max(");
            if (varPos == std::string::npos) varPos = value.find("clamp(");
            if (varPos != std::string::npos) {
                int depth = 1;
                size_t cursor = varPos + 4;
                while (cursor < value.size() && depth > 0) {
                    if (value[cursor] == '(') depth++;
                    if (value[cursor] == ')') depth--;
                    if (depth > 0) cursor++;
                }
                if (cursor < value.size()) {
                    style.unresolvedBorderColor = value.substr(varPos, cursor - varPos + 1);
                }
            }
        }
        Border b = parseBorder(value, emBase);
        style.borderT = b; style.hasBorderT = true; style.orderBorderTop = ++style.propertyOrder;
        style.borderR = b; style.hasBorderR = true; style.orderBorderRight = ++style.propertyOrder;
        style.borderB = b; style.hasBorderB = true; style.orderBorderBottom = ++style.propertyOrder;
        style.borderL = b; style.hasBorderL = true; style.orderBorderLeft = ++style.propertyOrder;
        style.border = b;
    } else if (name == "border-top") {
        style.borderT = parseBorder(value, emBase);
        style.hasBorderT = true;
        style.orderBorderTop = ++style.propertyOrder;
    } else if (name == "border-right") {
        style.borderR = parseBorder(value, emBase);
        style.hasBorderR = true;
        style.orderBorderRight = ++style.propertyOrder;
    } else if (name == "border-bottom") {
        style.borderB = parseBorder(value, emBase);
        style.hasBorderB = true;
        style.orderBorderBottom = ++style.propertyOrder;
    } else if (name == "border-left") {
        style.borderL = parseBorder(value, emBase);
        style.hasBorderL = true;
        style.orderBorderLeft = ++style.propertyOrder;
    } else if (name == "border-block-start") {
        style.borderBlockStart = parseBorder(value, emBase);
        style.hasBorderBlockStart = true;
        style.orderBorderBlockStart = ++style.propertyOrder;
    } else if (name == "border-block-end") {
        style.borderBlockEnd = parseBorder(value, emBase);
        style.hasBorderBlockEnd = true;
        style.orderBorderBlockEnd = ++style.propertyOrder;
    } else if (name == "border-inline-start") {
        style.borderInlineStart = parseBorder(value, emBase);
        style.hasBorderInlineStart = true;
        style.orderBorderInlineStart = ++style.propertyOrder;
    } else if (name == "border-inline-end") {
        style.borderInlineEnd = parseBorder(value, emBase);
        style.hasBorderInlineEnd = true;
        style.orderBorderInlineEnd = ++style.propertyOrder;
    } else if (name == "border-color") {
        if (isDynamicValue(value)) {
            style.unresolvedBorderColor = value;
        } else {
            style.border.color = parseColor(value);
        }
    } else if (name == "border-width") {
        style.border.width = parseLengthPixels(value, emBase);
    } else if (name == "outline") {
        style.outline = parseBorder(value, emBase);
    } else if (name == "outline-color") {
        style.outline.color = parseColor(value);
    } else if (name == "outline-width") {
        style.outline.width = parseLengthPixels(value, emBase);
    } else if (name == "outline-offset") {
        style.outlineOffset = parseLengthPixels(value, emBase);
    } else if (name == "padding") {
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.paddingT = inset.top; style.hasPaddingT = true; style.orderPaddingTop = ++style.propertyOrder;
        style.paddingR = inset.right; style.hasPaddingR = true; style.orderPaddingRight = ++style.propertyOrder;
        style.paddingB = inset.bottom; style.hasPaddingB = true; style.orderPaddingBottom = ++style.propertyOrder;
        style.paddingL = inset.left; style.hasPaddingL = true; style.orderPaddingLeft = ++style.propertyOrder;
    } else if (name == "padding-top") {
        style.paddingT = parseLengthPixels(value, emBase);
        style.hasPaddingT = true;
        style.orderPaddingTop = ++style.propertyOrder;
    } else if (name == "padding-right") {
        style.paddingR = parseLengthPixels(value, emBase);
        style.hasPaddingR = true;
        style.orderPaddingRight = ++style.propertyOrder;
    } else if (name == "padding-bottom") {
        style.paddingB = parseLengthPixels(value, emBase);
        style.hasPaddingB = true;
        style.orderPaddingBottom = ++style.propertyOrder;
    } else if (name == "padding-left") {
        style.paddingL = parseLengthPixels(value, emBase);
        style.hasPaddingL = true;
        style.orderPaddingLeft = ++style.propertyOrder;
    } else if (name == "padding-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.paddingBlockStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.paddingBlockEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.paddingBlockStart;
        style.hasPaddingBlockStart = count > 0;
        style.hasPaddingBlockEnd = count > 0;
        style.orderPaddingBlockStart = ++style.propertyOrder;
        style.orderPaddingBlockEnd = ++style.propertyOrder;
    } else if (name == "padding-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.paddingInlineStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.paddingInlineEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.paddingInlineStart;
        style.hasPaddingInlineStart = count > 0;
        style.hasPaddingInlineEnd = count > 0;
        style.orderPaddingInlineStart = ++style.propertyOrder;
        style.orderPaddingInlineEnd = ++style.propertyOrder;
    } else if (name == "padding-block-start") {
        style.paddingBlockStart = parseLengthPixels(value, emBase);
        style.hasPaddingBlockStart = true;
        style.orderPaddingBlockStart = ++style.propertyOrder;
    } else if (name == "padding-block-end") {
        style.paddingBlockEnd = parseLengthPixels(value, emBase);
        style.hasPaddingBlockEnd = true;
        style.orderPaddingBlockEnd = ++style.propertyOrder;
    } else if (name == "padding-inline-start") {
        style.paddingInlineStart = parseLengthPixels(value, emBase);
        style.hasPaddingInlineStart = true;
        style.orderPaddingInlineStart = ++style.propertyOrder;
    } else if (name == "padding-inline-end") {
        style.paddingInlineEnd = parseLengthPixels(value, emBase);
        style.hasPaddingInlineEnd = true;
        style.orderPaddingInlineEnd = ++style.propertyOrder;
    } else if (name == "margin") {
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.marginT = inset.top; style.hasMarginT = true; style.orderMarginTop = ++style.propertyOrder;
        style.marginR = inset.right; style.hasMarginR = true; style.orderMarginRight = ++style.propertyOrder;
        style.marginB = inset.bottom; style.hasMarginB = true; style.orderMarginBottom = ++style.propertyOrder;
        style.marginL = inset.left; style.hasMarginL = true; style.orderMarginLeft = ++style.propertyOrder;
    } else if (name == "margin-top") {
        style.marginT = parseLengthPixels(value, emBase);
        style.hasMarginT = true;
        style.orderMarginTop = ++style.propertyOrder;
    } else if (name == "margin-right") {
        style.marginR = parseLengthPixels(value, emBase);
        style.hasMarginR = true;
        style.orderMarginRight = ++style.propertyOrder;
    } else if (name == "margin-bottom") {
        style.marginB = parseLengthPixels(value, emBase);
        style.hasMarginB = true;
        style.orderMarginBottom = ++style.propertyOrder;
    } else if (name == "margin-left") {
        style.marginL = parseLengthPixels(value, emBase);
        style.hasMarginL = true;
        style.orderMarginLeft = ++style.propertyOrder;
    } else if (name == "margin-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.marginBlockStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.marginBlockEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.marginBlockStart;
        style.hasMarginBlockStart = count > 0;
        style.hasMarginBlockEnd = count > 0;
        style.orderMarginBlockStart = ++style.propertyOrder;
        style.orderMarginBlockEnd = ++style.propertyOrder;
    } else if (name == "margin-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.marginInlineStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.marginInlineEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.marginInlineStart;
        style.hasMarginInlineStart = count > 0;
        style.hasMarginInlineEnd = count > 0;
        style.orderMarginInlineStart = ++style.propertyOrder;
        style.orderMarginInlineEnd = ++style.propertyOrder;
    } else if (name == "margin-block-start") {
        style.marginBlockStart = parseLengthPixels(value, emBase);
        style.hasMarginBlockStart = true;
        style.orderMarginBlockStart = ++style.propertyOrder;
    } else if (name == "margin-block-end") {
        style.marginBlockEnd = parseLengthPixels(value, emBase);
        style.hasMarginBlockEnd = true;
        style.orderMarginBlockEnd = ++style.propertyOrder;
    } else if (name == "margin-inline-start") {
        style.marginInlineStart = parseLengthPixels(value, emBase);
        style.hasMarginInlineStart = true;
        style.orderMarginInlineStart = ++style.propertyOrder;
    } else if (name == "margin-inline-end") {
        style.marginInlineEnd = parseLengthPixels(value, emBase);
        style.hasMarginInlineEnd = true;
        style.orderMarginInlineEnd = ++style.propertyOrder;
    } else if (name == "inset") {
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.topVal = CSSValue::px(inset.top); style.hasTopVal = true; style.orderTop = ++style.propertyOrder;
        style.rightVal = CSSValue::px(inset.right); style.hasRightVal = true; style.orderRight = ++style.propertyOrder;
        style.bottomVal = CSSValue::px(inset.bottom); style.hasBottomVal = true; style.orderBottom = ++style.propertyOrder;
        style.leftVal = CSSValue::px(inset.left); style.hasLeftVal = true; style.orderLeft = ++style.propertyOrder;
    } else if (name == "inset-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.insetBlockStart = count > 0 ? parseCSSValue(std::string(tokens[0])) : CSSValue();
        style.insetBlockEnd = count > 1 ? parseCSSValue(std::string(tokens[1])) : style.insetBlockStart;
        style.hasInsetBlockStart = count > 0;
        style.hasInsetBlockEnd = count > 0;
        style.orderInsetBlockStart = ++style.propertyOrder;
        style.orderInsetBlockEnd = ++style.propertyOrder;
    } else if (name == "inset-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.insetInlineStart = count > 0 ? parseCSSValue(std::string(tokens[0])) : CSSValue();
        style.insetInlineEnd = count > 1 ? parseCSSValue(std::string(tokens[1])) : style.insetInlineStart;
        style.hasInsetInlineStart = count > 0;
        style.hasInsetInlineEnd = count > 0;
        style.orderInsetInlineStart = ++style.propertyOrder;
        style.orderInsetInlineEnd = ++style.propertyOrder;
    } else if (name == "inset-block-start") {
        style.insetBlockStart = parseCSSValue(value);
        style.hasInsetBlockStart = true;
        style.orderInsetBlockStart = ++style.propertyOrder;
    } else if (name == "inset-block-end") {
        style.insetBlockEnd = parseCSSValue(value);
        style.hasInsetBlockEnd = true;
        style.orderInsetBlockEnd = ++style.propertyOrder;
    } else if (name == "inset-inline-start") {
        style.insetInlineStart = parseCSSValue(value);
        style.hasInsetInlineStart = true;
        style.orderInsetInlineStart = ++style.propertyOrder;
    } else if (name == "inset-inline-end") {
        style.insetInlineEnd = parseCSSValue(value);
        style.hasInsetInlineEnd = true;
        style.orderInsetInlineEnd = ++style.propertyOrder;
    } else if (name == "width") {
        style.width = parseCSSValue(value);
        style.hasWidthVal = true;
        style.orderWidth = ++style.propertyOrder;
    } else if (name == "inline-size") {
        style.inlineSize = parseCSSValue(value);
        style.hasInlineSize = true;
        style.orderInlineSize = ++style.propertyOrder;
    } else if (name == "height") {
        style.height = parseCSSValue(value);
        style.hasHeightVal = true;
        style.orderHeight = ++style.propertyOrder;
    } else if (name == "block-size") {
        style.blockSize = parseCSSValue(value);
        style.hasBlockSize = true;
        style.orderBlockSize = ++style.propertyOrder;
    } else if (name == "min-width") {
        style.minWidth = parseCSSValue(value);
        style.hasMinWidthVal = true;
        style.orderMinWidth = ++style.propertyOrder;
    } else if (name == "min-inline-size") {
        style.minInlineSize = parseCSSValue(value);
        style.hasMinInlineSize = true;
        style.orderMinInlineSize = ++style.propertyOrder;
    } else if (name == "min-height") {
        style.minHeight = parseCSSValue(value);
        style.hasMinHeightVal = true;
        style.orderMinHeight = ++style.propertyOrder;
    } else if (name == "min-block-size") {
        style.minBlockSize = parseCSSValue(value);
        style.hasMinBlockSize = true;
        style.orderMinBlockSize = ++style.propertyOrder;
    } else if (name == "max-width") {
        style.maxWidth = parseCSSValue(value);
        style.hasMaxWidthVal = true;
        style.orderMaxWidth = ++style.propertyOrder;
    } else if (name == "max-inline-size") {
        style.maxInlineSize = parseCSSValue(value);
        style.hasMaxInlineSize = true;
        style.orderMaxInlineSize = ++style.propertyOrder;
    } else if (name == "max-height") {
        style.maxHeight = parseCSSValue(value);
        style.hasMaxHeightVal = true;
        style.orderMaxHeight = ++style.propertyOrder;
    } else if (name == "max-block-size") {
        style.maxBlockSize = parseCSSValue(value);
        style.hasMaxBlockSize = true;
        style.orderMaxBlockSize = ++style.propertyOrder;
    } else {
        return false; // not matched in Part1 � caller proceeds to Part2/Part3
    }
    return true;
}
void StyleSheet::mergePropertyPart2(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name == "font-size") {
        style.fontSize = parseFontSizePixels(value, style.fontSize);
        style.hasFontSize = true;
    } else if (name == "font") {
        std::string lower = lowerAscii(value);
        if (lower.find("-webkit-small-control") != std::string::npos) {
            style.fontSize = 13.333f;
            style.lineHeight = 1.2f;
            style.fontWeight = FontWeight::Normal;
            style.fontStyle = FontStyle::Normal;
            style.hasFontSize = true;
            style.hasLineHeight = true;
            style.hasFontWeight = true;
            style.hasFontStyle = true;
        } else {
            std::string_view tokens[8];
            int count = 0;
            splitWhitespace(value, tokens, 8, count);
            for (int idx = 0; idx < count; idx++) {
                std::string_view part = tokens[idx];
                std::string_view linePart;
                auto slash = part.find('/');
                if (slash != std::string_view::npos) {
                    linePart = part.substr(slash + 1);
                    part = part.substr(0, slash);
                }
                std::string lowerPart = lowerAscii(part);
                if (lowerPart == "bold" || parseFloat(lowerPart) >= 600.0f) {
                    style.fontWeight = FontWeight::Bold;
                    style.hasFontWeight = true;
                } else if (lowerPart == "normal") {
                    style.fontWeight = FontWeight::Normal;
                    style.fontStyle = FontStyle::Normal;
                    style.hasFontWeight = true;
                    style.hasFontStyle = true;
                } else if (lowerPart == "italic") {
                    style.fontStyle = FontStyle::Italic;
                    style.hasFontStyle = true;
                } else if (lowerPart == "oblique") {
                    style.fontStyle = FontStyle::Oblique;
                    style.hasFontStyle = true;
                }
                if (lowerPart.find("px") != std::string::npos ||
                    lowerPart.find("em") != std::string::npos ||
                    lowerPart.find("rem") != std::string::npos ||
                    lowerPart.find('%') != std::string::npos ||
                    lowerPart == "xx-small" || lowerPart == "x-small" ||
                    lowerPart == "small" || lowerPart == "medium" ||
                    lowerPart == "large" || lowerPart == "x-large" ||
                    lowerPart == "xx-large" || lowerPart == "xxx-large" ||
                    lowerPart == "smaller" || lowerPart == "larger") {
                    style.fontSize = parseFontSizePixels(lowerPart, style.fontSize);
                    style.hasFontSize = true;
                }
                if (!linePart.empty()) {
                    style.lineHeight = parseLineHeight(std::string(linePart), style.fontSize);
                    style.hasLineHeight = true;
                }
            }
        }
    } else if (name == "font-weight") {
        style.fontWeight = (value == "bold" || parseFloat(value) >= 600.0f) ?
            FontWeight::Bold : FontWeight::Normal;
        style.hasFontWeight = true;
    } else if (name == "font-style") {
        std::string lower = lowerAscii(value);
        if (lower.find("italic") != std::string::npos) {
            style.fontStyle = FontStyle::Italic;
        } else if (lower.find("oblique") != std::string::npos) {
            style.fontStyle = FontStyle::Oblique;
        } else {
            style.fontStyle = FontStyle::Normal;
        }
        style.hasFontStyle = true;
    } else if (name == "text-align") {
        if (value == "center") style.textAlign = TextAlign::Center;
        else if (value == "right") style.textAlign = TextAlign::Right;
        else if (value == "justify") style.textAlign = TextAlign::Justify;
        else style.textAlign = TextAlign::Left;
        style.hasTextAlign = true;
    } else if (name == "line-height") {
        style.lineHeight = parseLineHeight(value, style.fontSize);
        style.hasLineHeight = true;
    } else if (name == "direction") {
        if (value == "rtl") {
            style.direction = Direction::Rtl;
            style.hasDirection = true;
        } else {
            style.direction = Direction::Ltr;
            style.hasDirection = true;
        }
    } else if (name == "unicode-bidi") {
        if (value == "embed") {
            style.unicodeBidi = UnicodeBidi::Embed;
        } else if (value == "bidi-override") {
            style.unicodeBidi = UnicodeBidi::BidiOverride;
        } else if (value == "isolate") {
            style.unicodeBidi = UnicodeBidi::Isolate;
        } else if (value == "isolate-override") {
            style.unicodeBidi = UnicodeBidi::IsolateOverride;
        } else if (value == "plaintext") {
            style.unicodeBidi = UnicodeBidi::Plaintext;
        } else {
            style.unicodeBidi = UnicodeBidi::Normal;
        }
        style.hasUnicodeBidi = true;
    } else if (name == "writing-mode") {
        if (value == "vertical-rl") {
            style.writingMode = WritingMode::VerticalRl;
            style.hasWritingMode = true;
        } else if (value == "vertical-lr") {
            style.writingMode = WritingMode::VerticalLr;
            style.hasWritingMode = true;
        } else {
            style.writingMode = WritingMode::HorizontalTb;
            style.hasWritingMode = true;
        }
    } else if (name == "opacity") {
        style.opacity = parseFloat(value);
    } else if (name == "contain") {
        uint8_t flags = 0;
        if (value == "none") {
            style.contain = kContainNone;
        } else if (value == "strict") {
            style.contain = kContainStrict;
        } else if (value == "content") {
            style.contain = kContainContent;
        } else {
            std::vector<std::string> tokens;
            size_t pos = 0;
            while (pos < value.size()) {
                while (pos < value.size() && std::isspace((unsigned char)value[pos])) pos++;
                if (pos >= value.size()) break;
                size_t start = pos;
                while (pos < value.size() && !std::isspace((unsigned char)value[pos])) pos++;
                tokens.push_back(value.substr(start, pos - start));
            }
            for (const auto& tok : tokens) {
                if (tok == "size") flags |= (uint8_t)kContainSize;
                else if (tok == "layout") flags |= (uint8_t)kContainLayout;
                else if (tok == "paint") flags |= (uint8_t)kContainPaint;
                else if (tok == "style") flags |= (uint8_t)kContainStyle;
            }
            style.contain = static_cast<ContainmentFlags>(flags);
        }
    } else if (name == "float") {
        if (value == "left") style.cssFloat = CSSFloat::Left;
        else if (value == "right") style.cssFloat = CSSFloat::Right;
        else style.cssFloat = CSSFloat::None;
    } else if (name == "clear") {
        if (value == "left") style.cssClear = CSSClear::Left;
        else if (value == "right") style.cssClear = CSSClear::Right;
        else if (value == "both") style.cssClear = CSSClear::Both;
        else style.cssClear = CSSClear::None;
    } else if (name == "list-style-type") {
        if (value == "none") style.listStyleType = ListStyleType::None;
        else if (value == "disc") style.listStyleType = ListStyleType::Disc;
        else if (value == "circle") style.listStyleType = ListStyleType::Circle;
        else if (value == "square") style.listStyleType = ListStyleType::Square;
        else if (value == "decimal") style.listStyleType = ListStyleType::Decimal;
        else if (value == "decimal-leading-zero") style.listStyleType = ListStyleType::DecimalLeadingZero;
        else if (value == "lower-roman") style.listStyleType = ListStyleType::LowerRoman;
        else if (value == "upper-roman") style.listStyleType = ListStyleType::UpperRoman;
        else if (value == "lower-alpha" || value == "lower-latin") style.listStyleType = ListStyleType::LowerAlpha;
        else if (value == "upper-alpha" || value == "upper-latin") style.listStyleType = ListStyleType::UpperAlpha;
        else style.listStyleType = ListStyleType::Disc;
        style.hasListStyleType = true;
    } else if (name == "list-style") {
        // list-style shorthand: [<list-style-type> || <list-style-position> || <list-style-image>]
        // We resolve the recognized list-style-type keyword from the token list and
        // ignore position/image (image not supported, position handled separately).
        std::string_view lsTokens[6];
        int lsCount = 0;
        splitWhitespace(value, lsTokens, 6, lsCount);
        bool sawType = false;
        for (int i = 0; i < lsCount; ++i) {
            std::string tok = lowerAscii(std::string(lsTokens[i]));
            if (tok == "none") { style.listStyleType = ListStyleType::None; sawType = true; }
            else if (tok == "disc") { style.listStyleType = ListStyleType::Disc; sawType = true; }
            else if (tok == "circle") { style.listStyleType = ListStyleType::Circle; sawType = true; }
            else if (tok == "square") { style.listStyleType = ListStyleType::Square; sawType = true; }
            else if (tok == "decimal") { style.listStyleType = ListStyleType::Decimal; sawType = true; }
            else if (tok == "decimal-leading-zero") { style.listStyleType = ListStyleType::DecimalLeadingZero; sawType = true; }
            else if (tok == "lower-roman") { style.listStyleType = ListStyleType::LowerRoman; sawType = true; }
            else if (tok == "upper-roman") { style.listStyleType = ListStyleType::UpperRoman; sawType = true; }
            else if (tok == "lower-alpha" || tok == "lower-latin") { style.listStyleType = ListStyleType::LowerAlpha; sawType = true; }
            else if (tok == "upper-alpha" || tok == "upper-latin") { style.listStyleType = ListStyleType::UpperAlpha; sawType = true; }
            // "inside"/"outside" (position) and url(...) (image) are accepted but ignored.
        }
        if (sawType) style.hasListStyleType = true;
    } else if (name == "list-style-position" || name == "list-style-image") {
        // Accepted for compatibility; FluxUI renders markers inside the content box.
        // No-op to avoid the declaration being treated as unknown.
    } else if (name == "display") {
        if (value == "flex") style.display = Display::Flex;
        else if (value == "grid") style.display = Display::Grid;
        else if (value == "none") style.display = Display::None;
        else if (value == "inline-block") style.display = Display::InlineBlock;
        else if (value == "inline") style.display = Display::Inline;
        else if (value == "list-item") style.display = Display::ListItem;
        else if (value == "table") style.display = Display::Table;
        else if (value == "table-row-group") style.display = Display::TableRowGroup;
        else if (value == "table-header-group") style.display = Display::TableHeaderGroup;
        else if (value == "table-footer-group") style.display = Display::TableFooterGroup;
        else if (value == "table-row") style.display = Display::TableRow;
        else if (value == "table-cell") style.display = Display::TableCell;
        else if (value == "table-column") style.display = Display::TableColumn;
        else if (value == "table-column-group") style.display = Display::TableColumnGroup;
        else if (value == "table-caption") style.display = Display::TableCaption;
        else if (value == "contents") style.display = Display::Contents;
        else style.display = Display::Block;
    } else if (name == "flex-direction") {
        if (value == "row") style.flexDirection = FlexDirection::Row;
        else if (value == "row-reverse") style.flexDirection = FlexDirection::RowReverse;
        else if (value == "column-reverse") style.flexDirection = FlexDirection::ColumnReverse;
        else style.flexDirection = FlexDirection::Column;
    } else if (name == "justify-content") {
        if (value == "flex-end") style.justifyContent = JustifyContent::FlexEnd;
        else if (value == "center") style.justifyContent = JustifyContent::Center;
        else if (value == "space-between") style.justifyContent = JustifyContent::SpaceBetween;
        else if (value == "space-around") style.justifyContent = JustifyContent::SpaceAround;
        else if (value == "space-evenly") style.justifyContent = JustifyContent::SpaceEvenly;
        else style.justifyContent = JustifyContent::FlexStart;
    } else if (name == "align-items") {
        if (value == "flex-end") style.alignItems = AlignItems::FlexEnd;
        else if (value == "center") style.alignItems = AlignItems::Center;
        else if (value == "flex-start") style.alignItems = AlignItems::FlexStart;
        else style.alignItems = AlignItems::Stretch;
    } else if (name == "place-items") {
        // place-items: <align-items> [<justify-items>]  (CSS Box Alignment L3)
        std::string_view pi[2];
        int piCount = 0;
        splitWhitespace(value, pi, 2, piCount);
        std::string alignTok = piCount > 0 ? lowerAscii(std::string(pi[0])) : "stretch";
        std::string justifyTok = piCount > 1 ? lowerAscii(std::string(pi[1])) : alignTok;
        // align-items axis
        if (alignTok == "flex-end" || alignTok == "end") style.alignItems = AlignItems::FlexEnd;
        else if (alignTok == "center") style.alignItems = AlignItems::Center;
        else if (alignTok == "flex-start" || alignTok == "start") style.alignItems = AlignItems::FlexStart;
        else if (alignTok == "baseline") style.alignItems = AlignItems::Baseline;
        else style.alignItems = AlignItems::Stretch;
        // justify-items axis
        if (justifyTok == "start" || justifyTok == "flex-start") style.justifyItems = JustifyItems::FlexStart;
        else if (justifyTok == "end" || justifyTok == "flex-end") style.justifyItems = JustifyItems::FlexEnd;
        else if (justifyTok == "center") style.justifyItems = JustifyItems::Center;
        else if (justifyTok == "baseline") style.justifyItems = JustifyItems::Baseline;
        else style.justifyItems = JustifyItems::Stretch;
        style.hasJustifyItems = true;
    } else if (name == "place-content") {
        // place-content: <align-content> [<justify-content>]  (CSS Box Alignment L3)
        std::string_view pc[2];
        int pcCount = 0;
        splitWhitespace(value, pc, 2, pcCount);
        std::string alignTok = pcCount > 0 ? lowerAscii(std::string(pc[0])) : "stretch";
        std::string justifyTok = pcCount > 1 ? lowerAscii(std::string(pc[1])) : alignTok;
        // align-content axis
        if (alignTok == "flex-start" || alignTok == "start") style.alignContent = AlignContent::FlexStart;
        else if (alignTok == "flex-end" || alignTok == "end") style.alignContent = AlignContent::FlexEnd;
        else if (alignTok == "center") style.alignContent = AlignContent::Center;
        else if (alignTok == "space-between") style.alignContent = AlignContent::SpaceBetween;
        else if (alignTok == "space-around") style.alignContent = AlignContent::SpaceAround;
        else if (alignTok == "space-evenly") style.alignContent = AlignContent::SpaceEvenly;
        else style.alignContent = AlignContent::Stretch;
        // justify-content axis
        if (justifyTok == "flex-end" || justifyTok == "end") style.justifyContent = JustifyContent::FlexEnd;
        else if (justifyTok == "center") style.justifyContent = JustifyContent::Center;
        else if (justifyTok == "space-between") style.justifyContent = JustifyContent::SpaceBetween;
        else if (justifyTok == "space-around") style.justifyContent = JustifyContent::SpaceAround;
        else if (justifyTok == "space-evenly") style.justifyContent = JustifyContent::SpaceEvenly;
        else style.justifyContent = JustifyContent::FlexStart;
    } else if (name == "place-self") {
        // place-self: <align-self> [<justify-self>]  (CSS Box Alignment L3)
        std::string_view ps[2];
        int psCount = 0;
        splitWhitespace(value, ps, 2, psCount);
        std::string alignTok = psCount > 0 ? lowerAscii(std::string(ps[0])) : "auto";
        std::string justifyTok = psCount > 1 ? lowerAscii(std::string(ps[1])) : alignTok;
        if (alignTok == "flex-start" || alignTok == "start") style.alignSelf = AlignSelf::FlexStart;
        else if (alignTok == "flex-end" || alignTok == "end") style.alignSelf = AlignSelf::FlexEnd;
        else if (alignTok == "center") style.alignSelf = AlignSelf::Center;
        else if (alignTok == "stretch") style.alignSelf = AlignSelf::Stretch;
        else if (alignTok == "baseline") style.alignSelf = AlignSelf::Baseline;
        else style.alignSelf = AlignSelf::Auto;
        if (justifyTok == "start" || justifyTok == "flex-start") style.justifySelf = JustifySelf::FlexStart;
        else if (justifyTok == "end" || justifyTok == "flex-end") style.justifySelf = JustifySelf::FlexEnd;
        else if (justifyTok == "center") style.justifySelf = JustifySelf::Center;
        else if (justifyTok == "stretch") style.justifySelf = JustifySelf::Stretch;
        else if (justifyTok == "baseline") style.justifySelf = JustifySelf::Baseline;
        else style.justifySelf = JustifySelf::Auto;
        style.hasJustifySelf = true;
    } else if (name == "gap") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        float row = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        float column = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : row;
        style.gap = row;
        style.rowGap = row;
        style.columnGap = column;
    } else if (name == "row-gap") {
        style.rowGap = parseLengthPixels(value, emBase);
    } else if (name == "column-gap") {
        style.columnGap = parseLengthPixels(value, emBase);
    } else if (name == "column-count") {
        std::string v = trim(value);
        if (v == "auto") style.columnCount = 0;
        else style.columnCount = (int)parseFloat(v);
    } else if (name == "column-width") {
        std::string v = trim(value);
        if (v == "auto") style.columnWidth = 0.0f;
        else style.columnWidth = parseLengthPixels(v, emBase);
    } else if (name == "flex") {
        std::string v = trim(value);
        if (v == "none") {
            style.flexGrow = 0.0f;
            style.flexShrink = 0.0f;
            style.flexBasis = CSSValue::autoVal();
        } else if (v == "auto") {
            style.flexGrow = 1.0f;
            style.flexShrink = 1.0f;
            style.flexBasis = CSSValue::autoVal();
        } else {
            std::string_view tokens[4];
            int count = 0;
            splitWhitespace(v, tokens, 4, count);
            if (count > 0) style.flexGrow = parseFloat(std::string(tokens[0]));
            style.flexShrink = count > 1 ? parseFloat(std::string(tokens[1])) : 1.0f;
            if (count > 2) style.flexBasis = parseCSSValue(std::string(tokens[2]));
            else if (count == 1) style.flexBasis = CSSValue::pct(0.0f);
        }
    } else if (name == "flex-grow") {
        style.flexGrow = parseFloat(value);
    } else if (name == "flex-shrink") {
        style.flexShrink = parseFloat(value);
    } else if (name == "flex-basis") {
        style.flexBasis = parseCSSValue(value);
    } else if (name == "overflow") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.overflowX = count > 0 ? parseOverflowKeyword(std::string(tokens[0])) : Overflow::Visible;
        style.overflowY = count > 1 ? parseOverflowKeyword(std::string(tokens[1])) : style.overflowX;
        normalizeOverflowAxes(style);
    } else if (name == "overflow-x") {
        style.overflowX = parseOverflowKeyword(value);
        normalizeOverflowAxes(style);
    } else if (name == "overflow-y") {
        style.overflowY = parseOverflowKeyword(value);
        normalizeOverflowAxes(style);
    } else if (name == "box-shadow") {
        style.boxShadow = parseBoxShadow(value, emBase);
    } else if (name == "cursor") {
        if (value == "pointer") style.cursor = CursorType::Pointer;
        else if (value == "text") style.cursor = CursorType::Text;
        else if (value == "grab") style.cursor = CursorType::Grab;
        else if (value == "grabbing") style.cursor = CursorType::Grabbing;
        else if (value == "not-allowed") style.cursor = CursorType::NotAllowed;
        else if (value == "crosshair") style.cursor = CursorType::Crosshair;
        else if (value == "se-resize" || value == "nwse-resize") style.cursor = CursorType::ResizeNWSE;
        else style.cursor = CursorType::Default;
    } else if (name == "transition") {
        auto parts = splitTopLevel(value, ',');
        std::vector<std::string> props;
        std::vector<float> durations;
        std::vector<float> delays;
        std::vector<TimingFunction> tfs;
        std::vector<TransitionBehavior> behaviors;
        for (const auto& part : parts) {
            std::string p = trim(part);
            if (p.empty()) continue;
            std::string property = "all";
            float dur = 0.0f;
            float del = 0.0f;
            TimingFunction tf = TimingFunction::ease();
            TransitionBehavior behavior = TransitionBehavior::Normal;
            int timeSlots = 0;
            auto toks = splitWhitespaceTopLevel(p);
            for (const auto& tok : toks) {
                if (tok.empty()) continue;
                std::string tl = tok;
                for (char& c : tl) c = (char)std::tolower((unsigned char)c);
                bool isMs = tl.size() > 2 && tl.substr(tl.size() - 2) == "ms";
                bool isSec = tl.size() > 1 && tl.back() == 's' && !isMs;
                if (isMs || isSec) {
                    float t = parseDuration(tok);
                    if (timeSlots == 0) { dur = t; timeSlots++; }
                    else                { del = t; timeSlots++; }
                    continue;
                }
                if (tl == "linear" || tl == "ease" || tl == "ease-in" || tl == "ease-out" ||
                    tl == "ease-in-out" || tl == "step-start" || tl == "step-end" ||
                    tl.rfind("cubic-bezier(", 0) == 0 || tl.rfind("steps(", 0) == 0) {
                    tf = parseTimingFunction(tok);
                    continue;
                }
                if (tl == "allow-discrete") {
                    behavior = TransitionBehavior::AllowDiscrete;
                    continue;
                }
                property = tl;
            }
            props.push_back(property);
            durations.push_back(dur);
            delays.push_back(del);
            tfs.push_back(tf);
            behaviors.push_back(behavior);
        }
        style.transitionProperty         = std::move(props);
        style.transitionDurations        = std::move(durations);
        style.transitionDelays           = std::move(delays);
        style.transitionTimingFunctions  = std::move(tfs);
        style.transitionBehavior         = std::move(behaviors);
        if (!style.transitionDurations.empty()) {
            style.transitionDuration = style.transitionDurations[0];
        }
    } else if (name == "transition-property") {
        style.transitionProperty = parseTransitionPropertyList(value);
    } else if (name == "transition-duration") {
        style.transitionDurations = parseDurationList(value);
        if (!style.transitionDurations.empty()) {
            style.transitionDuration = style.transitionDurations[0];
        }
    } else if (name == "transition-delay") {
        style.transitionDelays = parseDurationList(value);
    } else if (name == "transition-timing-function") {
        style.transitionTimingFunctions = parseTimingFunctionList(value);
    } else if (name == "transition-behavior") {
        style.transitionBehavior = parseTransitionBehaviorList(value);
    } else if (name == "animation") {
        auto parts = splitTopLevel(value, ',');
        std::vector<std::string> names;
        std::vector<float> durations, delays, iterationCounts;
        std::vector<AnimationDirection> directions;
        std::vector<AnimationFillMode> fillModes;
        std::vector<AnimationPlayState> playStates;
        std::vector<TimingFunction> tfs;
        std::vector<AnimationComposition> compositions;
        for (const auto& part : parts) {
            std::string p = trim(part);
            if (p.empty()) continue;
            std::string name = "none";
            float dur = 0.0f;
            float del = 0.0f;
            float iter = 1.0f;
            AnimationDirection dir = AnimationDirection::Normal;
            AnimationFillMode fill = AnimationFillMode::None;
            AnimationPlayState play = AnimationPlayState::Running;
            TimingFunction tf = TimingFunction::ease();
            AnimationComposition comp = AnimationComposition::Replace;
            int timeSlots = 0;
            bool nameSet = false;
            auto toks = splitWhitespaceTopLevel(p);
            for (const auto& tok : toks) {
                if (tok.empty()) continue;
                std::string tl = tok;
                for (char& c : tl) c = (char)std::tolower((unsigned char)c);
                bool isMs = tl.size() > 2 && tl.substr(tl.size() - 2) == "ms";
                bool isSec = tl.size() > 1 && tl.back() == 's' && !isMs;
                if (isMs || isSec) {
                    float t = parseDuration(tok);
                    if (timeSlots == 0) { dur = t; timeSlots++; }
                    else                { del = t; timeSlots++; }
                    continue;
                }
                if (tl == "infinite") { iter = -1.0f; continue; }
                if (tl == "normal")             { dir   = AnimationDirection::Normal;            continue; }
                if (tl == "reverse")            { dir   = AnimationDirection::Reverse;           continue; }
                if (tl == "alternate")          { dir   = AnimationDirection::Alternate;         continue; }
                if (tl == "alternate-reverse")  { dir   = AnimationDirection::AlternateReverse;  continue; }
                if (tl == "none")      { fill = AnimationFillMode::None;      if (!nameSet) { name = "none"; nameSet = true; } continue; }
                if (tl == "forwards")  { fill = AnimationFillMode::Forwards;  continue; }
                if (tl == "backwards") { fill = AnimationFillMode::Backwards; continue; }
                if (tl == "both")      { fill = AnimationFillMode::Both;      continue; }
                if (tl == "running")  { play = AnimationPlayState::Running;  continue; }
                if (tl == "paused")   { play = AnimationPlayState::Paused;   continue; }
                if (tl == "add")        { comp = AnimationComposition::Add;        continue; }
                if (tl == "accumulate") { comp = AnimationComposition::Accumulate; continue; }
                if (tl == "linear" || tl == "ease" || tl == "ease-in" || tl == "ease-out" ||
                    tl == "ease-in-out" || tl == "step-start" || tl == "step-end" ||
                    tl.rfind("cubic-bezier(", 0) == 0 || tl.rfind("steps(", 0) == 0) {
                    tf = parseTimingFunction(tok);
                    continue;
                }
                bool isNumber = true;
                bool sawDigit = false;
                size_t idx = 0;
                if (idx < tl.size() && (tl[idx] == '+' || tl[idx] == '-')) idx++;
                for (; idx < tl.size(); ++idx) {
                    if (tl[idx] >= '0' && tl[idx] <= '9') sawDigit = true;
                    else if (tl[idx] == '.') {  }
                    else { isNumber = false; break; }
                }
                if (isNumber && sawDigit) {
                    iter = parseFloat(tok);
                    continue;
                }
                if (!nameSet) {
                    name = tok;
                    nameSet = true;
                }
            }
            names.push_back(name);
            durations.push_back(dur);
            delays.push_back(del);
            iterationCounts.push_back(iter);
            directions.push_back(dir);
            fillModes.push_back(fill);
            playStates.push_back(play);
            tfs.push_back(tf);
            compositions.push_back(comp);
        }
        style.animationName           = std::move(names);
        style.animationDuration       = std::move(durations);
        style.animationDelay          = std::move(delays);
        style.animationIterationCount = std::move(iterationCounts);
        style.animationDirection      = std::move(directions);
        style.animationFillMode       = std::move(fillModes);
        style.animationPlayState      = std::move(playStates);
        style.animationTimingFunction = std::move(tfs);
        style.animationComposition    = std::move(compositions);
    } else if (name == "animation-name") {
        style.animationName = parseAnimationNameList(value);
    } else if (name == "animation-duration") {
        style.animationDuration = parseDurationList(value);
    } else if (name == "animation-delay") {
        style.animationDelay = parseDurationList(value);
    } else if (name == "animation-iteration-count") {
        style.animationIterationCount.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string s = trim(part);
            for (char& c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "infinite") style.animationIterationCount.push_back(-1.0f);
            else                 style.animationIterationCount.push_back(parseFloat(s));
        }
    } else if (name == "animation-direction") {
        style.animationDirection = parseAnimationDirectionList(value);
    } else if (name == "animation-fill-mode") {
        style.animationFillMode = parseAnimationFillModeList(value);
    } else if (name == "animation-play-state") {
        style.animationPlayState = parseAnimationPlayStateList(value);
    } else if (name == "animation-timing-function") {
        style.animationTimingFunction = parseTimingFunctionList(value);
    } else if (name == "animation-composition") {
        style.animationComposition = parseAnimationCompositionList(value);
    } else if (name == "position") {
        if (value == "relative") style.position = Position::Relative;
        else if (value == "absolute") style.position = Position::Absolute;
        else if (value == "fixed") style.position = Position::Fixed;
        else if (value == "sticky") style.position = Position::Sticky;
        else style.position = Position::Static;
    } else if (name == "top") {
        style.topVal = parseCSSValue(value);
        style.hasTopVal = true;
        style.orderTop = ++style.propertyOrder;
    } else if (name == "right") {
        style.rightVal = parseCSSValue(value);
        style.hasRightVal = true;
        style.orderRight = ++style.propertyOrder;
    } else if (name == "bottom") {
        style.bottomVal = parseCSSValue(value);
        style.hasBottomVal = true;
        style.orderBottom = ++style.propertyOrder;
    } else if (name == "left") {
        style.leftVal = parseCSSValue(value);
        style.hasLeftVal = true;
        style.orderLeft = ++style.propertyOrder;
    } else if (name == "grid-template-columns") {
        style.gridTemplateColumns = value;
        if (value != "none" && !value.empty())
            style.gridTemplateColumnTracks = parseGridTrackList(value, emBase);
        else
            style.gridTemplateColumnTracks.clear();
    } else if (name == "grid-template-rows") {
        style.gridTemplateRows = value;
        if (value != "none" && !value.empty())
            style.gridTemplateRowTracks = parseGridTrackList(value, emBase);
        else
            style.gridTemplateRowTracks.clear();
    } else if (name == "grid-template-areas") {
        style.gridTemplateAreas = parseGridTemplateAreas(value);
        style.hasGridTemplateAreas = (style.gridTemplateAreas.rowCount > 0);
    } else if (name == "grid-template") {
        // grid-template: <rows> / <columns>  (no areas shorthand for now)
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            std::string rows = trim(value.substr(0, slashPos));
            std::string cols = trim(value.substr(slashPos + 1));
            style.gridTemplateRows = rows;
            style.gridTemplateColumns = cols;
            style.gridTemplateRowTracks    = parseGridTrackList(rows, emBase);
            style.gridTemplateColumnTracks = parseGridTrackList(cols, emBase);
        }
    } else if (name == "grid-auto-rows") {
        style.gridAutoRowTracks = parseGridTrackList(value, emBase);
    } else if (name == "grid-auto-columns") {
        style.gridAutoColumnTracks = parseGridTrackList(value, emBase);
    } else if (name == "grid-auto-flow") {
        std::string v = lowerAscii(trim(value));
        if (v == "column")             style.gridAutoFlow = GridAutoFlow::Column;
        else if (v == "row dense")     style.gridAutoFlow = GridAutoFlow::RowDense;
        else if (v == "column dense")  style.gridAutoFlow = GridAutoFlow::ColumnDense;
        else if (v == "dense")         style.gridAutoFlow = GridAutoFlow::RowDense;
        else                           style.gridAutoFlow = GridAutoFlow::Row;
    } else if (name == "grid") {
        // grid shorthand: <template> | <auto-flow> rows / cols � parse basic / form
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            std::string rows = trim(value.substr(0, slashPos));
            std::string cols = trim(value.substr(slashPos + 1));
            style.gridTemplateRows = rows;
            style.gridTemplateColumns = cols;
            style.gridTemplateRowTracks    = parseGridTrackList(rows, emBase);
            style.gridTemplateColumnTracks = parseGridTrackList(cols, emBase);
        }
    } else if (name == "grid-column") {
        style.gridColumn = value;
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            style.gridColumnStart = parseGridPlacement(trim(value.substr(0, slashPos)));
            style.gridColumnEnd   = parseGridPlacement(trim(value.substr(slashPos + 1)));
        } else {
            style.gridColumnStart = parseGridPlacement(trim(value));
            style.gridColumnEnd   = GridPlacement{};
        }
    } else if (name == "grid-row") {
        style.gridRow = value;
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            style.gridRowStart = parseGridPlacement(trim(value.substr(0, slashPos)));
            style.gridRowEnd   = parseGridPlacement(trim(value.substr(slashPos + 1)));
        } else {
            style.gridRowStart = parseGridPlacement(trim(value));
            style.gridRowEnd   = GridPlacement{};
        }
    } else if (name == "grid-column-start") {
        style.gridColumnStart = parseGridPlacement(value);
    } else if (name == "grid-column-end") {
        style.gridColumnEnd   = parseGridPlacement(value);
    } else if (name == "grid-row-start") {
        style.gridRowStart    = parseGridPlacement(value);
    } else if (name == "grid-row-end") {
        style.gridRowEnd      = parseGridPlacement(value);
    } else if (name == "grid-area") {
        style.gridArea = value;
        // grid-area: row-start / col-start / row-end / col-end
        // or just a named area reference
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i <= value.size(); i++) {
            if (i == value.size() || value[i] == '/') {
                parts.push_back(trim(value.substr(start, i - start)));
                start = i + 1;
            }
        }
        if (parts.size() == 4) {
            style.gridRowStart    = parseGridPlacement(parts[0]);
            style.gridColumnStart = parseGridPlacement(parts[1]);
            style.gridRowEnd      = parseGridPlacement(parts[2]);
            style.gridColumnEnd   = parseGridPlacement(parts[3]);
        } else if (parts.size() == 1 && !parts[0].empty()) {
            // Named area � stored as named-line references
            style.gridRowStart.type = GridPlacement::PlacementType::NamedLine;
            style.gridRowStart.name = parts[0];
            style.gridColumnStart.type = GridPlacement::PlacementType::NamedLine;
            style.gridColumnStart.name = parts[0];
        }
    } else if (name == "justify-items") {
        std::string v = lowerAscii(trim(value));
        if (v == "start" || v == "flex-start") style.justifyItems = JustifyItems::FlexStart;
        else if (v == "end" || v == "flex-end") style.justifyItems = JustifyItems::FlexEnd;
        else if (v == "center")   style.justifyItems = JustifyItems::Center;
        else if (v == "stretch")  style.justifyItems = JustifyItems::Stretch;
        else if (v == "baseline") style.justifyItems = JustifyItems::Baseline;
        else                      style.justifyItems = JustifyItems::Normal;
        style.hasJustifyItems = true;
    } else if (name == "justify-self") {
        std::string v = lowerAscii(trim(value));
        if (v == "start" || v == "flex-start") style.justifySelf = JustifySelf::FlexStart;
        else if (v == "end" || v == "flex-end") style.justifySelf = JustifySelf::FlexEnd;
        else if (v == "center")   style.justifySelf = JustifySelf::Center;
        else if (v == "stretch")  style.justifySelf = JustifySelf::Stretch;
        else if (v == "baseline") style.justifySelf = JustifySelf::Baseline;
        else                      style.justifySelf = JustifySelf::Auto;
        style.hasJustifySelf = true;
    } else if (name == "content") {
        std::string raw = value;
        if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\''))) {
            raw = raw.substr(1, raw.size() - 2);
        }
        style.content = raw;
    } else if (name == "font-family") {
        style.fontFamily = value;
        style.hasFontFamily = true;
    } else if (name == "scale") {
        style.scale = parseFloat(value);
    } else if (name == "transform") {
        style.transform = parseTransformOperations(value);
        style.hasTransform = true;
        for (const auto& op : style.transform) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.scale = val;
                }
            }
        }
    } else if (name == "transform-origin") {
        style.transformOrigin = parseTransformOrigin(value);
        style.hasTransformOrigin = true;
    } else if (name == "transform-style") {
        style.transformStyle = parseTransformStyle(value);
        style.hasTransformStyle = true;
    } else if (name == "transform-box") {
        style.transformBox = parseTransformBox(value);
        style.hasTransformBox = true;
    } else if (name == "perspective") {
        style.perspective = parsePerspective(value);
        style.hasPerspective = true;
    } else if (name == "perspective-origin") {
        style.perspectiveOrigin = parsePerspectiveOrigin(value);
        style.hasPerspectiveOrigin = true;
    } else if (name == "backface-visibility") {
        style.backfaceVisibility = parseBackfaceVisibility(value);
        style.hasBackfaceVisibility = true;
    }
    else if (name == "box-sizing") {
        if (value == "border-box") style.boxSizing = BoxSizing::BorderBox;
        else style.boxSizing = BoxSizing::ContentBox;
        style.hasBoxSizing = true;
    } else if (name == "visibility") {
        if (value == "hidden") style.visibility = Visibility::Hidden;
        else if (value == "collapse") style.visibility = Visibility::Collapse;
        else style.visibility = Visibility::Visible;
    } else if (name == "text-overflow") {
        if (value == "ellipsis") style.textOverflow = TextOverflow::Ellipsis;
        else style.textOverflow = TextOverflow::Clip;
        style.hasTextOverflow = true;
    } else if (name == "white-space") {
        if (value == "nowrap") style.whiteSpace = WhiteSpace::NoWrap;
        else if (value == "pre") style.whiteSpace = WhiteSpace::Pre;
        else if (value == "pre-wrap") style.whiteSpace = WhiteSpace::PreWrap;
        else if (value == "pre-line") style.whiteSpace = WhiteSpace::PreLine;
        else style.whiteSpace = WhiteSpace::Normal;
        style.hasWhiteSpace = true;
    } else if (name == "text-decoration" || name == "text-decoration-line") {
        if (value == "underline") style.textDecoration = TextDecoration::Underline;
        else if (value == "line-through") style.textDecoration = TextDecoration::LineThrough;
        else if (value == "overline") style.textDecoration = TextDecoration::Overline;
        else style.textDecoration = TextDecoration::None;
        style.hasTextDecoration = true;
    } else if (name == "text-decoration-color") {
        style.textDecorationColor = parseColor(value);
        style.hasTextDecorationColor = true;
    } else if (name == "text-transform") {
        if (value == "uppercase") style.textTransform = TextTransform::Uppercase;
        else if (value == "lowercase") style.textTransform = TextTransform::Lowercase;
        else if (value == "capitalize") style.textTransform = TextTransform::Capitalize;
        else style.textTransform = TextTransform::None;
        style.hasTextTransform = true;
    } else if (name == "letter-spacing") {
        style.letterSpacing = parseLengthPixels(value, emBase);
        style.hasLetterSpacing = true;
    } else if (name == "word-spacing") {
        style.wordSpacing = parseLengthPixels(value, emBase);
        style.hasWordSpacing = true;
    } else if (name == "pointer-events") {
        if (value == "none") style.pointerEvents = PointerEvents::None;
        else style.pointerEvents = PointerEvents::Auto;
    } else if (name == "flex-wrap") {
        if (value == "wrap") style.flexWrap = FlexWrap::Wrap;
        else if (value == "wrap-reverse") style.flexWrap = FlexWrap::WrapReverse;
        else style.flexWrap = FlexWrap::NoWrap;
    } else if (name == "align-self") {
        if (value == "flex-start") style.alignSelf = AlignSelf::FlexStart;
        else if (value == "flex-end") style.alignSelf = AlignSelf::FlexEnd;
        else if (value == "center") style.alignSelf = AlignSelf::Center;
        else if (value == "stretch") style.alignSelf = AlignSelf::Stretch;
        else if (value == "baseline") style.alignSelf = AlignSelf::Baseline;
        else style.alignSelf = AlignSelf::Auto;
    } else if (name == "align-content") {
        if (value == "flex-start") style.alignContent = AlignContent::FlexStart;
        else if (value == "flex-end") style.alignContent = AlignContent::FlexEnd;
        else if (value == "center") style.alignContent = AlignContent::Center;
        else if (value == "space-between") style.alignContent = AlignContent::SpaceBetween;
        else if (value == "space-around") style.alignContent = AlignContent::SpaceAround;
        else if (value == "space-evenly") style.alignContent = AlignContent::SpaceEvenly;
        else style.alignContent = AlignContent::Stretch;
    } else if (name == "order") {
        style.order = (int)parseFloat(value);
    } else if (name == "z-index") {
        style.zIndex = (int)parseFloat(value);
        style.hasZIndex = true;
    } else if (name == "aspect-ratio") {
        if (value == "auto") {
            style.aspectRatio = 0;
        } else {
            auto slashPos = value.find('/');
            if (slashPos != std::string::npos) {
                float w = parseFloat(trim(value.substr(0, slashPos)));
                float h = parseFloat(trim(value.substr(slashPos + 1)));
                style.aspectRatio = h > 0 ? w / h : 0;
            } else {
                style.aspectRatio = parseFloat(value);
            }
        }
    } else if (name == "object-fit") {
        std::string lower = value;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower == "contain") style.objectFit = ObjectFit::Contain;
        else if (lower == "cover") style.objectFit = ObjectFit::Cover;
        else if (lower == "none") style.objectFit = ObjectFit::None;
        else if (lower == "scale-down") style.objectFit = ObjectFit::ScaleDown;
        else style.objectFit = ObjectFit::Fill;
        style.hasObjectFit = true;
    } else if (name == "appearance" || name == "-webkit-appearance") {
        std::string lower = lowerAscii(value);
        if (lower == "none") style.appearance = Appearance::None;
        else if (lower == "textfield" || lower == "text-field") style.appearance = Appearance::TextField;
        else if (lower == "searchfield" || lower == "search-field") style.appearance = Appearance::SearchField;
        else if (lower == "push-button") style.appearance = Appearance::PushButton;
        else if (lower == "button") style.appearance = Appearance::Button;
        else if (lower == "checkbox") style.appearance = Appearance::Checkbox;
        else if (lower == "radio") style.appearance = Appearance::Radio;
        else if (lower == "menulist" || lower == "menulist-button") style.appearance = Appearance::Menulist;
        else if (lower == "textarea") style.appearance = Appearance::Textarea;
        else if (lower == "slider-horizontal") style.appearance = Appearance::SliderHorizontal;
        else if (lower == "square-button") style.appearance = Appearance::SquareButton;
        else style.appearance = Appearance::Auto;
        style.hasAppearance = true;
    } else if (name == "object-position") {
        Vec2 position;
        Vec2 offset;
        if (parseObjectPosition(value, position, offset)) {
            style.objectPosition = position;
            style.objectPositionOffset = offset;
            style.hasObjectPosition = true;
        }
    } else if (name == "row-gap") {
        style.rowGap = parseLengthPixels(value, emBase);
    } else if (name == "column-gap") {
        style.columnGap = parseLengthPixels(value, emBase);
    } else if (name == "column-count") {
        std::string v = trim(value);
        if (v == "auto") style.columnCount = 0;
        else style.columnCount = (int)parseFloat(v);
    } else if (name == "column-width") {
        std::string v = trim(value);
        if (v == "auto") style.columnWidth = 0.0f;
        else style.columnWidth = parseLengthPixels(v, emBase);
    } else if (name == "word-break") {
        if (value == "break-all") style.wordBreak = WordBreak::BreakAll;
        else if (value == "keep-all") style.wordBreak = WordBreak::KeepAll;
        else if (value == "break-word") style.wordBreak = WordBreak::BreakWord;
        else style.wordBreak = WordBreak::Normal;
        style.hasWordBreak = true;
    } else if (name == "vertical-align") {
        std::string lower = lowerAscii(value);
        if (lower == "sub") style.verticalAlign = VerticalAlign::Sub;
        else if (lower == "super") style.verticalAlign = VerticalAlign::Super;
        else if (lower == "middle") style.verticalAlign = VerticalAlign::Middle;
        else if (lower == "top") style.verticalAlign = VerticalAlign::Top;
        else if (lower == "bottom") style.verticalAlign = VerticalAlign::Bottom;
        else if (lower == "text-top") style.verticalAlign = VerticalAlign::TextTop;
        else if (lower == "text-bottom") style.verticalAlign = VerticalAlign::TextBottom;
        else style.verticalAlign = VerticalAlign::Baseline;
        style.hasVerticalAlign = true;
    }
    else if (name == "hover-background-color" || name == "--hover-bg") {
        style.hoverBackgroundColor = parseColor(value);
        style.hasHoverBg = true;
    } else if (name == "hover-color" || name == "--hover-color") {
        style.hoverColor = parseColor(value);
        style.hasHoverColor = true;
    } else if (name == "hover-border-color" || name == "--hover-border") {
        style.hoverBorderColor = parseColor(value);
        style.hasHoverBorder = true;
    } else if (name == "hover-opacity" || name == "--hover-opacity") {
        style.hoverOpacity = parseFloat(value);
    } else if (name == "hover-scale" || name == "--hover-scale") {
        style.hoverScale = parseFloat(value);
    }
}
// -- mergePropertyPart3: scroll-driven animations + timeline properties --
void StyleSheet::mergePropertyPart3(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name == "animation-timeline") {
        style.animationTimeline.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.animationTimeline.push_back(trim(part));
        }
        style.hasAnimationTimeline = !style.animationTimeline.empty();
    } else if (name == "animation-range" || name == "animation-range-start") {
        if (name == "animation-range") {
            auto parts = splitTopLevel(value, ' ');
            style.animationRangeStart = trim(parts.size() > 0 ? parts[0] : value);
            style.animationRangeEnd   = parts.size() > 1 ? trim(parts[1]) : "normal";
        } else {
            style.animationRangeStart = trim(value);
        }
    } else if (name == "animation-range-end") {
        style.animationRangeEnd = trim(value);
    } else if (name == "scroll-timeline") {
        style.scrollTimelineName.clear();
        style.scrollTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string s = trim(part);
            std::istringstream iss(s);
            std::string n, a;
            iss >> n; iss >> a;
            style.scrollTimelineName.push_back(n);
            style.scrollTimelineAxis.push_back(a.empty() ? "block" : lowerAscii(a));
        }
        style.hasScrollTimeline = !style.scrollTimelineName.empty();
    } else if (name == "scroll-timeline-name") {
        style.scrollTimelineName.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.scrollTimelineName.push_back(trim(part));
        }
        style.hasScrollTimeline = !style.scrollTimelineName.empty();
    } else if (name == "scroll-timeline-axis") {
        style.scrollTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.scrollTimelineAxis.push_back(lowerAscii(trim(part)));
        }
    } else if (name == "view-timeline") {
        style.viewTimelineName.clear();
        style.viewTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string s = trim(part);
            std::istringstream iss(s);
            std::string n, a;
            iss >> n; iss >> a;
            style.viewTimelineName.push_back(n);
            style.viewTimelineAxis.push_back(a.empty() ? "block" : lowerAscii(a));
        }
        style.hasViewTimeline = !style.viewTimelineName.empty();
    } else if (name == "view-timeline-name") {
        style.viewTimelineName.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.viewTimelineName.push_back(trim(part));
        }
        style.hasViewTimeline = !style.viewTimelineName.empty();
    } else if (name == "view-timeline-axis") {
        style.viewTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.viewTimelineAxis.push_back(lowerAscii(trim(part)));
        }
    } else if (name == "view-timeline-inset") {
        style.viewTimelineInset = trim(value);
    } else if (name == "timeline-scope") {
        style.timelineScope.clear();
        if (lowerAscii(trim(value)) != "none") {
            for (const auto& part : splitTopLevel(value, ',')) {
                style.timelineScope.push_back(trim(part));
            }
        }
        style.hasTimelineScope = !style.timelineScope.empty();
    } else if (name == "font-variant-caps") {
        style.fontVariantCaps = lowerAscii(trim(value));
        style.hasFontVariantCaps = (style.fontVariantCaps != "normal");
    } else if (name == "font-variant-numeric") {
        style.fontVariantNumeric = lowerAscii(trim(value));
        style.hasFontVariantNumeric = (style.fontVariantNumeric != "normal");
    } else if (name == "font-variant-ligatures") {
        style.fontVariantLigatures = lowerAscii(trim(value));
        style.hasFontVariantLigatures = (style.fontVariantLigatures != "normal");
    } else if (name == "font-variant-east-asian") {
        style.fontVariantEastAsian = lowerAscii(trim(value));
        style.hasFontVariantEastAsian = (style.fontVariantEastAsian != "normal");
    } else if (name == "font-variant-position") {
        style.fontVariantPosition = lowerAscii(trim(value));
    } else if (name == "font-variant-alternates") {
        style.fontVariantAlternates = trim(value);
    } else if (name == "font-variant") {
        // font-variant shorthand ? distribute to sub-properties
        std::string v = lowerAscii(trim(value));
        if (v == "normal" || v == "none") {
            style.fontVariantCaps = "normal";
            style.fontVariantNumeric = "normal";
            style.fontVariantLigatures = v;
            style.fontVariantEastAsian = "normal";
        } else if (v == "small-caps" || v == "all-small-caps" || v == "petite-caps" ||
                   v == "all-petite-caps" || v == "unicase" || v == "titling-caps") {
            style.fontVariantCaps = v;
            style.hasFontVariantCaps = true;
        } else {
            style.fontVariantCaps = v; // best-effort
        }
    } else if (name == "font-feature-settings") {
        style.fontFeatureSettings = trim(value);
        style.hasFontFeatureSettings = (lowerAscii(style.fontFeatureSettings) != "normal");
    } else if (name == "font-variation-settings") {
        style.fontVariationSettings = trim(value);
        style.hasFontVariationSettings = (lowerAscii(style.fontVariationSettings) != "normal");
    } else if (name == "font-optical-sizing") {
        style.fontOpticalSizing = lowerAscii(trim(value));
        style.hasFontOpticalSizing = (style.fontOpticalSizing != "auto");
    } else if (name == "font-palette") {
        style.fontPalette = trim(value);
    } else if (name == "font-stretch") {
        style.fontStretch = lowerAscii(trim(value));
        style.hasFontStretch = (style.fontStretch != "normal");
    } else if (name == "font-synthesis") {
        style.fontSynthesis = lowerAscii(trim(value));
    } else if (name == "font-language-override") {
        style.fontLanguageOverride = trim(value);
    } else if (name == "tab-size") {
        style.tabSize = parseLengthPixels(value, emBase);
        if (style.tabSize == 0.0f) {
            // Bare integer (number of spaces)
            try { style.tabSize = std::stof(trim(value)); } catch (...) {}
        }
        style.hasTabSize = true;
    } else if (name == "hyphens") {
        style.hyphens = lowerAscii(trim(value));
        style.hasHyphens = (style.hyphens != "manual");
    } else if (name == "line-break") {
        style.lineBreak = lowerAscii(trim(value));
        style.hasLineBreak = (style.lineBreak != "auto");
    } else if (name == "overflow-wrap" || name == "word-wrap") {
        style.overflowWrap = lowerAscii(trim(value));
        style.hasOverflowWrap = (style.overflowWrap != "normal");
    } else if (name == "text-justify") {
        style.textJustify = lowerAscii(trim(value));
        style.hasTextJustify = (style.textJustify != "auto");
    } else if (name == "text-indent") {
        style.textIndent = parseLengthPixels(value, emBase);
        style.hasTextIndent = true;
    } else if (name == "hanging-punctuation") {
        style.hangingPunctuation = lowerAscii(trim(value));
        style.hasHangingPunctuation = (style.hangingPunctuation != "none");
    } else if (name == "accent-color") {
        std::string v = lowerAscii(trim(value));
        if (v == "auto") { style.hasAccentColor = false; }
        else { style.accentColor = parseColor(value); style.hasAccentColor = true; }
    } else if (name == "caret-color") {
        std::string v = lowerAscii(trim(value));
        if (v == "auto") { style.hasCaretColor = false; }
        else { style.caretColor = parseColor(value); style.hasCaretColor = true; }
    } else if (name == "color-scheme") {
        style.colorScheme = lowerAscii(trim(value));
        style.hasColorScheme = (style.colorScheme != "normal" && !style.colorScheme.empty());
    } else if (name == "inert") {
        style.inert = (lowerAscii(trim(value)) != "false" && !value.empty());
    } else if (name == "field-sizing") {
        style.fieldSizing = lowerAscii(trim(value));
        style.hasFieldSizing = !style.fieldSizing.empty();
    } else if (name == "image-rendering") {
        style.imageRendering = lowerAscii(trim(value));
        style.hasImageRendering = (style.imageRendering != "auto");
    } else if (name == "image-orientation") {
        style.imageOrientation = lowerAscii(trim(value));
        style.hasImageOrientation = (style.imageOrientation != "from-image");
    } else if (name == "object-view-box") {
        style.objectViewBox = trim(value);
        style.hasObjectViewBox = (lowerAscii(style.objectViewBox) != "none" && !style.objectViewBox.empty());
    } else if (name == "touch-action") {
        style.touchAction = lowerAscii(trim(value));
        style.hasTouchAction = (style.touchAction != "auto");
    } else if (name == "user-select") {
        style.userSelect = lowerAscii(trim(value));
        style.hasUserSelect = (style.userSelect != "auto");
    } else if (name == "will-change") {
        style.willChange = trim(value);
        style.hasWillChange = (lowerAscii(style.willChange) != "auto" && !style.willChange.empty());
    } else if (name == "contain-intrinsic-size" || name == "contain-intrinsic-width" || name == "contain-intrinsic-height") {
        style.containIntrinsicSize = trim(value);
        style.hasContainIntrinsicSize = (lowerAscii(style.containIntrinsicSize) != "none");
    } else if (name == "content-visibility") {
        style.contentVisibility = lowerAscii(trim(value));
        style.hasContentVisibility = (style.contentVisibility != "visible");
    }
}
void StyleSheet::mergeHoverProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.hoverBackgroundGradient = parseGradient(value);
            style.hasHoverGradient = true;
        } else {
            style.hoverBackgroundColor = parseColor(value);
            style.hasHoverBg = true;
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.hoverBackgroundGradient = parseGradient(value);
            style.hasHoverGradient = true;
        }
    } else if (name == "color") {
        style.hoverColor = parseColor(value);
        style.hasHoverColor = true;
    } else if (name == "border-color") {
        style.hoverBorderColor = parseColor(value);
        style.hasHoverBorder = true;
    } else if (name == "border") {
        std::istringstream ss(value);
        std::string widthStr, typeStr, colorStr;
        ss >> widthStr >> typeStr;
        std::getline(ss, colorStr);
        colorStr = trim(colorStr);
        if (!colorStr.empty()) {
            style.hoverBorderColor = parseColor(colorStr);
            style.hasHoverBorder = true;
        }
    } else if (name == "opacity") {
        style.hoverOpacity = parseFloat(value);
    } else if (name == "scale") {
        style.hoverScale = parseFloat(value);
    } else if (name == "transform") {
        auto ops = parseTransformOperations(value);
        for (const auto& op : ops) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.hoverScale = val;
                }
            }
        }
    } else if (name == "hover-background-color" || name == "--hover-bg") {
        style.hoverBackgroundColor = parseColor(value);
        style.hasHoverBg = true;
    } else if (name == "hover-color" || name == "--hover-color") {
        style.hoverColor = parseColor(value);
        style.hasHoverColor = true;
    } else if (name == "hover-border-color" || name == "--hover-border") {
        style.hoverBorderColor = parseColor(value);
        style.hasHoverBorder = true;
    } else if (name == "hover-opacity" || name == "--hover-opacity") {
        style.hoverOpacity = parseFloat(value);
    } else if (name == "hover-scale" || name == "--hover-scale") {
        style.hoverScale = parseFloat(value);
    }
}
void StyleSheet::mergeFocusProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.focusBackgroundGradient = parseGradient(value);
            style.hasFocusGradient = true;
        } else {
            style.focusBackgroundColor = parseColor(value);
            style.hasFocusBg = true;
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.focusBackgroundGradient = parseGradient(value);
            style.hasFocusGradient = true;
        }
    } else if (name == "color") {
        style.focusColor = parseColor(value);
        style.hasFocusColor = true;
    } else if (name == "border-color") {
        style.focusBorderColor = parseColor(value);
        style.hasFocusBorder = true;
    } else if (name == "border") {
        style.focusBorderColor = parseBorder(value).color;
        style.hasFocusBorder = true;
    } else if (name == "outline") {
        style.focusOutline = parseBorder(value);
        style.hasFocusOutline = true;
    } else if (name == "outline-color") {
        style.focusOutline.color = parseColor(value);
        style.hasFocusOutline = true;
    } else if (name == "outline-width") {
        style.focusOutline.width = parseFloat(value);
        style.hasFocusOutline = true;
    } else if (name == "opacity") {
        style.focusOpacity = parseFloat(value);
    } else if (name == "scale") {
        style.focusScale = parseFloat(value);
    } else if (name == "transform") {
        auto ops = parseTransformOperations(value);
        for (const auto& op : ops) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.focusScale = val;
                }
            }
        }
    }
}
void StyleSheet::mergeActiveProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.activeBackgroundGradient = parseGradient(value);
            style.hasActiveGradient = true;
        } else {
            style.activeBackgroundColor = parseColor(value);
            style.hasActiveBg = true;
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.activeBackgroundGradient = parseGradient(value);
            style.hasActiveGradient = true;
        }
    } else if (name == "color") {
        style.activeColor = parseColor(value);
        style.hasActiveColor = true;
    } else if (name == "border-color") {
        style.activeBorderColor = parseColor(value);
        style.hasActiveBorder = true;
    } else if (name == "border") {
        style.activeBorderColor = parseBorder(value).color;
        style.hasActiveBorder = true;
    } else if (name == "outline") {
        style.activeOutline = parseBorder(value);
        style.hasActiveOutline = true;
    } else if (name == "outline-color") {
        style.activeOutline.color = parseColor(value);
        style.hasActiveOutline = true;
    } else if (name == "outline-width") {
        style.activeOutline.width = parseFloat(value);
        style.hasActiveOutline = true;
    } else if (name == "opacity") {
        style.activeOpacity = parseFloat(value);
    } else if (name == "scale") {
        style.activeScale = parseFloat(value);
    } else if (name == "transform") {
        auto ops = parseTransformOperations(value);
        for (const auto& op : ops) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.activeScale = val;
                }
            }
        }
    }
}

} // namespace FluxUI