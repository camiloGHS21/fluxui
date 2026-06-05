// FluxUI - SVG rasterization geometry + raster primitives (internal).
//
// The SVG affine/canvas/paint types and the low-level fill/stroke/path
// primitives shared between renderer_svg.cpp (parser + document walk) live here
// as inline FluxUI::detail definitions, mirroring software_internal.h. Private
// to fluxui/src/render; never installed.
#pragma once
#include "fluxui/core.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace FluxUI {
namespace detail {

struct SvgAffine {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, e = 0.0f, f = 0.0f;

    static SvgAffine identity() { return {}; }
    static SvgAffine translate(float tx, float ty) { return {1.0f, 0.0f, 0.0f, 1.0f, tx, ty}; }
    static SvgAffine scale(float sx, float sy) { return {sx, 0.0f, 0.0f, sy, 0.0f, 0.0f}; }
    static SvgAffine rotate(float degrees) {
        float radians = degrees * 3.14159265358979323846f / 180.0f;
        float cs = std::cos(radians);
        float sn = std::sin(radians);
        return {cs, sn, -sn, cs, 0.0f, 0.0f};
    }
    static SvgAffine skewX(float degrees) {
        float radians = degrees * 3.14159265358979323846f / 180.0f;
        return {1.0f, 0.0f, std::tan(radians), 1.0f, 0.0f, 0.0f};
    }
    static SvgAffine skewY(float degrees) {
        float radians = degrees * 3.14159265358979323846f / 180.0f;
        return {1.0f, std::tan(radians), 0.0f, 1.0f, 0.0f, 0.0f};
    }

    Vec2 apply(float x, float y) const {
        return {a * x + c * y + e, b * x + d * y + f};
    }

    SvgAffine multiply(const SvgAffine& other) const {
        return {
            a * other.a + c * other.b,
            b * other.a + d * other.b,
            a * other.c + c * other.d,
            b * other.c + d * other.d,
            a * other.e + c * other.f + e,
            b * other.e + d * other.f + f
        };
    }

    bool axisAligned() const {
        return std::abs(b) < 0.0001f && std::abs(c) < 0.0001f;
    }
};

struct SvgCanvas {
    std::vector<unsigned char>* pixels = nullptr;
    int width = 0;
    int height = 0;
    float viewX = 0.0f;
    float viewY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    SvgAffine transform;
};

inline Vec2 svgMapPoint(const SvgCanvas& canvas, float x, float y) {
    Vec2 transformed = canvas.transform.apply(x, y);
    return {canvas.offsetX + (transformed.x - canvas.viewX) * canvas.scaleX,
            canvas.offsetY + (transformed.y - canvas.viewY) * canvas.scaleY};
}

struct SvgAspectRatio {
    bool none = false;
    bool slice = false;
    float alignX = 0.5f;
    float alignY = 0.5f;
};

struct SvgPaintState {
    std::string fill = "black";
    std::string stroke = "none";
    std::string opacity = "1";
    std::string fillOpacity = "1";
    std::string strokeOpacity = "1";
    std::string strokeWidth = "1";
    std::string color = "black";
    std::string fillRule = "nonzero"; // SVG default is nonzero, not evenodd
};

// Bezier/arc flattening helpers (defined below; forward-declared for the shape
// primitives that reference them).
inline void appendCubic(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3);
inline void appendQuad(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2);
inline void appendArc(std::vector<Vec2>& out, Vec2 p0, float rx, float ry, float angle, bool largeArcFlag, bool sweepFlag, Vec2 p1, SvgCanvas& canvas);

inline void svgBlendPixel(SvgCanvas& canvas, int x, int y, Color color, float coverage = 1.0f) {
    if (!canvas.pixels || x < 0 || y < 0 || x >= canvas.width || y >= canvas.height) return;
    float srcA = std::clamp(color.a * coverage, 0.0f, 1.0f);
    if (srcA <= 0.0f) return;
    size_t index = ((size_t)y * (size_t)canvas.width + (size_t)x) * 4;
    float dstR = (*canvas.pixels)[index + 0] / 255.0f;
    float dstG = (*canvas.pixels)[index + 1] / 255.0f;
    float dstB = (*canvas.pixels)[index + 2] / 255.0f;
    float dstA = (*canvas.pixels)[index + 3] / 255.0f;
    float outA = srcA + dstA * (1.0f - srcA);
    if (outA <= 0.0f) return;
    float outR = (color.r * srcA + dstR * dstA * (1.0f - srcA)) / outA;
    float outG = (color.g * srcA + dstG * dstA * (1.0f - srcA)) / outA;
    float outB = (color.b * srcA + dstB * dstA * (1.0f - srcA)) / outA;
    (*canvas.pixels)[index + 0] = (unsigned char)std::round(std::clamp(outR, 0.0f, 1.0f) * 255.0f);
    (*canvas.pixels)[index + 1] = (unsigned char)std::round(std::clamp(outG, 0.0f, 1.0f) * 255.0f);
    (*canvas.pixels)[index + 2] = (unsigned char)std::round(std::clamp(outB, 0.0f, 1.0f) * 255.0f);
    (*canvas.pixels)[index + 3] = (unsigned char)std::round(std::clamp(outA, 0.0f, 1.0f) * 255.0f);
}

inline float pointSegmentDistance(float px, float py, Vec2 a, Vec2 b) {
    float vx = b.x - a.x;
    float vy = b.y - a.y;
    float wx = px - a.x;
    float wy = py - a.y;
    float len2 = vx * vx + vy * vy;
    float t = len2 > 0.0f ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
    float dx = px - (a.x + vx * t);
    float dy = py - (a.y + vy * t);
    return std::sqrt(dx * dx + dy * dy);
}

inline void strokePolyline(SvgCanvas& canvas, const std::vector<Vec2>& points, Color color, float strokeWidth, bool close) {
    if (points.size() < 2 || color.a <= 0.0f || strokeWidth <= 0.0f) return;
    float half = std::max(0.5f, strokeWidth * 0.5f);
    for (size_t i = 1; i < points.size() + (close ? 1 : 0); ++i) {
        Vec2 a = points[i - 1];
        Vec2 b = points[i % points.size()];
        int minX = (int)std::floor(std::min(a.x, b.x) - half - 1.0f);
        int maxX = (int)std::ceil(std::max(a.x, b.x) + half + 1.0f);
        int minY = (int)std::floor(std::min(a.y, b.y) - half - 1.0f);
        int maxY = (int)std::ceil(std::max(a.y, b.y) + half + 1.0f);
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float d = pointSegmentDistance(x + 0.5f, y + 0.5f, a, b);
                if (d <= half + 0.75f) {
                    float coverage = std::clamp(half + 0.75f - d, 0.0f, 1.0f);
                    svgBlendPixel(canvas, x, y, color, coverage);
                }
            }
        }
    }
}

