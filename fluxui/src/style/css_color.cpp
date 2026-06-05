// FluxUI — CSS color + gradient parsing.
// Extracted from the monolithic css_parser.cpp. Uses the shared channel parsers
// in css_internal.h (FluxUI::detail) and the StyleSheet static utilities.
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include "css_internal.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace FluxUI {

using detail::lowerAscii;
using detail::functionInner;
using detail::splitColorTokens;
using detail::parseRgbChannel;
using detail::parseAlphaChannel;
using detail::parseHue;
using detail::parseHslPercent;
using detail::parseOklabLightness;
using detail::parseLabLightness;
using detail::parseOklchChroma;
using detail::parseLchChroma;
using detail::parseOklabAxis;
using detail::parseLabAxis;
using detail::parseColorFnComponent;

Color StyleSheet::parseColor(const std::string& val) {
    std::string v = trim(val);
    if (v.empty()) return Color();
    std::string lower = lowerAscii(v);
    if (v[0] == '#') return Color::fromHex(v);
    static const std::unordered_map<std::string, uint32_t> namedColors = {
        {"transparent",0x00000000},{"aliceblue",0xFFF0F8FF},{"antiquewhite",0xFFFAEBD7},
        {"aqua",0xFF00FFFF},{"aquamarine",0xFF7FFFD4},{"azure",0xFFF0FFFF},
        {"beige",0xFFF5F5DC},{"bisque",0xFFFFE4C4},{"black",0xFF000000},
        {"blanchedalmond",0xFFFFEBCD},{"blue",0xFF0000FF},{"blueviolet",0xFF8A2BE2},
        {"brown",0xFFA52A2A},{"burlywood",0xFFDEB887},{"cadetblue",0xFF5F9EA0},
        {"chartreuse",0xFF7FFF00},{"chocolate",0xFFD2691E},{"coral",0xFFFF7F50},
        {"cornflowerblue",0xFF6495ED},{"cornsilk",0xFFFFF8DC},{"crimson",0xFFDC143C},
        {"cyan",0xFF00FFFF},{"darkblue",0xFF00008B},{"darkcyan",0xFF008B8B},
        {"darkgoldenrod",0xFFB8860B},{"darkgray",0xFFA9A9A9},{"darkgrey",0xFFA9A9A9},
        {"darkgreen",0xFF006400},{"darkkhaki",0xFFBDB76B},{"darkmagenta",0xFF8B008B},
        {"darkolivegreen",0xFF556B2F},{"darkorange",0xFFFF8C00},{"darkorchid",0xFF9932CC},
        {"darkred",0xFF8B0000},{"darksalmon",0xFFE9967A},{"darkseagreen",0xFF8FBC8F},
        {"darkslateblue",0xFF483D8B},{"darkslategray",0xFF2F4F4F},{"darkslategrey",0xFF2F4F4F},
        {"darkturquoise",0xFF00CED1},{"darkviolet",0xFF9400D3},{"deeppink",0xFFFF1493},
        {"deepskyblue",0xFF00BFFF},{"dimgray",0xFF696969},{"dimgrey",0xFF696969},
        {"dodgerblue",0xFF1E90FF},{"firebrick",0xFFB22222},{"floralwhite",0xFFFFFAF0},
        {"forestgreen",0xFF228B22},{"fuchsia",0xFFFF00FF},{"gainsboro",0xFFDCDCDC},
        {"ghostwhite",0xFFF8F8FF},{"gold",0xFFFFD700},{"goldenrod",0xFFDAA520},
        {"gray",0xFF808080},{"grey",0xFF808080},{"green",0xFF008000},
        {"greenyellow",0xFFADFF2F},{"honeydew",0xFFF0FFF0},{"hotpink",0xFFFF69B4},
        {"indianred",0xFFCD5C5C},{"indigo",0xFF4B0082},{"ivory",0xFFFFFFF0},
        {"khaki",0xFFF0E68C},{"lavender",0xFFE6E6FA},{"lavenderblush",0xFFFFF0F5},
        {"lawngreen",0xFF7CFC00},{"lemonchiffon",0xFFFFFACD},{"lightblue",0xFFADD8E6},
        {"lightcoral",0xFFF08080},{"lightcyan",0xFFE0FFFF},{"lightgoldenrodyellow",0xFFFAFAD2},
        {"lightgray",0xFFD3D3D3},{"lightgrey",0xFFD3D3D3},{"lightgreen",0xFF90EE90},
        {"lightpink",0xFFFFB6C1},{"lightsalmon",0xFFFFA07A},{"lightseagreen",0xFF20B2AA},
        {"lightskyblue",0xFF87CEFA},{"lightslategray",0xFF778899},{"lightslategrey",0xFF778899},
        {"lightsteelblue",0xFFB0C4DE},{"lightyellow",0xFFFFFFE0},{"lime",0xFF00FF00},
        {"limegreen",0xFF32CD32},{"linen",0xFFFAF0E6},{"magenta",0xFFFF00FF},
        {"maroon",0xFF800000},{"mediumaquamarine",0xFF66CDAA},{"mediumblue",0xFF0000CD},
        {"mediumorchid",0xFFBA55D3},{"mediumpurple",0xFF9370DB},{"mediumseagreen",0xFF3CB371},
        {"mediumslateblue",0xFF7B68EE},{"mediumspringgreen",0xFF00FA9A},
        {"mediumturquoise",0xFF48D1CC},{"mediumvioletred",0xFFC71585},
        {"midnightblue",0xFF191970},{"mintcream",0xFFF5FFFA},{"mistyrose",0xFFFFE4E1},
        {"moccasin",0xFFFFE4B5},{"navajowhite",0xFFFFDEAD},{"navy",0xFF000080},
        {"oldlace",0xFFFDF5E6},{"olive",0xFF808000},{"olivedrab",0xFF6B8E23},
        {"orange",0xFFFFA500},{"orangered",0xFFFF4500},{"orchid",0xFFDA70D6},
        {"palegoldenrod",0xFFEEE8AA},{"palegreen",0xFF98FB98},{"paleturquoise",0xFFAFEEEE},
        {"palevioletred",0xFFDB7093},{"papayawhip",0xFFFFEFD5},{"peachpuff",0xFFFFDAB9},
        {"peru",0xFFCD853F},{"pink",0xFFFFC0CB},{"plum",0xFFDDA0DD},
        {"powderblue",0xFFB0E0E6},{"purple",0xFF800080},{"rebeccapurple",0xFF663399},
        {"red",0xFFFF0000},{"rosybrown",0xFFBC8F8F},{"royalblue",0xFF4169E1},
        {"saddlebrown",0xFF8B4513},{"salmon",0xFFFA8072},{"sandybrown",0xFFF4A460},
        {"seagreen",0xFF2E8B57},{"seashell",0xFFFFF5EE},{"sienna",0xFFA0522D},
        {"silver",0xFFC0C0C0},{"skyblue",0xFF87CEEB},{"slateblue",0xFF6A5ACD},
        {"slategray",0xFF708090},{"slategrey",0xFF708090},{"snow",0xFFFFFAFA},
        {"springgreen",0xFF00FF7F},{"steelblue",0xFF4682B4},{"tan",0xFFD2B48C},
        {"teal",0xFF008080},{"thistle",0xFFD8BFD8},{"tomato",0xFFFF6347},
        {"turquoise",0xFF40E0D0},{"violet",0xFFEE82EE},{"wheat",0xFFF5DEB3},
        {"white",0xFFFFFFFF},{"whitesmoke",0xFFF5F5F5},{"yellow",0xFFFFFF00},
        {"yellowgreen",0xFF9ACD32}
    };
    auto it = namedColors.find(lower);
    if (it != namedColors.end()) {
        uint32_t c = it->second;
        float a = ((c >> 24) & 0xFF) / 255.0f;
        float r = ((c >> 16) & 0xFF) / 255.0f;
        float g = ((c >> 8) & 0xFF) / 255.0f;
        float b = ((c) & 0xFF) / 255.0f;
        return Color(r, g, b, a);
    }
    if (lower == "currentcolor") return Color(1, 1, 1, 1);
    // ── system-ui / system color keywords (CSS Color 4 system colors) ──
    if (lower == "canvas" || lower == "window")        return Color(1, 1, 1, 1);
    if (lower == "canvastext" || lower == "windowtext")return Color(0, 0, 0, 1);
    if (lower == "buttonface")                          return Color(0.94f, 0.94f, 0.94f, 1);
    if (lower == "buttontext")                          return Color(0, 0, 0, 1);
    if (lower == "field")                               return Color(1, 1, 1, 1);
    if (lower == "fieldtext")                           return Color(0, 0, 0, 1);
    if (lower == "highlight")                           return Color(0.0f, 0.46f, 1.0f, 1);
    if (lower == "highlighttext")                       return Color(1, 1, 1, 1);
    if (lower == "graytext")                            return Color(0.5f, 0.5f, 0.5f, 1);
    if (lower == "linktext")                            return Color(0.0f, 0.0f, 0.93f, 1);

    // ── light-dark(<light>, <dark>) — CSS Color Adjust 1 ──
    if (lower.rfind("light-dark(", 0) == 0) {
        std::string inner = functionInner(v);
        // Split on top-level comma
        int depth = 0; size_t comma = std::string::npos;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '(') ++depth;
            else if (inner[i] == ')') --depth;
            else if (inner[i] == ',' && depth == 0) { comma = i; break; }
        }
        if (comma != std::string::npos) {
            std::string lightC = trim(inner.substr(0, comma));
            std::string darkC  = trim(inner.substr(comma + 1));
            bool dark = false;
            if (auto* app = Application::instance())
                dark = app->stylesheet().prefersDark();
            return parseColor(dark ? darkC : lightC);
        }
        return Color();
    }

    // ── color-mix(in <space>, <c1> [p1%], <c2> [p2%]) — CSS Color 5 ──
    if (lower.rfind("color-mix(", 0) == 0) {
        std::string inner = functionInner(v);
        // Split on top-level commas
        std::vector<std::string> parts;
        { int depth = 0; std::string cur;
          for (char c : inner) {
            if (c == '(') { ++depth; cur += c; }
            else if (c == ')') { --depth; cur += c; }
            else if (c == ',' && depth == 0) { parts.push_back(trim(cur)); cur.clear(); }
            else cur += c;
          }
          if (!cur.empty()) parts.push_back(trim(cur));
        }
        if (parts.size() >= 3) {
            // parts[0] = "in <space>" (ignored beyond mixing in sRGB), parts[1], parts[2] = colors
            auto splitColorPct = [](const std::string& s, float& pct) -> std::string {
                std::string t = trim(s);
                size_t sp = t.rfind(' ');
                if (sp != std::string::npos && !t.empty() && t.back() == '%') {
                    std::string pctStr = trim(t.substr(sp + 1));
                    if (!pctStr.empty() && pctStr.back() == '%') {
                        try { pct = std::stof(pctStr.substr(0, pctStr.size() - 1)) / 100.0f; } catch (...) {}
                        return trim(t.substr(0, sp));
                    }
                }
                return t;
            };
            float p1 = -1.0f, p2 = -1.0f;
            std::string c1s = splitColorPct(parts[1], p1);
            std::string c2s = splitColorPct(parts[2], p2);
            Color c1 = parseColor(c1s);
            Color c2 = parseColor(c2s);
            // Normalize percentages (CSS Color 5 rules)
            if (p1 < 0 && p2 < 0) { p1 = 0.5f; p2 = 0.5f; }
            else if (p1 < 0)      { p1 = 1.0f - p2; }
            else if (p2 < 0)      { p2 = 1.0f - p1; }
            float sum = p1 + p2;
            if (sum <= 0.0f) return Color();
            p1 /= sum; p2 /= sum;
            // Mix in sRGB (component-wise) — approximation of in oklab/srgb
            return Color(c1.r * p1 + c2.r * p2,
                         c1.g * p1 + c2.g * p2,
                         c1.b * p1 + c2.b * p2,
                         c1.a * p1 + c2.a * p2);
        }
        return Color();
    }

    // ── oklch(L C H [/ A]) ──────────────────────────────────
    if (lower.rfind("oklch(", 0) == 0) {
        auto t = splitColorTokens(functionInner(v));
        if (t.size() >= 3) {
            float a = t.size() >= 4 ? parseAlphaChannel(t[3]) : 1.0f;
            return Color::fromOklch(parseOklabLightness(t[0]),
                                    parseOklchChroma(t[1]),
                                    parseHue(t[2]), a);
        }
    }
    // ── oklab(L a b [/ A]) ──────────────────────────────────
    if (lower.rfind("oklab(", 0) == 0) {
        auto t = splitColorTokens(functionInner(v));
        if (t.size() >= 3) {
            float a = t.size() >= 4 ? parseAlphaChannel(t[3]) : 1.0f;
            return Color::fromOklab(parseOklabLightness(t[0]),
                                    parseOklabAxis(t[1]),
                                    parseOklabAxis(t[2]), a);
        }
    }
    // ── lch(L C H [/ A]) ────────────────────────────────────
    if (lower.rfind("lch(", 0) == 0) {
        auto t = splitColorTokens(functionInner(v));
        if (t.size() >= 3) {
            float a = t.size() >= 4 ? parseAlphaChannel(t[3]) : 1.0f;
            return Color::fromLch(parseLabLightness(t[0]),
                                  parseLchChroma(t[1]),
                                  parseHue(t[2]), a);
        }
    }
    // ── lab(L a b [/ A]) ────────────────────────────────────
    if (lower.rfind("lab(", 0) == 0) {
        auto t = splitColorTokens(functionInner(v));
        if (t.size() >= 3) {
            float a = t.size() >= 4 ? parseAlphaChannel(t[3]) : 1.0f;
            return Color::fromLab(parseLabLightness(t[0]),
                                  parseLabAxis(t[1]),
                                  parseLabAxis(t[2]), a);
        }
    }
    // ── hwb(H W B [/ A]) ────────────────────────────────────
    if (lower.rfind("hwb(", 0) == 0) {
        auto t = splitColorTokens(functionInner(v));
        if (t.size() >= 3) {
            float a = t.size() >= 4 ? parseAlphaChannel(t[3]) : 1.0f;
            return Color::fromHWB(parseHue(t[0]),
                                  parseHslPercent(t[1]),
                                  parseHslPercent(t[2]), a);
        }
    }
    // ── color(<space> c1 c2 c3 [/ A]) ───────────────────────
    if (lower.rfind("color(", 0) == 0) {
        auto t = splitColorTokens(functionInner(v));
        if (t.size() >= 4) {
            std::string space = lowerAscii(t[0]);
            float a = t.size() >= 5 ? parseAlphaChannel(t[4]) : 1.0f;
            return Color::fromColorFunction(space,
                                            parseColorFnComponent(t[1]),
                                            parseColorFnComponent(t[2]),
                                            parseColorFnComponent(t[3]), a);
        }
    }
    if (lower.rfind("rgb", 0) == 0) {
        auto tokens = splitColorTokens(functionInner(v));
        if (tokens.size() >= 3) {
            float a = tokens.size() >= 4 ? parseAlphaChannel(tokens[3]) : 1.0f;
            return Color(parseRgbChannel(tokens[0]),
                         parseRgbChannel(tokens[1]),
                         parseRgbChannel(tokens[2]),
                         a);
        }
    }
    if (lower.rfind("hsl", 0) == 0) {
        auto tokens = splitColorTokens(functionInner(v));
        if (tokens.size() >= 3) {
            float a = tokens.size() >= 4 ? parseAlphaChannel(tokens[3]) : 1.0f;
            return Color::fromHSL(parseHue(tokens[0]),
                                  parseHslPercent(tokens[1]),
                                  parseHslPercent(tokens[2]),
                                  a);
        }
    }
    return Color();
}

