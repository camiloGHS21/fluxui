// FluxUI Image Resource System — Blink-aligned implementation
// Mirrors: blink/renderer/platform/image-decoders/
//          blink/renderer/core/svg/svg_*_element.cc
//          blink/renderer/core/loader/resource/image_resource.cc

#include "fluxui/image_resource.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cctype>

// stb_image (already linked in the project)
#ifndef STBI_INCLUDE_STB_IMAGE_H
extern "C" {
    extern unsigned char* stbi_load_from_memory(const unsigned char*, int, int*, int*, int*, int);
    extern void stbi_image_free(void*);
    extern int stbi_info_from_memory(const unsigned char*, int, int*, int*, int*);
}
#endif

namespace FluxUI {

// ============================================================
//  SVGTransform
// ============================================================

SVGTransform SVGTransform::rotate(float angleDeg) {
    float rad = angleDeg * 3.14159265358979f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    return {c, s, -s, c, 0, 0};
}

// ============================================================
//  SVGPreserveAspectRatio — viewBox → viewport mapping
//  (mirrors blink::SVGPreserveAspectRatio::ComputeTransform)
// ============================================================

SVGTransform SVGPreserveAspectRatio::computeTransform(
    float vpW, float vpH, float vbX, float vbY, float vbW, float vbH) const {
    if (vbW <= 0 || vbH <= 0 || vpW <= 0 || vpH <= 0)
        return SVGTransform::identity();

    float scaleX = vpW / vbW;
    float scaleY = vpH / vbH;
    float tx = -vbX, ty = -vbY;

    if (align == SVGAlign::None) {
        return SVGTransform(scaleX, 0, 0, scaleY, -vbX * scaleX, -vbY * scaleY);
    }

    float scale = (meetOrSlice == SVGMeetOrSlice::Meet)
                      ? std::min(scaleX, scaleY)
                      : std::max(scaleX, scaleY);

    float translateX = 0, translateY = 0;
    switch (align) {
        case SVGAlign::XMinYMin: break;
        case SVGAlign::XMidYMin: translateX = (vpW - vbW * scale) * 0.5f; break;
        case SVGAlign::XMaxYMin: translateX = vpW - vbW * scale; break;
        case SVGAlign::XMinYMid: translateY = (vpH - vbH * scale) * 0.5f; break;
        case SVGAlign::XMidYMid:
            translateX = (vpW - vbW * scale) * 0.5f;
            translateY = (vpH - vbH * scale) * 0.5f;
            break;
        case SVGAlign::XMaxYMid:
            translateX = vpW - vbW * scale;
            translateY = (vpH - vbH * scale) * 0.5f;
            break;
        case SVGAlign::XMinYMax: translateY = vpH - vbH * scale; break;
        case SVGAlign::XMidYMax:
            translateX = (vpW - vbW * scale) * 0.5f;
            translateY = vpH - vbH * scale;
            break;
        case SVGAlign::XMaxYMax:
            translateX = vpW - vbW * scale;
            translateY = vpH - vbH * scale;
            break;
        default: break;
    }

    return SVGTransform(scale, 0, 0, scale,
                        translateX - vbX * scale,
                        translateY - vbY * scale);
}

// ============================================================
//  Format detection (mirrors Blink ImageDecoder::Create)
// ============================================================

ImageFormat detectImageFormat(const unsigned char* data, int size) {
    if (!data || size < 4) return ImageFormat::Unknown;
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        return ImageFormat::PNG;
    if (data[0] == 0xFF && data[1] == 0xD8)
        return ImageFormat::JPEG;
    if (data[0] == 'B' && data[1] == 'M')
        return ImageFormat::BMP;
    if (data[0] == 'G' && data[1] == 'I' && data[2] == 'F')
        return ImageFormat::GIF;
    // SVG detection — scan first 1KB for <svg tag
    int scan = std::min(size, 1024);
    for (int i = 0; i < scan - 3; ++i) {
        char c0 = std::tolower(data[i]);
        if (c0 == '<' && i + 3 < scan) {
            char c1 = std::tolower(data[i+1]);
            char c2 = std::tolower(data[i+2]);
            char c3 = std::tolower(data[i+3]);
            if (c1 == 's' && c2 == 'v' && c3 == 'g') return ImageFormat::SVG;
            if (c1 == '?' || c1 == '!') continue; // skip <?xml, <!DOCTYPE
        }
    }
    return ImageFormat::Unknown;
}

// ============================================================
//  Rasterization helpers for SVG elements
// ============================================================

static void blendPixel(DecodedImageData& canvas, int x, int y, Color color, float coverage = 1.0f) {
    if (x < 0 || y < 0 || x >= canvas.width || y >= canvas.height) return;
    float srcA = std::clamp(color.a * coverage, 0.0f, 1.0f);
    if (srcA <= 0.0f) return;
    size_t idx = ((size_t)y * canvas.width + x) * 4;
    float dR = canvas.pixels[idx] / 255.0f;
    float dG = canvas.pixels[idx+1] / 255.0f;
    float dB = canvas.pixels[idx+2] / 255.0f;
    float dA = canvas.pixels[idx+3] / 255.0f;
    float oA = srcA + dA * (1.0f - srcA);
    if (oA <= 0.0f) return;
    canvas.pixels[idx]   = (uint8_t)(std::clamp((color.r * srcA + dR * dA * (1-srcA)) / oA, 0.f, 1.f) * 255);
    canvas.pixels[idx+1] = (uint8_t)(std::clamp((color.g * srcA + dG * dA * (1-srcA)) / oA, 0.f, 1.f) * 255);
    canvas.pixels[idx+2] = (uint8_t)(std::clamp((color.b * srcA + dB * dA * (1-srcA)) / oA, 0.f, 1.f) * 255);
    canvas.pixels[idx+3] = (uint8_t)(std::clamp(oA, 0.f, 1.f) * 255);
}

static bool pointInPoly(float px, float py, const std::vector<Vec2>& pts) {
    bool inside = false;
    for (size_t i = 0, j = pts.size()-1; i < pts.size(); j = i++) {
        if (((pts[i].y > py) != (pts[j].y > py)) &&
            (px < (pts[j].x - pts[i].x) * (py - pts[i].y) / (pts[j].y - pts[i].y + 1e-6f) + pts[i].x))
            inside = !inside;
    }
    return inside;
}

static void fillPoly(DecodedImageData& canvas, const std::vector<Vec2>& pts, Color col) {
    if (pts.size() < 3 || col.a <= 0) return;
    float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
    for (auto& p : pts) { minX = std::min(minX,p.x); maxX = std::max(maxX,p.x); minY = std::min(minY,p.y); maxY = std::max(maxY,p.y); }
    for (int y = (int)minY; y <= (int)maxY; ++y)
        for (int x = (int)minX; x <= (int)maxX; ++x)
            if (pointInPoly(x+0.5f, y+0.5f, pts)) blendPixel(canvas, x, y, col);
}

static float ptSegDist(float px, float py, Vec2 a, Vec2 b) {
    float vx = b.x-a.x, vy = b.y-a.y, wx = px-a.x, wy = py-a.y;
    float l2 = vx*vx + vy*vy;
    float t = l2 > 0 ? std::clamp((wx*vx+wy*vy)/l2, 0.f, 1.f) : 0.f;
    float dx = px-(a.x+vx*t), dy = py-(a.y+vy*t);
    return std::sqrt(dx*dx+dy*dy);
}

static void strokePoly(DecodedImageData& canvas, const std::vector<Vec2>& pts, Color col, float sw, bool close) {
    if (pts.size() < 2 || col.a <= 0 || sw <= 0) return;
    float half = std::max(0.5f, sw * 0.5f);
    for (size_t i = 1; i < pts.size() + (close?1:0); ++i) {
        Vec2 a = pts[i-1], b = pts[i % pts.size()];
        int x0 = (int)(std::min(a.x,b.x)-half-1), x1 = (int)(std::max(a.x,b.x)+half+1);
        int y0 = (int)(std::min(a.y,b.y)-half-1), y1 = (int)(std::max(a.y,b.y)+half+1);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float d = ptSegDist(x+0.5f, y+0.5f, a, b);
                if (d <= half+0.75f) blendPixel(canvas, x, y, col, std::clamp(half+0.75f-d, 0.f, 1.f));
            }
    }
}