inline bool pointInPolygon(float x, float y, const std::vector<Vec2>& points) {
    bool inside = false;
    if (points.size() < 3) return false;
    for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
        const Vec2& pi = points[i];
        const Vec2& pj = points[j];
        bool crosses = ((pi.y > y) != (pj.y > y)) &&
                       (x < (pj.x - pi.x) * (y - pi.y) / ((pj.y - pi.y) + 0.000001f) + pi.x);
        if (crosses) inside = !inside;
    }
    return inside;
}

inline void fillPolygon(SvgCanvas& canvas, const std::vector<Vec2>& points, Color color) {
    if (points.size() < 3 || color.a <= 0.0f) return;
    float minX = points[0].x, maxX = points[0].x, minY = points[0].y, maxY = points[0].y;
    for (const Vec2& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    int startY = std::max(0, (int)std::floor(minY));
    int endY = std::min(canvas.height - 1, (int)std::ceil(maxY));
    int startX = std::max(0, (int)std::floor(minX));
    int endX = std::min(canvas.width - 1, (int)std::ceil(maxX));
    if (startY > endY || startX > endX) return;

    struct Edge {
        float x1, y1, x2, y2;
        int dir; // +1 if y1<y2 (upward), -1 if y1>y2 (downward) — for nonzero winding
    };
    std::vector<Edge> edges;
    edges.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        Vec2 p1 = points[i];
        Vec2 p2 = points[(i + 1) % points.size()];
        if (std::abs(p1.y - p2.y) > 0.0001f) {
            int dir = (p1.y < p2.y) ? 1 : -1;
            edges.push_back({p1.x, p1.y, p2.x, p2.y, dir});
        }
    }

    std::vector<float> intersections1;
    std::vector<float> intersections2;
    intersections1.reserve(16);
    intersections2.reserve(16);

    for (int y = startY; y <= endY; ++y) {
        float y_sub1 = y + 0.25f;
        float y_sub2 = y + 0.75f;
        intersections1.clear();
        intersections2.clear();

        for (const auto& edge : edges) {
            if ((edge.y1 <= y_sub1 && edge.y2 > y_sub1) || (edge.y2 <= y_sub1 && edge.y1 > y_sub1)) {
                float t = (y_sub1 - edge.y1) / (edge.y2 - edge.y1);
                intersections1.push_back(edge.x1 + t * (edge.x2 - edge.x1));
            }
            if ((edge.y1 <= y_sub2 && edge.y2 > y_sub2) || (edge.y2 <= y_sub2 && edge.y1 > y_sub2)) {
                float t = (y_sub2 - edge.y1) / (edge.y2 - edge.y1);
                intersections2.push_back(edge.x1 + t * (edge.x2 - edge.x1));
            }
        }

        std::sort(intersections1.begin(), intersections1.end());
        std::sort(intersections2.begin(), intersections2.end());

        auto isInside = [](float x_sub, const std::vector<float>& intersections) -> bool {
            for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                if (x_sub >= intersections[i] && x_sub <= intersections[i + 1]) {
                    return true;
                }
            }
            return false;
        };

        for (int x = startX; x <= endX; ++x) {
            float x_sub1 = x + 0.25f;
            float x_sub2 = x + 0.75f;
            int insideCount = 0;

            if (isInside(x_sub1, intersections1)) insideCount++;
            if (isInside(x_sub2, intersections1)) insideCount++;
            if (isInside(x_sub1, intersections2)) insideCount++;
            if (isInside(x_sub2, intersections2)) insideCount++;

            if (insideCount > 0) {
                svgBlendPixel(canvas, x, y, color, insideCount / 4.0f);
            }
        }
    }
}

