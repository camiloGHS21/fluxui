// FluxUI CSS Parser Implementation
#include "fluxui/css_parser.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace FluxUI {

std::string StyleSheet::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::vector<std::string> StyleSheet::splitTopLevel(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;

    for (char c : value) {
        if (c == '(') depth++;
        if (c == ')' && depth > 0) depth--;

        if (c == delimiter && depth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }

    if (!trim(current).empty()) parts.push_back(trim(current));
    return parts;
}

static bool hasClassName(const std::string& className, const std::string& wanted) {
    std::istringstream classes(className);
    std::string cls;
    while (classes >> cls) {
        if (cls == wanted) return true;
    }
    return false;
}

std::string StyleSheet::cacheKey(const std::string& className,
                                 const std::string& id,
                                 const std::string& type) {
    std::string key;
    key.reserve(className.size() + id.size() + type.size() + 2);
    key += className;
    key.push_back('\x1f');
    key += id;
    key.push_back('\x1f');
    key += type;
    return key;
}

bool StyleSheet::selectorMatches(const std::string& selector,
                                 const std::string& className,
                                 const std::string& id,
                                 const std::string& type,
                                 std::string* pseudo) {
    std::string s = trim(selector);
    if (pseudo) *pseudo = "";
    if (s.empty()) return false;

    auto unsupported = s.find_first_of(" >+~[");
    if (unsupported != std::string::npos) return false;

    auto colon = s.find(':');
    if (colon != std::string::npos) {
        if (pseudo) *pseudo = trim(s.substr(colon + 1));
        s = trim(s.substr(0, colon));
    }

    if (s == "*") return true;

    std::string requiredType;
    std::string requiredId;
    std::vector<std::string> requiredClasses;

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '.') {
            size_t start = ++i;
            while (i < s.size() && s[i] != '.' && s[i] != '#') i++;
            requiredClasses.push_back(s.substr(start, i - start));
        } else if (s[i] == '#') {
            size_t start = ++i;
            while (i < s.size() && s[i] != '.' && s[i] != '#') i++;
            requiredId = s.substr(start, i - start);
        } else {
            size_t start = i;
            while (i < s.size() && s[i] != '.' && s[i] != '#') i++;
            requiredType = trim(s.substr(start, i - start));
        }
    }

    if (!requiredType.empty() && requiredType != type) return false;
    if (!requiredId.empty() && requiredId != id) return false;

    for (const auto& cls : requiredClasses) {
        if (cls.empty() || !hasClassName(className, cls)) return false;
    }

    return !requiredType.empty() || !requiredId.empty() || !requiredClasses.empty();
}

bool StyleSheet::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string css((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    parse(css);
    return true;
}

void StyleSheet::parse(const std::string& css) {
    resolvedCache_.clear();

    std::string cleaned;
    cleaned.reserve(css.size());

    // Remove comments
    bool inComment = false;
    for (size_t i = 0; i < css.size(); i++) {
        if (!inComment && i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
            inComment = true;
            i++;
            continue;
        }
        if (inComment && i + 1 < css.size() && css[i] == '*' && css[i + 1] == '/') {
            inComment = false;
            i++;
            continue;
        }
        if (!inComment) cleaned += css[i];
    }

    // Parse rules
    size_t pos = 0;
    while (pos < cleaned.size()) {
        // Find selector
        size_t braceOpen = cleaned.find('{', pos);
        if (braceOpen == std::string::npos) break;

        std::string selector = trim(cleaned.substr(pos, braceOpen - pos));

        // Find matching close brace (handles nested)
        int depth = 1;
        size_t braceClose = braceOpen + 1;
        while (braceClose < cleaned.size() && depth > 0) {
            if (cleaned[braceClose] == '{') depth++;
            if (cleaned[braceClose] == '}') depth--;
            if (depth > 0) braceClose++;
        }

        if (braceClose < cleaned.size()) {
            std::string body = cleaned.substr(braceOpen + 1, braceClose - braceOpen - 1);
            parseRule(selector, body);
        }

        pos = braceClose + 1;
    }
}

void StyleSheet::parseRule(const std::string& selector, const std::string& body) {
    std::vector<CSSProperty> properties;

    // Parse properties
    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line, ';')) {
        line = trim(line);
        if (line.empty()) continue;

        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        CSSProperty prop;
        prop.name = trim(line.substr(0, colon));
        prop.value = trim(line.substr(colon + 1));
        properties.push_back(prop);
    }

    for (const auto& prop : properties) {
        if (prop.name.rfind("--", 0) == 0) {
            variables_[prop.name] = prop.value;
        }
    }

    for (const auto& sel : splitTopLevel(selector, ',')) {
        std::string cleanSelector = trim(sel);
        if (cleanSelector.empty() || cleanSelector == ":root") continue;

        CSSRule rule;
        rule.selector = cleanSelector;
        for (const auto& prop : properties) {
            if (prop.name.rfind("--", 0) != 0) {
                rule.properties.push_back(prop);
            }
        }
        if (!rule.properties.empty()) rules.push_back(rule);
    }
}

