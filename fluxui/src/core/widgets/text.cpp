// FluxUI - text widgets: Text, Button, TextInput.
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

void Text::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    const bool hasOnlyChildContent = !children.empty() && content.empty();
    const bool shrinkToText = isInlineFlowItem(this) ||
        (s.flexGrow <= 0.0f && parentUsesRowFlex(this)) ||
        (s.display == Display::TableCell && parentBounds.w >= 9999.0f);
    if (!s.width.isSet() && shrinkToText) {
        if (hasOnlyChildContent) {
            float maxChildRight = 0.0f;
            for (const auto& child : children) {
                if (!child || !child->visible || isDisplayNone(child.get())) continue;
                Rect childMeasureArea = {0, 0, 10000.0f, 10000.0f};
                child->layout(childMeasureArea);
                maxChildRight = std::max(maxChildRight, child->bounds.w + child->computedStyle->margin.horizontal());
            }
            bounds.w = std::max(1.0f, maxChildRight + s.padding.horizontal() + usedBorderHorizontal(s));
        } else {
            bounds.w = std::max(1.0f, intrinsicTextWidth(content, s.fontSize, s.whiteSpace, renderFontName(s)) +
                s.padding.horizontal() + usedBorderHorizontal(s));
        }
    }
    if (!s.height.isSet() && !hasOnlyChildContent && (s.display != Display::TableCell || parentBounds.h >= 9999.0f)) {
        float availableW = std::max(0.0f, bounds.w - s.padding.horizontal());
        std::vector<std::string> lines = layoutTextLines(content, s.fontSize, availableW, s.whiteSpace, renderFontName(s));
        float lineCount = static_cast<float>(std::max<size_t>(1, lines.size()));
        bounds.h = std::max(1.0f, lineCount * (s.fontSize * s.lineHeight) +
            s.padding.vertical() + usedBorderVertical(s));
    } else if (!s.height.isSet() && hasOnlyChildContent && (s.display != Display::TableCell || parentBounds.h >= 9999.0f)) {
        float childBottom = bounds.y + s.padding.top;
        for (const auto& child : children) {
            if (!child || !child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) {
                continue;
            }
            const Style& cs = *(child->computedStyle);
            childBottom = std::max(childBottom, child->bounds.y + child->bounds.h + cs.margin.bottom);
        }
        bounds.h = std::max(1.0f, childBottom - bounds.y + s.padding.bottom + usedBorderVertical(s));
    }
}
void Text::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    renderListMarker(renderer);
    Color textColor = computedStyle->color;
    if (computedStyle->hasHoverColor && hoverAnim > 0) {
        textColor = Color::lerp(computedStyle->color, computedStyle->hoverColor, hoverAnim);
    }
    if (focused && computedStyle->hasFocusColor) {
        textColor = computedStyle->focusColor;
    }
    if (pressed && computedStyle->hasActiveColor) {
        textColor = computedStyle->activeColor;
    }
    Rect textRect = {
        bounds.x + computedStyle->padding.left,
        bounds.y + computedStyle->padding.top,
        std::max(0.0f, bounds.w - computedStyle->padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle->padding.vertical())
    };
    if (computedStyle->verticalAlign == VerticalAlign::Super) {
        textRect.y -= computedStyle->fontSize * 0.32f;
    } else if (computedStyle->verticalAlign == VerticalAlign::Sub) {
        textRect.y += computedStyle->fontSize * 0.22f;
    }
    const std::string* displayTextPtr = &content;
    std::string transformedText;
    if (computedStyle->textTransform != TextTransform::None && !content.empty()) {
        transformedText = applyTextTransform(content, computedStyle->textTransform);
        displayTextPtr = &transformedText;
    }
    const std::string& fontName = renderFontName(computedStyle);
    // Activate CSS text-shadow layers for this element's glyphs.
    if (computedStyle->hasTextShadow) {
        renderer.setTextShadows(computedStyle->textShadows);
    }
    if (computedStyle->whiteSpace != WhiteSpace::NoWrap && textRect.w > 0.0f) {
        std::vector<std::string> lines = layoutTextLines(*displayTextPtr,
                                                         computedStyle->fontSize,
                                                         textRect.w,
                                                         computedStyle->whiteSpace,
                                                         fontName);
        float lineHeight = computedStyle->fontSize * computedStyle->lineHeight;
        float totalTextH = lines.size() * lineHeight;
        float startY = textRect.y;
        if (!isDocumentFlowTextElement(this) && textRect.h > totalTextH) {
            startY += (textRect.h - totalTextH) / 2.0f;
        }
        for (size_t i = 0; i < lines.size(); ++i) {
            Rect lineRect = {
                textRect.x,
                startY + i * lineHeight,
                textRect.w,
                lineHeight
            };
            std::string ellipsizedText = lines[i];
            if (computedStyle->textOverflow == TextOverflow::Ellipsis && i == lines.size() - 1) {
                ellipsizedText = ellipsizeText(renderer, lines[i], textRect.w,
                                              computedStyle->fontSize, fontName);
            }
            renderer.drawTextInRect(ellipsizedText, lineRect, textColor,
                                    computedStyle->fontSize, computedStyle->textAlign,
                                    computedStyle->fontWeight, fontName,
                                    computedStyle->fontStyle,
                                    computedStyle->direction,
                                    computedStyle->unicodeBidi);
            renderTextDecoration(renderer, ellipsizedText, lineRect, textColor, computedStyle);
        }
    } else {
        std::string ellipsizedText;
        if (computedStyle->textOverflow == TextOverflow::Ellipsis) {
            ellipsizedText = ellipsizeText(renderer, *displayTextPtr, textRect.w,
                                          computedStyle->fontSize, fontName);
            displayTextPtr = &ellipsizedText;
        }
        renderer.drawTextInRect(*displayTextPtr, textRect, textColor,
                                computedStyle->fontSize, computedStyle->textAlign,
                                computedStyle->fontWeight, fontName,
                                computedStyle->fontStyle,
                                computedStyle->direction,
                                computedStyle->unicodeBidi);
        renderTextDecoration(renderer, *displayTextPtr, textRect, textColor, computedStyle);
    }
    if (computedStyle->hasTextShadow) {
        renderer.clearTextShadows();
    }
    renderChildren(renderer);
}
void Button::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    if (!s.width.isSet() && isInlineFlowItem(this)) {
        std::string fontName = renderFontName(s);
        bounds.w = std::max(16.0f, measureTextWidthExact(label, s.fontSize, fontName) +
            s.padding.horizontal() + usedBorderHorizontal(s));
    }
    if (!s.height.isSet()) {
        bounds.h = std::max(22.0f, s.fontSize * s.lineHeight +
            s.padding.vertical() + usedBorderVertical(s));
    }
}
void Button::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    bool hasScale = (renderScale != 1.0f && !layoutObject);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }
    Rect drawBounds = bounds;
    if (false) {
        // drawBounds.x += 1;
        // drawBounds.y += 1;
        // drawBounds.w -= 2;
        // drawBounds.h -= 2;
    }
    const Style& s = *computedStyle;
    if (s.boxShadow.blur > 0) {
        renderer.drawBoxShadow(drawBounds, s.boxShadow, s.borderRadius);
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
        renderer.drawRoundedRectGradient(drawBounds, bgGradient, s.borderRadius, opacity);
    } else {
        renderer.drawRoundedRect(drawBounds, bgColor, s.borderRadius, opacity);
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
    const std::string* displayLabelPtr = &label;
    std::string transformedLabel;
    if (s.textTransform != TextTransform::None && !label.empty()) {
        transformedLabel = applyTextTransform(label, s.textTransform);
        displayLabelPtr = &transformedLabel;
    }
    const std::string& fontName = renderFontName(s);
    std::string ellipsizedLabel;
    if (s.textOverflow == TextOverflow::Ellipsis) {
        ellipsizedLabel = ellipsizeText(renderer, *displayLabelPtr, textRect.w, s.fontSize, fontName);
        displayLabelPtr = &ellipsizedLabel;
    }
    renderer.drawTextInRect(*displayLabelPtr, textRect, textColor,
                            s.fontSize, s.textAlign, s.fontWeight, fontName,
                            s.fontStyle, s.direction, s.unicodeBidi);
    renderTextDecoration(renderer, *displayLabelPtr, textRect, textColor, s);
    renderChildren(renderer);
    if (hasScale) {
        renderer.popScale();
    }
}
void TextInput::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (inputType == TextInputType::Hidden) {
        bounds.w = 0.0f;
        bounds.h = 0.0f;
        contentHeight = 0.0f;
        return;
    }
    const Style& s = *computedStyle;
    if (!s.height.isSet()) {
        float browserMinHeight = type == "textarea" ? 54.0f : 20.0f;
        if (inputType == TextInputType::Color) browserMinHeight = 23.0f;
        bounds.h = std::max(browserMinHeight, s.fontSize * s.lineHeight +
            s.padding.vertical() + usedBorderVertical(s));
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
        bounds.x + bounds.w - computedStyle->padding.right - size - 8.0f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
}
TextInput* TextInput::setInputType(TextInputType kind) {
    if (inputType == kind) return this;
    inputType = kind;
    clearHovered_ = false;
    clearPressed_ = false;
    selecting_ = false;
    markStyleDirty();
    return this;
}
TextInput* TextInput::setInputType(const std::string& kind) {
    std::string lower = kind;
    for (char& c : lower) {
        c = (char)std::tolower((unsigned char)c);
    }
    if (lower == "password") return setInputType(TextInputType::Password);
    if (lower == "search") return setInputType(TextInputType::Search);
    if (lower == "email") return setInputType(TextInputType::Email);
    if (lower == "url") return setInputType(TextInputType::Url);
    if (lower == "tel" || lower == "telephone") return setInputType(TextInputType::Tel);
    if (lower == "number") return setInputType(TextInputType::Number);
    if (lower == "hidden") return setInputType(TextInputType::Hidden);
    if (lower == "button") return setInputType(TextInputType::Button);
    if (lower == "submit") return setInputType(TextInputType::Submit);
    if (lower == "reset") return setInputType(TextInputType::Reset);
    if (lower == "file") return setInputType(TextInputType::File);
    if (lower == "color") return setInputType(TextInputType::Color);
    if (lower == "date") return setInputType(TextInputType::Date);
    if (lower == "time") return setInputType(TextInputType::Time);
    if (lower == "month") return setInputType(TextInputType::Month);
    if (lower == "week") return setInputType(TextInputType::Week);
    if (lower == "datetime-local" || lower == "datetimelocal") {
        return setInputType(TextInputType::DateTimeLocal);
    }
    if (lower == "image") return setInputType(TextInputType::Image);
    return setInputType(TextInputType::Text);
}
void TextInput::update(const InputState& input) {
    Widget::update(input);
    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);
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
    bool textEditing = type == "textarea" || isTextEditingInputType(inputType);
    if (inputType == TextInputType::Hidden || !textEditing) {
        clearHovered_ = false;
        clearPressed_ = false;
        selecting_ = false;
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
        auto spawnPicker = [&]() {
#ifdef _WIN32
            // Initialize COM to ensure shell extensions and file dialogs work correctly
            HRESULT hrCoInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

            if (inputType == TextInputType::Color) {
                CHOOSECOLORA cc;
                static COLORREF acrCustClr[16];
                HWND hwnd = NULL;
                if (auto* app = Application::instance()) {
                    hwnd = (HWND)app->getWindowHandle();
                }
                ZeroMemory(&cc, sizeof(cc));
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hwnd;
                Color initColor = value.empty() ? Color(0, 0, 0, 1) : Color::fromHex(value);
                cc.rgbResult = RGB(
                    (int)(std::clamp(initColor.r, 0.0f, 1.0f) * 255.0f),
                    (int)(std::clamp(initColor.g, 0.0f, 1.0f) * 255.0f),
                    (int)(std::clamp(initColor.b, 0.0f, 1.0f) * 255.0f)
                );
                cc.lpCustColors = (LPDWORD) acrCustClr;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorA(&cc) == TRUE) {
                    char hex[8];
                    sprintf_s(hex, "#%02x%02x%02x", 
                              GetRValue(cc.rgbResult), 
                              GetGValue(cc.rgbResult), 
                              GetBValue(cc.rgbResult));
                    value = hex;
                    markStyleDirtyRecursive();
                    if (auto* app = Application::instance()) {
                        app->requestRedraw();
                    }
                } else {
                    DWORD err = CommDlgExtendedError();
                    if (err != 0) {
                        std::cerr << "[FluxUI Error] ChooseColorA failed with extended error: 0x" << std::hex << err << std::dec << std::endl;
                    }
                }
            } else if (inputType == TextInputType::File) {
                OPENFILENAMEA ofn;
                char szFile[260] = {0};
                HWND hwnd = NULL;
                if (auto* app = Application::instance()) {
                    hwnd = (HWND)app->getWindowHandle();
                }
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = "All Files\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn) == TRUE) {
                    value = szFile;
                    markStyleDirtyRecursive();
                    if (auto* app = Application::instance()) {
                        app->requestRedraw();
                    }
                } else {
                    DWORD err = CommDlgExtendedError();
                    if (err != 0) {
                        std::cerr << "[FluxUI Error] GetOpenFileNameA failed with extended error: 0x" << std::hex << err << std::dec << std::endl;
                    }
                }
            }

            if (SUCCEEDED(hrCoInit)) {
                CoUninitialize();
            }