// Multi-path fill with fill-rule support (nonzero winding / evenodd)
// This is the Blink-parity critical function: it collects edges from ALL subpaths
// so that holes (inner subpaths with opposite winding) are correctly subtracted.
inline void fillMultiPolygon(SvgCanvas& canvas, const std::vector<std::vector<Vec2>>& subpaths,
                     Color color, bool useEvenOdd) {
    if (color.a <= 0.0f) return;

    struct Edge {
        float x1, y1, x2, y2;
        int dir; // +1 going down, -1 going up (for nonzero winding)
    };

    // Collect edges from ALL subpaths and compute global bounding box
    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    std::vector<Edge> edges;
    size_t totalPts = 0;
    for (const auto& path : subpaths) totalPts += path.size();
    edges.reserve(totalPts);

    for (const auto& path : subpaths) {
        if (path.size() < 3) continue;
        for (size_t i = 0; i < path.size(); ++i) {
            const Vec2& p1 = path[i];
            const Vec2& p2 = path[(i + 1) % path.size()];
            minX = std::min(minX, std::min(p1.x, p2.x));
            maxX = std::max(maxX, std::max(p1.x, p2.x));
            minY = std::min(minY, std::min(p1.y, p2.y));
            maxY = std::max(maxY, std::max(p1.y, p2.y));
            if (std::abs(p1.y - p2.y) > 0.0001f) {
                int dir = (p1.y < p2.y) ? 1 : -1;
                edges.push_back({p1.x, p1.y, p2.x, p2.y, dir});
            }
        }
    }

    if (edges.empty()) return;

    int startY = std::max(0, (int)std::floor(minY));
    int endY = std::min(canvas.height - 1, (int)std::ceil(maxY));
    int startX = std::max(0, (int)std::floor(minX));
    int endX = std::min(canvas.width - 1, (int)std::ceil(maxX));
    if (startY > endY || startX > endX) return;

    // For nonzero winding, store intersection x-values paired with direction
    struct Crossing {
        float x;
        int dir;
    };
    std::vector<Crossing> crossings1, crossings2;
    crossings1.reserve(32);
    crossings2.reserve(32);

    for (int y = startY; y <= endY; ++y) {
        float y_sub1 = y + 0.25f;
        float y_sub2 = y + 0.75f;
        crossings1.clear();
        crossings2.clear();

        for (const auto& edge : edges) {
            if ((edge.y1 <= y_sub1 && edge.y2 > y_sub1) || (edge.y2 <= y_sub1 && edge.y1 > y_sub1)) {
                float t = (y_sub1 - edge.y1) / (edge.y2 - edge.y1);
                crossings1.push_back({edge.x1 + t * (edge.x2 - edge.x1), edge.dir});
            }
            if ((edge.y1 <= y_sub2 && edge.y2 > y_sub2) || (edge.y2 <= y_sub2 && edge.y1 > y_sub2)) {
                float t = (y_sub2 - edge.y1) / (edge.y2 - edge.y1);
                crossings2.push_back({edge.x1 + t * (edge.x2 - edge.x1), edge.dir});
            }
        }

        // Sort crossings by x position
        auto cmpCrossing = [](const Crossing& a, const Crossing& b) { return a.x < b.x; };
        std::sort(crossings1.begin(), crossings1.end(), cmpCrossing);
        std::sort(crossings2.begin(), crossings2.end(), cmpCrossing);

        // Lambda to check if subpixel X is inside using the selected fill rule
        auto isInsideFn = [useEvenOdd](float x_sub, const std::vector<Crossing>& crossings) -> bool {
            if (useEvenOdd) {
                // Even-odd: count crossings to the left; odd = inside
                int count = 0;
                for (const auto& c : crossings) {
                    if (c.x > x_sub) break;
                    ++count;
                }
                return (count % 2) != 0;
            } else {
                // Nonzero winding: sum directions of crossings to the left; nonzero = inside
                int winding = 0;
                for (const auto& c : crossings) {
                    if (c.x > x_sub) break;
                    winding += c.dir;
                }
                return winding != 0;
            }
        };

        for (int x = startX; x <= endX; ++x) {
            float x_sub1 = x + 0.25f;
            float x_sub2 = x + 0.75f;
            int insideCount = 0;

            if (isInsideFn(x_sub1, crossings1)) insideCount++;
            if (isInsideFn(x_sub2, crossings1)) insideCount++;
            if (isInsideFn(x_sub1, crossings2)) insideCount++;
            if (isInsideFn(x_sub2, crossings2)) insideCount++;

            if (insideCount > 0) {
                svgBlendPixel(canvas, x, y, color, insideCount / 4.0f);
            }
        }
    }
}

