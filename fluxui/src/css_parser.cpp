// FluxUI CSS Parser Implementation
#include "fluxui/css_parser.h"
#include <algorithm>
#include <cctype>
#include <cmath>
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

std::vector<std::string> StyleSheet::splitDeclarations(const std::string& body) {
    std::vector<std::string> declarations;
    std::string current;
    int depth = 0;
    char quote = 0;
    bool escaped = false;

    for (char c : body) {
        if (quote != 0) {
            current += c;
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            current += c;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            depth++;
        } else if ((c == ')' || c == ']' || c == '}') && depth > 0) {
            depth--;
        }

        if (c == ';' && depth == 0) {
            std::string item = trim(current);
            if (!item.empty()) declarations.push_back(item);
            current.clear();
        } else {
            current += c;
        }
    }

    std::string item = trim(current);
    if (!item.empty()) declarations.push_back(item);
    return declarations;
}

static bool hasClassName(const std::string& className, const std::string& wanted) {
    std::istringstream classes(className);
    std::string cls;
    while (classes >> cls) {
        if (cls == wanted) return true;
    }
    return false;
}

static std::string trimLocal(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string lowerAscii(std::string s) {
    for (char& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

static std::string functionInner(const std::string& value) {
    auto start = value.find('(');
    auto end = value.rfind(')');
    if (start == std::string::npos || end == std::string::npos || end <= start) return "";
    return value.substr(start + 1, end - start - 1);
}

static void splitSelectorChain(const std::string& selector,
                               std::vector<std::string>& parts,
                               std::vector<char>& combinators);
static std::vector<std::string> splitSelectorListLocal(const std::string& selectorList);

static bool matchCompoundSelector(const std::string& compound,
                                  const std::string& className,
                                  const std::string& id,
                                  const std::string& type) {
    std::string s = trimLocal(compound);
    if (s.empty() || s == "*") return s == "*";

    std::string requiredType;
    std::string requiredId;
    std::vector<std::string> requiredClasses;

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '.') {
            size_t start = ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
            requiredClasses.push_back(s.substr(start, i - start));
        } else if (s[i] == '#') {
            size_t start = ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
            requiredId = s.substr(start, i - start);
        } else if (s[i] == ':') {
            size_t nameStart = ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
            std::string pseudoName = lowerAscii(s.substr(nameStart, i - nameStart));
            if (i >= s.size() || s[i] != '(') return false;

            size_t innerStart = ++i;
            int depth = 1;
            while (i < s.size() && depth > 0) {
                if (s[i] == '(') depth++;
                else if (s[i] == ')') depth--;
                if (depth > 0) i++;
            }
            if (i >= s.size()) return false;
            std::string inner = s.substr(innerStart, i - innerStart);
            ++i;

            if (pseudoName != "is" && pseudoName != "where" && pseudoName != "not") {
                return false;
            }

            bool matchedAny = false;
            for (const auto& selector : splitSelectorListLocal(inner)) {
                std::vector<std::string> parts;
                std::vector<char> combinators;
                splitSelectorChain(selector, parts, combinators);
                if (parts.size() == 1 &&
                    matchCompoundSelector(parts[0], className, id, type)) {
                    matchedAny = true;
                    break;
                }
            }

            if (pseudoName == "not") {
                if (matchedAny) return false;
            } else if (!matchedAny) {
                return false;
            }
        } else if (s[i] == '[' || s[i] == '+' || s[i] == '~') {
            return false;
        } else {
            size_t start = i;
            while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != ':' &&
                   s[i] != '[' && s[i] != '+' && s[i] != '~') {
                i++;
            }
            requiredType = lowerAscii(trimLocal(s.substr(start, i - start)));
        }
    }

    if (!requiredType.empty() && requiredType != lowerAscii(type)) return false;
    if (!requiredId.empty() && requiredId != id) return false;
    for (const auto& cls : requiredClasses) {
        if (cls.empty() || !hasClassName(className, cls)) return false;
    }
    return !requiredType.empty() || !requiredId.empty() || !requiredClasses.empty() || s == "*";
}

static void splitSelectorChain(const std::string& selector,
                               std::vector<std::string>& parts,
                               std::vector<char>& combinators) {
    std::string current;
    int depth = 0;
    char quote = 0;
    bool escaped = false;
    char pendingCombinator = 0;

    auto pushCurrent = [&]() {
        std::string part = trimLocal(current);
        if (part.empty()) return;
        if (!parts.empty()) {
            combinators.push_back(pendingCombinator ? pendingCombinator : ' ');
        }
        parts.push_back(part);
        current.clear();
        pendingCombinator = 0;
    };

    for (size_t i = 0; i < selector.size(); ++i) {
        char c = selector[i];
        if (quote != 0) {
            current += c;
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            current += c;
            continue;
        }
        if (c == '(' || c == '[') {
            depth++;
            current += c;
            continue;
        }
        if ((c == ')' || c == ']') && depth > 0) {
            depth--;
            current += c;
            continue;
        }
        if (depth == 0 && (c == '>' || c == '+' || c == '~')) {
            pushCurrent();
            pendingCombinator = c;
            continue;
        }
        if (depth == 0 && std::isspace((unsigned char)c)) {
            pushCurrent();
            if (pendingCombinator == 0) pendingCombinator = ' ';
            continue;
        }
        current += c;
    }
    pushCurrent();
}

static std::vector<std::string> splitSelectorListLocal(const std::string& selectorList) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    char quote = 0;
    bool escaped = false;

    for (char c : selectorList) {
        if (quote != 0) {
            current += c;
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            current += c;
            continue;
        }
        if (c == '(' || c == '[') depth++;
        else if ((c == ')' || c == ']') && depth > 0) depth--;

        if (c == ',' && depth == 0) {
            std::string item = trimLocal(current);
            if (!item.empty()) parts.push_back(item);
            current.clear();
        } else {
            current += c;
        }
    }

    std::string item = trimLocal(current);
    if (!item.empty()) parts.push_back(item);
    return parts;
}

static bool extractTrailingStatePseudo(std::string& selector, std::string* pseudo) {
    int depth = 0;
    size_t colon = std::string::npos;
    for (size_t i = 0; i < selector.size(); ++i) {
        char c = selector[i];
        if (c == '(' || c == '[') depth++;
        else if ((c == ')' || c == ']') && depth > 0) depth--;
        else if (c == ':' && depth == 0) colon = i;
    }

    if (colon == std::string::npos) return false;

    std::string name = lowerAscii(trimLocal(selector.substr(colon + 1)));
    if (name == "hover" || name == "focus" ||
        name == "focus-visible" || name == "active") {
        if (pseudo) *pseudo = name;
        selector = trimLocal(selector.substr(0, colon));
        return true;
    }
    return false;
}

