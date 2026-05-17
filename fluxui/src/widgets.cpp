// FluxUI Widget Implementation
#include "fluxui/widgets.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <thread>
#include <future>
#include <atomic>

#include "fluxui/platform.h"

namespace FluxUI {

static bool isUtf8Continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

static size_t clampToUtf8Boundary(const std::string& text, size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && index < text.size() &&
           isUtf8Continuation((unsigned char)text[index])) {
        --index;
    }
    return index;
}

static size_t previousCodepoint(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index == 0) return 0;
    --index;
    while (index > 0 && isUtf8Continuation((unsigned char)text[index])) {
        --index;
    }
    return index;
}

static size_t nextCodepoint(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index >= text.size()) return text.size();
    ++index;
    while (index < text.size() && isUtf8Continuation((unsigned char)text[index])) {
        ++index;
    }
    return index;
}

static bool isAsciiWordChar(char c) {
    unsigned char uc = (unsigned char)c;
    return std::isalnum(uc) || c == '_';
}

static size_t previousWordBoundary(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    while (index > 0 && std::isspace((unsigned char)text[previousCodepoint(text, index)])) {
        index = previousCodepoint(text, index);
    }
    if (index == 0) return 0;

    size_t current = previousCodepoint(text, index);
    bool word = isAsciiWordChar(text[current]);
    while (current > 0) {
        size_t prev = previousCodepoint(text, current);
        if (std::isspace((unsigned char)text[prev]) || isAsciiWordChar(text[prev]) != word) break;
        current = prev;
    }
    return current;
}

static size_t nextWordBoundary(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index >= text.size()) return text.size();

    bool word = isAsciiWordChar(text[index]);
    while (index < text.size() &&
           !std::isspace((unsigned char)text[index]) &&
           isAsciiWordChar(text[index]) == word) {
        index = nextCodepoint(text, index);
    }
    while (index < text.size() && std::isspace((unsigned char)text[index])) {
        index = nextCodepoint(text, index);
    }
    return index;
}

static void wordRangeAt(const std::string& text, size_t index, size_t& start, size_t& end) {
    index = clampToUtf8Boundary(text, index);
    if (text.empty()) {
        start = end = 0;
        return;
    }

    if (index >= text.size()) index = previousCodepoint(text, text.size());
    if (std::isspace((unsigned char)text[index]) && index > 0) {
        index = previousCodepoint(text, index);
    }

    bool word = index < text.size() && isAsciiWordChar(text[index]);
    start = index;
    while (start > 0) {
        size_t prev = previousCodepoint(text, start);
        if (std::isspace((unsigned char)text[prev]) || isAsciiWordChar(text[prev]) != word) break;
        start = prev;
    }

    end = index;
    while (end < text.size()) {
        if (std::isspace((unsigned char)text[end]) || isAsciiWordChar(text[end]) != word) break;
        end = nextCodepoint(text, end);
    }
}

static float approximateGlyphAdvance(unsigned char c, float fontSize) {
    if (c == ' ') return fontSize * 0.32f;
    if (c == 'i' || c == 'l' || c == 'I' || c == '.' || c == ',' || c == ':' ||
        c == ';' || c == '!' || c == '\'' || c == '|') {
        return fontSize * 0.28f;
    }
    if (c == 'm' || c == 'w' || c == 'M' || c == 'W' || c == '@' || c == '#') {
        return fontSize * 0.82f;
    }
    if (c >= 128) return fontSize * 0.72f;
    return fontSize * 0.55f;
}

static float approximateTextWidth(const std::string& text, float fontSize) {
    float width = 0.0f;
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = (unsigned char)text[i];
        width += approximateGlyphAdvance(c, fontSize);
        i = nextCodepoint(text, i);
    }
    return width;
}

static bool parentUsesRowFlex(const Widget* widget) {
    if (!widget || !widget->parent) return false;
    const Style& parentStyle = widget->parent->computedStyle;
    if (parentStyle.display != Display::Flex) return false;
    return parentStyle.flexDirection == FlexDirection::Row ||
           parentStyle.flexDirection == FlexDirection::RowReverse;
}

static size_t approximateTextIndexAtX(const std::string& text, float x, float fontSize) {
    if (x <= 0 || text.empty()) return 0;

    float cursor = 0;
    for (size_t i = 0; i < text.size(); ) {
        size_t current = i;
        unsigned char c = (unsigned char)text[i];
        size_t next = nextCodepoint(text, i);
        float advance = approximateGlyphAdvance(c, fontSize);
        if (x < cursor + advance * 0.5f) return current;
        if (x < cursor + advance) return next;
        cursor += advance;
        i = next;
    }
    return text.size();
}

static bool consumesParentMainAxisHeight(const Widget* widget, const Style& style) {
    if (!widget || !widget->parent || style.flexGrow <= 0.0f) return false;
    const Style& parentStyle = widget->parent->computedStyle;
    if (parentStyle.display != Display::Flex) return false;

    return parentStyle.flexDirection == FlexDirection::Column ||
           parentStyle.flexDirection == FlexDirection::ColumnReverse;
}

static bool isOutOfFlow(const Widget* widget) {
    if (!widget) return false;
    return widget->computedStyle.position == Position::Absolute ||
           widget->computedStyle.position == Position::Fixed;
}

static bool rectIntersects(const Rect& a, const Rect& b, float padding = 0.0f) {
    return a.x + a.w >= b.x - padding &&
           a.x <= b.x + b.w + padding &&
           a.y + a.h >= b.y - padding &&
           a.y <= b.y + b.h + padding;
}