inline void drawSvgRect(SvgCanvas& canvas, float x, float y, float w, float h,
                 float rx, float ry, Color fill, Color stroke, float strokeWidth) {
    if (w <= 0.0f || h <= 0.0f) return;
    
    // Clamp rx and ry according to SVG spec
    if (rx < 0.0f && ry < 0.0f) rx = ry = 0.0f;
    else if (rx >= 0.0f && ry < 0.0f) ry = rx;
    else if (ry >= 0.0f && rx < 0.0f) rx = ry;
    
    rx = std::min(rx, w / 2.0f);
    ry = std::min(ry, h / 2.0f);
    
    if (rx > 0.0f && ry > 0.0f) {
        // High-fidelity Bezier corner approximation (Chromium/Blink parity)
        std::vector<Vec2> points;
        const float K = 0.55228475f; // Cubic bezier kappa for elliptical arc approximation
        
        // 1. Top edge and top-right corner
        Vec2 tr_start = {x + rx, y};
        Vec2 tr_end = {x + w - rx, y};
        points.push_back(svgMapPoint(canvas, tr_start.x, tr_start.y));
        points.push_back(svgMapPoint(canvas, tr_end.x, tr_end.y));
        
        Vec2 cp1 = {x + w - rx * (1.0f - K), y};
        Vec2 cp2 = {x + w, y + ry * (1.0f - K)};
        Vec2 tr_curve_end = {x + w, y + ry};
        appendCubic(points,
                    svgMapPoint(canvas, tr_end.x, tr_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, tr_curve_end.x, tr_curve_end.y));
                    
        // 2. Right edge and bottom-right corner
        Vec2 br_end = {x + w, y + h - ry};
        points.push_back(svgMapPoint(canvas, br_end.x, br_end.y));
        
        cp1 = {x + w, y + h - ry * (1.0f - K)};
        cp2 = {x + w - rx * (1.0f - K), y + h};
        Vec2 br_curve_end = {x + w - rx, y + h};
        appendCubic(points,
                    svgMapPoint(canvas, br_end.x, br_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, br_curve_end.x, br_curve_end.y));
                    
        // 3. Bottom edge and bottom-left corner
        Vec2 bl_end = {x + rx, y + h};
        points.push_back(svgMapPoint(canvas, bl_end.x, bl_end.y));
        
        cp1 = {x + rx * (1.0f - K), y + h};
        cp2 = {x, y + h - ry * (1.0f - K)};
        Vec2 bl_curve_end = {x, y + h - ry};
        appendCubic(points,
                    svgMapPoint(canvas, bl_end.x, bl_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, bl_curve_end.x, bl_curve_end.y));
                    
        // 4. Left edge and top-left corner
        Vec2 tl_end = {x, y + ry};
        points.push_back(svgMapPoint(canvas, tl_end.x, tl_end.y));
        
        cp1 = {x, y + ry * (1.0f - K)};
        cp2 = {x + rx * (1.0f - K), y};
        Vec2 tl_curve_end = {x + rx, y};
        appendCubic(points,
                    svgMapPoint(canvas, tl_end.x, tl_end.y),
                    svgMapPoint(canvas, cp1.x, cp1.y),
                    svgMapPoint(canvas, cp2.x, cp2.y),
                    svgMapPoint(canvas, tl_curve_end.x, tl_curve_end.y));
                    
        if (fill.a > 0.0f) fillPolygon(canvas, points, fill);
        if (stroke.a > 0.0f && strokeWidth > 0.0f) {
            strokePolyline(canvas, points, stroke, strokeWidth, true);
        }
        return;
    }
    
    if (!canvas.transform.axisAligned()) {
        std::vector<Vec2> points = {
            svgMapPoint(canvas, x, y),
            svgMapPoint(canvas, x + w, y),
            svgMapPoint(canvas, x + w, y + h),
            svgMapPoint(canvas, x, y + h)
        };
        if (fill.a > 0.0f) fillPolygon(canvas, points, fill);
        if (stroke.a > 0.0f && strokeWidth > 0.0f) {
            strokePolyline(canvas, points, stroke, strokeWidth, true);
        }
        return;
    }
    Vec2 p0 = svgMapPoint(canvas, x, y);
    Vec2 p1 = svgMapPoint(canvas, x + w, y + h);
    float left = std::min(p0.x, p1.x);
    float right = std::max(p0.x, p1.x);
    float top = std::min(p0.y, p1.y);
    float bottom = std::max(p0.y, p1.y);
    for (int py = (int)std::floor(top); py <= (int)std::ceil(bottom); ++py) {
        for (int px = (int)std::floor(left); px <= (int)std::ceil(right); ++px) {
            float xOverlap = std::max(0.0f, std::min((float)px + 1.0f, right) - std::max((float)px, left));
            float yOverlap = std::max(0.0f, std::min((float)py + 1.0f, bottom) - std::max((float)py, top));
            float coverage = xOverlap * yOverlap;
            if (coverage > 0.0f && fill.a > 0.0f) {
                svgBlendPixel(canvas, px, py, fill, coverage);
            }
        }
    }
    if (stroke.a > 0.0f && strokeWidth > 0.0f) {
        std::vector<Vec2> points = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
        strokePolyline(canvas, points, stroke, strokeWidth, true);
    }
}

