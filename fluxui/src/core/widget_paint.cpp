// FluxUI - Widget painting: list markers, background, children, and the main paint entry point (emits draw commands).
// Extracted from core/application.cpp. Shared helpers live in
// widget_internal.h (FluxUI::detail).
#include "fluxui/widgets.h"
#include "widget_internal.h"
#include "fluxui/layout_object.h"
#include "fluxui/property_trees.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

static int getListItemIndex(const Widget* widget) {
    if (!widget || !widget->parent) return 1;
    int index = 1;
    for (const auto& child : widget->parent->children) {
        if (child.get() == widget) {
            break;
        }
        if (child->computedStyle->display == Display::ListItem) {
            index++;
        }
    }
    return index;
}

static std::string toRoman(int val, bool upper) {
    if (val <= 0) return std::to_string(val);
    struct RomanMapping { int value; const char* symbol; };
    const RomanMapping mapping[] = {
        {1000, upper ? "M" : "m"}, {900, upper ? "CM" : "cm"},
        {500, upper ? "D" : "d"}, {400, upper ? "CD" : "cd"},
        {100, upper ? "C" : "c"}, {90, upper ? "XC" : "xc"},
        {50, upper ? "L" : "l"}, {40, upper ? "XL" : "xl"},
        {10, upper ? "X" : "x"}, {9, upper ? "IX" : "ix"},
        {5, upper ? "V" : "v"}, {4, upper ? "IV" : "iv"},
        {1, upper ? "I" : "i"}
    };
    std::string result;
    for (const auto& m : mapping) {
        while (val >= m.value) {
            result += m.symbol;
            val -= m.value;
        }
    }
    return result;
}

static std::string toAlpha(int val, bool upper) {
    if (val <= 0) return std::to_string(val);
    std::string result;
    int temp = val;
    while (temp > 0) {
        int rem = (temp - 1) % 26;
        char c = static_cast<char>((upper ? 'A' : 'a') + rem);
        result = c + result;
        temp = (temp - 1) / 26;
    }
    return result;
}

void Widget::renderListMarker(Renderer& renderer) {
    if (computedStyle->display != Display::ListItem) return;
    if (computedStyle->listStyleType == ListStyleType::None) return;

    std::string markerText;
    int index = getListItemIndex(this);
    switch (computedStyle->listStyleType) {
        case ListStyleType::Decimal:
            markerText = std::to_string(index) + ".";
            break;
        case ListStyleType::DecimalLeadingZero:
            markerText = (index < 10 ? "0" : "") + std::to_string(index) + ".";
            break;
        case ListStyleType::LowerRoman:
            markerText = toRoman(index, false) + ".";
            break;
        case ListStyleType::UpperRoman:
            markerText = toRoman(index, true) + ".";
            break;
        case ListStyleType::LowerAlpha:
            markerText = toAlpha(index, false) + ".";
            break;
        case ListStyleType::UpperAlpha:
            markerText = toAlpha(index, true) + ".";
            break;
        default:
            break;
    }

    Color textColor = hasMarkerStyle && markerStyle->hasColor
        ? markerStyle->color : computedStyle->color;
    float markerFontSize = hasMarkerStyle && markerStyle->hasFontSize
        ? markerStyle->fontSize : computedStyle->fontSize;
    FontWeight markerFontWeight = hasMarkerStyle && markerStyle->hasFontWeight
        ? markerStyle->fontWeight : computedStyle->fontWeight;
    const std::string& fontName = renderFontName(computedStyle);
    if (!markerText.empty()) {
        Vec2 textSize = renderer.measureText(markerText, markerFontSize, fontName);
        float x = 0.0f;
        float y = bounds.y + computedStyle->padding.top;
        if (computedStyle->direction == Direction::Ltr) {
            x = bounds.x - 8.0f - textSize.x;
        } else {
            x = bounds.x + bounds.w + 8.0f;
        }
        renderer.drawText(markerText, Vec2(x, y), textColor, markerFontSize,
                          markerFontWeight, fontName, computedStyle->fontStyle,
                          computedStyle->direction, computedStyle->unicodeBidi);
    } else {
        // Bullet shapes: Disc, Circle, Square
        float bulletRadius = computedStyle->fontSize * 0.2f;
        float d = bulletRadius * 2.0f;
        float x = 0.0f;
        float y = bounds.y + computedStyle->padding.top + computedStyle->fontSize * 0.5f - bulletRadius;
        if (computedStyle->direction == Direction::Ltr) {
            x = bounds.x - 15.0f - bulletRadius;
        } else {
            x = bounds.x + bounds.w + 15.0f - bulletRadius;
        }
        if (computedStyle->listStyleType == ListStyleType::Disc) {
            renderer.drawRoundedRect(Rect(x, y, d, d), textColor, BorderRadius(bulletRadius));
        } else if (computedStyle->listStyleType == ListStyleType::Circle) {
            renderer.drawBorder(Rect(x, y, d, d), Border(1.2f, textColor), BorderRadius(bulletRadius));
        } else if (computedStyle->listStyleType == ListStyleType::Square) {
            renderer.drawRoundedRect(Rect(x, y, d, d), textColor, BorderRadius(0.0f));
        }
    }
}

