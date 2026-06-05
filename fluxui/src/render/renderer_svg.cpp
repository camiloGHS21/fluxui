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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace FluxUI {

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

std::vector<float> parseSvgNumberList(const std::string& value) {
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

Vec2 svgMapPoint(const SvgCanvas& canvas, float x, float y) {
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

void appendCubic(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3);
void appendQuad(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2);
void appendArc(std::vector<Vec2>& out, Vec2 p0, float rx, float ry, float angle, bool largeArcFlag, bool sweepFlag, Vec2 p1, SvgCanvas& canvas);

void svgBlendPixel(SvgCanvas& canvas, int x, int y, Color color, float coverage = 1.0f) {
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

float pointSegmentDistance(float px, float py, Vec2 a, Vec2 b) {
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

void strokePolyline(SvgCanvas& canvas, const std::vector<Vec2>& points, Color color, float strokeWidth, bool close) {
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

bool pointInPolygon(float x, float y, const std::vector<Vec2>& points) {
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

void fillPolygon(SvgCanvas& canvas, const std::vector<Vec2>& points, Color color) {
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
void fillMultiPolygon(SvgCanvas& canvas, const std::vector<std::vector<Vec2>>& subpaths,
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

void drawSvgRect(SvgCanvas& canvas, float x, float y, float w, float h,
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

void drawSvgEllipse(SvgCanvas& canvas, float cx, float cy, float rx, float ry,
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

std::vector<Vec2> parseSvgPoints(const std::string& value, const SvgCanvas& canvas) {
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

void appendCubic(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3) {
    for (int i = 1; i <= 16; ++i) {
        float t = i / 16.0f;
        float it = 1.0f - t;
        out.push_back({
            it * it * it * p0.x + 3 * it * it * t * p1.x + 3 * it * t * t * p2.x + t * t * t * p3.x,
            it * it * it * p0.y + 3 * it * it * t * p1.y + 3 * it * t * t * p2.y + t * t * t * p3.y
        });
    }
}

void appendQuad(std::vector<Vec2>& out, Vec2 p0, Vec2 p1, Vec2 p2) {
    for (int i = 1; i <= 12; ++i) {
        float t = i / 12.0f;
        float it = 1.0f - t;
        out.push_back({
            it * it * p0.x + 2 * it * t * p1.x + t * t * p2.x,
            it * it * p0.y + 2 * it * t * p1.y + t * t * p2.y
        });
    }
}

void appendArc(std::vector<Vec2>& out, Vec2 p0, float rx, float ry, float angle, bool largeArcFlag, bool sweepFlag, Vec2 p1, SvgCanvas& canvas) {
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

void drawSvgPath(SvgCanvas& canvas, const std::string& d, Color fill, Color stroke, float strokeWidth, bool fillRuleEvenOdd = false) {
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