static bool rectEqual(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void hashCombine(size_t& seed, size_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

static void hashFloat(size_t& seed, float value) {
    hashCombine(seed, std::hash<int>{}((int)std::round(value * 1000.0f)));
}

static void hashCSSValue(size_t& seed, const CSSValue& value) {
    hashCombine(seed, std::hash<int>{}((int)value.unit));
    hashFloat(seed, value.value);
}

static size_t layoutStyleSignature(const Style& s) {
    size_t seed = 0;
    hashCombine(seed, std::hash<int>{}((int)s.display));
    hashCombine(seed, std::hash<int>{}((int)s.flexDirection));
    hashCombine(seed, std::hash<int>{}((int)s.justifyContent));
    hashCombine(seed, std::hash<int>{}((int)s.alignItems));
    hashCombine(seed, std::hash<int>{}((int)s.overflow));
    hashFloat(seed, s.flexGrow);
    hashFloat(seed, s.flexShrink);
    hashCSSValue(seed, s.flexBasis);
    hashFloat(seed, s.gap);
    hashCSSValue(seed, s.width);
    hashCSSValue(seed, s.height);
    hashCSSValue(seed, s.minWidth);
    hashCSSValue(seed, s.minHeight);
    hashCSSValue(seed, s.maxWidth);
    hashCSSValue(seed, s.maxHeight);
    hashFloat(seed, s.padding.top);
    hashFloat(seed, s.padding.right);
    hashFloat(seed, s.padding.bottom);
    hashFloat(seed, s.padding.left);
    hashFloat(seed, s.margin.top);
    hashFloat(seed, s.margin.right);
    hashFloat(seed, s.margin.bottom);
    hashFloat(seed, s.margin.left);
    hashCSSValue(seed, s.top);
    hashCSSValue(seed, s.right);
    hashCSSValue(seed, s.bottom);
    hashCSSValue(seed, s.left);
    hashFloat(seed, s.fontSize);
    hashFloat(seed, s.lineHeight);
    hashCombine(seed, std::hash<int>{}((int)s.fontWeight));
    hashCombine(seed, std::hash<int>{}((int)s.textAlign));
    return seed;
}

// ============================================================
//  Widget - Style Resolution
// ============================================================

void Widget::markLayoutDirty() {
    if (layoutDirty) return;
    layoutDirty = true;
    if (parent) parent->markLayoutDirty();
}

void Widget::markSubtreeStyleDirty() {
    if (subtreeStyleDirty) return;
    subtreeStyleDirty = true;
    if (parent) parent->markSubtreeStyleDirty();
}

void Widget::markStyleDirty() {
    styleDirty = true;
    subtreeStyleDirty = true;
    markLayoutDirty();
    if (parent) parent->markSubtreeStyleDirty();
}

void Widget::markStyleDirtyRecursive() {
    styleDirty = true;
    subtreeStyleDirty = true;
    markLayoutDirty();
    for (auto& child : children) {
        child->markStyleDirtyRecursive();
    }
    if (parent) parent->markSubtreeStyleDirty();
}

void Widget::resolveStyles(const StyleSheet& sheet) {
    if (!subtreeStyleDirty) {
        return;
    }

    if (styleDirty) {
        std::vector<CSSSelectorNode> ancestors;
        ancestors.reserve(8);
        for (Widget* node = parent; node; node = node->parent) {
            ancestors.push_back({node->className, node->id, node->type});
        }

        computedStyle = sheet.resolve(className, id, type, ancestors);

        if (parent) {
            const Style& inherited = parent->computedStyle;
            if (!computedStyle.hasColor) computedStyle.color = inherited.color;
            if (!computedStyle.hasFontSize) computedStyle.fontSize = inherited.fontSize;
            if (!computedStyle.hasFontWeight) computedStyle.fontWeight = inherited.fontWeight;
            if (!computedStyle.hasTextAlign) computedStyle.textAlign = inherited.textAlign;
            if (!computedStyle.hasLineHeight) computedStyle.lineHeight = inherited.lineHeight;
            if (!computedStyle.hasFontFamily) computedStyle.fontFamily = inherited.fontFamily;
        }

        // Merge inline styles on top of resolved.
        if (style.width.isSet()) computedStyle.width = style.width;
        if (style.height.isSet()) computedStyle.height = style.height;
        if (style.minWidth.isSet()) computedStyle.minWidth = style.minWidth;
        if (style.minHeight.isSet()) computedStyle.minHeight = style.minHeight;
        if (style.maxWidth.isSet()) computedStyle.maxWidth = style.maxWidth;
        if (style.maxHeight.isSet()) computedStyle.maxHeight = style.maxHeight;
        if (style.top.isSet()) computedStyle.top = style.top;
        if (style.right.isSet()) computedStyle.right = style.right;
        if (style.bottom.isSet()) computedStyle.bottom = style.bottom;
        if (style.left.isSet()) computedStyle.left = style.left;
        if (style.position != Position::Static) computedStyle.position = style.position;
        if (style.fontSize > 0 && style.fontSize != 14.0f) computedStyle.fontSize = style.fontSize;
        if (style.padding.top > 0 || style.padding.right > 0 ||
            style.padding.bottom > 0 || style.padding.left > 0)
            computedStyle.padding = style.padding;
        if (style.margin.top > 0 || style.margin.right > 0 ||
            style.margin.bottom > 0 || style.margin.left > 0)
            computedStyle.margin = style.margin;
        if (style.gap > 0) computedStyle.gap = style.gap;
        if (style.flexGrow > 0) computedStyle.flexGrow = style.flexGrow;
        if (style.flexBasis.isSet()) computedStyle.flexBasis = style.flexBasis;
        if (style.borderRadius.maxRadius() > 0) computedStyle.borderRadius = style.borderRadius;
        if (style.backgroundColor.a > 0) computedStyle.backgroundColor = style.backgroundColor;
        if (style.cursor != CursorType::Default) computedStyle.cursor = style.cursor;
        if (style.hasHoverBg) {
            computedStyle.hoverBackgroundColor = style.hoverBackgroundColor;
            computedStyle.hasHoverBg = true;
        }
        if (style.hasHoverColor) {
            computedStyle.hoverColor = style.hoverColor;
            computedStyle.hasHoverColor = true;
        }
        if (style.hasHoverBorder) {
            computedStyle.hoverBorderColor = style.hoverBorderColor;
            computedStyle.hasHoverBorder = true;
        }
        if (style.hoverScale >= 0) computedStyle.hoverScale = style.hoverScale;
        if (style.hoverOpacity >= 0) computedStyle.hoverOpacity = style.hoverOpacity;
        if (style.scale != 1.0f) computedStyle.scale = style.scale;
        for (const auto& prop : inlineProperties) {
            StyleSheet::mergeProperty(computedStyle, prop.name, prop.value);
        }

        size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
        if (nextLayoutSignature != layoutSignature) {
            layoutSignature = nextLayoutSignature;
            markLayoutDirty();
        }
        styleDirty = false;
    }

    for (auto& child : children) {
        if (child->subtreeStyleDirty) {
            child->resolveStyles(sheet);
        }
    }
    subtreeStyleDirty = false;
}

// ============================================================
//  Widget - Layout (Flexbox-lite)
// ============================================================

void Widget::layout(const Rect& parentBounds) {
    if (!layoutDirty && rectEqual(lastLayoutParentBounds, parentBounds)) {
        return;
    }
    lastLayoutParentBounds = parentBounds;

    auto& s = computedStyle;
    bool heightProvidedByParentFlex = consumesParentMainAxisHeight(this, s);

    // Calculate own bounds
    float x = parentBounds.x + s.margin.left;
    float y = parentBounds.y + s.margin.top;
    float w = s.width.isSet() ? s.width.resolve(parentBounds.w) :
              (parentBounds.w < 9999 ? parentBounds.w - s.margin.horizontal() : 0);
    float h = s.height.isSet() ? s.height.resolve(parentBounds.h) : 
              (parentBounds.h < 9999 ? parentBounds.h - s.margin.vertical() : 0);

    // Min/max constraints
    if (s.minWidth.isSet()) w = std::max(w, s.minWidth.resolve(parentBounds.w));
    if (s.maxWidth.isSet()) w = std::min(w, s.maxWidth.resolve(parentBounds.w));
    if (s.minHeight.isSet()) h = std::max(h, s.minHeight.resolve(parentBounds.h));

    bounds = {x, y, w, h};

    if (s.display == Display::Flex) {
        layoutFlexChildren();
    } else {
        // Block layout: stack children vertically
        float cy = bounds.y + s.padding.top;
        for (auto& child : children) {
            if (!child->visible) continue;
            if (isOutOfFlow(child.get())) continue;
            auto& cs = child->computedStyle;
            Rect childArea = {
                bounds.x + s.padding.left,
                cy,
                bounds.w - s.padding.horizontal(),
                bounds.h > 0 ? bounds.h - s.padding.vertical() : 10000
            };
            child->layout(childArea);
            cy = child->bounds.y + child->bounds.h + child->computedStyle.margin.bottom;
        }
        // Auto-height
        if (!s.height.isSet() && !heightProvidedByParentFlex && !children.empty()) {
            float maxY = bounds.y;
            for (auto& c : children) {
                if (!c->visible || isOutOfFlow(c.get())) continue;
                maxY = std::max(maxY, c->bounds.y + c->bounds.h + c->computedStyle.margin.bottom);
            }
            bounds.h = std::max(bounds.h, maxY - bounds.y + s.padding.bottom);
        }
        contentHeight = cy - bounds.y + s.padding.bottom;
    }

    if (s.maxHeight.isSet()) bounds.h = std::min(bounds.h, s.maxHeight.resolve(parentBounds.h));
    layoutPositionedChildren();
    layoutDirty = false;
}

void Widget::layoutFlexChildren() {
    auto& s = computedStyle;
    bool isRow = (s.flexDirection == FlexDirection::Row ||
                  s.flexDirection == FlexDirection::RowReverse);

    float contentX = bounds.x + s.padding.left;
    float contentY = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    float contentH = std::max(0.0f, bounds.h - s.padding.vertical());

    int visibleCount = 0;
    float totalFlexGrow = 0;
    float fixedSize = 0;
    std::vector<float> measuredMain(children.size(), 0.0f);

    if (!isRow) {
        for (size_t i = 0; i < children.size(); i++) {
            auto& child = children[i];
            if (!child->visible) continue;
            if (isOutOfFlow(child.get())) continue;

            visibleCount++;
            auto& cs = child->computedStyle;
            totalFlexGrow += cs.flexGrow;

            float childW = cs.width.isSet() ? cs.width.resolve(contentW) : contentW;
            if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                measuredMain[i] = cs.flexBasis.resolve(contentH);
                fixedSize += measuredMain[i] + cs.margin.vertical();
            } else if (cs.height.isSet()) {
                measuredMain[i] = cs.height.resolve(contentH);
                fixedSize += measuredMain[i] + cs.margin.vertical();
            } else if (cs.flexGrow <= 0) {
                Rect measureArea = {contentX, contentY, childW, 0};
                child->layout(measureArea);
                measuredMain[i] = child->bounds.h;
                fixedSize += measuredMain[i] + cs.margin.vertical();
            }
        }
    } else {
        for (size_t i = 0; i < children.size(); i++) {
            auto& child = children[i];
            if (!child->visible) continue;
            if (isOutOfFlow(child.get())) continue;

            visibleCount++;
            auto& cs = child->computedStyle;
            totalFlexGrow += cs.flexGrow;
            if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                measuredMain[i] = cs.flexBasis.resolve(contentW);
                fixedSize += measuredMain[i] + cs.margin.horizontal();
            } else if (cs.width.isSet()) {
                measuredMain[i] = cs.width.resolve(contentW);
                fixedSize += measuredMain[i] + cs.margin.horizontal();
            } else if (cs.flexGrow <= 0) {
                measuredMain[i] = 100;
                fixedSize += measuredMain[i] + cs.margin.horizontal();
            }
        }
    }

    float totalGap = s.gap * std::max(0, visibleCount - 1);
    float availableSpace = (isRow ? contentW : contentH) - fixedSize - totalGap;
    availableSpace = std::max(0.0f, availableSpace);

    float mainAxisAvailable = availableSpace;
    if (totalFlexGrow > 0) mainAxisAvailable = 0;
    
    float startOffset = 0;
    float gapOffset = s.gap;

    if (mainAxisAvailable > 0) {
        switch (s.justifyContent) {
            case JustifyContent::FlexStart:
                break;
            case JustifyContent::FlexEnd:
                startOffset = mainAxisAvailable;
                break;
            case JustifyContent::Center:
                startOffset = mainAxisAvailable / 2.0f;
                break;
            case JustifyContent::SpaceBetween:
                if (visibleCount > 1) {
                    gapOffset += mainAxisAvailable / (visibleCount - 1);
                    startOffset = 0;
                }
                break;
            case JustifyContent::SpaceAround:
                if (visibleCount > 0) {
                    float space = mainAxisAvailable / visibleCount;
                    startOffset = space / 2.0f;
                    gapOffset += space;
                }
                break;
            case JustifyContent::SpaceEvenly:
                if (visibleCount > 0) {
                    float space = mainAxisAvailable / (visibleCount + 1);
                    startOffset = space;
                    gapOffset += space;
                }
                break;
        }
    }

    float cursor = (isRow ? contentX : contentY) + startOffset;
    float maxCross = 0;
    float contentEnd = cursor;
    int laidOut = 0;

    // Parallel layout for large number of children
    const bool parallelLayout = children.size() > 32;
    std::vector<std::future<void>> futures;
    if (parallelLayout) futures.reserve(children.size());

    struct ChildLayoutTask {
        Widget* widget;
        Rect area;
    };
    std::vector<ChildLayoutTask> tasks;
    if (parallelLayout) tasks.reserve(children.size());

    for (size_t i = 0; i < children.size(); i++) {
        auto& child = children[i];
        if (!child->visible) continue;
        if (isOutOfFlow(child.get())) continue;
        auto& cs = child->computedStyle;
        float nextGap = (++laidOut < visibleCount) ? gapOffset : 0.0f;

        float childW, childH;

        if (isRow) {
            if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                childW = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
            } else {
                childW = measuredMain[i];
            }
            childH = cs.height.isSet() ? cs.height.resolve(contentH) : contentH;
            if (childH <= 0 && !cs.height.isSet()) childH = 0;

            float cy = contentY;
            if (s.alignItems == AlignItems::Center && contentH > childH) {
                cy = contentY + (contentH - childH) / 2;
            } else if (s.alignItems == AlignItems::FlexEnd && contentH > childH) {
                cy = contentY + contentH - childH;
            }

            Rect childArea = {cursor + cs.margin.left, cy + cs.margin.top,
                              std::max(0.0f, childW), std::max(0.0f, childH)};
            
            if (parallelLayout) {
                tasks.push_back({child.get(), childArea});
                cursor += childW + cs.margin.horizontal() + nextGap;
            } else {
                child->layout(childArea);
                if (!cs.height.isSet() && cs.overflow != Overflow::Scroll &&
                    child->contentHeight > child->bounds.h) {
                    child->bounds.h = child->contentHeight;
                }
                cursor += child->bounds.w + cs.margin.horizontal() + nextGap;
                maxCross = std::max(maxCross, child->bounds.h + cs.margin.vertical());
            }
            contentEnd = std::max(contentEnd, cursor - nextGap); 
        } else {
            childW = cs.width.isSet() ? cs.width.resolve(contentW) : contentW;
            if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                childH = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
            } else {
                childH = measuredMain[i];
            }

            float cx = contentX;
            if (s.alignItems == AlignItems::Center && contentW > childW) {
                cx = contentX + (contentW - childW) / 2;
            } else if (s.alignItems == AlignItems::FlexEnd && contentW > childW) {
                cx = contentX + contentW - childW;
            }

            Rect childArea = {cx + cs.margin.left, cursor + cs.margin.top,
                              std::max(0.0f, childW), std::max(0.0f, childH)};

            if (parallelLayout) {
                tasks.push_back({child.get(), childArea});
                cursor += childH + cs.margin.vertical() + nextGap;
            } else {
                child->layout(childArea);
                cursor += child->bounds.h + cs.margin.vertical() + nextGap;
                maxCross = std::max(maxCross, child->bounds.w + cs.margin.horizontal());
            }
            contentEnd = cursor;
        }
    }

    if (parallelLayout) {
        for (auto& task : tasks) {
            futures.push_back(std::async(std::launch::async, [task]() {
                task.widget->layout(task.area);
                if (!task.widget->computedStyle.height.isSet() && 
                    task.widget->computedStyle.overflow != Overflow::Scroll &&
                    task.widget->contentHeight > task.widget->bounds.h) {
                    task.widget->bounds.h = task.widget->contentHeight;
                }
            }));
        }
        for (auto& f : futures) f.get();

        // Second pass to update maxCross
        for (auto& child : children) {
            if (!child->visible || isOutOfFlow(child.get())) continue;
            maxCross = std::max(maxCross, (isRow ? child->bounds.h : child->bounds.w) + 
                                          (isRow ? child->computedStyle.margin.vertical() : child->computedStyle.margin.horizontal()));
        }
    }

    if (!s.height.isSet() && !isRow && bounds.h <= s.padding.vertical()) {
        bounds.h = (contentEnd - contentY) + s.padding.vertical();
    }
    if (!s.height.isSet() && isRow && bounds.h <= s.padding.vertical() && !children.empty()) {
        bounds.h = maxCross + s.padding.vertical();
    }

    contentHeight = isRow ?
        maxCross + s.padding.vertical() :
        std::max(0.0f, contentEnd - bounds.y + s.padding.bottom);

    if (!s.height.isSet() && !consumesParentMainAxisHeight(this, s)) {
        bounds.h = std::max(bounds.h, contentHeight);
    }
}