std::string StyleSheet::resolveValue(const std::string& value) const {
    std::string out;
    size_t pos = 0;

    while (pos < value.size()) {
        size_t varStart = value.find("var(", pos);
        if (varStart == std::string::npos) {
            out += value.substr(pos);
            break;
        }

        out += value.substr(pos, varStart - pos);
        size_t cursor = varStart + 4;
        int depth = 1;
        while (cursor < value.size() && depth > 0) {
            if (value[cursor] == '(') depth++;
            if (value[cursor] == ')') depth--;
            if (depth > 0) cursor++;
        }

        if (cursor >= value.size()) {
            out += value.substr(varStart);
            break;
        }

        std::string inner = trim(value.substr(varStart + 4, cursor - varStart - 4));
        std::string name = inner;
        std::string fallback;
        auto comma = inner.find(',');
        if (comma != std::string::npos) {
            name = trim(inner.substr(0, comma));
            fallback = trim(inner.substr(comma + 1));
        }

        auto it = variables_.find(name);
        out += (it != variables_.end()) ? it->second : fallback;
        pos = cursor + 1;
    }

    return trim(out);
}

Style StyleSheet::resolve(const std::string& className,
                          const std::string& id,
                          const std::string& type) const {
    std::string key = cacheKey(className, id, type);
    auto cached = resolvedCache_.find(key);
    if (cached != resolvedCache_.end()) {
        return cached->second;
    }

    Style style;

    for (auto& rule : rules) {
        std::string pseudo;
        if (selectorMatches(rule.selector, className, id, type, &pseudo)) {
            if (!pseudo.empty() && pseudo != "hover" &&
                pseudo != "focus" && pseudo != "focus-visible" &&
                pseudo != "active") continue;

            for (auto& prop : rule.properties) {
                std::string value = resolveValue(prop.value);
                if (pseudo == "hover") {
                    mergeHoverProperty(style, prop.name, value);
                } else if (pseudo == "focus" || pseudo == "focus-visible") {
                    mergeFocusProperty(style, prop.name, value);
                } else if (pseudo == "active") {
                    mergeActiveProperty(style, prop.name, value);
                } else {
                    mergeProperty(style, prop.name, value);
                }
            }
        }
    }

    resolvedCache_[std::move(key)] = style;
    return style;
}

