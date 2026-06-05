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

} // namespace detail
} // namespace FluxUI