void Widget::layoutPositionedChildren() {
    auto& s = computedStyle;
    float contentX = bounds.x + s.padding.left;
    float contentY = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    float contentH = std::max(0.0f, bounds.h - s.padding.vertical());

    for (auto& child : children) {
        if (!child->visible || !isOutOfFlow(child.get())) continue;
        auto& cs = child->computedStyle;

        bool hasLeft = cs.left.isSet();
        bool hasRight = cs.right.isSet();
        bool hasTop = cs.top.isSet();
        bool hasBottom = cs.bottom.isSet();

        float left = hasLeft ? cs.left.resolve(contentW) : 0.0f;
        float right = hasRight ? cs.right.resolve(contentW) : 0.0f;
        float top = hasTop ? cs.top.resolve(contentH) : 0.0f;
        float bottom = hasBottom ? cs.bottom.resolve(contentH) : 0.0f;

        float childW = cs.width.isSet() ? cs.width.resolve(contentW) :
            (hasLeft && hasRight ? contentW - left - right - cs.margin.horizontal()
                                 : contentW - cs.margin.horizontal());
        float childH = cs.height.isSet() ? cs.height.resolve(contentH) :
            (hasTop && hasBottom ? contentH - top - bottom - cs.margin.vertical() : 0.0f);

        float childX = hasLeft ? contentX + left :
            (hasRight ? contentX + contentW - right - std::max(0.0f, childW) - cs.margin.horizontal()
                      : contentX);
        float childY = hasTop ? contentY + top :
            (hasBottom && childH > 0.0f
                ? contentY + contentH - bottom - std::max(0.0f, childH) - cs.margin.vertical()
                : contentY);

        Rect childArea = {
            childX + cs.margin.left,
            childY + cs.margin.top,
            std::max(0.0f, childW),
            std::max(0.0f, childH)
        };
        child->layout(childArea);

        if (!hasTop && hasBottom && !cs.height.isSet()) {
            childY = contentY + contentH - bottom - child->bounds.h - cs.margin.bottom;
            childArea.y = childY + cs.margin.top;
            child->layout(childArea);
        }
        if (!hasLeft && hasRight && !cs.width.isSet()) {
            childX = contentX + contentW - right - child->bounds.w - cs.margin.right;
            childArea.x = childX + cs.margin.left;
            child->layout(childArea);
        }
    }
}

float Widget::maxScrollY() const {
    return std::max(0.0f, contentHeight - bounds.h);
}

bool Widget::getScrollBarRects(Rect& track, Rect& thumb) const {
    float maxScroll = maxScrollY();
    if (computedStyle.overflow != Overflow::Scroll || maxScroll <= 1.0f) {
        track = {};
        thumb = {};
        return false;
    }

    float trackW = 14.0f;
    float inset = 2.0f;
    float trackH = std::max(24.0f, bounds.h - inset * 2.0f);
    float visibleRatio = bounds.h / std::max(contentHeight, bounds.h);
    float thumbH = std::clamp(trackH * visibleRatio, 32.0f, trackH);
    float thumbTravel = std::max(0.0f, trackH - thumbH);
    float thumbY = bounds.y + inset + thumbTravel * (scrollY / maxScroll);

    track = {bounds.x + bounds.w - trackW, bounds.y + inset, trackW, trackH};
    thumb = {bounds.x + bounds.w - 12.0f, thumbY, 10.0f, thumbH};
    return true;
}

void Widget::clampScroll() {
    float maxScroll = maxScrollY();
    float clampedTarget = std::clamp(targetScrollY, 0.0f, maxScroll);
    float clampedScroll = std::clamp(scrollY, 0.0f, maxScroll);
    if (clampedTarget != targetScrollY || clampedScroll != scrollY) {
        scrollVelocity = 0.0f;
    }
    targetScrollY = clampedTarget;
    scrollY = clampedScroll;
}

// ============================================================
//  Widget - Update
// ============================================================

bool Widget::hasActiveAnimations() const {
    // Check hover spring
    float hoverTarget = hovered ? 1.0f : 0.0f;
    if (std::abs(hoverAnim - hoverTarget) > 0.001f || std::abs(hoverVelocity) > 0.001f) {
        return true;
    }
    // Check scroll spring
    if (std::abs(scrollY - targetScrollY) > 0.1f || std::abs(scrollVelocity) > 0.1f) {
        return true;
    }
    // Check children
    for (auto& child : children) {
        if (child && child->hasActiveAnimations()) return true;
    }
    return false;
}

void Widget::resetTransientMotion() {
    hoverVelocity = 0.0f;
    scrollVelocity = 0.0f;
    targetScrollY = scrollY;
    clampScroll();
    scrollbarDragging = false;
    scrollbarHovered = false;
    pressed = false;

    for (auto& child : children) {
        if (child) child->resetTransientMotion();
    }
}

