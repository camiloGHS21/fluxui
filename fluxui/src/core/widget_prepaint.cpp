// FluxUI - Widget pre-paint: builds the transform/clip/effect property-tree nodes (Blink-style pre-paint walk) before rasterization.
// Extracted from core/application.cpp. Shared helpers live in
// widget_internal.h (FluxUI::detail).
#include "fluxui/widgets.h"
#include "widget_internal.h"
#include "fluxui/layout_object.h"
#include "fluxui/property_trees.h"
#include "fluxui/compositor.h"

#include <algorithm>
#include <cmath>

namespace FluxUI {
using namespace FluxUI::detail;

void Widget::prePaint(const PaintProperties& parentProps) {
    if (!parent) {
        g_activePropertyTrees.clear();
    }
    lifecycleState = WidgetLifecycle::PrePaintDirty;

    // 1. Property Tree Builder phase:
    updatePaintProperties(parentProps);

    // 2. Paint Invalidation phase:
    invalidatePaintIfNeeded();

    lifecycleState = WidgetLifecycle::PrePaintClean;

    for (auto& child : children) {
        if (child) {
            child->prePaint(paintProperties);
        }
    }
}

void Widget::updatePaintProperties(const PaintProperties& parentProps) {
    paintProperties.translation = parentProps.translation;
    paintProperties.scale = parentProps.scale;
    paintProperties.clipRect = parentProps.clipRect;
    paintProperties.hasClip = parentProps.hasClip;
    paintProperties.opacity = parentProps.opacity * computedStyle->opacity;

    // 1. Resolve TransformNode
    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    if (scrollable) {
        scrollY = CompositorEngine::instance().getScrollY(reinterpret_cast<uintptr_t>(this));
        targetScrollY = CompositorEngine::instance().getTargetScrollY(reinterpret_cast<uintptr_t>(this));
        paintProperties.translation.y -= scrollY;

        Vec2 scrollOffset = {0.0f, -scrollY};
        transformNodeId = g_activePropertyTrees.insertTransformNode(
            parentProps.transformNodeId,
            1.0f,
            scrollOffset,
            {0.0f, 0.0f}
        );
    } else {
        transformNodeId = parentProps.transformNodeId;
    }
    paintProperties.transformNodeId = transformNodeId;

    // 2. Resolve ClipNode
    bool clip = clipsOverflow(computedStyle);
    if (clip) {
        Rect localClip = bounds;
        if (paintProperties.hasClip) {
            float x1 = std::max(paintProperties.clipRect.x, localClip.x);
            float y1 = std::max(paintProperties.clipRect.y, localClip.y);
            float x2 = std::min(paintProperties.clipRect.x + paintProperties.clipRect.w, localClip.x + localClip.w);
            float y2 = std::min(paintProperties.clipRect.y + paintProperties.clipRect.h, localClip.y + localClip.h);
            paintProperties.clipRect = { x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1) };
        } else {
            paintProperties.clipRect = localClip;
            paintProperties.hasClip = true;
        }

        clipNodeId = g_activePropertyTrees.insertClipNode(
            parentProps.clipNodeId,
            bounds,
            transformNodeId
        );
    } else {
        clipNodeId = parentProps.clipNodeId;
    }
    paintProperties.clipNodeId = clipNodeId;

    // 3. Resolve EffectNode
    if (computedStyle->opacity != 1.0f) {
        effectNodeId = g_activePropertyTrees.insertEffectNode(
            parentProps.effectNodeId,
            computedStyle->opacity,
            0.0f,
            transformNodeId
        );
    } else {
        effectNodeId = parentProps.effectNodeId;
    }
    paintProperties.effectNodeId = effectNodeId;
}

void Widget::invalidatePaintIfNeeded() {
    bool propertyTreeChanged = (paintProperties != lastPaintProperties) ||
                               (bounds.x != lastPaintBounds.x ||
                                bounds.y != lastPaintBounds.y ||
                                bounds.w != lastPaintBounds.w ||
                                bounds.h != lastPaintBounds.h);

    if (propertyTreeChanged) {
        if (layoutObject) {
            layoutObject->markPaintDirty();
        }
    }

    lastPaintProperties = paintProperties;
    lastPaintBounds = bounds;
}

} // namespace FluxUI