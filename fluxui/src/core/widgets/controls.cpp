// FluxUI — form control widgets (Checkbox, Radio, RangeInput, ProgressBar,
// Meter, Progress) implementation.
// Extracted from the monolithic core/application.cpp. Depends only on the
// public Widget API plus the shared detail:: helpers in widget_internal.h.
#include "fluxui/widgets.h"
#include "../widget_internal.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace FluxUI {

using detail::canPaintWidget;
using detail::normalizeTextEditingKey;
using detail::rootOfWidget;
using detail::clearRadioGroup;

// ============================================================
//  Checkbox
// ============================================================
void Checkbox::setChecked(bool value) {
    if (checked == value) return;
    checked = value;
    markStyleDirty();
    if (onChange) onChange(checked);
}
void Checkbox::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    if (!s.width.isSet()) bounds.w = 13.0f + s.margin.horizontal();
    if (!s.height.isSet()) bounds.h = 13.0f + s.margin.vertical();
}
void Checkbox::update(const InputState& input) {
    Widget::update(input);
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardToggle = focused && (keyCode == 0x20 || keyCode == 0x0D) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    if ((hovered && input.mouseClicked[0]) || keyboardToggle) {
        focused = true;
        setChecked(!checked);
    }
}
void Checkbox::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    float size = std::max(4.0f, std::min(bounds.w, bounds.h));
    Rect box = {
        bounds.x + (bounds.w - size) * 0.5f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
    Color fill;
    Color borderColor;
    if (checked) {
        Color baseAccent = Color::fromHex("#1a73e8");
        Color hoverAccent = Color::fromHex("#155bb5");
        Color activeAccent = Color::fromHex("#10478e");
        if (pressed) {
            fill = activeAccent;
        } else if (hoverAnim > 0.0f) {
            fill = Color::lerp(baseAccent, hoverAccent, hoverAnim);
        } else {
            fill = baseAccent;
        }
        borderColor = fill;
    } else {
        Color baseBg = s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1);
        Color activeBg = Color::fromHex("#efefef");
        Color baseBorder = s.border.color.a > 0.0f ? s.border.color : Color::fromHex("#767676");
        Color hoverBorder = Color::fromHex("#4f4f4f");
        if (pressed) {
            fill = activeBg;
            borderColor = hoverBorder;
        } else if (hoverAnim > 0.0f) {
            fill = baseBg;
            borderColor = Color::lerp(baseBorder, hoverBorder, hoverAnim);
        } else {
            fill = baseBg;
            borderColor = baseBorder;
        }
    }
    renderer.drawRoundedRect(box, fill, BorderRadius(2.0f));
    renderer.drawBorder(box, Border(std::max(1.0f, s.border.width), borderColor), BorderRadius(2.0f));
    if (checked) {
        renderer.drawTextInRect("\xE2\x9C\x93", box, Color(1, 1, 1, 1),
                                std::max(10.0f, size * 0.86f), TextAlign::Center,
                                FontWeight::Bold);
    }
    if (focused) {
        renderer.drawBorder({box.x - 3.0f, box.y - 3.0f, box.w + 6.0f, box.h + 6.0f},
                            Border(2.0f, Color(0.90f, 0.59f, 0.0f, 1.0f)),
                            BorderRadius(4.0f));
    }
}