void Widget::update(const InputState& input) {
    if (!visible) return;

    hovered = bounds.contains(input.mousePos);

    if (input.mouseClicked[0]) {
        if (hovered && (type == "button" || computedStyle.cursor == CursorType::Pointer)) {
            focused = true;
        } else if (focused) {
            focused = false;
        }
    }

    if (hovered && input.mouseClicked[0] && onClick) {
        onClick();
    }

    // Smooth hover animation
    // Spring physics for hover animation
    float target = hovered ? 1.0f : 0.0f;
    float dt = std::clamp(input.deltaTime, 0.001f, 0.1f);
    float k = computedStyle.springStiffness;
    float d = computedStyle.springDamping;
    
    // Semi-implicit Euler integration
    float force = -k * (hoverAnim - target) - d * hoverVelocity;
    hoverVelocity += force * dt;
    hoverAnim += hoverVelocity * dt;

    // Small epsilon to stabilize
    if (std::abs(hoverAnim - target) < 0.0001f && std::abs(hoverVelocity) < 0.0001f) {
        hoverAnim = target;
        hoverVelocity = 0.0f;
    }

    pressed = hovered && input.mouseDown[0];

    // Update current scale based on hover
    float currentScale = computedStyle.scale;
    if (computedStyle.hoverScale >= 0) {
        currentScale = computedStyle.scale + (computedStyle.hoverScale - computedStyle.scale) * hoverAnim;
    }
    if (focused && computedStyle.focusScale >= 0) {
        currentScale = computedStyle.focusScale;
    }
    if (pressed && computedStyle.activeScale >= 0) {
        currentScale = computedStyle.activeScale;
    }
    renderScale = currentScale;

    // Scroll handling
    if (computedStyle.overflow == Overflow::Scroll) {
        clampScroll();

        Rect track, thumb;
        bool hasScrollbar = getScrollBarRects(track, thumb);
        scrollbarHovered = hasScrollbar && hovered &&
            (track.contains(input.mousePos) || thumb.contains(input.mousePos));

        if (hasScrollbar && hovered && input.mouseClicked[0]) {
            if (thumb.contains(input.mousePos)) {
                scrollbarDragging = true;
                scrollbarDragOffset = input.mousePos.y - thumb.y;
                scrollVelocity = 0.0f;
            } else if (track.contains(input.mousePos)) {
                float maxScroll = maxScrollY();
                float travel = std::max(1.0f, track.h - thumb.h);
                float requestedY = input.mousePos.y - track.y - thumb.h * 0.5f;
                targetScrollY = std::clamp((requestedY / travel) * maxScroll, 0.0f, maxScroll);
                scrollY = targetScrollY;
                scrollVelocity = 0.0f;
                scrollbarDragging = true;
                scrollbarDragOffset = thumb.h * 0.5f;
            }
        }

        if (scrollbarDragging) {
            if (input.mouseDown[0]) {
                float maxScroll = maxScrollY();
                float travel = std::max(1.0f, track.h - thumb.h);
                float requestedY = input.mousePos.y - track.y - scrollbarDragOffset;
                targetScrollY = std::clamp((requestedY / travel) * maxScroll, 0.0f, maxScroll);
                scrollY = targetScrollY;
                scrollVelocity = 0.0f;
            } else {
                scrollbarDragging = false;
            }
        }

        if (hovered && !scrollbarDragging && input.scroll.y != 0) {
            targetScrollY -= input.scroll.y * 72.0f;
            clampScroll();
        }

        if (hovered && !scrollbarDragging && input.keyCode != 0) {
            float maxScroll = maxScrollY();
            switch (input.keyCode) {
            case 0x26: // VK_UP
                targetScrollY -= 48.0f;
                break;
            case 0x28: // VK_DOWN
                targetScrollY += 48.0f;
                break;
            case 0x21: // VK_PRIOR (PAGE UP)
                targetScrollY -= bounds.h * 0.88f;
                break;
            case 0x22: // VK_NEXT (PAGE DOWN)
                targetScrollY += bounds.h * 0.88f;
                break;
            case 0x24: // VK_HOME
                targetScrollY = 0.0f;
                break;
            case 0x23: // VK_END
                targetScrollY = maxScroll;
                break;
            default:
                break;
            }
            clampScroll();
        }

        float previousScroll = scrollY;
        float scrollBlend = 1.0f - std::exp(-dt * 22.0f);
        scrollY += (targetScrollY - scrollY) * scrollBlend;
        scrollVelocity = (scrollY - previousScroll) / std::max(dt, 0.001f);

        if (std::abs(scrollY - targetScrollY) < 0.08f || std::abs(scrollVelocity) < 0.02f) {
            scrollY = targetScrollY;
            scrollVelocity = 0.0f;
        }
        clampScroll();
    } else {
        scrollbarHovered = false;
        scrollbarDragging = false;
    }

    // Pass input to children with adjusted mouse position
    InputState childInput = input;
    if (computedStyle.overflow == Overflow::Scroll) {
        if (scrollbarHovered || scrollbarDragging) {
            childInput.mouseClicked[0] = false;
            childInput.mouseReleased[0] = false;
            childInput.scroll = {0, 0};
        }
        childInput.mousePos.y += scrollY;
    }

    Rect visibleContent = bounds;
    if (computedStyle.overflow == Overflow::Scroll) {
        visibleContent.y += scrollY;
    }

    size_t startIndex = 0;
    size_t endIndex = children.size();
    
    const bool isColumn = (computedStyle.flexDirection == FlexDirection::Column);
    if (children.size() > 256 && isColumn && computedStyle.overflow == Overflow::Scroll) {
        auto itStart = std::lower_bound(children.begin(), children.end(), visibleContent.y - 128.0f,
            [](const std::shared_ptr<Widget>& w, float y) {
                return w->bounds.y + w->bounds.h < y;
            });
        startIndex = std::distance(children.begin(), itStart);
        
        auto itEnd = std::upper_bound(itStart, children.end(), visibleContent.y + visibleContent.h + 128.0f,
            [](float y, const std::shared_ptr<Widget>& w) {
                return y < w->bounds.y;
            });
        endIndex = std::distance(children.begin(), itEnd);
        
        // Reset state for non-visible children that were skipped
        // In a real Ferrari, we might want to do this more selectively, 
        // but for now we only process the range.
    }

    for (size_t i = 0; i < children.size(); i++) {
        auto& child = children[i];
        
        // If outside the virtualized range, skip update logic but keep focus/dragging
        if (i < startIndex || i >= endIndex) {
            if (!child->focused && !child->scrollbarDragging) {
                child->hovered = false;
                child->pressed = false;
                continue;
            }
        }

        if (computedStyle.overflow == Overflow::Scroll &&
            !rectIntersects(child->bounds, visibleContent, 128.0f) &&
            !child->focused && !child->scrollbarDragging) {
            child->hovered = false;
            child->pressed = false;
            continue;
        }
        child->update(childInput);
    }
}

CursorType Widget::cursorAt(Vec2 point) const {
    if (!visible || computedStyle.display == Display::None) {
        return CursorType::Default;
    }
    if (scrollbarDragging) {
        return CursorType::Pointer;
    }
    if (!bounds.contains(point)) {
        return CursorType::Default;
    }

    if (computedStyle.overflow == Overflow::Scroll) {
        Rect track, thumb;
        if (getScrollBarRects(track, thumb) &&
            (track.contains(point) || thumb.contains(point) || scrollbarDragging)) {
            return CursorType::Pointer;
        }
    }

    Vec2 childPoint = point;
    if (computedStyle.overflow == Overflow::Scroll) {
        childPoint.y += scrollY;
    }

    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        CursorType childCursor = (*it)->cursorAt(childPoint);
        if (childCursor != CursorType::Default) return childCursor;
    }

    return computedStyle.cursor;
}

Widget* Widget::hitTest(Vec2 point, bool interactiveOnly) {
    if (!visible || computedStyle.display == Display::None || !bounds.contains(point)) {
        return nullptr;
    }

    Vec2 childPoint = point;
    if (computedStyle.overflow == Overflow::Scroll) {
        childPoint.y += scrollY;
    }

    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (Widget* child = (*it)->hitTest(childPoint, interactiveOnly)) {
            return child;
        }
    }

    if (!interactiveOnly || type == "button" || computedStyle.cursor == CursorType::Pointer || onClick) {
        return this;
    }
    return nullptr;
}

// ============================================================
//  Widget - Render
// ============================================================

void Widget::renderBackground(Renderer& renderer) {
    auto& s = computedStyle;

    // Shadow first (behind everything)
    if (s.boxShadow.blur > 0 || s.boxShadow.spread > 0) {
        renderer.drawBoxShadow(bounds, s.boxShadow, s.borderRadius);
    }

    // Background color (with hover interpolation)
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

    if (s.backgroundGradient.type != Gradient::None) {
        renderer.drawRoundedRectGradient(bounds, s.backgroundGradient, s.borderRadius, opacity);
    } else if (bgColor.a > 0.001f) {
        renderer.drawRoundedRect(bounds, bgColor, s.borderRadius, opacity);
    }

    // Border
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
    bool clip = (computedStyle.overflow == Overflow::Hidden ||
                 computedStyle.overflow == Overflow::Scroll);
    Rect visibleContent = bounds;
    if (computedStyle.overflow == Overflow::Scroll) {
        visibleContent.y += scrollY;
    }

    if (clip) renderer.pushScissor(bounds);

    if (computedStyle.overflow == Overflow::Scroll) {
        renderer.pushTranslation({0, -scrollY});
    }

    // List Virtualization: Use binary search for O(log N) visibility check in large lists
    size_t startIndex = 0;
    size_t endIndex = children.size();
    
    const bool isColumn = (computedStyle.flexDirection == FlexDirection::Column);
    if (children.size() > 256 && isColumn && computedStyle.overflow == Overflow::Scroll) {
        // Find first child that could be visible
        auto itStart = std::lower_bound(children.begin(), children.end(), visibleContent.y - 64.0f,
            [](const std::shared_ptr<Widget>& w, float y) {
                return w->bounds.y + w->bounds.h < y;
            });
        startIndex = std::distance(children.begin(), itStart);
        
        // Find first child that is definitely not visible anymore
        auto itEnd = std::upper_bound(itStart, children.end(), visibleContent.y + visibleContent.h + 64.0f,
            [](float y, const std::shared_ptr<Widget>& w) {
                return y < w->bounds.y;
            });
        endIndex = std::distance(children.begin(), itEnd);
    }

    for (size_t i = startIndex; i < endIndex; i++) {
        auto& child = children[i];
        if (!child->visible) continue;
        if (clip && !rectIntersects(child->bounds, visibleContent, 64.0f)) continue;
        child->render(renderer);
    }

    if (computedStyle.overflow == Overflow::Scroll) {
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
    if (!visible || computedStyle.display == Display::None) return;

    bool hasScale = (renderScale != 1.0f);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }

    renderBackground(renderer);
    renderChildren(renderer);

    if (hasScale) {
        renderer.popScale();
    }
}