void StyleSheet::mergeProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "color") {
        style.color = parseColor(value);
    } else if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.backgroundGradient = parseGradient(value);
        } else {
            style.backgroundColor = parseColor(value);
        }
    } else if (name == "border-radius") {
        style.borderRadius = parseBorderRadius(value);
    } else if (name == "border") {
        style.border = parseBorder(value);
    } else if (name == "border-color") {
        style.border.color = parseColor(value);
    } else if (name == "border-width") {
        style.border.width = parseFloat(value);
    } else if (name == "outline") {
        style.outline = parseBorder(value);
    } else if (name == "outline-color") {
        style.outline.color = parseColor(value);
    } else if (name == "outline-width") {
        style.outline.width = parseFloat(value);
    } else if (name == "outline-offset") {
        style.outlineOffset = parseFloat(value);
    } else if (name == "padding") {
        style.padding = parseEdgeInsets(value);
    } else if (name == "padding-top") {
        style.padding.top = parseFloat(value);
    } else if (name == "padding-right") {
        style.padding.right = parseFloat(value);
    } else if (name == "padding-bottom") {
        style.padding.bottom = parseFloat(value);
    } else if (name == "padding-left") {
        style.padding.left = parseFloat(value);
    } else if (name == "margin") {
        style.margin = parseEdgeInsets(value);
    } else if (name == "margin-top") {
        style.margin.top = parseFloat(value);
    } else if (name == "margin-right") {
        style.margin.right = parseFloat(value);
    } else if (name == "margin-bottom") {
        style.margin.bottom = parseFloat(value);
    } else if (name == "margin-left") {
        style.margin.left = parseFloat(value);
    } else if (name == "width") {
        style.width = parseCSSValue(value);
    } else if (name == "height") {
        style.height = parseCSSValue(value);
    } else if (name == "min-width") {
        style.minWidth = parseCSSValue(value);
    } else if (name == "min-height") {
        style.minHeight = parseCSSValue(value);
    } else if (name == "max-width") {
        style.maxWidth = parseCSSValue(value);
    } else if (name == "max-height") {
        style.maxHeight = parseCSSValue(value);
    } else if (name == "font-size") {
        style.fontSize = parseFloat(value);
    } else if (name == "font-weight") {
        style.fontWeight = (value == "bold" || parseFloat(value) >= 600.0f) ?
            FontWeight::Bold : FontWeight::Normal;
    } else if (name == "text-align") {
        if (value == "center") style.textAlign = TextAlign::Center;
        else if (value == "right") style.textAlign = TextAlign::Right;
        else style.textAlign = TextAlign::Left;
    } else if (name == "line-height") {
        style.lineHeight = parseFloat(value);
    } else if (name == "opacity") {
        style.opacity = parseFloat(value);
    } else if (name == "display") {
        if (value == "flex") style.display = Display::Flex;
        else if (value == "none") style.display = Display::None;
        else if (value == "inline-block") style.display = Display::InlineBlock;
        else style.display = Display::Block;
    } else if (name == "flex-direction") {
        if (value == "row") style.flexDirection = FlexDirection::Row;
        else if (value == "row-reverse") style.flexDirection = FlexDirection::RowReverse;
        else if (value == "column-reverse") style.flexDirection = FlexDirection::ColumnReverse;
        else style.flexDirection = FlexDirection::Column;
    } else if (name == "justify-content") {
        if (value == "flex-end") style.justifyContent = JustifyContent::FlexEnd;
        else if (value == "center") style.justifyContent = JustifyContent::Center;
        else if (value == "space-between") style.justifyContent = JustifyContent::SpaceBetween;
        else if (value == "space-around") style.justifyContent = JustifyContent::SpaceAround;
        else if (value == "space-evenly") style.justifyContent = JustifyContent::SpaceEvenly;
        else style.justifyContent = JustifyContent::FlexStart;
    } else if (name == "align-items") {
        if (value == "flex-end") style.alignItems = AlignItems::FlexEnd;
        else if (value == "center") style.alignItems = AlignItems::Center;
        else if (value == "flex-start") style.alignItems = AlignItems::FlexStart;
        else style.alignItems = AlignItems::Stretch;
    } else if (name == "gap") {
        style.gap = parseFloat(value);
    } else if (name == "flex-grow") {
        style.flexGrow = parseFloat(value);
    } else if (name == "flex-shrink") {
        style.flexShrink = parseFloat(value);
    } else if (name == "overflow") {
        if (value == "hidden") style.overflow = Overflow::Hidden;
        else if (value == "scroll") style.overflow = Overflow::Scroll;
        else style.overflow = Overflow::Visible;
    } else if (name == "box-shadow") {
        style.boxShadow = parseBoxShadow(value);
    } else if (name == "cursor") {
        if (value == "pointer") style.cursor = CursorType::Pointer;
        else if (value == "text") style.cursor = CursorType::Text;
        else style.cursor = CursorType::Default;
    } else if (name == "transition") {
        for (const auto& part : splitTopLevel(value, ',')) {
            std::istringstream ss(part);
            std::string token;
            while (ss >> token) {
                bool isMs = token.size() > 2 && token.substr(token.size() - 2) == "ms";
                bool isSec = token.size() > 1 && token.back() == 's' && !isMs;
                if (isMs || isSec) {
                    style.transitionDuration = parseDuration(token);
                    return;
                }
            }
        }
    } else if (name == "position") {
        if (value == "relative") style.position = Position::Relative;
        else if (value == "absolute") style.position = Position::Absolute;
        else if (value == "fixed") style.position = Position::Fixed;
        else style.position = Position::Static;
    } else if (name == "top") {
        style.top = parseCSSValue(value);
    } else if (name == "right") {
        style.right = parseCSSValue(value);
    } else if (name == "bottom") {
        style.bottom = parseCSSValue(value);
    } else if (name == "left") {
        style.left = parseCSSValue(value);
    } else if (name == "font-family") {
        style.fontFamily = value;
    } else if (name == "scale") {
        style.scale = parseFloat(value);
    } else if (name == "transform") {
        auto scalePos = value.find("scale(");
        if (scalePos != std::string::npos) {
            auto start = scalePos + 6;
            auto end = value.find(')', start);
            if (end != std::string::npos) {
                style.scale = parseFloat(value.substr(start, end - start));
            }
        }
    }
    // Hover states (custom extension: hover-background-color, hover-color)
    else if (name == "hover-background-color" || name == "--hover-bg") {
        style.hoverBackgroundColor = parseColor(value);
        style.hasHoverBg = true;
    } else if (name == "hover-color" || name == "--hover-color") {
        style.hoverColor = parseColor(value);
        style.hasHoverColor = true;
    } else if (name == "hover-border-color" || name == "--hover-border") {
        style.hoverBorderColor = parseColor(value);
        style.hasHoverBorder = true;
    } else if (name == "hover-opacity" || name == "--hover-opacity") {
        style.hoverOpacity = parseFloat(value);
    } else if (name == "hover-scale" || name == "--hover-scale") {
        style.hoverScale = parseFloat(value);
    }
}

