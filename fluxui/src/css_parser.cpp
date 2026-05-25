// FluxUI CSS Parser Implementation
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace FluxUI {

static bool supportsConditionMatches(std::string_view cond);

StyleSheet::StyleSheet() {
#if FLUXUI_FAST_STARTUP
    rules.reserve(FLUXUI_STYLE_CACHE_SIZE / 4);
    variables_.reserve(64);
    resolvedCache_.resize(FLUXUI_STYLE_CACHE_SIZE);
    idRuleIndex_.reserve(64);
    classRuleIndex_.reserve(256);
    typeRuleIndex_.reserve(64);
    universalRuleIndex_.reserve(64);
#endif
}

std::string StyleSheet::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::vector<StyleSheet::CSSToken> StyleSheet::tokenizeCSS(const std::string& css) {
    std::vector<CSSToken> tokens;
    size_t i = 0;
    size_t n = css.size();

    while (i < n) {
        char c = css[i];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
            size_t start = i;
            while (i < n && (css[i] == ' ' || css[i] == '\t' || css[i] == '\n' || css[i] == '\r' || css[i] == '\f')) {
                i++;
            }
            tokens.push_back({CSSToken::Whitespace, " "});
            continue;
        }

        if (c == '/' && i + 1 < n && css[i + 1] == '*') {
            i += 2;
            while (i < n && !(css[i] == '*' && i + 1 < n && css[i + 1] == '/')) {
                i++;
            }
            if (i < n) i += 2;
            continue;
        }

        if (c == '{') { tokens.push_back({CSSToken::LeftBrace, "{"}); i++; continue; }
        if (c == '}') { tokens.push_back({CSSToken::RightBrace, "}"}); i++; continue; }
        if (c == '(') { tokens.push_back({CSSToken::LeftParenthesis, "("}); i++; continue; }
        if (c == ')') { tokens.push_back({CSSToken::RightParenthesis, ")"}); i++; continue; }
        if (c == '[') { tokens.push_back({CSSToken::LeftBracket, "["}); i++; continue; }
        if (c == ']') { tokens.push_back({CSSToken::RightBracket, "]"}); i++; continue; }
        if (c == ':') { tokens.push_back({CSSToken::Colon, ":"}); i++; continue; }
        if (c == ';') { tokens.push_back({CSSToken::Semicolon, ";"}); i++; continue; }
        if (c == ',') { tokens.push_back({CSSToken::Comma, ","}); i++; continue; }

        if (c == '"' || c == '\'') {
            char quoteChar = c;
            std::string strVal;
            strVal += c;
            i++;
            bool escaped = false;
            while (i < n) {
                char nextC = css[i];
                strVal += nextC;
                i++;
                if (escaped) {
                    escaped = false;
                } else if (nextC == '\\') {
                    escaped = true;
                } else if (nextC == quoteChar) {
                    break;
                }
            }
            tokens.push_back({CSSToken::String, strVal});
            continue;
        }

        if (c == '@') {
            std::string keyword = "@";
            i++;
            while (i < n && (std::isalnum(static_cast<unsigned char>(css[i])) || css[i] == '-' || css[i] == '_')) {
                keyword += css[i];
                i++;
            }
            tokens.push_back({CSSToken::AtKeyword, keyword});
            continue;
        }

        std::string text;
        while (i < n &&
               css[i] != '{' && css[i] != '}' &&
               css[i] != '(' && css[i] != ')' &&
               css[i] != '[' && css[i] != ']' &&
               css[i] != ':' && css[i] != ';' &&
               css[i] != ',' && css[i] != '"' && css[i] != '\'' &&
               css[i] != '@' &&
               !(css[i] == '/' && i + 1 < n && css[i + 1] == '*') &&
               !(css[i] == ' ' || css[i] == '\t' || css[i] == '\n' || css[i] == '\r' || css[i] == '\f')) {
            text += css[i];
            i++;
        }
        tokens.push_back({CSSToken::Ident, text});
    }
    tokens.push_back({CSSToken::EndOfFile, ""});
    return tokens;
}

std::vector<std::string> StyleSheet::splitTopLevel(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    auto tokens = tokenizeCSS(value);
    
    std::string current;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;

    for (const auto& t : tokens) {
        if (t.type == CSSToken::EndOfFile) {
            break;
        }

        if (t.type == CSSToken::LeftParenthesis) parenDepth++;
        else if (t.type == CSSToken::RightParenthesis && parenDepth > 0) parenDepth--;
        else if (t.type == CSSToken::LeftBracket) bracketDepth++;
        else if (t.type == CSSToken::RightBracket && bracketDepth > 0) bracketDepth--;
        else if (t.type == CSSToken::LeftBrace) braceDepth++;
        else if (t.type == CSSToken::RightBrace && braceDepth > 0) braceDepth--;

        bool isDelimiter = false;
        if (delimiter == ',' && t.type == CSSToken::Comma) isDelimiter = true;
        else if (delimiter == ';' && t.type == CSSToken::Semicolon) isDelimiter = true;
        else if (t.text.size() == 1 && t.text[0] == delimiter && t.type == CSSToken::Ident) isDelimiter = true;

        if (isDelimiter && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += t.text;
        }
    }

    if (!trim(current).empty()) {
        parts.push_back(trim(current));
    }
    return parts;
}

std::vector<std::string> StyleSheet::splitDeclarations(const std::string& body) {
    std::vector<std::string> declarations;
    auto tokens = tokenizeCSS(body);
    
    std::string currentDecl;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;

    for (const auto& t : tokens) {
        if (t.type == CSSToken::EndOfFile) {
            break;
        }

        if (t.type == CSSToken::LeftParenthesis) parenDepth++;
        else if (t.type == CSSToken::RightParenthesis && parenDepth > 0) parenDepth--;
        else if (t.type == CSSToken::LeftBracket) bracketDepth++;
        else if (t.type == CSSToken::RightBracket && bracketDepth > 0) bracketDepth--;
        else if (t.type == CSSToken::LeftBrace) braceDepth++;
        else if (t.type == CSSToken::RightBrace && braceDepth > 0) braceDepth--;

        if (t.type == CSSToken::Semicolon && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            std::string item = trim(currentDecl);
            if (!item.empty()) {
                declarations.push_back(item);
            }
            currentDecl.clear();
        } else {
            currentDecl += t.text;
        }
    }

    std::string item = trim(currentDecl);
    if (!item.empty()) {
        declarations.push_back(item);
    }
    return declarations;
}

static bool hasClassName(std::string_view className, std::string_view wanted) {
    size_t pos = 0;
    while (pos < className.size()) {
        while (pos < className.size() && std::isspace((unsigned char)className[pos])) pos++;
        if (pos >= className.size()) break;
        size_t start = pos;
        while (pos < className.size() && !std::isspace((unsigned char)className[pos])) pos++;
        if (className.substr(start, pos - start) == wanted) return true;
    }
    return false;
}

static bool equalIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

static std::string trimLocal(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string_view trimLocal(std::string_view s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == std::string_view::npos) ? std::string_view{} : s.substr(start, end - start + 1);
}

