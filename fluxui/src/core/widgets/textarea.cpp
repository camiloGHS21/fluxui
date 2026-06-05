// FluxUI - multi-line text editor: TextArea.
// Extracted from core/application.cpp (via elements.cpp). Shared text/layout/
// paint helpers live in widget_internal.h (FluxUI::detail).
#include "fluxui/widgets.h"
#include "../widget_internal.h"
#include "fluxui/platform.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <objbase.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

void TextArea::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    std::string fontName = renderFontName(s);
    float charW = measureTextWidthExact("m", s.fontSize, fontName);
    float lineH = s.fontSize * 1.2f;
    
    float minHeight = bounds.h;
    if (!s.height.isSet()) {
        minHeight = rows * lineH + s.padding.vertical() + s.margin.vertical();
    }
    
    if (!s.width.isSet()) {
        bounds.w = cols * charW + s.padding.horizontal() + s.margin.horizontal();
    }
    
    float clipW = std::max(0.0f, bounds.w - s.padding.horizontal());
    auto lines = layoutLines(s.fontSize, clipW, fontName);
    contentHeight = lines.size() * lineH + s.padding.vertical();
    
    float requiredHeight = contentHeight + s.margin.vertical();
    if (s.height.isSet()) {
        bounds.h = minHeight;
    } else {
        bounds.h = std::max(minHeight, requiredHeight);
    }
}

bool TextArea::isOverResizeHandle(Vec2 point) const {
    float size = 14.0f;
    Rect handle = {
        bounds.x + bounds.w - size,
        bounds.y + bounds.h - size,
        size,
        size
    };
    return point.x >= handle.x && point.x <= handle.x + handle.w &&
           point.y >= handle.y && point.y <= handle.y + handle.h;
}

bool TextArea::hasSelection() const {
    return selectionAnchor_ != selectionFocus_;
}

size_t TextArea::selectionStart() const {
    return std::min(selectionAnchor_, selectionFocus_);
}

size_t TextArea::selectionEnd() const {
    return std::max(selectionAnchor_, selectionFocus_);
}

std::vector<TextArea::LineInfo> TextArea::layoutLines(float fontSize, float maxWidth, const std::string& fontName) const {
    std::vector<LineInfo> lines;
    if (value.empty()) {
        lines.push_back({0, 0, 0.0f});
        return lines;
    }
    size_t start = 0;
    while (start < value.size()) {
        size_t nextNewline = value.find('\n', start);
        size_t paragraphEnd = (nextNewline == std::string::npos) ? value.size() : nextNewline;
        if (wrap && maxWidth > 0) {
            size_t curr = start;
            while (curr < paragraphEnd) {
                size_t step = curr;
                float accumulatedWidth = 0.0f;
                size_t lastSpace = std::string::npos;
                while (step < paragraphEnd) {
                    size_t nextCp = clampToUtf8Boundary(value, step + 1);
                    if (nextCp <= step) nextCp = step + 1;
                    
                    std::string runStr = value.substr(curr, nextCp - curr);
                    float runWidth = measureTextWidthExact(runStr, fontSize, fontName);
                    
                    if (runWidth > maxWidth) {
                        break;
                    }
                    accumulatedWidth = runWidth;
                    if (value[step] == ' ' || value[step] == '\t') {
                        lastSpace = step;
                    }
                    step = nextCp;
                }
                size_t lineEnd = step;
                if (lineEnd < paragraphEnd && lastSpace != std::string::npos && lastSpace > curr) {
                    lineEnd = lastSpace + 1;
                }
                if (lineEnd == curr) {
                    lineEnd = clampToUtf8Boundary(value, curr + 1);
                    if (lineEnd <= curr) lineEnd = curr + 1;
                }
                float finalW = measureTextWidthExact(value.substr(curr, lineEnd - curr), fontSize, fontName);
                lines.push_back({curr, lineEnd, finalW});
                curr = lineEnd;
            }
        } else {
            float finalW = measureTextWidthExact(value.substr(start, paragraphEnd - start), fontSize, fontName);
            lines.push_back({start, paragraphEnd, finalW});
        }
        start = paragraphEnd + 1;
    }
    if (!value.empty() && value.back() == '\n') {
        lines.push_back({value.size(), value.size(), 0.0f});
    }
    return lines;
}