void StyleSheet::mergeHoverProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") == std::string::npos) {
            style.hoverBackgroundColor = parseColor(value);
            style.hasHoverBg = true;
        }
    } else if (name == "color") {
        style.hoverColor = parseColor(value);
        style.hasHoverColor = true;
    } else if (name == "border-color") {
        style.hoverBorderColor = parseColor(value);
        style.hasHoverBorder = true;
    } else if (name == "border") {
        std::istringstream ss(value);
        std::string widthStr, typeStr, colorStr;
        ss >> widthStr >> typeStr;
        std::getline(ss, colorStr);
        colorStr = trim(colorStr);
        if (!colorStr.empty()) {
            style.hoverBorderColor = parseColor(colorStr);
            style.hasHoverBorder = true;
        }
    } else if (name == "opacity") {
        style.hoverOpacity = parseFloat(value);
    } else if (name == "scale") {
        style.hoverScale = parseFloat(value);
    } else if (name == "transform") {
        auto scalePos = value.find("scale(");
        if (scalePos != std::string::npos) {
            auto start = scalePos + 6;
            auto end = value.find(')', start);
            if (end != std::string::npos) {
                style.hoverScale = parseFloat(value.substr(start, end - start));
            }
        }
    } else if (name == "hover-background-color" || name == "--hover-bg") {
        style.hoverBackgroundColor = parseColor(value);
        style.hasHoverBg = true;
    } else if (name == "hover-color" || name == "--hover-color") {
        style.hoverColor = parseColor(value);
        style.hasHoverColor = true;
    } else if (name == "hover-border-color" || name == "--hover-border") {
        style.hoverBorderColor = parseColor(value);
        style.hasHoverBorder = true;
    } else if (name == "hover-opacity" || name == "--hover-opacity") {
        style.hoverOpacity = parseFloat(value);
    } else if (name == "hover-scale" || name == "--hover-scale") {
        style.hoverScale = parseFloat(value);
    }
}

void StyleSheet::mergeFocusProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") == std::string::npos) {
            style.focusBackgroundColor = parseColor(value);
            style.hasFocusBg = true;
        }
    } else if (name == "color") {
        style.focusColor = parseColor(value);
        style.hasFocusColor = true;
    } else if (name == "border-color") {
        style.focusBorderColor = parseColor(value);
        style.hasFocusBorder = true;
    } else if (name == "border") {
        style.focusBorderColor = parseBorder(value).color;
        style.hasFocusBorder = true;
    } else if (name == "outline") {
        style.focusOutline = parseBorder(value);
        style.hasFocusOutline = true;
    } else if (name == "outline-color") {
        style.focusOutline.color = parseColor(value);
        style.hasFocusOutline = true;
    } else if (name == "outline-width") {
        style.focusOutline.width = parseFloat(value);
        style.hasFocusOutline = true;
    } else if (name == "opacity") {
        style.focusOpacity = parseFloat(value);
    } else if (name == "scale") {
        style.focusScale = parseFloat(value);
    } else if (name == "transform") {
        auto scalePos = value.find("scale(");
        if (scalePos != std::string::npos) {
            auto start = scalePos + 6;
            auto end = value.find(')', start);
            if (end != std::string::npos) {
                style.focusScale = parseFloat(value.substr(start, end - start));
            }
        }
    }
}