static std::vector<std::string> splitColorTokens(const std::string& inner) {
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

static float parseNumberToken(std::string token, bool* isPercent = nullptr) {
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

static float parseRgbChannel(const std::string& token) {
    bool percent = false;
    float value = parseNumberToken(token, &percent);
    if (percent) return std::clamp(value / 100.0f, 0.0f, 1.0f);
    if (value > 1.0f) value /= 255.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

static float parseAlphaChannel(const std::string& token) {
    bool percent = false;
    float value = parseNumberToken(token, &percent);
    if (percent) value /= 100.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

static float parseHue(const std::string& token) {
    std::string v = trimLocal(lowerAscii(token));
    float hue = parseNumberToken(v);
    if (v.find("turn") != std::string::npos) hue *= 360.0f;
    else if (v.find("rad") != std::string::npos) hue = hue * 180.0f / 3.1415926535f;
    hue = std::fmod(hue, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    return hue;
}

static float parseHslPercent(const std::string& token) {
    bool percent = false;
    float value = parseNumberToken(token, &percent);
    if (percent || value > 1.0f) value /= 100.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

std::string StyleSheet::cacheKey(const std::string& className,
                                 const std::string& id,
                                 const std::string& type,
                                 const std::vector<CSSSelectorNode>& ancestors) {
    std::string key;
    key.reserve(className.size() + id.size() + type.size() + ancestors.size() * 12 + 3);
    key += className;
    key.push_back('\x1f');
    key += id;
    key.push_back('\x1f');
    key += type;
    for (const auto& ancestor : ancestors) {
        key.push_back('\x1e');
        key += ancestor.className;
        key.push_back('\x1f');
        key += ancestor.id;
        key.push_back('\x1f');
        key += ancestor.type;
    }
    return key;
}

bool StyleSheet::selectorMatches(const std::string& selector,
                                 const std::string& className,
                                 const std::string& id,
                                 const std::string& type,
                                 std::string* pseudo) {
    static const std::vector<CSSSelectorNode> noAncestors;
    return selectorMatches(selector, className, id, type, noAncestors, pseudo);
}

bool StyleSheet::selectorMatches(const std::string& selector,
                                 const std::string& className,
                                 const std::string& id,
                                 const std::string& type,
                                 const std::vector<CSSSelectorNode>& ancestors,
                                 std::string* pseudo) {
    std::string s = trim(selector);
    if (pseudo) *pseudo = "";
    if (s.empty()) return false;

    extractTrailingStatePseudo(s, pseudo);

    std::vector<std::string> parts;
    std::vector<char> combinators;
    splitSelectorChain(s, parts, combinators);
    if (parts.empty()) return false;

    int last = (int)parts.size() - 1;
    if (!matchCompoundSelector(parts[(size_t)last], className, id, type)) return false;

    size_t ancestorCursor = 0;
    for (int i = last - 1; i >= 0; --i) {
        char combinator = combinators[(size_t)i];
        if (combinator == '>') {
            if (ancestorCursor >= ancestors.size()) return false;
            const auto& ancestor = ancestors[ancestorCursor];
            if (!matchCompoundSelector(parts[(size_t)i],
                                       ancestor.className,
                                       ancestor.id,
                                       ancestor.type)) {
                return false;
            }
            ancestorCursor++;
            continue;
        }

        bool found = false;
        while (ancestorCursor < ancestors.size()) {
            const auto& ancestor = ancestors[ancestorCursor];
            ancestorCursor++;
            if (matchCompoundSelector(parts[(size_t)i],
                                      ancestor.className,
                                      ancestor.id,
                                      ancestor.type)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

void StyleSheet::appendClassTokens(const std::string& className, std::vector<std::string>& out) {
    std::istringstream classes(className);
    std::string cls;
    while (classes >> cls) {
        out.push_back(cls);
    }
}

int StyleSheet::selectorSpecificity(const std::string& selector) {
    std::string s = trim(selector);
    if (s.empty() || s == "*") return 0;

    int ids = 0;
    int classes = 0;
    int types = 0;
    bool atTokenStart = true;

    for (size_t i = 0; i < s.size();) {
        char c = s[i];
        if (std::isspace((unsigned char)c) || c == '>' || c == '+' || c == '~' || c == ',') {
            atTokenStart = true;
            ++i;
            continue;
        }

        if (c == '#') {
            ids++;
            ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) ++i;
            atTokenStart = false;
            continue;
        }

        if (c == '.') {
            classes++;
            ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) ++i;
            atTokenStart = false;
            continue;
        }

        if (c == '[') {
            classes++;
            int depth = 1;
            ++i;
            while (i < s.size() && depth > 0) {
                if (s[i] == '[') depth++;
                else if (s[i] == ']') depth--;
                ++i;
            }
            atTokenStart = false;
            continue;
        }

        if (c == ':') {
            bool pseudoElement = i + 1 < s.size() && s[i + 1] == ':';
            i += pseudoElement ? 2 : 1;
            size_t nameStart = i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) ++i;
            std::string name = lowerAscii(s.substr(nameStart, i - nameStart));

            if (pseudoElement) {
                types++;
            } else if (name == "is" || name == "not" || name == "has") {
                int maxInner = 0;
                if (i < s.size() && s[i] == '(') {
                    size_t innerStart = ++i;
                    int depth = 1;
                    while (i < s.size() && depth > 0) {
                        if (s[i] == '(') depth++;
                        else if (s[i] == ')') depth--;
                        if (depth > 0) ++i;
                    }
                    std::string inner = s.substr(innerStart, i - innerStart);
                    for (const auto& item : splitSelectorListLocal(inner)) {
                        maxInner = std::max(maxInner, selectorSpecificity(item));
                    }
                    if (i < s.size() && s[i] == ')') ++i;
                }
                ids += maxInner / 10000;
                classes += (maxInner % 10000) / 100;
                types += maxInner % 100;
                atTokenStart = false;
                continue;
            } else if (name == "where") {
                if (i < s.size() && s[i] == '(') {
                    int depth = 1;
                    ++i;
                    while (i < s.size() && depth > 0) {
                        if (s[i] == '(') depth++;
                        else if (s[i] == ')') depth--;
                        ++i;
                    }
                }
                atTokenStart = false;
                continue;
            } else {
                classes++;
            }

            if (i < s.size() && s[i] == '(') {
                int depth = 1;
                ++i;
                while (i < s.size() && depth > 0) {
                    if (s[i] == '(') depth++;
                    else if (s[i] == ')') depth--;
                    ++i;
                }
            }
            atTokenStart = false;
            continue;
        }

        if (c == '*') {
            ++i;
            atTokenStart = false;
            continue;
        }

        if (atTokenStart && (std::isalpha((unsigned char)c) || c == '_')) {
            types++;
        }
        while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) ++i;
        atTokenStart = false;
    }

    return ids * 10000 + classes * 100 + types;
}

CSSRuleIndexKey StyleSheet::selectorIndexKey(const std::string& selector) {
    std::string s = trim(selector);
    extractTrailingStatePseudo(s, nullptr);

    std::vector<std::string> parts;
    std::vector<char> combinators;
    splitSelectorChain(s, parts, combinators);
    if (parts.empty()) return {};

    std::string rightmost = trim(parts.back());
    if (rightmost.empty() || rightmost == "*") return {};

    CSSRuleIndexKey key;

    for (size_t i = 0; i < rightmost.size();) {
        char c = rightmost[i];
        if (c == '#') {
            size_t start = ++i;
            while (i < rightmost.size() &&
                   (std::isalnum((unsigned char)rightmost[i]) ||
                    rightmost[i] == '-' || rightmost[i] == '_')) {
                i++;
            }
            key.bucket = CSSRuleBucket::Id;
            key.key = rightmost.substr(start, i - start);
            return key;
        }
        if (c == '.') {
            size_t start = ++i;
            while (i < rightmost.size() &&
                   (std::isalnum((unsigned char)rightmost[i]) ||
                    rightmost[i] == '-' || rightmost[i] == '_')) {
                i++;
            }
            if (key.bucket != CSSRuleBucket::Class) {
                key.bucket = CSSRuleBucket::Class;
                key.key = rightmost.substr(start, i - start);
            }
            continue;
        }
        if (c == '[' || c == ':' || c == '+' || c == '~') {
            break;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < rightmost.size() &&
                   (std::isalnum((unsigned char)rightmost[i]) ||
                    rightmost[i] == '-' || rightmost[i] == '_')) {
                i++;
            }
            if (key.bucket == CSSRuleBucket::Universal) {
                key.bucket = CSSRuleBucket::Type;
                key.key = lowerAscii(rightmost.substr(start, i - start));
            }
            continue;
        }
        i++;
    }

    if (key.key.empty()) {
        key.bucket = CSSRuleBucket::Universal;
    }
    return key;
}

bool StyleSheet::stripImportant(std::string& value) {
    std::string trimmed = trim(value);
    std::string lower = lowerAscii(trimmed);
    size_t bang = lower.rfind('!');
    if (bang != std::string::npos && trim(lower.substr(bang + 1)) == "important") {
        value = trim(trimmed.substr(0, bang));
        return true;
    }
    value = trimmed;
    return false;
}

bool StyleSheet::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string css((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    parse(css);
    return true;
}

bool StyleSheet::setViewportSize(float width, float height) {
    width = std::max(0.0f, width);
    height = std::max(0.0f, height);
    if (std::abs(width - viewportWidth_) < 0.5f &&
        std::abs(height - viewportHeight_) < 0.5f) {
        return false;
    }
    viewportWidth_ = width;
    viewportHeight_ = height;
    resolvedCache_.clear();
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

    parseRules(cleaned, "");
}

void StyleSheet::parseRules(const std::string& css, const std::string& mediaQuery) {
    // Parse rules
    size_t pos = 0;
    while (pos < css.size()) {
        // Find selector
        size_t braceOpen = css.find('{', pos);
        if (braceOpen == std::string::npos) break;

        std::string selector = trim(css.substr(pos, braceOpen - pos));

        // Find matching close brace (handles nested)
        int depth = 1;
        size_t braceClose = braceOpen + 1;
        while (braceClose < css.size() && depth > 0) {
            if (css[braceClose] == '{') depth++;
            if (css[braceClose] == '}') depth--;
            if (depth > 0) braceClose++;
        }

        if (braceClose < css.size()) {
            std::string body = css.substr(braceOpen + 1, braceClose - braceOpen - 1);
            std::string lowerSelector = lowerAscii(selector);
            if (lowerSelector.rfind("@media", 0) == 0) {
                std::string query = trim(selector.substr(6));
                std::string combinedQuery = mediaQuery.empty()
                    ? query
                    : mediaQuery + " and " + query;
                parseRules(body, combinedQuery);
            } else if (!selector.empty() && selector[0] != '@') {
                parseRule(selector, body, mediaQuery);
            }
        }

        pos = braceClose + 1;
    }
}

void StyleSheet::parseRule(const std::string& selector, const std::string& body, const std::string& mediaQuery) {
    std::vector<CSSProperty> properties;

    // Parse properties
    for (std::string line : splitDeclarations(body)) {
        line = trim(line);
        if (line.empty()) continue;

        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        CSSProperty prop;
        prop.name = lowerAscii(trim(line.substr(0, colon)));
        prop.value = trim(line.substr(colon + 1));
        prop.sourceOrder = nextPropertyOrder_++;
        properties.push_back(prop);
    }

    for (const auto& sel : splitTopLevel(selector, ',')) {
        std::string cleanSelector = trim(sel);
        if (cleanSelector.empty()) continue;
        if (cleanSelector == ":root") {
            for (const auto& prop : properties) {
                if (prop.name.rfind("--", 0) == 0) {
                    variables_[prop.name] = prop.value;
                }
            }
            continue;
        }

        CSSRule rule;
        rule.selector = cleanSelector;
        rule.mediaQuery = mediaQuery;
        rule.specificity = selectorSpecificity(cleanSelector);
        for (const auto& prop : properties) {
            rule.properties.push_back(prop);
        }
        if (!rule.properties.empty()) {
            rules.push_back(std::move(rule));
            indexRule(rules.size() - 1);
        }
    }
}

void StyleSheet::indexRule(size_t ruleIndex) {
    if (ruleIndex >= rules.size()) return;

    CSSRuleIndexKey key = selectorIndexKey(rules[ruleIndex].selector);
    switch (key.bucket) {
        case CSSRuleBucket::Id:
            if (!key.key.empty()) {
                idRuleIndex_[key.key].push_back(ruleIndex);
                return;
            }
            break;
        case CSSRuleBucket::Class:
            if (!key.key.empty()) {
                classRuleIndex_[key.key].push_back(ruleIndex);
                return;
            }
            break;
        case CSSRuleBucket::Type:
            if (!key.key.empty()) {
                typeRuleIndex_[key.key].push_back(ruleIndex);
                return;
            }
            break;
        case CSSRuleBucket::Universal:
            break;
    }
    universalRuleIndex_.push_back(ruleIndex);
}

std::string StyleSheet::resolveValue(const std::string& value,
                                     const std::unordered_map<std::string, std::string>& customProperties,
                                     bool* valid) const {
    bool localValid = true;
    std::string resolved = resolveValueInternal(value, &customProperties, &localValid, 0);
    if (valid) *valid = localValid;
    return resolved;
}

std::string StyleSheet::resolveValueInternal(const std::string& value,
                                             const std::unordered_map<std::string, std::string>* customProperties,
                                             bool* valid,
                                             int depth) const {
    if (depth > 8) {
        if (valid) *valid = false;
        return "";
    }

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
        auto args = splitTopLevel(inner, ',');
        if (!args.empty()) {
            name = trim(args[0]);
            if (args.size() > 1) {
                fallback = args[1];
                for (size_t i = 2; i < args.size(); ++i) {
                    fallback += ", " + args[i];
                }
                fallback = trim(fallback);
            }
        }

        if (customProperties) {
            auto customIt = customProperties->find(name);
            if (customIt != customProperties->end()) {
                out += resolveValueInternal(customIt->second, customProperties, valid, depth + 1);
                pos = cursor + 1;
                continue;
            }
        }

        auto it = variables_.find(name);
        if (it != variables_.end()) {
            out += resolveValueInternal(it->second, customProperties, valid, depth + 1);
        } else if (!fallback.empty()) {
            out += resolveValueInternal(fallback, customProperties, valid, depth + 1);
        } else {
            if (valid) *valid = false;
            return "";
        }
        pos = cursor + 1;
    }

    return trim(out);
}

static bool applyCSSWideProperty(Style& target,
                                 const std::string& name,
                                 const std::string& keyword,
                                 const Style* parentStyle,
                                 const Style& initialStyle);

Style StyleSheet::resolve(const std::string& className,
                          const std::string& id,
                          const std::string& type) const {
    static const std::vector<CSSSelectorNode> noAncestors;
    return resolve(className, id, type, noAncestors);
}

Style StyleSheet::resolve(const std::string& className,
                          const std::string& id,
                          const std::string& type,
                          const std::vector<CSSSelectorNode>& ancestors) const {
    return resolve(className, id, type, ancestors, nullptr);
}

Style StyleSheet::resolve(const std::string& className,
                          const std::string& id,
                          const std::string& type,
                          const std::vector<CSSSelectorNode>& ancestors,
                          const Style* parentStyle) const {
    const auto* inheritedCustomProperties = parentStyle ? &parentStyle->customProperties : nullptr;
    std::string key = cacheKey(className, id, type, ancestors);
    if (inheritedCustomProperties && !inheritedCustomProperties->empty()) {
        std::vector<std::pair<std::string, std::string>> inherited(
            inheritedCustomProperties->begin(), inheritedCustomProperties->end());
        std::sort(inherited.begin(), inherited.end());
        for (const auto& entry : inherited) {
            key.push_back('\x1d');
            key += entry.first;
            key.push_back('=');
            key += entry.second;
        }
    }
    if (parentStyle) {
        key.push_back('\x1c');
        auto appendFloat = [&key](float value) {
            key += std::to_string((int)std::round(value * 1000.0f));
            key.push_back(',');
        };
        auto appendColor = [&](const Color& color) {
            appendFloat(color.r);
            appendFloat(color.g);
            appendFloat(color.b);
            appendFloat(color.a);
        };
        appendColor(parentStyle->color);
        appendFloat(parentStyle->fontSize);
        key += std::to_string((int)parentStyle->fontWeight) + ",";
        key += std::to_string((int)parentStyle->textAlign) + ",";
        appendFloat(parentStyle->lineHeight);
        key += parentStyle->fontFamily;
        key.push_back(',');
        appendFloat(parentStyle->margin.top);
        appendFloat(parentStyle->margin.right);
        appendFloat(parentStyle->margin.bottom);
        appendFloat(parentStyle->margin.left);
        appendFloat(parentStyle->padding.top);
        appendFloat(parentStyle->padding.right);
        appendFloat(parentStyle->padding.bottom);
        appendFloat(parentStyle->padding.left);
    }
    auto cached = resolvedCache_.find(key);
    if (cached != resolvedCache_.end()) {
        return cached->second;
    }

    Style style;
    applyUserAgentDefaults(style, type, ancestors);
    style.customProperties = variables_;
    if (inheritedCustomProperties) {
        for (const auto& entry : *inheritedCustomProperties) {
            style.customProperties[entry.first] = entry.second;
        }
    }

    struct CascadedProperty {
        const CSSProperty* property = nullptr;
        int specificity = 0;
        bool important = false;
    };

    auto lessCascadePriority = [](const CascadedProperty& a, const CascadedProperty& b) {
        if (a.important != b.important) return !a.important && b.important;
        if (a.specificity != b.specificity) return a.specificity < b.specificity;
        return a.property->sourceOrder < b.property->sourceOrder;
    };

    std::vector<CascadedProperty> baseProperties;
    std::vector<CascadedProperty> hoverProperties;
    std::vector<CascadedProperty> focusProperties;
    std::vector<CascadedProperty> activeProperties;
    std::vector<size_t> candidateRules;
    collectCandidateRules(className, id, type, candidateRules);

    for (size_t ruleIndex : candidateRules) {
        if (ruleIndex >= rules.size()) continue;
        const auto& rule = rules[ruleIndex];
        if (!mediaQueryMatches(rule.mediaQuery)) continue;
        std::string pseudo;
        if (selectorMatches(rule.selector, className, id, type, ancestors, &pseudo)) {
            if (!pseudo.empty() && pseudo != "hover" &&
                pseudo != "focus" && pseudo != "focus-visible" &&
                pseudo != "active") continue;

            for (const auto& prop : rule.properties) {
                std::string value = prop.value;
                CascadedProperty cascaded{&prop, rule.specificity, stripImportant(value)};
                if (pseudo == "hover") {
                    hoverProperties.push_back(cascaded);
                } else if (pseudo == "focus" || pseudo == "focus-visible") {
                    focusProperties.push_back(cascaded);
                } else if (pseudo == "active") {
                    activeProperties.push_back(cascaded);
                } else {
                    baseProperties.push_back(cascaded);
                }
            }
        }
    }

    auto applyCustomProperties = [&](std::vector<CascadedProperty>& properties,
                                     std::unordered_map<std::string, std::string>& customProperties) {
        std::sort(properties.begin(), properties.end(), lessCascadePriority);
        for (const auto& item : properties) {
            if (item.property->name.rfind("--", 0) != 0) continue;
            bool valid = true;
            std::string value = resolveValueInternal(item.property->value, &customProperties, &valid);
            if (!valid) continue;
            stripImportant(value);
            customProperties[item.property->name] = value;
        }
    };

    Style initialStyle;
    auto applyProperties = [&](std::vector<CascadedProperty>& properties,
                               auto mergeFn,
                               const std::unordered_map<std::string, std::string>& customProperties) {
        std::sort(properties.begin(), properties.end(), lessCascadePriority);
        for (const auto& item : properties) {
            if (item.property->name.rfind("--", 0) == 0) continue;
            bool valid = true;
            std::string value = resolveValueInternal(item.property->value, &customProperties, &valid);
            if (!valid) continue;
            stripImportant(value);
            if (applyCSSWideProperty(style,
                                     item.property->name,
                                     lowerAscii(value),
                                     parentStyle,
                                     initialStyle)) {
                continue;
            }
            mergeFn(style, item.property->name, value);
        }
    };

    applyCustomProperties(baseProperties, style.customProperties);
    applyProperties(baseProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeProperty(target, name, value);
    }, style.customProperties);

    auto hoverCustomProperties = style.customProperties;
    auto focusCustomProperties = style.customProperties;
    auto activeCustomProperties = style.customProperties;
    applyCustomProperties(hoverProperties, hoverCustomProperties);
    applyCustomProperties(focusProperties, focusCustomProperties);
    applyCustomProperties(activeProperties, activeCustomProperties);

    applyProperties(hoverProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeHoverProperty(target, name, value);
    }, hoverCustomProperties);
    applyProperties(focusProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeFocusProperty(target, name, value);
    }, focusCustomProperties);
    applyProperties(activeProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeActiveProperty(target, name, value);
    }, activeCustomProperties);

    resolvedCache_[std::move(key)] = style;
    return style;
}

void StyleSheet::collectCandidateRules(const std::string& className,
                                       const std::string& id,
                                       const std::string& type,
                                       std::vector<size_t>& out) const {
    out.clear();
    out.reserve(universalRuleIndex_.size() + 8);

    auto append = [&out](const std::vector<size_t>& rulesForKey) {
        out.insert(out.end(), rulesForKey.begin(), rulesForKey.end());
    };

    append(universalRuleIndex_);

    if (!id.empty()) {
        auto it = idRuleIndex_.find(id);
        if (it != idRuleIndex_.end()) append(it->second);
    }

    std::vector<std::string> classes;
    appendClassTokens(className, classes);
    for (const auto& cls : classes) {
        auto it = classRuleIndex_.find(cls);
        if (it != classRuleIndex_.end()) append(it->second);
    }

    std::string lowerType = lowerAscii(type);
    if (!lowerType.empty()) {
        auto it = typeRuleIndex_.find(lowerType);
        if (it != typeRuleIndex_.end()) append(it->second);
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

static std::vector<std::string> splitMediaAndClauses(const std::string& query) {
    std::vector<std::string> clauses;
    std::string current;
    int depth = 0;
    std::string lower = lowerAscii(query);

    for (size_t i = 0; i < query.size();) {
        char c = query[i];
        if (c == '(') depth++;
        else if (c == ')' && depth > 0) depth--;

        bool isAnd = depth == 0 &&
                     i + 5 <= query.size() &&
                     lower.compare(i, 5, " and ") == 0;
        if (isAnd) {
            std::string item = trimLocal(current);
            if (!item.empty()) clauses.push_back(item);
            current.clear();
            i += 5;
            continue;
        }

        current += c;
        ++i;
    }

    std::string item = trimLocal(current);
    if (!item.empty()) clauses.push_back(item);
    return clauses;
}

static bool parseMediaFeature(const std::string& clause,
                              float viewportWidth,
                              float viewportHeight) {
    std::string c = trimLocal(lowerAscii(clause));
    if (c.size() >= 2 && c.front() == '(' && c.back() == ')') {
        c = trimLocal(c.substr(1, c.size() - 2));
    }
    if (c.empty()) return true;

    auto colon = c.find(':');
    if (colon != std::string::npos) {
        std::string name = trimLocal(c.substr(0, colon));
        std::string value = trimLocal(c.substr(colon + 1));
        float numeric = parseNumberToken(value);
        if (value.find("rem") != std::string::npos ||
            value.find("em") != std::string::npos) {
            numeric *= 16.0f;
        }
        if (name == "min-width") return viewportWidth >= numeric;
        if (name == "max-width") return viewportWidth <= numeric;
        if (name == "width") return std::abs(viewportWidth - numeric) < 0.5f;
        if (name == "min-height") return viewportHeight >= numeric;
        if (name == "max-height") return viewportHeight <= numeric;
        if (name == "height") return std::abs(viewportHeight - numeric) < 0.5f;
        if (name == "orientation") {
            bool landscape = viewportWidth >= viewportHeight;
            return (value == "landscape" && landscape) ||
                   (value == "portrait" && !landscape);
        }
        if (name == "prefers-color-scheme") {
            return value == "dark";
        }
        return false;
    }

    auto compare = [&](const std::string& op, bool leftWidth) -> bool {
        auto pos = c.find(op);
        if (pos == std::string::npos) return false;
        std::string lhs = trimLocal(c.substr(0, pos));
        std::string rhs = trimLocal(c.substr(pos + op.size()));
        float viewport = leftWidth ? viewportWidth : viewportHeight;
        if (lhs == "width" || lhs == "height") {
            float value = parseNumberToken(rhs);
            if (rhs.find("rem") != std::string::npos ||
                rhs.find("em") != std::string::npos) {
                value *= 16.0f;
            }
            if (op == "<") return viewport < value;
            if (op == "<=") return viewport <= value;
            if (op == ">") return viewport > value;
            if (op == ">=") return viewport >= value;
        }
        if (rhs == "width" || rhs == "height") {
            viewport = rhs == "width" ? viewportWidth : viewportHeight;
            float value = parseNumberToken(lhs);
            if (lhs.find("rem") != std::string::npos ||
                lhs.find("em") != std::string::npos) {
                value *= 16.0f;
            }
            if (op == "<") return value < viewport;
            if (op == "<=") return value <= viewport;
            if (op == ">") return value > viewport;
            if (op == ">=") return value >= viewport;
        }
        return false;
    };

    if (c.find("width") != std::string::npos) {
        if (compare("<=", true) || compare(">=", true) ||
            compare("<", true) || compare(">", true)) {
            return true;
        }
    }
    if (c.find("height") != std::string::npos) {
        if (compare("<=", false) || compare(">=", false) ||
            compare("<", false) || compare(">", false)) {
            return true;
        }
    }

    return false;
}

bool StyleSheet::mediaQueryMatches(const std::string& query) const {
    std::string trimmed = trim(query);
    if (trimmed.empty()) return true;

    for (std::string alternative : splitTopLevel(trimmed, ',')) {
        alternative = trimLocal(lowerAscii(alternative));
        if (alternative.empty()) continue;

        bool negate = false;
        if (alternative.rfind("not ", 0) == 0) {
            negate = true;
            alternative = trimLocal(alternative.substr(4));
        }
        if (alternative.rfind("only ", 0) == 0) {
            alternative = trimLocal(alternative.substr(5));
        }

        bool matches = true;
        for (std::string clause : splitMediaAndClauses(alternative)) {
            clause = trimLocal(clause);
            if (clause == "all" || clause == "screen") {
                continue;
            }
            if (clause == "print" || clause == "speech") {
                matches = false;
                break;
            }
            if (!parseMediaFeature(clause, viewportWidth_, viewportHeight_)) {
                matches = false;
                break;
            }
        }

        if (negate) matches = !matches;
        if (matches) return true;
    }

    return false;
}

static bool isInheritedCSSProperty(const std::string& name) {
    return name == "color" || name == "font-size" ||
           name == "font-weight" || name == "font-family" ||
           name == "line-height" || name == "text-align" ||
           name == "visibility" || name == "cursor" ||
           name == "letter-spacing" || name == "word-spacing" ||
           name == "text-decoration" || name == "text-decoration-line" ||
           name == "text-transform" || name == "white-space" ||
           name == "text-overflow" || name == "word-break" ||
           name == "pointer-events";
}

static const Style& cssWideSource(const std::string& name,
                                  const std::string& keyword,
                                  const Style* parentStyle,
                                  const Style& initialStyle) {
    if (keyword == "inherit") {
        return parentStyle ? *parentStyle : initialStyle;
    }
    if (keyword == "unset" && isInheritedCSSProperty(name)) {
        return parentStyle ? *parentStyle : initialStyle;
    }
    return initialStyle;
}

static void copyAllNonCustomProperties(Style& target, const Style& source) {
    auto customProperties = std::move(target.customProperties);
    target = source;
    target.customProperties = std::move(customProperties);
}

static bool applyCSSWideProperty(Style& target,
                                 const std::string& name,
                                 const std::string& keyword,
                                 const Style* parentStyle,
                                 const Style& initialStyle) {
    if (keyword != "inherit" && keyword != "initial" && keyword != "unset") {
        return false;
    }

    const Style& source = cssWideSource(name, keyword, parentStyle, initialStyle);
    if (name == "all") {
        copyAllNonCustomProperties(target, source);
    } else if (name == "color") {
        target.color = source.color;
        target.hasColor = true;
    } else if (name == "background" || name == "background-color" || name == "background-image") {
        target.backgroundColor = source.backgroundColor;
        target.backgroundGradient = source.backgroundGradient;
    } else if (name == "border" || name == "border-top" ||
               name == "border-right" || name == "border-bottom" ||
               name == "border-left" || name == "border-block-start" ||
               name == "border-block-end" || name == "border-inline-start" ||
               name == "border-inline-end" || name == "border-color" ||
               name == "border-width") {
        target.border = source.border;
    } else if (name == "outline" || name == "outline-color" ||
               name == "outline-width" || name == "outline-offset") {
        target.outline = source.outline;
        target.outlineOffset = source.outlineOffset;
    } else if (name == "border-radius") {
        target.borderRadius = source.borderRadius;
    } else if (name == "box-shadow") {
        target.boxShadow = source.boxShadow;
    } else if (name == "padding" || name == "padding-top" ||
               name == "padding-right" || name == "padding-bottom" ||
               name == "padding-left" || name == "padding-block" ||
               name == "padding-inline" || name == "padding-block-start" ||
               name == "padding-block-end" || name == "padding-inline-start" ||
               name == "padding-inline-end") {
        target.padding = source.padding;
    } else if (name == "margin" || name == "margin-top" ||
               name == "margin-right" || name == "margin-bottom" ||
               name == "margin-left" || name == "margin-block" ||
               name == "margin-inline" || name == "margin-block-start" ||
               name == "margin-block-end" || name == "margin-inline-start" ||
               name == "margin-inline-end") {
        target.margin = source.margin;
    } else if (name == "width" || name == "inline-size") {
        target.width = source.width;
    } else if (name == "height" || name == "block-size") {
        target.height = source.height;
    } else if (name == "min-width" || name == "min-inline-size") {
        target.minWidth = source.minWidth;
    } else if (name == "min-height" || name == "min-block-size") {
        target.minHeight = source.minHeight;
    } else if (name == "max-width" || name == "max-inline-size") {
        target.maxWidth = source.maxWidth;
    } else if (name == "max-height" || name == "max-block-size") {
        target.maxHeight = source.maxHeight;
    } else if (name == "inset" || name == "top" ||
               name == "right" || name == "bottom" || name == "left" ||
               name == "inset-block" || name == "inset-inline" ||
               name == "inset-block-start" || name == "inset-block-end" ||
               name == "inset-inline-start" || name == "inset-inline-end") {
        target.top = source.top;
        target.right = source.right;
        target.bottom = source.bottom;
        target.left = source.left;
    } else if (name == "font-size") {
        target.fontSize = source.fontSize;
        target.hasFontSize = true;
    } else if (name == "font-weight") {
        target.fontWeight = source.fontWeight;
        target.hasFontWeight = true;
    } else if (name == "font-family") {
        target.fontFamily = source.fontFamily;
        target.hasFontFamily = true;
    } else if (name == "line-height") {
        target.lineHeight = source.lineHeight;
        target.hasLineHeight = true;
    } else if (name == "text-align") {
        target.textAlign = source.textAlign;
        target.hasTextAlign = true;
    } else if (name == "opacity") {
        target.opacity = source.opacity;
    } else if (name == "display") {
        target.display = source.display;
    } else if (name == "position") {
        target.position = source.position;
    } else if (name == "flex-direction") {
        target.flexDirection = source.flexDirection;
    } else if (name == "flex-wrap") {
        target.flexWrap = source.flexWrap;
    } else if (name == "justify-content") {
        target.justifyContent = source.justifyContent;
    } else if (name == "align-items") {
        target.alignItems = source.alignItems;
    } else if (name == "align-content") {
        target.alignContent = source.alignContent;
    } else if (name == "align-self") {
        target.alignSelf = source.alignSelf;
    } else if (name == "gap" || name == "row-gap" || name == "column-gap") {
        target.gap = source.gap;
        target.rowGap = source.rowGap;
        target.columnGap = source.columnGap;
    } else if (name == "flex") {
        target.flexGrow = source.flexGrow;
        target.flexShrink = source.flexShrink;
        target.flexBasis = source.flexBasis;
    } else if (name == "flex-grow") {
        target.flexGrow = source.flexGrow;
    } else if (name == "flex-shrink") {
        target.flexShrink = source.flexShrink;
    } else if (name == "flex-basis") {
        target.flexBasis = source.flexBasis;
    } else if (name == "order") {
        target.order = source.order;
    } else if (name == "overflow" || name == "overflow-x" || name == "overflow-y") {
        target.overflow = source.overflow;
        target.overflowX = source.overflowX;
        target.overflowY = source.overflowY;
    } else if (name == "cursor") {
        target.cursor = source.cursor;
    } else if (name == "transition") {
        target.transitionDuration = source.transitionDuration;
    } else if (name == "scale" || name == "transform") {
        target.scale = source.scale;
    } else if (name == "box-sizing") {
        target.boxSizing = source.boxSizing;
        target.hasBoxSizing = true;
    } else if (name == "visibility") {
        target.visibility = source.visibility;
    } else if (name == "text-overflow") {
        target.textOverflow = source.textOverflow;
    } else if (name == "white-space") {
        target.whiteSpace = source.whiteSpace;
    } else if (name == "text-decoration" || name == "text-decoration-line") {
        target.textDecoration = source.textDecoration;
    } else if (name == "text-transform") {
        target.textTransform = source.textTransform;
    } else if (name == "letter-spacing") {
        target.letterSpacing = source.letterSpacing;
    } else if (name == "word-spacing") {
        target.wordSpacing = source.wordSpacing;
    } else if (name == "pointer-events") {
        target.pointerEvents = source.pointerEvents;
    } else if (name == "z-index") {
        target.zIndex = source.zIndex;
        target.hasZIndex = source.hasZIndex;
    } else if (name == "aspect-ratio") {
        target.aspectRatio = source.aspectRatio;
    } else if (name == "object-fit") {
        target.objectFit = source.objectFit;
        target.hasObjectFit = source.hasObjectFit;
    } else if (name == "object-position") {
        target.objectPosition = source.objectPosition;
        target.objectPositionOffset = source.objectPositionOffset;
        target.hasObjectPosition = source.hasObjectPosition;
    } else if (name == "word-break") {
        target.wordBreak = source.wordBreak;
    } else {
        return false;
    }
    return true;
}

void StyleSheet::applyUserAgentDefaults(Style& style,
                                        const std::string& type,
                                        const std::vector<CSSSelectorNode>& ancestors) {
    std::string t = lowerAscii(type);
    constexpr float medium = 16.0f;
    auto isSectioning = [](const std::string& nodeType) {
        std::string lower = lowerAscii(nodeType);
        return lower == "article" || lower == "aside" ||
               lower == "nav" || lower == "section";
    };
    auto block = [&]() {
        style.display = Display::Block;
    };
    auto heading = [&](float size, float marginEm, bool setFontSize = true) {
        block();
        if (setFontSize) {
            style.fontSize = size;
            style.hasFontSize = true;
        }
        style.margin = EdgeInsets(marginEm * medium, 0.0f, marginEm * medium, 0.0f);
        style.fontWeight = FontWeight::Bold;
        style.hasFontWeight = true;
    };

    if (t == "h1") {
        int sectionDepth = 0;
        for (const auto& ancestor : ancestors) {
            if (isSectioning(ancestor.type)) ++sectionDepth;
        }
        static constexpr float sizes[] = {2.0f, 1.5f, 1.17f, 1.0f, 0.83f, 0.67f};
        static constexpr float margins[] = {0.67f, 0.83f, 1.0f, 1.33f, 1.67f, 2.33f};
        size_t rank = static_cast<size_t>(std::min(sectionDepth, 5));
        heading(sizes[rank] * medium, margins[rank]);
    } else if (t == "h2") {
        heading(1.5f * medium, 0.83f);
    } else if (t == "h3") {
        heading(1.17f * medium, 1.0f);
    } else if (t == "h4") {
        heading(medium, 1.33f, false);
    } else if (t == "h5") {
        heading(0.83f * medium, 1.67f);
    } else if (t == "h6") {
        heading(0.67f * medium, 2.33f);
    } else if (t == "p") {
        block();
        style.margin = EdgeInsets(medium, 0.0f, medium, 0.0f);
    } else if (t == "html") {
        block();
    } else if (t == "body") {
        block();
        style.margin = EdgeInsets(8.0f);
    } else if (t == "blockquote") {
        block();
        style.margin = EdgeInsets(medium, 40.0f, medium, 40.0f);
    } else if (t == "center") {
        block();
        style.textAlign = TextAlign::Center;
        style.hasTextAlign = true;
    } else if (t == "ul" || t == "ol" || t == "menu" || t == "dir") {
        block();
        style.margin = EdgeInsets(medium, 0.0f, medium, 0.0f);
        style.padding.left = 40.0f;
    } else if (t == "li") {
        block();
    } else if (t == "strong" || t == "b") {
        style.display = Display::InlineBlock;
        style.fontWeight = FontWeight::Bold;
        style.hasFontWeight = true;
    } else if (t == "small") {
        style.display = Display::InlineBlock;
        style.fontSize = 13.333f;
        style.hasFontSize = true;
    } else if (t == "pre" || t == "xmp" || t == "plaintext" || t == "listing") {
        block();
        style.margin = EdgeInsets(medium, 0.0f, medium, 0.0f);
        style.fontFamily = "monospace";
        style.hasFontFamily = true;
        style.whiteSpace = WhiteSpace::Pre;
        style.hasWhiteSpace = true;
    } else if (t == "code" || t == "kbd" || t == "samp" || t == "tt") {
        style.display = Display::InlineBlock;
        style.fontFamily = "monospace";
        style.hasFontFamily = true;
    } else if (t == "nobr") {
        style.display = Display::InlineBlock;
        style.whiteSpace = WhiteSpace::NoWrap;
        style.hasWhiteSpace = true;
    } else if (t == "img" || t == "svg" || t == "picture") {
        style.display = Display::InlineBlock;
        style.objectFit = ObjectFit::Fill;
        style.hasObjectFit = true;
    } else if (t == "rp" || t == "noframes") {
        style.display = Display::None;
    } else if (t == "div" || t == "article" || t == "aside" || t == "footer" ||
               t == "header" || t == "main" || t == "nav" || t == "section") {
        block();
    } else if (t == "span") {
        style.display = Display::InlineBlock;
    } else if (t == "button") {
        style.cursor = CursorType::Default;
        style.display = Display::Flex;
        style.textAlign = TextAlign::Center;
        style.hasTextAlign = true;
        style.padding = EdgeInsets(2.0f, 6.0f, 3.0f, 6.0f);
        style.border = Border(2.0f, Color(0.63f, 0.63f, 0.63f, 1.0f));
        style.backgroundColor = Color(0.94f, 0.94f, 0.94f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
        style.lineHeight = 1.2f;
        style.hasLineHeight = true;
    } else if (t == "input") {
        style.cursor = CursorType::Text;
        style.display = Display::InlineBlock;
        style.padding = EdgeInsets(1.0f);
        style.border = Border(2.0f, Color(0.62f, 0.62f, 0.62f, 1.0f));
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
        style.lineHeight = 1.2f;
        style.hasLineHeight = true;
    }
}

static Overflow parseOverflowKeyword(const std::string& value) {
    if (value == "hidden") return Overflow::Hidden;
    if (value == "scroll") return Overflow::Scroll;
    if (value == "auto") return Overflow::Auto;
    if (value == "clip") return Overflow::Clip;
    return Overflow::Visible;
}

static bool isOverflowVisibleOrClip(Overflow overflow) {
    return overflow == Overflow::Visible || overflow == Overflow::Clip;
}

static Overflow normalizeOverflowAxis(Overflow axis, Overflow otherAxis) {
    if (!isOverflowVisibleOrClip(otherAxis)) {
        if (axis == Overflow::Visible) return Overflow::Auto;
        if (axis == Overflow::Clip) return Overflow::Hidden;
    }
    return axis;
}

static void normalizeOverflowAxes(Style& style) {
    Overflow x = style.overflowX;
    Overflow y = style.overflowY;
    style.overflowX = normalizeOverflowAxis(x, y);
    style.overflowY = normalizeOverflowAxis(y, x);
    style.overflow = style.overflowY;
}

struct ObjectPositionAxis {
    float fraction = 0.5f;
    float offset = 0.0f;
    bool set = false;
};

static bool isObjectPositionLength(const std::string& token) {
    if (token.empty()) return false;
    char first = token[0];
    return first == '-' || first == '+' || first == '.' ||
           std::isdigit((unsigned char)first);
}

static bool setObjectPositionKeyword(const std::string& token,
                                     ObjectPositionAxis& x,
                                     ObjectPositionAxis& y) {
    if (token == "left") {
        x.fraction = 0.0f;
        x.offset = 0.0f;
        x.set = true;
        return true;
    }
    if (token == "right") {
        x.fraction = 1.0f;
        x.offset = 0.0f;
        x.set = true;
        return true;
    }
    if (token == "top") {
        y.fraction = 0.0f;
        y.offset = 0.0f;
        y.set = true;
        return true;
    }
    if (token == "bottom") {
        y.fraction = 1.0f;
        y.offset = 0.0f;
        y.set = true;
        return true;
    }
    if (token == "center") {
        if (!x.set) {
            x.fraction = 0.5f;
            x.offset = 0.0f;
            x.set = true;
        } else {
            y.fraction = 0.5f;
            y.offset = 0.0f;
            y.set = true;
        }
        return true;
    }
    return false;
}

static float parseObjectPositionFloat(const std::string& token) {
    char* end = nullptr;
    float value = std::strtof(token.c_str(), &end);
    return end == token.c_str() ? 0.0f : value;
}

static float parseObjectPositionLengthPixels(const std::string& token) {
    if (token.size() > 3 && token.substr(token.size() - 3) == "rem") {
        return parseObjectPositionFloat(token) * 16.0f;
    }
    if (token.size() > 2 && token.substr(token.size() - 2) == "em") {
        return parseObjectPositionFloat(token) * 16.0f;
    }
    return parseObjectPositionFloat(token);
}

static void setObjectPositionLength(const std::string& token,
                                    ObjectPositionAxis& axis,
                                    bool fromEnd) {
    if (!token.empty() && token.back() == '%') {
        axis.fraction = std::clamp(parseObjectPositionFloat(token) / 100.0f, -1.0f, 2.0f);
        axis.offset = 0.0f;
    } else {
        float px = parseObjectPositionLengthPixels(token);
        axis.fraction = fromEnd ? 1.0f : 0.0f;
        axis.offset = fromEnd ? -px : px;
    }
    axis.set = true;
}

static bool parseObjectPosition(const std::string& value,
                                Vec2& position,
                                Vec2& offset) {
    std::istringstream ss(value);
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) tokens.push_back(lowerAscii(token));
    if (tokens.empty() || tokens.size() > 4) return false;

    ObjectPositionAxis x;
    ObjectPositionAxis y;
    bool sawValidToken = false;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& current = tokens[i];
        bool horizontalEnd = current == "right";
        bool verticalEnd = current == "bottom";
        bool isEdge = current == "left" || current == "right" ||
                      current == "top" || current == "bottom";
        if (isEdge) {
            setObjectPositionKeyword(current, x, y);
            sawValidToken = true;
            if (i + 1 < tokens.size() && isObjectPositionLength(tokens[i + 1])) {
                if (current == "left" || current == "right") {
                    setObjectPositionLength(tokens[++i], x, horizontalEnd);
                } else {
                    setObjectPositionLength(tokens[++i], y, verticalEnd);
                }
                sawValidToken = true;
            }
            continue;
        }
        if (current == "center") {
            setObjectPositionKeyword(current, x, y);
            sawValidToken = true;
            continue;
        }
        if (isObjectPositionLength(current)) {
            if (!x.set) {
                setObjectPositionLength(current, x, false);
            } else {
                setObjectPositionLength(current, y, false);
            }
            sawValidToken = true;
            continue;
        }
        return false;
    }

    if (!sawValidToken) return false;
    position = {x.set ? x.fraction : 0.5f, y.set ? y.fraction : 0.5f};
    offset = {x.set ? x.offset : 0.0f, y.set ? y.offset : 0.0f};
    return true;
}

void StyleSheet::mergeProperty(Style& style, const std::string& name, const std::string& value) {
    if (name.rfind("--", 0) == 0) {
        style.customProperties[name] = value;
    } else if (name == "color") {
        style.color = parseColor(value);
        style.hasColor = true;
    } else if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.backgroundGradient = parseGradient(value);
        } else {
            style.backgroundColor = parseColor(value);
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.backgroundGradient = parseGradient(value);
        }
    } else if (name == "border-radius") {
        style.borderRadius = parseBorderRadius(value);
    } else if (name == "border") {
        style.border = parseBorder(value);
    } else if (name == "border-top" || name == "border-block-start") {
        style.borderTop = parseBorder(value);
        style.hasBorderTop = true;
    } else if (name == "border-right" || name == "border-inline-end") {
        style.borderRight = parseBorder(value);
        style.hasBorderRight = true;
    } else if (name == "border-bottom" || name == "border-block-end") {
        style.borderBottom = parseBorder(value);
        style.hasBorderBottom = true;
    } else if (name == "border-left" || name == "border-inline-start") {
        style.borderLeft = parseBorder(value);
        style.hasBorderLeft = true;
    } else if (name == "border-color") {
        style.border.color = parseColor(value);
    } else if (name == "border-width") {
        style.border.width = parseLengthPixels(value);
    } else if (name == "outline") {
        style.outline = parseBorder(value);
    } else if (name == "outline-color") {
        style.outline.color = parseColor(value);
    } else if (name == "outline-width") {
        style.outline.width = parseLengthPixels(value);
    } else if (name == "outline-offset") {
        style.outlineOffset = parseLengthPixels(value);
    } else if (name == "padding") {
        style.padding = parseEdgeInsets(value);
    } else if (name == "padding-top") {
        style.padding.top = parseLengthPixels(value);
    } else if (name == "padding-right") {
        style.padding.right = parseLengthPixels(value);
    } else if (name == "padding-bottom") {
        style.padding.bottom = parseLengthPixels(value);
    } else if (name == "padding-left") {
        style.padding.left = parseLengthPixels(value);
    } else if (name == "padding-block") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.padding.top = parseLengthPixels(first);
        style.padding.bottom = second.empty() ? style.padding.top : parseLengthPixels(second);
    } else if (name == "padding-inline") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.padding.left = parseLengthPixels(first);
        style.padding.right = second.empty() ? style.padding.left : parseLengthPixels(second);
    } else if (name == "padding-block-start") {
        style.padding.top = parseLengthPixels(value);
    } else if (name == "padding-block-end") {
        style.padding.bottom = parseLengthPixels(value);
    } else if (name == "padding-inline-start") {
        style.padding.left = parseLengthPixels(value);
    } else if (name == "padding-inline-end") {
        style.padding.right = parseLengthPixels(value);
    } else if (name == "margin") {
        style.margin = parseEdgeInsets(value);
    } else if (name == "margin-top") {
        style.margin.top = parseLengthPixels(value);
    } else if (name == "margin-right") {
        style.margin.right = parseLengthPixels(value);
    } else if (name == "margin-bottom") {
        style.margin.bottom = parseLengthPixels(value);
    } else if (name == "margin-left") {
        style.margin.left = parseLengthPixels(value);
    } else if (name == "margin-block") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.margin.top = parseLengthPixels(first);
        style.margin.bottom = second.empty() ? style.margin.top : parseLengthPixels(second);
    } else if (name == "margin-inline") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.margin.left = parseLengthPixels(first);
        style.margin.right = second.empty() ? style.margin.left : parseLengthPixels(second);
    } else if (name == "margin-block-start") {
        style.margin.top = parseLengthPixels(value);
    } else if (name == "margin-block-end") {
        style.margin.bottom = parseLengthPixels(value);
    } else if (name == "margin-inline-start") {
        style.margin.left = parseLengthPixels(value);
    } else if (name == "margin-inline-end") {
        style.margin.right = parseLengthPixels(value);
    } else if (name == "inset") {
        EdgeInsets inset = parseEdgeInsets(value);
        style.top = CSSValue::px(inset.top);
        style.right = CSSValue::px(inset.right);
        style.bottom = CSSValue::px(inset.bottom);
        style.left = CSSValue::px(inset.left);
    } else if (name == "inset-block") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.top = parseCSSValue(first);
        style.bottom = second.empty() ? style.top : parseCSSValue(second);
    } else if (name == "inset-inline") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.left = parseCSSValue(first);
        style.right = second.empty() ? style.left : parseCSSValue(second);
    } else if (name == "inset-block-start") {
        style.top = parseCSSValue(value);
    } else if (name == "inset-block-end") {
        style.bottom = parseCSSValue(value);
    } else if (name == "inset-inline-start") {
        style.left = parseCSSValue(value);
    } else if (name == "inset-inline-end") {
        style.right = parseCSSValue(value);
    } else if (name == "width" || name == "inline-size") {
        style.width = parseCSSValue(value);
    } else if (name == "height" || name == "block-size") {
        style.height = parseCSSValue(value);
    } else if (name == "min-width" || name == "min-inline-size") {
        style.minWidth = parseCSSValue(value);
    } else if (name == "min-height" || name == "min-block-size") {
        style.minHeight = parseCSSValue(value);
    } else if (name == "max-width" || name == "max-inline-size") {
        style.maxWidth = parseCSSValue(value);
    } else if (name == "max-height" || name == "max-block-size") {
        style.maxHeight = parseCSSValue(value);
    } else if (name == "font-size") {
        style.fontSize = parseFontSizePixels(value, style.fontSize);
        style.hasFontSize = true;
    } else if (name == "font") {
        std::string lower = lowerAscii(value);
        if (lower.find("-webkit-small-control") != std::string::npos) {
            style.fontSize = 13.333f;
            style.lineHeight = 1.2f;
            style.fontWeight = FontWeight::Normal;
            style.hasFontSize = true;
            style.hasLineHeight = true;
            style.hasFontWeight = true;
        } else {
            std::istringstream ss(value);
            std::string token;
            while (ss >> token) {
                std::string part = token;
                std::string linePart;
                auto slash = part.find('/');
                if (slash != std::string::npos) {
                    linePart = part.substr(slash + 1);
                    part = part.substr(0, slash);
                }
                std::string lowerPart = lowerAscii(part);
                if (lowerPart == "bold" || parseFloat(lowerPart) >= 600.0f) {
                    style.fontWeight = FontWeight::Bold;
                    style.hasFontWeight = true;
                } else if (lowerPart == "normal") {
                    style.fontWeight = FontWeight::Normal;
                    style.hasFontWeight = true;
                }
                if (lowerPart.find("px") != std::string::npos ||
                    lowerPart.find("em") != std::string::npos ||
                    lowerPart.find("rem") != std::string::npos ||
                    lowerPart.find('%') != std::string::npos ||
                    lowerPart == "xx-small" || lowerPart == "x-small" ||
                    lowerPart == "small" || lowerPart == "medium" ||
                    lowerPart == "large" || lowerPart == "x-large" ||
                    lowerPart == "xx-large" || lowerPart == "xxx-large" ||
                    lowerPart == "smaller" || lowerPart == "larger") {
                    style.fontSize = parseFontSizePixels(lowerPart, style.fontSize);
                    style.hasFontSize = true;
                }
                if (!linePart.empty()) {
                    style.lineHeight = parseLineHeight(linePart, style.fontSize);
                    style.hasLineHeight = true;
                }
            }
        }
    } else if (name == "font-weight") {
        style.fontWeight = (value == "bold" || parseFloat(value) >= 600.0f) ?
            FontWeight::Bold : FontWeight::Normal;
        style.hasFontWeight = true;
    } else if (name == "text-align") {
        if (value == "center") style.textAlign = TextAlign::Center;
        else if (value == "right") style.textAlign = TextAlign::Right;
        else if (value == "justify") style.textAlign = TextAlign::Justify;
        else style.textAlign = TextAlign::Left;
        style.hasTextAlign = true;
    } else if (name == "line-height") {
        style.lineHeight = parseLineHeight(value, style.fontSize);
        style.hasLineHeight = true;
    } else if (name == "opacity") {
        style.opacity = parseFloat(value);
    } else if (name == "display") {
        if (value == "flex") style.display = Display::Flex;
        else if (value == "none") style.display = Display::None;
        else if (value == "inline-block") style.display = Display::InlineBlock;
        else if (value == "inline") style.display = Display::Inline;
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
    } else if (name == "place-items") {
        if (value == "center") {
            style.alignItems = AlignItems::Center;
            style.justifyContent = JustifyContent::Center;
        }
    } else if (name == "gap") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        float row = parseLengthPixels(first);
        float column = second.empty() ? row : parseLengthPixels(second);
        style.gap = row;
        style.rowGap = row;
        style.columnGap = column;
    } else if (name == "row-gap") {
        style.rowGap = parseLengthPixels(value);
    } else if (name == "column-gap") {
        style.columnGap = parseLengthPixels(value);
    } else if (name == "flex") {
        std::string v = trim(value);
        if (v == "none") {
            style.flexGrow = 0.0f;
            style.flexShrink = 0.0f;
            style.flexBasis = CSSValue::autoVal();
        } else if (v == "auto") {
            style.flexGrow = 1.0f;
            style.flexShrink = 1.0f;
            style.flexBasis = CSSValue::autoVal();
        } else {
            std::istringstream ss(v);
            std::vector<std::string> tokens;
            std::string token;
            while (ss >> token) tokens.push_back(token);
            if (!tokens.empty()) style.flexGrow = parseFloat(tokens[0]);
            style.flexShrink = tokens.size() > 1 ? parseFloat(tokens[1]) : 1.0f;
            if (tokens.size() > 2) style.flexBasis = parseCSSValue(tokens[2]);
            else if (tokens.size() == 1) style.flexBasis = CSSValue::pct(0.0f);
        }
    } else if (name == "flex-grow") {
        style.flexGrow = parseFloat(value);
    } else if (name == "flex-shrink") {
        style.flexShrink = parseFloat(value);
    } else if (name == "flex-basis") {
        style.flexBasis = parseCSSValue(value);
    } else if (name == "overflow") {
        std::istringstream ss(value);
        std::string first, second;
        ss >> first >> second;
        style.overflowX = parseOverflowKeyword(first);
        style.overflowY = second.empty() ? style.overflowX : parseOverflowKeyword(second);
        normalizeOverflowAxes(style);
    } else if (name == "overflow-x") {
        style.overflowX = parseOverflowKeyword(value);
        normalizeOverflowAxes(style);
    } else if (name == "overflow-y") {
        style.overflowY = parseOverflowKeyword(value);
        normalizeOverflowAxes(style);
    } else if (name == "box-shadow") {
        style.boxShadow = parseBoxShadow(value);
    } else if (name == "cursor") {
        if (value == "pointer") style.cursor = CursorType::Pointer;
        else if (value == "text") style.cursor = CursorType::Text;
        else if (value == "grab") style.cursor = CursorType::Grab;
        else if (value == "grabbing") style.cursor = CursorType::Grabbing;
        else if (value == "not-allowed") style.cursor = CursorType::NotAllowed;
        else if (value == "crosshair") style.cursor = CursorType::Crosshair;
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
        else if (value == "sticky") style.position = Position::Sticky;
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
        style.hasFontFamily = true;
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
    // Blink CSS properties: box-sizing, visibility, text-overflow, white-space, etc.
    else if (name == "box-sizing") {
        if (value == "border-box") style.boxSizing = BoxSizing::BorderBox;
        else style.boxSizing = BoxSizing::ContentBox;
        style.hasBoxSizing = true;
    } else if (name == "visibility") {
        if (value == "hidden") style.visibility = Visibility::Hidden;
        else if (value == "collapse") style.visibility = Visibility::Collapse;
        else style.visibility = Visibility::Visible;
    } else if (name == "text-overflow") {
        if (value == "ellipsis") style.textOverflow = TextOverflow::Ellipsis;
        else style.textOverflow = TextOverflow::Clip;
        style.hasTextOverflow = true;
    } else if (name == "white-space") {
        if (value == "nowrap") style.whiteSpace = WhiteSpace::NoWrap;
        else if (value == "pre") style.whiteSpace = WhiteSpace::Pre;
        else if (value == "pre-wrap") style.whiteSpace = WhiteSpace::PreWrap;
        else if (value == "pre-line") style.whiteSpace = WhiteSpace::PreLine;
        else style.whiteSpace = WhiteSpace::Normal;
        style.hasWhiteSpace = true;
    } else if (name == "text-decoration" || name == "text-decoration-line") {
        if (value == "underline") style.textDecoration = TextDecoration::Underline;
        else if (value == "line-through") style.textDecoration = TextDecoration::LineThrough;
        else if (value == "overline") style.textDecoration = TextDecoration::Overline;
        else style.textDecoration = TextDecoration::None;
        style.hasTextDecoration = true;
    } else if (name == "text-decoration-color") {
        style.textDecorationColor = parseColor(value);
        style.hasTextDecorationColor = true;
    } else if (name == "text-transform") {
        if (value == "uppercase") style.textTransform = TextTransform::Uppercase;
        else if (value == "lowercase") style.textTransform = TextTransform::Lowercase;
        else if (value == "capitalize") style.textTransform = TextTransform::Capitalize;
        else style.textTransform = TextTransform::None;
        style.hasTextTransform = true;
    } else if (name == "letter-spacing") {
        style.letterSpacing = parseLengthPixels(value);
        style.hasLetterSpacing = true;
    } else if (name == "word-spacing") {
        style.wordSpacing = parseLengthPixels(value);
        style.hasWordSpacing = true;
    } else if (name == "pointer-events") {
        if (value == "none") style.pointerEvents = PointerEvents::None;
        else style.pointerEvents = PointerEvents::Auto;
    } else if (name == "flex-wrap") {
        if (value == "wrap") style.flexWrap = FlexWrap::Wrap;
        else if (value == "wrap-reverse") style.flexWrap = FlexWrap::WrapReverse;
        else style.flexWrap = FlexWrap::NoWrap;
    } else if (name == "align-self") {
        if (value == "flex-start") style.alignSelf = AlignSelf::FlexStart;
        else if (value == "flex-end") style.alignSelf = AlignSelf::FlexEnd;
        else if (value == "center") style.alignSelf = AlignSelf::Center;
        else if (value == "stretch") style.alignSelf = AlignSelf::Stretch;
        else if (value == "baseline") style.alignSelf = AlignSelf::Baseline;
        else style.alignSelf = AlignSelf::Auto;
    } else if (name == "align-content") {
        if (value == "flex-start") style.alignContent = AlignContent::FlexStart;
        else if (value == "flex-end") style.alignContent = AlignContent::FlexEnd;
        else if (value == "center") style.alignContent = AlignContent::Center;
        else if (value == "space-between") style.alignContent = AlignContent::SpaceBetween;
        else if (value == "space-around") style.alignContent = AlignContent::SpaceAround;
        else if (value == "space-evenly") style.alignContent = AlignContent::SpaceEvenly;
        else style.alignContent = AlignContent::Stretch;
    } else if (name == "order") {
        style.order = (int)parseFloat(value);
    } else if (name == "z-index") {
        style.zIndex = (int)parseFloat(value);
        style.hasZIndex = true;
    } else if (name == "aspect-ratio") {
        if (value == "auto") {
            style.aspectRatio = 0;
        } else {
            auto slashPos = value.find('/');
            if (slashPos != std::string::npos) {
                float w = parseFloat(trim(value.substr(0, slashPos)));
                float h = parseFloat(trim(value.substr(slashPos + 1)));
                style.aspectRatio = h > 0 ? w / h : 0;
            } else {
                style.aspectRatio = parseFloat(value);
            }
        }
    } else if (name == "object-fit") {
        std::string lower = value;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower == "contain") style.objectFit = ObjectFit::Contain;
        else if (lower == "cover") style.objectFit = ObjectFit::Cover;
        else if (lower == "none") style.objectFit = ObjectFit::None;
        else if (lower == "scale-down") style.objectFit = ObjectFit::ScaleDown;
        else style.objectFit = ObjectFit::Fill;
        style.hasObjectFit = true;
    } else if (name == "object-position") {
        Vec2 position;
        Vec2 offset;
        if (parseObjectPosition(value, position, offset)) {
            style.objectPosition = position;
            style.objectPositionOffset = offset;
            style.hasObjectPosition = true;
        }
    } else if (name == "row-gap") {
        style.rowGap = parseLengthPixels(value);
    } else if (name == "column-gap") {
        style.columnGap = parseLengthPixels(value);
    } else if (name == "word-break") {
        if (value == "break-all") style.wordBreak = WordBreak::BreakAll;
        else if (value == "keep-all") style.wordBreak = WordBreak::KeepAll;
        else if (value == "break-word") style.wordBreak = WordBreak::BreakWord;
        else style.wordBreak = WordBreak::Normal;
        style.hasWordBreak = true;
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
    std::string lower = lowerAscii(v);

    // Hex colors
    if (v[0] == '#') return Color::fromHex(v);

    // All 148 CSS named colors (matching Blink css_value_keywords.json5)
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

    // currentcolor keyword (Blink supports this)
    if (lower == "currentcolor") return Color(1, 1, 1, 1); // placeholder, resolved later

    // rgb()/rgba() with comma or modern space-slash syntax.
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

    // hsl()/hsla() with comma or modern space-slash syntax.
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

CSSValue StyleSheet::parseCSSValue(const std::string& val) {
    std::string v = trim(val);
    std::string lower = lowerAscii(v);
    if (lower == "auto") return CSSValue::autoVal();
    if (v.empty()) return CSSValue();

    // Intrinsic sizing keywords (Blink)
    if (lower == "min-content") return CSSValue::minContent();
    if (lower == "max-content") return CSSValue::maxContent();
    if (lower == "fit-content") return CSSValue::fitContent();

    // calc() support - simplified but covers calc(100% - 200px) pattern
    if (lower.rfind("calc(", 0) == 0) {
        std::string inner = functionInner(v);
        inner = trim(inner);
        // Find + or - operator (skip the first char to allow negative numbers)
        size_t opPos = std::string::npos;
        CSSValue::CalcOp op = CSSValue::CalcNone;
        int depth = 0;
        for (size_t i = 1; i < inner.size(); ++i) {
            if (inner[i] == '(') depth++;
            else if (inner[i] == ')') depth--;
            if (depth == 0 && (inner[i] == '+' || inner[i] == '-') &&
                i > 0 && inner[i-1] == ' ') {
                opPos = i;
                op = (inner[i] == '+') ? CSSValue::CalcAdd : CSSValue::CalcSub;
                break;
            }
            if (depth == 0 && inner[i] == '*') {
                opPos = i;
                op = CSSValue::CalcMul;
                break;
            }
            if (depth == 0 && inner[i] == '/') {
                opPos = i;
                op = CSSValue::CalcDiv;
                break;
            }
        }
        if (opPos != std::string::npos && op != CSSValue::CalcNone) {
            CSSValue left = parseCSSValue(trim(inner.substr(0, opPos)));
            CSSValue right = parseCSSValue(trim(inner.substr(opPos + 1)));
            CSSValue result;
            result.value = left.value;
            result.unit = left.unit;
            result.calcOp = op;
            result.calcValue2 = right.value;
            result.calcUnit2 = right.unit;
            return result;
        }
        return parseCSSValue(inner);
    }

    // min() / max() / clamp() - evaluate with fixed viewport for now
    if (lower.rfind("min(", 0) == 0) {
        auto parts = splitTopLevel(functionInner(v), ',');
        if (parts.size() >= 2) {
            CSSValue a = parseCSSValue(trim(parts[0]));
            CSSValue b = parseCSSValue(trim(parts[1]));
            float va = a.resolve(0), vb = b.resolve(0);
            return va < vb ? a : b;
        }
    }
    if (lower.rfind("max(", 0) == 0) {
        auto parts = splitTopLevel(functionInner(v), ',');
        if (parts.size() >= 2) {
            CSSValue a = parseCSSValue(trim(parts[0]));
            CSSValue b = parseCSSValue(trim(parts[1]));
            float va = a.resolve(0), vb = b.resolve(0);
            return va > vb ? a : b;
        }
    }
    if (lower.rfind("clamp(", 0) == 0) {
        auto parts = splitTopLevel(functionInner(v), ',');
        if (parts.size() >= 3) {
            CSSValue lo = parseCSSValue(trim(parts[0]));
            CSSValue val2 = parseCSSValue(trim(parts[1]));
            CSSValue hi = parseCSSValue(trim(parts[2]));
            // Return the preferred value - clamping needs runtime resolution
            return val2;
        }
    }

    if (v.back() == '%') {
        return CSSValue::pct(std::stof(v.substr(0, v.size() - 1)));
    }

    // vw / vh units (Blink viewport units)
    if (lower.size() > 2 && lower.substr(lower.size() - 2) == "vw") {
        return CSSValue::vw(parseFloat(lower));
    }
    if (lower.size() > 2 && lower.substr(lower.size() - 2) == "vh") {
        return CSSValue::vh(parseFloat(lower));
    }

    if (lower.size() > 3 && lower.substr(lower.size() - 3) == "rem") {
        return CSSValue::rem(parseFloat(lower));
    }
    if (lower.size() > 2 && lower.substr(lower.size() - 2) == "em") {
        return CSSValue::em(parseFloat(lower));
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

float StyleSheet::parseLengthPixels(const std::string& val, float emBase) {
    std::string v = trim(val);
    std::string lower = lowerAscii(v);
    if (lower.empty() || lower == "auto") return 0.0f;

    if (lower.find("__qem") != std::string::npos) {
        return parseFloat(lower) * emBase;
    }
    if (lower.size() > 3 && lower.substr(lower.size() - 3) == "rem") {
        return parseFloat(lower) * 16.0f;
    }
    if (lower.size() > 2 && lower.substr(lower.size() - 2) == "em") {
        return parseFloat(lower) * emBase;
    }
    if (lower.back() == '%') {
        return parseFloat(lower) * emBase / 100.0f;
    }
    return parseFloat(lower);
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

EdgeInsets StyleSheet::parseEdgeInsets(const std::string& val) {
    std::istringstream ss(val);
    std::vector<float> values;
    std::string token;
    while (ss >> token) {
        values.push_back(parseLengthPixels(token));
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
            token == "yellow" || token == "cyan" || token == "magenta" ||
            token == "gray" || token == "grey")) {
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
