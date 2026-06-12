// FluxUI — internal widget helpers shared across the core/ translation units.
//
// These are implementation-detail helpers that used to live as file-local
// `static` functions inside the monolithic widgets.cpp. To let widget
// implementations be split into multiple .cpp files (video.cpp, etc.) without
// duplicating logic, the cross-cutting ones are exposed here as `inline`
// functions in the FluxUI::detail namespace (header-only, internal to the
// engine — NOT part of the public API).
//
// This header is private to fluxui/src and is never installed.
#pragma once
#include "fluxui/widgets.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace FluxUI {
namespace detail {

// ── Visibility / hit-testing gates (used by every widget's render()) ──
inline bool isDisplayNone(const Widget* widget) {
    return widget && widget->computedStyle->display == Display::None;
}
inline bool canPaintWidget(const Widget* widget) {
    return widget && widget->visible &&
           widget->computedStyle->display != Display::None &&
           widget->computedStyle->visibility == Visibility::Visible;
}
inline bool canHitTestWidget(const Widget* widget) {
    return canPaintWidget(widget) &&
           widget->computedStyle->pointerEvents != PointerEvents::None;
}
inline bool rectIntersects(const Rect& a, const Rect& b, float padding = 0.0f) {
    return a.x + a.w >= b.x - padding &&
           a.x <= b.x + b.w + padding &&
           a.y + a.h >= b.y - padding &&
           a.y <= b.y + b.h + padding;
}

// ── Shared input/widget-tree helpers (used by form controls) ──
// Normalizes SDL/POSIX scan codes to the Win32 virtual-key codes the widget
// update() handlers compare against.
inline int normalizeTextEditingKey(int keyCode) {
    switch (keyCode) {
    case 0x4000004a: return 0x24; // SDL home
    case 0x4000004d: return 0x23; // SDL end
    case 0x4000004f: return 0x27; // SDL right
    case 0x40000050: return 0x25; // SDL left
    case 0x40000051: return 0x28; // SDL down
    case 0x40000052: return 0x26; // SDL up
    case 0x4000004b: return 0x21; // SDL page up
    case 0x4000004e: return 0x22; // SDL page down
    case 0x7f:       return 0x2E; // POSIX delete
    default:         return keyCode;
    }
}
inline Widget* rootOfWidget(Widget* widget) {
    if (!widget) return nullptr;
    while (widget->parent) {
        widget = widget->parent;
    }
    return widget;
}
// Unchecks every other Radio in the same group (or, for an unnamed group, the
// same parent) when `active` becomes checked.
inline void clearRadioGroup(Widget* widget, Radio* active, const std::string& group) {
    if (!widget) return;
    if (auto* radio = dynamic_cast<Radio*>(widget)) {
        bool sameGroup = group.empty() ? (radio->parent == active->parent) : (radio->group == group);
        if (radio != active && sameGroup) {
            radio->checked = false;
            radio->markStyleDirty();
        }
    }
    for (auto& child : widget->children) {
        clearRadioGroup(child.get(), active, group);
    }
}


// ============================================================
//  Shared text / layout / paint helpers (consolidated from
//  application.cpp so concrete widget TUs can reuse them).
// ============================================================
inline bool isUtf8Continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}
inline size_t clampToUtf8Boundary(const std::string& text, size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && index < text.size() &&
           isUtf8Continuation((unsigned char)text[index])) {
        --index;
    }
    return index;
}
inline size_t previousCodepoint(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index == 0) return 0;
    --index;
    while (index > 0 && isUtf8Continuation((unsigned char)text[index])) {
        --index;
    }
    return index;
}
inline size_t nextCodepoint(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index >= text.size()) return text.size();
    ++index;
    while (index < text.size() && isUtf8Continuation((unsigned char)text[index])) {
        ++index;
    }
    return index;
}
inline bool isAsciiWordChar(char c) {
    unsigned char uc = (unsigned char)c;
    return std::isalnum(uc) || c == '_';
}
inline size_t previousWordBoundary(const std::string& text, size_t index) {
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
inline size_t nextWordBoundary(const std::string& text, size_t index) {
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
inline void wordRangeAt(const std::string& text, size_t index, size_t& start, size_t& end) {
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
inline float approximateGlyphAdvance(unsigned char c, float fontSize) {
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
inline std::string approximateLigatures(const std::string& text) {
    std::string result = text;
    const std::vector<std::string> ligs = {"ffi", "ffl", "ff", "fi", "fl", "ft", "st"};
    for (const auto& lig : ligs) {
        size_t pos = 0;
        while ((pos = result.find(lig, pos)) != std::string::npos) {
            result.replace(pos, lig.length(), "_");
            pos += 1;
        }
    }
    return result;
}

inline float approximateTextWidth(const std::string& text, float fontSize) {
    std::string temp = approximateLigatures(text);
    float width = 0.0f;
    for (size_t i = 0; i < temp.size(); ) {
        unsigned char c = (unsigned char)temp[i];
        width += approximateGlyphAdvance(c, fontSize);
        i = nextCodepoint(temp, i);
    }
    return width;
}

inline float measureTextWidthExact(const std::string& text, float fontSize, const std::string& fontName = "default") {
    if (auto* app = Application::instance()) {
        return app->renderer().measureText(text, fontSize, fontName).x;
    }
    return approximateTextWidth(text, fontSize);
}

inline std::vector<std::string> wrapText(const std::string& text, float fontSize, float maxWidth, const std::string& fontName = "default") {
    std::vector<std::string> lines;
    if (maxWidth <= 0.0f || text.empty()) {
        lines.push_back(text);
        return lines;
    }
    std::string currentLine;
    float currentWidth = 0.0f;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
            currentWidth = 0.0f;
            i++;
            continue;
        }
        size_t start = i;
        if (std::isspace((unsigned char)text[i])) {
            while (i < text.size() && text[i] != '\n' && std::isspace((unsigned char)text[i])) {
                i = nextCodepoint(text, i);
            }
        } else {
            while (i < text.size() && text[i] != '\n' && !std::isspace((unsigned char)text[i])) {
                i = nextCodepoint(text, i);
            }
        }
        std::string chunk = text.substr(start, i - start);
        float chunkWidth = measureTextWidthExact(chunk, fontSize, fontName);
        if (currentLine.empty()) {
            currentLine = chunk;
            currentWidth = chunkWidth;
        } else if (currentWidth + chunkWidth <= maxWidth) {
            currentLine += chunk;
            currentWidth += chunkWidth;
        } else {
            if (std::isspace((unsigned char)chunk[0])) {
                lines.push_back(currentLine);
                currentLine.clear();
                currentWidth = 0.0f;
            } else {
                lines.push_back(currentLine);
                currentLine = chunk;
                currentWidth = chunkWidth;
            }
        }
    }
    if (!currentLine.empty() || lines.empty()) {
        lines.push_back(currentLine);
    }
    return lines;
}

inline std::vector<std::string> splitPreservedLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    if (lines.empty()) lines.push_back("");
    return lines;
}

inline std::vector<std::string> layoutTextLines(const std::string& text,
                                                float fontSize,
                                                float maxWidth,
                                                WhiteSpace whiteSpace,
                                                const std::string& fontName = "default") {
    if (whiteSpace == WhiteSpace::Pre) {
        return splitPreservedLines(text);
    }
    if (whiteSpace == WhiteSpace::PreWrap || whiteSpace == WhiteSpace::PreLine) {
        std::vector<std::string> lines;
        for (const auto& preservedLine : splitPreservedLines(text)) {
            std::vector<std::string> wrapped = wrapText(preservedLine, fontSize, maxWidth, fontName);
            lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        }
        if (lines.empty()) lines.push_back("");
        return lines;
    }
    if (whiteSpace == WhiteSpace::NoWrap) {
        return {text};
    }
    return wrapText(text, fontSize, maxWidth, fontName);
}

inline float intrinsicTextWidth(const std::string& text,
                                float fontSize,
                                WhiteSpace whiteSpace,
                                const std::string& fontName = "default") {
    float width = 0.0f;
    if (whiteSpace == WhiteSpace::Pre || whiteSpace == WhiteSpace::PreWrap ||
        whiteSpace == WhiteSpace::PreLine) {
        for (const auto& line : splitPreservedLines(text)) {
            width = std::max(width, measureTextWidthExact(line, fontSize, fontName));
        }
    } else {
        width = measureTextWidthExact(text, fontSize, fontName);
    }
    return width;
}
inline std::string trimAsciiLocal(std::string value) {
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isWs((unsigned char)value.front())) value.erase(value.begin());
    while (!value.empty() && isWs((unsigned char)value.back())) value.pop_back();
    return value;
}
inline float parseSvgLengthLocal(const std::string& value, float fallback = 0.0f) {
    std::string s = trimAsciiLocal(value);
    if (s.empty()) return fallback;
    char* end = nullptr;
    float number = parseLocaleIndependentFloat(s.c_str(), &end);
    if (end == s.c_str()) return fallback;
    return number;
}
inline std::string attrFromTagLocal(const std::string& tag, const std::string& name) {
    size_t pos = tag.find(name);
    while (pos != std::string::npos) {
        bool leftOk = pos == 0 || std::isspace((unsigned char)tag[pos - 1]) || tag[pos - 1] == '<';
        size_t after = pos + name.size();
        bool rightOk = after < tag.size() && (std::isspace((unsigned char)tag[after]) || tag[after] == '=');
        if (leftOk && rightOk) break;
        pos = tag.find(name, pos + 1);
    }
    if (pos == std::string::npos) return {};
    size_t eq = tag.find('=', pos + name.size());
    if (eq == std::string::npos) return {};
    size_t start = eq + 1;
    while (start < tag.size() && std::isspace((unsigned char)tag[start])) ++start;
    if (start >= tag.size()) return {};
    char quote = tag[start];
    if (quote == '"' || quote == '\'') {
        size_t end = tag.find(quote, start + 1);
        if (end == std::string::npos) return {};
        return tag.substr(start + 1, end - start - 1);
    }
    size_t end = start;
    while (end < tag.size() && !std::isspace((unsigned char)tag[end]) && tag[end] != '>') ++end;
    return tag.substr(start, end - start);
}
inline bool parentUsesRowFlex(const Widget* widget) {
    if (!widget || !widget->parent) return false;
    const Style& parentStyle = widget->parent->computedStyle;
    if (parentStyle.display != Display::Flex) return false;
    return parentStyle.flexDirection == FlexDirection::Row ||
           parentStyle.flexDirection == FlexDirection::RowReverse;
}
inline size_t approximateTextIndexAtX(const std::string& text, float x, float fontSize) {
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
inline size_t passwordTextIndexAtX(const std::string& text, float x, float fontSize) {
    if (x <= 0 || text.empty()) return 0;
    float cursor = 0;
    float advance = approximateGlyphAdvance('*', fontSize);
    for (size_t i = 0; i < text.size(); ) {
        size_t current = i;
        size_t next = nextCodepoint(text, i);
        if (x < cursor + advance * 0.5f) return current;
        if (x < cursor + advance) return next;
        cursor += advance;
        i = next;
    }
    return text.size();
}
inline size_t getTextIndexAtXExact(const std::string& text, float x, float fontSize, const std::string& fontName = "default", bool isPassword = false) {
    if (x <= 0 || text.empty()) return 0;

    std::string measureTextStr;
    if (isPassword) {
        size_t cpCount = 0;
        size_t tempIdx = 0;
        while (tempIdx < text.size()) {
            tempIdx = nextCodepoint(text, tempIdx);
            cpCount++;
        }
        measureTextStr = std::string(cpCount, '*');
    } else {
        measureTextStr = text;
    }

    auto* app = Application::instance();
    if (!app) {
        if (isPassword) return passwordTextIndexAtX(text, x, fontSize);
        return approximateTextIndexAtX(text, x, fontSize);
    }

    std::vector<size_t> boundaries;
    boundaries.push_back(0);
    size_t idx = 0;
    while (idx < measureTextStr.size()) {
        idx = nextCodepoint(measureTextStr, idx);
        boundaries.push_back(idx);
    }

    size_t low = 0;
    size_t high = boundaries.size() - 1;
    size_t bestIdx = 0;
    float bestDiff = std::abs(x);

    auto getWidth = [&](size_t bIdx) -> float {
        size_t byteOffset = boundaries[bIdx];
        std::string prefix = measureTextStr.substr(0, byteOffset);
        return app->renderer().measureText(prefix, fontSize, fontName).x;
    };

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        float cursorX = getWidth(mid);
        float diff = std::abs(x - cursorX);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = boundaries[mid];
        }

        if (cursorX < x) {
            low = mid + 1;
        } else if (cursorX > x) {
            if (mid == 0) break;
            high = mid - 1;
        } else {
            return boundaries[mid];
        }
    }

    auto it = std::find(boundaries.begin(), boundaries.end(), bestIdx);
    if (it != boundaries.end()) {
        size_t bIdx = std::distance(boundaries.begin(), it);
        if (bIdx > 0) {
            float prevX = getWidth(bIdx - 1);
            if (std::abs(x - prevX) < bestDiff) {
                bestIdx = boundaries[bIdx - 1];
                bestDiff = std::abs(x - prevX);
            }
        }
        if (bIdx + 1 < boundaries.size()) {
            float nextX = getWidth(bIdx + 1);
            if (std::abs(x - nextX) < bestDiff) {
                bestIdx = boundaries[bIdx + 1];
            }
        }
    }

    if (isPassword) {
        size_t targetCp = bestIdx;
        size_t textByteIdx = 0;
        for (size_t cp = 0; cp < targetCp && textByteIdx < text.size(); ++cp) {
            textByteIdx = nextCodepoint(text, textByteIdx);
        }
        return textByteIdx;
    }

    return bestIdx;
}
inline size_t codepointCountInRange(const std::string& text, size_t start, size_t end) {
    start = clampToUtf8Boundary(text, start);
    end = clampToUtf8Boundary(text, std::min(end, text.size()));
    if (end < start) std::swap(start, end);
    size_t count = 0;
    for (size_t i = start; i < end; i = nextCodepoint(text, i)) {
        ++count;
    }
    return count;
}
inline std::string maskedPasswordRange(const std::string& text, size_t start, size_t end) {
    return std::string(codepointCountInRange(text, start, end), '*');
}
inline std::string inputVisibleRange(TextInputType type,
                                     const std::string& text,
                                     size_t start,
                                     size_t end) {
    if (type == TextInputType::Password) {
        return maskedPasswordRange(text, start, end);
    }
    start = clampToUtf8Boundary(text, start);
    end = clampToUtf8Boundary(text, std::min(end, text.size()));
    if (end < start) std::swap(start, end);
    return text.substr(start, end - start);
}
inline bool isTextEditingInputType(TextInputType type) {
    switch (type) {
    case TextInputType::Text:
    case TextInputType::Password:
    case TextInputType::Search:
    case TextInputType::Email:
    case TextInputType::Url:
    case TextInputType::Tel:
    case TextInputType::Number:
    case TextInputType::Date:
    case TextInputType::Time:
    case TextInputType::Month:
    case TextInputType::Week:
    case TextInputType::DateTimeLocal:
        return true;
    default:
        return false;
    }
}
inline bool isButtonLikeInputType(TextInputType type) {
    switch (type) {
    case TextInputType::Button:
    case TextInputType::Submit:
    case TextInputType::Reset:
    case TextInputType::File:
    case TextInputType::Color:
    case TextInputType::Image:
        return true;
    default:
        return false;
    }
}
inline bool consumesParentMainAxisHeight(const Widget* widget, const Style& style) {
    if (!widget || !widget->parent || style.flexGrow <= 0.0f) return false;
    const Style& parentStyle = widget->parent->computedStyle;
    if (parentStyle.display != Display::Flex) return false;
    return parentStyle.flexDirection == FlexDirection::Column ||
           parentStyle.flexDirection == FlexDirection::ColumnReverse;
}
inline bool isOutOfFlow(const Widget* widget) {
    if (!widget) return false;
    return widget->computedStyle->position == Position::Absolute ||
           widget->computedStyle->position == Position::Fixed;
}
inline bool isInlineDisplay(Display display) {
    return display == Display::Inline || display == Display::InlineBlock;
}
inline bool isImplicitInlineText(const Widget* widget) {
    return widget && widget->type == "text" && widget->className.empty();
}
inline bool isDocumentFlowTextElement(const Widget* widget) {
    if (!widget) return false;
    const std::string& type = widget->type;
    return type == "p" || type == "h1" || type == "h2" || type == "h3" ||
           type == "h4" || type == "h5" || type == "h6" || type == "li" ||
           type == "dt" || type == "dd" || type == "legend" ||
           type == "summary" || type == "figcaption" || type == "blockquote";
}
inline bool isInlineFlowItem(const Widget* widget) {
    if (!widget || !widget->computedStyle) return false;
    return isInlineDisplay(widget->computedStyle->display) || isImplicitInlineText(widget);
}
inline bool shouldUseInlineFlow(const Widget* widget) {
    if (!widget) return false;
    for (const auto& child : widget->children) {
        if (!child || !child->visible || isDisplayNone(child.get()) ||
            isOutOfFlow(child.get())) {
            continue;
        }
        if (isInlineFlowItem(child.get())) {
            return true;
        }
    }
    return false;
}
inline bool isExpandedSelectWidget(const Widget* widget) {
    auto* select = dynamic_cast<const Select*>(widget);
    return select && select->expanded;
}
inline bool isKeyboardFocusableWidget(const Widget* widget) {
    if (!canHitTestWidget(widget)) return false;
    return widget->type == "input" ||
           widget->type == "textarea" ||
           widget->type == "button" ||
           widget->type == "checkbox" ||
           widget->type == "radio" ||
           widget->type == "range" ||
           widget->type == "select" ||
           widget->type == "summary" ||
           widget->type == "a" ||
           widget->onClick ||
           widget->computedStyle->cursor == CursorType::Pointer;
}
inline void collectKeyboardFocusableWidgets(Widget* widget, std::vector<Widget*>& out) {
    if (!widget) return;
    if (isKeyboardFocusableWidget(widget)) {
        out.push_back(widget);
    }
    for (auto& child : widget->children) {
        collectKeyboardFocusableWidgets(child.get(), out);
    }
}
inline void clearFocusRecursive(Widget* widget) {
    if (!widget) return;
    widget->focused = false;
    for (auto& child : widget->children) {
        clearFocusRecursive(child.get());
    }
}
inline Widget* focusedWidgetInSubtree(Widget* widget) {
    if (!widget) return nullptr;
    if (widget->focused) return widget;
    for (auto& child : widget->children) {
        if (Widget* focused = focusedWidgetInSubtree(child.get())) {
            return focused;
        }
    }
    return nullptr;
}
inline bool moveDocumentFocus(Widget* root, bool backwards) {
    if (!root) return false;
    std::vector<Widget*> focusables;
    collectKeyboardFocusableWidgets(root, focusables);
    if (focusables.empty()) return false;

    Widget* current = focusedWidgetInSubtree(root);
    size_t nextIndex = backwards ? focusables.size() - 1 : 0;
    if (current) {
        auto it = std::find(focusables.begin(), focusables.end(), current);
        if (it != focusables.end()) {
            size_t index = static_cast<size_t>(std::distance(focusables.begin(), it));
            nextIndex = backwards
                ? (index == 0 ? focusables.size() - 1 : index - 1)
                : (index + 1) % focusables.size();
        }
    }
    clearFocusRecursive(root);
    focusables[nextIndex]->focused = true;
    return true;
}
inline std::vector<Option*> selectOptions(Select* select) {
    std::vector<Option*> options;
    if (!select) return options;
    options.reserve(select->children.size());
    for (auto& child : select->children) {
        if (auto* option = dynamic_cast<Option*>(child.get())) {
            options.push_back(option);
        }
    }
    return options;
}
inline const char* textInputTypeSelector(TextInputType type) {
    switch (type) {
    case TextInputType::Password: return "password";
    case TextInputType::Search: return "search";
    case TextInputType::Email: return "email";
    case TextInputType::Url: return "url";
    case TextInputType::Tel: return "tel";
    case TextInputType::Number: return "number";
    case TextInputType::Hidden: return "hidden";
    case TextInputType::Button: return "button";
    case TextInputType::Submit: return "submit";
    case TextInputType::Reset: return "reset";
    case TextInputType::File: return "file";
    case TextInputType::Color: return "color";
    case TextInputType::Date: return "date";
    case TextInputType::Time: return "time";
    case TextInputType::Month: return "month";
    case TextInputType::Week: return "week";
    case TextInputType::DateTimeLocal: return "datetime-local";
    case TextInputType::Image: return "image";
    case TextInputType::Text:
    default: return "text";
    }
}