void StyleSheet::mergeActiveProperty(Style& style, const std::string& name, const std::string& value) {
    if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") == std::string::npos) {
            style.activeBackgroundColor = parseColor(value);
            style.hasActiveBg = true;
        }
    } else if (name == "color") {
        style.activeColor = parseColor(value);
        style.hasActiveColor = true;
    } else if (name == "border-color") {
        style.activeBorderColor = parseColor(value);
        style.hasActiveBorder = true;
    } else if (name == "border") {
        style.activeBorderColor = parseBorder(value).color;
        style.hasActiveBorder = true;
    } else if (name == "outline") {
        style.activeOutline = parseBorder(value);
        style.hasActiveOutline = true;
    } else if (name == "outline-color") {
        style.activeOutline.color = parseColor(value);
        style.hasActiveOutline = true;
    } else if (name == "outline-width") {
        style.activeOutline.width = parseFloat(value);
        style.hasActiveOutline = true;
    } else if (name == "opacity") {
        style.activeOpacity = parseFloat(value);
    } else if (name == "scale") {
        style.activeScale = parseFloat(value);
    } else if (name == "transform") {
        auto scalePos = value.find("scale(");
        if (scalePos != std::string::npos) {
            auto start = scalePos + 6;
            auto end = value.find(')', start);
            if (end != std::string::npos) {
                style.activeScale = parseFloat(value.substr(start, end - start));
            }
        }
    }
}

Color StyleSheet::parseColor(const std::string& val) {
    std::string v = trim(val);
    if (v.empty()) return Color();

    // Hex colors
    if (v[0] == '#') return Color::fromHex(v);

    // Named colors
    if (v == "transparent") return Color(0, 0, 0, 0);
    if (v == "white") return Color(1, 1, 1, 1);
    if (v == "black") return Color(0, 0, 0, 1);
    if (v == "red") return Color(1, 0, 0, 1);
    if (v == "green") return Color(0, 0.5f, 0, 1);
    if (v == "blue") return Color(0, 0, 1, 1);
    if (v == "yellow") return Color(1, 1, 0, 1);
    if (v == "cyan") return Color(0, 1, 1, 1);
    if (v == "magenta") return Color(1, 0, 1, 1);

    // rgb(r, g, b) or rgba(r, g, b, a)
    if (v.substr(0, 4) == "rgba") {
        auto start = v.find('(');
        auto end = v.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string inner = v.substr(start + 1, end - start - 1);
            std::istringstream ss(inner);
            float r, g, b, a;
            char comma;
            ss >> r >> comma >> g >> comma >> b >> comma >> a;
            if (r > 1 || g > 1 || b > 1) { r /= 255; g /= 255; b /= 255; }
            return Color(r, g, b, a);
        }
    }
    if (v.substr(0, 3) == "rgb") {
        auto start = v.find('(');
        auto end = v.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string inner = v.substr(start + 1, end - start - 1);
            std::istringstream ss(inner);
            float r, g, b;
            char comma;
            ss >> r >> comma >> g >> comma >> b;
            if (r > 1 || g > 1 || b > 1) { r /= 255; g /= 255; b /= 255; }
            return Color(r, g, b, 1);
        }
    }

    // hsl(h, s%, l%)
    if (v.substr(0, 3) == "hsl") {
        auto start = v.find('(');
        auto end = v.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string inner = v.substr(start + 1, end - start - 1);
            // Remove % signs
            std::string clean;
            for (char c : inner) {
                if (c != '%') clean += c;
            }
            std::istringstream ss(clean);
            float h, s, l;
            char comma;
            ss >> h >> comma >> s >> comma >> l;
            s /= 100.0f;
            l /= 100.0f;
            return Color::fromHSL(h, s, l);
        }
    }

    return Color();
}

CSSValue StyleSheet::parseCSSValue(const std::string& val) {
    std::string v = trim(val);
    if (v == "auto") return CSSValue::autoVal();
    if (v.empty()) return CSSValue();

    if (v.back() == '%') {
        return CSSValue::pct(std::stof(v.substr(0, v.size() - 1)));
    }

    // Remove px suffix
    std::string numStr = v;
    auto pxPos = v.find("px");
    if (pxPos != std::string::npos) numStr = v.substr(0, pxPos);

    try {
        return CSSValue::px(std::stof(numStr));
    } catch (...) {
        return CSSValue();
    }
}

