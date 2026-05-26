// FluxUI Blink-style Decoupled Layout Solver Implementations
#include "fluxui/layout.h"
#include "fluxui/widgets.h"
#include <algorithm>
#include <sstream>
#include <vector>
#include <future>

namespace FluxUI {

    // File-local helper functions replicating Blink Layout helper APIs
    namespace {
        bool isDisplayNone(const Widget* widget) {
            return widget && widget->computedStyle->display == Display::None;
        }

        bool isOutOfFlow(const Widget* widget) {
            if (!widget) return false;
            return widget->computedStyle->position == Position::Absolute ||
                   widget->computedStyle->position == Position::Fixed;
        }

        bool clipsOverflow(Overflow overflow) {
            return overflow == Overflow::Hidden || overflow == Overflow::Scroll ||
                   overflow == Overflow::Auto || overflow == Overflow::Clip;
        }

        bool isOverflowVisibleOrClip(Overflow overflow) {
            return overflow == Overflow::Visible || overflow == Overflow::Clip;
        }

        Overflow normalizedOverflowAxis(Overflow axis, Overflow otherAxis) {
            if (!isOverflowVisibleOrClip(otherAxis)) {
                if (axis == Overflow::Visible) return Overflow::Auto;
                if (axis == Overflow::Clip) return Overflow::Hidden;
            }
            return axis;
        }

        Overflow effectiveOverflowX(const Style& style) {
            return normalizedOverflowAxis(style.overflowX, style.overflowY);
        }

        Overflow effectiveOverflowY(const Style& style) {
            return normalizedOverflowAxis(style.overflowY, style.overflowX);
        }

        bool clipsOverflow(const Style& style) {
            return clipsOverflow(effectiveOverflowX(style)) ||
                   clipsOverflow(effectiveOverflowY(style));
        }

        bool consumesParentMainAxisHeight(const Widget* widget, const Style& style) {
            if (!widget || !widget->parent || style.flexGrow <= 0.0f) return false;
            const Style& parentStyle = widget->parent->computedStyle;
            if (parentStyle.display != Display::Flex) return false;
            return parentStyle.flexDirection == FlexDirection::Column ||
                   parentStyle.flexDirection == FlexDirection::ColumnReverse;
        }

        float usedBorderTopWidth(const Style& style) {
            return style.hasBorderTop ? style.borderTop.width : style.border.width;
        }

        float usedBorderRightWidth(const Style& style) {
            return style.hasBorderRight ? style.borderRight.width : style.border.width;
        }

        float usedBorderBottomWidth(const Style& style) {
            return style.hasBorderBottom ? style.borderBottom.width : style.border.width;
        }

        float usedBorderLeftWidth(const Style& style) {
            return style.hasBorderLeft ? style.borderLeft.width : style.border.width;
        }

        float usedBorderHorizontal(const Style& style) {
            return usedBorderLeftWidth(style) + usedBorderRightWidth(style);
        }

        float usedBorderVertical(const Style& style) {
            return usedBorderTopWidth(style) + usedBorderBottomWidth(style);
        }
    }

    // ============================================================
    //  FlexLayoutAlgorithm Implementation
    // ============================================================
    LayoutResult FlexLayoutAlgorithm::layout(Widget* widget, const LayoutConstraints& constraints) {
        LayoutResult result;
        auto& s = *widget->computedStyle;
        bool isRow = (s.flexDirection == FlexDirection::Row ||
                      s.flexDirection == FlexDirection::RowReverse);

        float contentX = widget->bounds.x + s.padding.left;
        float contentY = widget->bounds.y + s.padding.top;
        float contentW = std::max(0.0f, widget->bounds.w - s.padding.horizontal());
        float contentH = std::max(0.0f, widget->bounds.h - s.padding.vertical());

        float mainGap = isRow
            ? (s.columnGap > 0.0f ? s.columnGap : s.gap)
            : (s.rowGap > 0.0f ? s.rowGap : s.gap);

        auto& children = widget->children;

        if (s.flexWrap == FlexWrap::NoWrap) {
            int visibleCount = 0;
            float totalFlexGrow = 0;
            float fixedSize = 0;
            std::vector<float> measuredMain(children.size(), 0.0f);

            if (!isRow) {
                for (size_t i = 0; i < children.size(); i++) {
                    auto& child = children[i];
                    if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                    visibleCount++;
                    auto& cs = *child->computedStyle;
                    totalFlexGrow += cs.flexGrow;
                    float childW = cs.width.isSet() ? cs.width.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase) : contentW;
                    if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                        measuredMain[i] = cs.flexBasis.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                        fixedSize += measuredMain[i] + cs.margin.vertical();
                    } else if (cs.height.isSet()) {
                        measuredMain[i] = cs.height.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
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
                    if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                    visibleCount++;
                    auto& cs = *child->computedStyle;
                    totalFlexGrow += cs.flexGrow;
                    if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                        measuredMain[i] = cs.flexBasis.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                        fixedSize += measuredMain[i] + cs.margin.horizontal();
                    } else if (cs.width.isSet()) {
                        measuredMain[i] = cs.width.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                        fixedSize += measuredMain[i] + cs.margin.horizontal();
                    } else if (cs.flexGrow <= 0) {
                        measuredMain[i] = 100.0f; // Default flex item width fallback
                        fixedSize += measuredMain[i] + cs.margin.horizontal();
                    }
                }
            }