// ============================================================
//  Text Widget
// ============================================================

void Text::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);

    auto& s = computedStyle;
    if (!s.width.isSet() && s.flexGrow <= 0.0f && parentUsesRowFlex(this)) {
        bounds.w = std::max(1.0f, approximateTextWidth(content, s.fontSize) + s.padding.horizontal());
    }
    if (!s.height.isSet()) {
        bounds.h = std::max(1.0f, s.fontSize * s.lineHeight + s.padding.vertical());
    }
}

void Text::render(Renderer& renderer) {
    if (!visible) return;
    renderBackground(renderer);

    Color textColor = computedStyle.color;
    if (computedStyle.hasHoverColor && hoverAnim > 0) {
        textColor = Color::lerp(computedStyle.color, computedStyle.hoverColor, hoverAnim);
    }
    if (focused && computedStyle.hasFocusColor) {
        textColor = computedStyle.focusColor;
    }
    if (pressed && computedStyle.hasActiveColor) {
        textColor = computedStyle.activeColor;
    }

    Rect textRect = {
        bounds.x + computedStyle.padding.left,
        bounds.y + computedStyle.padding.top,
        std::max(0.0f, bounds.w - computedStyle.padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle.padding.vertical())
    };

    renderer.drawTextInRect(content, textRect, textColor,
                            computedStyle.fontSize, computedStyle.textAlign,
                            computedStyle.fontWeight);
    renderChildren(renderer);
}

// ============================================================
//  Button Widget
// ============================================================

void Button::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);

    auto& s = computedStyle;
    if (!s.height.isSet()) {
        bounds.h = std::max(36.0f, s.fontSize * s.lineHeight + s.padding.vertical());
    }
}

void Button::render(Renderer& renderer) {
    if (!visible) return;

    bool hasScale = (renderScale != 1.0f);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }

    // Pressed effect: slight scale down
    Rect drawBounds = bounds;
    if (pressed) {
        drawBounds.x += 1;
        drawBounds.y += 1;
        drawBounds.w -= 2;
        drawBounds.h -= 2;
    }

    auto& s = computedStyle;

    // Shadow
    if (s.boxShadow.blur > 0) {
        renderer.drawBoxShadow(drawBounds, s.boxShadow, s.borderRadius);
    }

    // Background with hover
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

    if (s.backgroundGradient.type != Gradient::None) {
        renderer.drawRoundedRectGradient(drawBounds, s.backgroundGradient, s.borderRadius, s.opacity);
    } else {
        renderer.drawRoundedRect(drawBounds, bgColor, s.borderRadius, s.opacity);
    }

    if (pressed) {
        renderer.drawRoundedRect(drawBounds, Color(0, 0, 0, 0.16f), s.borderRadius);
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
        renderer.drawBorder(drawBounds, b, s.borderRadius);
    }

    Border outline = s.outline;
    if (focused && s.hasFocusOutline) outline = s.focusOutline;
    if (pressed && s.hasActiveOutline) outline = s.activeOutline;
    if (outline.width > 0) {
        float expand = s.outlineOffset + outline.width;
        renderer.drawBorder({drawBounds.x - expand, drawBounds.y - expand,
                             drawBounds.w + expand * 2.0f, drawBounds.h + expand * 2.0f},
                            outline, BorderRadius(s.borderRadius.uniform() + expand));
    }

    // Label
    Color textColor = s.color;
    if (s.hasHoverColor && hoverAnim > 0) {
        textColor = Color::lerp(s.color, s.hoverColor, hoverAnim);
    }
    if (focused && s.hasFocusColor) {
        textColor = s.focusColor;
    }
    if (pressed && s.hasActiveColor) {
        textColor = s.activeColor;
    }
    Rect textRect = {
        drawBounds.x + s.padding.left,
        drawBounds.y + s.padding.top,
        std::max(0.0f, drawBounds.w - s.padding.horizontal()),
        std::max(0.0f, drawBounds.h - s.padding.vertical())
    };
    renderer.drawTextInRect(label, textRect, textColor,
                            s.fontSize, s.textAlign, s.fontWeight);

    renderChildren(renderer);

    if (hasScale) {
        renderer.popScale();
    }
}

// ============================================================
//  TextInput Widget
// ============================================================

void TextInput::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);

    auto& s = computedStyle;
    if (!s.height.isSet()) {
        bounds.h = std::max(38.0f, s.fontSize * s.lineHeight + s.padding.vertical());
    }
}

bool TextInput::hasSelection() const {
    return selectionAnchor_ != selectionFocus_;
}

size_t TextInput::selectionStart() const {
    return std::min(selectionAnchor_, selectionFocus_);
}

size_t TextInput::selectionEnd() const {
    return std::max(selectionAnchor_, selectionFocus_);
}