void Widget::renderBackground(Renderer& renderer) {
    const Style& s = *computedStyle;
    if (s.hasBackdropFilterBlur && s.backdropFilterBlur > 0.0f) {
        renderer.drawBackdropFilterBlur(bounds, s.backdropFilterBlur, s.borderRadius);
    }
    if (s.boxShadow.blur > 0 || s.boxShadow.spread > 0) {
        renderer.drawBoxShadow(bounds, s.boxShadow, s.borderRadius);
    }
    Color bgColor = s.backgroundColor;
    if (s.hasHoverBg && hoverAnim > 0) {
        bgColor = Color::lerp(s.backgroundColor, s.hoverBackgroundColor, hoverAnim);
    }
    if (focused && s.hasFocusBg) {
        bgColor = s.focusBackgroundColor;
    }
    if (pressed && s.hasActiveBg) {
        bgColor = s.activeBackgroundColor;
    }
    float opacity = s.opacity;
    if (s.hoverOpacity >= 0 && hoverAnim > 0) {
        opacity = s.opacity + (s.hoverOpacity - s.opacity) * hoverAnim;
    }
    if (focused && s.focusOpacity >= 0) {
        opacity = s.focusOpacity;
    }
    if (pressed && s.activeOpacity >= 0) {
        opacity = s.activeOpacity;
    }
    Gradient bgGradient = s.backgroundGradient;
    if (s.hasHoverGradient && hoverAnim > 0) {
        bgGradient = Gradient::lerp(s.backgroundGradient, s.hoverBackgroundGradient, hoverAnim);
    }
    if (focused && s.hasFocusGradient) {
        bgGradient = s.focusBackgroundGradient;
    }
    if (pressed && s.hasActiveGradient) {
        bgGradient = s.activeBackgroundGradient;
    }
    if (bgGradient.type != Gradient::None) {
        renderer.drawRoundedRectGradient(bounds, bgGradient, s.borderRadius, opacity);
    } else if (bgColor.a > 0.001f) {
        renderer.drawRoundedRect(bounds, bgColor, s.borderRadius, opacity);
    }
    if (s.border.width > 0) {
        Border b = s.border;
        if (s.hasHoverBorder && hoverAnim > 0) {
            b.color = Color::lerp(s.border.color, s.hoverBorderColor, hoverAnim);
        }
        if (focused && s.hasFocusBorder) {
            b.color = s.focusBorderColor;
        }
        if (pressed && s.hasActiveBorder) {
            b.color = s.activeBorderColor;
        }
        renderer.drawBorder(bounds, b, s.borderRadius);
    }
    auto drawEdgeBorder = [&](const Border& border, const Rect& rect) {
        if (border.width <= 0) return;
        renderer.drawRoundedRect(rect, border.color, BorderRadius(0));
    };
    if (s.hasBorderTop) {
        drawEdgeBorder(s.borderTop, {bounds.x, bounds.y, bounds.w, s.borderTop.width});
    }
    if (s.hasBorderRight) {
        drawEdgeBorder(s.borderRight,
                       {bounds.x + bounds.w - s.borderRight.width, bounds.y,
                        s.borderRight.width, bounds.h});
    }
    if (s.hasBorderBottom) {
        drawEdgeBorder(s.borderBottom,
                       {bounds.x, bounds.y + bounds.h - s.borderBottom.width,
                        bounds.w, s.borderBottom.width});
    }
    if (s.hasBorderLeft) {
        drawEdgeBorder(s.borderLeft, {bounds.x, bounds.y, s.borderLeft.width, bounds.h});
    }
    Border outline = s.outline;
    if (focused && s.hasFocusOutline) outline = s.focusOutline;
    if (pressed && s.hasActiveOutline) outline = s.activeOutline;
    if (outline.width > 0) {
        float expand = s.outlineOffset + outline.width;
        Rect outlineRect = {
            bounds.x - expand,
            bounds.y - expand,
            bounds.w + expand * 2.0f,
            bounds.h + expand * 2.0f
        };
        renderer.drawBorder(outlineRect, outline,
                            BorderRadius(s.borderRadius.uniform() + expand));
    }
}
void Widget::renderChildren(Renderer& renderer) {
    if (skipDOMChildrenPaint) return;
    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    bool clip = clipsOverflow(computedStyle);
    Rect visibleContent = bounds;
    if (scrollable) {
        visibleContent.y += scrollY;
    }
    if (clip) renderer.pushScissor(bounds);
    if (scrollable) {
        renderer.pushTranslation({0, -scrollY});
    }
    size_t startIndex = 0;
    size_t endIndex = children.size();
    const bool isColumn = (computedStyle->flexDirection == FlexDirection::Column);
    if (children.size() > 256 && isColumn && scrollable) {
        auto itStart = std::lower_bound(children.begin(), children.end(), visibleContent.y - 64.0f,
            [](const std::shared_ptr<Widget>& w, float y) {
                return w->bounds.y + w->bounds.h < y;
            });
        startIndex = std::distance(children.begin(), itStart);
        auto itEnd = std::upper_bound(itStart, children.end(), visibleContent.y + visibleContent.h + 64.0f,
            [](float y, const std::shared_ptr<Widget>& w) {
                return y < w->bounds.y;
            });
        endIndex = std::distance(children.begin(), itEnd);
    }
    for (size_t i = startIndex; i < endIndex; i++) {
        auto& child = children[i];
        if (!canPaintWidget(child.get())) continue;
        if (child->computedStyle->position == Position::Fixed) continue;
        if (clip && !rectIntersects(child->bounds, visibleContent, 64.0f)) continue;
        if (isExpandedSelectWidget(child.get())) continue;
        child->render(renderer);
    }
    for (size_t i = startIndex; i < endIndex; i++) {
        auto& child = children[i];
        if (!canPaintWidget(child.get())) continue;
        if (child->computedStyle->position == Position::Fixed) continue;
        if (clip && !rectIntersects(child->bounds, visibleContent, 64.0f)) continue;
        if (!isExpandedSelectWidget(child.get())) continue;
        child->render(renderer);
    }
    if (scrollable) {
        renderer.popTranslation();
        Rect track, thumb;
        if (getScrollBarRects(track, thumb)) {
            float active = (scrollbarHovered || scrollbarDragging) ? 1.0f : 0.0f;
            float pressed = scrollbarDragging ? 1.0f : 0.0f;
            Rect visualTrack = {
                track.x,
                track.y,
                track.w,
                track.h
            };
            Rect visualThumb = thumb;
            if (!scrollbarHovered && !scrollbarDragging) {
                visualThumb.x += 2.0f;
                visualThumb.w -= 4.0f;
            }
            renderer.drawRoundedRect(visualTrack,
                                     Color(0.13f, 0.14f, 0.16f, 0.32f + active * 0.22f),
                                     BorderRadius(0));
            renderer.drawRoundedRect(visualThumb,
                                     Color(0.47f + pressed * 0.16f,
                                           0.49f + pressed * 0.16f,
                                           0.53f + pressed * 0.16f,
                                           0.72f + active * 0.18f),
                                     BorderRadius(5));
        }
    }
    if (clip) renderer.popScissor();
}
void Widget::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;

    const Style& s = *computedStyle;

    // ── Push compositing layer for filter/blend/isolation (Blink cc::PaintOp parity) ──
    bool needsCompositingLayer = s.rare().hasMixBlendMode || s.hasFilter || s.rare().hasIsolation;
    if (needsCompositingLayer && renderer.isRecording()) {
        RenderCommand saveCmd;
        saveCmd.type = RenderCommandType::SaveLayer;
        saveCmd.rect = bounds;
        saveCmd.opacity = s.opacity;
        saveCmd.blendMode = s.rare().mixBlendMode;
        saveCmd.filterOps = s.filterOperations;
        saveCmd.isolate   = (s.rare().isolation == Style::Isolation::Isolate);
        renderer.recordCommand(saveCmd);
    }

    bool hasScale = (renderScale != 1.0f && !layoutObject);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }
    renderBackground(renderer);
    renderListMarker(renderer);
    renderChildren(renderer);
    if (hasScale) {
        renderer.popScale();
    }

    // ── Pop compositing layer ──
    if (needsCompositingLayer && renderer.isRecording()) {
        RenderCommand restoreCmd;
        restoreCmd.type = RenderCommandType::RestoreLayer;
        restoreCmd.rect = bounds;
        renderer.recordCommand(restoreCmd);
    }
}

} // namespace FluxUI