void TextArea::getLineAndColumnOfOffset(const std::vector<LineInfo>& lines, size_t offset, size_t& outLine, size_t& outCol) const {
    outLine = 0;
    outCol = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (offset >= lines[i].start && offset <= lines[i].end) {
            outLine = i;
            outCol = offset - lines[i].start;
            return;
        }
    }
    if (!lines.empty()) {
        outLine = lines.size() - 1;
        outCol = lines.back().end - lines.back().start;
    }
}

void TextArea::update(const InputState& input) {
    Widget::update(input);
    
    auto updateFocusAnimation = [&]() {
        float focusTarget = focused ? 1.0f : 0.0f;
        float focusSpeed = 16.0f;
        if (focusAnim_ < focusTarget) {
            focusAnim_ = std::min(focusAnim_ + input.deltaTime * focusSpeed, focusTarget);
        }
        if (focusAnim_ > focusTarget) {
            focusAnim_ = std::max(focusAnim_ - input.deltaTime * focusSpeed, focusTarget);
        }
    };

    if (resizing_) {
        if (input.mouseDown[0]) {
            float minW = 50.0f;
            float minH = 30.0f;
            float maxW = 9999.0f;
            float maxH = 9999.0f;
            if (computedStyle->minWidth.isSet()) {
                minW = std::max(minW, computedStyle->minWidth.resolve(bounds.w));
            }
            if (computedStyle->minHeight.isSet()) {
                minH = std::max(minH, computedStyle->minHeight.resolve(bounds.h));
            }
            if (computedStyle->maxWidth.isSet()) {
                maxW = computedStyle->maxWidth.resolve(bounds.w);
            }
            if (computedStyle->maxHeight.isSet()) {
                maxH = computedStyle->maxHeight.resolve(bounds.h);
            }
            float newWidth = resizeStartSize_.x + (input.mousePos.x - resizeStartMousePos_.x);
            float newHeight = resizeStartSize_.y + (input.mousePos.y - resizeStartMousePos_.y);
            newWidth = std::clamp(newWidth, minW, maxW);
            newHeight = std::clamp(newHeight, minH, maxH);
            style.width = CSSValue::px(newWidth);
            style.height = CSSValue::px(newHeight);
            markStyleDirty();
        } else {
            resizing_ = false;
        }
        if (input.mouseReleased[0]) {
            resizing_ = false;
        }
        updateFocusAnimation();
        return;
    }
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
        float localX = input.mousePos.x - bounds.x - computedStyle->padding.left + scrollX_;
        float localY = input.mousePos.y - bounds.y - computedStyle->padding.top + scrollY;
        float lineHeight = computedStyle->fontSize * 1.2f;
        float maxWidth = std::max(0.0f, bounds.w - computedStyle->padding.horizontal());
        std::string fontName = renderFontName(*computedStyle);
        auto lines = layoutLines(computedStyle->fontSize, maxWidth, fontName);
        int lineIdx = static_cast<int>(localY / lineHeight);
        if (lineIdx < 0) lineIdx = 0;
        if (lineIdx >= static_cast<int>(lines.size())) lineIdx = static_cast<int>(lines.size()) - 1;
        if (lines.empty()) return static_cast<size_t>(0);
        const auto& line = lines[lineIdx];
        std::string lineStr = value.substr(line.start, line.end - line.start);
        size_t relativeIndex = getTextIndexAtXExact(lineStr, localX, computedStyle->fontSize, fontName, false);
        return line.start + relativeIndex;
    };
    
    bool shift = (input.modifiers & MOD_SHIFT) != 0;
    bool ctrl = (input.modifiers & MOD_CTRL) != 0;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    int commandKey = keyCode;
    if (commandKey >= 'a' && commandKey <= 'z') {
        commandKey = commandKey - 'a' + 'A';
    }
    
    if (hovered && input.mouseClicked[0]) {
        if (isOverResizeHandle(input.mousePos)) {
            resizing_ = true;
            resizeStartMousePos_ = input.mousePos;
            resizeStartSize_ = { bounds.w, bounds.h };
            focused = true;
            updateFocusAnimation();
            return;
        }
        focused = true;
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
    
    updateFocusAnimation();
    if (!focused) return;
    caretBlinkTime_ += input.deltaTime;
    
    if (keyCode != 0) {
        if (ctrl && commandKey == 'A') {
            selectionAnchor_ = 0;
            selectionFocus_ = value.size();
            caretIndex_ = value.size();
            caretBlinkTime_ = 0;
            return;
        }
        if (ctrl && (commandKey == 'C' || commandKey == 'X')) {
            if (hasSelection()) {
                std::string selected = value.substr(selectionStart(), selectionEnd() - selectionStart());
                Platform::setClipboardText(selected.c_str());
                if (commandKey == 'X') eraseSelection();
            }
            return;
        }
        if (ctrl && commandKey == 'V') {
            std::string clip = Platform::getClipboardText();
            if (!clip.empty()) {
                insertText(clip);
            }
            return;
        }
        
        switch (keyCode) {
        case 0x08: // Backspace
            if (!eraseSelection() && caretIndex_ > 0) {
                size_t prev = ctrl ? previousWordBoundary(value, caretIndex_) :
                    previousCodepoint(value, caretIndex_);
                value.erase(prev, caretIndex_ - prev);
                setCaret(prev, false);
            }
            return;
        case 0x2E: // Delete
            if (!eraseSelection() && caretIndex_ < value.size()) {
                size_t next = ctrl ? nextWordBoundary(value, caretIndex_) :
                    nextCodepoint(value, caretIndex_);
                value.erase(caretIndex_, next - caretIndex_);
                setCaret(caretIndex_, false);
            }
            return;
        case 0x25: // Left Arrow
        {
            size_t target = hasSelection() && !shift ? selectionStart() :
                (ctrl ? previousWordBoundary(value, caretIndex_) : previousCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x27: // Right Arrow
        {
            size_t target = hasSelection() && !shift ? selectionEnd() :
                (ctrl ? nextWordBoundary(value, caretIndex_) : nextCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x26: // Up Arrow
        {
            float maxWidth = std::max(0.0f, bounds.w - computedStyle->padding.horizontal());
            std::string fontName = renderFontName(*computedStyle);
            auto lines = layoutLines(computedStyle->fontSize, maxWidth, fontName);
            size_t currLine = 0, currCol = 0;
            getLineAndColumnOfOffset(lines, caretIndex_, currLine, currCol);
            if (currLine > 0) {
                size_t targetLine = currLine - 1;
                size_t targetCol = std::min(currCol, lines[targetLine].end - lines[targetLine].start);
                setCaret(lines[targetLine].start + targetCol, shift);
            } else {
                setCaret(0, shift);
            }
            return;
        }
        case 0x28: // Down Arrow
        {
            float maxWidth = std::max(0.0f, bounds.w - computedStyle->padding.horizontal());
            std::string fontName = renderFontName(*computedStyle);
            auto lines = layoutLines(computedStyle->fontSize, maxWidth, fontName);
            size_t currLine = 0, currCol = 0;
            getLineAndColumnOfOffset(lines, caretIndex_, currLine, currCol);
            if (currLine + 1 < lines.size()) {
                size_t targetLine = currLine + 1;
                size_t targetCol = std::min(currCol, lines[targetLine].end - lines[targetLine].start);
                setCaret(lines[targetLine].start + targetCol, shift);
            } else {
                setCaret(value.size(), shift);
            }
            return;
        }
        case 0x24: // Home
            setCaret(0, shift);
            return;
        case 0x23: // End
            setCaret(value.size(), shift);
            return;
        case 0x1B: // Escape
            focused = false;
            selecting_ = false;
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
            return;
        case 0x0D: // Enter
            insertText("\n");
            return;
        default:
            break;
        }
    }
    if (!ctrl && !input.text.empty()) {
        insertText(input.text);
    }
    float maxWidth = std::max(0.0f, bounds.w - computedStyle->padding.horizontal());
    std::string fontName = renderFontName(*computedStyle);
    auto lines = layoutLines(computedStyle->fontSize, maxWidth, fontName);
    size_t caretLine = 0, caretCol = 0;
    getLineAndColumnOfOffset(lines, caretIndex_, caretLine, caretCol);
    float lineHeight = computedStyle->fontSize * 1.2f;
    float caretY = caretLine * lineHeight;
    float clipH = std::max(0.0f, bounds.h - computedStyle->padding.vertical());
    if (caretY - scrollY > clipH - lineHeight) {
        targetScrollY = caretY - clipH + lineHeight;
        scrollY = targetScrollY;
    } else if (caretY - scrollY < 0) {
        targetScrollY = caretY;
        scrollY = targetScrollY;
    }
    float maxScroll = maxScrollY();
    targetScrollY = std::clamp(targetScrollY, 0.0f, maxScroll);
    scrollY = std::clamp(scrollY, 0.0f, maxScroll);
}

CursorType TextArea::cursorAt(Vec2 point) const {
    if (resizing_) {
        return CursorType::ResizeNWSE;
    }
    if (!canHitTestWidget(this) || !bounds.contains(point)) {
        return CursorType::Default;
    }
    if (isOverResizeHandle(point)) {
        return CursorType::ResizeNWSE;
    }
    return CursorType::Text;
}

void TextArea::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    const Style& s = *computedStyle;
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
    Rect clipRect = {
        bounds.x + s.padding.left,
        bounds.y + s.padding.top,
        std::max(0.0f, bounds.w - s.padding.horizontal()),
        std::max(0.0f, bounds.h - s.padding.vertical())
    };
    std::string fontName = renderFontName(s);
    float lineHeight = s.fontSize * 1.2f;
    auto lines = layoutLines(s.fontSize, clipRect.w, fontName);
    renderer.pushScissor(clipRect);
    if (hasSelection() && !value.empty()) {
        size_t start = selectionStart();
        size_t end = selectionEnd();
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            float lineY = bounds.y + s.padding.top + i * lineHeight - scrollY;
            if (lineY + lineHeight < bounds.y || lineY > bounds.y + bounds.h) {
                continue;
            }
            if (end > line.start && start < line.end) {
                size_t s_in_line = std::max(start, line.start);
                size_t e_in_line = std::min(end, line.end);
                std::string lineStr = value.substr(line.start, line.end - line.start);
                std::string prefix = lineStr.substr(0, s_in_line - line.start);
                std::string selectionText = lineStr.substr(s_in_line - line.start, e_in_line - s_in_line);
                float startX = renderer.measureText(prefix, s.fontSize, fontName).x;
                float width = renderer.measureText(selectionText, s.fontSize, fontName).x;
                if (e_in_line == line.end && e_in_line < end) {
                    width += renderer.measureText(" ", s.fontSize, fontName).x;
                }
                Rect selectionRect = {
                    clipRect.x + startX,
                    lineY + (lineHeight - s.fontSize) * 0.5f,
                    width,
                    s.fontSize + 2.0f
                };
                renderer.drawRoundedRect(selectionRect, Color(0.54f, 0.70f, 0.98f, 0.56f), BorderRadius(2));
            }
        }
    }
    float bgLum = s.backgroundColor.r * 0.2126f +
                  s.backgroundColor.g * 0.7152f +
                  s.backgroundColor.b * 0.0722f;
    Color placeholderColor = bgLum > 0.45f
        ? Color(0.459f, 0.459f, 0.459f, 1.0f)
        : Color(0.604f, 0.627f, 0.659f, 0.92f);
    if (value.empty()) {
        size_t start = 0;
        int lineIdx = 0;
        while (start < placeholder.size()) {
            size_t nextNewline = placeholder.find('\n', start);
            size_t lineEnd = (nextNewline == std::string::npos) ? placeholder.size() : nextNewline;
            std::string lineStr = placeholder.substr(start, lineEnd - start);
            
            float lineY = bounds.y + s.padding.top + lineIdx * lineHeight - scrollY;
            if (lineY + lineHeight >= bounds.y && lineY <= bounds.y + bounds.h) {
                Rect textRect = {
                    clipRect.x - scrollX_,
                    lineY,
                    clipRect.w + scrollX_,
                    lineHeight
                };
                renderer.drawTextInRect(lineStr, textRect, placeholderColor,
                                        s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                                        s.fontStyle, s.direction, s.unicodeBidi);
            }
            
            start = lineEnd + 1;
            lineIdx++;
        }
    } else {
        Color textColor = s.color;
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            float lineY = bounds.y + s.padding.top + i * lineHeight - scrollY;
            if (lineY + lineHeight < bounds.y || lineY > bounds.y + bounds.h) {
                continue;
            }
            std::string lineStr = value.substr(line.start, line.end - line.start);
            if (s.textTransform != TextTransform::None) {
                lineStr = applyTextTransform(lineStr, s.textTransform);
            }
            Rect textRect = {
                clipRect.x - scrollX_,
                lineY,
                std::max(clipRect.w + scrollX_, line.width + 8.0f),
                lineHeight
            };
            renderer.drawTextInRect(lineStr, textRect, textColor,
                                    s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
            renderTextDecoration(renderer, lineStr, textRect, textColor, s);
        }
    }
    if (focused) {
        size_t caretLine = 0, caretCol = 0;
        getLineAndColumnOfOffset(lines, caretIndex_, caretLine, caretCol);
        const auto& line = lines[caretLine];
        std::string lineStr = value.substr(line.start, line.end - line.start);
        std::string prefix = lineStr.substr(0, caretCol);
        float caretX = clipRect.x + renderer.measureText(prefix, s.fontSize, fontName).x - scrollX_;
        float caretY = bounds.y + s.padding.top + caretLine * lineHeight - scrollY;
        float cursorH = s.fontSize + 4.0f;
        float cursorY = caretY + (lineHeight - cursorH) * 0.5f;
        float blink = std::fmod(caretBlinkTime_, 1.0f);
        if ((blink < 0.55f || selecting_) && caretX >= clipRect.x - 1 && caretX <= clipRect.x + clipRect.w + 1 &&
            caretY + lineHeight >= clipRect.y && caretY <= clipRect.y + clipRect.h) {
            renderer.drawRoundedRect({caretX, cursorY, 1.5f, cursorH},
                                     Color(0.54f, 0.70f, 0.98f, 1.0f), BorderRadius(1));
        }
    }
    renderer.popScissor();

    // Draw Scrollbar (matching Widget::render scrollbar drawing)
    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    if (scrollable) {
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

    // Draw Resize Handle in the bottom-right corner of the whole widget bounds
    float rx = bounds.x + bounds.w;
    float ry = bounds.y + bounds.h;
    Color dotColor = Color(0.50f, 0.53f, 0.57f, 0.8f);
    renderer.drawRoundedRect({rx - 4.5f, ry - 4.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    renderer.drawRoundedRect({rx - 7.5f, ry - 4.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    renderer.drawRoundedRect({rx - 10.5f, ry - 4.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    
    renderer.drawRoundedRect({rx - 4.5f, ry - 7.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    renderer.drawRoundedRect({rx - 7.5f, ry - 7.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    
    renderer.drawRoundedRect({rx - 4.5f, ry - 10.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));

    renderChildren(renderer);
}


} // namespace FluxUI