Rect TextInput::clearButtonRect() const {
    float size = std::min(18.0f, std::max(12.0f, bounds.h - 16.0f));
    return {
        bounds.x + bounds.w - computedStyle.padding.right - size - 8.0f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
}

void TextInput::update(const InputState& input) {
    Widget::update(input);

    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);

    auto setCaret = [&](size_t index, bool extendSelection) {
        caretIndex_ = clampToUtf8Boundary(value, index);
        if (extendSelection) {
            selectionFocus_ = caretIndex_;
        } else {
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
        }
        caretBlinkTime_ = 0;
    };

    auto eraseSelection = [&]() -> bool {
        if (!hasSelection()) return false;
        size_t start = selectionStart();
        size_t end = selectionEnd();
        value.erase(start, end - start);
        caretIndex_ = start;
        selectionAnchor_ = start;
        selectionFocus_ = start;
        caretBlinkTime_ = 0;
        return true;
    };

    auto insertText = [&](const std::string& text) {
        if (text.empty()) return;
        eraseSelection();
        value.insert(caretIndex_, text);
        caretIndex_ += text.size();
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
        caretBlinkTime_ = 0;
    };

    auto indexAtMouse = [&]() {
        float localX = input.mousePos.x - bounds.x - computedStyle.padding.left + scrollX_;
        return approximateTextIndexAtX(value, localX, computedStyle.fontSize);
    };

    bool shift = (input.modifiers & MOD_SHIFT) != 0;
    bool ctrl = (input.modifiers & MOD_CTRL) != 0;
    Rect clearRect = clearButtonRect();
    clearHovered_ = !value.empty() && clearRect.contains(input.mousePos);
    clearPressed_ = clearHovered_ && input.mouseDown[0];

    if (hovered && input.mouseClicked[0]) {
        focused = true;
        if (clearHovered_) {
            value.clear();
            scrollX_ = 0;
            selecting_ = false;
            setCaret(0, false);
        } else {
            size_t index = indexAtMouse();
            if (!shift && input.mouseClickCount[0] >= 2) {
                size_t start = 0;
                size_t end = 0;
                wordRangeAt(value, index, start, end);
                selectionAnchor_ = start;
                selectionFocus_ = end;
                caretIndex_ = end;
                caretBlinkTime_ = 0;
                selecting_ = false;
            } else {
                selecting_ = true;
                if (!shift) {
                    selectionAnchor_ = index;
                }
                setCaret(index, shift);
            }
        }
    } else if (!hovered && input.mouseClicked[0]) {
        focused = false;
        selecting_ = false;
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
    }

    if (selecting_ && focused && input.mouseDown[0]) {
        setCaret(indexAtMouse(), true);
    }
    if (input.mouseReleased[0]) {
        selecting_ = false;
    }

    float focusTarget = focused ? 1.0f : 0.0f;
    float focusSpeed = 16.0f;
    if (focusAnim_ < focusTarget) focusAnim_ = std::min(focusAnim_ + input.deltaTime * focusSpeed, focusTarget);
    if (focusAnim_ > focusTarget) focusAnim_ = std::max(focusAnim_ - input.deltaTime * focusSpeed, focusTarget);

    if (!focused) return;

    caretBlinkTime_ += input.deltaTime;

    // IME Rect handling removed for pure Win32 simplicity or could use ImmSetCompositionWindow

    if (input.keyCode != 0) {
        if (ctrl && input.keyCode == 'A') {
            selectionAnchor_ = 0;
            selectionFocus_ = value.size();
            caretIndex_ = value.size();
            caretBlinkTime_ = 0;
            return;
        }
        if (ctrl && (input.keyCode == 'C' || input.keyCode == 'X')) {
            if (hasSelection()) {
                std::string selected = value.substr(selectionStart(), selectionEnd() - selectionStart());
                Platform::setClipboardText(selected.c_str());
                if (input.keyCode == 'X') eraseSelection();
            }
            return;
        }
        if (ctrl && input.keyCode == 'V') {
            std::string clip = Platform::getClipboardText();
            if (!clip.empty()) {
                insertText(clip);
            }
            return;
        }

        switch (input.keyCode) {
        case 0x08: // VK_BACK
            if (!eraseSelection() && caretIndex_ > 0) {
                size_t prev = ctrl ? previousWordBoundary(value, caretIndex_) :
                    previousCodepoint(value, caretIndex_);
                value.erase(prev, caretIndex_ - prev);
                setCaret(prev, false);
            }
            return;
        case 0x2E: // VK_DELETE
            if (!eraseSelection() && caretIndex_ < value.size()) {
                size_t next = ctrl ? nextWordBoundary(value, caretIndex_) :
                    nextCodepoint(value, caretIndex_);
                value.erase(caretIndex_, next - caretIndex_);
                setCaret(caretIndex_, false);
            }
            return;
        case 0x25: // VK_LEFT
        {
            size_t target = hasSelection() && !shift ? selectionStart() :
                (ctrl ? previousWordBoundary(value, caretIndex_) : previousCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x27: // VK_RIGHT
        {
            size_t target = hasSelection() && !shift ? selectionEnd() :
                (ctrl ? nextWordBoundary(value, caretIndex_) : nextCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x24: // VK_HOME
            setCaret(0, shift);
            return;
        case 0x23: // VK_END
            setCaret(value.size(), shift);
            return;
        case 0x1B: // VK_ESCAPE
            focused = false;
            selecting_ = false;
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
            return;
        case 0x0D: // VK_RETURN
            focused = false;
            selecting_ = false;
            return;
        default:
            break;
        }
    }

    if (!ctrl && !input.text.empty()) {
        insertText(input.text);
    }
}

CursorType TextInput::cursorAt(Vec2 point) const {
    if (!visible || computedStyle.display == Display::None || !bounds.contains(point)) {
        return CursorType::Default;
    }
    if (!value.empty() && clearButtonRect().contains(point)) {
        return CursorType::Pointer;
    }
    return CursorType::Text;
}

void TextInput::render(Renderer& renderer) {
    if (!visible) return;
    renderBackground(renderer);

    auto& s = computedStyle;
    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);

    bool cssHandlesFocus = s.hasFocusOutline || s.hasFocusBorder || s.hasFocusBg;
    bool cssHandlesHover = s.hasHoverBorder || s.hasHoverBg;
    if (!cssHandlesFocus && focusAnim_ > 0.001f) {
        Rect ring = {bounds.x - focusAnim_, bounds.y - focusAnim_,
                     bounds.w + focusAnim_ * 2.0f, bounds.h + focusAnim_ * 2.0f};
        Color focusColor = Color(0.54f, 0.70f, 0.98f, 0.42f + focusAnim_ * 0.42f);
        renderer.drawBorder(ring, Border(1.0f + focusAnim_, focusColor), s.borderRadius);
    } else if (!cssHandlesHover && hoverAnim > 0.001f) {
        renderer.drawBorder(bounds, Border(1.0f, Color(0.50f, 0.53f, 0.57f, 0.44f * hoverAnim)),
                            s.borderRadius);
    }

    float clearSpace = value.empty() ? 0.0f : 28.0f;
    Rect clipRect = {
        bounds.x + s.padding.left,
        bounds.y,
        std::max(0.0f, bounds.w - s.padding.horizontal() - clearSpace),
        bounds.h
    };

    if (value.empty()) {
        scrollX_ = 0;
    } else {
        float caretX = renderer.measureText(value.substr(0, caretIndex_), s.fontSize).x;
        float rightPadding = 10.0f;
        if (caretX - scrollX_ > clipRect.w - rightPadding) {
            scrollX_ = caretX - clipRect.w + rightPadding;
        } else if (caretX - scrollX_ < 0) {
            scrollX_ = std::max(0.0f, caretX - 4.0f);
        }
        scrollX_ = std::max(0.0f, scrollX_);
    }

    renderer.pushScissor(clipRect);

    if (hasSelection() && !value.empty()) {
        size_t start = selectionStart();
        size_t end = selectionEnd();
        float startX = renderer.measureText(value.substr(0, start), s.fontSize).x - scrollX_;
        float width = renderer.measureText(value.substr(start, end - start), s.fontSize).x;
        float selH = std::min(bounds.h - 10.0f, s.fontSize + 10.0f);
        Rect selectionRect = {
            clipRect.x + startX,
            bounds.y + (bounds.h - selH) * 0.5f,
            width,
            selH
        };
        renderer.drawRoundedRect(selectionRect, Color(0.54f, 0.70f, 0.98f, 0.56f), BorderRadius(2));
    }

    std::string displayText = value.empty() ? placeholder : value;
    Color textColor = value.empty() ? Color(0.81f, 0.84f, 0.87f, 0.56f) : s.color;
    Rect textRect = {
        clipRect.x - scrollX_,
        bounds.y,
        std::max(clipRect.w + scrollX_, renderer.measureText(displayText, s.fontSize).x + 8.0f),
        bounds.h
    };
    renderer.drawTextInRect(displayText, textRect, textColor,
                            s.fontSize, TextAlign::Left, s.fontWeight);

    // Cursor blink
    if (focused) {
        float caretX = clipRect.x + renderer.measureText(value.substr(0, caretIndex_), s.fontSize).x - scrollX_;
        float cursorH = std::min(bounds.h - 12.0f, s.fontSize + 8.0f);
        float cursorY = bounds.y + (bounds.h - cursorH) * 0.5f;
        float blink = std::fmod(caretBlinkTime_, 1.0f);
        if ((blink < 0.55f || selecting_) && caretX >= clipRect.x - 1 && caretX <= clipRect.x + clipRect.w + 1) {
            renderer.drawRoundedRect({caretX, cursorY, 1.5f, cursorH},
                                     Color(0.54f, 0.70f, 0.98f, 1.0f), BorderRadius(1));
        }
    }

    renderer.popScissor();

    if (!value.empty()) {
        Rect clear = clearButtonRect();
        Color iconColor = clearPressed_ ? Color(0.91f, 0.93f, 0.96f, 1.0f) :
            Color(0.74f, 0.77f, 0.81f, clearHovered_ ? 0.96f : 0.74f);
        if (clearHovered_ || clearPressed_) {
            renderer.drawRoundedRect(clear,
                                     Color(0.54f, 0.56f, 0.60f, clearPressed_ ? 0.36f : 0.22f),
                                     BorderRadius(clear.w * 0.5f));
        }
        renderer.drawTextInRect("x", clear, iconColor,
                                std::max(11.0f, s.fontSize - 1.0f),
                                TextAlign::Center, FontWeight::Bold);
    }

    renderChildren(renderer);
}

// ============================================================
//  Icon Widget
// ============================================================

void Icon::render(Renderer& renderer) {
    if (!visible) return;
    renderBackground(renderer);

    Color c = computedStyle.color;
    if (computedStyle.hasHoverColor && hoverAnim > 0) {
        c = Color::lerp(computedStyle.color, computedStyle.hoverColor, hoverAnim);
    }

    float size = std::max(1.0f, std::min(bounds.w, bounds.h));
    float x = bounds.x + (bounds.w - size) * 0.5f;
    float y = bounds.y + (bounds.h - size) * 0.5f;
    auto rect = [&](float rx, float ry, float rw, float rh, float radius = 1.0f, float alpha = 1.0f) {
        renderer.drawRoundedRect({x + rx * size, y + ry * size, rw * size, rh * size},
                                 c.withAlpha(c.a * alpha), BorderRadius(radius));
    };
    auto border = [&](float rx, float ry, float rw, float rh, float radius = 2.0f, float width = 1.4f) {
        renderer.drawBorder({x + rx * size, y + ry * size, rw * size, rh * size},
                            Border(width, c), BorderRadius(radius));
    };
    auto textIcon = [&](const std::string& text, float scale = 0.66f, float alpha = 1.0f) {
        renderer.drawTextInRect(text, {x, y, size, size}, c.withAlpha(c.a * alpha),
                                std::max(9.0f, size * scale), TextAlign::Center,
                                FontWeight::Bold);
    };

    if (glyph == "dashboard") {
        rect(0.12f, 0.14f, 0.30f, 0.30f, 2);
        rect(0.56f, 0.14f, 0.32f, 0.22f, 2, 0.75f);
        rect(0.12f, 0.58f, 0.32f, 0.26f, 2, 0.75f);
        rect(0.58f, 0.50f, 0.30f, 0.34f, 2);
    } else if (glyph == "back") {
        textIcon("<", 0.76f);
    } else if (glyph == "forward") {
        rect(0.22f, 0.45f, 0.40f, 0.10f, 1.2f);
        rect(0.53f, 0.30f, 0.10f, 0.20f, 1.2f);
        rect(0.53f, 0.50f, 0.10f, 0.20f, 1.2f);
        rect(0.63f, 0.40f, 0.14f, 0.20f, 1.2f);
    } else if (glyph == "reload") {
        border(0.22f, 0.22f, 0.56f, 0.56f, 10, 1.45f);
        rect(0.58f, 0.13f, 0.22f, 0.08f, 1.2f);
        rect(0.73f, 0.18f, 0.08f, 0.18f, 1.2f);
    } else if (glyph == "search") {
        border(0.19f, 0.18f, 0.46f, 0.46f, 8, 1.55f);
        rect(0.58f, 0.61f, 0.25f, 0.09f, 1.5f);
    } else if (glyph == "lock") {
        border(0.28f, 0.42f, 0.44f, 0.34f, 3, 1.35f);
        border(0.35f, 0.22f, 0.30f, 0.32f, 6, 1.35f);
        rect(0.47f, 0.55f, 0.06f, 0.12f, 1.0f);
    } else if (glyph == "close") {
        textIcon("x", 0.58f, 0.88f);
    } else if (glyph == "minimize") {
        rect(0.26f, 0.54f, 0.48f, 0.08f, 1.0f, 0.92f);
    } else if (glyph == "maximize") {
        border(0.28f, 0.28f, 0.44f, 0.44f, 1.5f, 1.35f);
    } else if (glyph == "plus") {
        textIcon("+", 0.74f);
    } else if (glyph == "star") {
        textIcon("*", 0.82f);
    } else if (glyph == "menu") {
        rect(0.46f, 0.22f, 0.08f, 0.08f, 3);
        rect(0.46f, 0.46f, 0.08f, 0.08f, 3);
        rect(0.46f, 0.70f, 0.08f, 0.08f, 3);
    } else if (glyph == "shield") {
        border(0.22f, 0.14f, 0.56f, 0.64f, 5, 1.35f);
        rect(0.38f, 0.42f, 0.10f, 0.20f, 1.2f);
        rect(0.47f, 0.56f, 0.22f, 0.08f, 1.2f);
    } else if (glyph == "scanner") {
        border(0.13f, 0.13f, 0.52f, 0.52f, 6, 1.6f);
        rect(0.60f, 0.63f, 0.25f, 0.10f, 1.2f);
        rect(0.76f, 0.72f, 0.10f, 0.16f, 1.2f);
    } else if (glyph == "alert") {
        border(0.16f, 0.15f, 0.68f, 0.66f, 4, 1.5f);
        rect(0.47f, 0.31f, 0.08f, 0.27f, 1.0f);
        rect(0.47f, 0.65f, 0.08f, 0.08f, 2.0f);
    } else if (glyph == "rules") {
        rect(0.13f, 0.20f, 0.13f, 0.13f, 2);
        rect(0.35f, 0.23f, 0.48f, 0.07f, 1);
        rect(0.13f, 0.45f, 0.13f, 0.13f, 2, 0.8f);
        rect(0.35f, 0.48f, 0.42f, 0.07f, 1, 0.8f);
        rect(0.13f, 0.70f, 0.13f, 0.13f, 2, 0.65f);
        rect(0.35f, 0.73f, 0.54f, 0.07f, 1, 0.65f);
    } else if (glyph == "report") {
        border(0.20f, 0.10f, 0.58f, 0.78f, 3, 1.5f);
        rect(0.33f, 0.31f, 0.32f, 0.07f, 1);
        rect(0.33f, 0.48f, 0.34f, 0.07f, 1, 0.75f);
        rect(0.33f, 0.65f, 0.24f, 0.07f, 1, 0.6f);
    } else if (glyph == "settings") {
        border(0.28f, 0.28f, 0.44f, 0.44f, 8, 1.5f);
        rect(0.46f, 0.08f, 0.08f, 0.16f, 1);
        rect(0.46f, 0.76f, 0.08f, 0.16f, 1);
        rect(0.08f, 0.46f, 0.16f, 0.08f, 1);
        rect(0.76f, 0.46f, 0.16f, 0.08f, 1);
        rect(0.44f, 0.44f, 0.12f, 0.12f, 4);
    } else if (glyph == "block") {
        border(0.16f, 0.16f, 0.68f, 0.68f, 8, 1.5f);
        rect(0.30f, 0.46f, 0.40f, 0.09f, 1.5f);
    } else if (glyph == "card") {
        border(0.13f, 0.24f, 0.74f, 0.54f, 3, 1.4f);
        rect(0.19f, 0.36f, 0.62f, 0.08f, 1);
        rect(0.24f, 0.58f, 0.18f, 0.07f, 1, 0.7f);
    } else if (glyph == "id") {
        border(0.15f, 0.18f, 0.70f, 0.64f, 3, 1.4f);
        rect(0.25f, 0.34f, 0.18f, 0.18f, 5);
        rect(0.22f, 0.59f, 0.24f, 0.08f, 2);
        rect(0.55f, 0.38f, 0.20f, 0.06f, 1, 0.8f);
        rect(0.55f, 0.55f, 0.18f, 0.06f, 1, 0.55f);
    } else if (glyph == "check") {
        rect(0.18f, 0.50f, 0.11f, 0.22f, 1.5f);
        rect(0.26f, 0.63f, 0.43f, 0.10f, 1.5f);
        rect(0.64f, 0.30f, 0.11f, 0.43f, 1.5f);
    } else if (glyph == "download") {
        rect(0.46f, 0.15f, 0.08f, 0.42f, 1);
        rect(0.32f, 0.50f, 0.36f, 0.10f, 1);
        rect(0.24f, 0.72f, 0.52f, 0.09f, 1.5f);
    } else if (glyph == "usb") {
        rect(0.42f, 0.12f, 0.16f, 0.44f, 2);
        rect(0.34f, 0.50f, 0.32f, 0.30f, 3);
        rect(0.38f, 0.20f, 0.06f, 0.08f, 1, 0.75f);
        rect(0.56f, 0.20f, 0.06f, 0.08f, 1, 0.75f);
        rect(0.45f, 0.80f, 0.10f, 0.10f, 1);
    } else if (glyph == "mail") {
        border(0.14f, 0.24f, 0.72f, 0.52f, 4, 1.45f);
        rect(0.20f, 0.32f, 0.30f, 0.08f, 1, 0.72f);
        rect(0.50f, 0.32f, 0.30f, 0.08f, 1, 0.72f);
        rect(0.30f, 0.58f, 0.40f, 0.07f, 1, 0.56f);
    } else if (glyph == "cloud") {
        border(0.18f, 0.42f, 0.64f, 0.28f, 8, 1.45f);
        border(0.26f, 0.28f, 0.26f, 0.30f, 8, 1.25f);
        border(0.46f, 0.24f, 0.30f, 0.34f, 9, 1.25f);
    } else if (glyph == "database") {
        border(0.22f, 0.16f, 0.56f, 0.24f, 8, 1.35f);
        border(0.22f, 0.36f, 0.56f, 0.24f, 8, 1.35f);
        border(0.22f, 0.56f, 0.56f, 0.24f, 8, 1.35f);
    } else if (glyph == "clock") {
        border(0.18f, 0.18f, 0.64f, 0.64f, 10, 1.5f);
        rect(0.48f, 0.30f, 0.06f, 0.23f, 1.0f);
        rect(0.50f, 0.50f, 0.22f, 0.06f, 1.0f);
    } else if (glyph == "play") {
        textIcon(">", 0.82f);
    } else if (glyph == "pause") {
        rect(0.32f, 0.24f, 0.12f, 0.52f, 1.5f);
        rect(0.56f, 0.24f, 0.12f, 0.52f, 1.5f);
    } else {
        rect(0.25f, 0.25f, 0.50f, 0.50f, 8);
    }

    renderChildren(renderer);
}

// ============================================================
//  ProgressBar
// ============================================================

void ProgressBar::render(Renderer& renderer) {
    if (!visible) return;
    auto& s = computedStyle;

    // Background track
    renderer.drawRoundedRect(bounds, s.backgroundColor.a > 0 ?
        s.backgroundColor : Color(1, 1, 1, 0.1f), s.borderRadius);

    // Fill bar
    if (progress > 0) {
        Rect fillRect = bounds;
        fillRect.w = bounds.w * std::clamp(progress, 0.0f, 1.0f);
        renderer.drawRoundedRect(fillRect, barColor, s.borderRadius);
    }

    renderChildren(renderer);
}

// ============================================================
//  StatCard
// ============================================================

void StatCard::render(Renderer& renderer) {
    if (!visible) return;

    auto& s = computedStyle;

    // Apply scaling for premium feel
    renderer.pushScale(renderScale, bounds.center());

    // Shadow
    if (s.boxShadow.blur > 0) {
        renderer.drawBoxShadow(bounds, s.boxShadow, s.borderRadius);
    }

    // Card background
    Color bgColor = s.backgroundColor;
    if (bgColor.a < 0.01f) bgColor = Color::fromHex("#131420");
    if (s.hasHoverBg && hoverAnim > 0) {
        bgColor = Color::lerp(bgColor, s.hoverBackgroundColor, hoverAnim);
    }
    renderer.drawRoundedRect(bounds, bgColor, s.borderRadius, s.opacity);

    // Subtle glow based on accent color
    BoxShadow glow;
    glow.blur = 15 + hoverAnim * 10;
    glow.color = accentColor;
    glow.color.a = 0.1f + hoverAnim * 0.1f;
    renderer.drawBoxShadow(bounds, glow, s.borderRadius);

    // Accent bar (vertical on left for premium look)
    Rect accentBar = {bounds.x + 2, bounds.y + 12, 4, bounds.h - 24};
    renderer.drawRoundedRect(accentBar, accentColor, BorderRadius(2));

    // Border
    if (s.border.width > 0) {
        Border b = s.border;
        if (s.hasHoverBorder && hoverAnim > 0) {
            b.color = Color::lerp(s.border.color, s.hoverBorderColor, hoverAnim);
        }
        renderer.drawBorder(bounds, b, s.borderRadius);
    }

    // Content
    float px = (s.padding.left > 0 ? s.padding.left : 24) + 10;
    float py = s.padding.top > 0 ? s.padding.top : 24;

    // Title
    renderer.drawText(title, {bounds.x + px, bounds.y + py},
                      Color(1, 1, 1, 0.78f), 13, FontWeight::Bold);

    // Value (large)
    renderer.drawText(value, {bounds.x + px, bounds.y + py + 22},
                      Color(1, 1, 1, 1.0f), 32, FontWeight::Bold);

    // Subtitle
    renderer.drawText(subtitle, {bounds.x + px, bounds.y + bounds.h - 35},
                      accentColor, 12);

    renderChildren(renderer);
    renderer.popScale();
}

// ============================================================
//  Application
// ============================================================

// ============================================================
//  Application (Win32 Implementation)
// ============================================================

// ============================================================
//  Internal Event Handler
// ============================================================

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
void Internal_OnWindowEvent(void* appPtr, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* app = static_cast<Application*>(appPtr);
    if (!app) return;

    switch (msg) {
    case WM_CLOSE:
        app->running = false;
        break;
    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        app->input().windowSize = {(float)w, (float)h};
        if (app->root()) {
            app->root()->resetTransientMotion();
        }
        UIEvent event;
        event.type = UIEventType::WindowResized;
        event.position = app->input().windowSize;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_MOUSEMOVE: {
        Vec2 oldPos = app->input().mousePos;
        app->input().mousePos = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        app->input().mouseDelta = app->input().mousePos - oldPos;
        UIEvent event;
        event.type = UIEventType::MouseMove;
        event.position = app->input().mousePos;
        event.delta = app->input().mouseDelta;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        int btn = (msg == WM_LBUTTONDOWN) ? 0 : (msg == WM_RBUTTONDOWN ? 1 : 2);
        app->input().mouseDown[btn] = true;
        app->input().mouseClicked[btn] = true;
        app->input().mouseClickCount[btn] = 1;
        UIEvent event;
        event.type = UIEventType::MouseDown;
        event.position = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        event.button = btn + 1;
        event.clickCount = 1;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        int btn = (msg == WM_LBUTTONUP) ? 0 : (msg == WM_RBUTTONUP ? 1 : 2);
        app->input().mouseDown[btn] = false;
        app->input().mouseReleased[btn] = true;
        UIEvent event;
        event.type = UIEventType::MouseUp;
        event.position = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        event.button = btn + 1;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_MOUSEWHEEL: {
        float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
        app->input().scroll.y += delta;
        UIEvent event;
        event.type = UIEventType::MouseWheel;
        event.delta = {0, delta};
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_CHAR: {
        if (wParam >= 32) {
            char utf8[5] = {0};
            WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)&wParam, 1, utf8, 4, nullptr, nullptr);
            app->input().text += utf8;
            UIEvent event;
            event.type = UIEventType::TextInput;
            event.text = utf8;
            app->emit(std::move(event));
            app->requestRedraw();
        }
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        app->input().keyCode = (int)wParam;
        app->input().modifiers = MOD_NONE;
        if (GetKeyState(VK_SHIFT) & 0x8000) app->input().modifiers |= MOD_SHIFT;
        if (GetKeyState(VK_CONTROL) & 0x8000) app->input().modifiers |= MOD_CTRL;
        if (GetKeyState(VK_MENU) & 0x8000) app->input().modifiers |= MOD_ALT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) app->input().modifiers |= MOD_GUI;
        UIEvent event;
        event.type = UIEventType::KeyDown;
        event.keyCode = app->input().keyCode;
        event.modifiers = app->input().modifiers;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        UIEvent event;
        event.type = UIEventType::KeyUp;
        event.keyCode = (int)wParam;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_PAINT:
        app->requestRedraw();
        break;
    }
}
#else
// Placeholder for other platforms
void Internal_OnWindowEvent(void* app, uint32_t msg, uint64_t w, uint64_t l) {}
#endif

bool Application::init(const std::string& title, int width, int height) {
    if (!Platform::init()) return false;

    PlatformWindowConfig config;
    config.title = title;
    config.width = width;
    config.height = height;
    
    window_ = Platform::createWindow(config);
    if (!window_) return false;

#ifdef _WIN32
    SetWindowLongPtr((HWND)window_, GWLP_USERDATA, (LONG_PTR)this);
#endif

    renderer_.setBackend(backendPreference_);
    if (!renderer_.init(window_)) return false;

    defaultCursor_ = Platform::createSystemCursor(CursorType::Default);
    pointerCursor_ = Platform::createSystemCursor(CursorType::Pointer);
    textCursor_ = Platform::createSystemCursor(CursorType::Text);

    root_ = std::make_shared<Panel>();
    root_->id = "root";
    root_->className = "root";
    root_->computedStyle.display = Display::Flex;
    root_->computedStyle.flexDirection = FlexDirection::Row;

    input_.windowSize = {(float)width, (float)height};
    return true;
}

bool Application::init(const std::string& title, int width, int height, RenderBackendType backend) {
    setBackend(backend);
    return init(title, width, height);
}

void Application::setBackend(RenderBackendType backend) {
    backendPreference_ = backend;
    renderer_.setBackend(backend);
}

void Application::processEvents() {
    input_.mouseClicked[0] = input_.mouseClicked[1] = input_.mouseClicked[2] = false;
    input_.mouseReleased[0] = input_.mouseReleased[1] = input_.mouseReleased[2] = false;
    input_.mouseClickCount[0] = input_.mouseClickCount[1] = input_.mouseClickCount[2] = 0;
    input_.mouseDelta = {0, 0};
    input_.scroll = {0, 0};
    input_.text.clear();
    input_.keyCode = 0;

    Platform::processEvents(running);
}

void Application::updateCursor(CursorType cursor) {
    if (cursor == activeCursor_) return;
    Platform::setCursor((NativeCursorHandle)
        (cursor == CursorType::Pointer ? pointerCursor_ : 
         (cursor == CursorType::Text ? textCursor_ : defaultCursor_)));
    activeCursor_ = cursor;
}

bool Application::loadStylesheet(const std::string& path) {
    bool ok = stylesheet_.loadFile(path);
    if (ok && root_) root_->markStyleDirtyRecursive();
    return ok;
}

void Application::addStylesheet(const std::string& css) {
    stylesheet_.parse(css);
    if (root_) root_->markStyleDirtyRecursive();
}

size_t Application::on(UIEventType type, EventCallback callback) {
    if (!callback) return 0;
    size_t id = nextEventListenerId_++;
    eventListeners_.push_back({id, type, std::move(callback)});
    return id;
}

void Application::off(size_t listenerId) {
    eventListeners_.erase(
        std::remove_if(eventListeners_.begin(), eventListeners_.end(),
                       [listenerId](const EventListener& listener) {
                           return listener.id == listenerId;
                       }),
        eventListeners_.end());
}

void Application::emit(UIEvent event) {
    for (auto& listener : eventListeners_) {
        if (listener.type == UIEventType::Any || listener.type == event.type) {
            listener.callback(event);
            if (event.handled) break;
        }
    }
}

void Application::addRoute(const std::string& path, RouteBuilder builder) {
    if (path.empty() || !builder) return;
    routes_[path] = std::move(builder);
    if (currentRoute_.empty()) {
        currentRoute_ = path;
        routeDirty_ = true;
    }
}

void Application::setNotFoundRoute(RouteBuilder builder) {
    notFoundRoute_ = std::move(builder);
}

bool Application::navigate(const std::string& path) {
    if (path.empty()) return false;
    bool found = routes_.find(path) != routes_.end();
    if (!found && !notFoundRoute_) return false;
    if (path == currentRoute_ && !routeDirty_) return found;

    std::string previous = currentRoute_;
    currentRoute_ = path;
    routeDirty_ = true;

    UIEvent event;
    event.type = UIEventType::RouteChanged;
    event.route = currentRoute_;
    event.previousRoute = previous;
    emit(std::move(event));
    needsRedraw_ = true;
    return found;
}

bool Application::renderRoute(Widget* container) {
    if (!container) return false;
    auto route = routes_.find(currentRoute_);
    if (route == routes_.end()) {
        if (!notFoundRoute_) return false;
        container->clearChildren();
        notFoundRoute_(*this, container);
        routeDirty_ = false;
        container->markStyleDirtyRecursive();
        return false;
    }

    container->clearChildren();
    route->second(*this, container);
    routeDirty_ = false;
    container->markStyleDirtyRecursive();
    return true;
}

#ifdef _WIN32
#include <windows.h>
#endif

void Application::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    bool firstFrame = true;

    while (running) {
        processEvents();

        bool hasAnimations = root_ && root_->hasActiveAnimations();
        
        // If nothing needs to be drawn and no animations are active, sleep until an event occurs
        if (!needsRedraw_ && !hasAnimations && !firstFrame) {
#ifdef _WIN32
            WaitMessage();
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Fallback
#endif
            lastTime = std::chrono::high_resolution_clock::now(); // Reset time to avoid massive delta
            continue;
        }

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - lastTime;
        input_.deltaTime = std::clamp(elapsed.count(), 0.001f, 1.0f / 30.0f);
        lastTime = now;

        RECT rect;
#ifdef _WIN32
        GetClientRect((HWND)window_, &rect);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;
#else
        int w = 800, h = 600; // Fallback
#endif
        if (w <= 0 || h <= 0) {
            if (root_) root_->resetTransientMotion();
            needsRedraw_ = false;
#ifdef _WIN32
            WaitMessage();
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
            lastTime = std::chrono::high_resolution_clock::now();
            continue;
        }

        // Consume the redraw flag
        needsRedraw_ = false;
        if (onUpdate) onUpdate(input_.deltaTime);

        // Resolve styles
        root_->resolveStyles(stylesheet_);

        // Layout
        root_->layout({0, 0, (float)w, (float)h});

        // Update
        root_->update(input_);
        if (input_.mouseClicked[0]) {
            UIEvent clickEvent;
            clickEvent.type = UIEventType::WidgetClick;
            clickEvent.target = root_->hitTest(input_.mousePos, true);
            clickEvent.position = input_.mousePos;
            clickEvent.button = 1;
            clickEvent.clickCount = input_.mouseClickCount[0];
            if (clickEvent.target) {
                emit(std::move(clickEvent));
            }
        }
        updateCursor(root_->cursorAt(input_.mousePos));

        // Render
        renderer_.beginFrame(w, h);
        root_->render(renderer_);
        if (onRender) onRender();
        renderer_.endFrame();

#ifdef _WIN32
        if (firstFrame) {
            ShowWindow((HWND)window_, SW_SHOWMAXIMIZED);
            firstFrame = false;
        }
#endif

        constexpr float targetFrameSeconds = 1.0f / 120.0f;
        auto frameElapsed = std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - now).count();
        if (frameElapsed < targetFrameSeconds) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(targetFrameSeconds - frameElapsed));
        } else {
            std::this_thread::yield();
        }
    }
}

void Application::shutdown() {
    renderer_.shutdown();
#ifdef _WIN32
    if (window_) DestroyWindow((HWND)window_);
#endif
}

} // namespace FluxUI