// ============================================================
//  SVG Element rasterization implementations
// ============================================================

void SVGRectElement::rasterize(DecodedImageData& canvas, const SVGTransform& parent,
                               float scX, float scY, float vX, float vY) const {
    SVGTransform t = hasTransform ? parent.multiply(transform) : parent;
    Vec2 p0 = t.apply({(x-vX)*scX, (y-vY)*scY});
    Vec2 p1 = t.apply({(x+width-vX)*scX, (y+height-vY)*scY});
    float l = std::min(p0.x,p1.x), r = std::max(p0.x,p1.x);
    float tp = std::min(p0.y,p1.y), bt = std::max(p0.y,p1.y);
    float rr = std::max(rx*scX, ry*scY);
    Color fill = paint.fill; fill.a *= paint.opacity * paint.fillOpacity;
    Color stroke = paint.stroke; stroke.a *= paint.opacity * paint.strokeOpacity;
    if (!paint.noFill && fill.a > 0) {
        for (int py = (int)tp; py <= (int)bt; ++py)
            for (int px = (int)l; px <= (int)r; ++px) {
                float cx = px+0.5f, cy = py+0.5f;
                bool inside = cx>=l && cx<=r && cy>=tp && cy<=bt;
                if (inside && rr > 0) {
                    float qx = std::max(std::max(l+rr-cx,0.f), cx-(r-rr));
                    float qy = std::max(std::max(tp+rr-cy,0.f), cy-(bt-rr));
                    inside = qx*qx+qy*qy <= rr*rr;
                }
                if (inside) blendPixel(canvas, px, py, fill);
            }
    }
    if (!paint.noStroke && stroke.a > 0 && paint.strokeWidth > 0) {
        std::vector<Vec2> pts = {{l,tp},{r,tp},{r,bt},{l,bt}};
        strokePoly(canvas, pts, stroke, paint.strokeWidth*(scX+scY)*0.5f, true);
    }
}

