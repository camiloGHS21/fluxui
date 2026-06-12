// FluxUI - CSS typed-value + animation/timing parsers.
// Extracted from css_parser.cpp: StyleSheet members that parse length/border/
// shadow/duration values, keyframes, the animation/transition list grammars,
// timing-function (de)serialization + sampling, and the @supports evaluator.
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include "css_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace FluxUI {
using detail::trimLocal;
using detail::lowerAscii;
using detail::functionInner;
using detail::splitWhitespace;
using detail::splitWhitespaceTopLevel;

// StyleSheet::parseColor moved to css_color.cpp.
CSSValue StyleSheet::parseCSSValue(const std::string& val) {
    std::string v = trim(val);
    std::string lower = lowerAscii(v);
    if (lower == "auto") return CSSValue::autoVal();
    if (v.empty()) return CSSValue();
    if (lower == "min-content") return CSSValue::minContent();
    if (lower == "max-content") return CSSValue::maxContent();
    if (lower == "fit-content") return CSSValue::fitContent();
    if (auto expr = CSSMathExpressionParser::parse(v)) {
        return CSSValue(expr);
    }
    if (v.back() == '%') {
        return CSSValue::pct(std::stof(v.substr(0, v.size() - 1)));
    }
    std::string numStr = v;
    auto pxPos = v.find("px");
    if (pxPos != std::string::npos) numStr = v.substr(0, pxPos);
    try {
        return CSSValue::px(std::stof(numStr));
    } catch (...) {
        return CSSValue();
    }
}
float StyleSheet::parseLengthPixels(const std::string& val, float emBase) {
    std::string v = trim(val);
    if (v.empty() || lowerAscii(v) == "auto") return 0.0f;
    CSSValue cssVal = parseCSSValue(v);
    float vpW = 1920.0f;
    float vpH = 1080.0f;
    if (auto* app = Application::instance()) {
        vpW = app->stylesheet().viewportWidth();
        vpH = app->stylesheet().viewportHeight();
    }
    std::string lower = lowerAscii(v);
    if (lower.find("__qem") != std::string::npos) {
        return parseFloat(lower) * emBase;
    }
    return cssVal.resolve(emBase, vpW, vpH, emBase);
}
float StyleSheet::parseFontSizePixels(const std::string& val, float currentSize) {
    std::string lower = lowerAscii(trim(val));
    if (lower == "xx-small") return 9.0f;
    if (lower == "x-small") return 10.0f;
    if (lower == "small") return 13.0f;
    if (lower == "medium") return 16.0f;
    if (lower == "large") return 18.0f;
    if (lower == "x-large") return 24.0f;
    if (lower == "xx-large") return 32.0f;
    if (lower == "xxx-large") return 48.0f;
    if (lower == "smaller") return std::max(1.0f, currentSize * 0.833f);
    if (lower == "larger") return currentSize * 1.2f;
    return parseLengthPixels(lower, currentSize);
}
float StyleSheet::parseLineHeight(const std::string& val, float fontSize) {
    std::string v = trim(val);
    std::string lower = lowerAscii(v);
    float base = std::max(1.0f, fontSize);
    if (lower.empty() || lower == "normal") return 1.2f;
    if (lower.back() == '%') return parseFloat(lower) / 100.0f;
    if ((lower.size() > 2 && lower.substr(lower.size() - 2) == "px") ||
        (lower.size() > 3 && lower.substr(lower.size() - 3) == "rem") ||
        (lower.size() > 2 && lower.substr(lower.size() - 2) == "em")) {
        return std::max(0.0f, parseLengthPixels(lower, base) / base);
    }
    return parseFloat(lower);
}
EdgeInsets StyleSheet::parseEdgeInsets(const std::string& val, float emBase) {
    float values[4] = {0, 0, 0, 0};
    int count = 0;
    size_t i = 0;
    while (i < val.size() && count < 4) {
        while (i < val.size() && (val[i] == ' ' || val[i] == '\t' || val[i] == '\r' || val[i] == '\n')) {
            i++;
        }
        if (i == val.size()) break;
        size_t start = i;
        while (i < val.size() && val[i] != ' ' && val[i] != '\t' && val[i] != '\r' && val[i] != '\n') {
            i++;
        }
        std::string token(val.data() + start, i - start);
        values[count++] = parseLengthPixels(token, emBase);
    }
    if (count == 1) return EdgeInsets(values[0]);
    if (count == 2) return EdgeInsets(values[0], values[1]);
    if (count == 3) return EdgeInsets(values[0], values[1], values[2], values[1]);
    if (count >= 4) return EdgeInsets(values[0], values[1], values[2], values[3]);
    return EdgeInsets();
}
BorderRadius StyleSheet::parseBorderRadius(const std::string& val, float emBase) {
    float values[4] = {0, 0, 0, 0};
    int count = 0;
    size_t i = 0;
    while (i < val.size() && count < 4) {
        while (i < val.size() && (val[i] == ' ' || val[i] == '\t' || val[i] == '\r' || val[i] == '\n')) {
            i++;
        }
        if (i == val.size()) break;
        size_t start = i;
        while (i < val.size() && val[i] != ' ' && val[i] != '\t' && val[i] != '\r' && val[i] != '\n') {
            i++;
        }
        std::string token(val.data() + start, i - start);
        values[count++] = parseLengthPixels(token, emBase);
    }
    if (count == 1) return BorderRadius(values[0]);
    if (count == 2) {
        BorderRadius br;
        br.tl = br.br = values[0];
        br.tr = br.bl = values[1];
        return br;
    }
    if (count >= 4) {
        BorderRadius br;
        br.tl = values[0]; br.tr = values[1];
        br.br = values[2]; br.bl = values[3];
        return br;
    }
    return BorderRadius();
}
Border StyleSheet::parseBorder(const std::string& val, float emBase) {
    std::string v = trim(val);
    if (v.empty() || v == "none" || v == "0" || v == "0px") {
        return Border();
    }
    std::string colorStr;
    auto varPos = v.find("var(");
    if (varPos == std::string::npos) varPos = v.find("min(");
    if (varPos == std::string::npos) varPos = v.find("max(");
    if (varPos == std::string::npos) varPos = v.find("clamp(");
    if (varPos != std::string::npos) {
        int depth = 1;
        size_t cursor = varPos + 4;
        while (cursor < v.size() && depth > 0) {
            if (v[cursor] == '(') depth++;
            if (v[cursor] == ')') depth--;
            if (depth > 0) cursor++;
        }
        if (cursor < v.size()) {
            colorStr = v.substr(varPos, cursor - varPos + 1);
            v = v.substr(0, varPos) + v.substr(cursor + 1);
        }
    } else {
        auto hashPos = v.find('#');
        if (hashPos != std::string::npos) {
            auto end = v.find(' ', hashPos + 1);
            if (end == std::string::npos) end = v.size();
            colorStr = v.substr(hashPos, end - hashPos);
            v = v.substr(0, hashPos) + v.substr(end);
        } else {
            auto rgbPos = v.find("rgb");
            auto hslPos = v.find("hsl");
            auto colorPos = rgbPos != std::string::npos ? rgbPos : hslPos;
            if (colorPos != std::string::npos) {
                auto end = v.find(')', colorPos);
                if (end != std::string::npos) {
                    colorStr = v.substr(colorPos, end - colorPos + 1);
                    v = v.substr(0, colorPos) + v.substr(end + 1);
                }
            }
        }
    }
    std::istringstream ss(v);
    std::string token;
    Border border;
    if (!colorStr.empty()) border.color = parseColor(colorStr);
    while (ss >> token) {
        if (token == "solid" || token == "dashed" || token == "none") {
            if (token == "none") border.width = 0;
            continue;
        }
        if (!token.empty() && (token == "white" || token == "black" || token == "transparent" ||
            token == "red" || token == "green" || token == "blue" ||
            token == "yellow" || token == "cyan" || token == "magenta" ||
            token == "gray" || token == "grey")) {
            border.color = parseColor(token);
            continue;
        }
        float width = parseLengthPixels(token, emBase);
        if (width > 0 || token.find('0') != std::string::npos) {
            border.width = width;
        }
    }
    return border;
}
BoxShadow StyleSheet::parseBoxShadow(const std::string& val, float emBase) {
    BoxShadow shadow;
    if (val == "none") return shadow;
    std::string v = val;
    if (v.find("inset") != std::string::npos) {
        shadow.inset = true;
        auto pos = v.find("inset");
        v = v.substr(0, pos) + v.substr(pos + 5);
    }
    std::string colorStr;
    auto hashPos = v.find('#');
    if (hashPos != std::string::npos) {
        auto end = v.find(' ', hashPos + 1);
        if (end == std::string::npos) end = v.size();
        colorStr = v.substr(hashPos, end - hashPos);
        v = v.substr(0, hashPos) + v.substr(end);
    } else {
        auto rgbPos = v.find("rgb");
        auto hslPos = v.find("hsl");
        auto colorPos = rgbPos != std::string::npos ? rgbPos : hslPos;
        if (colorPos != std::string::npos) {
            auto end = v.find(')', colorPos);
            if (end != std::string::npos) {
                colorStr = v.substr(colorPos, end - colorPos + 1);
                v = v.substr(0, colorPos) + v.substr(end + 1);
            }
        }
    }
    std::istringstream ss(trim(v));
    std::vector<float> vals;
    std::string token;
    while (ss >> token) {
        vals.push_back(parseLengthPixels(token, emBase));
    }
    if (vals.size() >= 1) shadow.offsetX = vals[0];
    if (vals.size() >= 2) shadow.offsetY = vals[1];
    if (vals.size() >= 3) shadow.blur = vals[2];
    if (vals.size() >= 4) shadow.spread = vals[3];
    if (!colorStr.empty()) shadow.color = parseColor(colorStr);
    return shadow;
}
// StyleSheet::parseGradient moved to css_color.cpp.
std::vector<TextShadow> StyleSheet::parseTextShadowList(const std::string& val, float emBase) {
    std::vector<TextShadow> shadows;
    std::string trimmed = trim(val);
    if (trimmed.empty() || trimmed == "none") return shadows;
    // Split into comma-separated layers, ignoring commas inside rgb()/hsl().
    for (const auto& layerRaw : splitTopLevel(trimmed, ',')) {
        std::string v = trim(layerRaw);
        if (v.empty()) continue;
        // Extract a color token (hex or rgb()/hsl()) wherever it appears.
        std::string colorStr;
        auto hashPos = v.find('#');
        if (hashPos != std::string::npos) {
            auto end = v.find(' ', hashPos + 1);
            if (end == std::string::npos) end = v.size();
            colorStr = v.substr(hashPos, end - hashPos);
            v = v.substr(0, hashPos) + v.substr(end);
        } else {
            auto rgbPos = v.find("rgb");
            auto hslPos = v.find("hsl");
            auto colorPos = rgbPos != std::string::npos ? rgbPos : hslPos;
            if (colorPos != std::string::npos) {
                auto end = v.find(')', colorPos);
                if (end != std::string::npos) {
                    colorStr = v.substr(colorPos, end - colorPos + 1);
                    v = v.substr(0, colorPos) + v.substr(end + 1);
                }
            } else {
                // Named color leading or trailing the lengths (e.g. "red 1px 1px").
                std::istringstream cs(v);
                std::string tok;
                std::string rest;
                while (cs >> tok) {
                    bool isLen = !tok.empty() &&
                        (std::isdigit((unsigned char)tok[0]) || tok[0] == '-' ||
                         tok[0] == '+' || tok[0] == '.');
                    if (!isLen && colorStr.empty()) {
                        colorStr = tok;  // treat first non-length token as color
                    } else {
                        if (!rest.empty()) rest += ' ';
                        rest += tok;
                    }
                }
                v = rest;
            }
        }
        std::istringstream ss(trim(v));
        std::vector<float> vals;
        std::string token;
        while (ss >> token) {
            vals.push_back(parseLengthPixels(token, emBase));
        }
        TextShadow sh;
        if (vals.size() >= 1) sh.offsetX = vals[0];
        if (vals.size() >= 2) sh.offsetY = vals[1];
        if (vals.size() >= 3) sh.blur = std::max(0.0f, vals[2]);
        sh.color = colorStr.empty() ? Color(0, 0, 0, 1.0f) : parseColor(colorStr);
        // A text-shadow needs at least the two offsets to be meaningful.
        if (vals.size() >= 2) shadows.push_back(sh);
    }
    return shadows;
}
std::string StyleSheet::formatCounter(int value, const std::string& style) {
    auto toRoman = [](int n, bool upper) -> std::string {
        if (n <= 0 || n >= 4000) return std::to_string(n);
        static const int vals[] = {1000,900,500,400,100,90,50,40,10,9,5,4,1};
        static const char* up[] = {"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};
        static const char* lo[] = {"m","cm","d","cd","c","xc","l","xl","x","ix","v","iv","i"};
        std::string out;
        for (int i = 0; i < 13; ++i) {
            while (n >= vals[i]) { out += (upper ? up[i] : lo[i]); n -= vals[i]; }
        }
        return out;
    };
    auto toAlpha = [](int n, bool upper) -> std::string {
        if (n <= 0) return std::to_string(n);
        std::string out;
        char base = upper ? 'A' : 'a';
        while (n > 0) {
            int rem = (n - 1) % 26;
            out.insert(out.begin(), (char)(base + rem));
            n = (n - 1) / 26;
        }
        return out;
    };
    if (style == "lower-roman") return toRoman(value, false);
    if (style == "upper-roman") return toRoman(value, true);
    if (style == "lower-alpha" || style == "lower-latin") return toAlpha(value, false);
    if (style == "upper-alpha" || style == "upper-latin") return toAlpha(value, true);
    if (style == "decimal-leading-zero") {
        std::string s = std::to_string(value);
        if (value >= 0 && value < 10) s = "0" + s;
        return s;
    }
    if (style == "none") return "";
    return std::to_string(value);  // decimal (default)
}
std::string StyleSheet::substituteCounters(const std::string& content,
                                           const std::unordered_map<std::string,int>& counters) {
    if (content.find("counter") == std::string::npos) return content;
    std::string out;
    size_t i = 0;
    auto lookup = [&](const std::string& name) -> int {
        auto it = counters.find(name);
        return it != counters.end() ? it->second : 0;
    };
    while (i < content.size()) {
        // Match counter( or counters( at this position.
        bool isCounters = content.compare(i, 9, "counters(") == 0;
        bool isCounter = !isCounters && content.compare(i, 8, "counter(") == 0;
        if (isCounter || isCounters) {
            size_t open = content.find('(', i);
            size_t close = content.find(')', open);
            if (close == std::string::npos) { out += content.substr(i); break; }
            std::string args = content.substr(open + 1, close - open - 1);
            // Split args by comma.
            std::vector<std::string> a;
            std::string cur;
            for (char c : args) {
                if (c == ',') { a.push_back(trim(cur)); cur.clear(); }
                else cur += c;
            }
            a.push_back(trim(cur));
            auto unquote = [](std::string s) {
                if (s.size() >= 2 && ((s.front()=='"'&&s.back()=='"')||(s.front()=='\''&&s.back()=='\'')))
                    return s.substr(1, s.size()-2);
                return s;
            };
            if (isCounters) {
                // counters(name, sep [, style]) — flat list has a single level,
                // so this behaves like counter() with no separator repetition.
                std::string name = a.size() > 0 ? a[0] : "";
                std::string style = a.size() > 2 ? a[2] : "decimal";
                out += formatCounter(lookup(name), style);
            } else {
                std::string name = a.size() > 0 ? a[0] : "";
                std::string style = a.size() > 1 ? a[1] : "decimal";
                out += formatCounter(lookup(name), style);
            }
            i = close + 1;
        } else {
            out += content[i++];
        }
    }
    return out;
}
std::vector<std::pair<std::string,int>> StyleSheet::parseCounterList(const std::string& val, int defaultValue) {
    std::vector<std::pair<std::string,int>> out;
    std::string v = trim(val);
    if (v.empty() || v == "none") return out;
    // Tokens: <name> [<int>]? repeated. e.g. "section 0 item" / "chapter 2".
    std::vector<std::string> toks;
    std::string cur;
    for (char c : v) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
        } else cur += c;
    }
    if (!cur.empty()) toks.push_back(cur);
    for (size_t i = 0; i < toks.size(); ) {
        std::string name = toks[i++];
        int value = defaultValue;
        if (i < toks.size()) {
            // Is the next token an integer?
            const std::string& t = toks[i];
            bool isInt = !t.empty() && (std::isdigit((unsigned char)t[0]) || t[0] == '-' || t[0] == '+');
            if (isInt) { value = (int)parseFloat(t); ++i; }
        }
        out.emplace_back(name, value);
    }
    return out;
}
float StyleSheet::parseFloat(const std::string& val) {
    std::string v = trim(val);
    for (auto& suffix : {"px", "rem", "em", "deg", "ms", "s", "%"}) {
        auto pos = v.find(suffix);
        if (pos != std::string::npos) {
            v = v.substr(0, pos);
            break;
        }
    }
    try {
        return std::stof(v);
    } catch (...) {
        return 0;
    }
}
float StyleSheet::parseDuration(const std::string& val) {
    std::string v = trim(val);
    if (v.size() > 2 && v.substr(v.size() - 2) == "ms") {
        return parseFloat(v) / 1000.0f;
    }
    if (v.size() > 1 && v.back() == 's') {
        return parseFloat(v);
    }
    return parseFloat(v);
}
static bool supportsFeatureMatches(std::string_view feature) {
    while (!feature.empty() && feature.front() == '(') {
        if (feature.back() == ')') {
            feature.remove_prefix(1);
            feature.remove_suffix(1);
        } else {
            break;
        }
    }
    while (!feature.empty() && std::isspace((unsigned char)feature.front())) feature.remove_prefix(1);
    while (!feature.empty() && std::isspace((unsigned char)feature.back())) feature.remove_suffix(1);
    if (feature.empty()) return false;
    size_t colon = feature.find(':');
    if (colon == std::string_view::npos) return false;
    std::string prop = StyleSheet::trim(std::string(feature.substr(0, colon)));
    std::string val = StyleSheet::trim(std::string(feature.substr(colon + 1)));
    for (char& c : prop) c = std::tolower((unsigned char)c);
    for (char& c : val) c = std::tolower((unsigned char)c);
    if (prop == "display") {
        return val == "block" || val == "flex" || val == "inline-block" ||
               val == "inline" || val == "none" || val == "grid" ||
               val.rfind("table", 0) == 0;
    }
    if (prop == "position") {
        return val == "static" || val == "relative" || val == "absolute" || val == "fixed" || val == "sticky";
    }
    if (prop == "grid-template-columns" || prop == "grid-template-rows" || prop == "grid-column" || prop == "grid-row") {
        return true;
    }
    if (prop == "transition" || prop == "animation" || prop == "transform") {
        return true;
    }
    return prop == "color" || prop == "background-color" || prop == "border" ||
           prop == "border-radius" || prop == "padding" || prop == "margin" ||
           prop == "font-size" || prop == "font-weight" || prop == "font-style" ||
           prop == "font-family" || prop == "line-height" || prop == "text-align" ||
           prop == "width" || prop == "height" || prop == "min-width" || prop == "min-height" ||
           prop == "max-width" || prop == "max-height" || prop == "gap" || prop == "row-gap" ||
           prop == "column-gap" || prop == "flex-grow" || prop == "flex-shrink" || prop == "flex-basis";
}
bool supportsConditionMatches(std::string_view cond) {
    while (!cond.empty() && std::isspace((unsigned char)cond.front())) cond.remove_prefix(1);
    while (!cond.empty() && std::isspace((unsigned char)cond.back())) cond.remove_suffix(1);
    if (cond.empty()) return false;
    if (cond.rfind("not ", 0) == 0 || cond.rfind("not(", 0) == 0) {
        size_t skip = cond[3] == '(' ? 3 : 4;
        return !supportsConditionMatches(cond.substr(skip));
    }
    int depth = 0;
    for (size_t i = 0; i < cond.size(); ++i) {
        if (cond[i] == '(') depth++;
        else if (cond[i] == ')') depth--;
        else if (depth == 0) {
            if (i + 5 < cond.size() && cond.substr(i, 5) == " and ") {
                return supportsConditionMatches(cond.substr(0, i)) &&
                       supportsConditionMatches(cond.substr(i + 5));
            }
            if (i + 4 < cond.size() && cond.substr(i, 4) == " or ") {
                return supportsConditionMatches(cond.substr(0, i)) ||
                       supportsConditionMatches(cond.substr(i + 4));
            }
        }
    }
    if (cond.front() == '(' && cond.back() == ')') {
        return supportsConditionMatches(cond.substr(1, cond.size() - 2));
    }
    return supportsFeatureMatches(cond);
}
void StyleSheet::parseFontFace(const std::string& body) {
    std::string fontFamily;
    std::string src;
    for (std::string line : splitDeclarations(body)) {
        line = trim(line);
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = lowerAscii(trim(line.substr(0, colon)));
        std::string val = trim(line.substr(colon + 1));
        if (name == "font-family") {
            if (val.size() >= 2 && (val.front() == '"' || val.front() == '\'') && val.front() == val.back()) {
                fontFamily = val.substr(1, val.size() - 2);
            } else {
                fontFamily = val;
            }
        } else if (name == "src") {
            size_t urlStart = val.find("url(");
            if (urlStart != std::string::npos) {
                size_t start = urlStart + 4;
                size_t end = val.find(')', start);
                if (end != std::string::npos) {
                    std::string urlPath = trim(val.substr(start, end - start));
                    if (urlPath.size() >= 2 && (urlPath.front() == '"' || urlPath.front() == '\'') && urlPath.front() == urlPath.back()) {
                        src = urlPath.substr(1, urlPath.size() - 2);
                    } else {
                        src = urlPath;
                    }
                }
            } else {
                src = val;
            }
        }
    }
    if (!fontFamily.empty() && !src.empty()) {
        fontFaces.push_back({fontFamily, src});
    }
}
void StyleSheet::parsePropertyRule(const std::string& name, const std::string& body) {
    CSSPropertyDefinition def;
    def.name = name;
    auto declarations = splitDeclarations(body);
    for (const auto& decl : declarations) {
        size_t colon = decl.find(':');
        if (colon == std::string::npos) continue;
        std::string propName = lowerAscii(trim(decl.substr(0, colon)));
        std::string propValue = trim(decl.substr(colon + 1));
        if (propName == "syntax") {
            if (propValue.size() >= 2 && (propValue.front() == '"' || propValue.front() == '\'') && propValue.front() == propValue.back()) {
                propValue = propValue.substr(1, propValue.size() - 2);
            }
            def.syntax = propValue;
        } else if (propName == "inherits") {
            def.inherits = (propValue == "true");
        } else if (propName == "initial-value") {
            def.initialValue = propValue;
        }
    }
    propertyDefinitions_[name] = def;
}
static void extractSelectorFeatures(const std::string& part,
                                    std::vector<std::string>& classes,
                                    std::vector<std::string>& ids,
                                    std::vector<std::string>& types) {
    size_t i = 0;
    if (i < part.size() && part[i] != '.' && part[i] != '#' && part[i] != '[' && part[i] != ':') {
        std::string type;
        while (i < part.size() && part[i] != '.' && part[i] != '#' && part[i] != '[' && part[i] != ':') {
            type += part[i];
            i++;
        }
        if (!type.empty() && type != "*") {
            types.push_back(type);
        }
    }
    while (i < part.size()) {
        if (part[i] == '.') {
            i++;
            std::string cls;
            while (i < part.size() && part[i] != '.' && part[i] != '#' && part[i] != '[' && part[i] != ':') {
                cls += part[i];
                i++;
            }
            if (!cls.empty()) classes.push_back(cls);
        } else if (part[i] == '#') {
            i++;
            std::string id;
            while (i < part.size() && part[i] != '.' && part[i] != '#' && part[i] != '[' && part[i] != ':') {
                id += part[i];
                i++;
            }
            if (!id.empty()) ids.push_back(id);
        } else if (part[i] == '[') {
            while (i < part.size() && part[i] != ']') {
                i++;
            }
            if (i < part.size()) i++;
        } else {
            i++;
        }
    }
}
void StyleSheet::buildInvalidationSets() {
    classInvalidationSets_.clear();
    idInvalidationSets_.clear();
    typeInvalidationSets_.clear();
    for (const auto& rule : rules) {
        if (rule.parts.size() <= 1) continue;
        size_t K = rule.parts.size() - 1;
        for (size_t i = 0; i < K; ++i) {
            std::vector<std::string> srcClasses, srcIds, srcTypes;
            extractSelectorFeatures(rule.parts[i], srcClasses, srcIds, srcTypes);
            for (size_t j = i + 1; j <= K; ++j) {
                char relationship = ' ';
                if (j == i + 1) {
                    relationship = rule.combinators[i];
                } else {
                    bool hasSibling = false;
                    bool hasDescendant = false;
                    for (size_t c = i; c < j; ++c) {
                        if (rule.combinators[c] == '+' || rule.combinators[c] == '~') {
                            hasSibling = true;
                        } else if (rule.combinators[c] == ' ') {
                            hasDescendant = true;
                        }
                    }
                    if (hasSibling) {
                        relationship = '~';
                    } else if (hasDescendant) {
                        relationship = ' ';
                    } else {
                        relationship = ' ';
                    }
                }
                std::vector<std::string> tgtClasses, tgtIds, tgtTypes;
                extractSelectorFeatures(rule.parts[j], tgtClasses, tgtIds, tgtTypes);
                if (tgtClasses.empty() && tgtIds.empty() && tgtTypes.empty()) {
                    auto addAllToInvalidationSet = [&](InvalidationSet& set) {
                        if (relationship == ' ') {
                            set.invalidateAllDescendants = true;
                        } else if (relationship == '>') {
                            set.invalidateAllChildren = true;
                        } else if (relationship == '+') {
                            set.invalidateAllAdjacentSiblings = true;
                        } else if (relationship == '~') {
                            set.invalidateAllSiblings = true;
                        }
                    };
                    for (const auto& c : srcClasses) addAllToInvalidationSet(classInvalidationSets_[c]);
                    for (const auto& idVal : srcIds) addAllToInvalidationSet(idInvalidationSets_[idVal]);
                    for (const auto& t : srcTypes) addAllToInvalidationSet(typeInvalidationSets_[t]);
                } else {
                    auto addToInvalidationSet = [&](InvalidationSet& set) {
                        if (relationship == ' ') {
                            for (const auto& c : tgtClasses) set.descendantClasses.insert(c);
                            for (const auto& idVal : tgtIds) set.descendantIds.insert(idVal);
                            for (const auto& t : tgtTypes) set.descendantTypes.insert(t);
                        } else if (relationship == '>') {
                            for (const auto& c : tgtClasses) set.childClasses.insert(c);
                            for (const auto& idVal : tgtIds) set.childIds.insert(idVal);
                            for (const auto& t : tgtTypes) set.childTypes.insert(t);
                        } else if (relationship == '+') {
                            for (const auto& c : tgtClasses) set.adjacentSiblingClasses.insert(c);
                            for (const auto& idVal : tgtIds) set.adjacentSiblingIds.insert(idVal);
                            for (const auto& t : tgtTypes) set.adjacentSiblingTypes.insert(t);
                        } else if (relationship == '~') {
                            for (const auto& c : tgtClasses) set.siblingClasses.insert(c);
                            for (const auto& idVal : tgtIds) set.siblingIds.insert(idVal);
                            for (const auto& t : tgtTypes) set.siblingTypes.insert(t);
                        }
                    };
                    for (const auto& c : srcClasses) addToInvalidationSet(classInvalidationSets_[c]);
                    for (const auto& idVal : srcIds) addToInvalidationSet(idInvalidationSets_[idVal]);
                    for (const auto& t : srcTypes) addToInvalidationSet(typeInvalidationSets_[t]);
                }
            }
        }
    }
}
const StyleSheet::InvalidationSet* StyleSheet::getClassInvalidationSet(const std::string& className) const {
    auto it = classInvalidationSets_.find(className);
    if (it != classInvalidationSets_.end()) {
        return &it->second;
    }
    return nullptr;
}
const StyleSheet::InvalidationSet* StyleSheet::getIdInvalidationSet(const std::string& id) const {
    auto it = idInvalidationSets_.find(id);
    if (it != idInvalidationSets_.end()) {
        return &it->second;
    }
    return nullptr;
}
static float parseKeyframeOffset(std::string_view sel) {
    std::string s = StyleSheet::trim(std::string(sel));
    if (s.empty()) return -1.0f;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s == "from") return 0.0f;
    if (s == "to")   return 1.0f;
    if (!s.empty() && s.back() == '%') {
        s.pop_back();
        return std::clamp(StyleSheet::parseFloat(s) / 100.0f, 0.0f, 1.0f);
    }
    float v = StyleSheet::parseFloat(s);
    if (v < 0.0f) return -1.0f;
    return std::clamp(v, 0.0f, 1.0f);
}
void StyleSheet::parseKeyframes(const std::string& prelude, const std::vector<CSSToken>& block) {
    std::string name = trim(prelude);
    if (name.empty()) return;
    CSSKeyframesRule rule;
    rule.name = name;
    size_t i = 0;
    const size_t n = block.size();
    while (i < n) {
        while (i < n && (block[i].type == CSSToken::Whitespace ||
                         block[i].type == CSSToken::Semicolon)) {
            i++;
        }
        if (i >= n) break;
        std::string selectorList;
        int parenDepth = 0;
        while (i < n) {
            const auto& t = block[i];
            if (t.type == CSSToken::LeftParenthesis) parenDepth++;
            else if (t.type == CSSToken::RightParenthesis && parenDepth > 0) parenDepth--;
            if (parenDepth == 0 && t.type == CSSToken::LeftBrace) break;
            selectorList += t.text;
            i++;
        }
        if (i >= n || block[i].type != CSSToken::LeftBrace) break;
        i++;
        std::string body;
        int braceDepth = 1;
        while (i < n && braceDepth > 0) {
            const auto& t = block[i];
            if (t.type == CSSToken::LeftBrace) braceDepth++;
            else if (t.type == CSSToken::RightBrace) {
                braceDepth--;
                if (braceDepth == 0) { i++; break; }
            }
            body += t.text;
            i++;
        }
        CSSKeyframeRule kf;
        std::string group = selectorList;
        size_t start = 0;
        bool anyOffset = false;
        while (start <= group.size()) {
            size_t comma = group.find(',', start);
            std::string part = trim(group.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
            float off = parseKeyframeOffset(part);
            if (off >= 0.0f) {
                kf.keyTimes.push_back(off);
                anyOffset = true;
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        if (!anyOffset) continue;
        for (std::string line : splitDeclarations(body)) {
            line = trim(line);
            if (line.empty()) continue;
            auto colon = line.find(':');
            if (colon == std::string::npos) continue;
            CSSProperty prop;
            prop.name = lowerAscii(trim(line.substr(0, colon)));
            prop.value = trim(line.substr(colon + 1));
            prop.sourceOrder = nextPropertyOrder_++;
            kf.properties.push_back(std::move(prop));
        }
        if (!kf.properties.empty()) {
            rule.keyframes.push_back(std::move(kf));
        }
    }
    std::sort(rule.keyframes.begin(), rule.keyframes.end(),
              [](const CSSKeyframeRule& a, const CSSKeyframeRule& b) {
                  float ma = a.keyTimes.empty() ? 0.0f : *std::min_element(a.keyTimes.begin(), a.keyTimes.end());
                  float mb = b.keyTimes.empty() ? 0.0f : *std::min_element(b.keyTimes.begin(), b.keyTimes.end());
                  return ma < mb;
              });
    for (auto& existing : keyframesRules) {
        if (existing.name == rule.name) {
            existing = std::move(rule);
            return;
        }
    }
    keyframesRules.push_back(std::move(rule));
}
const CSSKeyframesRule* StyleSheet::findKeyframes(const std::string& name) const {
    for (const auto& kf : keyframesRules) {
        if (kf.name == name) return &kf;
    }
    return nullptr;
}
static std::vector<std::string> splitTopLevelLocal(const std::string& value, char delimiter) {
    std::vector<std::string> out;
    std::string current;
    int depth = 0;
    for (char c : value) {
        if (c == '(' || c == '[') depth++;
        else if (c == ')' || c == ']') depth--;
        if (c == delimiter && depth == 0) {
            out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    out.push_back(current);
    return out;
}
static TimingFunction parseTimingFunctionToken(std::string_view tok);
static TimingFunction parseTimingFunctionToken(std::string tok) {
    tok = StyleSheet::trim(std::string(tok));
    if (tok.empty()) return TimingFunction::ease();
    for (char& c : tok) c = (char)std::tolower((unsigned char)c);
    if (tok == "linear")      return TimingFunction::linear();
    if (tok == "ease")        return TimingFunction::ease();
    if (tok == "ease-in")     return TimingFunction::easeIn();
    if (tok == "ease-out")    return TimingFunction::easeOut();
    if (tok == "ease-in-out") return TimingFunction::easeInOut();
    if (tok == "step-start")  return TimingFunction::stepStart();
    if (tok == "step-end")    return TimingFunction::stepEnd();
    if (tok.rfind("cubic-bezier(", 0) == 0 && tok.back() == ')') {
        std::string inner = tok.substr(13, tok.size() - 14);
        std::vector<std::string> parts = splitTopLevelLocal(inner, ',');
        if (parts.size() == 4) {
            float v[4];
            for (int i = 0; i < 4; ++i) v[i] = StyleSheet::parseFloat(parts[i]);
            return TimingFunction::bezier(v[0], v[1], v[2], v[3]);
        }
    }
    if (tok.rfind("steps(", 0) == 0 && tok.back() == ')') {
        std::string inner = tok.substr(6, tok.size() - 7);
        std::vector<std::string> parts = splitTopLevelLocal(inner, ',');
        int n = parts.empty() ? 1 : (int)StyleSheet::parseFloat(parts[0]);
        if (n < 1) n = 1;
        TimingFunction::StepPosition pos = TimingFunction::JumpEnd;
        if (parts.size() >= 2) {
            std::string p = StyleSheet::trim(parts[1]);
            for (char& c : p) c = (char)std::tolower((unsigned char)c);
            if      (p == "jump-start" || p == "start")     pos = TimingFunction::JumpStart;
            else if (p == "jump-end"   || p == "end")       pos = TimingFunction::JumpEnd;
            else if (p == "jump-none")                      pos = TimingFunction::JumpNone;
            else if (p == "jump-both" || p == "both")      pos = TimingFunction::JumpBoth;
        }
        return TimingFunction::steps(n, pos);
    }
    return TimingFunction::ease();
}
TimingFunction StyleSheet::parseTimingFunction(const std::string& value) {
    return parseTimingFunctionToken(value);
}
std::vector<TimingFunction> StyleSheet::parseTimingFunctionList(const std::string& value) {
    std::vector<TimingFunction> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseTimingFunctionToken(part));
    }
    return out;
}
std::vector<float> StyleSheet::parseDurationList(const std::string& value) {
    std::vector<float> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseDuration(part));
    }
    return out;
}
std::vector<std::string> StyleSheet::parseAnimationNameList(const std::string& value) {
    std::vector<std::string> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        std::string s = trim(part);
        if (s.empty()) continue;
        out.push_back(s);
    }
    return out;
}
static AnimationDirection parseAnimationDirectionKeyword(std::string tok) {
    for (char& c : tok) c = (char)std::tolower((unsigned char)c);
    tok = StyleSheet::trim(tok);
    if (tok == "normal")             return AnimationDirection::Normal;
    if (tok == "reverse")            return AnimationDirection::Reverse;
    if (tok == "alternate")          return AnimationDirection::Alternate;
    if (tok == "alternate-reverse")  return AnimationDirection::AlternateReverse;
    return AnimationDirection::Normal;
}
std::vector<AnimationDirection> StyleSheet::parseAnimationDirectionList(const std::string& value) {
    std::vector<AnimationDirection> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseAnimationDirectionKeyword(part));
    }
    return out;
}
static AnimationFillMode parseAnimationFillModeKeyword(std::string tok) {
    for (char& c : tok) c = (char)std::tolower((unsigned char)c);
    tok = StyleSheet::trim(tok);
    if (tok == "none")      return AnimationFillMode::None;
    if (tok == "forwards")  return AnimationFillMode::Forwards;
    if (tok == "backwards") return AnimationFillMode::Backwards;
    if (tok == "both")      return AnimationFillMode::Both;
    return AnimationFillMode::None;
}
std::vector<AnimationFillMode> StyleSheet::parseAnimationFillModeList(const std::string& value) {
    std::vector<AnimationFillMode> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseAnimationFillModeKeyword(part));
    }
    return out;
}
static AnimationPlayState parseAnimationPlayStateKeyword(std::string tok) {
    for (char& c : tok) c = (char)std::tolower((unsigned char)c);
    tok = StyleSheet::trim(tok);
    if (tok == "paused")  return AnimationPlayState::Paused;
    return AnimationPlayState::Running;
}
std::vector<AnimationPlayState> StyleSheet::parseAnimationPlayStateList(const std::string& value) {
    std::vector<AnimationPlayState> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseAnimationPlayStateKeyword(part));
    }
    return out;
}
static AnimationComposition parseAnimationCompositionKeyword(std::string tok) {
    for (char& c : tok) c = (char)std::tolower((unsigned char)c);
    tok = StyleSheet::trim(tok);
    if (tok == "add")        return AnimationComposition::Add;
    if (tok == "accumulate") return AnimationComposition::Accumulate;
    return AnimationComposition::Replace;
}
std::vector<AnimationComposition> StyleSheet::parseAnimationCompositionList(const std::string& value) {
    std::vector<AnimationComposition> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseAnimationCompositionKeyword(part));
    }
    return out;
}
static TransitionBehavior parseTransitionBehaviorKeyword(std::string tok) {
    for (char& c : tok) c = (char)std::tolower((unsigned char)c);
    tok = StyleSheet::trim(tok);
    if (tok == "allow-discrete") return TransitionBehavior::AllowDiscrete;
    return TransitionBehavior::Normal;
}
std::vector<TransitionBehavior> StyleSheet::parseTransitionBehaviorList(const std::string& value) {
    std::vector<TransitionBehavior> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        out.push_back(parseTransitionBehaviorKeyword(part));
    }
    return out;
}
std::vector<std::string> StyleSheet::parseTransitionPropertyList(const std::string& value) {
    std::vector<std::string> out;
    for (const auto& part : splitTopLevel(value, ',')) {
        std::string s = trim(part);
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        if (!s.empty()) out.push_back(s);
    }
    return out;
}
std::string StyleSheet::serializeTimingFunction(const TimingFunction& tf) {
    switch (tf.kind) {
        case TimingFunction::Linear:    return "linear";
        case TimingFunction::Ease:      return "ease";
        case TimingFunction::EaseIn:    return "ease-in";
        case TimingFunction::EaseOut:   return "ease-out";
        case TimingFunction::EaseInOut: return "ease-in-out";
        case TimingFunction::StepStart: return "step-start";
        case TimingFunction::StepEnd:   return "step-end";
        case TimingFunction::CubicBezier: {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "cubic-bezier(%g, %g, %g, %g)",
                          tf.params[0], tf.params[1], tf.params[2], tf.params[3]);
            return buf;
        }
        case TimingFunction::Steps: {
            const char* posNames[] = {"jump-start", "jump-end", "jump-none", "jump-both", "start", "end"};
            int idx = (int)tf.stepPosition;
            if (idx < 0 || idx > 5) idx = 1;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "steps(%d, %s)", tf.stepCount, posNames[idx]);
            return buf;
        }
    }
    return "ease";
}
float StyleSheet::sampleTimingFunction(const TimingFunction& tf, float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    switch (tf.kind) {
        case TimingFunction::Linear:
            return t;
        case TimingFunction::Ease:
        case TimingFunction::EaseIn:
        case TimingFunction::EaseOut:
        case TimingFunction::EaseInOut:
        case TimingFunction::CubicBezier: {
            float x1 = tf.params[0], y1 = tf.params[1];
            float x2 = tf.params[2], y2 = tf.params[3];
            float cx = 3.0f * x1;
            float bx = 3.0f * (x2 - x1) - cx;
            float ax = 1.0f - cx - bx;
            float cy = 3.0f * y1;
            float by = 3.0f * (y2 - y1) - cy;
            float ay = 1.0f - cy - by;
            auto sampleX = [&](float v) { return ((ax * v + bx) * v + cx) * v; };
            auto sampleY = [&](float v) { return ((ay * v + by) * v + cy) * v; };
            auto sampleDX = [&](float v) { return (3.0f * ax * v + 2.0f * bx) * v + cx; };
            float x = t;
            float t_guess = t;
            for (int i = 0; i < 8; ++i) {
                float x_est = sampleX(t_guess) - x;
                if (std::abs(x_est) < 1e-6f) return sampleY(t_guess);
                float d = sampleDX(t_guess);
                if (std::abs(d) < 1e-6f) break;
                t_guess -= x_est / d;
            }
            float lo = 0.0f, hi = 1.0f;
            t_guess = t;
            if (t_guess < lo) return sampleY(lo);
            if (t_guess > hi) return sampleY(hi);
            while (lo < hi) {
                float x_est = sampleX(t_guess);
                if (std::abs(x_est - x) < 1e-4f) return sampleY(t_guess);
                if (x > x_est) lo = t_guess; else hi = t_guess;
                t_guess = (hi + lo) * 0.5f;
            }
            return sampleY(t_guess);
        }
        case TimingFunction::StepStart:
            return 1.0f;
        case TimingFunction::StepEnd:
            return 0.0f;
        case TimingFunction::Steps: {
            int n = std::max(1, tf.stepCount);
            float progress = t * n;
            switch (tf.stepPosition) {
                case TimingFunction::JumpStart: return std::min(1.0f, std::ceil(progress) / n);
                case TimingFunction::JumpEnd:   return std::min(1.0f, std::floor(progress) / n);
                case TimingFunction::JumpNone: {
                    float steps = (float)n;
                    float cur = std::floor(progress);
                    if (cur >= steps) return 1.0f;
                    float frac = progress - cur;
                    if (frac <= 0.5f) return cur / steps;
                    return (cur + 1.0f) / steps;
                }
                case TimingFunction::JumpBoth: {
                    float steps = (float)n;
                    float cur = std::floor(progress);
                    if (cur < 0.0f) cur = 0.0f;
                    if (cur > steps) cur = steps;
                    return cur / steps;
                }
                case TimingFunction::Start: return t == 0.0f ? 0.0f : std::min(1.0f, std::ceil(t * n) / n);
                case TimingFunction::End:   return std::min(1.0f, std::floor(t * n) / n);
            }
            return t;
        }
    }
    return t;
}


} // namespace FluxUI