            float totalGap = mainGap * std::max(0, visibleCount - 1);
            float availableSpace = (isRow ? contentW : contentH) - fixedSize - totalGap;
            availableSpace = std::max(0.0f, availableSpace);

            float mainAxisAvailable = availableSpace;
            if (totalFlexGrow > 0) mainAxisAvailable = 0;

            float startOffset = 0;
            float gapOffset = mainGap;

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

            bool rtlRow = isRow && s.direction == Direction::Rtl;
            float cursor = 0.0f;
            if (rtlRow) {
                cursor = contentX + contentW - startOffset;
            } else {
                cursor = (isRow ? contentX : contentY) + startOffset;
            }

            float maxCross = 0.0f;
            float contentEnd = cursor;
            int laidOut = 0;

            for (size_t i = 0; i < children.size(); i++) {
                auto& child = children[i];
                if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                auto& cs = *child->computedStyle;
                float nextGap = (++laidOut < visibleCount) ? gapOffset : 0.0f;
                float childW = 0.0f, childH = 0.0f;

                if (isRow) {
                    if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                        childW = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
                    } else {
                        childW = measuredMain[i];
                    }
                    childH = cs.height.isSet() ? cs.height.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase) : contentH;
                    if (childH <= 0 && !cs.height.isSet()) childH = 0.0f;

                    AlignItems effectiveAlign = s.alignItems;
                    if (cs.alignSelf != AlignSelf::Auto) {
                        switch (cs.alignSelf) {
                            case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                            case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                            case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                            case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                            case AlignSelf::Baseline: effectiveAlign = AlignItems::Baseline; break;
                            default: break;
                        }
                    }

                    float cy = contentY;
                    if (effectiveAlign == AlignItems::Center && contentH > childH) {
                        cy = contentY + (contentH - childH) / 2;
                    } else if (effectiveAlign == AlignItems::FlexEnd && contentH > childH) {
                        cy = contentY + contentH - childH;
                    }

                    Rect childArea;
                    if (rtlRow) {
                        childArea = {cursor - childW - cs.margin.right, cy + cs.margin.top,
                                     std::max(0.0f, childW), std::max(0.0f, childH)};
                    } else {
                        childArea = {cursor + cs.margin.left, cy + cs.margin.top,
                                     std::max(0.0f, childW), std::max(0.0f, childH)};
                    }

                    child->layout(childArea);

                    if (!cs.height.isSet() && !clipsOverflow(cs) &&
                        child->contentHeight > child->bounds.h) {
                        child->bounds.h = child->contentHeight;
                    }

                    if (rtlRow)
                        cursor -= child->bounds.w + cs.margin.horizontal() + nextGap;
                    else
                        cursor += child->bounds.w + cs.margin.horizontal() + nextGap;