#endif
        };
        if (hovered && input.mouseClicked[0]) {
            focused = true;
            spawnPicker();
        } else if (!hovered && input.mouseClicked[0]) {
            focused = false;
        }
        int keyCode = normalizeTextEditingKey(input.keyCode);
        bool keyboardActivate = focused && isButtonLikeInputType(inputType) &&
            (keyCode == 0x0D || keyCode == 0x20) &&
            (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
        if (keyboardActivate) {
            pressed = true;
            if (onClick) onClick();
            spawnPicker();
        }
        updateFocusAnimation();
        return;
    }
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
        std::string fontName = renderFontName(*computedStyle);
        if (inputType == TextInputType::Password) {
            return getTextIndexAtXExact(value, localX, computedStyle->fontSize, fontName, true);
        }
        return getTextIndexAtXExact(value, localX, computedStyle->fontSize, fontName, false);
    };
    bool shift = (input.modifiers & MOD_SHIFT) != 0;
    bool ctrl = (input.modifiers & MOD_CTRL) != 0;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    int commandKey = keyCode;
    if (commandKey >= 'a' && commandKey <= 'z') {
        commandKey = commandKey - 'a' + 'A';
    }
    bool canShowClear = inputType == TextInputType::Search && !value.empty();
    Rect clearRect = clearButtonRect();
    clearHovered_ = canShowClear && clearRect.contains(input.mousePos);
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
        case 0x08:
            if (!eraseSelection() && caretIndex_ > 0) {
                size_t prev = ctrl ? previousWordBoundary(value, caretIndex_) :
                    previousCodepoint(value, caretIndex_);
                value.erase(prev, caretIndex_ - prev);
                setCaret(prev, false);
            }
            return;
        case 0x2E:
            if (!eraseSelection() && caretIndex_ < value.size()) {
                size_t next = ctrl ? nextWordBoundary(value, caretIndex_) :
                    nextCodepoint(value, caretIndex_);
                value.erase(caretIndex_, next - caretIndex_);
                setCaret(caretIndex_, false);
            }
            return;
        case 0x25:
        {
            size_t target = hasSelection() && !shift ? selectionStart() :
                (ctrl ? previousWordBoundary(value, caretIndex_) : previousCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x27:
        {
            size_t target = hasSelection() && !shift ? selectionEnd() :
                (ctrl ? nextWordBoundary(value, caretIndex_) : nextCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x24:
            setCaret(0, shift);
            return;
        case 0x23:
            setCaret(value.size(), shift);
            return;
        case 0x1B:
            focused = false;
            selecting_ = false;
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
            return;
        case 0x0D:
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
    if (!canHitTestWidget(this) || !bounds.contains(point)) {
        return CursorType::Default;
    }
    if (inputType == TextInputType::Hidden) {
        return CursorType::Default;
    }
    if (inputType == TextInputType::Search && !value.empty() && clearButtonRect().contains(point)) {
        return CursorType::Pointer;
    }
    if (type != "textarea" && !isTextEditingInputType(inputType)) {
        return inputType == TextInputType::Image ? CursorType::Pointer : CursorType::Default;
    }
    return CursorType::Text;
}
void TextInput::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    if (inputType == TextInputType::Hidden) return;
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
    bool textEditing = type == "textarea" || isTextEditingInputType(inputType);
    if (!textEditing) {
        std::string fontName = renderFontName(s);
        Color textColor = s.color;
        if (inputType == TextInputType::Color) {
            Color swatch = value.empty() ? Color(0.0f, 0.0f, 0.0f, 1.0f) : Color::fromHex(value);
            Rect swatchRect = {
                bounds.x + std::max(2.0f, s.padding.left + 2.0f),
                bounds.y + std::max(3.0f, s.padding.top + 3.0f),
                std::max(0.0f, bounds.w - s.padding.horizontal() - 8.0f),
                std::max(0.0f, bounds.h - s.padding.vertical() - 8.0f)
            };
            renderer.drawRoundedRect(swatchRect, swatch, BorderRadius(0.0f));
            renderer.drawBorder(swatchRect,
                                Border(1.0f, Color(0.466f, 0.466f, 0.466f, 1.0f)),
                                BorderRadius(0.0f));
        } else if (inputType == TextInputType::File) {
            const std::string chooseText = placeholder.empty() ? "Choose File" : placeholder;
            const std::string fileText = value.empty() ? "No file chosen" : value;
            float buttonW = std::min(std::max(84.0f,
                renderer.measureText(chooseText, s.fontSize, fontName).x + 22.0f),
                std::max(84.0f, bounds.w * 0.62f));
            Rect buttonRect = {
                bounds.x + s.padding.left,
                bounds.y + s.padding.top,
                std::max(0.0f, buttonW),
                std::max(0.0f, bounds.h - s.padding.vertical())
            };
            renderer.drawRoundedRect(buttonRect,
                                     pressed ? Color(0.82f, 0.82f, 0.82f, 1.0f)
                                             : Color(0.94f, 0.94f, 0.94f, 1.0f),
                                     BorderRadius(2.0f));
            renderer.drawBorder(buttonRect,
                                Border(2.0f, Color(0.46f, 0.46f, 0.46f, 1.0f)),
                                BorderRadius(2.0f));
            renderer.drawTextInRect(chooseText, buttonRect, Color(0, 0, 0, 1),
                                    s.fontSize, TextAlign::Center, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
            Rect labelRect = {
                buttonRect.x + buttonRect.w + 6.0f,
                bounds.y,
                std::max(0.0f, bounds.x + bounds.w - (buttonRect.x + buttonRect.w + 6.0f)),
                bounds.h
            };
            renderer.drawTextInRect(fileText, labelRect, textColor,
                                    s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
        } else {
            std::string label = value.empty() ? placeholder : value;
            if (label.empty()) {
                if (inputType == TextInputType::Submit) label = "Submit";
                else if (inputType == TextInputType::Reset) label = "Reset";
            }
            Rect textRect = {
                bounds.x + s.padding.left,
                bounds.y + s.padding.top,
                std::max(0.0f, bounds.w - s.padding.horizontal()),
                std::max(0.0f, bounds.h - s.padding.vertical())
            };
            renderer.drawTextInRect(label, textRect, textColor,
                                    s.fontSize, TextAlign::Center, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
        }
        renderChildren(renderer);
        return;
    }
    const bool passwordMode = inputType == TextInputType::Password;
    std::string visibleValue = inputVisibleRange(inputType, value, 0, value.size());
    auto visiblePrefix = [&](size_t index) {
        return inputVisibleRange(inputType, value, 0, index);
    };
    auto visibleRange = [&](size_t start, size_t end) {
        return inputVisibleRange(inputType, value, start, end);
    };
    const bool canShowClear = inputType == TextInputType::Search && !value.empty();
    float clearSpace = canShowClear ? 28.0f : 0.0f;
    Rect clipRect = {
        bounds.x + s.padding.left,
        bounds.y,
        std::max(0.0f, bounds.w - s.padding.horizontal() - clearSpace),
        bounds.h
    };
    std::string fontName = renderFontName(s);
    if (value.empty()) {
        scrollX_ = 0;
    } else {
        float caretX = renderer.measureText(visiblePrefix(caretIndex_), s.fontSize, fontName).x;
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
        float startX = renderer.measureText(visiblePrefix(start), s.fontSize, fontName).x - scrollX_;
        float width = renderer.measureText(visibleRange(start, end), s.fontSize, fontName).x;
        float selH = std::min(bounds.h - 10.0f, s.fontSize + 10.0f);
        Rect selectionRect = {
            clipRect.x + startX,
            bounds.y + (bounds.h - selH) * 0.5f,
            width,
            selH
        };
        renderer.drawRoundedRect(selectionRect, Color(0.54f, 0.70f, 0.98f, 0.56f), BorderRadius(2));
    }
    const std::string* displayTextPtr = value.empty() ? &placeholder : &visibleValue;
    std::string transformedText;
    if (s.textTransform != TextTransform::None && !passwordMode && !displayTextPtr->empty()) {
        transformedText = applyTextTransform(*displayTextPtr, s.textTransform);
        displayTextPtr = &transformedText;
    }
    float bgLum = s.backgroundColor.r * 0.2126f +
                  s.backgroundColor.g * 0.7152f +
                  s.backgroundColor.b * 0.0722f;
    Color placeholderColor = hasPlaceholderStyle && placeholderStyle->hasColor
        ? placeholderStyle->color
        : (bgLum > 0.45f
            ? Color(0.459f, 0.459f, 0.459f, 1.0f)
            : Color(0.604f, 0.627f, 0.659f, 0.92f));
    Color textColor = value.empty() ? placeholderColor : s.color;
    Rect textRect = {
        clipRect.x - scrollX_,
        bounds.y,
        std::max(clipRect.w + scrollX_, renderer.measureText(*displayTextPtr, s.fontSize, fontName).x + 8.0f),
        bounds.h
    };
    renderer.drawTextInRect(*displayTextPtr, textRect, textColor,
                            s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                            s.fontStyle, s.direction, s.unicodeBidi);
    renderTextDecoration(renderer, *displayTextPtr, textRect, textColor, s);
    if (focused) {
        float caretX = clipRect.x + renderer.measureText(visiblePrefix(caretIndex_), s.fontSize, fontName).x - scrollX_;
        float cursorH = std::min(bounds.h - 12.0f, s.fontSize + 8.0f);
        float cursorY = bounds.y + (bounds.h - cursorH) * 0.5f;
        float blink = std::fmod(caretBlinkTime_, 1.0f);
        if ((blink < 0.55f || selecting_) && caretX >= clipRect.x - 1 && caretX <= clipRect.x + clipRect.w + 1) {
            renderer.drawRoundedRect({caretX, cursorY, 1.5f, cursorH},
                                     Color(0.54f, 0.70f, 0.98f, 1.0f), BorderRadius(1));
        }
    }
    renderer.popScissor();
    if (canShowClear) {
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


} // namespace FluxUI