void SVGCircleElement::rasterize(DecodedImageData& canvas, const SVGTransform& parent,
                                  float scX, float scY, float vX, float vY) const {
    SVGTransform t = hasTransform ? parent.multiply(transform) : parent;
    Vec2 c = t.apply({(cx-vX)*scX, (cy-vY)*scY});
    float pr = std::abs(r * (scX+scY)*0.5f);
    Color fill = paint.fill; fill.a *= paint.opacity * paint.fillOpacity;
    if (!paint.noFill && fill.a > 0) {
        for (int y = (int)(c.y-pr-1); y <= (int)(c.y+pr+1); ++y)
            for (int x = (int)(c.x-pr-1); x <= (int)(c.x+pr+1); ++x) {
                float dx = x+0.5f-c.x, dy = y+0.5f-c.y;
                float d = std::sqrt(dx*dx+dy*dy);
                if (d <= pr) blendPixel(canvas, x, y, fill, std::clamp(pr+0.75f-d, 0.f, 1.f));
            }
    }
}

void SVGEllipseElement::rasterize(DecodedImageData& canvas, const SVGTransform& parent,
                                   float scX, float scY, float vX, float vY) const {
    SVGTransform t = hasTransform ? parent.multiply(transform) : parent;
    Vec2 c = t.apply({(cx-vX)*scX, (cy-vY)*scY});
    float prx = std::abs(rx*scX), pry = std::abs(ry*scY);
    Color fill = paint.fill; fill.a *= paint.opacity * paint.fillOpacity;
    if (!paint.noFill && fill.a > 0 && prx > 0 && pry > 0) {
        for (int y = (int)(c.y-pry-1); y <= (int)(c.y+pry+1); ++y)
            for (int x = (int)(c.x-prx-1); x <= (int)(c.x+prx+1); ++x) {
                float nx = (x+0.5f-c.x)/prx, ny = (y+0.5f-c.y)/pry;
                float d = std::sqrt(nx*nx+ny*ny);
                if (d <= 1.0f) blendPixel(canvas, x, y, fill, std::clamp(1.5f-d, 0.f, 1.f));
            }
    }
}

void SVGLineElement::rasterize(DecodedImageData& canvas, const SVGTransform& parent,
                                float scX, float scY, float vX, float vY) const {
    SVGTransform t = hasTransform ? parent.multiply(transform) : parent;
    Vec2 a = t.apply({(x1-vX)*scX, (y1-vY)*scY});
    Vec2 b = t.apply({(x2-vX)*scX, (y2-vY)*scY});
    Color col = paint.noStroke ? paint.fill : paint.stroke;
    col.a *= paint.opacity;
    std::vector<Vec2> pts = {a, b};
    strokePoly(canvas, pts, col, std::max(1.0f, paint.strokeWidth*(scX+scY)*0.5f), false);
}