inline void drawSvgEllipse(SvgCanvas& canvas, float cx, float cy, float rx, float ry,
                    Color fill, Color stroke, float strokeWidth) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    
    std::vector<Vec2> points;
    const float K = 0.55228475f; // Cubic bezier kappa for elliptical arc approximation
    
    // Quadrant 1: Top-Right (from 0 to 90 deg / from (cx + rx, cy) to (cx, cy + ry))
    Vec2 p0 = {cx + rx, cy};
    Vec2 cp1 = {cx + rx, cy + ry * K};
    Vec2 cp2 = {cx + rx * K, cy + ry};
    Vec2 p1 = {cx, cy + ry};
    
    // Evaluate Q1
    appendCubic(points,
                svgMapPoint(canvas, p0.x, p0.y),
                svgMapPoint(canvas, cp1.x, cp1.y),
                svgMapPoint(canvas, cp2.x, cp2.y),
                svgMapPoint(canvas, p1.x, p1.y));
                
    // Quadrant 2: Bottom-Right (from (cx, cy + ry) to (cx - rx, cy))
    Vec2 cp3 = {cx - rx * K, cy + ry};
    Vec2 cp4 = {cx - rx, cy + ry * K};
    Vec2 p2 = {cx - rx, cy};
    
    // Evaluate Q2
    appendCubic(points,
                svgMapPoint(canvas, p1.x, p1.y),
                svgMapPoint(canvas, cp3.x, cp3.y),
                svgMapPoint(canvas, cp4.x, cp4.y),
                svgMapPoint(canvas, p2.x, p2.y));
                
    // Quadrant 3: Bottom-Left (from (cx - rx, cy) to (cx, cy - ry))
    Vec2 cp5 = {cx - rx, cy - ry * K};
    Vec2 cp6 = {cx - rx * K, cy - ry};
    Vec2 p3 = {cx, cy - ry};
    
    // Evaluate Q3
    appendCubic(points,
                svgMapPoint(canvas, p2.x, p2.y),
                svgMapPoint(canvas, cp5.x, cp5.y),
                svgMapPoint(canvas, cp6.x, cp6.y),
                svgMapPoint(canvas, p3.x, p3.y));
                
    // Quadrant 4: Top-Left (from (cx, cy - ry) to (cx + rx, cy))
    Vec2 cp7 = {cx + rx * K, cy - ry};
    Vec2 cp8 = {cx + rx, cy - ry * K};
    
    // Evaluate Q4
    appendCubic(points,
                svgMapPoint(canvas, p3.x, p3.y),
                svgMapPoint(canvas, cp7.x, cp7.y),
                svgMapPoint(canvas, cp8.x, cp8.y),
                svgMapPoint(canvas, p0.x, p0.y));
                
    if (fill.a > 0.0f) fillPolygon(canvas, points, fill);
    if (stroke.a > 0.0f && strokeWidth > 0.0f) {
        strokePolyline(canvas, points, stroke, strokeWidth, true);
    }
}