Gradient StyleSheet::parseGradient(const std::string& val) {
    Gradient grad;
    if (val.find("linear-gradient") == std::string::npos) return grad;
    grad.type = Gradient::Linear;
    auto start = val.find('(');
    auto end = val.rfind(')');
    if (start == std::string::npos || end == std::string::npos) return grad;
    std::string inner = val.substr(start + 1, end - start - 1);
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (char c : inner) {
        if (c == '(') depth++;
        if (c == ')') depth--;
        if (c == ',' && depth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(trim(current));
    size_t colorStart = 0;
    if (!parts.empty()) {
        std::string first = parts[0];
        if (first.find("deg") != std::string::npos) {
            grad.angle = parseFloat(first);
            colorStart = 1;
        } else if (first == "to right") {
            grad.angle = 90;
            colorStart = 1;
        } else if (first == "to left") {
            grad.angle = 270;
            colorStart = 1;
        } else if (first == "to bottom") {
            grad.angle = 180;
            colorStart = 1;
        } else if (first == "to top") {
            grad.angle = 0;
            colorStart = 1;
        }
    }
    float autoPos = 0;
    float autoStep = (parts.size() > colorStart + 1) ?
        1.0f / (float)(parts.size() - colorStart - 1) : 1.0f;
    for (size_t i = colorStart; i < parts.size(); i++) {
        std::string part = trim(parts[i]);
        float pos = autoPos;
        auto pctPos = part.rfind('%');
        if (pctPos != std::string::npos) {
            auto numStart = part.rfind(' ', pctPos);
            if (numStart != std::string::npos) {
                pos = std::stof(part.substr(numStart + 1, pctPos - numStart - 1)) / 100.0f;
                part = trim(part.substr(0, numStart));
            }
        }
        grad.stops.push_back({parseColor(part), pos});
        autoPos += autoStep;
    }
    return grad;
}

} // namespace FluxUI
