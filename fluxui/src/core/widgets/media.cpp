// FluxUI - media + list widgets: Option, Select, Icon, Image, Canvas, VirtualList, StatCard.
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
#include <stb_image.h>

namespace FluxUI {
using namespace FluxUI::detail;

// Image-only helper: probe natural size (SVG dims / stb_image header).
static Vec2 probeImageNaturalSize(const std::string& source) {
    if (source.empty()) return {0, 0};
    std::ifstream file(source, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {0, 0};
    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0 || fileSize > 32 * 1024 * 1024) return {0, 0};
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes((size_t)fileSize);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), fileSize).good()) return {0, 0};
    std::string head(reinterpret_cast<const char*>(bytes.data()),
                     std::min<size_t>(bytes.size(), 4096));
    size_t svgPos = head.find("<svg");
    if (svgPos != std::string::npos) {
        size_t end = head.find('>', svgPos);
        std::string tag = end == std::string::npos ? head.substr(svgPos) : head.substr(svgPos, end - svgPos + 1);
        float width = parseSvgLengthLocal(attrFromTagLocal(tag, "width"), 0.0f);
        float height = parseSvgLengthLocal(attrFromTagLocal(tag, "height"), 0.0f);
        if ((width <= 0.0f || height <= 0.0f)) {
            std::istringstream viewBox(attrFromTagLocal(tag, "viewBox"));
            float x = 0, y = 0, w = 0, h = 0;
            if (viewBox >> x >> y >> w >> h) {
                if (width <= 0.0f) width = w;
                if (height <= 0.0f) height = h;
            }
        }
        if (width > 0.0f && height > 0.0f) return {width, height};
        return {300.0f, 150.0f};
    }
    int w = 0, h = 0, comp = 0;
    if (stbi_info_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &comp) && w > 0 && h > 0) {
        return {(float)w, (float)h};
    }
    return {0, 0};
}

