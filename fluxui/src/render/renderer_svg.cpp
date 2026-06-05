// FluxUI - SVG software rasterizer.
// Parses an SVG document (or an in-memory <svg> blob) and rasterizes its
// shapes/paths to an RGBA8 buffer on the CPU. Extracted from renderer.cpp.
//
// The anonymous-namespace helpers (parsing, path/shape fill + stroke) are
// internal to this TU. rasterizeSvgToRgba() is exported via renderer_svg.h so
// the renderer's image-decode path can rasterize SVG image assets.
#include "fluxui/renderer.h"
#include "fluxui/widgets.h"
#include "renderer_svg.h"
#include "renderer_svg_internal.h"   // SVG geometry + raster primitives (FluxUI::detail)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

// Exported via renderer_svg.h (used by the renderer's image-decode + font path).
std::string trimSvgString(std::string value) {
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isWs((unsigned char)value.front())) value.erase(value.begin());
    while (!value.empty() && isWs((unsigned char)value.back())) value.pop_back();
    return value;
}

std::string lowerSvgString(std::string value) {
    for (char& c : value) c = (char)std::tolower((unsigned char)c);
    return value;
}

bool hasSvgSignature(const unsigned char* data, int dataSize) {
    if (!data || dataSize <= 0) return false;
    int n = std::min(dataSize, 1024);
    std::string head(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + n);
    return lowerSvgString(head).find("<svg") != std::string::npos;
}