// ============================================================
//  Radio
// ============================================================
void Radio::setChecked(bool value) {
    if (checked == value) return;
    checked = value;
    if (checked) {
        clearRadioGroup(rootOfWidget(this), this, group);
    }
    markStyleDirty();
    if (onChange) onChange(checked);
}
void Radio::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    if (!s.width.isSet()) bounds.w = 13.0f + s.margin.horizontal();
    if (!s.height.isSet()) bounds.h = 13.0f + s.margin.vertical();
}
void Radio::update(const InputState& input) {
    Widget::update(input);
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardToggle = focused && (keyCode == 0x20 || keyCode == 0x0D) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    if ((hovered && input.mouseClicked[0]) || keyboardToggle) {
        focused = true;
        setChecked(true);
    }
}
void Radio::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    float size = std::max(4.0f, std::min(bounds.w, bounds.h));
    Rect ring = {
        bounds.x + (bounds.w - size) * 0.5f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
    Color fill;
    Color borderColor;
    Color dotColor;
    Color baseAccent = Color::fromHex("#1a73e8");
    Color hoverAccent = Color::fromHex("#155bb5");
    Color activeAccent = Color::fromHex("#10478e");
    if (checked) {
        fill = s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1);
        if (pressed) {
            dotColor = activeAccent; borderColor = activeAccent;
        } else if (hoverAnim > 0.0f) {
            dotColor = Color::lerp(baseAccent, hoverAccent, hoverAnim); borderColor = dotColor;
        } else {
            dotColor = baseAccent; borderColor = baseAccent;
        }
    } else {
        Color baseBg = s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1);
        Color baseBorder = s.border.color.a > 0.0f ? s.border.color : Color::fromHex("#767676");
        Color hoverBorder = Color::fromHex("#4f4f4f");
        if (pressed) {
            fill = Color::fromHex("#efefef"); borderColor = hoverBorder;
        } else if (hoverAnim > 0.0f) {
            fill = baseBg; borderColor = Color::lerp(baseBorder, hoverBorder, hoverAnim);
        } else {
            fill = baseBg; borderColor = baseBorder;
        }
        dotColor = baseAccent;
    }
    renderer.drawRoundedRect(ring, fill, BorderRadius(size * 0.5f));
    renderer.drawBorder(ring, Border(std::max(1.0f, s.border.width), borderColor),
                        BorderRadius(size * 0.5f));
    if (checked) {
        float dot = std::max(4.0f, size * 0.48f);
        renderer.drawRoundedRect({ring.x + (ring.w - dot) * 0.5f,
                                  ring.y + (ring.h - dot) * 0.5f,
                                  dot,
                                  dot},
                                 dotColor,
                                 BorderRadius(dot * 0.5f));
    }
    if (focused) {
        renderer.drawBorder({ring.x - 3.0f, ring.y - 3.0f, ring.w + 6.0f, ring.h + 6.0f},
                            Border(2.0f, Color(0.90f, 0.59f, 0.0f, 1.0f)),
                            BorderRadius((size + 6.0f) * 0.5f));
    }
}

// ============================================================
//  RangeInput
// ============================================================
void RangeInput::setValue(float newValue, bool notify) {
    if (max < min) std::swap(max, min);
    float clamped = std::clamp(newValue, min, max);
    if (step > 0.0f) {
        clamped = min + std::round((clamped - min) / step) * step;
        clamped = std::clamp(clamped, min, max);
    }
    if (std::abs(value - clamped) < 0.0001f) return;
    value = clamped;
    if (notify && onChange) onChange(value);
}
void RangeInput::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (!computedStyle->width.isSet()) bounds.w = 129.0f;
    if (!computedStyle->height.isSet()) bounds.h = 16.0f;
}
void RangeInput::update(const InputState& input) {
    Widget::update(input);
    auto setFromPoint = [&](float x) {
        float pad = 7.0f;
        float t = (x - (bounds.x + pad)) / std::max(1.0f, bounds.w - pad * 2.0f);
        setValue(min + std::clamp(t, 0.0f, 1.0f) * (max - min));
    };
    if (hovered && input.mouseClicked[0]) {
        focused = true;
        dragging_ = true;
        setFromPoint(input.mousePos.x);
    }
    if (dragging_ && input.mouseDown[0]) {
        setFromPoint(input.mousePos.x);
    }
    if (input.mouseReleased[0]) {
        dragging_ = false;
    }
    if (!focused) return;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    if (keyCode == 0) return;
    float delta = step > 0.0f ? step : (max - min) / 100.0f;
    if (keyCode == 0x25 || keyCode == 0x28) setValue(value - delta);
    else if (keyCode == 0x27 || keyCode == 0x26) setValue(value + delta);
    else if (keyCode == 0x21) setValue(value + delta * 10.0f);
    else if (keyCode == 0x22) setValue(value - delta * 10.0f);
    else if (keyCode == 0x24) setValue(min);
    else if (keyCode == 0x23) setValue(max);
}
void RangeInput::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    float pad = 7.0f;
    float trackH = 4.0f;
    Rect track = {bounds.x + pad, bounds.y + (bounds.h - trackH) * 0.5f,
                  std::max(1.0f, bounds.w - pad * 2.0f), trackH};
    float t = (max == min) ? 0.0f : std::clamp((value - min) / (max - min), 0.0f, 1.0f);
    Color activeAccent = Color::fromHex("#1a73e8");
    Color hoverAccent = Color::fromHex("#155bb5");
    Color pressedAccent = Color::fromHex("#10478e");
    Color accentColor;
    if (dragging_) {
        accentColor = pressedAccent;
    } else if (hoverAnim > 0.0f) {
        accentColor = Color::lerp(activeAccent, hoverAccent, hoverAnim);
    } else {
        accentColor = activeAccent;
    }
    Color trackColor = Color::fromHex("#cccccc");
    renderer.drawRoundedRect(track, trackColor, BorderRadius(trackH * 0.5f));
    renderer.drawRoundedRect({track.x, track.y, track.w * t, track.h},
                             accentColor, BorderRadius(trackH * 0.5f));
    float thumb = 14.0f;
    Rect knob = {track.x + track.w * t - thumb * 0.5f,
                 bounds.y + (bounds.h - thumb) * 0.5f,
                 thumb,
                 thumb};
    renderer.drawRoundedRect(knob, accentColor, BorderRadius(thumb * 0.5f));
    renderer.drawBorder(knob, Border(1.0f, Color(1, 1, 1, 1.0f)),
                        BorderRadius(thumb * 0.5f));
    if (focused) {
        renderer.drawBorder({knob.x - 3.0f, knob.y - 3.0f, knob.w + 6.0f, knob.h + 6.0f},
                            Border(2.0f, Color(0.90f, 0.59f, 0.0f, 1.0f)),
                            BorderRadius((thumb + 6.0f) * 0.5f));
    }
}

