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
            combinators.push_back(pendingCombinator == '>' ? '>' : ' ');
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
        if (depth == 0 && c == '>') {
            pushCurrent();
            pendingCombinator = '>';
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
    applyUserAgentDefaults(style, type);
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

static bool isInheritedCSSProperty(const std::string& name) {
    return name == "color" || name == "font-size" ||
           name == "font-weight" || name == "font-family" ||
           name == "line-height" || name == "text-align";
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
               name == "border-left" || name == "border-color" ||
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
               name == "padding-left") {
        target.padding = source.padding;
    } else if (name == "margin" || name == "margin-top" ||
               name == "margin-right" || name == "margin-bottom" ||
               name == "margin-left") {
        target.margin = source.margin;
    } else if (name == "width") {
        target.width = source.width;
    } else if (name == "height") {
        target.height = source.height;
    } else if (name == "min-width") {
        target.minWidth = source.minWidth;
    } else if (name == "min-height") {
        target.minHeight = source.minHeight;
    } else if (name == "max-width") {
        target.maxWidth = source.maxWidth;
    } else if (name == "max-height") {
        target.maxHeight = source.maxHeight;
    } else if (name == "inset" || name == "top" ||
               name == "right" || name == "bottom" || name == "left") {
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
    } else if (name == "justify-content") {
        target.justifyContent = source.justifyContent;
    } else if (name == "align-items") {
        target.alignItems = source.alignItems;
    } else if (name == "gap" || name == "row-gap" || name == "column-gap") {
        target.gap = source.gap;
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
    } else if (name == "overflow" || name == "overflow-x" || name == "overflow-y") {
        target.overflow = source.overflow;
    } else if (name == "cursor") {
        target.cursor = source.cursor;
    } else if (name == "transition") {
        target.transitionDuration = source.transitionDuration;
    } else if (name == "scale" || name == "transform") {
        target.scale = source.scale;
    } else {
        return false;
    }
    return true;
}

void StyleSheet::applyUserAgentDefaults(Style& style, const std::string& type) {
    std::string t = lowerAscii(type);
    if (t == "h1") {
        style.display = Display::Block;
        style.fontSize = 32.0f;
        style.fontWeight = FontWeight::Bold;
        style.lineHeight = 1.16f;
        style.hasFontSize = true;
        style.hasFontWeight = true;
        style.hasLineHeight = true;
    } else if (t == "h2") {
        style.display = Display::Block;
        style.fontSize = 24.0f;
        style.fontWeight = FontWeight::Bold;
        style.lineHeight = 1.20f;
        style.hasFontSize = true;
        style.hasFontWeight = true;
        style.hasLineHeight = true;
    } else if (t == "h3") {
        style.display = Display::Block;
        style.fontSize = 18.0f;
        style.fontWeight = FontWeight::Bold;
        style.lineHeight = 1.24f;
        style.hasFontSize = true;
        style.hasFontWeight = true;
        style.hasLineHeight = true;
    } else if (t == "p") {
        style.display = Display::Block;
        style.lineHeight = 1.45f;
        style.hasLineHeight = true;
    } else if (t == "span") {
        style.display = Display::InlineBlock;
    } else if (t == "button") {
        style.cursor = CursorType::Pointer;
        style.display = Display::Flex;
    } else if (t == "input") {
        style.cursor = CursorType::Text;
    }
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
    } else if (name == "border" || name == "border-top" ||
               name == "border-right" || name == "border-bottom" ||
               name == "border-left") {
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
    } else if (name == "inset") {
        EdgeInsets inset = parseEdgeInsets(value);
        style.top = CSSValue::px(inset.top);
        style.right = CSSValue::px(inset.right);
        style.bottom = CSSValue::px(inset.bottom);
        style.left = CSSValue::px(inset.left);
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
        style.hasFontSize = true;
    } else if (name == "font-weight") {
        style.fontWeight = (value == "bold" || parseFloat(value) >= 600.0f) ?
            FontWeight::Bold : FontWeight::Normal;
        style.hasFontWeight = true;
    } else if (name == "text-align") {
        if (value == "center") style.textAlign = TextAlign::Center;
        else if (value == "right") style.textAlign = TextAlign::Right;
        else style.textAlign = TextAlign::Left;
        style.hasTextAlign = true;
    } else if (name == "line-height") {
        style.lineHeight = parseFloat(value);
        style.hasLineHeight = true;
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
    } else if (name == "place-items") {
        if (value == "center") {
            style.alignItems = AlignItems::Center;
            style.justifyContent = JustifyContent::Center;
        }
    } else if (name == "gap" || name == "row-gap" || name == "column-gap") {
        style.gap = parseFloat(value);
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
    } else if (name == "overflow" || name == "overflow-x" || name == "overflow-y") {
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

    // Named colors
    if (lower == "transparent") return Color(0, 0, 0, 0);
    if (lower == "white") return Color(1, 1, 1, 1);
    if (lower == "black") return Color(0, 0, 0, 1);
    if (lower == "red") return Color(1, 0, 0, 1);
    if (lower == "green") return Color(0, 0.5f, 0, 1);
    if (lower == "blue") return Color(0, 0, 1, 1);
    if (lower == "yellow") return Color(1, 1, 0, 1);
    if (lower == "cyan") return Color(0, 1, 1, 1);
    if (lower == "magenta") return Color(1, 0, 1, 1);
    if (lower == "gray" || lower == "grey") return Color(0.5f, 0.5f, 0.5f, 1);

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

    if (v.back() == '%') {
        return CSSValue::pct(std::stof(v.substr(0, v.size() - 1)));
    }

    if (lower.size() > 3 && lower.substr(lower.size() - 3) == "rem") {
        return CSSValue::px(parseFloat(lower) * 16.0f);
    }
    if (lower.size() > 2 && lower.substr(lower.size() - 2) == "em") {
        return CSSValue::px(parseFloat(lower) * 16.0f);
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
