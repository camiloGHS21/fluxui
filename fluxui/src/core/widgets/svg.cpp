// FluxUI — SVG widget (SvgElement / Svg) implementation.
// Extracted from the monolithic core/application.cpp. Depends only on the
// public Widget/SVG API and the renderer's SVG rasterization entry points.
#include "fluxui/widgets.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace FluxUI {

void SvgElement::markSvgDirty() {
    Widget* p = parent;
    while (p) {
        if (p->type == "svg") {
            static_cast<Svg*>(p)->isRasterDirty = true;
            break;
        }
        p = p->parent;
    }
    if (auto* app = Application::instance()) {
        app->requestRedraw();
    }
}

void SvgElement::setAttribute(const std::string& name, const std::string& value) {
    Widget::setAttribute(name, value);
    if (name == "fill") fill = value;
    else if (name == "stroke") stroke = value;
    else if (name == "stroke-width") strokeWidth = value;
    else if (name == "transform") transformAttr = value;
    else if (name == "opacity") opacityAttr = value;
    else if (name == "fill-opacity") fillOpacity = value;
    else if (name == "stroke-opacity") strokeOpacity = value;
    else if (name == "fill-rule") fillRuleAttr = value;
    else if (type == "path" && name == "d") static_cast<SvgPath*>(this)->d = value;
    else if (type == "rect") {
        auto* r = static_cast<SvgRect*>(this);
        if (name == "x") r->x = value;
        else if (name == "y") r->y = value;
        else if (name == "width") r->width = value;
        else if (name == "height") r->height = value;
        else if (name == "rx") r->rx = value;
        else if (name == "ry") r->ry = value;
    } else if (type == "circle") {
        auto* c = static_cast<SvgCircle*>(this);
        if (name == "cx") c->cx = value;
        else if (name == "cy") c->cy = value;
        else if (name == "r") c->r = value;
    } else if (type == "ellipse") {
        auto* el = static_cast<SvgEllipse*>(this);
        if (name == "cx") el->cx = value;
        else if (name == "cy") el->cy = value;
        else if (name == "rx") el->rx = value;
        else if (name == "ry") el->ry = value;
    } else if (type == "line") {
        auto* l = static_cast<SvgLine*>(this);
        if (name == "x1") l->x1 = value;
        else if (name == "y1") l->y1 = value;
        else if (name == "x2") l->x2 = value;
        else if (name == "y2") l->y2 = value;
    } else if (type == "polyline" && name == "points") {
        static_cast<SvgPolyline*>(this)->points = value;
    } else if (type == "polygon" && name == "points") {
        static_cast<SvgPolygon*>(this)->points = value;
    }
    markSvgDirty();
}

Svg::~Svg() {
}

void Svg::setAttribute(const std::string& name, const std::string& value) {
    Widget::setAttribute(name, value);
    if (name == "viewBox") viewBox = value;
    else if (name == "width") {
        width = value;
        css("width: " + value + (value.find('%') == std::string::npos && value.find("px") == std::string::npos ? "px" : "") + ";");
    }
    else if (name == "height") {
        height = value;
        css("height: " + value + (value.find('%') == std::string::npos && value.find("px") == std::string::npos ? "px" : "") + ";");
    }
    else if (name == "preserveAspectRatio") preserveAspectRatio = value;
    isRasterDirty = true;
    if (auto* app = Application::instance()) {
        app->requestRedraw();
    }
}

void Svg::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    for (auto& child : children) {
        child->bounds = bounds;
        child->layout(bounds);
    }
}

void Svg::render(Renderer& renderer) {
    if (!visible) return;

    int outW = std::clamp((int)std::round(bounds.w), 1, 4096);
    int outH = std::clamp((int)std::round(bounds.h), 1, 4096);

    if (isRasterDirty || cachedImage.width != outW || cachedImage.height != outH) {
        renderer.rasterizeSvgWidget(this, cachedImage);
        isRasterDirty = false;

        if (loadedTextureKey.empty()) {
            loadedTextureKey = "svg_dyn_" + std::to_string((uintptr_t)this);
        }
        renderer.updateDynamicTexture(loadedTextureKey, cachedImage);
    }

    renderer.drawImage(loadedTextureKey, bounds);
}

} // namespace FluxUI