namespace {
float parseSvgFloat(const std::string& value, float fallback = 0.0f) {
    std::string s = trimSvgString(value);
    if (s.empty()) return fallback;
    char* end = nullptr;
    float number = parseLocaleIndependentFloat(s.c_str(), &end);
    if (end == s.c_str()) return fallback;
    return number;
}

using SvgAttrs = std::unordered_map<std::string, std::string>;

SvgAttrs parseSvgAttrs(const std::string& tag) {
    SvgAttrs attrs;
    size_t i = 0;
    while (i < tag.size() && tag[i] != '<') ++i;
    if (i < tag.size()) ++i;
    while (i < tag.size() && !std::isspace((unsigned char)tag[i]) && tag[i] != '>' && tag[i] != '/') ++i;

    while (i < tag.size()) {
        while (i < tag.size() && (std::isspace((unsigned char)tag[i]) || tag[i] == '/')) ++i;
        if (i >= tag.size() || tag[i] == '>') break;
        size_t nameStart = i;
        while (i < tag.size() &&
               (std::isalnum((unsigned char)tag[i]) || tag[i] == '-' || tag[i] == '_' || tag[i] == ':')) {
            ++i;
        }
        std::string name = lowerSvgString(tag.substr(nameStart, i - nameStart));
        while (i < tag.size() && std::isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size() || tag[i] != '=') {
            attrs[name] = "";
            continue;
        }
        ++i;
        while (i < tag.size() && std::isspace((unsigned char)tag[i])) ++i;
        if (i >= tag.size()) break;
        std::string value;
        char quote = tag[i];
        if (quote == '"' || quote == '\'') {
            ++i;
            size_t valueStart = i;
            while (i < tag.size() && tag[i] != quote) ++i;
            value = tag.substr(valueStart, i - valueStart);
            if (i < tag.size()) ++i;
        } else {
            size_t valueStart = i;
            while (i < tag.size() && !std::isspace((unsigned char)tag[i]) && tag[i] != '>') ++i;
            value = tag.substr(valueStart, i - valueStart);
        }
        if (!name.empty()) attrs[name] = trimSvgString(value);
    }

    auto styleIt = attrs.find("style");
    if (styleIt != attrs.end()) {
        std::istringstream ss(styleIt->second);
        std::string item;
        while (std::getline(ss, item, ';')) {
            size_t colon = item.find(':');
            if (colon == std::string::npos) continue;
            std::string key = lowerSvgString(trimSvgString(item.substr(0, colon)));
            std::string val = trimSvgString(item.substr(colon + 1));
            if (!key.empty()) attrs[key] = val;
        }
    }

    return attrs;
}

std::string svgAttr(const SvgAttrs& attrs, const std::string& name, const std::string& fallback = "") {
    auto it = attrs.find(name);
    return it == attrs.end() ? fallback : it->second;
}

Color parseSvgColor(const std::string& raw, Color fallback, bool* none = nullptr) {
    if (none) *none = false;
    std::string value = lowerSvgString(trimSvgString(raw));
    if (value.empty()) return fallback;
    if (value == "none") {
        if (none) *none = true;
        return fallback.withAlpha(0.0f);
    }
    if (value == "transparent") return Color(0, 0, 0, 0);
    if (value == "currentcolor") return fallback;
    if (!value.empty() && value[0] == '#') return Color::fromHex(value);
    if (value.rfind("rgb(", 0) == 0 || value.rfind("rgba(", 0) == 0) {
        auto start = value.find('(');
        auto end = value.find(')', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string sub = value.substr(start + 1, end - start - 1);
            std::vector<float> nums;
            std::vector<bool> isPercent;
            
            const char* p = sub.c_str();
            while (*p) {
                while (*p && (std::isspace((unsigned char)*p) || *p == ',' || *p == '/')) {
                    ++p;
                }
                if (!*p) break;
                
                char* parseEnd = nullptr;
                float val = parseLocaleIndependentFloat(p, &parseEnd);
                if (parseEnd == p) {
                    ++p;
                    continue;
                }
                p = parseEnd;
                
                bool percent = false;
                if (*p == '%') {
                    percent = true;
                    ++p;
                }
                nums.push_back(val);
                isPercent.push_back(percent);
            }
            
            if (nums.size() >= 3) {
                float r = isPercent[0] ? (nums[0] / 100.0f) * 255.0f : nums[0];
                float g = isPercent[1] ? (nums[1] / 100.0f) * 255.0f : nums[1];
                float b = isPercent[2] ? (nums[2] / 100.0f) * 255.0f : nums[2];
                float a = 1.0f;
                if (nums.size() >= 4) {
                    a = isPercent[3] ? nums[3] / 100.0f : nums[3];
                }
                return Color(r / 255.0f, g / 255.0f, b / 255.0f, a);
            }
        }
    }

    struct Named { const char* name; Color color; };
    static const Named named[] = {
        {"aliceblue", Color::fromHex("#F0F8FF")},
        {"antiquewhite", Color::fromHex("#FAEBD7")},
        {"aqua", Color::fromHex("#00FFFF")},
        {"aquamarine", Color::fromHex("#7FFFD4")},
        {"azure", Color::fromHex("#F0FFFF")},
        {"beige", Color::fromHex("#F5F5DC")},
        {"bisque", Color::fromHex("#FFE4C4")},
        {"black", Color::fromHex("#000000")},
        {"blanchedalmond", Color::fromHex("#FFEBCD")},
        {"blue", Color::fromHex("#0000FF")},
        {"blueviolet", Color::fromHex("#8A2BE2")},
        {"brown", Color::fromHex("#A52A2A")},
        {"burlywood", Color::fromHex("#DEB887")},
        {"cadetblue", Color::fromHex("#5F9EA0")},
        {"chartreuse", Color::fromHex("#7FFF00")},
        {"chocolate", Color::fromHex("#D2691E")},
        {"coral", Color::fromHex("#FF7F50")},
        {"cornflowerblue", Color::fromHex("#6495ED")},
        {"cornsilk", Color::fromHex("#FFF8DC")},
        {"crimson", Color::fromHex("#DC143C")},
        {"cyan", Color::fromHex("#00FFFF")},
        {"darkblue", Color::fromHex("#00008B")},
        {"darkcyan", Color::fromHex("#008B8B")},
        {"darkgoldenrod", Color::fromHex("#B8860B")},
        {"darkgray", Color::fromHex("#A9A9A9")},
        {"darkgreen", Color::fromHex("#006400")},
        {"darkgrey", Color::fromHex("#A9A9A9")},
        {"darkkhaki", Color::fromHex("#BDB76B")},
        {"darkmagenta", Color::fromHex("#8B008B")},
        {"darkolivegreen", Color::fromHex("#556B2F")},
        {"darkorange", Color::fromHex("#FF8C00")},
        {"darkorchid", Color::fromHex("#9932CC")},
        {"darkred", Color::fromHex("#8B0000")},
        {"darksalmon", Color::fromHex("#E9967A")},
        {"darkseagreen", Color::fromHex("#8FBC8F")},
        {"darkslateblue", Color::fromHex("#483D8B")},
        {"darkslategray", Color::fromHex("#2F4F4F")},
        {"darkslategrey", Color::fromHex("#2F4F4F")},
        {"darkturquoise", Color::fromHex("#00CED1")},
        {"darkviolet", Color::fromHex("#9400D3")},
        {"deeppink", Color::fromHex("#FF1493")},
        {"deepskyblue", Color::fromHex("#00BFFF")},
        {"dimgray", Color::fromHex("#696969")},
        {"dimgrey", Color::fromHex("#696969")},
        {"dodgerblue", Color::fromHex("#1E90FF")},
        {"firebrick", Color::fromHex("#B22222")},
        {"floralwhite", Color::fromHex("#FFFAF0")},
        {"forestgreen", Color::fromHex("#228B22")},
        {"fuchsia", Color::fromHex("#FF00FF")},
        {"gainsboro", Color::fromHex("#DCDCDC")},
        {"ghostwhite", Color::fromHex("#F8F8FF")},
        {"gold", Color::fromHex("#FFD700")},
        {"goldenrod", Color::fromHex("#DAA520")},
        {"gray", Color::fromHex("#808080")},
        {"green", Color::fromHex("#008000")},
        {"greenyellow", Color::fromHex("#ADFF2F")},
        {"grey", Color::fromHex("#808080")},
        {"honeydew", Color::fromHex("#F0FFF0")},
        {"hotpink", Color::fromHex("#FF69B4")},
        {"indianred", Color::fromHex("#CD5C5C")},
        {"indigo", Color::fromHex("#4B0082")},
        {"ivory", Color::fromHex("#FFFFF0")},
        {"khaki", Color::fromHex("#F0E68C")},
        {"lavender", Color::fromHex("#E6E6FA")},
        {"lavenderblush", Color::fromHex("#FFF0F5")},
        {"lawngreen", Color::fromHex("#7CFC00")},
        {"lemonchiffon", Color::fromHex("#FFFACD")},
        {"lightblue", Color::fromHex("#ADD8E6")},
        {"lightcoral", Color::fromHex("#F08080")},
        {"lightcyan", Color::fromHex("#E0FFFF")},
        {"lightgoldenrodyellow", Color::fromHex("#FAFAD2")},
        {"lightgray", Color::fromHex("#D3D3D3")},
        {"lightgreen", Color::fromHex("#90EE90")},
        {"lightgrey", Color::fromHex("#D3D3D3")},
        {"lightpink", Color::fromHex("#FFB6C1")},
        {"lightsalmon", Color::fromHex("#FFA07A")},
        {"lightseagreen", Color::fromHex("#20B2AA")},
        {"lightskyblue", Color::fromHex("#87CEFA")},
        {"lightslategray", Color::fromHex("#778899")},
        {"lightslategrey", Color::fromHex("#778899")},
        {"lightsteelblue", Color::fromHex("#B0C4DE")},
        {"lightyellow", Color::fromHex("#FFFFE0")},
        {"lime", Color::fromHex("#00FF00")},
        {"limegreen", Color::fromHex("#32CD32")},
        {"linen", Color::fromHex("#FAF0E6")},
        {"magenta", Color::fromHex("#FF00FF")},
        {"maroon", Color::fromHex("#800000")},
        {"mediumaquamarine", Color::fromHex("#66CDAA")},
        {"mediumblue", Color::fromHex("#0000CD")},
        {"mediumorchid", Color::fromHex("#BA55D3")},
        {"mediumpurple", Color::fromHex("#9370DB")},
        {"mediumseagreen", Color::fromHex("#3CB371")},
        {"mediumslateblue", Color::fromHex("#7B68EE")},
        {"mediumspringgreen", Color::fromHex("#00FA9A")},
        {"mediumturquoise", Color::fromHex("#48D1CC")},
        {"mediumvioletred", Color::fromHex("#C71585")},
        {"midnightblue", Color::fromHex("#191970")},
        {"mintcream", Color::fromHex("#F5FFFA")},
        {"mistyrose", Color::fromHex("#FFE4E1")},
        {"moccasin", Color::fromHex("#FFE4B5")},
        {"navajowhite", Color::fromHex("#FFDEAD")},
        {"navy", Color::fromHex("#000080")},
        {"oldlace", Color::fromHex("#FDF5E6")},
        {"olive", Color::fromHex("#808000")},
        {"olivedrab", Color::fromHex("#6B8E23")},
        {"orange", Color::fromHex("#FFA500")},
        {"orangered", Color::fromHex("#FF4500")},
        {"orchid", Color::fromHex("#DA70D6")},
        {"palegoldenrod", Color::fromHex("#EEE8AA")},
        {"palegreen", Color::fromHex("#98FB98")},
        {"paleturquoise", Color::fromHex("#AFEEEE")},
        {"palevioletred", Color::fromHex("#DB7093")},
        {"papayawhip", Color::fromHex("#FFEFD5")},
        {"peachpuff", Color::fromHex("#FFDAB9")},
        {"peru", Color::fromHex("#CD853F")},
        {"pink", Color::fromHex("#FFC0CB")},
        {"plum", Color::fromHex("#DDA0DD")},
        {"powderblue", Color::fromHex("#B0E0E6")},
        {"purple", Color::fromHex("#800080")},
        {"rebeccapurple", Color::fromHex("#663399")},
        {"red", Color::fromHex("#FF0000")},
        {"rosybrown", Color::fromHex("#BC8F8F")},
        {"royalblue", Color::fromHex("#4169E1")},
        {"saddlebrown", Color::fromHex("#8B4513")},
        {"salmon", Color::fromHex("#FA8072")},
        {"sandybrown", Color::fromHex("#F4A460")},
        {"seagreen", Color::fromHex("#2E8B57")},
        {"seashell", Color::fromHex("#FFF5EE")},
        {"sienna", Color::fromHex("#A0522D")},
        {"silver", Color::fromHex("#C0C0C0")},
        {"skyblue", Color::fromHex("#87CEEB")},
        {"slateblue", Color::fromHex("#6A5ACD")},
        {"slategray", Color::fromHex("#708090")},
        {"slategrey", Color::fromHex("#708090")},
        {"snow", Color::fromHex("#FFFAFA")},
        {"springgreen", Color::fromHex("#00FF7F")},
        {"steelblue", Color::fromHex("#4682B4")},
        {"tan", Color::fromHex("#D2B48C")},
        {"teal", Color::fromHex("#008080")},
        {"thistle", Color::fromHex("#D8BFD8")},
        {"tomato", Color::fromHex("#FF6347")},
        {"turquoise", Color::fromHex("#40E0D0")},
        {"violet", Color::fromHex("#EE82EE")},
        {"wheat", Color::fromHex("#F5DEB3")},
        {"white", Color::fromHex("#FFFFFF")},
        {"whitesmoke", Color::fromHex("#F5F5F5")},
        {"yellow", Color::fromHex("#FFFF00")},
        {"yellowgreen", Color::fromHex("#9ACD32")}
    };

    auto it = std::lower_bound(std::begin(named), std::end(named), value, [](const Named& item, const std::string& val) {
        return std::strcmp(item.name, val.c_str()) < 0;
    });
    if (it != std::end(named) && value == it->name) {
        return it->color;
    }
    return fallback;
}


SvgAspectRatio parseSvgPreserveAspectRatio(const std::string& rawValue) {
    SvgAspectRatio result;
    std::string value = lowerSvgString(trimSvgString(rawValue));
    if (value.empty()) return result;

    std::istringstream stream(value);
    std::string token;
    while (stream >> token) {
        if (token == "defer") continue;
        if (token == "none") {
            result.none = true;
            result.alignX = 0.0f;
            result.alignY = 0.0f;
            continue;
        }
        if (token == "slice") {
            result.slice = true;
            continue;
        }
        if (token == "meet") {
            result.slice = false;
            continue;
        }
        if (token.find("xmin") != std::string::npos) result.alignX = 0.0f;
        else if (token.find("xmax") != std::string::npos) result.alignX = 1.0f;
        else if (token.find("xmid") != std::string::npos) result.alignX = 0.5f;

        if (token.find("ymin") != std::string::npos) result.alignY = 0.0f;
        else if (token.find("ymax") != std::string::npos) result.alignY = 1.0f;
        else if (token.find("ymid") != std::string::npos) result.alignY = 0.5f;
    }
    return result;
}

SvgAffine parseSvgTransformList(const std::string& rawValue) {
    SvgAffine combined = SvgAffine::identity();
    std::string value = trimSvgString(rawValue);
    size_t pos = 0;
    while (pos < value.size()) {
        while (pos < value.size() && (std::isspace((unsigned char)value[pos]) || value[pos] == ',')) ++pos;
        size_t nameStart = pos;
        while (pos < value.size() && (std::isalpha((unsigned char)value[pos]) || value[pos] == '-')) ++pos;
        if (nameStart == pos) break;
        std::string name = lowerSvgString(value.substr(nameStart, pos - nameStart));
        while (pos < value.size() && std::isspace((unsigned char)value[pos])) ++pos;
        if (pos >= value.size() || value[pos] != '(') break;
        size_t argsStart = ++pos;
        int depth = 1;
        while (pos < value.size() && depth > 0) {
            if (value[pos] == '(') ++depth;
            else if (value[pos] == ')') --depth;
            if (depth > 0) ++pos;
        }
        if (depth != 0) break;
        std::vector<float> args = parseSvgNumberList(value.substr(argsStart, pos - argsStart));
        if (pos < value.size() && value[pos] == ')') ++pos;

        SvgAffine next = SvgAffine::identity();
        if (name == "matrix" && args.size() >= 6) {
            next = {args[0], args[1], args[2], args[3], args[4], args[5]};
        } else if (name == "translate" && !args.empty()) {
            next = SvgAffine::translate(args[0], args.size() >= 2 ? args[1] : 0.0f);
        } else if (name == "scale" && !args.empty()) {
            next = SvgAffine::scale(args[0], args.size() >= 2 ? args[1] : args[0]);
        } else if (name == "rotate" && !args.empty()) {
            next = SvgAffine::rotate(args[0]);
            if (args.size() >= 3) {
                next = SvgAffine::translate(args[1], args[2])
                    .multiply(next)
                    .multiply(SvgAffine::translate(-args[1], -args[2]));
            }
        } else if (name == "skewx" && !args.empty()) {
            next = SvgAffine::skewX(args[0]);
        } else if (name == "skewy" && !args.empty()) {
            next = SvgAffine::skewY(args[0]);
        }
        combined = combined.multiply(next);
    }
    return combined;
}

SvgPaintState mergeSvgPaintState(const SvgPaintState& parent, const SvgAttrs& attrs) {
    SvgPaintState state = parent;
    auto inheritAttr = [&](std::string& field, const char* name) {
        auto it = attrs.find(name);
        if (it != attrs.end()) field = it->second;
    };
    inheritAttr(state.fill, "fill");
    inheritAttr(state.stroke, "stroke");
    inheritAttr(state.opacity, "opacity");
    inheritAttr(state.fillOpacity, "fill-opacity");
    inheritAttr(state.strokeOpacity, "stroke-opacity");
    inheritAttr(state.strokeWidth, "stroke-width");
    inheritAttr(state.color, "color");
    inheritAttr(state.fillRule, "fill-rule");
    return state;
}

std::string svgTagName(const std::string& tag, size_t offset = 1) {
    size_t i = offset;
    while (i < tag.size() && (std::isspace((unsigned char)tag[i]) || tag[i] == '/')) ++i;
    size_t start = i;
    while (i < tag.size() && (std::isalnum((unsigned char)tag[i]) ||
           tag[i] == '-' || tag[i] == '_' || tag[i] == ':')) ++i;
    return lowerSvgString(tag.substr(start, i - start));
}

bool isSvgRenderableTag(const std::string& name) {
    return name == "rect" || name == "circle" || name == "ellipse" ||
           name == "line" || name == "polyline" || name == "polygon" ||
           name == "path";
}

bool isSvgDefinitionContainer(const std::string& name) {
    return name == "defs" || name == "symbol" || name == "clippath" ||
           name == "mask" || name == "pattern" || name == "marker" ||
           name == "lineargradient" || name == "radialgradient" ||
           name == "filter";
}

bool svgTagIsSelfClosing(const std::string& tag) {
    for (size_t i = tag.size(); i > 0; --i) {
        char c = tag[i - 1];
        if (std::isspace((unsigned char)c) || c == '>') continue;
        return c == '/';
    }
    return false;
}


void paintSvgRenderableElement(SvgCanvas& canvas,
                               const std::string& name,
                               const SvgAttrs& attrs,
                               const SvgPaintState& paint) {
    float opacity = parseSvgFloat(paint.opacity, 1.0f);
    bool noFill = false;
    bool noStroke = false;
    Color currentColor = parseSvgColor(paint.color, Color(1, 1, 1, 1));
    Color fill = parseSvgColor(paint.fill, currentColor, &noFill);
    Color stroke = parseSvgColor(paint.stroke, currentColor, &noStroke);
    fill.a *= parseSvgFloat(paint.fillOpacity, 1.0f) * opacity;
    stroke.a *= parseSvgFloat(paint.strokeOpacity, 1.0f) * opacity;
    if (noFill) fill.a = 0.0f;
    if (noStroke) stroke.a = 0.0f;
    float strokeWidth = parseSvgFloat(paint.strokeWidth, 1.0f) *
                        (std::abs(canvas.scaleX) + std::abs(canvas.scaleY)) * 0.5f;

    if (name == "rect") {
        drawSvgRect(canvas,
                    parseSvgFloat(svgAttr(attrs, "x")),
                    parseSvgFloat(svgAttr(attrs, "y")),
                    parseSvgFloat(svgAttr(attrs, "width")),
                    parseSvgFloat(svgAttr(attrs, "height")),
                    parseSvgFloat(svgAttr(attrs, "rx")),
                    parseSvgFloat(svgAttr(attrs, "ry")),
                    fill,
                    stroke,
                    strokeWidth);
    } else if (name == "circle") {
        float r = parseSvgFloat(svgAttr(attrs, "r"));
        drawSvgEllipse(canvas, parseSvgFloat(svgAttr(attrs, "cx")),
                       parseSvgFloat(svgAttr(attrs, "cy")), r, r, fill, stroke, strokeWidth);
    } else if (name == "ellipse") {
        drawSvgEllipse(canvas, parseSvgFloat(svgAttr(attrs, "cx")),
                       parseSvgFloat(svgAttr(attrs, "cy")),
                       parseSvgFloat(svgAttr(attrs, "rx")),
                       parseSvgFloat(svgAttr(attrs, "ry")),
                       fill, stroke, strokeWidth);
    } else if (name == "line") {
        std::vector<Vec2> points = {
            svgMapPoint(canvas, parseSvgFloat(svgAttr(attrs, "x1")), parseSvgFloat(svgAttr(attrs, "y1"))),
            svgMapPoint(canvas, parseSvgFloat(svgAttr(attrs, "x2")), parseSvgFloat(svgAttr(attrs, "y2")))
        };
        strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
    } else if (name == "polyline") {
        auto points = parseSvgPoints(svgAttr(attrs, "points"), canvas);
        strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
    } else if (name == "polygon") {
        auto points = parseSvgPoints(svgAttr(attrs, "points"), canvas);
        fillPolygon(canvas, points, fill);
        strokePolyline(canvas, points, stroke, strokeWidth, true);
    } else if (name == "path") {
        std::string fr = lowerSvgString(trimSvgString(paint.fillRule));
        bool isEvenOdd = (fr == "evenodd");
        drawSvgPath(canvas, svgAttr(attrs, "d"), fill, stroke, std::max(1.0f, strokeWidth), isEvenOdd);
    }
}
} // namespace

