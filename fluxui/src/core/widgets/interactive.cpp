// FluxUI - interactive / disclosure widgets: LazyPanel, Anchor, Details,
// Summary, Dialog. Extracted from core/application.cpp. Shared helpers live in
// widget_internal.h (FluxUI::detail).
#include "fluxui/widgets.h"
#include "../widget_internal.h"
#include "fluxui/css_parser.h"
#include "fluxui/platform.h"

#include <algorithm>
#include <string>
#include <thread>

namespace FluxUI {
using namespace FluxUI::detail;

void LazyPanel::update(const InputState& input) {
    if (!initialized) {
        initialized = true;
        if (skeletonBuilder) {
            clearChildren();
            skeletonBuilder(this);
        }
    }
    bool currentLoaded = loaded.load(std::memory_order_acquire);
    if (currentLoaded && !lastLoadedState) {
        lastLoadedState = true;
        clearChildren();
        if (contentBuilder) {
            contentBuilder(this);
        }
        markLayoutDirty();
        markStyleDirtyRecursive();
    }
    Widget::update(input);
}
void Anchor::update(const InputState& input) {
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardActivate = focused && keyCode == 0x0D &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    bool clicked = hovered && input.mouseClicked[0];
    Text::update(input);
    if ((clicked || keyboardActivate) && !href.empty()) {
        Platform::openSystemURL(href);
    }
}
void Anchor::setAttribute(const std::string& name, const std::string& value) {
    Widget::setAttribute(name, value);
    if (name == "href") {
        href = value;
    }
}

void Details::layout(const Rect& parentBounds) {
    for (auto& child : children) {
        if (child->type != "summary") {
            child->visible = open;
        }
    }
    Widget::layout(parentBounds);
}

void Summary::update(const InputState& input) {
    bool clicked = hovered && input.mouseClicked[0];
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardToggle = focused && (keyCode == 0x0D || keyCode == 0x20) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    Text::update(input);
    if ((clicked || keyboardToggle) && parent && parent->type == "details") {
        if (auto* details = dynamic_cast<Details*>(parent)) {
            details->open = !details->open;
            details->markLayoutDirty();
            details->markStyleDirtyRecursive();
            if (auto* app = Application::instance()) {
                app->requestRedraw();
            }
        }
    }
}

void Summary::render(Renderer& renderer) {
    Text::render(renderer);
    if (!visible) return;
    bool isOpen = false;
    if (parent && parent->type == "details") {
        if (auto* details = dynamic_cast<Details*>(parent)) {
            isOpen = details->open;
        }
    }
    float mSize = computedStyle->fontSize * 0.8f;
    float mX = bounds.x + computedStyle->padding.left * 0.3f;
    float mY = bounds.y + (bounds.h - mSize) * 0.5f;
    renderer.drawText(isOpen ? "v" : ">", {mX, mY}, computedStyle->color, mSize, FontWeight::Bold);
}

void Dialog::show() {
    open = true;
    modal = false;
    style.display = Display::Block;
    markStyleDirty();
    if (auto* app = Application::instance()) app->requestRedraw();
}

void Dialog::showModal() {
    open = true;
    modal = true;
    style.display = Display::Block;
    markStyleDirty();
    if (auto* app = Application::instance()) app->requestRedraw();
}

void Dialog::close() {
    open = false;
    modal = false;
    style.display = Display::None;
    markStyleDirty();
    if (auto* app = Application::instance()) app->requestRedraw();
}

void Dialog::resolveStyles(const StyleSheet& sheet) {
    Widget::resolveStyles(sheet);
    computedStyle.ensureMutable().display = open ? Display::Block : Display::None;
    size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
    if (nextLayoutSignature != layoutSignature) {
        layoutSignature = nextLayoutSignature;
        markLayoutDirty();
    }
}

void Dialog::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (!open || !computedStyle) return;

    const Style& s = *computedStyle;

    // --- Measure content height (block flow) ---
    float cy = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    for (auto& child : children) {
        if (!child || !child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) {
            continue;
        }
        const Style& cs = *(child->computedStyle);
        Rect childArea = {
            bounds.x + s.padding.left + cs.margin.left,
            cy + cs.margin.top,
            std::max(0.0f, contentW - cs.margin.horizontal()),
            10000.0f
        };
        child->layout(childArea);
        cy = child->bounds.y + child->bounds.h + cs.margin.bottom;
    }

    float measuredHeight = cy - bounds.y + s.padding.bottom;
    contentHeight = std::max(0.0f, measuredHeight);
    if (!s.height.isSet() || s.height.isAuto() || measuredHeight > bounds.h) {
        bounds.h = std::max(0.0f, measuredHeight);
    }

    // --- Center the dialog in the viewport (Blink top-layer centering) ---
    // Since FluxUI does not support transform: translate(-50%, -50%),
    // we center the dialog procedurally after measuring its natural size.
    const Widget* root = this;
    while (root->parent) root = root->parent;
    float vpW = root->bounds.w;
    float vpH = root->bounds.h;

    // Blink uses max-width: calc(100% - 6px - 2em) and max-height similarly
    float maxDialogW = vpW - 6.0f - 2.0f * s.fontSize * 2.0f;
    float maxDialogH = vpH - 6.0f - 2.0f * s.fontSize * 2.0f;

    if (!s.width.isSet()) {
        // fit-content: use current width but cap to viewport
        bounds.w = std::min(bounds.w, std::max(0.0f, maxDialogW));
    }
    if (!s.height.isSet()) {
        bounds.h = std::min(bounds.h, std::max(0.0f, maxDialogH));
    }

    // Center horizontally and vertically
    bounds.x = root->bounds.x + (vpW - bounds.w) * 0.5f;
    bounds.y = root->bounds.y + (vpH - bounds.h) * 0.5f;

    // Re-layout children with the new centered position
    cy = bounds.y + s.padding.top;
    contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    for (auto& child : children) {
        if (!child || !child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) {
            continue;
        }
        const Style& cs = *(child->computedStyle);
        Rect childArea = {
            bounds.x + s.padding.left + cs.margin.left,
            cy + cs.margin.top,
            std::max(0.0f, contentW - cs.margin.horizontal()),
            10000.0f
        };
        child->layout(childArea);
        cy = child->bounds.y + child->bounds.h + cs.margin.bottom;
    }

    layoutPositionedChildren();
}

void Dialog::update(const InputState& input) {
    if (!open) return;
    Widget::update(input);
    int keyCode = normalizeTextEditingKey(input.keyCode);
    if (keyCode == 0x1B) {
        close();
    }
}

void Dialog::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    if (open && modal) {
        Vec2 winSize = renderer.getWindowSize();
        renderer.drawRoundedRect({0.0f, 0.0f, winSize.x, winSize.y},
                                 Color(0.0f, 0.0f, 0.0f, 0.55f),
                                 BorderRadius(0.0f),
                                 1.0f);
    }
    Widget::render(renderer);
}

} // namespace FluxUI