// FluxUI - Widget CSS cascade: resolveStyles() matches rules and computes the
// final ComputedStyle (the heavy half of the style pipeline). Extracted from
// core/application.cpp.
#include "fluxui/widgets.h"
#include "widget_internal.h"
#include "fluxui/css_parser.h"
#include "fluxui/layout_object.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

void Widget::resolveStyles(const StyleSheet& sheet) {
    currentSheet = &sheet;
    if (!subtreeStyleDirty) {
        return;
    }
    if (styleDirty) {
        // High-Fidelity Sibling Style Sharing (inspired by Chromium Blink's Peer ComputedStyle Sharing)
        // If an already-resolved sibling shares identical classes, inline styles, and constraints,
        // we can copy its computedStyle directly, completely bypassing expensive stylesheet matches.
        if (parent) {
            std::string_view selectorType = widgetSelectorType(this);
            for (const auto& sibling : parent->children) {
                if (sibling.get() == this) break;
                if (!sibling->styleDirty &&
                    sibling->className == className &&
                    sibling->id == id &&
                    std::string_view(widgetSelectorType(sibling.get())) == selectorType &&
                    sibling->style.width == style.width &&
                    sibling->style.height == style.height &&
                    sibling->style.minWidth == style.minWidth &&
                    sibling->style.minHeight == style.minHeight &&
                    sibling->style.maxWidth == style.maxWidth &&
                    sibling->style.maxHeight == style.maxHeight &&
                    sibling->style.top == style.top &&
                    sibling->style.right == style.right &&
                    sibling->style.bottom == style.bottom &&
                    sibling->style.left == style.left &&
                    sibling->style.padding == style.padding &&
                    sibling->style.margin == style.margin &&
                    sibling->style.position == style.position &&
                    sibling->style.flexGrow == style.flexGrow &&
                    sibling->style.flexShrink == style.flexShrink &&
                    sibling->style.flexBasis == style.flexBasis &&
                    sibling->style.aspectRatio == style.aspectRatio &&
                    sibling->style.backgroundColor == style.backgroundColor &&
                    sibling->style.color == style.color &&
                    sibling->style.fontSize == style.fontSize &&
                    sibling->style.fontFamily == style.fontFamily &&
                    sibling->inlineProperties.size() == inlineProperties.size() &&
                    sibling->inlinePropertyEpoch == inlinePropertyEpoch) {
                    
                    bool inlinePropsMatch = true;
                    for (size_t i = 0; i < inlineProperties.size(); ++i) {
                        if (inlineProperties[i].name != sibling->inlineProperties[i].name ||
                            inlineProperties[i].value != sibling->inlineProperties[i].value) {
                            inlinePropsMatch = false;
                            break;
                        }
                    }
                    
                    if (inlinePropsMatch) {
                        computedStyle = sibling->computedStyle;
                        lastResolveKey = sibling->lastResolveKey;
                        lastStyleSheetEpoch = sibling->lastStyleSheetEpoch;
                        hasLastResolveKey = sibling->hasLastResolveKey;
                        ancestorH1 = sibling->ancestorH1;
                        ancestorH2 = sibling->ancestorH2;
                        
                        size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
                        if (nextLayoutSignature != layoutSignature) {
                            layoutSignature = nextLayoutSignature;
                            markLayoutDirty();
                        }
                        
                        styleDirty = false;
                        break;
                    }
                }
            }
        }
    }
    if (styleDirty) {
        cachedSelectorType.clear();
        if (parent) {
            uint64_t h1 = parent->ancestorH1;
            uint64_t h2 = parent->ancestorH2;

            auto hashStr = [&](std::string_view sv) {
                for (char c : sv) {
                    h1 ^= static_cast<uint64_t>(c);
                    h1 *= 1099511628211ULL;
                }
                for (char c : sv) {
                    h2 = ((h2 << 5) + h2) + static_cast<uint64_t>(c);
                }
            };

            h1 ^= 0xDDULL; h2 ^= 0xDDULL;
            hashStr(parent->className);
            h1 ^= 0xCCULL; h2 ^= 0xCCULL;
            hashStr(parent->id);
            h1 ^= 0xBBULL; h2 ^= 0xBBULL;
            hashStr(widgetSelectorType(parent));

            ancestorH1 = h1;
            ancestorH2 = h2;
        } else {
            ancestorH1 = 14695981039346656037ULL;
            ancestorH2 = 5381ULL;
        }

        const Style* parentStyle = parent ? parent->computedStyle.get() : nullptr;
        std::string_view selectorType = widgetSelectorType(this);

        uint64_t h1 = ancestorH1;
        uint64_t h2 = ancestorH2;
        auto hashStr = [&](std::string_view sv) {
            for (char c : sv) {
                h1 ^= static_cast<uint64_t>(c);
                h1 *= 1099511628211ULL;
            }
            for (char c : sv) {
                h2 = ((h2 << 5) + h2) + static_cast<uint64_t>(c);
            }
        };
        hashStr(className);
        h1 ^= 0xFFULL; h2 ^= 0xFFULL;
        hashStr(id);
        h1 ^= 0xEEULL; h2 ^= 0xEEULL;
        hashStr(selectorType);

        if (parentStyle) {
            h1 ^= parentStyle->inheritedHash;
            h2 ^= ~parentStyle->inheritedHash;
        }
        StyleCacheKey currentKey{h1, h2};

        if (hasLastResolveKey && lastResolveKey == currentKey && lastStyleSheetEpoch == sheet.getEpoch()
            && inlineProperties.size() == lastInlinePropertyCount && inlinePropertyEpoch == lastInlinePropertyEpoch) {
            styleDirty = false;
        }

        if (styleDirty) {
            auto getAncestors = [this]() -> std::vector<CSSSelectorNode> {
                std::vector<CSSSelectorNode> t_ancestors;
                size_t ancestorCount = 0;
                for (Widget* node = parent; node; node = node->parent) {
                    ++ancestorCount;
                }
                t_ancestors.reserve(ancestorCount);
                for (Widget* node = parent; node; node = node->parent) {
                    t_ancestors.push_back({node->className, node->id, widgetSelectorType(node), node});
                }
                return t_ancestors;
            };

            this->computedStyle = sheet.resolveLazy(className, id, selectorType, ancestorH1, ancestorH2, parentStyle, getAncestors, this);
            Style& computedStyle = this->computedStyle.ensureMutable();
        if (!parent && computedStyle.overflowY == Overflow::Visible) {
            computedStyle.overflowY = Overflow::Auto;
        }
        if (parent) {
            const Style& inherited = parent->computedStyle;
            // ── Table-driven inheritance of CSS inherited properties ──────────
            // Each entry inherits the parent's computed value when the child did
            // not set the property itself (tracked by its has* flag). This set
            // is kept in sync with computeInheritedHash() so the resolved-style
            // cache invalidates whenever an inherited value changes.
            // (CSS Cascading & Inheritance L5 / Blink: properties flagged
            //  `inherited: true` in css_properties.json5.)
            #define FLUX_INHERIT(field, flag) \
                if (!computedStyle.flag) computedStyle.field = inherited.field;
            FLUX_INHERIT(color, hasColor)
            FLUX_INHERIT(fontSize, hasFontSize)
            FLUX_INHERIT(fontWeight, hasFontWeight)
            FLUX_INHERIT(fontStyle, hasFontStyle)
            FLUX_INHERIT(fontFamily, hasFontFamily)
            FLUX_INHERIT(textAlign, hasTextAlign)
            FLUX_INHERIT(lineHeight, hasLineHeight)
            FLUX_INHERIT(letterSpacing, hasLetterSpacing)
            FLUX_INHERIT(wordSpacing, hasWordSpacing)
            FLUX_INHERIT(whiteSpace, hasWhiteSpace)
            FLUX_INHERIT(textTransform, hasTextTransform)
            FLUX_INHERIT(wordBreak, hasWordBreak)
            FLUX_INHERIT(direction, hasDirection)
            FLUX_INHERIT(writingMode, hasWritingMode)
            #undef FLUX_INHERIT
            // list-style-type also propagates its has* flag so the value keeps
            // cascading to grandchildren that query hasListStyleType directly.
            if (!computedStyle.hasListStyleType) {
                computedStyle.listStyleType = inherited.listStyleType;
                computedStyle.hasListStyleType = inherited.hasListStyleType;
            }
            // cursor / visibility / pointer-events are CSS-inherited but have no
            // has* flag; inherit when the child still holds the initial value
            // (same heuristic the property cascade already uses for cursor).
            if (computedStyle.cursor == CursorType::Default) {
                computedStyle.cursor = inherited.cursor;
            }
            if (computedStyle.visibility == Visibility::Visible) {
                computedStyle.visibility = inherited.visibility;
            }
            if (computedStyle.pointerEvents == PointerEvents::Auto) {
                computedStyle.pointerEvents = inherited.pointerEvents;
            }
        }
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
        if (style.overflow != Overflow::Visible) computedStyle.overflow = style.overflow;
        if (style.overflowX != Overflow::Visible) computedStyle.overflowX = style.overflowX;
        if (style.overflowY != Overflow::Visible) computedStyle.overflowY = style.overflowY;
        if (style.fontSize > 0 && style.fontSize != 14.0f) computedStyle.fontSize = style.fontSize;
        if (style.hasFontStyle) {
            computedStyle.fontStyle = style.fontStyle;
            computedStyle.hasFontStyle = true;
        }
        if (style.padding.top > 0 || style.padding.right > 0 ||
            style.padding.bottom > 0 || style.padding.left > 0)
            computedStyle.padding = style.padding;
        if (style.margin.top > 0 || style.margin.right > 0 ||
            style.margin.bottom > 0 || style.margin.left > 0)
            computedStyle.margin = style.margin;
        if (style.gap > 0) computedStyle.gap = style.gap;
        if (style.rowGap > 0) computedStyle.rowGap = style.rowGap;
        if (style.columnGap > 0) computedStyle.columnGap = style.columnGap;
        if (style.columnCount > 0) computedStyle.columnCount = style.columnCount;
        if (style.columnWidth > 0.0f) computedStyle.columnWidth = style.columnWidth;
        if (style.aspectRatio > 0) computedStyle.aspectRatio = style.aspectRatio;
        if (style.hasObjectFit) {
            computedStyle.objectFit = style.objectFit;
            computedStyle.hasObjectFit = true;
        }
        if (style.hasAppearance) {
            computedStyle.appearance = style.appearance;
            computedStyle.hasAppearance = true;
        }
        if (style.hasObjectPosition) {
            computedStyle.objectPosition = style.objectPosition;
            computedStyle.objectPositionOffset = style.objectPositionOffset;
            computedStyle.hasObjectPosition = true;
        }
        if (style.hasVerticalAlign) {
            computedStyle.verticalAlign = style.verticalAlign;
            computedStyle.hasVerticalAlign = true;
        }
        if (style.hasBoxSizing || style.boxSizing != BoxSizing::ContentBox) {
            computedStyle.boxSizing = style.boxSizing;
            computedStyle.hasBoxSizing = true;
        }
        if (style.flexGrow > 0) computedStyle.flexGrow = style.flexGrow;
        if (style.flexBasis.isSet()) computedStyle.flexBasis = style.flexBasis;
        if (style.borderRadius.maxRadius() > 0) computedStyle.borderRadius = style.borderRadius;
        if (style.hasBackdropFilter) {
            computedStyle.backdropFilterOperations = style.backdropFilterOperations;
            computedStyle.hasBackdropFilter = true;
            computedStyle.backdropFilterBlur = style.backdropFilterBlur;
            computedStyle.hasBackdropFilterBlur = style.hasBackdropFilterBlur;
        }
        if (style.hasFilter) {
            computedStyle.filterOperations = style.filterOperations;
            computedStyle.hasFilter = true;
        }
        // Grid layout cascade
        if (!style.gridTemplateColumnTracks.empty() || !style.gridTemplateColumns.empty()) {
            computedStyle.gridTemplateColumnTracks = style.gridTemplateColumnTracks;
            computedStyle.gridTemplateColumns      = style.gridTemplateColumns;
        }
        if (!style.gridTemplateRowTracks.empty() || !style.gridTemplateRows.empty()) {
            computedStyle.gridTemplateRowTracks = style.gridTemplateRowTracks;
            computedStyle.gridTemplateRows      = style.gridTemplateRows;
        }
        if (style.hasGridTemplateAreas) {
            computedStyle.gridTemplateAreas    = style.gridTemplateAreas;
            computedStyle.hasGridTemplateAreas = true;
        }
        if (!style.gridAutoRowTracks.empty())    computedStyle.gridAutoRowTracks    = style.gridAutoRowTracks;
        if (!style.gridAutoColumnTracks.empty()) computedStyle.gridAutoColumnTracks = style.gridAutoColumnTracks;
        if (style.gridAutoFlow != GridAutoFlow::Row) computedStyle.gridAutoFlow = style.gridAutoFlow;
        if (!style.gridColumnStart.isAuto()) computedStyle.gridColumnStart = style.gridColumnStart;
        if (!style.gridColumnEnd.isAuto())   computedStyle.gridColumnEnd   = style.gridColumnEnd;
        if (!style.gridRowStart.isAuto())    computedStyle.gridRowStart    = style.gridRowStart;
        if (!style.gridRowEnd.isAuto())      computedStyle.gridRowEnd      = style.gridRowEnd;
        if (!style.gridColumn.empty())       computedStyle.gridColumn      = style.gridColumn;
        if (!style.gridRow.empty())          computedStyle.gridRow         = style.gridRow;
        if (!style.gridArea.empty())         computedStyle.gridArea        = style.gridArea;
        if (style.hasJustifyItems) {
            computedStyle.justifyItems    = style.justifyItems;
            computedStyle.hasJustifyItems = true;
        }
        if (style.hasJustifySelf) {
            computedStyle.justifySelf    = style.justifySelf;
            computedStyle.hasJustifySelf = true;
        }
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
            if (prop.name.rfind("--", 0) == 0) {
                bool valid = true;
                std::string value = sheet.resolveValue(prop.value, computedStyle.customProperties, &valid);
                if (!valid) continue;
                computedStyle.customProperties[prop.name] =
                    std::move(value);
            }
        }
        for (const auto& prop : inlineProperties) {
            if (prop.name.rfind("--", 0) == 0) continue;
            bool valid = true;
            std::string value = sheet.resolveValue(prop.value, computedStyle.customProperties, &valid);
            if (!valid) continue;

            std::string lowerValue = value;
            for (char& c : lowerValue) c = (char)std::tolower((unsigned char)c);
            if (lowerValue == "inherit" || lowerValue == "initial" || lowerValue == "unset") {
                Style initialStyle;
                bool inherited = prop.name == "color" || prop.name == "font-size" ||
                                 prop.name == "font-weight" || prop.name == "font-style" ||
                                 prop.name == "font-family" ||
                                 prop.name == "line-height" || prop.name == "text-align";
                const Style& source = (parent && (lowerValue == "inherit" ||
                                      (lowerValue == "unset" && inherited)))
                    ? parent->computedStyle
                    : initialStyle;
                if (prop.name == "all") {
                    auto customProperties = std::move(computedStyle.customProperties);
                    computedStyle = source;
                    computedStyle.customProperties = std::move(customProperties);
                    continue;
                }
                if (prop.name == "color") { computedStyle.color = source.color; computedStyle.hasColor = true; continue; }
                if (prop.name == "font-size") { computedStyle.fontSize = source.fontSize; computedStyle.hasFontSize = true; continue; }
                if (prop.name == "font-weight") { computedStyle.fontWeight = source.fontWeight; computedStyle.hasFontWeight = true; continue; }
                if (prop.name == "font-style") { computedStyle.fontStyle = source.fontStyle; computedStyle.hasFontStyle = true; continue; }
                if (prop.name == "font-family") { computedStyle.fontFamily = source.fontFamily; computedStyle.hasFontFamily = true; continue; }
                if (prop.name == "line-height") { computedStyle.lineHeight = source.lineHeight; computedStyle.hasLineHeight = true; continue; }
                if (prop.name == "text-align") { computedStyle.textAlign = source.textAlign; computedStyle.hasTextAlign = true; continue; }
                if (prop.name == "vertical-align") { computedStyle.verticalAlign = source.verticalAlign; computedStyle.hasVerticalAlign = source.hasVerticalAlign; continue; }
                if (prop.name == "display") { computedStyle.display = source.display; continue; }
                if (prop.name == "position") { computedStyle.position = source.position; continue; }
                if (prop.name == "opacity") { computedStyle.opacity = source.opacity; continue; }
                if (prop.name == "filter") {
                    computedStyle.filterOperations = source.filterOperations;
                    computedStyle.hasFilter = source.hasFilter;
                    continue;
                }
                if (prop.name == "backdrop-filter") {
                    computedStyle.backdropFilterOperations = source.backdropFilterOperations;
                    computedStyle.hasBackdropFilter = source.hasBackdropFilter;
                    computedStyle.backdropFilterBlur = source.backdropFilterBlur;
                    computedStyle.hasBackdropFilterBlur = source.hasBackdropFilterBlur;
                    continue;
                }
                if (prop.name == "margin") { computedStyle.margin = source.margin; continue; }
                if (prop.name == "padding") { computedStyle.padding = source.padding; continue; }
                if (prop.name == "background" || prop.name == "background-color") {
                    computedStyle.backgroundColor = source.backgroundColor;
                    computedStyle.backgroundGradient = source.backgroundGradient;
                    continue;
                }
                if (prop.name == "border") { computedStyle.border = source.border; continue; }
                if (prop.name == "object-fit") {
                    computedStyle.objectFit = source.objectFit;
                    computedStyle.hasObjectFit = source.hasObjectFit;
                    continue;
                }
                if (prop.name == "object-position") {
                    computedStyle.objectPosition = source.objectPosition;
                    computedStyle.objectPositionOffset = source.objectPositionOffset;
                    computedStyle.hasObjectPosition = source.hasObjectPosition;
                    continue;
                }
            }
            StyleSheet::mergeProperty(computedStyle,
                                      prop.name,
                                      value);
        }

        computedStyle.resolveLogicalProperties();

        // Resolve dynamic color/gradient properties using the active custom properties and parent/viewport styles
        if (!computedStyle.unresolvedColor.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.customProperties, &valid);
            if (valid) {
                computedStyle.color = StyleSheet::parseColor(resolved);
                computedStyle.hasColor = true;
            }
        }
        if (!computedStyle.unresolvedBackgroundColor.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.customProperties, &valid);
            if (valid) {
                computedStyle.backgroundColor = StyleSheet::parseColor(resolved);
            }
        }
        if (!computedStyle.unresolvedBackgroundGradient.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.customProperties, &valid);
            if (valid && resolved.find("linear-gradient") != std::string::npos) {
                computedStyle.backgroundGradient = StyleSheet::parseGradient(resolved);
            }
        }
        if (!computedStyle.unresolvedBorderColor.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.customProperties, &valid);
            if (valid) {
                computedStyle.border.color = StyleSheet::parseColor(resolved);
            }
        }

        // Evaluate hover, focus, active custom properties
        // Hover state:
        {
            if (!computedStyle.unresolvedColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.hoverCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.color) {
                        computedStyle.hoverColor = c;
                        computedStyle.hasHoverColor = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.hoverCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.backgroundColor) {
                        computedStyle.hoverBackgroundColor = c;
                        computedStyle.hasHoverBg = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundGradient.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.hoverCustomProperties, &valid);
                if (valid && resolved.find("linear-gradient") != std::string::npos) {
                    computedStyle.hoverBackgroundGradient = StyleSheet::parseGradient(resolved);
                    computedStyle.hasHoverGradient = true;
                }
            }
            if (!computedStyle.unresolvedBorderColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.hoverCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.border.color) {
                        computedStyle.hoverBorderColor = c;
                        computedStyle.hasHoverBorder = true;
                    }
                }
            }
        }

        // Focus state:
        {
            if (!computedStyle.unresolvedColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.focusCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.color) {
                        computedStyle.focusColor = c;
                        computedStyle.hasFocusColor = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.focusCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.backgroundColor) {
                        computedStyle.focusBackgroundColor = c;
                        computedStyle.hasFocusBg = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundGradient.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.focusCustomProperties, &valid);
                if (valid && resolved.find("linear-gradient") != std::string::npos) {
                    computedStyle.focusBackgroundGradient = StyleSheet::parseGradient(resolved);
                    computedStyle.hasFocusGradient = true;
                }
            }
            if (!computedStyle.unresolvedBorderColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.focusCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.border.color) {
                        computedStyle.focusBorderColor = c;
                        computedStyle.hasFocusBorder = true;
                    }
                }
            }
        }

        // Active state:
        {
            if (!computedStyle.unresolvedColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.activeCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.color) {
                        computedStyle.activeColor = c;
                        computedStyle.hasActiveColor = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.activeCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.backgroundColor) {
                        computedStyle.activeBackgroundColor = c;
                        computedStyle.hasActiveBg = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundGradient.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.activeCustomProperties, &valid);
                if (valid && resolved.find("linear-gradient") != std::string::npos) {
                    computedStyle.activeBackgroundGradient = StyleSheet::parseGradient(resolved);
                    computedStyle.hasActiveGradient = true;
                }
            }
            if (!computedStyle.unresolvedBorderColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.activeCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.border.color) {
                        computedStyle.activeBorderColor = c;
                        computedStyle.hasActiveBorder = true;
                    }
                }
            }
        }

        uint64_t oldInheritedHash = computedStyle.inheritedHash;
        computedStyle.inheritedHash = StyleSheet::computeInheritedHash(computedStyle);
        if (computedStyle.inheritedHash != oldInheritedHash) {
            for (auto& child : children) {
                child->styleDirty = true;
                child->markSubtreeStyleDirty();
            }
        }
        
        size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
        if (nextLayoutSignature != layoutSignature) {
            layoutSignature = nextLayoutSignature;
            markLayoutDirty();
        }
        styleDirty = false;
        lastInlinePropertyEpoch = inlinePropertyEpoch;
        lastInlinePropertyCount = inlineProperties.size();
        
        // ::before — only resolve if the stylesheet has ::before rules, or if a
        // before node already exists (so it can be torn down when rules are removed).
        if (sheet.hasBeforeRules() || beforePseudoNode) {
            Style beforeStyle = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "before");
            beforeStyle.resolveLogicalProperties();
            if (!beforeStyle.content.empty()) {
                if (!beforePseudoNode) {
                    beforePseudoNode = std::make_shared<Text>(beforeStyle.content, "");
                    beforePseudoNode->parent = this;
                    beforePseudoNode->type = "pseudo-before";
                    children.insert(children.begin(), beforePseudoNode);
                } else {
                    static_cast<Text*>(beforePseudoNode.get())->content = beforeStyle.content;
                }
                beforePseudoNode->computedStyle = beforeStyle;
                beforePseudoNode->styleDirty = false;
                beforePseudoNode->subtreeStyleDirty = true;
            } else {
                if (beforePseudoNode) {
                    children.erase(std::remove(children.begin(), children.end(), beforePseudoNode), children.end());
                    beforePseudoNode.reset();
                }
            }
        }

        // ::after — same fast-path gating as ::before.
        if (sheet.hasAfterRules() || afterPseudoNode) {
            Style afterStyle = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "after");
            afterStyle.resolveLogicalProperties();
            if (!afterStyle.content.empty()) {
                if (!afterPseudoNode) {
                    afterPseudoNode = std::make_shared<Text>(afterStyle.content, "");
                    afterPseudoNode->parent = this;
                    afterPseudoNode->type = "pseudo-after";
                    children.push_back(afterPseudoNode);
                } else {
                    static_cast<Text*>(afterPseudoNode.get())->content = afterStyle.content;
                }
                afterPseudoNode->computedStyle = afterStyle;
                afterPseudoNode->styleDirty = false;
                afterPseudoNode->subtreeStyleDirty = true;
            } else {
                if (afterPseudoNode) {
                    children.erase(std::remove(children.begin(), children.end(), afterPseudoNode), children.end());
                    afterPseudoNode.reset();
                }
            }
        }

        // ── Resolve additional pseudo-element styles (Blink PseudoId parity) ──
        // Fast-path: skip entirely when the stylesheet has no such rules.
        // ::placeholder — for input/textarea placeholder text color/font
        if (sheet.hasPlaceholderRules()) {
            Style ps = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "placeholder");
            if (ps.hasColor || ps.hasFontSize || ps.hasFontWeight || ps.opacity != 1.0f) {
                if (!placeholderStyle) placeholderStyle = std::make_unique<Style>();
                *placeholderStyle = std::move(ps);
                hasPlaceholderStyle = true;
            } else {
                hasPlaceholderStyle = false;
            }
        } else {
            hasPlaceholderStyle = false;
        }
        // ::selection — selection highlight color/background
        if (sheet.hasSelectionRules()) {
            Style ss = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "selection");
            if (ss.hasColor || ss.backgroundColor.a > 0) {
                if (!selectionStyle) selectionStyle = std::make_unique<Style>();
                *selectionStyle = std::move(ss);
                hasSelectionStyle = true;
            } else {
                hasSelectionStyle = false;
            }
        } else {
            hasSelectionStyle = false;
        }
        // ::marker — list-item marker color/font
        if (sheet.hasMarkerRules()) {
            Style ms = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "marker");
            if (ms.hasColor || ms.hasFontSize || ms.hasFontWeight) {
                if (!markerStyle) markerStyle = std::make_unique<Style>();
                *markerStyle = std::move(ms);
                hasMarkerStyle = true;
            } else {
                hasMarkerStyle = false;
            }
        } else {
            hasMarkerStyle = false;
        }

        lastResolveKey = currentKey;
        lastStyleSheetEpoch = sheet.getEpoch();
        hasLastResolveKey = true;
        styleDirty = false;
        }
    }

    // Inline C++ style overrides: Programmatic style changes set in C++ should always override the matched stylesheet styles
    Style& computedStyle = this->computedStyle.ensureMutable();
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
    if (style.overflow != Overflow::Visible) computedStyle.overflow = style.overflow;
    if (style.overflowX != Overflow::Visible) computedStyle.overflowX = style.overflowX;
    if (style.overflowY != Overflow::Visible) computedStyle.overflowY = style.overflowY;
    if (style.fontSize > 0 && style.fontSize != 14.0f) computedStyle.fontSize = style.fontSize;
    if (style.hasFontStyle) {
        computedStyle.fontStyle = style.fontStyle;
        computedStyle.hasFontStyle = true;
    }
    if (style.padding.top > 0 || style.padding.right > 0 ||
        style.padding.bottom > 0 || style.padding.left > 0)
        computedStyle.padding = style.padding;
    if (style.margin.top > 0 || style.margin.right > 0 ||
        style.margin.bottom > 0 || style.margin.left > 0)
        computedStyle.margin = style.margin;
    if (style.gap > 0) computedStyle.gap = style.gap;
    if (style.rowGap > 0) computedStyle.rowGap = style.rowGap;
    if (style.columnGap > 0) computedStyle.columnGap = style.columnGap;
    if (style.columnCount > 0) computedStyle.columnCount = style.columnCount;
    if (style.columnWidth > 0.0f) computedStyle.columnWidth = style.columnWidth;
    if (style.aspectRatio > 0) computedStyle.aspectRatio = style.aspectRatio;
    if (style.hasObjectFit) {
        computedStyle.objectFit = style.objectFit;
        computedStyle.hasObjectFit = true;
    }
    if (style.hasAppearance) {
        computedStyle.appearance = style.appearance;
        computedStyle.hasAppearance = true;
    }
    if (style.hasObjectPosition) {
        computedStyle.objectPosition = style.objectPosition;
        computedStyle.objectPositionOffset = style.objectPositionOffset;
        computedStyle.hasObjectPosition = true;
    }
    if (style.hasVerticalAlign) {
        computedStyle.verticalAlign = style.verticalAlign;
        computedStyle.hasVerticalAlign = true;
    }
    if (style.hasBoxSizing || style.boxSizing != BoxSizing::ContentBox) {
        computedStyle.boxSizing = style.boxSizing;
        computedStyle.hasBoxSizing = true;
    }
    if (style.flexGrow > 0) computedStyle.flexGrow = style.flexGrow;
    if (style.flexBasis.isSet()) computedStyle.flexBasis = style.flexBasis;
    if (style.borderRadius.maxRadius() > 0) computedStyle.borderRadius = style.borderRadius;
    if (style.hasBackdropFilter) {
        computedStyle.backdropFilterOperations = style.backdropFilterOperations;
        computedStyle.hasBackdropFilter = true;
        computedStyle.backdropFilterBlur = style.backdropFilterBlur;
        computedStyle.hasBackdropFilterBlur = style.hasBackdropFilterBlur;
    }
    if (style.hasFilter) {
        computedStyle.filterOperations = style.filterOperations;
        computedStyle.hasFilter = true;
    }
    // Grid layout cascade (second block)
    if (!style.gridTemplateColumnTracks.empty() || !style.gridTemplateColumns.empty()) {
        computedStyle.gridTemplateColumnTracks = style.gridTemplateColumnTracks;
        computedStyle.gridTemplateColumns      = style.gridTemplateColumns;
    }
    if (!style.gridTemplateRowTracks.empty() || !style.gridTemplateRows.empty()) {
        computedStyle.gridTemplateRowTracks = style.gridTemplateRowTracks;
        computedStyle.gridTemplateRows      = style.gridTemplateRows;
    }
    if (style.hasGridTemplateAreas) {
        computedStyle.gridTemplateAreas    = style.gridTemplateAreas;
        computedStyle.hasGridTemplateAreas = true;
    }
    if (!style.gridAutoRowTracks.empty())    computedStyle.gridAutoRowTracks    = style.gridAutoRowTracks;
    if (!style.gridAutoColumnTracks.empty()) computedStyle.gridAutoColumnTracks = style.gridAutoColumnTracks;
    if (style.gridAutoFlow != GridAutoFlow::Row) computedStyle.gridAutoFlow = style.gridAutoFlow;
    if (!style.gridColumnStart.isAuto()) computedStyle.gridColumnStart = style.gridColumnStart;
    if (!style.gridColumnEnd.isAuto())   computedStyle.gridColumnEnd   = style.gridColumnEnd;
    if (!style.gridRowStart.isAuto())    computedStyle.gridRowStart    = style.gridRowStart;
    if (!style.gridRowEnd.isAuto())      computedStyle.gridRowEnd      = style.gridRowEnd;
    if (!style.gridColumn.empty())       computedStyle.gridColumn      = style.gridColumn;
    if (!style.gridRow.empty())          computedStyle.gridRow         = style.gridRow;
    if (!style.gridArea.empty())         computedStyle.gridArea        = style.gridArea;
    if (style.hasJustifyItems) {
        computedStyle.justifyItems    = style.justifyItems;
        computedStyle.hasJustifyItems = true;
    }
    if (style.hasJustifySelf) {
        computedStyle.justifySelf    = style.justifySelf;
        computedStyle.hasJustifySelf = true;
    }
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
    if (style.contain != ContainmentFlags::kContainNone) computedStyle.contain = style.contain;

    size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
    if (nextLayoutSignature != layoutSignature) {
        layoutSignature = nextLayoutSignature;
        markLayoutDirty();
    }

    for (auto& child : children) {
        if (child->subtreeStyleDirty) {
            child->resolveStyles(sheet);
        }
    }
    subtreeStyleDirty = false;
    lifecycleState = WidgetLifecycle::StyleClean;
}

} // namespace FluxUI