// ============================================================
//  ProgressBar
// ============================================================
void ProgressBar::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    const Style& s = *computedStyle;
    renderer.drawRoundedRect(bounds, s.backgroundColor.a > 0 ?
        s.backgroundColor : Color(1, 1, 1, 0.1f), s.borderRadius);
    if (progress > 0) {
        Rect fillRect = bounds;
        fillRect.w = bounds.w * std::clamp(progress, 0.0f, 1.0f);
        renderer.drawRoundedRect(fillRect, barColor, s.borderRadius);
    }
    renderChildren(renderer);
}

// ============================================================
//  Meter (HTMLMeterElement)
// ============================================================
void Meter::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    float rangeVal = max - min;
    float fraction = 0.0f;
    if (rangeVal > 0.0f) {
        fraction = (value - min) / rangeVal;
    }
    fraction = std::max(0.0f, std::min(1.0f, fraction));
    if (fraction > 0.0f) {
        // Blink-faithful meter colors (non-auto appearance, light mode):
        // Optimum (normal): #107c10, Suboptimum (warning): #ffb900, Even-less-good (danger): #d83b01
        Color barColor = Color::fromHex("#107c10"); // Default green (optimum)
        if (value < low || value > high) {
            barColor = Color::fromHex("#d83b01"); // Red — even-less-good
        } else if (optimum < low && value > low) {
            barColor = Color::fromHex("#ffb900"); // Yellow — suboptimum
        } else if (optimum > high && value < high) {
            barColor = Color::fromHex("#ffb900"); // Yellow — suboptimum
        }
        Rect barRect = bounds;
        barRect.x += computedStyle->padding.left;
        barRect.y += computedStyle->padding.top;
        barRect.w = (bounds.w - computedStyle->padding.left - computedStyle->padding.right) * fraction;
        barRect.h = bounds.h - computedStyle->padding.top - computedStyle->padding.bottom;
        BorderRadius barRadius = computedStyle->borderRadius;
        renderer.drawRoundedRect(barRect, barColor, barRadius);
    }
    if (computedStyle->border.width > 0) {
        renderer.drawBorder(bounds, computedStyle->border, computedStyle->borderRadius);
    }
}

// ============================================================
//  Progress (HTMLProgressElement)
// ============================================================
void Progress::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);

    Rect fillRect = bounds;
    fillRect.x += computedStyle->padding.left;
    fillRect.y += computedStyle->padding.top;
    fillRect.h = bounds.h - computedStyle->padding.top - computedStyle->padding.bottom;
    float maxW = bounds.w - computedStyle->padding.left - computedStyle->padding.right;

    Color progressColor = Color::fromHex("#107c10"); // Blink: green progress value

    if (value < 0.0f) {
        // Indeterminate state: animate sliding bar
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        double ms = std::chrono::duration<double, std::milli>(now).count();
        float pulseFraction = std::sin(ms * 0.005) * 0.5f + 0.5f;
        fillRect.w = maxW * 0.3f;
        fillRect.x += (maxW - fillRect.w) * pulseFraction;
        renderer.drawRoundedRect(fillRect, progressColor, computedStyle->borderRadius);
        if (auto* app = Application::instance()) {
            app->requestRedraw();
        }
    } else {
        // Determinate state
        float fraction = max > 0.0f ? std::max(0.0f, std::min(1.0f, value / max)) : 0.0f;
        if (fraction > 0.0f) {
            fillRect.w = maxW * fraction;
            renderer.drawRoundedRect(fillRect, progressColor, computedStyle->borderRadius);
        }
    }

    if (computedStyle->border.width > 0) {
        renderer.drawBorder(bounds, computedStyle->border, computedStyle->borderRadius);
    }
}

} // namespace FluxUI