// Exported (declared in renderer_svg.h) so renderer.cpp can decode SVG assets.
bool rasterizeSvgToRgba(const unsigned char* data, int dataSize, ImageData& image) {
    if (!hasSvgSignature(data, dataSize)) return false;
    std::string svg(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + dataSize);
    size_t rootPos = lowerSvgString(svg.substr(0, std::min<size_t>(svg.size(), 4096))).find("<svg");
    if (rootPos == std::string::npos) return false;
    size_t rootEnd = svg.find('>', rootPos);
    std::string rootTag = rootEnd == std::string::npos ? svg.substr(rootPos) : svg.substr(rootPos, rootEnd - rootPos + 1);
    SvgAttrs root = parseSvgAttrs(rootTag);

    std::vector<float> vb = parseSvgNumberList(svgAttr(root, "viewbox"));
    float viewX = vb.size() >= 4 ? vb[0] : 0.0f;
    float viewY = vb.size() >= 4 ? vb[1] : 0.0f;
    float viewW = vb.size() >= 4 ? vb[2] : 0.0f;
    float viewH = vb.size() >= 4 ? vb[3] : 0.0f;
    float width = parseSvgFloat(svgAttr(root, "width"), viewW > 0.0f ? viewW : 300.0f);
    float height = parseSvgFloat(svgAttr(root, "height"), viewH > 0.0f ? viewH : 150.0f);
    if (viewW <= 0.0f) viewW = width;
    if (viewH <= 0.0f) viewH = height;
    int outW = std::clamp((int)std::round(width), 1, 4096);
    int outH = std::clamp((int)std::round(height), 1, 4096);
    if ((size_t)outW * (size_t)outH > 16u * 1024u * 1024u) return false;

    image.width = outW;
    image.height = outH;
    image.svg = true;
    image.textureId = 0;
    image.loaded = true;
    image.pixels.assign((size_t)outW * (size_t)outH * 4u, 0);

    SvgCanvas canvas;
    canvas.pixels = &image.pixels;
    canvas.width = outW;
    canvas.height = outH;
    canvas.viewX = viewX;
    canvas.viewY = viewY;
    SvgAspectRatio aspect = parseSvgPreserveAspectRatio(
        svgAttr(root, "preserveaspectratio", "xMidYMid meet"));
    float rawScaleX = outW / std::max(1.0f, viewW);
    float rawScaleY = outH / std::max(1.0f, viewH);
    if (aspect.none) {
        canvas.scaleX = rawScaleX;
        canvas.scaleY = rawScaleY;
    } else {
        float uniformScale = aspect.slice ? std::max(rawScaleX, rawScaleY)
                                          : std::min(rawScaleX, rawScaleY);
        canvas.scaleX = uniformScale;
        canvas.scaleY = uniformScale;
        canvas.offsetX = (outW - viewW * uniformScale) * aspect.alignX;
        canvas.offsetY = (outH - viewH * uniformScale) * aspect.alignY;
    }
    SvgPaintState rootPaint;
    rootPaint.fill = svgAttr(root, "fill", "currentColor");
    rootPaint.stroke = svgAttr(root, "stroke", "none");
    rootPaint.opacity = svgAttr(root, "opacity", "1");
    rootPaint.fillOpacity = svgAttr(root, "fill-opacity", "1");
    rootPaint.strokeOpacity = svgAttr(root, "stroke-opacity", "1");
    rootPaint.strokeWidth = svgAttr(root, "stroke-width", "1");
    rootPaint.color = svgAttr(root, "color", "white");

    std::vector<SvgAffine> transformStack;
    transformStack.push_back(SvgAffine::identity());
    std::vector<SvgPaintState> paintStack;
    paintStack.push_back(rootPaint);

    std::unordered_map<std::string, std::string> idTags;
    size_t indexPos = 0;
    while ((indexPos = svg.find('<', indexPos)) != std::string::npos) {
        if (indexPos + 1 >= svg.size() || svg[indexPos + 1] == '/' ||
            svg[indexPos + 1] == '!' || svg[indexPos + 1] == '?') {
            ++indexPos;
            continue;
        }
        size_t indexEnd = svg.find('>', indexPos);
        if (indexEnd == std::string::npos) break;
        std::string tag = svg.substr(indexPos, indexEnd - indexPos + 1);
        std::string name = svgTagName(tag);
        if (isSvgRenderableTag(name)) {
            SvgAttrs attrs = parseSvgAttrs(tag);
            std::string id = svgAttr(attrs, "id");
            if (!id.empty() && idTags.find(id) == idTags.end()) {
                idTags[id] = tag;
            }
        }
        indexPos = indexEnd + 1;
    }

    int definitionDepth = 0;
    size_t pos = 0;
    while ((pos = svg.find('<', pos)) != std::string::npos) {
        if (pos + 1 >= svg.size()) break;
        if (svg[pos + 1] == '/') {
            size_t closeEnd = svg.find('>', pos);
            if (closeEnd == std::string::npos) break;
            std::string closeName = svgTagName(svg.substr(pos, closeEnd - pos + 1), 2);
            if (isSvgDefinitionContainer(closeName)) {
                definitionDepth = std::max(0, definitionDepth - 1);
                pos = closeEnd + 1;
                continue;
            }
            if ((closeName == "g" || closeName == "svg") && transformStack.size() > 1) {
                transformStack.pop_back();
                paintStack.pop_back();
            }
            pos = closeEnd + 1;
            continue;
        }
        if (svg[pos + 1] == '!' || svg[pos + 1] == '?') {
            ++pos;
            continue;
        }
        size_t end = svg.find('>', pos);
        if (end == std::string::npos) break;
        std::string tag = svg.substr(pos, end - pos + 1);
        std::string name = svgTagName(tag);
        SvgAttrs attrs = parseSvgAttrs(tag);
        bool selfClosing = svgTagIsSelfClosing(tag);
        if (isSvgDefinitionContainer(name)) {
            if (!selfClosing) ++definitionDepth;
            pos = end + 1;
            continue;
        }
        if (definitionDepth > 0) {
            pos = end + 1;
            continue;
        }
        SvgAffine elementTransform = transformStack.back().multiply(
            parseSvgTransformList(svgAttr(attrs, "transform")));
        SvgPaintState elementPaint = mergeSvgPaintState(paintStack.back(), attrs);
        if (name == "g" || name == "svg") {
            if (!selfClosing) {
                transformStack.push_back(elementTransform);
                paintStack.push_back(elementPaint);
            }
            pos = end + 1;
            continue;
        }

        SvgAffine previousTransform = canvas.transform;
        canvas.transform = elementTransform;
        if (name == "use") {
            std::string href = svgAttr(attrs, "href");
            if (href.empty()) href = svgAttr(attrs, "xlink:href");
            if (!href.empty() && href[0] == '#') {
                auto refIt = idTags.find(href.substr(1));
                if (refIt != idTags.end()) {
                    SvgAttrs refAttrs = parseSvgAttrs(refIt->second);
                    std::string refName = svgTagName(refIt->second);
                    SvgAffine useTransform = canvas.transform
                        .multiply(SvgAffine::translate(parseSvgFloat(svgAttr(attrs, "x")),
                                                       parseSvgFloat(svgAttr(attrs, "y"))))
                        .multiply(parseSvgTransformList(svgAttr(refAttrs, "transform")));
                    SvgPaintState refPaint = mergeSvgPaintState(elementPaint, refAttrs);
                    canvas.transform = useTransform;
                    paintSvgRenderableElement(canvas, refName, refAttrs, refPaint);
                }
            }
        } else if (isSvgRenderableTag(name)) {
            paintSvgRenderableElement(canvas, name, attrs, elementPaint);
        }
        canvas.transform = previousTransform;
        pos = end + 1;
    }

    return true;
}