inline std::vector<float> parseSvgNumberList(const std::string& value) {
    std::vector<float> numbers;
    const char* p = value.c_str();
    char* end = nullptr;
    while (*p) {
        while (*p && (std::isspace((unsigned char)*p) || *p == ',')) ++p;
        if (!*p) break;
        float v = parseLocaleIndependentFloat(p, &end);
        if (end == p) {
            ++p;
            continue;
        }
        numbers.push_back(v);
        p = end;
    }
    return numbers;
}

inline std::vector<Vec2> parseSvgPoints(const std::string& value, const SvgCanvas& canvas) {
    std::vector<Vec2> points;
    std::vector<float> nums = parseSvgNumberList(value);
    for (size_t i = 1; i < nums.size(); i += 2) {
        points.push_back(svgMapPoint(canvas, nums[i - 1], nums[i]));
    }
    return points;
}

struct SvgPathParser {
    const std::string& d;
    size_t pos = 0;
    char command = 0;

    void skip() {
        while (pos < d.size() && (std::isspace((unsigned char)d[pos]) || d[pos] == ',')) ++pos;
    }

    bool hasNumber() {
        skip();
        return pos < d.size() && (d[pos] == '-' || d[pos] == '+' || d[pos] == '.' ||
                                  std::isdigit((unsigned char)d[pos]));
    }

    bool readNumber(float& value) {
        skip();
        if (pos >= d.size()) return false;
        char* end = nullptr;
        value = parseLocaleIndependentFloat(d.c_str() + pos, &end);
        if (end == d.c_str() + pos) return false;
        pos = (size_t)(end - d.c_str());
        return true;
    }

    bool readFlag(float& value) {
        skip();
        if (pos >= d.size()) return false;
        char c = d[pos];
        if (c == '0' || c == '1') {
            value = (c == '1') ? 1.0f : 0.0f;
            pos++;
            return true;
        }
        return false;
    }

    bool readCommand() {
        skip();
        if (pos < d.size() && std::isalpha((unsigned char)d[pos])) {
            command = d[pos++];
            return true;
        }
        return command != 0;
    }
};

inline void appendCubic(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3) {
    for (int i = 1; i <= 16; ++i) {
        float t = i / 16.0f;
        float it = 1.0f - t;
        out.push_back({
            it * it * it * p0.x + 3 * it * it * t * p1.x + 3 * it * t * t * p2.x + t * t * t * p3.x,
            it * it * it * p0.y + 3 * it * it * t * p1.y + 3 * it * t * t * p2.y + t * t * t * p3.y
        });
    }
}

inline void appendQuad(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2) {
    for (int i = 1; i <= 12; ++i) {
        float t = i / 12.0f;
        float it = 1.0f - t;
        out.push_back({
            it * it * p0.x + 2 * it * t * p1.x + t * t * p2.x,
            it * it * p0.y + 2 * it * t * p1.y + t * t * p2.y
        });
    }
}