static std::string lowerAscii(std::string s) {
    for (char& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

static std::string lowerAscii(std::string_view s) {
    std::string res(s);
    for (char& c : res) {
        c = (char)std::tolower((unsigned char)c);
    }
    return res;
}

static void splitWhitespace(std::string_view val, std::string_view tokens[], int maxTokens, int& count) {
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

static std::string_view selectorBaseType(std::string_view type) {
    size_t sep = type.find('|');
    return sep == std::string_view::npos ? type : type.substr(0, sep);
}

static bool selectorHasFlag(std::string_view type, std::string_view flag) {
    size_t pos = 0;
    while (pos <= type.size()) {
        size_t next = type.find('|', pos);
        std::string_view token = next == std::string_view::npos
            ? type.substr(pos)
            : type.substr(pos, next - pos);
        if (token == flag) return true;
        if (next == std::string_view::npos) break;
        pos = next + 1;
    }
    return false;
}

static std::string_view selectorAttributeValue(std::string_view type, std::string_view name) {
    size_t pos = 0;
    while (pos <= type.size()) {
        size_t next = type.find('|', pos);
        std::string_view token = next == std::string_view::npos
            ? type.substr(pos)
            : type.substr(pos, next - pos);
        size_t eq = token.find('=');
        if (eq != std::string_view::npos && token.substr(0, eq) == name) {
            return token.substr(eq + 1);
        }
        if (next == std::string_view::npos) break;
        pos = next + 1;
    }
    return {};
}

static std::string normalizeAttributeValue(std::string value) {
    value = trimLocal(value);
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return lowerAscii(trimLocal(value));
}

static bool selectorTypeMatches(std::string_view requiredType, std::string_view actualType) {
    if (requiredType.empty()) return true;
    std::string_view actualBase = selectorBaseType(actualType);
    if (equalIgnoreCase(requiredType, actualBase)) return true;

    if (equalIgnoreCase(requiredType, "input")) {
        return equalIgnoreCase(actualBase, "checkbox") ||
               equalIgnoreCase(actualBase, "radio") ||
               equalIgnoreCase(actualBase, "range");
    }
    if (equalIgnoreCase(actualBase, "input")) {
        std::string_view inputType = selectorAttributeValue(actualType, "type");
        return !inputType.empty() && equalIgnoreCase(requiredType, inputType);
    }
    return false;
}

static bool selectorAttributeMatches(const std::string& selector,
                                     std::string_view actualType) {
    std::string body = trimLocal(selector);
    if (body.empty()) return false;
    if (body.size() >= 2 && body.substr(body.size() - 2) == " i") {
        body = trimLocal(body.substr(0, body.size() - 2));
    }

    size_t eq = body.find('=');
    std::string name = lowerAscii(trimLocal(eq == std::string::npos ? body : body.substr(0, eq)));
    if (name.empty()) return false;

    std::string value;
    if (eq != std::string::npos) {
        value = normalizeAttributeValue(body.substr(eq + 1));
    }

    if (name == "type") {
        std::string_view attr = selectorAttributeValue(actualType, "type");
        if (attr.empty() && selectorBaseType(actualType) == "input") attr = "text";
        if (eq == std::string::npos) return !attr.empty();
        return equalIgnoreCase(value, attr);
    }
    if (name == "open") {
        return eq == std::string::npos && selectorHasFlag(actualType, "open");
    }
    if (name == "checked") {
        return eq == std::string::npos && selectorHasFlag(actualType, "checked");
    }
    if (name == "value") {
        return eq == std::string::npos && selectorHasFlag(actualType, "value");
    }
    if (name == "dir") {
        std::string_view attr = selectorAttributeValue(actualType, "dir");
        if (eq == std::string::npos) return !attr.empty();
        return equalIgnoreCase(value, attr);
    }
    return false;
}

static bool selectorPseudoMatches(std::string_view pseudoName,
                                  std::string_view actualType) {
    if (pseudoName == "checked") return selectorHasFlag(actualType, "checked");
    if (pseudoName == "open") return selectorHasFlag(actualType, "open");
    if (pseudoName == "indeterminate") return selectorHasFlag(actualType, "indeterminate");
    if (pseudoName == "enabled") return true;
    if (pseudoName == "disabled") return false;
    if (pseudoName == "read-write" || pseudoName == "read-only") {
        std::string_view baseType = selectorBaseType(actualType);
        std::string_view inputType = selectorAttributeValue(actualType, "type");
        bool editable = baseType == "textarea";
        if (baseType == "input") {
            editable = inputType.empty() || inputType == "text" || inputType == "password" ||
                       inputType == "search" || inputType == "email" || inputType == "url" ||
                       inputType == "tel" || inputType == "number" || inputType == "date" ||
                       inputType == "time" || inputType == "month" || inputType == "week" ||
                       inputType == "datetime-local";
        }
        return pseudoName == "read-write" ? editable : !editable;
    }
    return false;
}

static void splitSelectorChain(const std::string& selector,
                               std::vector<std::string>& parts,
                               std::vector<char>& combinators);

static bool parseNth(std::string_view inner, int& a, int& b) {
    // Trim spaces
    while (!inner.empty() && std::isspace((unsigned char)inner.front())) inner.remove_prefix(1);
    while (!inner.empty() && std::isspace((unsigned char)inner.back())) inner.remove_suffix(1);
    if (inner == "even") {
        a = 2; b = 0;
        return true;
    }
    if (inner == "odd") {
        a = 2; b = 1;
        return true;
    }
    a = 0; b = 0;
    size_t nPos = inner.find('n');
    if (nPos == std::string_view::npos) {
        int val = 0;
        bool neg = false;
        size_t idx = 0;
        if (idx < inner.size() && inner[idx] == '+') idx++;
        else if (idx < inner.size() && inner[idx] == '-') { neg = true; idx++; }
        while (idx < inner.size() && std::isdigit((unsigned char)inner[idx])) {
            val = val * 10 + (inner[idx] - '0');
            idx++;
        }
        b = neg ? -val : val;
        a = 0;
        return true;
    } else {
        std::string_view aPart = inner.substr(0, nPos);
        if (aPart.empty()) {
            a = 1;
        } else if (aPart == "+") {
            a = 1;
        } else if (aPart == "-") {
            a = -1;
        } else {
            int val = 0;
            bool neg = false;
            size_t idx = 0;
            if (idx < aPart.size() && aPart[idx] == '+') idx++;
            else if (idx < aPart.size() && aPart[idx] == '-') { neg = true; idx++; }
            while (idx < aPart.size() && std::isdigit((unsigned char)aPart[idx])) {
                val = val * 10 + (aPart[idx] - '0');
                idx++;
            }
            a = neg ? -val : val;
        }
        std::string_view bPart = inner.substr(nPos + 1);
        while (!bPart.empty() && std::isspace((unsigned char)bPart.front())) bPart.remove_prefix(1);
        if (bPart.empty()) {
            b = 0;
        } else {
            int val = 0;
            bool neg = false;
            size_t idx = 0;
            if (idx < bPart.size() && bPart[idx] == '+') idx++;
            else if (idx < bPart.size() && bPart[idx] == '-') { neg = true; idx++; }
            while (idx < bPart.size() && std::isdigit((unsigned char)bPart[idx])) {
                val = val * 10 + (bPart[idx] - '0');
                idx++;
            }
            b = neg ? -val : val;
        }
        return true;
    }
}

static bool matchNthIndex(int index, int a, int b) {
    if (a == 0) {
        return index == b;
    }
    if (a > 0) {
        return index >= b && (index - b) % a == 0;
    }
    return index <= b && (b - index) % (-a) == 0;
}

static int getSiblingIndex(const Widget* widget, bool ofType, bool fromEnd) {
    if (!widget || !widget->parent) return 1;
    const auto& siblings = widget->parent->children;
    std::string_view targetType = widget->selectorType();
    
    int count = 0;
    int index = 0;
    for (const auto& sibling : siblings) {
        if (!sibling) continue;
        if (ofType && sibling->selectorType() != targetType) continue;
        count++;
        if (sibling.get() == widget) {
            index = count;
        }
    }
    if (fromEnd) {
        return count - index + 1;
    }
    return index;
}

static bool widgetHasDescendantMatching(const Widget* root, const std::string& selector) {
    if (!root) return false;
    struct Traversal {
        static bool search(const Widget* current, const std::string& selector, const Widget* limitRoot) {
            for (const auto& childShared : current->children) {
                const Widget* child = childShared.get();
                if (!child) continue;
                
                std::vector<CSSSelectorNode> childAncestors;
                for (const Widget* p = child->parent; p; p = p->parent) {
                    childAncestors.push_back({p->className, p->id, p->selectorType(), p});
                }
                
                std::vector<std::string> parts;
                std::vector<char> combinators;
                splitSelectorChain(selector, parts, combinators);
                CSSRule rule;
                rule.selector = selector;
                rule.parts = parts;
                rule.combinators = combinators;
                
                if (StyleSheet::selectorMatches(rule, child->className, child->id, child->selectorType(), childAncestors, nullptr, child)) {
                    return true;
                }
                
                if (search(child, selector, limitRoot)) {
                    return true;
                }
            }
            return false;
        }
    };
    return Traversal::search(root, selector, root);
}

static bool matchCompoundSelector(std::string_view compound,
                                  std::string_view className,
                                  std::string_view id,
                                  std::string_view type,
                                  const Widget* widget) {
    std::string_view s = trimLocal(compound);
    if (s.empty() || s == "*") return s == "*";

    bool hasAnySelector = false;

    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '.') {
            size_t start = ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
            std::string_view cls = s.substr(start, i - start);
            if (cls.empty() || !hasClassName(className, cls)) return false;
            hasAnySelector = true;
        } else if (s[i] == '#') {
            size_t start = ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
            std::string_view reqId = s.substr(start, i - start);
            if (reqId != id) return false;
            hasAnySelector = true;
        } else if (s[i] == ':') {
            size_t nameStart = ++i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
            std::string pseudoName = lowerAscii(s.substr(nameStart, i - nameStart));
            if (i >= s.size() || s[i] != '(') {
                if (pseudoName == "first-child") {
                    if (!widget || getSiblingIndex(widget, false, false) != 1) return false;
                } else if (pseudoName == "last-child") {
                    if (!widget || getSiblingIndex(widget, false, true) != 1) return false;
                } else if (pseudoName == "only-child") {
                    if (!widget || getSiblingIndex(widget, false, false) != 1 || getSiblingIndex(widget, false, true) != 1) return false;
                } else if (pseudoName == "first-of-type") {
                    if (!widget || getSiblingIndex(widget, true, false) != 1) return false;
                } else if (pseudoName == "last-of-type") {
                    if (!widget || getSiblingIndex(widget, true, true) != 1) return false;
                } else if (pseudoName == "only-of-type") {
                    if (!widget || getSiblingIndex(widget, true, false) != 1 || getSiblingIndex(widget, true, true) != 1) return false;
                } else if (pseudoName == "empty") {
                    if (!widget) return false;
                    bool hasChild = false;
                    for (const auto& child : widget->children) {
                        if (child) { hasChild = true; break; }
                    }
                    if (hasChild) return false;
                } else {
                    if (!selectorPseudoMatches(pseudoName, type)) return false;
                }
                hasAnySelector = true;
                continue;
            }

            size_t innerStart = ++i;
            int depth = 1;
            while (i < s.size() && depth > 0) {
                if (s[i] == '(') depth++;
                else if (s[i] == ')') depth--;
                if (depth > 0) i++;
            }
            if (i >= s.size()) return false;
            std::string inner = std::string(s.substr(innerStart, i - innerStart));
            ++i;

            if (pseudoName == "has") {
                if (!widget) return false;
                if (!widgetHasDescendantMatching(widget, inner)) return false;
                hasAnySelector = true;
                continue;
            }

            if (pseudoName == "nth-child" || pseudoName == "nth-last-child" ||
                pseudoName == "nth-of-type" || pseudoName == "nth-last-of-type") {
                if (!widget) return false;
                int a = 0, b = 0;
                if (!parseNth(inner, a, b)) return false;
                bool ofType = (pseudoName == "nth-of-type" || pseudoName == "nth-last-of-type");
                bool fromEnd = (pseudoName == "nth-last-child" || pseudoName == "nth-last-of-type");
                int index = getSiblingIndex(widget, ofType, fromEnd);
                if (!matchNthIndex(index, a, b)) return false;
                hasAnySelector = true;
                continue;
            }

            if (pseudoName != "is" && pseudoName != "where" &&
                pseudoName != "-webkit-any" && pseudoName != "not") {
                return false;
            }

            bool matchedAny = false;
            for (const auto& selector : splitSelectorListLocal(inner)) {
                std::vector<std::string> parts;
                std::vector<char> combinators;
                splitSelectorChain(selector, parts, combinators);
                if (parts.size() == 1 &&
                    matchCompoundSelector(parts[0], className, id, type, widget)) {
                    matchedAny = true;
                    break;
                }
            }

            if (pseudoName == "not") {
                if (matchedAny) return false;
            } else if (!matchedAny) {
                return false;
            }
            hasAnySelector = true;
        } else if (s[i] == '[') {
            size_t innerStart = ++i;
            int depth = 1;
            while (i < s.size() && depth > 0) {
                if (s[i] == '[') depth++;
                else if (s[i] == ']') depth--;
                if (depth > 0) i++;
            }
            if (i >= s.size()) return false;
            std::string inner = std::string(s.substr(innerStart, i - innerStart));
            ++i;
            if (!selectorAttributeMatches(inner, type)) return false;
            hasAnySelector = true;
        } else if (s[i] == '+' || s[i] == '~') {
            return false;
        } else {
            size_t start = i;
            while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != ':' &&
                   s[i] != '[' && s[i] != '+' && s[i] != '~') {
                i++;
            }
            std::string_view reqType = s.substr(start, i - start);
            while (!reqType.empty() && std::isspace((unsigned char)reqType.front())) reqType.remove_prefix(1);
            while (!reqType.empty() && std::isspace((unsigned char)reqType.back())) reqType.remove_suffix(1);
            std::string lowerType = lowerAscii(reqType);
            if (!selectorTypeMatches(lowerType, type)) return false;
            hasAnySelector = true;
        }
    }

    return hasAnySelector || s == "*";
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

    size_t nameStart = colon + 1;
    if (nameStart < selector.size() && selector[nameStart] == ':') {
        nameStart++;
    }
    std::string name = lowerAscii(trimLocal(selector.substr(nameStart)));
    if (name == "hover" || name == "focus" ||
        name == "focus-visible" || name == "active" ||
        name == "before" || name == "after") {
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

StyleCacheKey StyleSheet::buildCacheKey(std::string_view className,
                                         std::string_view id,
                                         std::string_view type,
                                         const std::vector<CSSSelectorNode>& ancestors,
                                         const Style* parentStyle) {
    uint64_t h1 = 14695981039346656037ULL;
    uint64_t h2 = 5381ULL;

    auto hashStr = [&](std::string_view sv) {
        for (char c : sv) {
            h1 ^= static_cast<uint64_t>(c);
            h1 *= 1099511628211ULL;
        }
        for (char c : sv) {
            h2 = ((h2 << 5) + h2) + static_cast<uint64_t>(c);
        }
    };

    // Hash ancestors first (root-first)
    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
        const auto& ancestor = *it;
        h1 ^= 0xDDULL; h2 ^= 0xDDULL;
        hashStr(ancestor.className);
        h1 ^= 0xCCULL; h2 ^= 0xCCULL;
        hashStr(ancestor.id);
        h1 ^= 0xBBULL; h2 ^= 0xBBULL;
        hashStr(ancestor.type);
    }

    // Hash target widget last
    hashStr(className);
    h1 ^= 0xFFULL; h2 ^= 0xFFULL;
    hashStr(id);
    h1 ^= 0xEEULL; h2 ^= 0xEEULL;
    hashStr(type);

    if (parentStyle) {
        h1 ^= parentStyle->inheritedHash;
        h2 ^= ~parentStyle->inheritedHash;
    }

    return {h1, h2};
}

bool StyleSheet::selectorMatches(const CSSRule& rule,
                                 std::string_view className,
                                 std::string_view id,
                                 std::string_view type,
                                 const std::vector<CSSSelectorNode>& ancestors,
                                 std::string_view* pseudo,
                                 const Widget* widget) {
    if (pseudo) {
        *pseudo = rule.pseudoState;
    }
    if (rule.parts.empty()) return false;

    int last = (int)rule.parts.size() - 1;
    if (!matchCompoundSelector(rule.parts[(size_t)last], className, id, type, widget)) return false;

    if (widget) {
        const Widget* current = widget;
        for (int i = last - 1; i >= 0; --i) {
            char combinator = rule.combinators[(size_t)i];
            if (combinator == '>') {
                current = current->parent;
                if (!current) return false;
                if (!matchCompoundSelector(rule.parts[(size_t)i],
                                           current->className,
                                           current->id,
                                           current->selectorType(),
                                           current)) {
                    return false;
                }
            } else if (combinator == ' ') {
                bool found = false;
                current = current->parent;
                while (current) {
                    if (matchCompoundSelector(rule.parts[(size_t)i],
                                              current->className,
                                              current->id,
                                              current->selectorType(),
                                              current)) {
                        found = true;
                        break;
                    }
                    current = current->parent;
                }
                if (!found) return false;
            } else if (combinator == '+') {
                if (!current || !current->parent) return false;
                const auto& siblings = current->parent->children;
                const Widget* prev = nullptr;
                for (const auto& child : siblings) {
                    if (child.get() == current) break;
                    if (child) prev = child.get();
                }
                if (!prev) return false;
                current = prev;
                if (!matchCompoundSelector(rule.parts[(size_t)i],
                                           current->className,
                                           current->id,
                                           current->selectorType(),
                                           current)) {
                    return false;
                }
            } else if (combinator == '~') {
                if (!current || !current->parent) return false;
                const auto& siblings = current->parent->children;
                size_t idx = 0;
                bool foundIdx = false;
                for (size_t k = 0; k < siblings.size(); ++k) {
                    if (siblings[k].get() == current) {
                        idx = k;
                        foundIdx = true;
                        break;
                    }
                }
                if (!foundIdx) return false;
                bool found = false;
                for (int k = (int)idx - 1; k >= 0; --k) {
                    const Widget* sibling = siblings[(size_t)k].get();
                    if (sibling && matchCompoundSelector(rule.parts[(size_t)i],
                                                         sibling->className,
                                                         sibling->id,
                                                         sibling->selectorType(),
                                                         sibling)) {
                        current = sibling;
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
        }
    } else {
        size_t ancestorCursor = 0;
        for (int i = last - 1; i >= 0; --i) {
            char combinator = rule.combinators[(size_t)i];
            if (combinator == '>') {
                if (ancestorCursor >= ancestors.size()) return false;
                const auto& ancestor = ancestors[ancestorCursor];
                if (!matchCompoundSelector(rule.parts[(size_t)i],
                                           ancestor.className,
                                           ancestor.id,
                                           ancestor.type,
                                           ancestor.widget)) {
                    return false;
                }
                ancestorCursor++;
                continue;
            } else if (combinator == ' ') {
                bool found = false;
                while (ancestorCursor < ancestors.size()) {
                    const auto& ancestor = ancestors[ancestorCursor];
                    ancestorCursor++;
                    if (matchCompoundSelector(rule.parts[(size_t)i],
                                              ancestor.className,
                                              ancestor.id,
                                              ancestor.type,
                                              ancestor.widget)) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            } else {
                return false;
            }
        }
    }

    return true;
}

void StyleSheet::appendClassTokens(std::string_view className, std::vector<std::string_view>& out) {
    size_t pos = 0;
    while (pos < className.size()) {
        while (pos < className.size() && std::isspace((unsigned char)className[pos])) pos++;
        if (pos >= className.size()) break;
        size_t start = pos;
        while (pos < className.size() && !std::isspace((unsigned char)className[pos])) pos++;
        out.push_back(className.substr(start, pos - start));
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
    currentEpoch_++;
    return true;
}

void StyleSheet::parse(const std::string& css) {
    currentEpoch_++;
    auto tokens = tokenizeCSS(css);
    parseRulesFromTokens(tokens, "");
    buildInvalidationSets();
}

void StyleSheet::registerLayer(const std::string& name) {
    if (name.empty()) return;
    if (std::find(layersOrder.begin(), layersOrder.end(), name) == layersOrder.end()) {
        layersOrder.push_back(name);
    }
}

int StyleSheet::getLayerPriority(const std::string& layerName) const {
    if (layerName.empty()) {
        return (int)layersOrder.size(); // Unlayered has highest priority
    }
    for (size_t i = 0; i < layersOrder.size(); ++i) {
        if (layersOrder[i] == layerName) {
            return (int)i;
        }
    }
    return -1; // Unknown layer
}

void StyleSheet::parseRules(const std::string& css, const std::string& mediaQuery, const std::string& currentLayer) {
    auto tokens = tokenizeCSS(css);
    parseRulesFromTokens(tokens, mediaQuery, currentLayer);
}

void StyleSheet::parseRulesFromTokens(const std::vector<CSSToken>& tokens, const std::string& mediaQuery, const std::string& currentLayer) {
    size_t index = 0;
    size_t n = tokens.size();

    auto consumeBalancedBlock = [](const std::vector<CSSToken>& tokens, size_t& index) {
        std::vector<CSSToken> content;
        if (index >= tokens.size() || tokens[index].type != CSSToken::LeftBrace) {
            return content;
        }
        index++; // consume '{'
        int depth = 1;
        while (index < tokens.size() && depth > 0) {
            const auto& t = tokens[index];
            if (t.type == CSSToken::LeftBrace) depth++;
            else if (t.type == CSSToken::RightBrace) depth--;
            
            if (depth > 0) {
                content.push_back(t);
                index++;
            }
        }
        if (index < tokens.size()) {
            index++; // consume '}'
        }
        return content;
    };

    while (index < n) {
        if (tokens[index].type == CSSToken::Whitespace || tokens[index].type == CSSToken::Semicolon) {
            index++;
            continue;
        }
        if (tokens[index].type == CSSToken::EndOfFile) {
            break;
        }

        std::vector<CSSToken> prelude;
        bool hasBlock = false;
        bool foundEnd = false;
        int parenDepth = 0;
        int bracketDepth = 0;

        while (index < n) {
            const auto& t = tokens[index];
            if (t.type == CSSToken::LeftParenthesis) parenDepth++;
            else if (t.type == CSSToken::RightParenthesis && parenDepth > 0) parenDepth--;
            else if (t.type == CSSToken::LeftBracket) bracketDepth++;
            else if (t.type == CSSToken::RightBracket && bracketDepth > 0) bracketDepth--;

            if (parenDepth == 0 && bracketDepth == 0) {
                if (t.type == CSSToken::LeftBrace) {
                    hasBlock = true;
                    foundEnd = true;
                    break;
                }
                if (t.type == CSSToken::Semicolon) {
                    foundEnd = true;
                    index++; // consume ';'
                    break;
                }
            }
            prelude.push_back(t);
            index++;
        }

        if (!foundEnd) {
            break;
        }

        std::string preludeStr;
        for (const auto& t : prelude) {
            preludeStr += t.text;
        }
        preludeStr = trim(preludeStr);

        if (hasBlock) {
            std::vector<CSSToken> blockContent = consumeBalancedBlock(tokens, index);

            if (!preludeStr.empty() && preludeStr[0] == '@') {
                std::string lowerPrelude = lowerAscii(preludeStr);
                if (lowerPrelude.rfind("@media", 0) == 0) {
                    std::string query = trim(preludeStr.substr(6));
                    std::string combinedQuery = mediaQuery.empty()
                        ? query
                        : mediaQuery + " and " + query;
                    parseRulesFromTokens(blockContent, combinedQuery, currentLayer);
                } else if (lowerPrelude.rfind("@supports", 0) == 0) {
                    std::string condition = trim(preludeStr.substr(9));
                    if (supportsConditionMatches(condition)) {
                        parseRulesFromTokens(blockContent, mediaQuery, currentLayer);
                    }
                } else if (lowerPrelude.rfind("@container", 0) == 0) {
                    parseRulesFromTokens(blockContent, mediaQuery, currentLayer);
                } else if (lowerPrelude.rfind("@font-face", 0) == 0) {
                    std::string bodyStr;
                    for (const auto& t : blockContent) {
                        bodyStr += t.text;
                    }
                    parseFontFace(bodyStr);
                } else if (lowerPrelude.rfind("@property", 0) == 0) {
                    std::string propName = trim(preludeStr.substr(9));
                    std::string bodyStr;
                    for (const auto& t : blockContent) {
                        bodyStr += t.text;
                    }
                    parsePropertyRule(propName, bodyStr);
                } else if (lowerPrelude.rfind("@layer", 0) == 0) {
                    std::string layerName = trim(preludeStr.substr(6));
                    std::string nestedLayerName;
                    if (!layerName.empty()) {
                        nestedLayerName = currentLayer.empty() ? layerName : currentLayer + "." + layerName;
                        registerLayer(nestedLayerName);
                    } else {
                        static int anonCount = 0;
                        nestedLayerName = "::anon_layer_" + std::to_string(++anonCount);
                        registerLayer(nestedLayerName);
                    }
                    parseRulesFromTokens(blockContent, mediaQuery, nestedLayerName);
                }
            } else {
                if (!preludeStr.empty()) {
                    std::string bodyStr;
                    for (const auto& t : blockContent) {
                        bodyStr += t.text;
                    }
                    parseRule(preludeStr, bodyStr, mediaQuery, currentLayer);
                }
            }
        } else {
            if (!preludeStr.empty() && preludeStr[0] == '@') {
                std::string lowerPrelude = lowerAscii(preludeStr);
                if (lowerPrelude.rfind("@layer", 0) == 0) {
                    std::string names = trim(preludeStr.substr(6));
                    size_t start = 0;
                    while (true) {
                        size_t comma = names.find(',', start);
                        std::string name = trim(names.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
                        if (!name.empty()) {
                            std::string nestedName = currentLayer.empty() ? name : currentLayer + "." + name;
                            registerLayer(nestedName);
                        }
                        if (comma == std::string::npos) break;
                        start = comma + 1;
                    }
                }
            }
        }
    }
}

void StyleSheet::parseRule(const std::string& selector, const std::string& body, const std::string& mediaQuery, const std::string& currentLayer) {
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
        rule.layer = currentLayer;
        rule.specificity = selectorSpecificity(cleanSelector);

        rule.selectorWithoutPseudo = cleanSelector;
        extractTrailingStatePseudo(rule.selectorWithoutPseudo, &rule.pseudoState);
        splitSelectorChain(rule.selectorWithoutPseudo, rule.parts, rule.combinators);

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

Style StyleSheet::resolve(std::string_view className,
                          std::string_view id,
                          std::string_view type) const {
    static const std::vector<CSSSelectorNode> noAncestors;
    return resolve(className, id, type, noAncestors);
}

Style StyleSheet::resolve(std::string_view className,
                          std::string_view id,
                          std::string_view type,
                          const std::vector<CSSSelectorNode>& ancestors) const {
    return resolve(className, id, type, ancestors, nullptr);
}

struct CascadedProperty {
    const CSSProperty* property = nullptr;
    int specificity = 0;
    bool important = false;
    int layerPriority = 0;
};

uint64_t StyleSheet::computeInheritedHash(const Style& style) {
    uint64_t hash = 14695981039346656037ULL;

    auto hashBytes = [&hash](const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
    };

    hashBytes(&style.color, sizeof(style.color));
    hashBytes(&style.fontSize, sizeof(style.fontSize));
    hashBytes(&style.fontWeight, sizeof(style.fontWeight));
    hashBytes(&style.fontStyle, sizeof(style.fontStyle));
    hashBytes(&style.textAlign, sizeof(style.textAlign));
    hashBytes(&style.lineHeight, sizeof(style.lineHeight));
    hashBytes(&style.visibility, sizeof(style.visibility));
    hashBytes(&style.cursor, sizeof(style.cursor));
    hashBytes(&style.letterSpacing, sizeof(style.letterSpacing));
    hashBytes(&style.wordSpacing, sizeof(style.wordSpacing));
    hashBytes(&style.textDecoration, sizeof(style.textDecoration));
    hashBytes(&style.textTransform, sizeof(style.textTransform));
    hashBytes(&style.whiteSpace, sizeof(style.whiteSpace));
    hashBytes(&style.textOverflow, sizeof(style.textOverflow));
    hashBytes(&style.wordBreak, sizeof(style.wordBreak));
    hashBytes(&style.pointerEvents, sizeof(style.pointerEvents));
    hashBytes(&style.listStyleType, sizeof(style.listStyleType));

    for (char c : style.fontFamily) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }

    if (!style.customProperties.empty()) {
        uint64_t customHash = 0;
        for (const auto& entry : style.customProperties) {
            uint64_t pairHash = 14695981039346656037ULL;
            for (char c : entry.first) {
                pairHash ^= static_cast<uint64_t>(c);
                pairHash *= 1099511628211ULL;
            }
            pairHash ^= 0x123456789ABCDEF0ULL;
            for (char c : entry.second) {
                pairHash ^= static_cast<uint64_t>(c);
                pairHash *= 1099511628211ULL;
            }
            customHash += pairHash;
        }
        hash ^= customHash;
        hash *= 1099511628211ULL;
    }

    return hash;
}

Style StyleSheet::resolve(std::string_view className,
                          std::string_view id,
                          std::string_view type,
                          const std::vector<CSSSelectorNode>& ancestors,
                          const Style* parentStyle,
                          const Widget* widget,
                          std::string_view targetPseudo) const {
    const auto* inheritedCustomProperties = parentStyle ? &parentStyle->customProperties : nullptr;
    StyleCacheKey key = buildCacheKey(className, id, type, ancestors, parentStyle);

#if FLUXUI_STYLE_CACHE_SIZE > 0
    {
        size_t cacheIdx = (key.h1 ^ key.h2) % FLUXUI_STYLE_CACHE_SIZE;
        if (resolvedCache_[cacheIdx].epoch == currentEpoch_ && resolvedCache_[cacheIdx].key == key) {
            return resolvedCache_[cacheIdx].style;
        }
    }
#endif

    Style style;
    if (parentStyle) {
        style.fontSize = parentStyle->fontSize;
    } else {
        style.fontSize = 16.0f;
    }
    applyUserAgentDefaults(style, type, ancestors);
    for (const auto& entry : propertyDefinitions_) {
        if (!entry.second.initialValue.empty()) {
            style.customProperties[entry.second.name] = entry.second.initialValue;
        }
    }
    if (!variables_.empty()) {
        for (const auto& entry : variables_) {
            style.customProperties[entry.first] = entry.second;
        }
    }
    if (inheritedCustomProperties && !inheritedCustomProperties->empty()) {
        for (const auto& entry : *inheritedCustomProperties) {
            bool shouldInherit = true;
            auto defIt = propertyDefinitions_.find(entry.first);
            if (defIt != propertyDefinitions_.end()) {
                shouldInherit = defIt->second.inherits;
            }
            if (shouldInherit) {
                style.customProperties[entry.first] = entry.second;
            }
        }
    }

    auto lessCascadePriority = [](const CascadedProperty& a, const CascadedProperty& b) {
        if (a.important != b.important) return !a.important && b.important;
        if (a.layerPriority != b.layerPriority) {
            if (a.important) {
                return a.layerPriority > b.layerPriority;
            } else {
                return a.layerPriority < b.layerPriority;
            }
        }
        if (a.specificity != b.specificity) return a.specificity < b.specificity;
        return a.property->sourceOrder < b.property->sourceOrder;
    };

    thread_local std::vector<CascadedProperty> baseProperties;
    thread_local std::vector<CascadedProperty> hoverProperties;
    thread_local std::vector<CascadedProperty> focusProperties;
    thread_local std::vector<CascadedProperty> activeProperties;
    thread_local std::vector<size_t> candidateRules;

    baseProperties.clear();
    hoverProperties.clear();
    focusProperties.clear();
    activeProperties.clear();
    candidateRules.clear();

    collectCandidateRules(className, id, type, candidateRules);

    for (size_t ruleIndex : candidateRules) {
        if (ruleIndex >= rules.size()) continue;
        const auto& rule = rules[ruleIndex];
        if (!mediaQueryMatches(rule.mediaQuery)) continue;
        std::string_view pseudo;
        if (selectorMatches(rule, className, id, type, ancestors, &pseudo, widget)) {
            if (targetPseudo.empty()) {
                if (!pseudo.empty() && pseudo != "hover" &&
                    pseudo != "focus" && pseudo != "focus-visible" &&
                    pseudo != "active") continue;
            } else {
                if (pseudo != targetPseudo) continue;
            }

            for (const auto& prop : rule.properties) {
                std::string value = prop.value;
                bool isImp = stripImportant(value);
                int layerPri = getLayerPriority(rule.layer);
                CascadedProperty cascaded{&prop, rule.specificity, isImp, layerPri};
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
                                     FastCustomProperties& customProperties) {
        std::sort(properties.begin(), properties.end(), lessCascadePriority);
        for (const auto& item : properties) {
            if (item.property->name.rfind("--", 0) != 0) continue;
            bool valid = true;
            std::string value = resolveValueInternal(item.property->value, customProperties.getMapPointer(), &valid);
            if (!valid) continue;
            stripImportant(value);

            // Syntax validation check
            auto defIt = propertyDefinitions_.find(item.property->name);
            if (defIt != propertyDefinitions_.end()) {
                if (!isValidSyntax(value, defIt->second.syntax)) {
                    value = defIt->second.initialValue;
                }
            }

            customProperties[item.property->name] = value;
        }
    };

    Style initialStyle;
    auto applyProperties = [&](std::vector<CascadedProperty>& properties,
                               auto mergeFn,
                               const FastCustomProperties& customProperties) {
        std::sort(properties.begin(), properties.end(), lessCascadePriority);
        for (const auto& item : properties) {
            if (item.property->name.rfind("--", 0) == 0) continue;
            bool valid = true;
            std::string value = resolveValueInternal(item.property->value, customProperties.getMapPointer(), &valid);
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
        StyleSheet::mergeProperty(target, name, value, target.fontSize);
    }, style.customProperties);

    auto hoverCustomProperties = style.customProperties;
    if (parentStyle && !parentStyle->hoverCustomProperties.empty()) {
        for (const auto& entry : parentStyle->hoverCustomProperties) {
            bool shouldInherit = true;
            auto defIt = propertyDefinitions_.find(entry.first);
            if (defIt != propertyDefinitions_.end()) {
                shouldInherit = defIt->second.inherits;
            }
            if (shouldInherit) {
                hoverCustomProperties[entry.first] = entry.second;
            }
        }
    }
    applyCustomProperties(hoverProperties, hoverCustomProperties);

    auto focusCustomProperties = style.customProperties;
    if (parentStyle && !parentStyle->focusCustomProperties.empty()) {
        for (const auto& entry : parentStyle->focusCustomProperties) {
            bool shouldInherit = true;
            auto defIt = propertyDefinitions_.find(entry.first);
            if (defIt != propertyDefinitions_.end()) {
                shouldInherit = defIt->second.inherits;
            }
            if (shouldInherit) {
                focusCustomProperties[entry.first] = entry.second;
            }
        }
    }
    applyCustomProperties(focusProperties, focusCustomProperties);

    auto activeCustomProperties = style.customProperties;
    if (parentStyle && !parentStyle->activeCustomProperties.empty()) {
        for (const auto& entry : parentStyle->activeCustomProperties) {
            bool shouldInherit = true;
            auto defIt = propertyDefinitions_.find(entry.first);
            if (defIt != propertyDefinitions_.end()) {
                shouldInherit = defIt->second.inherits;
            }
            if (shouldInherit) {
                activeCustomProperties[entry.first] = entry.second;
            }
        }
    }
    applyCustomProperties(activeProperties, activeCustomProperties);

    style.hoverCustomProperties = hoverCustomProperties;
    style.focusCustomProperties = focusCustomProperties;
    style.activeCustomProperties = activeCustomProperties;

    applyProperties(hoverProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeHoverProperty(target, name, value);
    }, hoverCustomProperties);
    applyProperties(focusProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeFocusProperty(target, name, value);
    }, focusCustomProperties);
    applyProperties(activeProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeActiveProperty(target, name, value);
    }, activeCustomProperties);

    style.inheritedHash = computeInheritedHash(style);
#if FLUXUI_STYLE_CACHE_SIZE > 0
    {
        size_t cacheIdx = (key.h1 ^ key.h2) % FLUXUI_STYLE_CACHE_SIZE;
        resolvedCache_[cacheIdx].key = key;
        resolvedCache_[cacheIdx].style = style;
        resolvedCache_[cacheIdx].epoch = currentEpoch_;
    }
#endif
    return style;
}


void StyleSheet::collectCandidateRules(std::string_view className,
                                       std::string_view id,
                                       std::string_view type,
                                       std::vector<size_t>& out) const {
    out.clear();
    out.reserve(universalRuleIndex_.size() + 8);

    auto append = [&out](const std::vector<size_t>& rulesForKey) {
        out.insert(out.end(), rulesForKey.begin(), rulesForKey.end());
    };

    append(universalRuleIndex_);

    if (!id.empty()) {
        thread_local std::string idKey;
        idKey.assign(id.data(), id.size());
        auto it = idRuleIndex_.find(idKey);
        if (it != idRuleIndex_.end()) append(it->second);
    }

    thread_local std::vector<std::string_view> classes;
    classes.clear();
    appendClassTokens(className, classes);
    for (const auto& cls : classes) {
        thread_local std::string clsKey;
        clsKey.assign(cls.data(), cls.size());
        auto it = classRuleIndex_.find(clsKey);
        if (it != classRuleIndex_.end()) append(it->second);
    }

    if (!type.empty()) {
        thread_local std::string typeKey;
        std::string_view baseType = selectorBaseType(type);
        typeKey.resize(baseType.size());
        for (size_t i = 0; i < baseType.size(); ++i) {
            typeKey[i] = (char)std::tolower((unsigned char)baseType[i]);
        }
        auto it = typeRuleIndex_.find(typeKey);
        if (it != typeRuleIndex_.end()) append(it->second);
        std::string_view inputType = selectorAttributeValue(type, "type");
        if (baseType == "input" && !inputType.empty()) {
            typeKey.resize(inputType.size());
            for (size_t i = 0; i < inputType.size(); ++i) {
                typeKey[i] = (char)std::tolower((unsigned char)inputType[i]);
            }
            auto typedIt = typeRuleIndex_.find(typeKey);
            if (typedIt != typeRuleIndex_.end()) append(typedIt->second);
        }
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
           name == "font-weight" || name == "font-style" ||
           name == "font-family" ||
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
    auto hoverCustomProperties = std::move(target.hoverCustomProperties);
    auto focusCustomProperties = std::move(target.focusCustomProperties);
    auto activeCustomProperties = std::move(target.activeCustomProperties);
    auto unresolvedBackgroundColor = std::move(target.unresolvedBackgroundColor);
    auto unresolvedColor = std::move(target.unresolvedColor);
    auto unresolvedBorderColor = std::move(target.unresolvedBorderColor);
    auto unresolvedBackgroundGradient = std::move(target.unresolvedBackgroundGradient);

    target = source;

    target.customProperties = std::move(customProperties);
    target.hoverCustomProperties = std::move(hoverCustomProperties);
    target.focusCustomProperties = std::move(focusCustomProperties);
    target.activeCustomProperties = std::move(activeCustomProperties);
    target.unresolvedBackgroundColor = std::move(unresolvedBackgroundColor);
    target.unresolvedColor = std::move(unresolvedColor);
    target.unresolvedBorderColor = std::move(unresolvedBorderColor);
    target.unresolvedBackgroundGradient = std::move(unresolvedBackgroundGradient);
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
    } else if (name == "font-style") {
        target.fontStyle = source.fontStyle;
        target.hasFontStyle = true;
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
    } else if (name == "appearance" || name == "-webkit-appearance") {
        target.appearance = source.appearance;
        target.hasAppearance = source.hasAppearance;
    } else if (name == "object-position") {
        target.objectPosition = source.objectPosition;
        target.objectPositionOffset = source.objectPositionOffset;
        target.hasObjectPosition = source.hasObjectPosition;
    } else if (name == "word-break") {
        target.wordBreak = source.wordBreak;
    } else if (name == "vertical-align") {
        target.verticalAlign = source.verticalAlign;
        target.hasVerticalAlign = source.hasVerticalAlign;
    } else {
        return false;
    }
    return true;
}

void StyleSheet::applyUserAgentDefaults(Style& style,
                                        std::string_view type,
                                        const std::vector<CSSSelectorNode>& ancestors) {
    std::string t = lowerAscii(std::string(selectorBaseType(type)));
    std::string inputKind = lowerAscii(std::string(selectorAttributeValue(type, "type")));
    constexpr float medium = 16.0f;
    auto isSectioning = [](std::string_view nodeType) {
        return equalIgnoreCase(nodeType, "article") || equalIgnoreCase(nodeType, "aside") ||
               equalIgnoreCase(nodeType, "nav") || equalIgnoreCase(nodeType, "section");
    };
    auto block = [&]() {
        style.display = Display::Block;
    };
    auto inlineBox = [&]() {
        style.display = Display::InlineBlock;
    };
    auto inlineElement = [&]() {
        style.display = Display::Inline;
    };
    auto heading = [&](float size, float marginEm, bool setFontSize = true) {
        block();
        if (setFontSize) {
            style.fontSize = size;
            style.hasFontSize = true;
        }
        style.marginBlockStart = marginEm * medium;
        style.marginBlockEnd = marginEm * medium;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
        style.fontWeight = FontWeight::Bold;
        style.hasFontWeight = true;
    };
    auto smallControl = [&]() {
        style.margin = EdgeInsets(0.0f);
        style.fontSize = 13.333f;
        style.hasFontSize = true;
        style.fontWeight = FontWeight::Normal;
        style.hasFontWeight = true;
        style.fontStyle = FontStyle::Normal;
        style.hasFontStyle = true;
        style.lineHeight = 1.2f;
        style.hasLineHeight = true;
        style.letterSpacing = 0.0f;
        style.hasLetterSpacing = true;
        style.wordSpacing = 0.0f;
        style.hasWordSpacing = true;
        style.textTransform = TextTransform::None;
        style.hasTextTransform = true;
        style.textAlign = TextAlign::Left;
        style.hasTextAlign = true;
        style.display = Display::InlineBlock;
    };
    auto pushButtonControl = [&]() {
        smallControl();
        style.appearance = Appearance::PushButton;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.textAlign = TextAlign::Center;
        style.hasTextAlign = true;
        style.padding = EdgeInsets(2.0f, 6.0f, 3.0f, 6.0f);
        style.border = Border(2.0f, Color(0.46f, 0.46f, 0.46f, 1.0f));
        style.borderRadius = BorderRadius(2.0f);
        style.backgroundColor = Color(0.94f, 0.94f, 0.94f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
        style.boxSizing = BoxSizing::BorderBox;
        style.hasBoxSizing = true;
    };

    if (t == "head" || t == "meta" || t == "title" || t == "link" ||
        t == "style" || t == "script" || t == "param" || t == "datalist" ||
        t == "template" || t == "base" || t == "source" || t == "track") {
        style.display = Display::None;
    } else if (t == "h1") {
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
        style.marginBlockStart = medium;
        style.marginBlockEnd = medium;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
    } else if (t == "html") {
        block();
    } else if (t == "body") {
        block();
        style.margin = EdgeInsets(8.0f);
    } else if (t == "blockquote") {
        block();
        style.marginBlockStart = medium;
        style.marginBlockEnd = medium;
        style.marginInlineStart = 40.0f;
        style.marginInlineEnd = 40.0f;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
        style.hasMarginInlineStart = true;
        style.hasMarginInlineEnd = true;
    } else if (t == "figure") {
        block();
        style.marginBlockStart = medium;
        style.marginBlockEnd = medium;
        style.marginInlineStart = 40.0f;
        style.marginInlineEnd = 40.0f;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
        style.hasMarginInlineStart = true;
        style.hasMarginInlineEnd = true;
    } else if (t == "figcaption" || t == "dt" || t == "form" ||
               t == "layer" || t == "hgroup" || t == "search" ||
               t == "frameset" || t == "frame") {
        block();
    } else if (t == "address") {
        block();
        style.fontStyle = FontStyle::Italic;
        style.hasFontStyle = true;
    } else if (t == "dl") {
        block();
        style.marginBlockStart = medium;
        style.marginBlockEnd = medium;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
    } else if (t == "dd") {
        block();
        style.marginInlineStart = 40.0f;
        style.hasMarginInlineStart = true;
    } else if (t == "center") {
        block();
        style.textAlign = TextAlign::Center;
        style.hasTextAlign = true;
    } else if (t == "hr") {
        block();
        style.overflow = Overflow::Hidden;
        style.overflowX = Overflow::Hidden;
        style.overflowY = Overflow::Hidden;
        style.marginBlockStart = 0.5f * medium;
        style.marginBlockEnd = 0.5f * medium;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
        style.border = Border(1.0f, Color(0.5f, 0.5f, 0.5f, 1.0f));
    } else if (t == "ul" || t == "ol" || t == "menu" || t == "dir") {
        block();
        style.marginBlockStart = medium;
        style.marginBlockEnd = medium;
        style.paddingInlineStart = 40.0f;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
        style.hasPaddingInlineStart = true;
        if (t == "ol") {
            style.listStyleType = ListStyleType::Decimal;
        } else {
            int listDepth = 0;
            for (const auto& ancestor : ancestors) {
                std::string aType = lowerAscii(std::string(selectorBaseType(ancestor.type)));
                if (aType == "ul" || aType == "ol") {
                    listDepth++;
                }
            }
            if (listDepth == 0) {
                style.listStyleType = ListStyleType::Disc;
            } else if (listDepth == 1) {
                style.listStyleType = ListStyleType::Circle;
            } else {
                style.listStyleType = ListStyleType::Square;
            }
        }
        style.hasListStyleType = true;
    } else if (t == "li") {
        style.display = Display::ListItem;
    } else if (t == "table") {
        style.display = Display::Table;
        style.border = Border(0.0f, Color(0.5f, 0.5f, 0.5f, 1.0f));
    } else if (t == "thead") {
        style.display = Display::TableHeaderGroup;
        style.verticalAlign = VerticalAlign::Middle;
        style.hasVerticalAlign = true;
    } else if (t == "tbody") {
        style.display = Display::TableRowGroup;
        style.verticalAlign = VerticalAlign::Middle;
        style.hasVerticalAlign = true;
    } else if (t == "tfoot") {
        style.display = Display::TableFooterGroup;
        style.verticalAlign = VerticalAlign::Middle;
        style.hasVerticalAlign = true;
    } else if (t == "tr") {
        style.display = Display::TableRow;
        style.verticalAlign = VerticalAlign::Middle;
        style.hasVerticalAlign = true;
    } else if (t == "td" || t == "th") {
        style.display = Display::TableCell;
        style.verticalAlign = VerticalAlign::Middle;
        style.hasVerticalAlign = true;
        if (t == "th") {
            style.fontWeight = FontWeight::Bold;
            style.hasFontWeight = true;
            style.textAlign = TextAlign::Center;
            style.hasTextAlign = true;
        }
    } else if (t == "caption") {
        style.display = Display::TableCaption;
        style.textAlign = TextAlign::Center;
        style.hasTextAlign = true;
    } else if (t == "col") {
        style.display = Display::TableColumn;
    } else if (t == "colgroup") {
        style.display = Display::TableColumnGroup;
    } else if (t == "strong" || t == "b") {
        inlineBox();
        style.fontWeight = FontWeight::Bold;
        style.hasFontWeight = true;
    } else if (t == "a") {
        inlineBox();
        style.color = Color(0.0f, 0.0f, 0.933f, 1.0f);
        style.hasColor = true;
        style.textDecoration = TextDecoration::Underline;
        style.hasTextDecoration = true;
        style.cursor = CursorType::Pointer;
    } else if (t == "u" || t == "ins") {
        inlineBox();
        style.textDecoration = TextDecoration::Underline;
        style.hasTextDecoration = true;
    } else if (t == "s" || t == "strike" || t == "del") {
        inlineBox();
        style.textDecoration = TextDecoration::LineThrough;
        style.hasTextDecoration = true;
    } else if (t == "mark") {
        inlineBox();
        style.backgroundColor = Color(1.0f, 1.0f, 0.0f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
    } else if (t == "big") {
        inlineBox();
        style.fontSize = 19.2f;
        style.hasFontSize = true;
    } else if (t == "i" || t == "cite" || t == "em" ||
               t == "var" || t == "dfn") {
        inlineBox();
        style.fontStyle = FontStyle::Italic;
        style.hasFontStyle = true;
    } else if (t == "sub") {
        inlineBox();
        style.verticalAlign = VerticalAlign::Sub;
        style.hasVerticalAlign = true;
        style.fontSize = 13.333f;
        style.hasFontSize = true;
    } else if (t == "sup") {
        inlineBox();
        style.verticalAlign = VerticalAlign::Super;
        style.hasVerticalAlign = true;
        style.fontSize = 13.333f;
        style.hasFontSize = true;
    } else if (t == "small") {
        inlineBox();
        style.fontSize = 13.333f;
        style.hasFontSize = true;
    } else if (t == "pre" || t == "xmp" || t == "plaintext" || t == "listing") {
        block();
        style.marginBlockStart = medium;
        style.marginBlockEnd = medium;
        style.hasMarginBlockStart = true;
        style.hasMarginBlockEnd = true;
        style.fontFamily = "monospace";
        style.hasFontFamily = true;
        style.whiteSpace = WhiteSpace::Pre;
        style.hasWhiteSpace = true;
    } else if (t == "code" || t == "kbd" || t == "samp" || t == "tt") {
        inlineBox();
        style.fontFamily = "monospace";
        style.hasFontFamily = true;
    } else if (t == "nobr") {
        inlineBox();
        style.whiteSpace = WhiteSpace::NoWrap;
        style.hasWhiteSpace = true;
    } else if (t == "img" || t == "svg" || t == "picture") {
        inlineBox();
        style.objectFit = ObjectFit::Fill;
        style.hasObjectFit = true;
    } else if (t == "video") {
        inlineBox();
        style.objectFit = ObjectFit::Contain;
        style.hasObjectFit = true;
    } else if (t == "rp" || t == "noframes") {
        style.display = Display::None;
    } else if (t == "div" || t == "article" || t == "aside" || t == "footer" ||
               t == "header" || t == "main" || t == "nav" || t == "section") {
        block();
    } else if (t == "span" || t == "q" || t == "map" || t == "area" ||
               t == "abbr" || t == "acronym" || t == "bdi" || t == "bdo" ||
               t == "data" || t == "time" || t == "output" || t == "rb" ||
               t == "rtc" || t == "ruby" || t == "audio" || t == "embed" ||
               t == "iframe" || t == "object") {
        inlineElement();
        if (t == "iframe") {
            style.border = Border(2.0f, Color(0.46f, 0.46f, 0.46f, 1.0f));
        }
        // Blink UA: bdi, output { unicode-bidi: isolate; }
        if (t == "bdi" || t == "output") {
            style.unicodeBidi = UnicodeBidi::Isolate;
            style.hasUnicodeBidi = true;
        }
        // Blink UA: bdo { unicode-bidi: bidi-override; }
        if (t == "bdo") {
            style.unicodeBidi = UnicodeBidi::BidiOverride;
            style.hasUnicodeBidi = true;
        }
    } else if (t == "slot") {
        style.display = Display::Contents;
    } else if (t == "rt") {
        block();
        style.fontSize = 8.0f;
        style.hasFontSize = true;
    } else if (t == "label") {
        inlineElement();
        style.cursor = CursorType::Default;
    } else if (t == "fieldset") {
        block();
        style.margin = EdgeInsets(0.0f, 2.0f, 0.0f, 2.0f);
        style.padding = EdgeInsets(5.6f, 12.0f, 10.0f, 12.0f);
        style.border = Border(2.0f, Color(0.63f, 0.63f, 0.63f, 1.0f));
        style.minWidth = CSSValue::px(0.0f);
    } else if (t == "legend") {
        block();
        style.padding = EdgeInsets(0.0f, 2.0f, 0.0f, 2.0f);
    } else if (t == "button") {
        pushButtonControl();
        style.appearance = Appearance::Button;
    } else if (t == "input" && inputKind == "hidden") {
        smallControl();
        style.display = Display::None;
        style.appearance = Appearance::Auto;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.padding = EdgeInsets(0.0f);
        style.border = Border(0.0f, Color(0, 0, 0, 0));
        style.backgroundColor = Color(0, 0, 0, 0);
        style.width = CSSValue::px(0.0f);
        style.height = CSSValue::px(0.0f);
    } else if (t == "input" && (inputKind == "button" ||
               inputKind == "submit" || inputKind == "reset")) {
        pushButtonControl();
    } else if (t == "input" && inputKind == "file") {
        smallControl();
        style.appearance = Appearance::Auto;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.padding = EdgeInsets(0.0f);
        style.border = Border(0.0f, Color(0, 0, 0, 0));
        style.backgroundColor = Color(0, 0, 0, 0);
        style.width = CSSValue::px(253.0f);
        style.height = CSSValue::px(21.0f);
    } else if (t == "input" && inputKind == "image") {
        smallControl();
        style.appearance = Appearance::Auto;
        style.hasAppearance = true;
        style.cursor = CursorType::Pointer;
        style.padding = EdgeInsets(0.0f);
        style.border = Border(0.0f, Color(0, 0, 0, 0));
        style.backgroundColor = Color(0, 0, 0, 0);
    } else if (t == "input" && inputKind == "color") {
        smallControl();
        style.appearance = Appearance::SquareButton;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.width = CSSValue::px(44.0f);
        style.height = CSSValue::px(23.0f);
        style.padding = EdgeInsets(1.0f, 2.0f, 1.0f, 2.0f);
        style.border = Border(1.0f, Color(0.663f, 0.663f, 0.663f, 1.0f));
        style.backgroundColor = Color(0.94f, 0.94f, 0.94f, 1.0f);
        style.boxSizing = BoxSizing::BorderBox;
        style.hasBoxSizing = true;
    } else if (t == "input" && inputKind != "checkbox" &&
               inputKind != "radio" && inputKind != "range") {
        smallControl();
        style.appearance = inputKind == "search" ? Appearance::SearchField : Appearance::TextField;
        style.hasAppearance = true;
        style.cursor = CursorType::Text;
        style.padding = EdgeInsets(1.0f, 2.0f, 1.0f, 2.0f);
        style.border = Border(2.0f, Color(0.46f, 0.46f, 0.46f, 1.0f));
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
    } else if (t == "textarea") {
        smallControl();
        style.appearance = Appearance::Auto;
        style.hasAppearance = true;
        style.cursor = CursorType::Text;
        style.overflow = Overflow::Auto;
        style.whiteSpace = WhiteSpace::PreWrap;
        style.hasWhiteSpace = true;
        style.wordBreak = WordBreak::BreakWord;
        style.hasWordBreak = true;
        style.fontFamily = "monospace";
        style.hasFontFamily = true;
        style.padding = EdgeInsets(2.0f);
        style.border = Border(1.0f, Color(0.46f, 0.46f, 0.46f, 1.0f));
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
    } else if (t == "select") {
        smallControl();
        style.appearance = Appearance::Menulist;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.padding = EdgeInsets(1.0f, 22.0f, 1.0f, 4.0f);
        style.border = Border(1.0f, Color(0.0f, 0.0f, 0.0f, 1.0f));
        style.borderRadius = BorderRadius(5.0f);
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
    } else if (t == "option") {
        smallControl();
        style.display = Display::Block;
        style.padding = EdgeInsets(0.0f, 2.0f, 1.0f, 2.0f);
        style.whiteSpace = WhiteSpace::Pre;
        style.hasWhiteSpace = true;
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
    } else if (t == "checkbox" || (t == "input" && inputKind == "checkbox")) {
        smallControl();
        style.appearance = Appearance::Checkbox;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.margin = EdgeInsets(3.0f, 4.0f, 3.0f, 4.0f);
        style.padding = EdgeInsets(0.0f);
        style.width = CSSValue::px(13.0f);
        style.height = CSSValue::px(13.0f);
        style.border = Border(1.0f, Color(0.46f, 0.46f, 0.46f, 1.0f));
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    } else if (t == "radio" || (t == "input" && inputKind == "radio")) {
        smallControl();
        style.appearance = Appearance::Radio;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.margin = EdgeInsets(3.0f, 4.0f, 3.0f, 4.0f);
        style.padding = EdgeInsets(0.0f);
        style.width = CSSValue::px(13.0f);
        style.height = CSSValue::px(13.0f);
        style.border = Border(1.0f, Color(0.46f, 0.46f, 0.46f, 1.0f));
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    } else if (t == "range" || (t == "input" && inputKind == "range")) {
        smallControl();
        style.appearance = Appearance::SliderHorizontal;
        style.hasAppearance = true;
        style.cursor = CursorType::Default;
        style.margin = EdgeInsets(2.0f);
        style.padding = EdgeInsets(0.0f);
        style.width = CSSValue::px(129.0f);
        style.height = CSSValue::px(16.0f);
        style.border = Border(0.0f, Color(0, 0, 0, 0));
        style.color = Color(0.565f, 0.565f, 0.565f, 1.0f);
        style.hasColor = true;
    } else if (t == "details") {
        block();
    } else if (t == "summary") {
        block();
        style.padding.left = 20.0f;
        style.cursor = CursorType::Pointer;
    } else if (t == "dialog") {
        style.display = Display::None;
        style.position = Position::Absolute;
        style.backgroundColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        style.color = Color(0.0f, 0.0f, 0.0f, 1.0f);
        style.hasColor = true;
        style.border = Border(1.0f, Color(0.0f, 0.0f, 0.0f, 1.0f));
        style.padding = EdgeInsets(20.0f);
    } else if (t == "meter") {
        inlineBox();
        style.width = CSSValue::px(80.0f);
        style.height = CSSValue::px(16.0f);
        style.backgroundColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
        style.border = Border(1.0f, Color(0.7f, 0.7f, 0.7f, 1.0f));
        style.borderRadius = BorderRadius(4.0f);
    } else if (t == "progress") {
        inlineBox();
        style.width = CSSValue::px(160.0f);
        style.height = CSSValue::px(16.0f);
        style.backgroundColor = Color(0.1f, 0.1f, 0.1f, 0.1f);
        style.border = Border(1.0f, Color(0.3f, 0.3f, 0.3f, 0.3f));
        style.borderRadius = BorderRadius(8.0f);
    } else if (t == "br") {
        style.display = Display::Block;
        style.width = CSSValue::pct(100.0f);
        style.height = CSSValue::px(0.0f);
    }

    // Blink UA: [dir="rtl"] { direction: rtl; unicode-bidi: isolate; }
    std::string_view dirAttr = selectorAttributeValue(type, "dir");
    if (!dirAttr.empty()) {
        if (equalIgnoreCase(dirAttr, "rtl")) {
            style.direction = Direction::Rtl;
            style.hasDirection = true;
            style.unicodeBidi = UnicodeBidi::Isolate;
            style.hasUnicodeBidi = true;
        } else if (equalIgnoreCase(dirAttr, "ltr")) {
            style.direction = Direction::Ltr;
            style.hasDirection = true;
            style.unicodeBidi = UnicodeBidi::Isolate;
            style.hasUnicodeBidi = true;
        }
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

void StyleSheet::mergeProperty(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name == "padding") {
        style.hasPaddingInlineStart = false; style.hasPaddingInlineEnd = false;
        style.hasPaddingBlockStart = false; style.hasPaddingBlockEnd = false;
    } else if (name == "padding-top") {
        style.hasPaddingBlockStart = false; style.hasPaddingInlineStart = false;
    } else if (name == "padding-bottom") {
        style.hasPaddingBlockEnd = false; style.hasPaddingInlineEnd = false;
    } else if (name == "padding-left") {
        style.hasPaddingInlineStart = false; style.hasPaddingBlockStart = false;
    } else if (name == "padding-right") {
        style.hasPaddingInlineEnd = false; style.hasPaddingBlockEnd = false;
    } else if (name == "margin") {
        style.hasMarginInlineStart = false; style.hasMarginInlineEnd = false;
        style.hasMarginBlockStart = false; style.hasMarginBlockEnd = false;
    } else if (name == "margin-top") {
        style.hasMarginBlockStart = false; style.hasMarginInlineStart = false;
    } else if (name == "margin-bottom") {
        style.hasMarginBlockEnd = false; style.hasMarginInlineEnd = false;
    } else if (name == "margin-left") {
        style.hasMarginInlineStart = false; style.hasMarginBlockStart = false;
    } else if (name == "margin-right") {
        style.hasMarginInlineEnd = false; style.hasMarginBlockEnd = false;
    } else if (name == "width") {
        style.hasInlineSize = false; style.hasBlockSize = false;
    } else if (name == "height") {
        style.hasInlineSize = false; style.hasBlockSize = false;
    } else if (name == "min-width") {
        style.hasMinInlineSize = false; style.hasMinBlockSize = false;
    } else if (name == "min-height") {
        style.hasMinInlineSize = false; style.hasMinBlockSize = false;
    } else if (name == "max-width") {
        style.hasMaxInlineSize = false; style.hasMaxBlockSize = false;
    } else if (name == "max-height") {
        style.hasMaxInlineSize = false; style.hasMaxBlockSize = false;
    } else if (name == "border") {
        style.hasBorderInlineStart = false; style.hasBorderInlineEnd = false;
        style.hasBorderBlockStart = false; style.hasBorderBlockEnd = false;
    } else if (name == "border-top") {
        style.hasBorderBlockStart = false; style.hasBorderInlineStart = false;
    } else if (name == "border-bottom") {
        style.hasBorderBlockEnd = false; style.hasBorderInlineEnd = false;
    } else if (name == "border-left") {
        style.hasBorderInlineStart = false; style.hasBorderBlockStart = false;
    } else if (name == "border-right") {
        style.hasBorderInlineEnd = false; style.hasBorderBlockEnd = false;
    } else if (name == "top" || name == "bottom" || name == "left" || name == "right") {
        style.hasInsetInlineStart = false; style.hasInsetInlineEnd = false;
        style.hasInsetBlockStart = false; style.hasInsetBlockEnd = false;
    }

    mergePropertyPart1(style, name, value, emBase);
    mergePropertyPart2(style, name, value, emBase);
}

static bool isDynamicValue(const std::string& val) {
    return val.find("var(") != std::string::npos ||
           val.find("min(") != std::string::npos ||
           val.find("max(") != std::string::npos ||
           val.find("clamp(") != std::string::npos ||
           val.find("calc(") != std::string::npos;
}

bool StyleSheet::mergePropertyPart1(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name.rfind("--", 0) == 0) {
        style.customProperties[name] = value;
    } else if (name == "color") {
        if (isDynamicValue(value)) {
            style.unresolvedColor = value;
        } else {
            style.color = parseColor(value);
            style.hasColor = true;
        }
    } else if (name == "background-color" || name == "background") {
        if (value.find("linear-gradient") != std::string::npos) {
            if (isDynamicValue(value)) {
                style.unresolvedBackgroundGradient = value;
            } else {
                style.backgroundGradient = parseGradient(value);
            }
        } else {
            if (isDynamicValue(value)) {
                style.unresolvedBackgroundColor = value;
            } else {
                style.backgroundColor = parseColor(value);
            }
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            if (isDynamicValue(value)) {
                style.unresolvedBackgroundGradient = value;
            } else {
                style.backgroundGradient = parseGradient(value);
            }
        }
    } else if (name == "border-radius") {
        style.borderRadius = parseBorderRadius(value, emBase);
    } else if (name == "border") {
        if (isDynamicValue(value)) {
            size_t varPos = value.find("var(");
            if (varPos == std::string::npos) varPos = value.find("min(");
            if (varPos == std::string::npos) varPos = value.find("max(");
            if (varPos == std::string::npos) varPos = value.find("clamp(");
            if (varPos != std::string::npos) {
                int depth = 1;
                size_t cursor = varPos + 4;
                while (cursor < value.size() && depth > 0) {
                    if (value[cursor] == '(') depth++;
                    if (value[cursor] == ')') depth--;
                    if (depth > 0) cursor++;
                }
                if (cursor < value.size()) {
                    style.unresolvedBorderColor = value.substr(varPos, cursor - varPos + 1);
                }
            }
        }
        style.border = parseBorder(value, emBase);
    } else if (name == "border-top") {
        style.borderTop = parseBorder(value, emBase);
        style.hasBorderTop = true;
    } else if (name == "border-right") {
        style.borderRight = parseBorder(value, emBase);
        style.hasBorderRight = true;
    } else if (name == "border-bottom") {
        style.borderBottom = parseBorder(value, emBase);
        style.hasBorderBottom = true;
    } else if (name == "border-left") {
        style.borderLeft = parseBorder(value, emBase);
        style.hasBorderLeft = true;
    } else if (name == "border-block-start") {
        style.borderBlockStart = parseBorder(value, emBase);
        style.hasBorderBlockStart = true;
    } else if (name == "border-block-end") {
        style.borderBlockEnd = parseBorder(value, emBase);
        style.hasBorderBlockEnd = true;
    } else if (name == "border-inline-start") {
        style.borderInlineStart = parseBorder(value, emBase);
        style.hasBorderInlineStart = true;
    } else if (name == "border-inline-end") {
        style.borderInlineEnd = parseBorder(value, emBase);
        style.hasBorderInlineEnd = true;
    } else if (name == "border-color") {
        if (isDynamicValue(value)) {
            style.unresolvedBorderColor = value;
        } else {
            style.border.color = parseColor(value);
        }
    } else if (name == "border-width") {
        style.border.width = parseLengthPixels(value, emBase);
    } else if (name == "outline") {
        style.outline = parseBorder(value, emBase);
    } else if (name == "outline-color") {
        style.outline.color = parseColor(value);
    } else if (name == "outline-width") {
        style.outline.width = parseLengthPixels(value, emBase);
    } else if (name == "outline-offset") {
        style.outlineOffset = parseLengthPixels(value, emBase);
    } else if (name == "padding") {
        style.padding = parseEdgeInsets(value, emBase);
    } else if (name == "padding-top") {
        style.padding.top = parseLengthPixels(value, emBase);
    } else if (name == "padding-right") {
        style.padding.right = parseLengthPixels(value, emBase);
    } else if (name == "padding-bottom") {
        style.padding.bottom = parseLengthPixels(value, emBase);
    } else if (name == "padding-left") {
        style.padding.left = parseLengthPixels(value, emBase);
    } else if (name == "padding-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.paddingBlockStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.paddingBlockEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.paddingBlockStart;
        style.hasPaddingBlockStart = count > 0;
        style.hasPaddingBlockEnd = count > 0;
    } else if (name == "padding-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.paddingInlineStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.paddingInlineEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.paddingInlineStart;
        style.hasPaddingInlineStart = count > 0;
        style.hasPaddingInlineEnd = count > 0;
    } else if (name == "padding-block-start") {
        style.paddingBlockStart = parseLengthPixels(value, emBase);
        style.hasPaddingBlockStart = true;
    } else if (name == "padding-block-end") {
        style.paddingBlockEnd = parseLengthPixels(value, emBase);
        style.hasPaddingBlockEnd = true;
    } else if (name == "padding-inline-start") {
        style.paddingInlineStart = parseLengthPixels(value, emBase);
        style.hasPaddingInlineStart = true;
    } else if (name == "padding-inline-end") {
        style.paddingInlineEnd = parseLengthPixels(value, emBase);
        style.hasPaddingInlineEnd = true;
    } else if (name == "margin") {
        style.margin = parseEdgeInsets(value, emBase);
    } else if (name == "margin-top") {
        style.margin.top = parseLengthPixels(value, emBase);
    } else if (name == "margin-right") {
        style.margin.right = parseLengthPixels(value, emBase);
    } else if (name == "margin-bottom") {
        style.margin.bottom = parseLengthPixels(value, emBase);
    } else if (name == "margin-left") {
        style.margin.left = parseLengthPixels(value, emBase);
    } else if (name == "margin-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.marginBlockStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.marginBlockEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.marginBlockStart;
        style.hasMarginBlockStart = count > 0;
        style.hasMarginBlockEnd = count > 0;
    } else if (name == "margin-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.marginInlineStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.marginInlineEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.marginInlineStart;
        style.hasMarginInlineStart = count > 0;
        style.hasMarginInlineEnd = count > 0;
    } else if (name == "margin-block-start") {
        style.marginBlockStart = parseLengthPixels(value, emBase);
        style.hasMarginBlockStart = true;
    } else if (name == "margin-block-end") {
        style.marginBlockEnd = parseLengthPixels(value, emBase);
        style.hasMarginBlockEnd = true;
    } else if (name == "margin-inline-start") {
        style.marginInlineStart = parseLengthPixels(value, emBase);
        style.hasMarginInlineStart = true;
    } else if (name == "margin-inline-end") {
        style.marginInlineEnd = parseLengthPixels(value, emBase);
        style.hasMarginInlineEnd = true;
    } else if (name == "inset") {
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.top = CSSValue::px(inset.top);
        style.right = CSSValue::px(inset.right);
        style.bottom = CSSValue::px(inset.bottom);
        style.left = CSSValue::px(inset.left);
    } else if (name == "inset-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.insetBlockStart = count > 0 ? parseCSSValue(std::string(tokens[0])) : CSSValue();
        style.insetBlockEnd = count > 1 ? parseCSSValue(std::string(tokens[1])) : style.insetBlockStart;
        style.hasInsetBlockStart = count > 0;
        style.hasInsetBlockEnd = count > 0;
    } else if (name == "inset-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.insetInlineStart = count > 0 ? parseCSSValue(std::string(tokens[0])) : CSSValue();
        style.insetInlineEnd = count > 1 ? parseCSSValue(std::string(tokens[1])) : style.insetInlineStart;
        style.hasInsetInlineStart = count > 0;
        style.hasInsetInlineEnd = count > 0;
    } else if (name == "inset-block-start") {
        style.insetBlockStart = parseCSSValue(value);
        style.hasInsetBlockStart = true;
    } else if (name == "inset-block-end") {
        style.insetBlockEnd = parseCSSValue(value);
        style.hasInsetBlockEnd = true;
    } else if (name == "inset-inline-start") {
        style.insetInlineStart = parseCSSValue(value);
        style.hasInsetInlineStart = true;
    } else if (name == "inset-inline-end") {
        style.insetInlineEnd = parseCSSValue(value);
        style.hasInsetInlineEnd = true;
    } else if (name == "width") {
        style.width = parseCSSValue(value);
    } else if (name == "inline-size") {
        style.inlineSize = parseCSSValue(value);
        style.hasInlineSize = true;
    } else if (name == "height") {
        style.height = parseCSSValue(value);
    } else if (name == "block-size") {
        style.blockSize = parseCSSValue(value);
        style.hasBlockSize = true;
    } else if (name == "min-width") {
        style.minWidth = parseCSSValue(value);
    } else if (name == "min-inline-size") {
        style.minInlineSize = parseCSSValue(value);
        style.hasMinInlineSize = true;
    } else if (name == "min-height") {
        style.minHeight = parseCSSValue(value);
    } else if (name == "min-block-size") {
        style.minBlockSize = parseCSSValue(value);
        style.hasMinBlockSize = true;
    } else if (name == "max-width") {
        style.maxWidth = parseCSSValue(value);
    } else if (name == "max-inline-size") {
        style.maxInlineSize = parseCSSValue(value);
        style.hasMaxInlineSize = true;
    } else if (name == "max-height") {
        style.maxHeight = parseCSSValue(value);
    } else if (name == "max-block-size") {
        style.maxBlockSize = parseCSSValue(value);
        style.hasMaxBlockSize = true;
    }
    return false;
}

void StyleSheet::mergePropertyPart2(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name == "font-size") {
        style.fontSize = parseFontSizePixels(value, style.fontSize);
        style.hasFontSize = true;
    } else if (name == "font") {
        std::string lower = lowerAscii(value);
        if (lower.find("-webkit-small-control") != std::string::npos) {
            style.fontSize = 13.333f;
            style.lineHeight = 1.2f;
            style.fontWeight = FontWeight::Normal;
            style.fontStyle = FontStyle::Normal;
            style.hasFontSize = true;
            style.hasLineHeight = true;
            style.hasFontWeight = true;
            style.hasFontStyle = true;
        } else {
            std::string_view tokens[8];
            int count = 0;
            splitWhitespace(value, tokens, 8, count);
            for (int idx = 0; idx < count; idx++) {
                std::string_view part = tokens[idx];
                std::string_view linePart;
                auto slash = part.find('/');
                if (slash != std::string_view::npos) {
                    linePart = part.substr(slash + 1);
                    part = part.substr(0, slash);
                }
                std::string lowerPart = lowerAscii(part);
                if (lowerPart == "bold" || parseFloat(lowerPart) >= 600.0f) {
                    style.fontWeight = FontWeight::Bold;
                    style.hasFontWeight = true;
                } else if (lowerPart == "normal") {
                    style.fontWeight = FontWeight::Normal;
                    style.fontStyle = FontStyle::Normal;
                    style.hasFontWeight = true;
                    style.hasFontStyle = true;
                } else if (lowerPart == "italic") {
                    style.fontStyle = FontStyle::Italic;
                    style.hasFontStyle = true;
                } else if (lowerPart == "oblique") {
                    style.fontStyle = FontStyle::Oblique;
                    style.hasFontStyle = true;
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
                    style.lineHeight = parseLineHeight(std::string(linePart), style.fontSize);
                    style.hasLineHeight = true;
                }
            }
        }
    } else if (name == "font-weight") {
        style.fontWeight = (value == "bold" || parseFloat(value) >= 600.0f) ?
            FontWeight::Bold : FontWeight::Normal;
        style.hasFontWeight = true;
    } else if (name == "font-style") {
        std::string lower = lowerAscii(value);
        if (lower.find("italic") != std::string::npos) {
            style.fontStyle = FontStyle::Italic;
        } else if (lower.find("oblique") != std::string::npos) {
            style.fontStyle = FontStyle::Oblique;
        } else {
            style.fontStyle = FontStyle::Normal;
        }
        style.hasFontStyle = true;
    } else if (name == "text-align") {
        if (value == "center") style.textAlign = TextAlign::Center;
        else if (value == "right") style.textAlign = TextAlign::Right;
        else if (value == "justify") style.textAlign = TextAlign::Justify;
        else style.textAlign = TextAlign::Left;
        style.hasTextAlign = true;
    } else if (name == "line-height") {
        style.lineHeight = parseLineHeight(value, style.fontSize);
        style.hasLineHeight = true;
    } else if (name == "direction") {
        if (value == "rtl") {
            style.direction = Direction::Rtl;
            style.hasDirection = true;
        } else {
            style.direction = Direction::Ltr;
            style.hasDirection = true;
        }
    } else if (name == "unicode-bidi") {
        if (value == "embed") {
            style.unicodeBidi = UnicodeBidi::Embed;
        } else if (value == "bidi-override") {
            style.unicodeBidi = UnicodeBidi::BidiOverride;
        } else if (value == "isolate") {
            style.unicodeBidi = UnicodeBidi::Isolate;
        } else if (value == "isolate-override") {
            style.unicodeBidi = UnicodeBidi::IsolateOverride;
        } else if (value == "plaintext") {
            style.unicodeBidi = UnicodeBidi::Plaintext;
        } else {
            style.unicodeBidi = UnicodeBidi::Normal;
        }
        style.hasUnicodeBidi = true;
    } else if (name == "writing-mode") {
        if (value == "vertical-rl") {
            style.writingMode = WritingMode::VerticalRl;
            style.hasWritingMode = true;
        } else if (value == "vertical-lr") {
            style.writingMode = WritingMode::VerticalLr;
            style.hasWritingMode = true;
        } else {
            style.writingMode = WritingMode::HorizontalTb;
            style.hasWritingMode = true;
        }
    } else if (name == "opacity") {
        style.opacity = parseFloat(value);
    } else if (name == "float") {
        if (value == "left") style.cssFloat = CSSFloat::Left;
        else if (value == "right") style.cssFloat = CSSFloat::Right;
        else style.cssFloat = CSSFloat::None;
    } else if (name == "clear") {
        if (value == "left") style.cssClear = CSSClear::Left;
        else if (value == "right") style.cssClear = CSSClear::Right;
        else if (value == "both") style.cssClear = CSSClear::Both;
        else style.cssClear = CSSClear::None;
    } else if (name == "list-style-type") {
        if (value == "none") style.listStyleType = ListStyleType::None;
        else if (value == "disc") style.listStyleType = ListStyleType::Disc;
        else if (value == "circle") style.listStyleType = ListStyleType::Circle;
        else if (value == "square") style.listStyleType = ListStyleType::Square;
        else if (value == "decimal") style.listStyleType = ListStyleType::Decimal;
        else if (value == "decimal-leading-zero") style.listStyleType = ListStyleType::DecimalLeadingZero;
        else if (value == "lower-roman") style.listStyleType = ListStyleType::LowerRoman;
        else if (value == "upper-roman") style.listStyleType = ListStyleType::UpperRoman;
        else if (value == "lower-alpha" || value == "lower-latin") style.listStyleType = ListStyleType::LowerAlpha;
        else if (value == "upper-alpha" || value == "upper-latin") style.listStyleType = ListStyleType::UpperAlpha;
        else style.listStyleType = ListStyleType::Disc;
        style.hasListStyleType = true;
    } else if (name == "display") {
        if (value == "flex") style.display = Display::Flex;
        else if (value == "grid") style.display = Display::Grid;
        else if (value == "none") style.display = Display::None;
        else if (value == "inline-block") style.display = Display::InlineBlock;
        else if (value == "inline") style.display = Display::Inline;
        else if (value == "list-item") style.display = Display::ListItem;
        else if (value == "table") style.display = Display::Table;
        else if (value == "table-row-group") style.display = Display::TableRowGroup;
        else if (value == "table-header-group") style.display = Display::TableHeaderGroup;
        else if (value == "table-footer-group") style.display = Display::TableFooterGroup;
        else if (value == "table-row") style.display = Display::TableRow;
        else if (value == "table-cell") style.display = Display::TableCell;
        else if (value == "table-column") style.display = Display::TableColumn;
        else if (value == "table-column-group") style.display = Display::TableColumnGroup;
        else if (value == "table-caption") style.display = Display::TableCaption;
        else if (value == "contents") style.display = Display::Contents;
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
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        float row = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        float column = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : row;
        style.gap = row;
        style.rowGap = row;
        style.columnGap = column;
    } else if (name == "row-gap") {
        style.rowGap = parseLengthPixels(value, emBase);
    } else if (name == "column-gap") {
        style.columnGap = parseLengthPixels(value, emBase);
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
            std::string_view tokens[4];
            int count = 0;
            splitWhitespace(v, tokens, 4, count);
            if (count > 0) style.flexGrow = parseFloat(std::string(tokens[0]));
            style.flexShrink = count > 1 ? parseFloat(std::string(tokens[1])) : 1.0f;
            if (count > 2) style.flexBasis = parseCSSValue(std::string(tokens[2]));
            else if (count == 1) style.flexBasis = CSSValue::pct(0.0f);
        }
    } else if (name == "flex-grow") {
        style.flexGrow = parseFloat(value);
    } else if (name == "flex-shrink") {
        style.flexShrink = parseFloat(value);
    } else if (name == "flex-basis") {
        style.flexBasis = parseCSSValue(value);
    } else if (name == "overflow") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.overflowX = count > 0 ? parseOverflowKeyword(std::string(tokens[0])) : Overflow::Visible;
        style.overflowY = count > 1 ? parseOverflowKeyword(std::string(tokens[1])) : style.overflowX;
        normalizeOverflowAxes(style);
    } else if (name == "overflow-x") {
        style.overflowX = parseOverflowKeyword(value);
        normalizeOverflowAxes(style);
    } else if (name == "overflow-y") {
        style.overflowY = parseOverflowKeyword(value);
        normalizeOverflowAxes(style);
    } else if (name == "box-shadow") {
        style.boxShadow = parseBoxShadow(value, emBase);
    } else if (name == "cursor") {
        if (value == "pointer") style.cursor = CursorType::Pointer;
        else if (value == "text") style.cursor = CursorType::Text;
        else if (value == "grab") style.cursor = CursorType::Grab;
        else if (value == "grabbing") style.cursor = CursorType::Grabbing;
        else if (value == "not-allowed") style.cursor = CursorType::NotAllowed;
        else if (value == "crosshair") style.cursor = CursorType::Crosshair;
        else if (value == "se-resize" || value == "nwse-resize") style.cursor = CursorType::ResizeNWSE;
        else style.cursor = CursorType::Default;
    } else if (name == "transition") {
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string_view tokens[4];
            int count = 0;
            splitWhitespace(part, tokens, 4, count);
            for (int idx = 0; idx < count; idx++) {
                std::string_view token = tokens[idx];
                bool isMs = token.size() > 2 && token.substr(token.size() - 2) == "ms";
                bool isSec = token.size() > 1 && token.back() == 's' && !isMs;
                if (isMs || isSec) {
                    style.transitionDuration = parseDuration(std::string(token));
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
    } else if (name == "grid-template-columns") {
        style.gridTemplateColumns = value;
    } else if (name == "grid-template-rows") {
        style.gridTemplateRows = value;
    } else if (name == "grid-column") {
        style.gridColumn = value;
    } else if (name == "grid-row") {
        style.gridRow = value;
    } else if (name == "content") {
        std::string raw = value;
        if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\''))) {
            raw = raw.substr(1, raw.size() - 2);
        }
        style.content = raw;
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
        style.letterSpacing = parseLengthPixels(value, emBase);
        style.hasLetterSpacing = true;
    } else if (name == "word-spacing") {
        style.wordSpacing = parseLengthPixels(value, emBase);
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
    } else if (name == "appearance" || name == "-webkit-appearance") {
        std::string lower = lowerAscii(value);
        if (lower == "none") style.appearance = Appearance::None;
        else if (lower == "textfield" || lower == "text-field") style.appearance = Appearance::TextField;
        else if (lower == "searchfield" || lower == "search-field") style.appearance = Appearance::SearchField;
        else if (lower == "push-button") style.appearance = Appearance::PushButton;
        else if (lower == "button") style.appearance = Appearance::Button;
        else if (lower == "checkbox") style.appearance = Appearance::Checkbox;
        else if (lower == "radio") style.appearance = Appearance::Radio;
        else if (lower == "menulist" || lower == "menulist-button") style.appearance = Appearance::Menulist;
        else if (lower == "textarea") style.appearance = Appearance::Textarea;
        else if (lower == "slider-horizontal") style.appearance = Appearance::SliderHorizontal;
        else if (lower == "square-button") style.appearance = Appearance::SquareButton;
        else style.appearance = Appearance::Auto;
        style.hasAppearance = true;
    } else if (name == "object-position") {
        Vec2 position;
        Vec2 offset;
        if (parseObjectPosition(value, position, offset)) {
            style.objectPosition = position;
            style.objectPositionOffset = offset;
            style.hasObjectPosition = true;
        }
    } else if (name == "row-gap") {
        style.rowGap = parseLengthPixels(value, emBase);
    } else if (name == "column-gap") {
        style.columnGap = parseLengthPixels(value, emBase);
    } else if (name == "word-break") {
        if (value == "break-all") style.wordBreak = WordBreak::BreakAll;
        else if (value == "keep-all") style.wordBreak = WordBreak::KeepAll;
        else if (value == "break-word") style.wordBreak = WordBreak::BreakWord;
        else style.wordBreak = WordBreak::Normal;
        style.hasWordBreak = true;
    } else if (name == "vertical-align") {
        std::string lower = lowerAscii(value);
        if (lower == "sub") style.verticalAlign = VerticalAlign::Sub;
        else if (lower == "super") style.verticalAlign = VerticalAlign::Super;
        else if (lower == "middle") style.verticalAlign = VerticalAlign::Middle;
        else if (lower == "top") style.verticalAlign = VerticalAlign::Top;
        else if (lower == "bottom") style.verticalAlign = VerticalAlign::Bottom;
        else if (lower == "text-top") style.verticalAlign = VerticalAlign::TextTop;
        else if (lower == "text-bottom") style.verticalAlign = VerticalAlign::TextBottom;
        else style.verticalAlign = VerticalAlign::Baseline;
        style.hasVerticalAlign = true;
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
        if (value.find("linear-gradient") != std::string::npos) {
            style.hoverBackgroundGradient = parseGradient(value);
            style.hasHoverGradient = true;
        } else {
            style.hoverBackgroundColor = parseColor(value);
            style.hasHoverBg = true;
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.hoverBackgroundGradient = parseGradient(value);
            style.hasHoverGradient = true;
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
        if (value.find("linear-gradient") != std::string::npos) {
            style.focusBackgroundGradient = parseGradient(value);
            style.hasFocusGradient = true;
        } else {
            style.focusBackgroundColor = parseColor(value);
            style.hasFocusBg = true;
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.focusBackgroundGradient = parseGradient(value);
            style.hasFocusGradient = true;
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
        if (value.find("linear-gradient") != std::string::npos) {
            style.activeBackgroundGradient = parseGradient(value);
            style.hasActiveGradient = true;
        } else {
            style.activeBackgroundColor = parseColor(value);
            style.hasActiveBg = true;
        }
    } else if (name == "background-image") {
        if (value.find("linear-gradient") != std::string::npos) {
            style.activeBackgroundGradient = parseGradient(value);
            style.hasActiveGradient = true;
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

bool StyleSheet::isValidSyntax(const std::string& value, const std::string& syntax) {
    std::string val = trim(value);
    std::string syn = trim(syntax);
    if (syn == "*") return true;
    if (syn.empty()) return true;

    // Handle combinations like "left | right"
    if (syn.find('|') != std::string::npos) {
        std::istringstream ss(syn);
        std::string part;
        while (std::getline(ss, part, '|')) {
            part = trim(part);
            if (part == val || (part.size() > 0 && part[0] == '<' && isValidSyntax(val, part))) {
                return true;
            }
        }
        return false;
    }

    if (syn == "<color>") {
        if (val.empty()) return false;
        if (val == "transparent" || val == "currentColor" || val == "inherit" || val == "initial" || val == "unset") return true;
        if (val[0] == '#') return true;
        if (val.rfind("rgb", 0) == 0 || val.rfind("hsl", 0) == 0) return true;
        std::string lower = lowerAscii(val);
        Color c = parseColor(val);
        if (c.r == 0 && c.g == 0 && c.b == 0 && c.a == 1.0f) {
            return lower == "black" || lower == "#000000" || lower == "#000" || lower == "rgb(0,0,0)" || lower == "rgba(0,0,0,1)";
        }
        if (c.r == 0 && c.g == 0 && c.b == 0 && c.a == 0.0f) {
            return lower == "transparent" || lower == "rgba(0,0,0,0)" || lower == "hsla(0,0%,0%,0)";
        }
        return true;
    }

    if (syn == "<length>") {
        if (val.empty()) return false;
        if (val == "0") return true;
        std::string lower = lowerAscii(val);
        size_t lastNum = lower.find_last_of("0123456789.");
        if (lastNum == std::string::npos) return false;
        std::string unit = lower.substr(lastNum + 1);
        return unit == "px" || unit == "em" || unit == "rem" || unit == "vw" || unit == "vh" || unit == "pt" || unit == "%" || unit == "in" || unit == "cm" || unit == "mm" || unit == "pc";
    }

    if (syn == "<percentage>") {
        if (val.empty()) return false;
        return val.back() == '%';
    }

    if (syn == "<length-percentage>") {
        return isValidSyntax(val, "<length>") || isValidSyntax(val, "<percentage>");
    }

    if (syn == "<number>" || syn == "<integer>") {
        if (val.empty()) return false;
        bool hasDecimal = false;
        for (size_t i = 0; i < val.size(); i++) {
            char c = val[i];
            if (c == '+' || c == '-') {
                if (i != 0) return false;
            } else if (c == '.') {
                if (syn == "<integer>") return false;
                if (hasDecimal) return false;
                hasDecimal = true;
            } else if (!std::isdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    }

    if (syn == "<angle>") {
        if (val.empty()) return false;
        if (val == "0") return true;
        std::string lower = lowerAscii(val);
        size_t lastNum = lower.find_last_of("0123456789.");
        if (lastNum == std::string::npos) return false;
        std::string unit = lower.substr(lastNum + 1);
        return unit == "deg" || unit == "rad" || unit == "grad" || unit == "turn";
    }

    if (syn == "<time>") {
        if (val.empty()) return false;
        if (val == "0") return true;
        std::string lower = lowerAscii(val);
        size_t lastNum = lower.find_last_of("0123456789.");
        if (lastNum == std::string::npos) return false;
        std::string unit = lower.substr(lastNum + 1);
        return unit == "s" || unit == "ms";
    }

    return lowerAscii(val) == lowerAscii(syn);
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

    // min() / max() / clamp() - evaluate dynamically
    if (lower.rfind("min(", 0) == 0) {
        auto parts = splitTopLevel(functionInner(v), ',');
        if (parts.size() >= 2) {
            CSSValue a = parseCSSValue(trim(parts[0]));
            CSSValue b = parseCSSValue(trim(parts[1]));
            CSSValue result;
            result.value = a.value;
            result.unit = a.unit;
            result.calcOp = CSSValue::CalcMin;
            result.calcValue2 = b.value;
            result.calcUnit2 = b.unit;
            return result;
        }
    }
    if (lower.rfind("max(", 0) == 0) {
        auto parts = splitTopLevel(functionInner(v), ',');
        if (parts.size() >= 2) {
            CSSValue a = parseCSSValue(trim(parts[0]));
            CSSValue b = parseCSSValue(trim(parts[1]));
            CSSValue result;
            result.value = a.value;
            result.unit = a.unit;
            result.calcOp = CSSValue::CalcMax;
            result.calcValue2 = b.value;
            result.calcUnit2 = b.unit;
            return result;
        }
    }
    if (lower.rfind("clamp(", 0) == 0) {
        auto parts = splitTopLevel(functionInner(v), ',');
        if (parts.size() >= 3) {
            CSSValue lo = parseCSSValue(trim(parts[0]));
            CSSValue val2 = parseCSSValue(trim(parts[1]));
            CSSValue hi = parseCSSValue(trim(parts[2]));
            CSSValue result;
            result.value = val2.value;
            result.unit = val2.unit;
            result.calcOp = CSSValue::CalcClamp;
            result.calcValue2 = lo.value;
            result.calcUnit2 = lo.unit;
            result.calcValue3 = hi.value;
            result.calcUnit3 = hi.unit;
            return result;
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
        vals.push_back(parseLengthPixels(token, emBase));
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

static bool supportsConditionMatches(std::string_view cond) {
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
    
    // Parse declarations inside the body
    auto declarations = splitDeclarations(body);
    for (const auto& decl : declarations) {
        size_t colon = decl.find(':');
        if (colon == std::string::npos) continue;
        std::string propName = lowerAscii(trim(decl.substr(0, colon)));
        std::string propValue = trim(decl.substr(colon + 1));
        
        if (propName == "syntax") {
            // Strip quotes around syntax string if present
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
                bool isDescendant = false;
                for (size_t c = i; c < j; ++c) {
                    if (rule.combinators[c] == ' ' || rule.combinators[c] == '>') {
                        isDescendant = true;
                        break;
                    }
                }
                
                std::vector<std::string> tgtClasses, tgtIds, tgtTypes;
                extractSelectorFeatures(rule.parts[j], tgtClasses, tgtIds, tgtTypes);
                
                if (tgtClasses.empty() && tgtIds.empty() && tgtTypes.empty()) {
                    auto addAllToInvalidationSet = [&](InvalidationSet& set) {
                        if (isDescendant) {
                            set.invalidateAllDescendants = true;
                        } else {
                            set.invalidateAllSiblings = true;
                        }
                    };
                    for (const auto& c : srcClasses) addAllToInvalidationSet(classInvalidationSets_[c]);
                    for (const auto& idVal : srcIds) addAllToInvalidationSet(idInvalidationSets_[idVal]);
                    for (const auto& t : srcTypes) addAllToInvalidationSet(typeInvalidationSets_[t]);
                } else {
                    auto addToInvalidationSet = [&](InvalidationSet& set) {
                        if (isDescendant) {
                            for (const auto& c : tgtClasses) set.descendantClasses.insert(c);
                            for (const auto& idVal : tgtIds) set.descendantIds.insert(idVal);
                            for (const auto& t : tgtTypes) set.descendantTypes.insert(t);
                        } else {
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

} // namespace FluxUI