bool Renderer::rasterizeSvgWidget(Widget* svgWidget, ImageData& image) {
    if (!svgWidget || svgWidget->type != "svg") return false;
    
    Svg* svg = static_cast<Svg*>(svgWidget);
    std::vector<float> vb = parseSvgNumberList(svg->viewBox);
    float viewX = vb.size() >= 4 ? vb[0] : 0.0f;
    float viewY = vb.size() >= 4 ? vb[1] : 0.0f;
    float viewW = vb.size() >= 4 ? vb[2] : 0.0f;
    float viewH = vb.size() >= 4 ? vb[3] : 0.0f;
    
    float width = svg->bounds.w;
    float height = svg->bounds.h;
    if (width <= 0.0f) width = parseSvgFloat(svg->width, viewW > 0.0f ? viewW : 300.0f);
    if (height <= 0.0f) height = parseSvgFloat(svg->height, viewH > 0.0f ? viewH : 150.0f);
    
    if (viewW <= 0.0f) viewW = width;
    if (viewH <= 0.0f) viewH = height;
    
    int outW = std::clamp((int)std::round(width), 1, 4096);
    int outH = std::clamp((int)std::round(height), 1, 4096);
    if ((size_t)outW * (size_t)outH > 16u * 1024u * 1024u) return false;
    
    image.width = outW;
    image.height = outH;
    image.svg = true;
    image.textureId = 0;
    image.loaded = true;
    image.pixels.assign((size_t)outW * (size_t)outH * 4u, 0);
    
    SvgCanvas canvas;
    canvas.pixels = &image.pixels;
    canvas.width = outW;
    canvas.height = outH;
    canvas.viewX = viewX;
    canvas.viewY = viewY;
    
    SvgAspectRatio aspect = parseSvgPreserveAspectRatio(svg->preserveAspectRatio);
    float rawScaleX = outW / std::max(1.0f, viewW);
    float rawScaleY = outH / std::max(1.0f, viewH);
    if (aspect.none) {
        canvas.scaleX = rawScaleX;
        canvas.scaleY = rawScaleY;
    } else {
        float uniformScale = aspect.slice ? std::max(rawScaleX, rawScaleY)
                                          : std::min(rawScaleX, rawScaleY);
        canvas.scaleX = uniformScale;
        canvas.scaleY = uniformScale;
        canvas.offsetX = (outW - viewW * uniformScale) * aspect.alignX;
        canvas.offsetY = (outH - viewH * uniformScale) * aspect.alignY;
    }
    
    SvgPaintState rootPaint;
    rootPaint.fill = "currentColor";
    rootPaint.stroke = "none";
    rootPaint.opacity = "1";
    rootPaint.fillOpacity = "1";
    rootPaint.strokeOpacity = "1";
    rootPaint.strokeWidth = "1";
    rootPaint.color = "currentColor";
    
    std::vector<SvgAffine> transformStack;
    transformStack.push_back(SvgAffine::identity());
    std::vector<SvgPaintState> paintStack;
    paintStack.push_back(rootPaint);
    
    std::function<void(Widget*)> traverse = [&](Widget* w) {
        if (!w || !w->visible) return;
        
        std::string type = w->type;
        if (type == "svg" && w != svgWidget) return;
        
        if (type == "g") {
            SvgG* g = static_cast<SvgG*>(w);
            SvgAffine elementTransform = transformStack.back().multiply(
                parseSvgTransformList(g->transformAttr));
                
            SvgAttrs attrs;
            if (!g->fill.empty()) attrs["fill"] = g->fill;
            if (!g->stroke.empty()) attrs["stroke"] = g->stroke;
            if (!g->strokeWidth.empty()) attrs["stroke-width"] = g->strokeWidth;
            if (!g->opacityAttr.empty()) attrs["opacity"] = g->opacityAttr;
            if (!g->fillOpacity.empty()) attrs["fill-opacity"] = g->fillOpacity;
            if (!g->strokeOpacity.empty()) attrs["stroke-opacity"] = g->strokeOpacity;
            if (!g->fillRuleAttr.empty()) attrs["fill-rule"] = g->fillRuleAttr;
            
            SvgPaintState elementPaint = mergeSvgPaintState(paintStack.back(), attrs);
            
            transformStack.push_back(elementTransform);
            paintStack.push_back(elementPaint);
            
            for (auto& child : g->children) {
                traverse(child.get());
            }
            
            transformStack.pop_back();
            paintStack.pop_back();
            return;
        }
        
        if (type == "path" || type == "rect" || type == "circle" || type == "ellipse" ||
            type == "line" || type == "polyline" || type == "polygon") {
            SvgElement* elem = static_cast<SvgElement*>(w);
            SvgAffine elementTransform = transformStack.back().multiply(
                parseSvgTransformList(elem->transformAttr));
                
            SvgAttrs attrs;
            if (!elem->fill.empty()) attrs["fill"] = elem->fill;
            if (!elem->stroke.empty()) attrs["stroke"] = elem->stroke;
            if (!elem->strokeWidth.empty()) attrs["stroke-width"] = elem->strokeWidth;
            if (!elem->opacityAttr.empty()) attrs["opacity"] = elem->opacityAttr;
            if (!elem->fillOpacity.empty()) attrs["fill-opacity"] = elem->fillOpacity;
            if (!elem->strokeOpacity.empty()) attrs["stroke-opacity"] = elem->strokeOpacity;
            if (!elem->fillRuleAttr.empty()) attrs["fill-rule"] = elem->fillRuleAttr;
            
            SvgPaintState paint = mergeSvgPaintState(paintStack.back(), attrs);
            
            bool noFill = false;
            bool noStroke = false;
            Color currentColor = parseSvgColor(paint.color, svgWidget->computedStyle->color);
            Color fill = parseSvgColor(paint.fill, currentColor, &noFill);
            Color stroke = parseSvgColor(paint.stroke, currentColor, &noStroke);
            float opacity = parseSvgFloat(paint.opacity, 1.0f);
            fill.a *= parseSvgFloat(paint.fillOpacity, 1.0f) * opacity;
            stroke.a *= parseSvgFloat(paint.strokeOpacity, 1.0f) * opacity;
            if (noFill) fill.a = 0.0f;
            if (noStroke) stroke.a = 0.0f;
            float strokeWidth = parseSvgFloat(paint.strokeWidth, 1.0f) *
                                (std::abs(canvas.scaleX) + std::abs(canvas.scaleY)) * 0.5f;
                                
            SvgAffine previousTransform = canvas.transform;
            canvas.transform = elementTransform;
            
            if (type == "rect") {
                SvgRect* r = static_cast<SvgRect*>(w);
                drawSvgRect(canvas,
                            parseSvgFloat(r->x),
                            parseSvgFloat(r->y),
                            parseSvgFloat(r->width),
                            parseSvgFloat(r->height),
                            parseSvgFloat(r->rx),
                            parseSvgFloat(r->ry),
                            fill, stroke, strokeWidth);
            } else if (type == "circle") {
                SvgCircle* c = static_cast<SvgCircle*>(w);
                float rad = parseSvgFloat(c->r);
                drawSvgEllipse(canvas,
                               parseSvgFloat(c->cx),
                               parseSvgFloat(c->cy),
                               rad, rad,
                               fill, stroke, strokeWidth);
            } else if (type == "ellipse") {
                SvgEllipse* el = static_cast<SvgEllipse*>(w);
                drawSvgEllipse(canvas,
                               parseSvgFloat(el->cx),
                               parseSvgFloat(el->cy),
                               parseSvgFloat(el->rx),
                               parseSvgFloat(el->ry),
                               fill, stroke, strokeWidth);
            } else if (type == "line") {
                SvgLine* l = static_cast<SvgLine*>(w);
                std::vector<Vec2> points = {
                    svgMapPoint(canvas, parseSvgFloat(l->x1), parseSvgFloat(l->y1)),
                    svgMapPoint(canvas, parseSvgFloat(l->x2), parseSvgFloat(l->y2))
                };
                strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
            } else if (type == "polyline") {
                SvgPolyline* pl = static_cast<SvgPolyline*>(w);
                auto points = parseSvgPoints(pl->points, canvas);
                strokePolyline(canvas, points, stroke.a > 0.0f ? stroke : fill, std::max(1.0f, strokeWidth), false);
            } else if (type == "polygon") {
                SvgPolygon* pg = static_cast<SvgPolygon*>(w);
                auto points = parseSvgPoints(pg->points, canvas);
                fillPolygon(canvas, points, fill);
                strokePolyline(canvas, points, stroke, strokeWidth, true);
            } else if (type == "path") {
                SvgPath* p = static_cast<SvgPath*>(w);
                std::string fr = lowerSvgString(trimSvgString(paint.fillRule));
                bool isEvenOdd = (fr == "evenodd");
                drawSvgPath(canvas, p->d, fill, stroke, std::max(1.0f, strokeWidth), isEvenOdd);
            }
            
            canvas.transform = previousTransform;
        }
    };
    
    for (auto& child : svg->children) {
        traverse(child.get());
    }
    
    return true;
}

} // namespace FluxUI