inline void appendArc(std::vector<Vec2>& out, Vec2 p0, float rx, float ry, float angle, bool largeArcFlag, bool sweepFlag, Vec2 p1, SvgCanvas& canvas) {
    double dx2 = (p0.x - p1.x) / 2.0;
    double dy2 = (p0.y - p1.y) / 2.0;
    double angleRad = angle * 3.14159265358979323846 / 180.0;
    double cosAngle = std::cos(angleRad);
    double sinAngle = std::sin(angleRad);
    
    double x1_prime = cosAngle * dx2 + sinAngle * dy2;
    double y1_prime = -sinAngle * dx2 + cosAngle * dy2;
    
    double rx_d = std::abs(rx);
    double ry_d = std::abs(ry);
    if (rx_d == 0.0 || ry_d == 0.0) {
        out.push_back(svgMapPoint(canvas, p1.x, p1.y));
        return;
    }
    
    double lambda = (x1_prime * x1_prime) / (rx_d * rx_d) + (y1_prime * y1_prime) / (ry_d * ry_d);
    if (lambda > 1.0) {
        rx_d *= std::sqrt(lambda);
        ry_d *= std::sqrt(lambda);
    }
    
    double rx_sq = rx_d * rx_d;
    double ry_sq = ry_d * ry_d;
    double x1_prime_sq = x1_prime * x1_prime;
    double y1_prime_sq = y1_prime * y1_prime;
    
    double sq_term = (rx_sq * ry_sq - rx_sq * y1_prime_sq - ry_sq * x1_prime_sq) / (rx_sq * y1_prime_sq + ry_sq * x1_prime_sq);
    if (sq_term < 0.0) sq_term = 0.0;
    double sign = (largeArcFlag == sweepFlag) ? -1.0 : 1.0;
    double coef = sign * std::sqrt(sq_term);
    
    double cx_prime = coef * ((rx_d * y1_prime) / ry_d);
    double cy_prime = coef * -((ry_d * x1_prime) / rx_d);
    
    double cx = cosAngle * cx_prime - sinAngle * cy_prime + (p0.x + p1.x) / 2.0;
    double cy = sinAngle * cx_prime + cosAngle * cy_prime + (p0.y + p1.y) / 2.0;
    
    double ux = (x1_prime - cx_prime) / rx_d;
    double uy = (y1_prime - cy_prime) / ry_d;
    double vx = (-x1_prime - cx_prime) / rx_d;
    double vy = (-y1_prime - cy_prime) / ry_d;
    
    auto vectorAngle = [](double ux_val, double uy_val, double vx_val, double vy_val) {
        double dot = ux_val * vx_val + uy_val * vy_val;
        double len_u = std::sqrt(ux_val * ux_val + uy_val * uy_val);
        double len_v = std::sqrt(vx_val * vx_val + vy_val * vy_val);
        double val = dot / std::max(0.000001, len_u * len_v);
        if (val < -1.0) val = -1.0;
        if (val > 1.0) val = 1.0;
        double a = std::acos(val);
        if ((ux_val * vy_val - uy_val * vx_val) < 0.0) a = -a;
        return a;
    };
    
    double startAngle = vectorAngle(1.0, 0.0, ux, uy);
    double deltaAngle = vectorAngle(ux, uy, vx, vy);
    
    if (!sweepFlag && deltaAngle > 0.0) {
        deltaAngle -= 2.0 * 3.14159265358979323846;
    } else if (sweepFlag && deltaAngle < 0.0) {
        deltaAngle += 2.0 * 3.14159265358979323846;
    }
    
    int segments = std::max(6, static_cast<int>(std::ceil(std::abs(deltaAngle) / (10.0 * 3.14159265358979323846 / 180.0))));
    for (int i = 1; i <= segments; ++i) {
        double t = static_cast<double>(i) / segments;
        double currentAngle = startAngle + deltaAngle * t;
        double x_prime = rx_d * std::cos(currentAngle);
        double y_prime = ry_d * std::sin(currentAngle);
        double px = cosAngle * x_prime - sinAngle * y_prime + cx;
        double py = sinAngle * x_prime + cosAngle * y_prime + cy;
        out.push_back(svgMapPoint(canvas, static_cast<float>(px), static_cast<float>(py)));
    }
}