                    maxCross = std::max(maxCross, child->bounds.h + cs.margin.vertical());
                    contentEnd = rtlRow ? std::min(contentEnd, cursor + nextGap)
                                        : std::max(contentEnd, cursor - nextGap);
                } else {
                    childW = cs.width.isSet() ? cs.width.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase) : contentW;
                    if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                        childH = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
                    } else {
                        childH = measuredMain[i];
                    }

                    AlignItems effectiveAlign = s.alignItems;
                    if (cs.alignSelf != AlignSelf::Auto) {
                        switch (cs.alignSelf) {
                            case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                            case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                            case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                            case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                            case AlignSelf::Baseline: effectiveAlign = AlignItems::Baseline; break;
                            default: break;
                        }
                    }

                    float cx = contentX;
                    if (effectiveAlign == AlignItems::Center && contentW > childW) {
                        cx = contentX + (contentW - childW) / 2;
                    } else if (effectiveAlign == AlignItems::FlexEnd && contentW > childW) {
                        cx = contentX + contentW - childW;
                    }

                    Rect childArea = {cx + cs.margin.left, cursor + cs.margin.top,
                                      std::max(0.0f, childW), std::max(0.0f, childH)};

                    child->layout(childArea);
                    cursor += child->bounds.h + cs.margin.vertical() + nextGap;
                    maxCross = std::max(maxCross, child->bounds.w + cs.margin.horizontal());
                    contentEnd = cursor;
                }
            }

            if (!s.height.isSet() && !isRow && widget->bounds.h <= s.padding.vertical()) {
                widget->bounds.h = (contentEnd - contentY) + s.padding.vertical();
            }
            if (!s.height.isSet() && isRow && widget->bounds.h <= s.padding.vertical() && !children.empty()) {
                widget->bounds.h = maxCross + s.padding.vertical();
            }

            result.contentHeight = isRow ?
                maxCross + s.padding.vertical() :
                std::max(0.0f, contentEnd - widget->bounds.y + s.padding.bottom);

            if (!s.height.isSet() && !consumesParentMainAxisHeight(widget, s)) {
                widget->bounds.h = std::max(widget->bounds.h, result.contentHeight);
            }
        }
        else {
            // Flex Wrapping Implementation
            float crossGap = isRow
                ? (s.rowGap > 0.0f ? s.rowGap : s.gap)
                : (s.columnGap > 0.0f ? s.columnGap : s.gap);

            struct FlexItem {
                Widget* widget;
                float mainSize = 0.0f;
                float crossSize = 0.0f;
                float flexGrow = 0.0f;
                float flexShrink = 0.0f;
                float mainMargin = 0.0f;
                float crossMargin = 0.0f;
            };

            std::vector<FlexItem> items;
            items.reserve(children.size());

            if (isRow) {
                for (auto& child : children) {
                    if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                    auto& cs = *child->computedStyle;
                    FlexItem item;
                    item.widget = child.get();
                    item.flexGrow = cs.flexGrow;
                    item.flexShrink = cs.flexShrink;
                    item.mainMargin = cs.margin.horizontal();
                    item.crossMargin = cs.margin.vertical();
                    if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                        item.mainSize = cs.flexBasis.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else if (cs.width.isSet()) {
                        item.mainSize = cs.width.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else if (cs.flexGrow <= 0) {
                        item.mainSize = 100.0f;
                    }
                    if (cs.height.isSet()) {
                        item.crossSize = cs.height.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else {
                        child->layout({contentX, contentY, item.mainSize, 0});
                        item.crossSize = child->bounds.h;
                    }
                    items.push_back(item);
                }
            } else {
                for (auto& child : children) {
                    if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                    auto& cs = *child->computedStyle;
                    FlexItem item;
                    item.widget = child.get();
                    item.flexGrow = cs.flexGrow;
                    item.flexShrink = cs.flexShrink;
                    item.mainMargin = cs.margin.vertical();
                    item.crossMargin = cs.margin.horizontal();
                    if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                        item.mainSize = cs.flexBasis.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else if (cs.height.isSet()) {
                        item.mainSize = cs.height.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else if (cs.flexGrow <= 0) {
                        item.mainSize = 100.0f;
                    }
                    if (cs.width.isSet()) {
                        item.crossSize = cs.width.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else {
                        child->layout({contentX, contentY, contentW, item.mainSize});
                        item.crossSize = child->bounds.w;
                    }
                    items.push_back(item);
                }
            }

            struct FlexLine {
                std::vector<FlexItem> lineItems;
                float mainSize = 0.0f;
                float crossSize = 0.0f;
                float totalFlexGrow = 0.0f;
            };

            std::vector<FlexLine> lines;
            FlexLine currentLine;
            float maxMainSize = isRow ? contentW : contentH;

            for (auto& item : items) {
                float itemMainAlloc = item.mainSize + item.mainMargin;
                if (!currentLine.lineItems.empty() && currentLine.mainSize + itemMainAlloc + mainGap > maxMainSize) {
                    lines.push_back(currentLine);
                    currentLine = FlexLine();
                }
                if (!currentLine.lineItems.empty()) currentLine.mainSize += mainGap;
                currentLine.lineItems.push_back(item);
                currentLine.mainSize += itemMainAlloc;
                currentLine.crossSize = std::max(currentLine.crossSize, item.crossSize + item.crossMargin);
                currentLine.totalFlexGrow += item.flexGrow;
            }
            if (!currentLine.lineItems.empty()) {
                lines.push_back(currentLine);
            }

            float crossCursor = isRow ? contentY : contentX;
            for (size_t l = 0; l < lines.size(); l++) {
                auto& line = lines[l];
                float lineMainAvailable = maxMainSize - line.mainSize;
                lineMainAvailable = std::max(0.0f, lineMainAvailable);

                float lineTotalGrow = line.totalFlexGrow;
                if (lineTotalGrow > 0) lineMainAvailable = 0.0f;

                float startOffset = 0.0f;
                float gapOffset = mainGap;
                int visibleCount = (int)line.lineItems.size();

                if (lineMainAvailable > 0.0f) {
                    switch (s.justifyContent) {
                        case JustifyContent::FlexStart: break;
                        case JustifyContent::FlexEnd: startOffset = lineMainAvailable; break;
                        case JustifyContent::Center: startOffset = lineMainAvailable / 2.0f; break;
                        case JustifyContent::SpaceBetween:
                            if (visibleCount > 1) {
                                gapOffset += lineMainAvailable / (visibleCount - 1);
                                startOffset = 0.0f;
                            }
                            break;
                        case JustifyContent::SpaceAround:
                            if (visibleCount > 0) {
                                float space = lineMainAvailable / visibleCount;
                                startOffset = space / 2.0f;
                                gapOffset += space;
                            }
                            break;
                        case JustifyContent::SpaceEvenly:
                            if (visibleCount > 0) {
                                float space = lineMainAvailable / (visibleCount + 1);
                                startOffset = space;
                                gapOffset += space;
                            }
                            break;
                    }
                }

                float mainCursor = (isRow ? contentX : contentY) + startOffset;
                for (size_t i = 0; i < line.lineItems.size(); i++) {
                    auto& item = line.lineItems[i];
                    float nextGap = (i + 1 < line.lineItems.size()) ? gapOffset : 0.0f;
                    float childW = 0.0f, childH = 0.0f;

                    if (isRow) {
                        if (item.flexGrow > 0 && line.totalFlexGrow > 0) {
                            childW = item.mainSize + lineMainAvailable * (item.flexGrow / line.totalFlexGrow);
                        } else {
                            childW = item.mainSize;
                        }
                        childH = item.crossSize;

                        AlignItems effectiveAlign = s.alignItems;
                        if (item.widget->computedStyle->alignSelf != AlignSelf::Auto) {
                            switch (item.widget->computedStyle->alignSelf) {
                                case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                                case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                                case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                                case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                                default: break;
                            }
                        }

                        float cy = crossCursor;
                        if (effectiveAlign == AlignItems::Center && line.crossSize > (childH + item.crossMargin)) {
                            cy = crossCursor + (line.crossSize - (childH + item.crossMargin)) / 2;
                        } else if (effectiveAlign == AlignItems::FlexEnd && line.crossSize > (childH + item.crossMargin)) {
                            cy = crossCursor + line.crossSize - (childH + item.crossMargin);
                        }

                        Rect childArea = {mainCursor + item.widget->computedStyle->margin.left, cy + item.widget->computedStyle->margin.top,
                                          std::max(0.0f, childW), std::max(0.0f, childH)};
                        item.widget->layout(childArea);
                        mainCursor += childW + item.mainMargin + nextGap;
                    } else {
                        if (item.flexGrow > 0 && line.totalFlexGrow > 0) {
                            childH = item.mainSize + lineMainAvailable * (item.flexGrow / line.totalFlexGrow);
                        } else {
                            childH = item.mainSize;
                        }
                        childW = item.crossSize;

                        AlignItems effectiveAlign = s.alignItems;
                        if (item.widget->computedStyle->alignSelf != AlignSelf::Auto) {
                            switch (item.widget->computedStyle->alignSelf) {
                                case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                                case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                                case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                                case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                                default: break;
                            }
                        }

                        float cx = crossCursor;
                        if (effectiveAlign == AlignItems::Center && line.crossSize > (childW + item.crossMargin)) {
                            cx = crossCursor + (line.crossSize - (childW + item.crossMargin)) / 2;
                        } else if (effectiveAlign == AlignItems::FlexEnd && line.crossSize > (childW + item.crossMargin)) {
                            cx = crossCursor + line.crossSize - (childW + item.crossMargin);
                        }

                        Rect childArea = {cx + item.widget->computedStyle->margin.left, mainCursor + item.widget->computedStyle->margin.top,
                                          std::max(0.0f, childW), std::max(0.0f, childH)};
                        item.widget->layout(childArea);
                        mainCursor += childH + item.mainMargin + nextGap;
                    }
                }
                crossCursor += line.crossSize + crossGap;
            }

            float finalCrossHeight = crossCursor - crossGap - (isRow ? contentY : contentX);
            finalCrossHeight = std::max(0.0f, finalCrossHeight);

            if (!s.height.isSet() && isRow && widget->bounds.h <= s.padding.vertical()) {
                widget->bounds.h = finalCrossHeight + s.padding.vertical();
            }

            result.contentHeight = isRow ?
                finalCrossHeight + s.padding.vertical() :
                widget->bounds.h; // Column wraps are content bound
        }

        result.x = widget->bounds.x;
        result.y = widget->bounds.y;
        result.width = widget->bounds.w;
        result.height = widget->bounds.h;
        return result;
    }

    // ============================================================
    //  GridLayoutAlgorithm Implementation (Blink parity)
    // ============================================================
    LayoutResult GridLayoutAlgorithm::layout(Widget* widget, const LayoutConstraints& constraints) {
        LayoutResult result;
        auto& s = *widget->computedStyle;
        auto& children = widget->children;

        float contentX = widget->bounds.x + s.padding.left;
        float contentY = widget->bounds.y + s.padding.top;
        float contentW = std::max(0.0f, widget->bounds.w - s.padding.horizontal());

        int cols = 1;
        if (!s.gridTemplateColumns.empty()) {
            auto repeatPos = s.gridTemplateColumns.find("repeat(");
            if (repeatPos != std::string::npos) {
                auto comma = s.gridTemplateColumns.find(',', repeatPos);
                if (comma != std::string::npos) {
                    try {
                        cols = std::stoi(s.gridTemplateColumns.substr(repeatPos + 7, comma - (repeatPos + 7)));
                    } catch (...) {}
                }
            } else {
                std::istringstream iss(s.gridTemplateColumns);
                std::string token;
                int count = 0;
                while (iss >> token) {
                    count++;
                }
                if (count > 0) cols = count;
            }
        }
        cols = std::max(1, cols);

        float mainGap = s.columnGap > 0.0f ? s.columnGap : s.gap;
        float crossGap = s.rowGap > 0.0f ? s.rowGap : s.gap;

        float colW = (contentW - mainGap * (cols - 1)) / cols;
        colW = std::max(0.0f, colW);

        float cy = contentY;
        int activeIdx = 0;
        float maxRowH = 0.0f;

        for (size_t i = 0; i < children.size(); i++) {
            auto& child = children[i];
            if (!child->visible || isDisplayNone(child.get())) continue;
            
            auto& cs = *child->computedStyle;
            int col = activeIdx % cols;
            float cx = contentX + col * (colW + mainGap);
            float childH = cs.height.isSet() ? cs.height.resolve(0, constraints.parentWidth, constraints.parentHeight, constraints.emBase) : 100.0f;

            Rect childArea = {cx + cs.margin.left, cy + cs.margin.top,
                              std::max(0.0f, colW - cs.margin.horizontal()),
                              std::max(0.0f, childH)};
            child->layout(childArea);

            if (!cs.height.isSet() && !clipsOverflow(cs) && child->contentHeight > child->bounds.h) {
                child->bounds.h = child->contentHeight;
            }

            maxRowH = std::max(maxRowH, child->bounds.h + cs.margin.vertical());
            activeIdx++;

            if (activeIdx % cols == 0 || i + 1 == children.size()) {
                cy += maxRowH + crossGap;
                maxRowH = 0.0f;
            }
        }

        contentY = cy;
        result.contentHeight = cy - widget->bounds.y + s.padding.bottom;

        if (!s.height.isSet() && !consumesParentMainAxisHeight(widget, s)) {
            widget->bounds.h = std::max(widget->bounds.h, result.contentHeight);
        }

        result.x = widget->bounds.x;
        result.y = widget->bounds.y;
        result.width = widget->bounds.w;
        result.height = widget->bounds.h;
        return result;
    }

} // namespace FluxUI