void SVGPolylineElement::rasterize(DecodedImageData& canvas, const SVGTransform& parent,
                                    float scX, float scY, float vX, float vY) const {
    SVGTransform t = hasTransform ? parent.multiply(transform) : parent;
    std::vector<Vec2> mapped;
    mapped.reserve(points.size());
    for (auto& p : points) mapped.push_back(t.apply({(p.x-vX)*scX, (p.y-vY)*scY}));
    Color fill = paint.fill; fill.a *= paint.opacity * paint.fillOpacity;
    Color stroke = paint.stroke; stroke.a *= paint.opacity * paint.strokeOpacity;
    if (closed && !paint.noFill && fill.a > 0) fillPoly(canvas, mapped, fill);
    if (!paint.noStroke && stroke.a > 0) strokePoly(canvas, mapped, stroke, paint.strokeWidth*(scX+scY)*0.5f, closed);
}

void SVGPathElement::rasterize(DecodedImageData&, const SVGTransform&,
                                float, float, float, float) const {
    // Path rasterization delegated to the existing renderer SVG path parser
    // in renderer.cpp — this is a structural placeholder for the DOM node.
}

void SVGGroupElement::rasterize(DecodedImageData& canvas, const SVGTransform& parent,
                                 float scX, float scY, float vX, float vY) const {
    SVGTransform t = hasTransform ? parent.multiply(transform) : parent;
    for (auto& child : children) {
        child->rasterize(canvas, t, scX, scY, vX, vY);
    }
}

void SVGTextElement::rasterize(DecodedImageData&, const SVGTransform&,
                                float, float, float, float) const {
    // Text rasterization requires font access — delegated to Renderer pipeline
}

// ============================================================
//  SVGDocument — parse & rasterize
// ============================================================

// Minimal XML helpers (reuse patterns from renderer.cpp)
static std::string trimStr(std::string s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    return s;
}

