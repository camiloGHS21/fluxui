// FluxUI Blink-style Decoupled Layout Solver Implementations
#include "fluxui/layout.h"
#include "fluxui/widgets.h"
#include "fluxui/layout_object.h"
#include <algorithm>
#include <sstream>
#include <vector>
#include <future>
#include <iostream>

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
                    } else {
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

            float computedContentW = 0.0f;
            if (isRow) {
                if (totalFlexGrow > 0.0f) {
                    computedContentW = contentW;
                } else {
                    computedContentW = fixedSize + totalGap;
                }
            } else {
                float tempMaxCross = 0.0f;
                for (size_t i = 0; i < children.size(); i++) {
                    auto& child = children[i];
                    if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                    tempMaxCross = std::max(tempMaxCross, child->bounds.w + child->computedStyle->margin.horizontal());
                }
                computedContentW = tempMaxCross;
            }
            computedContentW += s.padding.horizontal();

            if (!s.width.isSet() || s.width.isAuto()) {
                float parentAvailW = constraints.availableWidth - s.margin.horizontal();
                float resolvedW = computedContentW;
                if (parentAvailW > 0.0f && parentAvailW < 9999.0f) {
                    resolvedW = std::min(resolvedW, parentAvailW);
                }
                if (s.minWidth.isSet()) {
                    resolvedW = std::max(resolvedW, s.minWidth.resolve(constraints.availableWidth, constraints.parentWidth, constraints.parentHeight, constraints.emBase));
                }
                if (s.maxWidth.isSet()) {
                    resolvedW = std::min(resolvedW, s.maxWidth.resolve(constraints.availableWidth, constraints.parentWidth, constraints.parentHeight, constraints.emBase));
                }
                widget->bounds.w = resolvedW;
                contentW = std::max(0.0f, widget->bounds.w - s.padding.horizontal());
                contentX = widget->bounds.x + s.padding.left;
            }
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
                    childH = (cs.height.isSet() && !cs.height.isAuto()) ? cs.height.resolve(contentH, constraints.parentWidth, constraints.parentHeight, constraints.emBase) : contentH;
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

                    if (cs.width.isSet() && !cs.width.isAuto()) {
                        childW = cs.width.resolve(contentW, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    } else if (effectiveAlign == AlignItems::Stretch) {
                        childW = contentW;
                    } else {
                        childW = child->bounds.w;
                    }

                    if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                        childH = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
                    } else {
                        childH = measuredMain[i];
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

        if (!widget->parent) {
            widget->bounds.x = 0.0f;
            widget->bounds.y = 0.0f;
            widget->bounds.w = constraints.availableWidth;
            widget->bounds.h = constraints.availableHeight;
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

        if (!widget->parent) {
            widget->bounds.x = 0.0f;
            widget->bounds.y = 0.0f;
            widget->bounds.w = constraints.availableWidth;
            widget->bounds.h = constraints.availableHeight;
        }

        result.x = widget->bounds.x;
        result.y = widget->bounds.y;
        result.width = widget->bounds.w;
        result.height = widget->bounds.h;
        return result;
    }

    // ============================================================
    //  TableLayoutAlgorithm Implementation (Blink parity)
    // ============================================================
    LayoutResult TableLayoutAlgorithm::layout(Widget* widget, const LayoutConstraints& constraints) {
        // Global Table Invalidation: whenever the table layout algorithm executes,
        // we must recursively invalidate the layout object caches of all descendants
        // in the table's subtree (row groups, rows, and cells). Because a table's matrix
        // layout has global dependencies, any cell visibility/span/size change affects
        // the resolved positions of all other cells in the matrix. Failing to invalidate
        // their caches will cause LayoutBox::layout to apply cached, stale physical fragments,
        // which overwrites the newly calculated W3C matrix positions with old bounds.
        std::function<void(Widget*)> invalidateTableSubtreeCache = [&](Widget* w) {
            if (!w) return;
            if (w->layoutObject) {
                w->layoutObject->invalidateCache();
            }
            for (auto& child : w->children) {
                invalidateTableSubtreeCache(child.get());
            }
        };
        for (auto& child : widget->children) {
            invalidateTableSubtreeCache(child.get());
        }

        LayoutResult result;
        auto& s = *widget->computedStyle;

        // 1. Collect all rows in standard rendering order (thead, then tbody/tr, then tfoot)
        std::vector<Widget*> rows;
        std::function<void(Widget*)> collectRows = [&](Widget* w) {
            if (!w) return;
            for (auto& child : w->children) {
                if (!child->visible || child->computedStyle->display == Display::None) continue;
                if (child->computedStyle->display == Display::TableRow) {
                    rows.push_back(child.get());
                } else if (child->computedStyle->display == Display::TableRowGroup ||
                           child->computedStyle->display == Display::TableHeaderGroup ||
                           child->computedStyle->display == Display::TableFooterGroup) {
                    collectRows(child.get());
                }
            }
        };

        // We traverse to collect rows group-by-group to respect standard HTML ordering
        Widget* thead = nullptr;
        Widget* tfoot = nullptr;
        std::vector<Widget*> tbodiesAndBareRows;

        for (auto& child : widget->children) {
            if (!child->visible || child->computedStyle->display == Display::None) continue;
            if (child->computedStyle->display == Display::TableHeaderGroup) {
                thead = child.get();
            } else if (child->computedStyle->display == Display::TableFooterGroup) {
                tfoot = child.get();
            } else if (child->computedStyle->display == Display::TableRowGroup ||
                       child->computedStyle->display == Display::TableRow) {
                tbodiesAndBareRows.push_back(child.get());
            }
        }

        if (thead) collectRows(thead);
        for (Widget* item : tbodiesAndBareRows) {
            if (item->computedStyle->display == Display::TableRow) {
                rows.push_back(item);
            } else {
                collectRows(item);
            }
        }
        if (tfoot) collectRows(tfoot);

        // 2. Build Table Matrix to resolve colspan and rowspan
        std::vector<std::vector<Widget*>> matrix;
        size_t maxCols = 0;

        for (size_t r = 0; r < rows.size(); ++r) {
            Widget* rowWidget = rows[r];
            if (matrix.size() <= r) {
                matrix.resize(r + 1);
            }

            std::vector<Widget*> cells;
            for (auto& child : rowWidget->children) {
                if (!child->visible || child->computedStyle->display == Display::None) continue;
                if (child->computedStyle->display == Display::TableCell) {
                    cells.push_back(child.get());
                }
            }

            for (Widget* cell : cells) {
                int colSpan = std::max(1, cell->colspan);
                int rowSpan = std::max(1, cell->rowspan);

                size_t c = 0;
                while (c < matrix[r].size() && matrix[r][c] != nullptr) {
                    c++;
                }

                for (int dr = 0; dr < rowSpan; ++dr) {
                    size_t targetRow = r + dr;
                    if (matrix.size() <= targetRow) {
                        matrix.resize(targetRow + 1);
                    }
                    if (matrix[targetRow].size() <= c + colSpan) {
                        matrix[targetRow].resize(c + colSpan, nullptr);
                    }
                    for (int dc = 0; dc < colSpan; ++dc) {
                        matrix[targetRow][c + dc] = cell;
                    }
                }

                maxCols = std::max(maxCols, c + colSpan);
            }
        }

        for (auto& rowSlots : matrix) {
            if (rowSlots.size() < maxCols) {
                rowSlots.resize(maxCols, nullptr);
            }
        }


        // 3. Pass 1: Measure preferred column widths (Blink-style)
        // For cells with explicit CSS width: use the resolved CSS width + padding/border
        // For cells without explicit width: measure intrinsic content size
        std::vector<float> colMinWidths(maxCols, 0.0f);
        std::vector<float> colMaxWidths(maxCols, 0.0f);

        for (size_t c = 0; c < maxCols; ++c) {
            float minW = 0.0f;
            float maxW = 0.0f;
            for (size_t r = 0; r < matrix.size(); ++r) {
                Widget* cell = matrix[r][c];
                if (!cell || cell->colspan > 1) continue;

                const auto& cs = *cell->computedStyle;
                bool hasExplicitWidth = cs.width.isSet() && !cs.width.isAuto();

                if (hasExplicitWidth) {
                    // Cell has explicit CSS width — use it as the preferred column width
                    float resolved = cs.width.resolve(constraints.availableWidth, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    // Add padding and border for the full cell box width
                    float cellPadH = cs.padding.horizontal();
                    float cellBorderH = usedBorderHorizontal(cs);
                    float fullW = resolved + cellPadH + cellBorderH;
                    minW = std::max(minW, fullW);
                    maxW = std::max(maxW, fullW);
                } else {
                    // Cell has auto width — measure intrinsic content size
                    // Max-content: single-line text width (measure at large available width)
                    Rect maxMeasure = {0, 0, 10000.0f, 10000.0f};
                    cell->layout(maxMeasure);
                    float maxContentW = cell->bounds.w;
                    // Clamp max-content to a reasonable size for auto-width cells
                    // (don't let one long text dominate the entire table)
                    float cellPadH = cs.padding.horizontal();
                    float cellBorderH = usedBorderHorizontal(cs);
                    
                    // Min-content: wrapped text width (measure at narrow width)
                    Rect minMeasure = {0, 0, cellPadH + cellBorderH + 1.0f, 10000.0f};
                    cell->layout(minMeasure);
                    float minContentW = cell->bounds.w;

                    minW = std::max(minW, std::min(maxContentW, 100.0f));
                    maxW = std::max(maxW, maxContentW);
                }
            }
            colMinWidths[c] = std::max(5.0f, minW);
            colMaxWidths[c] = std::max(5.0f, maxW);
        }

        std::printf("DEBUG TABLE: maxCols=%zu\n", maxCols);
        for (size_t c = 0; c < maxCols; ++c) {
            std::printf("  Col %zu: minW=%.2f, maxW=%.2f\n", c, colMinWidths[c], colMaxWidths[c]);
        }

        for (size_t r = 0; r < matrix.size(); ++r) {
            for (size_t c = 0; c < maxCols; ++c) {
                Widget* cell = matrix[r][c];
                if (!cell || cell->colspan <= 1) continue;

                bool isStart = true;
                if (c > 0 && matrix[r][c - 1] == cell) isStart = false;
                if (r > 0 && matrix[r - 1][c] == cell) isStart = false;
                if (!isStart) continue;

                int colSpan = std::max(1, cell->colspan);
                Rect measureArea = {0, 0, 10000.0f, 10000.0f};
                cell->layout(measureArea);
                float cellW = cell->bounds.w;
                if (cell->computedStyle->width.isSet()) {
                    float resolved = cell->computedStyle->width.resolve(constraints.availableWidth, constraints.parentWidth, constraints.parentHeight, constraints.emBase);
                    cellW = std::max(cellW, resolved);
                }

                float currentSum = 0.0f;
                for (int dc = 0; dc < colSpan && (c + dc) < maxCols; ++dc) {
                    currentSum += colMaxWidths[c + dc];
                }

                if (cellW > currentSum) {
                    float excess = (cellW - currentSum) / colSpan;
                    for (int dc = 0; dc < colSpan && (c + dc) < maxCols; ++dc) {
                        colMaxWidths[c + dc] += excess;
                        colMinWidths[c + dc] += excess;
                    }
                }
            }
        }

        // 4. Distribute table width to columns
        std::vector<float> colWidths = colMaxWidths;
        float totalMaxW = 0.0f;
        for (float w : colWidths) totalMaxW += w;

        float availW = widget->bounds.w - s.padding.horizontal();
        if (!s.width.isSet() || s.width.isAuto()) {
            // Blink behavior: table auto-width uses max-content width but caps at container available width
            float containerAvailW = widget->bounds.w - s.padding.horizontal();
            if (containerAvailW > 0.0f && containerAvailW < 9999.0f) {
                availW = std::min(totalMaxW, containerAvailW);
            } else {
                availW = totalMaxW;
            }
            widget->bounds.w = availW + s.padding.horizontal() + usedBorderHorizontal(s);
        }

        if (totalMaxW > 0.0f && totalMaxW > availW) {
            float totalMinW = 0.0f;
            for (float w : colMinWidths) totalMinW += w;

            if (totalMinW <= availW) {
                float diff = totalMaxW - totalMinW;
                float shrinkRatio = (diff > 0.0f) ? (totalMaxW - availW) / diff : 1.0f;
                for (size_t col = 0; col < maxCols; ++col) {
                    colWidths[col] = colMaxWidths[col] - shrinkRatio * (colMaxWidths[col] - colMinWidths[col]);
                }
            } else {
                for (size_t col = 0; col < maxCols; ++col) {
                    colWidths[col] = (colMinWidths[col] / totalMinW) * availW;
                }
            }
        } else if (totalMaxW > 0.0f && totalMaxW < availW) {
            float extra = availW - totalMaxW;
            for (size_t col = 0; col < maxCols; ++col) {
                colWidths[col] += extra * (colMaxWidths[col] / totalMaxW);
            }
        } else if (totalMaxW > 0.0f) {
            // totalMaxW == availW, do nothing (keep colMaxWidths)
        } else if (maxCols > 0) {
            for (size_t col = 0; col < maxCols; ++col) {
                colWidths[col] = availW / maxCols;
            }
        }

        // 5. Pass 2: Layout cells to determine row heights
        std::vector<float> rowHeights(matrix.size(), 0.0f);
        float startX = widget->bounds.x + s.padding.left;
        float startY = widget->bounds.y + s.padding.top;

        for (size_t r = 0; r < matrix.size(); ++r) {
            float maxRowH = 0.0f;
            for (size_t c = 0; c < maxCols; ++c) {
                Widget* cell = matrix[r][c];
                if (!cell || cell->rowspan > 1) continue;
                if (c > 0 && matrix[r][c - 1] == cell) continue;

                int colSpan = std::max(1, cell->colspan);
                float spanW = 0.0f;
                for (int dc = 0; dc < colSpan && (c + dc) < maxCols; ++dc) {
                    spanW += colWidths[c + dc];
                }

                Rect cellArea = {0, 0, spanW, 10000.0f};
                cell->layout(cellArea);
                maxRowH = std::max(maxRowH, cell->bounds.h);
            }
            rowHeights[r] = std::max(20.0f, maxRowH);
        }

        for (size_t r = 0; r < matrix.size(); ++r) {
            for (size_t c = 0; c < maxCols; ++c) {
                Widget* cell = matrix[r][c];
                if (!cell || cell->rowspan <= 1) continue;

                bool isStart = true;
                if (c > 0 && matrix[r][c - 1] == cell) isStart = false;
                if (r > 0 && matrix[r - 1][c] == cell) isStart = false;
                if (!isStart) continue;

                int colSpan = std::max(1, cell->colspan);
                int rowSpan = std::max(1, cell->rowspan);

                float spanW = 0.0f;
                for (int dc = 0; dc < colSpan && (c + dc) < maxCols; ++dc) {
                    spanW += colWidths[c + dc];
                }

                Rect cellArea = {0, 0, spanW, 10000.0f};
                cell->layout(cellArea);
                float cellH = cell->bounds.h;

                float currentSum = 0.0f;
                for (int dr = 0; dr < rowSpan && (r + dr) < matrix.size(); ++dr) {
                    currentSum += rowHeights[r + dr];
                }

                if (cellH > currentSum) {
                    float excess = (cellH - currentSum) / rowSpan;
                    for (int dr = 0; dr < rowSpan && (r + dr) < matrix.size(); ++dr) {
                        rowHeights[r + dr] += excess;
                    }
                }
            }
        }

        // 6. Pre-calculate snapped column and row boundaries to enforce pixel-perfect gridlines
        std::vector<float> snappedX(maxCols + 1, 0.0f);
        {
            float cumX = startX;
            snappedX[0] = std::floor(cumX + 0.5f);
            for (size_t col = 0; col < maxCols; ++col) {
                cumX += colWidths[col];
                snappedX[col + 1] = std::floor(cumX + 0.5f);
            }
        }

        std::vector<float> snappedY(matrix.size() + 1, 0.0f);
        {
            float cumY = startY;
            snappedY[0] = std::floor(cumY + 0.5f);
            for (size_t row = 0; row < matrix.size(); ++row) {
                cumY += rowHeights[row];
                snappedY[row + 1] = std::floor(cumY + 0.5f);
            }
        }

        // Pass 3: Final cell placement using snapped coordinates
        for (size_t r = 0; r < matrix.size(); ++r) {
            Widget* rowWidget = (r < rows.size()) ? rows[r] : nullptr;

            for (size_t c = 0; c < maxCols; ++c) {
                Widget* cell = matrix[r][c];
                if (!cell) continue;

                bool isStart = true;
                if (c > 0 && matrix[r][c - 1] == cell) isStart = false;
                if (r > 0 && matrix[r - 1][c] == cell) isStart = false;
                if (!isStart) continue;

                int colSpan = std::max(1, cell->colspan);
                int rowSpan = std::max(1, cell->rowspan);

                float cellX = snappedX[c];
                float cellW = snappedX[c + colSpan] - cellX;
                float cellY = snappedY[r];
                float cellH = snappedY[r + rowSpan] - cellY;
                Rect cellArea = {cellX, cellY, cellW, cellH};
                cell->layout(cellArea);
            }

            if (rowWidget) {
                float rowWidgetY = snappedY[r];
                float rowWidgetH = snappedY[r + 1] - rowWidgetY;
                rowWidget->bounds = {snappedX[0], rowWidgetY, snappedX[maxCols] - snappedX[0], rowWidgetH};
                rowWidget->layoutDirty = false;
            }
        }

        for (auto& child : widget->children) {
            if (!child->visible || child->computedStyle->display == Display::None) continue;
            if (child->computedStyle->display == Display::TableRowGroup ||
                child->computedStyle->display == Display::TableHeaderGroup ||
                child->computedStyle->display == Display::TableFooterGroup) {
                
                float groupMinY = 1e9f;
                float groupMaxY = -1e9f;
                bool hasGroupRows = false;
                
                std::function<void(Widget*)> getGroupBounds = [&](Widget* w) {
                    if (w->computedStyle->display == Display::TableRow) {
                        groupMinY = std::min(groupMinY, w->bounds.y);
                        groupMaxY = std::max(groupMaxY, w->bounds.y + w->bounds.h);
                        hasGroupRows = true;
                    } else {
                        for (auto& subChild : w->children) {
                            if (subChild->visible && subChild->computedStyle->display != Display::None) {
                                getGroupBounds(subChild.get());
                            }
                        }
                    }
                };
                getGroupBounds(child.get());
                
                if (hasGroupRows) {
                    child->bounds = {snappedX[0], groupMinY, snappedX[maxCols] - snappedX[0], groupMaxY - groupMinY};
                } else {
                    child->bounds = {snappedX[0], snappedY[matrix.size()], snappedX[maxCols] - snappedX[0], 0.0f};
                }
                child->layoutDirty = false;
            }
        }

        result.contentHeight = snappedY[matrix.size()] - widget->bounds.y + s.padding.bottom;

        if (!s.height.isSet() && !consumesParentMainAxisHeight(widget, s)) {
            widget->bounds.h = std::max(widget->bounds.h, result.contentHeight);
        }

        if (!widget->parent) {
            widget->bounds.x = 0.0f;
            widget->bounds.y = 0.0f;
            widget->bounds.w = constraints.availableWidth;
            widget->bounds.h = constraints.availableHeight;
        }

        result.x = widget->bounds.x;
        result.y = widget->bounds.y;
        result.width = widget->bounds.w;
        result.height = widget->bounds.h;
        return result;
    }

} // namespace FluxUI