inline void drawSvgPath(SvgCanvas& canvas, const std::string& d, Color fill, Color stroke, float strokeWidth, bool fillRuleEvenOdd = false) {
    SvgPathParser parser{d};
    std::vector<std::vector<Vec2>> subpaths;
    std::vector<bool> subpathClosed;
    std::vector<Vec2> currentPath;
    Vec2 current = {0, 0};
    Vec2 start = {0, 0};
    Vec2 lastControl = {0, 0};
    char lastCmd = 0;

    auto flushPath = [&](bool isClosed = false) {
        if (!currentPath.empty()) {
            subpaths.push_back(currentPath);
            subpathClosed.push_back(isClosed);
            currentPath.clear();
        }
    };

    auto ensureStartPoint = [&]() {
        if (currentPath.empty()) {
            currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            start = current;
        }
    };

    while (parser.pos < d.size() && parser.readCommand()) {
        size_t startPos = parser.pos;
        char cmd = parser.command;
        bool rel = std::islower((unsigned char)cmd) != 0;
        char upper = (char)std::toupper((unsigned char)cmd);
        if (upper == 'Z') {
            if (!currentPath.empty()) currentPath.push_back(svgMapPoint(canvas, start.x, start.y));
            current = start;
            flushPath(true);
            parser.command = 0;
            lastControl = current;
            lastCmd = 'Z';
            continue;
        }

        auto map = [&](float x, float y) {
            return rel ? Vec2(current.x + x, current.y + y) : Vec2(x, y);
        };

        if (upper == 'M') {
            float x = 0, y = 0;
            if (!parser.readNumber(x) || !parser.readNumber(y)) break;
            flushPath(false);
            current = map(x, y);
            start = current;
            currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            parser.command = rel ? 'l' : 'L';
            lastControl = current;
            lastCmd = 'M';
        } else if (upper == 'L') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x = 0, y = 0;
                if (!parser.readNumber(x) || !parser.readNumber(y)) break;
                current = map(x, y);
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
            lastControl = current;
            lastCmd = 'L';
        } else if (upper == 'H') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x = 0;
                if (!parser.readNumber(x)) break;
                current.x = rel ? current.x + x : x;
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
            lastControl = current;
            lastCmd = 'H';
        } else if (upper == 'V') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float y = 0;
                if (!parser.readNumber(y)) break;
                current.y = rel ? current.y + y : y;
                currentPath.push_back(svgMapPoint(canvas, current.x, current.y));
            }
            lastControl = current;
            lastCmd = 'V';
        } else if (upper == 'C') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x1, y1, x2, y2, x, y;
                if (!parser.readNumber(x1) || !parser.readNumber(y1) ||
                    !parser.readNumber(x2) || !parser.readNumber(y2) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = rel ? svgMapPoint(canvas, current.x + x1, current.y + y1) : svgMapPoint(canvas, x1, y1);
                Vec2 c2 = rel ? svgMapPoint(canvas, current.x + x2, current.y + y2) : svgMapPoint(canvas, x2, y2);
                Vec2 unmappedC2 = rel ? Vec2(current.x + x2, current.y + y2) : Vec2(x2, y2);
                current = map(x, y);
                Vec2 p3 = svgMapPoint(canvas, current.x, current.y);
                appendCubic(currentPath, p0, c1, c2, p3);
                lastControl = unmappedC2;
            }
            lastCmd = 'C';
        } else if (upper == 'S') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x2, y2, x, y;
                if (!parser.readNumber(x2) || !parser.readNumber(y2) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 unmappedC1;
                if (lastCmd == 'C' || lastCmd == 'S') {
                    unmappedC1.x = 2.0f * current.x - lastControl.x;
                    unmappedC1.y = 2.0f * current.y - lastControl.y;
                } else {
                    unmappedC1 = current;
                }
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = svgMapPoint(canvas, unmappedC1.x, unmappedC1.y);
                Vec2 c2 = rel ? svgMapPoint(canvas, current.x + x2, current.y + y2) : svgMapPoint(canvas, x2, y2);
                Vec2 unmappedC2 = rel ? Vec2(current.x + x2, current.y + y2) : Vec2(x2, y2);
                current = map(x, y);
                Vec2 p3 = svgMapPoint(canvas, current.x, current.y);
                appendCubic(currentPath, p0, c1, c2, p3);
                lastControl = unmappedC2;
                lastCmd = 'S';
            }
        } else if (upper == 'Q') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x1, y1, x, y;
                if (!parser.readNumber(x1) || !parser.readNumber(y1) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = rel ? svgMapPoint(canvas, current.x + x1, current.y + y1) : svgMapPoint(canvas, x1, y1);
                Vec2 unmappedC1 = rel ? Vec2(current.x + x1, current.y + y1) : Vec2(x1, y1);
                current = map(x, y);
                Vec2 p2 = svgMapPoint(canvas, current.x, current.y);
                appendQuad(currentPath, p0, c1, p2);
                lastControl = unmappedC1;
            }
            lastCmd = 'Q';
        } else if (upper == 'T') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float x, y;
                if (!parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 unmappedC1;
                if (lastCmd == 'Q' || lastCmd == 'T') {
                    unmappedC1.x = 2.0f * current.x - lastControl.x;
                    unmappedC1.y = 2.0f * current.y - lastControl.y;
                } else {
                    unmappedC1 = current;
                }
                Vec2 p0 = svgMapPoint(canvas, current.x, current.y);
                Vec2 c1 = svgMapPoint(canvas, unmappedC1.x, unmappedC1.y);
                current = map(x, y);
                Vec2 p2 = svgMapPoint(canvas, current.x, current.y);
                appendQuad(currentPath, p0, c1, p2);
                lastControl = unmappedC1;
                lastCmd = 'T';
            }
        } else if (upper == 'A') {
            ensureStartPoint();
            while (parser.hasNumber()) {
                float rx, ry, rot, largeArc, sweep, x, y;
                if (!parser.readNumber(rx) || !parser.readNumber(ry) || !parser.readNumber(rot) ||
                    !parser.readFlag(largeArc) || !parser.readFlag(sweep) ||
                    !parser.readNumber(x) || !parser.readNumber(y)) break;
                Vec2 p1 = map(x, y);
                appendArc(currentPath, current, rx, ry, rot, largeArc != 0.0f, sweep != 0.0f, p1, canvas);
                current = p1;
            }
            lastControl = current;
            lastCmd = 'A';
        } else {
            parser.command = 0;
            ++parser.pos;
        }

        if (parser.pos == startPos) {
            parser.command = 0;
            break;
        }
    }
    flushPath(false);

    if (fill.a > 0.0f) {
        fillMultiPolygon(canvas, subpaths, fill, fillRuleEvenOdd);
    }
    for (size_t i = 0; i < subpaths.size(); ++i) {
        bool isClosed = i < subpathClosed.size() ? subpathClosed[i] : false;
        if (stroke.a > 0.0f) strokePolyline(canvas, subpaths[i], stroke, strokeWidth, isClosed);
    }
}


} // namespace detail
} // namespace FluxUI