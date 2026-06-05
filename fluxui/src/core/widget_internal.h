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

} // namespace detail
} // namespace FluxUI
