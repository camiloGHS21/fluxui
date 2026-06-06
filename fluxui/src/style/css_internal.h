// FluxUI — internal CSS helpers shared across the style/ translation units.
//
// These were file-local `static` helpers inside the monolithic css_parser.cpp.
// To let color parsing (css_color.cpp) live in its own TU while still sharing
// the low-level string utilities and channel parsers, they are exposed here as
// `inline` functions in FluxUI::detail (header-only, engine-internal — NOT part
// of the public API). This header is private to fluxui/src/style.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <cmath>
#include <algorithm>

namespace FluxUI {
namespace detail {

// ── Low-level string utilities ──────────────────────────────
inline std::string trimLocal(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}
inline std::string_view trimLocal(std::string_view s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string_view::npos) ? std::string_view{} : s.substr(start, end - start + 1);
}
inline std::string lowerAscii(std::string s) {
    for (char& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}
inline std::string lowerAscii(std::string_view s) {
    std::string res(s);
    for (char& c : res) {
        c = (char)std::tolower((unsigned char)c);
    }
    return res;
}
inline std::string functionInner(const std::string& value) {
    auto start = value.find('(');
    auto end = value.rfind(')');
    if (start == std::string::npos || end == std::string::npos || end <= start) return "";
    return value.substr(start + 1, end - start - 1);
}
// Splits a string on ASCII whitespace into a fixed-size token array (no alloc).
inline void splitWhitespace(std::string_view val, std::string_view tokens[], int maxTokens, int& count) {
    count = 0;
    size_t i = 0;
    while (i < val.size() && count < maxTokens) {
        while (i < val.size() && (val[i] == ' ' || val[i] == '\t' || val[i] == '\r' || val[i] == '\n')) {
            i++;
        }
        if (i == val.size()) break;
        size_t start = i;
        while (i < val.size() && val[i] != ' ' && val[i] != '\t' && val[i] != '\r' && val[i] != '\n') {
            i++;
        }
        tokens[count++] = val.substr(start, i - start);
    }
}
// Splits on top-level whitespace, ignoring whitespace inside (), [], {}.
inline std::vector<std::string> splitWhitespaceTopLevel(std::string_view val) {
    std::vector<std::string> tokens;
    std::string current;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    for (size_t i = 0; i < val.size(); ++i) {
        char c = val[i];
        if (c == '(') parenDepth++;
        else if (c == ')' && parenDepth > 0) parenDepth--;
        else if (c == '[') bracketDepth++;
        else if (c == ']' && bracketDepth > 0) bracketDepth--;
        else if (c == '{') braceDepth++;
        else if (c == '}' && braceDepth > 0) braceDepth--;
        bool isWhitespace = (c == ' ' || c == '\t' || c == '\r' || c == '\n');
        if (isWhitespace && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

// ── CSS color channel parsers (shared by parseColor / parseGradient) ──
inline std::vector<std::string> splitColorTokens(const std::string& inner) {
    std::vector<std::string> tokens;
    std::string current;
    int depth = 0;
    for (char c : inner) {
        if (c == '(') depth++;
        if (c == ')' && depth > 0) depth--;
        bool separator = depth == 0 && (c == ',' || c == '/' || std::isspace((unsigned char)c));
        if (separator) {
            if (!current.empty()) {
                tokens.push_back(trimLocal(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(trimLocal(current));
    return tokens;
}
inline float parseNumberToken(std::string token, bool* isPercent = nullptr) {
    token = trimLocal(lowerAscii(token));
    bool percent = false;
    if (!token.empty() && token.back() == '%') {
        percent = true;
        token.pop_back();
    }
    for (const auto& suffix : {"px", "rem", "em", "deg", "turn", "rad", "ms", "s"}) {
        auto pos = token.find(suffix);
        if (pos != std::string::npos) {
            token = token.substr(0, pos);
            break;
        }
    }
    if (isPercent) *isPercent = percent;
    try {
        return std::stof(token);
    } catch (...) {
        return 0.0f;
    }
}
inline float parseRgbChannel(const std::string& token) {
    bool percent = false;
    float value = parseNumberToken(token, &percent);
    if (percent) return std::clamp(value / 100.0f, 0.0f, 1.0f);
    if (value > 1.0f) value /= 255.0f;
    return std::clamp(value, 0.0f, 1.0f);
}
inline float parseAlphaChannel(const std::string& token) {
    bool percent = false;
    float value = parseNumberToken(token, &percent);
    if (percent) value /= 100.0f;
    return std::clamp(value, 0.0f, 1.0f);
}
inline float parseHue(const std::string& token) {
    std::string v = trimLocal(lowerAscii(token));
    float hue = parseNumberToken(v);
    if (v.find("turn") != std::string::npos) hue *= 360.0f;
    else if (v.find("rad") != std::string::npos) hue = hue * 180.0f / 3.1415926535f;
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    return hue;
}
inline float parseHslPercent(const std::string& token) {
    bool percent = false;
    float value = parseNumberToken(token, &percent);
    if (percent || value > 1.0f) value /= 100.0f;
    return std::clamp(value, 0.0f, 1.0f);
}
// CSS Color 4 channel parsers.
// Lightness for oklab/oklch: 0%..100% maps to 0..1; bare number used as-is.
inline float parseOklabLightness(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return std::clamp(v / 100.0f, 0.0f, 1.0f);
    return std::clamp(v, 0.0f, 1.0f);
}
// Lightness for lab/lch: 0%..100% maps to 0..100; bare number used as-is.
inline float parseLabLightness(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return std::clamp(v, 0.0f, 100.0f);
    return std::clamp(v, 0.0f, 100.0f);
}
// Chroma for oklch: percentage 100% == 0.4; number used directly.
inline float parseOklchChroma(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return (v / 100.0f) * 0.4f;
    return std::max(0.0f, v);
}
// Chroma for lch: percentage 100% == 150; number used directly.
inline float parseLchChroma(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return (v / 100.0f) * 150.0f;
    return std::max(0.0f, v);
}
// oklab a/b axis: percentage 100% == 0.4; number used directly.
inline float parseOklabAxis(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return (v / 100.0f) * 0.4f;
    return v;
}
// lab a/b axis: percentage 100% == 125; number used directly.
inline float parseLabAxis(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return (v / 100.0f) * 125.0f;
    return v;
}
// color() component: percentage maps to 0..1; number used directly.
inline float parseColorFnComponent(const std::string& token) {
    bool percent = false;
    float v = parseNumberToken(token, &percent);
    if (percent) return v / 100.0f;
    return v;
}

} // namespace detail
} // namespace FluxUI