void Option::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (!computedStyle->height.isSet()) {
        bounds.h = std::max(18.0f, computedStyle->fontSize * computedStyle->lineHeight +
            computedStyle->padding.vertical());
    }
}
void Option::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    Rect textRect = {
        bounds.x + computedStyle->padding.left,
        bounds.y + computedStyle->padding.top,
        std::max(0.0f, bounds.w - computedStyle->padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle->padding.vertical())
    };
    renderer.drawTextInRect(label, textRect, computedStyle->color,
                            computedStyle->fontSize, TextAlign::Left,
                            computedStyle->fontWeight, renderFontName(computedStyle),
                            computedStyle->fontStyle, computedStyle->direction,
                            computedStyle->unicodeBidi);
}
void Select::selectIndex(size_t index, bool notify) {
    auto options = selectOptions(this);
    if (options.empty()) {
        selectedIndex = 0;
        return;
    }
    size_t next = std::min(index, options.size() - 1);
    if (selectedIndex == next) return;
    selectedIndex = next;
    if (notify && onChange) onChange(selectedIndex, selectedValue());
}
std::string Select::selectedLabel() const {
    auto* self = const_cast<Select*>(this);
    auto options = selectOptions(self);
    if (options.empty()) return "";
    size_t index = std::min(selectedIndex, options.size() - 1);
    return options[index]->label;
}
std::string Select::selectedValue() const {
    auto* self = const_cast<Select*>(this);
    auto options = selectOptions(self);
    if (options.empty()) return "";
    size_t index = std::min(selectedIndex, options.size() - 1);
    return options[index]->value;
}
void Select::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    if (!s.width.isSet() && isInlineFlowItem(this)) {
        std::string fontName = renderFontName(s);
        float widest = measureTextWidthExact(selectedLabel(), s.fontSize, fontName);
        for (auto* option : selectOptions(this)) {
            widest = std::max(widest, measureTextWidthExact(option->label, s.fontSize, fontName));
        }
        float intrinsicW = widest + s.padding.horizontal() +
            usedBorderHorizontal(s) + 20.0f;
        if (s.minWidth.isSet()) {
            intrinsicW = std::max(intrinsicW, s.minWidth.resolve(parentBounds.w));
        }
        if (s.maxWidth.isSet()) {
            intrinsicW = std::min(intrinsicW, s.maxWidth.resolve(parentBounds.w));
        }
        bounds.w = std::max(1.0f, intrinsicW);
    } else if (!s.width.isSet() && parentUsesRowFlex(this)) {
        bounds.w = 128.0f;
    }
    if (!s.height.isSet()) {
        bounds.h = std::max(22.0f, s.fontSize * s.lineHeight +
            s.padding.vertical() + usedBorderVertical(s));
    }
}
void Select::update(const InputState& input) {
    Widget::update(input);
    auto options = selectOptions(this);
    float rowH = std::max(20.0f, computedStyle->fontSize * computedStyle->lineHeight + 5.0f);
    Rect listRect(bounds.x, bounds.y + bounds.h, bounds.w, rowH * options.size());
    bool listHovered = expanded && listRect.contains(input.mousePos);
    if (input.mouseClicked[0]) {
        if (hovered) {
            focused = true;
            expanded = !expanded;
            markStyleDirty();
        } else if (listHovered) {
            size_t index = std::min(options.size() - 1,
                static_cast<size_t>((input.mousePos.y - listRect.y) / rowH));
            selectIndex(index);
            focused = true;
            expanded = false;
            markStyleDirty();
        } else if (expanded) {
            expanded = false;
            markStyleDirty();
        }
    }
    if (!focused) return;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    if (keyCode == 0x1B) {
        expanded = false;
    } else if (keyCode == 0x20 || keyCode == 0x0D) {
        expanded = !expanded;
    } else if (!options.empty() && (keyCode == 0x28 || keyCode == 0x27)) {
        selectIndex(std::min(selectedIndex + 1, options.size() - 1));
    } else if (!options.empty() && (keyCode == 0x26 || keyCode == 0x25)) {
        selectIndex(selectedIndex == 0 ? 0 : selectedIndex - 1);
    } else if (!options.empty() && keyCode == 0x24) {
        selectIndex(0);
    } else if (!options.empty() && keyCode == 0x23) {
        selectIndex(options.size() - 1);
    }
}
void Select::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    Color baseBg = s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1);
    Color bg = baseBg;
    if (pressed || expanded) {
        bg = baseBg;
    } else if (hoverAnim > 0.0f) {
        bg = baseBg;
    }
    renderer.drawRoundedRect(bounds, bg, s.borderRadius);
    Border baseBorder = s.border.width > 0.0f ? s.border : Border(1.0f, Color::fromHex("#767676"));
    Color baseBorderColor = baseBorder.color;
    Color hoverBorderColor = Color::fromHex("#4f4f4f");
    Color activeBorderColor = Color::fromHex("#005fcc");
    Color currentBorderColor = baseBorderColor;
    if (pressed || expanded) {
        currentBorderColor = activeBorderColor;
    } else if (hoverAnim > 0.0f) {
        currentBorderColor = Color::lerp(baseBorderColor, hoverBorderColor, hoverAnim);
    }
    renderer.drawBorder(bounds, Border(baseBorder.width, currentBorderColor), s.borderRadius);
    Rect textRect = {bounds.x + s.padding.left,
                     bounds.y + s.padding.top,
                     std::max(0.0f, bounds.w - s.padding.horizontal() - 18.0f),
                     std::max(0.0f, bounds.h - s.padding.vertical())};
    renderer.drawTextInRect(selectedLabel(), textRect, s.color,
                            s.fontSize, TextAlign::Left, s.fontWeight, renderFontName(s),
                            s.fontStyle, s.direction, s.unicodeBidi);
    Rect arrowRect = {bounds.x + bounds.w - 20.0f, bounds.y, 16.0f, bounds.h};
    renderer.drawTextInRect("\xE2\x96\xBE", arrowRect, s.color,
                            std::max(9.0f, s.fontSize - 1.0f),
                            TextAlign::Center, FontWeight::Bold, renderFontName(s),
                            FontStyle::Normal, Direction::Ltr, UnicodeBidi::Normal);
    if (focused) {
        renderer.drawBorder({bounds.x - 2.0f, bounds.y - 2.0f, bounds.w + 4.0f, bounds.h + 4.0f},
                            Border(2.0f, Color(0.90f, 0.59f, 0.0f, 1.0f)),
                            BorderRadius(s.borderRadius.uniform() + 2.0f));
    }
    if (expanded) {
        auto options = selectOptions(this);
        float rowH = std::max(20.0f, s.fontSize * s.lineHeight + 5.0f);
        Rect list = {bounds.x, bounds.y + bounds.h, bounds.w, rowH * options.size()};
        renderer.drawRoundedRect(list, Color(1, 1, 1, 1), BorderRadius(0.0f));
        renderer.drawBorder(list, Border(1.0f, Color::fromHex("#767676")),
                            BorderRadius(0.0f));
        for (size_t i = 0; i < options.size(); ++i) {
            Rect row = {list.x, list.y + rowH * i, list.w, rowH};
            if (i == selectedIndex) {
                renderer.drawRoundedRect(row, Color(0.0f, 0.47f, 0.91f, 0.2f),
                                         BorderRadius(0.0f));
            }
            Rect optionText = {row.x + s.padding.left, row.y, row.w - s.padding.horizontal(), row.h};
            renderer.drawTextInRect(options[i]->label, optionText, Color(0, 0, 0, 1),
                                    s.fontSize, TextAlign::Left, s.fontWeight,
                                    renderFontName(s), s.fontStyle, s.direction,
                                    s.unicodeBidi);
        }
    }
}
void Icon::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    Color c = computedStyle->color;
    if (computedStyle->hasHoverColor && hoverAnim > 0) {
        c = Color::lerp(computedStyle->color, computedStyle->hoverColor, hoverAnim);
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
void Image::updateCurrentSrc() {
    currentSrc = source;
    intrinsicDensity = 1.0f;
    if (srcset.empty()) return;
    std::istringstream stream(srcset);
    std::string token;
    float bestDensityDiff = 999.0f;
    std::string bestSrc = currentSrc;
    float bestDensity = 1.0f;
    while (std::getline(stream, token, ',')) {
        size_t start = 0;
        while (start < token.size() && std::isspace((unsigned char)token[start])) ++start;
        if (start >= token.size()) continue;
        size_t space = token.find(' ', start);
        std::string url = token.substr(start, space == std::string::npos ? std::string::npos : space - start);
        float density = 1.0f;
        if (space != std::string::npos) {
            size_t descStart = space + 1;
            while (descStart < token.size() && std::isspace((unsigned char)token[descStart])) ++descStart;
            size_t descEnd = token.size();
            while (descEnd > descStart && std::isspace((unsigned char)token[descEnd - 1])) --descEnd;
            if (descEnd > descStart && token[descEnd - 1] == 'x') {
                try {
                    density = std::stof(token.substr(descStart, descEnd - descStart - 1));
                } catch (...) {}
            }
        }
        float diff = std::abs(density - devicePixelRatio);
        if (diff < bestDensityDiff) {
            bestDensityDiff = diff;
            bestSrc = url;
            bestDensity = density;
        }
    }
    currentSrc = bestSrc;
    intrinsicDensity = bestDensity > 0.0f ? bestDensity : 1.0f;
    naturalSize = {0, 0};
    loadState = ImageWidgetState::Idle;
}
void Image::layout(const Rect& parentBounds) {
    if (!layoutDirty && lastLayoutParentBounds.w == parentBounds.w && lastLayoutParentBounds.h == parentBounds.h) {
        float dx = parentBounds.x - lastLayoutParentBounds.x;
        float dy = parentBounds.y - lastLayoutParentBounds.y;
        if (dx != 0.0f || dy != 0.0f) {
            translateLayout(dx, dy);
        }
        return;
    }
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    if ((naturalSize.x <= 0.0f || naturalSize.y <= 0.0f) && !currentSrc.empty()) {
        naturalSize = probeImageNaturalSize(currentSrc);
    }
    float naturalW = naturalSize.x > 0.0f ? (naturalSize.x / intrinsicDensity) : 300.0f;
    float naturalH = naturalSize.y > 0.0f ? (naturalSize.y / intrinsicDensity) : 150.0f;
    float ratio = naturalH > 0.0f ? naturalW / naturalH : 1.0f;
    bool hasW = s.width.isSet();
    bool hasH = s.height.isSet();
    if (!hasW && !hasH) {
        bounds.w = naturalW + s.padding.horizontal();
        bounds.h = naturalH + s.padding.vertical();
    } else if (hasW && !hasH) {
        float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
        bounds.h = contentW / std::max(0.001f, ratio) + s.padding.vertical();
    } else if (!hasW && hasH) {
        float contentH = std::max(0.0f, bounds.h - s.padding.vertical());
        bounds.w = contentH * ratio + s.padding.horizontal();
    }
}
void Image::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    Rect content = {
        bounds.x + computedStyle->padding.left,
        bounds.y + computedStyle->padding.top,
        std::max(0.0f, bounds.w - computedStyle->padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle->padding.vertical())
    };
    if (content.w <= 0.0f || content.h <= 0.0f || currentSrc.empty()) {
        renderChildren(renderer);
        return;
    }
    if (loadState == ImageWidgetState::Idle) {
        loadState = ImageWidgetState::Loading;
    }
    Vec2 natural = { naturalSize.x / intrinsicDensity, naturalSize.y / intrinsicDensity };
    if (natural.x <= 0.0f || natural.y <= 0.0f) {
        Vec2 rawSize = renderer.imageSize(currentSrc);
        natural = { rawSize.x / intrinsicDensity, rawSize.y / intrinsicDensity };
        if (rawSize.x > 0.0f && rawSize.y > 0.0f) {
            naturalSize = rawSize;
            if (loadState == ImageWidgetState::Loading) {
                loadState = ImageWidgetState::Complete;
                if (onLoad) onLoad();
            }
        } else {
            if (loadState == ImageWidgetState::Loading) {
                loadState = ImageWidgetState::Error;
                if (onError) onError();
            }
            natural = {content.w, content.h};
        }
    }
    Rect draw = content;
    float scaleX = natural.x > 0.0f ? content.w / natural.x : 1.0f;
    float scaleY = natural.y > 0.0f ? content.h / natural.y : 1.0f;
    bool clipToContent = false;
    if (computedStyle->objectFit == ObjectFit::Contain ||
        computedStyle->objectFit == ObjectFit::ScaleDown) {
        float scale = std::min(scaleX, scaleY);
        if (computedStyle->objectFit == ObjectFit::ScaleDown) {
            scale = std::min(1.0f, scale);
        }
        draw.w = natural.x * scale;
        draw.h = natural.y * scale;
        draw.x = content.x + (content.w - draw.w) * computedStyle->objectPosition.x +
                 computedStyle->objectPositionOffset.x;
        draw.y = content.y + (content.h - draw.h) * computedStyle->objectPosition.y +
                 computedStyle->objectPositionOffset.y;
    } else if (computedStyle->objectFit == ObjectFit::Cover) {
        float scale = std::max(scaleX, scaleY);
        draw.w = natural.x * scale;
        draw.h = natural.y * scale;
        draw.x = content.x + (content.w - draw.w) * computedStyle->objectPosition.x +
                 computedStyle->objectPositionOffset.x;
        draw.y = content.y + (content.h - draw.h) * computedStyle->objectPosition.y +
                 computedStyle->objectPositionOffset.y;
        clipToContent = true;
    } else if (computedStyle->objectFit == ObjectFit::None) {
        draw.w = natural.x;
        draw.h = natural.y;
        draw.x = content.x + (content.w - draw.w) * computedStyle->objectPosition.x +
                 computedStyle->objectPositionOffset.x;
        draw.y = content.y + (content.h - draw.h) * computedStyle->objectPosition.y +
                 computedStyle->objectPositionOffset.y;
        clipToContent = true;
    }
    if (loadState == ImageWidgetState::Error && !alt.empty()) {
        renderer.pushScissor(content);
        Rect altDraw = content;
        float fontSize = computedStyle->fontSize > 0.0f ? computedStyle->fontSize : 14.0f;
        std::string fontName = computedStyle->fontFamily.empty() ? "sans-serif" : computedStyle->fontFamily;
        altDraw.y += (content.h - fontSize) * 0.5f;
        altDraw.x += 4.0f;
        renderer.drawText(alt, {altDraw.x, altDraw.y}, Color(0.5f, 0.5f, 0.5f, 1.0f), fontSize, FontWeight::Normal, fontName,
                          computedStyle->fontStyle, computedStyle->direction, computedStyle->unicodeBidi);
        renderer.popScissor();
        renderChildren(renderer);
        return;
    }
    if (clipToContent) {
        Rect visible;
        if (intersectRects(draw, content, visible)) {
            Rect sourceUv(
                (visible.x - draw.x) / std::max(1.0f, draw.w),
                (visible.y - draw.y) / std::max(1.0f, draw.h),
                visible.w / std::max(1.0f, draw.w),
                visible.h / std::max(1.0f, draw.h));
            renderer.drawImage(currentSrc, visible, sourceUv, computedStyle->opacity, computedStyle->color);
        }
    } else {
        renderer.drawImage(currentSrc, draw, computedStyle->opacity, computedStyle->color);
    }
    renderChildren(renderer);
}
void Canvas::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    if (s.backgroundColor.a > 0) {
        renderer.drawRoundedRect(bounds, s.backgroundColor, s.borderRadius);
    }
    if (onDraw) {
        renderer.flush();
        renderer.pushScissor(bounds);
        onDraw(renderer, bounds);
        renderer.flush();
        renderer.popScissor();
    }
    renderChildren(renderer);
}
void VirtualList::setItemCount(size_t count) {
    if (itemCount == count) return;
    itemCount = count;
    forceRebuild_ = true;
    markLayoutDirty();
}
void VirtualList::refresh() {
    forceRebuild_ = true;
    markLayoutDirty();
}
void VirtualList::scrollToIndex(size_t index, VirtualListScrollStrategy strategy) {
    if (itemCount == 0) return;
    itemHeight = std::max(1.0f, itemHeight);
    index = std::min(index, itemCount - 1);
    float viewportH = std::max(0.0f, bounds.h - computedStyle->padding.vertical());
    if (viewportH <= 0.0f) {
        targetScrollY = itemHeight * static_cast<float>(index);
        scrollY = targetScrollY;
        forceRebuild_ = true;
        markLayoutDirty();
        return;
    }

    float itemTop = itemHeight * static_cast<float>(index);
    float itemBottom = itemTop + itemHeight;
    float target = scrollY;
    switch (strategy) {
    case VirtualListScrollStrategy::Start:
        target = itemTop;
        break;
    case VirtualListScrollStrategy::Center:
        target = itemTop - (viewportH - itemHeight) * 0.5f;
        break;
    case VirtualListScrollStrategy::End:
        target = itemBottom - viewportH;
        break;
    case VirtualListScrollStrategy::Nearest:
    default:
        if (itemTop < scrollY) {
            target = itemTop;
        } else if (itemBottom > scrollY + viewportH) {
            target = itemBottom - viewportH;
        } else {
            return;
        }
        break;
    }

    float maxScroll = std::max(0.0f, contentHeight - bounds.h);
    targetScrollY = std::clamp(target, 0.0f, maxScroll);
    scrollY = targetScrollY;
    forceRebuild_ = true;
    rebuildVisibleItems();
}
void VirtualList::layout(const Rect& parentBounds) {
    if (itemHeight <= 0.0f) {
        itemHeight = 1.0f;
    }
    if (computedStyle->display == Display::None) {
        bounds = {parentBounds.x, parentBounds.y, 0.0f, 0.0f};
        contentHeight = 0.0f;
        children.clear();
        visibleStart_ = visibleEnd_ = 0;
        layoutDirty = false;
        return;
    }
    if (!layoutDirty && lastLayoutParentBounds.w == parentBounds.w && lastLayoutParentBounds.h == parentBounds.h) {
        float dx = parentBounds.x - lastLayoutParentBounds.x;
        float dy = parentBounds.y - lastLayoutParentBounds.y;
        if (dx != 0.0f || dy != 0.0f) {
            translateLayout(dx, dy);
        }
        contentHeight = computedStyle->padding.vertical() + itemHeight * static_cast<float>(itemCount);
        clampScroll();
        rebuildVisibleItems();
        return;
    }
    if (!layoutDirty && rectEqual(lastLayoutParentBounds, parentBounds)) {
        contentHeight = computedStyle->padding.vertical() + itemHeight * static_cast<float>(itemCount);
        clampScroll();
        rebuildVisibleItems();
        return;
    }

    lastLayoutParentBounds = parentBounds;
    const Style& s = *computedStyle;
    bool heightProvidedByParentFlex = consumesParentMainAxisHeight(this, s);
    float x = parentBounds.x + s.margin.left;
    float y = parentBounds.y + s.margin.top;
    float w = s.width.isSet() ? s.width.resolve(parentBounds.w) :
              (parentBounds.w < 9999 ? parentBounds.w - s.margin.horizontal() : 0);
    float h = s.height.isSet() ? s.height.resolve(parentBounds.h) :
              (parentBounds.h < 9999 ? parentBounds.h - s.margin.vertical() : 0);
    bool widthControlsRatio = s.aspectRatio > 0.0f && s.width.isSet() && !s.height.isSet();
    bool heightControlsRatio = s.aspectRatio > 0.0f && s.height.isSet() && !s.width.isSet();
    if (s.minWidth.isSet()) w = std::max(w, s.minWidth.resolve(parentBounds.w));
    if (s.maxWidth.isSet()) w = std::min(w, s.maxWidth.resolve(parentBounds.w));
    if (widthControlsRatio) h = w / s.aspectRatio;
    if (s.minHeight.isSet()) h = std::max(h, s.minHeight.resolve(parentBounds.h));
    if (s.maxHeight.isSet()) h = std::min(h, s.maxHeight.resolve(parentBounds.h));
    if (heightControlsRatio) w = h * s.aspectRatio;
    if (s.hasBoxSizing && s.boxSizing == BoxSizing::ContentBox) {
        if (s.width.isSet()) w += s.padding.horizontal() + usedBorderHorizontal(s);
        if (s.height.isSet()) h += s.padding.vertical() + usedBorderVertical(s);
    }

    bounds = {x, y, std::max(0.0f, w), std::max(0.0f, h)};
    contentHeight = s.padding.vertical() + itemHeight * static_cast<float>(itemCount);
    if (!s.height.isSet() && !heightProvidedByParentFlex && !clipsOverflow(s)) {
        bounds.h = std::max(bounds.h, contentHeight);
    }
    clampScroll();
    rebuildVisibleItems();
    layoutPositionedChildren();
    layoutDirty = false;
    lifecycleState = WidgetLifecycle::LayoutClean;
}
void VirtualList::update(const InputState& input) {
    float previousScroll = scrollY;
    float previousTargetScroll = targetScrollY;
    Widget::update(input);
    if (std::abs(previousScroll - scrollY) > 0.01f ||
        std::abs(previousTargetScroll - targetScrollY) > 0.01f) {
        rebuildVisibleItems();
    }
}
void VirtualList::rebuildVisibleItems() {
    itemHeight = std::max(1.0f, itemHeight);
    const Style& s = computedStyle;
    float contentX = bounds.x + s.padding.left;
    float contentY = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    float viewportH = std::max(0.0f, bounds.h - s.padding.vertical());
    float listHeight = itemHeight * static_cast<float>(itemCount);

    size_t nextStart = 0;
    size_t nextEnd = 0;
    if (itemCount > 0 && viewportH > 0.0f) {
        float startPx = std::max(0.0f, scrollY - overdraw);
        float endPx = std::min(listHeight, scrollY + viewportH + overdraw);
        nextStart = std::min(itemCount, static_cast<size_t>(std::floor(startPx / itemHeight)));
        nextEnd = std::min(itemCount, static_cast<size_t>(std::ceil(endPx / itemHeight)));
        if (nextEnd <= nextStart) {
            nextEnd = std::min(itemCount, nextStart + 1);
        }
    }

    bool rangeChanged = nextStart != visibleStart_ || nextEnd != visibleEnd_;
    bool widthChanged = std::abs(contentW - lastBuildWidth_) > 0.5f;
    bool itemHeightChanged = std::abs(itemHeight - lastBuildItemHeight_) > 0.01f;
    if (forceRebuild_ || rangeChanged || widthChanged || itemHeightChanged) {
        children.clear();
        children.reserve(nextEnd - nextStart);
        if (!childArena) {
            childArena = detail::makeWidgetArena();
        }
        for (size_t index = nextStart; index < nextEnd; ++index) {
            detail::WidgetArenaAllocator<Panel> allocator(childArena);
            auto row = std::allocate_shared<Panel>(allocator, itemClassName);
            row->type = "virtual-list-item";
            row->parent = this;
            row->style.height = CSSValue::px(itemHeight);
            row->style.width = CSSValue::pct(100.0f);
            row->reserveChildren(2);
            if (itemBuilder) {
                itemBuilder(row.get(), index);
            }
            children.push_back(std::move(row));
        }
        visibleStart_ = nextStart;
        visibleEnd_ = nextEnd;
        lastBuildWidth_ = contentW;
        lastBuildItemHeight_ = itemHeight;
        forceRebuild_ = false;
    }

    Application* app = Application::instance();
    StyleSheet* sheet = app ? &app->stylesheet() : nullptr;
    for (size_t offset = 0; offset < children.size(); ++offset) {
        auto& row = children[offset];
        if (!row) continue;
        if (sheet) {
            row->resolveStyles(*sheet);
        }
        size_t index = visibleStart_ + offset;
        Rect rowArea = {
            contentX,
            contentY + static_cast<float>(index) * itemHeight,
            contentW,
            itemHeight
        };
        row->layout(rowArea);
    }
    subtreeStyleDirty = false;
}
void StatCard::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    bool hasScale = (renderScale != 1.0f && !layoutObject);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }
    if (s.boxShadow.blur > 0) {
        renderer.drawBoxShadow(bounds, s.boxShadow, s.borderRadius);
    }
    Color bgColor = s.backgroundColor;
    if (bgColor.a < 0.01f) bgColor = Color::fromHex("#131420");
    if (s.hasHoverBg && hoverAnim > 0) {
        bgColor = Color::lerp(bgColor, s.hoverBackgroundColor, hoverAnim);
    }
    renderer.drawRoundedRect(bounds, bgColor, s.borderRadius, s.opacity);
    BoxShadow glow;
    glow.blur = 15 + hoverAnim * 10;
    glow.color = accentColor;
    glow.color.a = 0.1f + hoverAnim * 0.1f;
    renderer.drawBoxShadow(bounds, glow, s.borderRadius);
    Rect accentBar = {bounds.x + 2, bounds.y + 12, 4, bounds.h - 24};
    renderer.drawRoundedRect(accentBar, accentColor, BorderRadius(2));
    if (s.border.width > 0) {
        Border b = s.border;
        if (s.hasHoverBorder && hoverAnim > 0) {
            b.color = Color::lerp(s.border.color, s.hoverBorderColor, hoverAnim);
        }
        renderer.drawBorder(bounds, b, s.borderRadius);
    }
    float px = (s.padding.left > 0 ? s.padding.left : 24) + 10;
    float py = s.padding.top > 0 ? s.padding.top : 24;
    renderer.drawText(title, {bounds.x + px, bounds.y + py},
                      Color(1, 1, 1, 0.78f), 13, FontWeight::Bold);
    renderer.drawText(value, {bounds.x + px, bounds.y + py + 22},
                      Color(1, 1, 1, 1.0f), 32, FontWeight::Bold);
    renderer.drawText(subtitle, {bounds.x + px, bounds.y + bounds.h - 35},
                      accentColor, 12);
    renderChildren(renderer);
    if (hasScale) {
        renderer.popScale();
    }
}

} // namespace FluxUI