inline const std::string& widgetSelectorType(const Widget* widget) {
    if (!widget) {
        static const std::string empty;
        return empty;
    }
    return widget->selectorType();
}
inline bool clipsOverflow(Overflow overflow) {
    return overflow == Overflow::Hidden || overflow == Overflow::Scroll ||
           overflow == Overflow::Auto || overflow == Overflow::Clip;
}
inline bool isOverflowVisibleOrClip(Overflow overflow) {
    return overflow == Overflow::Visible || overflow == Overflow::Clip;
}
inline Overflow normalizedOverflowAxis(Overflow axis, Overflow otherAxis) {
    if (!isOverflowVisibleOrClip(otherAxis)) {
        if (axis == Overflow::Visible) return Overflow::Auto;
        if (axis == Overflow::Clip) return Overflow::Hidden;
    }
    return axis;
}
inline Overflow effectiveOverflowX(const Style& style) {
    return normalizedOverflowAxis(style.overflowX, style.overflowY);
}
inline Overflow effectiveOverflowY(const Style& style) {
    return normalizedOverflowAxis(style.overflowY, style.overflowX);
}
inline bool clipsOverflow(const Style& style) {
    return clipsOverflow(effectiveOverflowX(style)) ||
           clipsOverflow(effectiveOverflowY(style));
}
inline bool scrollsOverflowY(const Style& style, float contentHeight, float boundsHeight) {
    Overflow overflow = effectiveOverflowY(style);
    if (overflow == Overflow::Scroll) return contentHeight > boundsHeight + 1.0f;
    if (overflow == Overflow::Auto) return contentHeight > boundsHeight + 1.0f;
    return false;
}