EdgeInsets StyleSheet::parseEdgeInsets(const std::string& val) {
    std::istringstream ss(val);
    std::vector<float> values;
    std::string token;
    while (ss >> token) {
        values.push_back(parseFloat(token));
    }

    if (values.size() == 1) return EdgeInsets(values[0]);
    if (values.size() == 2) return EdgeInsets(values[0], values[1]);
    if (values.size() == 3) return EdgeInsets(values[0], values[1], values[2], values[1]);
    if (values.size() >= 4) return EdgeInsets(values[0], values[1], values[2], values[3]);
    return EdgeInsets();
}

BorderRadius StyleSheet::parseBorderRadius(const std::string& val) {
    std::istringstream ss(val);
    std::vector<float> values;
    std::string token;
    while (ss >> token) {
        values.push_back(parseFloat(token));
    }

    if (values.size() == 1) return BorderRadius(values[0]);
    if (values.size() == 2) {
        BorderRadius br;
        br.tl = br.br = values[0];
        br.tr = br.bl = values[1];
        return br;
    }
    if (values.size() >= 4) {
        BorderRadius br;
        br.tl = values[0]; br.tr = values[1];
        br.br = values[2]; br.bl = values[3];
        return br;
    }
    return BorderRadius();
}

Border StyleSheet::parseBorder(const std::string& val) {
    std::string v = trim(val);
    if (v.empty() || v == "none" || v == "0" || v == "0px") {
        return Border();
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
            token == "yellow" || token == "cyan" || token == "magenta")) {
            border.color = parseColor(token);
            continue;
        }
        float width = parseFloat(token);
        if (width > 0 || token.find('0') != std::string::npos) {
            border.width = width;
        }
    }
    return border;
}

BoxShadow StyleSheet::parseBoxShadow(const std::string& val) {
    BoxShadow shadow;
    if (val == "none") return shadow;

    // box-shadow: offsetX offsetY blur spread color
    std::string v = val;

    // Check for inset
    if (v.find("inset") != std::string::npos) {
        shadow.inset = true;
        auto pos = v.find("inset");
        v = v.substr(0, pos) + v.substr(pos + 5);
    }

    // Find color (starts with # or rgb/rgba/hsl)
    std::string colorStr;
    auto hashPos = v.find('#');
    if (hashPos != std::string::npos) {
        // Extract from # to next space or end
        auto end = v.find(' ', hashPos + 1);
        if (end == std::string::npos) end = v.size();
        colorStr = v.substr(hashPos, end - hashPos);
        v = v.substr(0, hashPos) + v.substr(end);
    } else {
        auto rgbPos = v.find("rgb");
        if (rgbPos != std::string::npos) {
            auto end = v.find(')', rgbPos);
            if (end != std::string::npos) {
                colorStr = v.substr(rgbPos, end - rgbPos + 1);
                v = v.substr(0, rgbPos) + v.substr(end + 1);
            }
        }
    }

    // Parse numeric values
    std::istringstream ss(trim(v));
    std::vector<float> vals;
    std::string token;
    while (ss >> token) {
        vals.push_back(parseFloat(token));
    }

    if (vals.size() >= 1) shadow.offsetX = vals[0];
    if (vals.size() >= 2) shadow.offsetY = vals[1];
    if (vals.size() >= 3) shadow.blur = vals[2];
    if (vals.size() >= 4) shadow.spread = vals[3];
    if (!colorStr.empty()) shadow.color = parseColor(colorStr);

    return shadow;
}

Gradient StyleSheet::parseGradient(const std::string& val) {
    Gradient grad;
    if (val.find("linear-gradient") == std::string::npos) return grad;

    grad.type = Gradient::Linear;

    auto start = val.find('(');
    auto end = val.rfind(')');
    if (start == std::string::npos || end == std::string::npos) return grad;

    std::string inner = val.substr(start + 1, end - start - 1);

    // Split by commas (respecting parentheses)
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

    // First part might be angle
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

    // Parse color stops
    float autoPos = 0;
    float autoStep = (parts.size() > colorStart + 1) ?
        1.0f / (float)(parts.size() - colorStart - 1) : 1.0f;

    for (size_t i = colorStart; i < parts.size(); i++) {
        std::string part = trim(parts[i]);
        // Try to find a percentage at the end
        float pos = autoPos;

        // Check if there's a percentage
        auto pctPos = part.rfind('%');
        if (pctPos != std::string::npos) {
            // Find the number before %
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

float StyleSheet::parseFloat(const std::string& val) {
    std::string v = trim(val);
    // Remove common suffixes
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

} // namespace FluxUI