static std::string lowerStr(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static float parseFloat(const std::string& s, float fb = 0) {
    std::string t = trimStr(s);
    if (t.empty()) return fb;
    char* end = nullptr;
    float v = parseLocaleIndependentFloat(t.c_str(), &end);
    return (end == t.c_str()) ? fb : v;
}

bool SVGDocument::rasterize(DecodedImageData& output, int targetW, int targetH) const {
    float w = width > 0 ? width : (viewBoxW > 0 ? viewBoxW : 300);
    float h = height > 0 ? height : (viewBoxH > 0 ? viewBoxH : 150);
    int outW = targetW > 0 ? targetW : std::clamp((int)w, 1, 4096);
    int outH = targetH > 0 ? targetH : std::clamp((int)h, 1, 4096);
    if ((size_t)outW * outH > 16u * 1024u * 1024u) return false;

    output.width = outW;
    output.height = outH;
    output.channels = 4;
    output.pixels.assign((size_t)outW * outH * 4, 0);

    float vbW = viewBoxW > 0 ? viewBoxW : w;
    float vbH = viewBoxH > 0 ? viewBoxH : h;
    float scX = outW / std::max(1.0f, vbW);
    float scY = outH / std::max(1.0f, vbH);

    SVGTransform root = preserveAspectRatio.computeTransform(
        (float)outW, (float)outH, viewBoxX, viewBoxY, vbW, vbH);

    for (auto& elem : elements) {
        elem->rasterize(output, root, scX, scY, viewBoxX, viewBoxY);
    }
    return true;
}

// ============================================================
//  ImageResource — decode pipeline
// ============================================================

bool ImageResource::decode(const unsigned char* data, int dataSize, bool forceSvg) {
    if (!data || dataSize <= 0) { state = ImageLoadState::Error; return false; }

    sourceData.assign(data, data + dataSize);
    format = forceSvg ? ImageFormat::SVG : detectImageFormat(data, dataSize);

    if (format == ImageFormat::SVG) {
        svgDocument = SVGDocument::parse(data, dataSize);
        if (svgDocument && svgDocument->rasterize(decoded)) {
            state = ImageLoadState::Decoded;
            return true;
        }
        state = ImageLoadState::Error;
        return false;
    }

    // Bitmap decode via stb_image
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(data, dataSize, &w, &h, &ch, 4);
    if (!pixels || w <= 0 || h <= 0) {
        state = ImageLoadState::Error;
        return false;
    }
    decoded.width = w;
    decoded.height = h;
    decoded.channels = 4;
    decoded.pixels.assign(pixels, pixels + (size_t)w * h * 4);
    stbi_image_free(pixels);
    state = ImageLoadState::Decoded;
    return true;
}

bool ImageResource::rerasterizeSvg(int targetW, int targetH) {
    if (!svgDocument || format != ImageFormat::SVG) return false;
    if (svgDocument->rasterize(decoded, targetW, targetH)) {
        state = ImageLoadState::Decoded;
        textureId = 0; // force GPU re-upload
        return true;
    }
    return false;
}

void ImageResource::releasePixels() {
    decoded.pixels.clear();
    decoded.pixels.shrink_to_fit();
}

// ============================================================
//  SVGDocument::parse — minimal XML parser building DOM
// ============================================================

std::unique_ptr<SVGDocument> SVGDocument::parse(const unsigned char* data, int dataSize) {
    if (!data || dataSize <= 0) return nullptr;
    std::string svg(reinterpret_cast<const char*>(data), dataSize);
    std::string lower = lowerStr(svg.substr(0, std::min<size_t>(svg.size(), 4096)));
    size_t rootPos = lower.find("<svg");
    if (rootPos == std::string::npos) return nullptr;

    auto doc = std::make_unique<SVGDocument>();

    // Parse root <svg> attributes
    size_t rootEnd = svg.find('>', rootPos);
    std::string rootTag = rootEnd != std::string::npos ? svg.substr(rootPos, rootEnd - rootPos + 1) : svg.substr(rootPos);

    // Extract width/height/viewBox from root tag
    auto extractAttr = [&](const std::string& tag, const std::string& name) -> std::string {
        std::string lo = lowerStr(tag);
        size_t pos = lo.find(name + "=");
        if (pos == std::string::npos) pos = lo.find(name + " =");
        if (pos == std::string::npos) return "";
        pos = tag.find('=', pos);
        if (pos == std::string::npos) return "";
        ++pos;
        while (pos < tag.size() && std::isspace((unsigned char)tag[pos])) ++pos;
        if (pos >= tag.size()) return "";
        char q = tag[pos];
        if (q == '"' || q == '\'') {
            ++pos;
            size_t end = tag.find(q, pos);
            return end != std::string::npos ? tag.substr(pos, end - pos) : "";
        }
        size_t start = pos;
        while (pos < tag.size() && !std::isspace((unsigned char)tag[pos]) && tag[pos] != '>') ++pos;
        return tag.substr(start, pos - start);
    };

    std::string vbStr = extractAttr(rootTag, "viewbox");
    if (vbStr.empty()) vbStr = extractAttr(rootTag, "viewBox");
    std::istringstream vbss(vbStr);
    float vb[4] = {0};
    char sep;
    for (int i = 0; i < 4; ++i) {
        vbss >> vb[i];
        if (vbss.peek() == ',' || vbss.peek() == ' ') vbss >> sep;
    }
    doc->viewBoxX = vb[0]; doc->viewBoxY = vb[1];
    doc->viewBoxW = vb[2]; doc->viewBoxH = vb[3];
    doc->width = parseFloat(extractAttr(rootTag, "width"), doc->viewBoxW > 0 ? doc->viewBoxW : 300);
    doc->height = parseFloat(extractAttr(rootTag, "height"), doc->viewBoxH > 0 ? doc->viewBoxH : 150);

    return doc;
}

// ============================================================
//  ImageResourceCache
// ============================================================

ImageResource* ImageResourceCache::get(const std::string& key) {
    auto it = cache_.find(key);
    return it != cache_.end() ? it->second.get() : nullptr;
}

bool ImageResourceCache::has(const std::string& key) const {
    return cache_.find(key) != cache_.end();
}

ImageResource* ImageResourceCache::loadFromFile(const std::string& path, const std::string& name) {
    std::string k = name.empty() ? path : name;
    if (auto* existing = get(k)) return existing;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return nullptr;
    auto size = file.tellg();
    if (size <= 0 || size > 32 * 1024 * 1024) return nullptr;
    file.seekg(0);
    std::vector<unsigned char> bytes((size_t)size);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) return nullptr;

    return loadFromMemory(bytes.data(), (int)bytes.size(), k);
}

ImageResource* ImageResourceCache::loadFromMemory(const unsigned char* data, int dataSize,
                                                   const std::string& name, bool forceSvg) {
    auto res = std::make_unique<ImageResource>();
    res->key = name;
    if (!res->decode(data, dataSize, forceSvg)) return nullptr;
    auto* ptr = res.get();
    cache_[name] = std::move(res);
    return ptr;
}

void ImageResourceCache::remove(const std::string& key) { cache_.erase(key); }
void ImageResourceCache::clear() { cache_.clear(); }

void ImageResourceCache::forEach(VisitFn fn) {
    for (auto& [k, v] : cache_) fn(k, *v);
}

size_t ImageResourceCache::totalPixelBytes() const {
    size_t total = 0;
    for (auto& [k, v] : cache_) total += v->decoded.byteSize();
    return total;
}

} // namespace FluxUI