inline float usedBorderTopWidth(const Style& style) {
    return style.hasBorderTop ? style.borderTop.width : style.border.width;
}
inline float usedBorderRightWidth(const Style& style) {
    return style.hasBorderRight ? style.borderRight.width : style.border.width;
}
inline float usedBorderBottomWidth(const Style& style) {
    return style.hasBorderBottom ? style.borderBottom.width : style.border.width;
}
inline float usedBorderLeftWidth(const Style& style) {
    return style.hasBorderLeft ? style.borderLeft.width : style.border.width;
}
inline float usedBorderHorizontal(const Style& style) {
    return usedBorderLeftWidth(style) + usedBorderRightWidth(style);
}
inline float usedBorderVertical(const Style& style) {
    return usedBorderTopWidth(style) + usedBorderBottomWidth(style);
}
inline const std::string& renderFontName(const Style& style) {
    static const std::string s_defaultFont = "default";
    return style.fontFamily.empty() ? s_defaultFont : style.fontFamily;
}
inline std::string applyTextTransform(const std::string& text, TextTransform transform) {
    if (transform == TextTransform::None || text.empty()) return text;
    std::string out = text;
    if (transform == TextTransform::Uppercase) {
        for (char& c : out) c = (char)std::toupper((unsigned char)c);
    } else if (transform == TextTransform::Lowercase) {
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
    } else if (transform == TextTransform::Capitalize) {
        bool startWord = true;
        for (char& c : out) {
            unsigned char uc = (unsigned char)c;
            if (std::isspace(uc)) {
                startWord = true;
            } else if (startWord) {
                c = (char)std::toupper(uc);
                startWord = false;
            } else {
                c = (char)std::tolower(uc);
            }
        }
    }
    return out;
}
inline void renderTextDecoration(Renderer& renderer,
                                 const std::string& text,
                                 const Rect& rect,
                                 const Color& textColor,
                                 const Style& style) {
    if (style.textDecoration == TextDecoration::None || text.empty()) return;
    std::string fontName = renderFontName(style);
    float textWidth = renderer.measureText(text, style.fontSize, fontName).x;
    float x = rect.x;
    if (style.textAlign == TextAlign::Center) {
        x = rect.x + (rect.w - textWidth) * 0.5f;
    } else if (style.textAlign == TextAlign::Right) {
        x = rect.x + rect.w - textWidth;
    }
    Color lineColor = style.hasTextDecorationColor ? style.textDecorationColor : textColor;
    // text-decoration-thickness (px) or auto (font-size based default).
    float thickness = style.rare().textDecorationThickness > 0.0f
        ? style.rare().textDecorationThickness
        : std::max(1.0f, std::round(style.fontSize / 14.0f));
    float y = rect.y + rect.h * 0.5f;
    if (style.textDecoration == TextDecoration::Underline) {
        y += style.fontSize * 0.36f;
        // text-underline-offset shifts the underline away from the baseline.
        if (style.rare().textUnderlineOffset > 0.0f) y += style.rare().textUnderlineOffset;
    } else if (style.textDecoration == TextDecoration::Overline) {
        y -= style.fontSize * 0.44f;
    }
    float w = std::max(0.0f, textWidth);
    const std::string& decoStyle = style.rare().textDecorationStyle;

    auto drawSegment = [&](float sx, float sw, float sy) {
        renderer.drawRoundedRect({sx, sy, sw, thickness}, lineColor,
                                 BorderRadius(thickness * 0.5f));
    };

    if (decoStyle == "double") {
        float gap = thickness + std::max(1.0f, thickness);
        drawSegment(x, w, y);
        drawSegment(x, w, y + gap);
    } else if (decoStyle == "dotted" || decoStyle == "dashed") {
        float seg = decoStyle == "dotted" ? thickness * 1.5f : thickness * 4.0f;
        float gap = decoStyle == "dotted" ? thickness * 1.5f : thickness * 3.0f;
        for (float sx = x; sx < x + w; sx += seg + gap) {
            drawSegment(sx, std::min(seg, x + w - sx), y);
        }
    } else if (decoStyle == "wavy") {
        // Approximate a wave with short alternating-height segments.
        float step = std::max(2.0f, thickness * 2.0f);
        float amp = thickness;
        bool up = true;
        for (float sx = x; sx < x + w; sx += step) {
            drawSegment(sx, std::min(step, x + w - sx), y + (up ? -amp : amp) * 0.5f);
            up = !up;
        }
    } else {
        drawSegment(x, w, y);   // solid (default)
    }
}
inline std::string ellipsizeText(Renderer& renderer,
                                 const std::string& text,
                                 float maxWidth,
                                 float fontSize,
                                 const std::string& fontName) {
    if (text.empty() || maxWidth <= 0.0f) return "";
    if (renderer.measureText(text, fontSize, fontName).x <= maxWidth) return text;
    constexpr const char* ellipsis = "...";
    float ellipsisWidth = renderer.measureText(ellipsis, fontSize, fontName).x;
    if (ellipsisWidth >= maxWidth) return "";
    size_t lo = 0;
    size_t hi = text.size();
    size_t best = 0;
    while (lo <= hi) {
        size_t mid = clampToUtf8Boundary(text, (lo + hi) / 2);
        std::string candidate = text.substr(0, mid) + ellipsis;
        float width = renderer.measureText(candidate, fontSize, fontName).x;
        if (width <= maxWidth) {
            best = mid;
            lo = mid + 1;
        } else {
            if (mid == 0) break;
            hi = mid - 1;
        }
    }
    return text.substr(0, clampToUtf8Boundary(text, best)) + ellipsis;
}
inline bool rectEqual(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
inline bool intersectRects(const Rect& a, const Rect& b, Rect& out) {
    float x0 = std::max(a.x, b.x);
    float y0 = std::max(a.y, b.y);
    float x1 = std::min(a.x + a.w, b.x + b.w);
    float y1 = std::min(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        out = Rect();
        return false;
    }
    out = {x0, y0, x1 - x0, y1 - y0};
    return true;
}
inline void hashCombine(size_t& seed, size_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}
inline void hashFloat(size_t& seed, float value) {
    hashCombine(seed, std::hash<int>{}((int)std::round(value * 1000.0f)));
}
inline void hashCSSValue(size_t& seed, const CSSValue& value) {
    hashCombine(seed, std::hash<int>{}((int)value.unit));
    hashFloat(seed, value.value);
}
inline size_t layoutStyleSignature(const Style& s) {
    size_t seed = 0;
    hashCombine(seed, std::hash<int>{}((int)s.display));
    hashCombine(seed, std::hash<int>{}((int)s.flexDirection));
    hashCombine(seed, std::hash<int>{}((int)s.justifyContent));
    hashCombine(seed, std::hash<int>{}((int)s.alignItems));
    hashCombine(seed, std::hash<int>{}((int)s.overflow));
    hashCombine(seed, std::hash<int>{}((int)s.overflowX));
    hashCombine(seed, std::hash<int>{}((int)s.overflowY));
    hashCombine(seed, std::hash<int>{}((int)s.boxSizing));
    hashCombine(seed, std::hash<bool>{}(s.hasBoxSizing));
    hashFloat(seed, s.flexGrow);
    hashFloat(seed, s.flexShrink);
    hashCSSValue(seed, s.flexBasis);
    hashFloat(seed, s.gap);
    hashFloat(seed, s.rowGap);
    hashFloat(seed, s.columnGap);
    hashCombine(seed, std::hash<int>{}(s.columnCount));
    hashFloat(seed, s.columnWidth);
    hashFloat(seed, s.aspectRatio);
    hashCombine(seed, std::hash<int>{}((int)s.objectFit));
    hashFloat(seed, s.objectPosition.x);
    hashFloat(seed, s.objectPosition.y);
    hashFloat(seed, s.objectPositionOffset.x);
    hashFloat(seed, s.objectPositionOffset.y);
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
    hashCombine(seed, std::hash<int>{}((int)s.fontStyle));
    hashCombine(seed, std::hash<int>{}((int)s.textAlign));
    hashCombine(seed, std::hash<int>{}((int)s.verticalAlign));
    hashCombine(seed, std::hash<int>{}((int)s.direction));
    hashCombine(seed, std::hash<int>{}((int)s.unicodeBidi));
    return seed;
}

} // namespace detail
} // namespace FluxUI
