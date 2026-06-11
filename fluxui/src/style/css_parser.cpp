#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include "css_internal.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
namespace FluxUI {
// Bring the shared CSS helpers (now in css_internal.h / FluxUI::detail) into
// this TU's scope so the existing call sites compile unchanged. Color parsing
// lives in css_color.cpp; selector matching + cascade stay here.
using detail::trimLocal;
using detail::lowerAscii;
using detail::functionInner;
using detail::splitColorTokens;
using detail::parseNumberToken;
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
// Defined in css_value_parser.cpp (the @supports evaluator).
bool supportsConditionMatches(std::string_view cond);
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
    parse(
        ".root {\n"
        "    display: flex;\n"
        "    flex-direction: column;\n"
        "    overflow-y: auto;\n"
        "    font-family: sans-serif;\n"
        "    font-size: 16px;\n"
        "    line-height: 1.2;\n"
        "    color: #000000;\n"
        "    background-color: #ffffff;\n"
        "}\n"
        "html, body, div, section, article, aside, header, footer, main, nav, "
        "form, details, summary, address, figure, figcaption, hgroup, search { display: block; }\n"
        "body { margin: 8px; font-family: sans-serif; font-size: 16px; line-height: 1.2; color: #000000; background-color: #ffffff; }\n"
        "h1 { display: block; font-size: 2em; font-weight: bold; margin: 0.67em 0; }\n"
        "h2 { display: block; font-size: 1.5em; font-weight: bold; margin: 0.83em 0; }\n"
        "h3 { display: block; font-size: 1.17em; font-weight: bold; margin: 1em 0; }\n"
        "h4 { display: block; font-size: 1em; font-weight: bold; margin: 1.33em 0; }\n"
        "h5 { display: block; font-size: 0.83em; font-weight: bold; margin: 1.67em 0; }\n"
        "h6 { display: block; font-size: 0.67em; font-weight: bold; margin: 2.33em 0; }\n"
        "strong, b { font-weight: bold; }\n"
        "em, i { font-style: italic; }\n"
        "small { font-size: 0.83em; }\n"
        "span { display: inline; }\n"
        "mark { background-color: #ffff00; color: #000000; padding: 0 2px; }\n"
        "abbr { text-decoration: underline; }\n"
        "sub { font-size: 0.83em; }\n"
        "sup { font-size: 0.83em; }\n"
        "q { display: inline; }\n"
        "cite { font-style: italic; }\n"
        "time { display: inline; }\n"
        "p { display: block; margin: 1em 0; }\n"
        "blockquote { display: block; margin: 1em 40px; }\n"
        "pre { display: block; font-family: monospace; white-space: pre; margin: 1em 0; }\n"
        "code, kbd, samp { font-family: monospace; font-size: 1em; }\n"
        "ul, ol { display: block; margin: 1em 0; padding-left: 40px; }\n"
        "li { display: list-item; }\n"
        "a { display: inline; color: #0000ee; text-decoration: underline; cursor: pointer; }\n"
        "a:hover { color: #0000ee; }\n"
        "a:active { color: #ff0000; }\n"
        "button {\n"
        "    display: inline-block;\n"
        "    padding: 1px 6px;\n"
        "    background-color: #efefef;\n"
        "    border: 2px outset #767676;\n"
        "    border-radius: 2px;\n"
        "    color: #1f1f1f;\n"
        "    font-size: 13.333px;\n"
        "    font-family: inherit;\n"
        "    cursor: default;\n"
        "    text-align: center;\n"
        "    min-height: 22px;\n"
        "    box-sizing: border-box;\n"
        "}\n"
        "button:active { border-style: inset; }\n"
        "button:disabled { background-color: rgba(239,239,239,0.3); border-color: rgba(118,118,118,0.3); color: rgba(16,16,16,0.3); }\n"
        "input {\n"
        "    display: inline-block;\n"
        "    padding: 1px 2px;\n"
        "    background-color: #ffffff;\n"
        "    border: 2px inset #767676;\n"
        "    border-radius: 0;\n"
        "    color: #000000;\n"
        "    font-size: 13.333px;\n"
        "    font-family: inherit;\n"
        "    line-height: normal;\n"
        "    text-transform: none;\n"
        "    text-indent: 0;\n"
        "    text-shadow: none;\n"
        "    text-align: start;\n"
        "    letter-spacing: normal;\n"
        "    word-spacing: normal;\n"
        "    cursor: text;\n"
        "}\n"
        "textarea {\n"
        "    display: block;\n"
        "    padding: 2px;\n"
        "    background-color: #ffffff;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 0;\n"
        "    color: #000000;\n"
        "    font-size: 13.333px;\n"
        "    font-family: monospace;\n"
        "    white-space: pre-wrap;\n"
        "    word-wrap: break-word;\n"
        "    cursor: text;\n"
        "}\n"
        "input:focus, textarea:focus {\n"
        "    border-color: #4d90fe;\n"
        "    outline: 2px solid rgba(77,144,254,0.4);\n"
        "    outline-offset: 0;\n"
        "}\n"
        "input:disabled, textarea:disabled { cursor: default; background-color: rgba(239,239,239,0.3); color: #545454; border-color: rgba(118,118,118,0.3); }\n"
        "label { display: inline; cursor: default; }\n"
        "fieldset {\n"
        "    display: block;\n"
        "    margin: 0 2px;\n"
        "    padding: 0.35em 0.75em 0.625em 0.75em;\n"
        "    border: 2px groove #c0c0c0;\n"
        "    border-radius: 0;\n"
        "}\n"
        "legend {\n"
        "    display: block;\n"
        "    padding: 0 2px;\n"
        "    font-weight: normal;\n"
        "    font-size: 1em;\n"
        "}\n"
        "select {\n"
        "    display: inline-block;\n"
        "    padding: 1px 4px;\n"
        "    background-color: #ffffff;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 0;\n"
        "    color: #000000;\n"
        "    font-size: 13.333px;\n"
        "    font-family: inherit;\n"
        "    min-height: 22px;\n"
        "    cursor: default;\n"
        "    box-sizing: border-box;\n"
        "    white-space: pre;\n"
        "    line-height: normal;\n"
        "}\n"
        "select:disabled { opacity: 0.7; border-color: rgba(118,118,118,0.3); }\n"
        "option { display: none; }\n"
        "input[type='checkbox'] {\n"
        "    display: inline-block;\n"
        "    width: 13px;\n"
        "    height: 13px;\n"
        "    padding: 0;\n"
        "    margin: 3px 3px 3px 4px;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 2px;\n"
        "    background-color: #ffffff;\n"
        "    cursor: default;\n"
        "    box-sizing: border-box;\n"
        "}\n"
        "input[type='radio'] {\n"
        "    display: inline-block;\n"
        "    width: 13px;\n"
        "    height: 13px;\n"
        "    padding: 0;\n"
        "    margin: 3px 3px 0 5px;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 50%;\n"
        "    background-color: #ffffff;\n"
        "    cursor: default;\n"
        "    box-sizing: border-box;\n"
        "}\n"
        "input[type='color'] {\n"
        "    width: 44px;\n"
        "    height: 27px;\n"
        "    padding: 3px 2px;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 4px;\n"
        "    background-color: #ffffff;\n"
        "    box-sizing: border-box;\n"
        "    cursor: default;\n"
        "}\n"
        "input[type='file'] {\n"
        "    display: inline-block;\n"
        "    border: none;\n"
        "    background-color: transparent;\n"
        "    cursor: default;\n"
        "}\n"
        "input[type='range'] {\n"
        "    display: inline-block;\n"
        "    min-width: 129px;\n"
        "    height: 22px;\n"
        "    padding: 0;\n"
        "    border: 0;\n"
        "    background-color: transparent;\n"
        "    cursor: pointer;\n"
        "}\n"
        "meter {\n"
        "    display: inline-block;\n"
        "    min-width: 80px;\n"
        "    height: 16px;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 4px;\n"
        "    background-color: #efefef;\n"
        "}\n"
        "progress {\n"
        "    display: inline-block;\n"
        "    min-width: 160px;\n"
        "    height: 16px;\n"
        "    border: 1px solid #767676;\n"
        "    border-radius: 4px;\n"
        "    background-color: #efefef;\n"
        "}\n"
        "hr { display: block; overflow: hidden; margin: 0.5em auto; border-style: inset; border-width: 1px; color: gray; }\n"
        "details { display: block; margin: 0; padding: 0; }\n"
        "summary {\n"
        "    display: block;\n"
        "    cursor: pointer;\n"
        "    padding: 4px 0;\n"
        "    font-weight: normal;\n"
        "}\n"
        "dialog {\n"
        "    display: none;\n"
        "    position: absolute;\n"
        "    background-color: #ffffff;\n"
        "    color: #000000;\n"
        "    border: 1px solid rgba(0,0,0,0.2);\n"
        "    border-radius: 6px;\n"
        "    padding: 1em;\n"
        "    box-shadow: 0 4px 16px rgba(0,0,0,0.15);\n"
        "}\n"
        "table { display: table; border-collapse: separate; border-spacing: 2px; }\n"
        "thead { display: table-header-group; }\n"
        "tbody { display: table-row-group; }\n"
        "tfoot { display: table-footer-group; }\n"
        "tr { display: table-row; }\n"
        "th { display: table-cell; font-weight: bold; text-align: center; padding: 1px; }\n"
        "td { display: table-cell; padding: 1px; }\n"
        "caption { display: table-caption; text-align: center; }\n"
        "br { display: block; height: 0; }\n"
        "u, ins { text-decoration: underline; }\n"
        "s, strike, del { text-decoration: line-through; }\n"
        "tt { font-family: monospace; }\n"
        "head, title, base, link, meta, style, script, noscript, template,\n"
        "param, source, track, area, datalist { display: none; }\n"
        "picture { display: inline-block; }\n"
        "audio { display: inline-block; width: 300px; height: 54px; }\n"
        "embed, iframe, object { display: inline-block; width: 300px; height: 150px; }\n"
        "iframe { border: 2px solid #767676; }\n"
        "map { display: inline; }\n"
        "optgroup { display: block; font-weight: bold; }\n"
        "input:focus, textarea:focus, select:focus { outline-offset: 0; }\n"
    );
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
        else if (t.type == CSSToken::RightBrace && braceDepth > 0) {
            braceDepth--;
            if (braceDepth == 0 && parenDepth == 0 && bracketDepth == 0) {
                currentDecl += t.text;
                std::string item = trim(currentDecl);
                if (!item.empty()) {
                    declarations.push_back(item);
                }
                currentDecl.clear();
                continue;
            }
        }
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
using detail::splitWhitespace;
using detail::splitWhitespaceTopLevel;
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
static std::string normalizeAttributeValue(std::string value, bool keepCase = false) {
    value = trimLocal(value);
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return keepCase ? trimLocal(value) : lowerAscii(trimLocal(value));
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
    bool caseInsensitive = false;
    std::string body = trimLocal(selector);
    if (body.empty()) return false;
    if (body.size() >= 2 && (body.substr(body.size() - 2) == " i" || body.substr(body.size() - 2) == " I")) {
        caseInsensitive = true;
        body = trimLocal(body.substr(0, body.size() - 2));
    } else if (body.size() >= 2 && (body.substr(body.size() - 2) == " s" || body.substr(body.size() - 2) == " S")) {
        caseInsensitive = false;
        body = trimLocal(body.substr(0, body.size() - 2));
    }
    size_t eq = body.find('=');
    std::string op = "=";
    std::string name;
    std::string rawValue;
    if (eq == std::string::npos) {
        name = lowerAscii(trimLocal(body));
    } else {
        size_t nameEnd = eq;
        if (eq > 0 && (body[eq - 1] == '~' || body[eq - 1] == '|' || body[eq - 1] == '^' || body[eq - 1] == '$' || body[eq - 1] == '*')) {
            nameEnd = eq - 1;
            op = body.substr(eq - 1, 2);
        }
        name = lowerAscii(trimLocal(body.substr(0, nameEnd)));
        rawValue = body.substr(eq + 1);
    }
    if (name.empty()) return false;
    std::string_view actualValSV = selectorAttributeValue(actualType, name);
    bool isFlag = selectorHasFlag(actualType, name);
    if (eq == std::string::npos) {
        if (name == "type" && selectorBaseType(actualType) == "input") return true;
        return !actualValSV.empty() || isFlag;
    }
    std::string requiredValue = normalizeAttributeValue(rawValue, !caseInsensitive);
    std::string actualValue;
    if (name == "type" && actualValSV.empty() && selectorBaseType(actualType) == "input") {
        actualValue = "text";
    } else if (isFlag) {
        actualValue = name;
    } else {
        actualValue = std::string(actualValSV);
    }
    if (!caseInsensitive) {
        if (name == "class" || name == "id" || name == "type" || name == "dir") {
            actualValue = lowerAscii(actualValue);
            requiredValue = lowerAscii(requiredValue);
        }
    } else {
        actualValue = lowerAscii(actualValue);
        requiredValue = lowerAscii(requiredValue);
    }
    if (op == "=") {
        return actualValue == requiredValue;
    } else if (op == "~=") {
        std::vector<std::string> words;
        std::string current;
        for (char c : actualValue) {
            if (std::isspace((unsigned char)c)) {
                if (!current.empty()) {
                    words.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) words.push_back(current);
        for (const auto& word : words) {
            if (word == requiredValue) return true;
        }
        return false;
    } else if (op == "|=") {
        return actualValue == requiredValue ||
               (actualValue.size() > requiredValue.size() &&
                actualValue.substr(0, requiredValue.size()) == requiredValue &&
                actualValue[requiredValue.size()] == '-');
    } else if (op == "^=") {
        if (requiredValue.empty()) return false;
        return actualValue.size() >= requiredValue.size() &&
               actualValue.substr(0, requiredValue.size()) == requiredValue;
    } else if (op == "$=") {
        if (requiredValue.empty()) return false;
        return actualValue.size() >= requiredValue.size() &&
               actualValue.substr(actualValue.size() - requiredValue.size()) == requiredValue;
    } else if (op == "*=") {
        if (requiredValue.empty()) return false;
        return actualValue.find(requiredValue) != std::string::npos;
    }
    return false;
}
static bool selectorPseudoMatches(std::string_view pseudoName,
                                  std::string_view actualType,
                                  const Widget* widget = nullptr) {
    if (pseudoName == "checked") return selectorHasFlag(actualType, "checked");
    if (pseudoName == "open") return selectorHasFlag(actualType, "open");
    if (pseudoName == "indeterminate") return selectorHasFlag(actualType, "indeterminate");
    if (pseudoName == "enabled") {
        if (selectorHasFlag(actualType, "disabled")) return false;
        return widget ? !widget->disabled : true;
    }
    if (pseudoName == "disabled") {
        return selectorHasFlag(actualType, "disabled") || (widget && widget->disabled);
    }
    if (pseudoName == "hover") return widget && widget->hovered;
    if (pseudoName == "focus" || pseudoName == "focus-visible") return widget && widget->focused;
    if (pseudoName == "active") return widget && widget->pressed;
    if (pseudoName == "focus-within") {
        if (!widget) return false;
        struct FocusCheck {
            static bool check(const Widget* w) {
                if (w->focused) return true;
                for (const auto& child : w->children) {
                    if (child && check(child.get())) return true;
                }
                return false;
            }
        };
        return FocusCheck::check(widget);
    }
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
    // ── Form-control pseudo-classes (CSS Selectors L4 §4.4 / CSS UI L4) ──
    // States are encoded into selectorType() (|required, |invalid,
    // |placeholder-shown) just like |checked / |disabled.
    if (pseudoName == "placeholder-shown") {
        return selectorHasFlag(actualType, "placeholder-shown");
    }
    if (pseudoName == "required") {
        return selectorHasFlag(actualType, "required");
    }
    if (pseudoName == "optional") {
        // Optional applies to form controls that are not required.
        std::string_view baseType = selectorBaseType(actualType);
        bool isFormControl = baseType == "input" || baseType == "textarea" ||
                             baseType == "select";
        return isFormControl && !selectorHasFlag(actualType, "required");
    }
    if (pseudoName == "invalid") {
        return selectorHasFlag(actualType, "invalid");
    }
    if (pseudoName == "valid") {
        // Valid: a form control that is not currently invalid.
        std::string_view baseType = selectorBaseType(actualType);
        bool isFormControl = baseType == "input" || baseType == "textarea" ||
                             baseType == "select";
        return isFormControl && !selectorHasFlag(actualType, "invalid");
    }
    return false;
}
static void splitSelectorChain(const std::string& selector,
                               std::vector<std::string>& parts,
                               std::vector<char>& combinators);
static bool parseNth(std::string_view inner, int& a, int& b) {
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
        if (ofType && std::string_view(sibling->selectorType()) != targetType) continue;
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

    // :has() takes a forgiving relative selector list. Each relative selector
    // may start with a combinator (>, +, ~) that anchors it to `root`:
    //   :has(.x)   / :has(> .x)  → descendant / direct-child axis
    //   :has(+ .x)               → next adjacent sibling
    //   :has(~ .x)               → any following sibling
    // (CSS Selectors L4 §4.5 / Blink CheckPseudoHas.)
    auto matchesAnchored = [](const Widget* anchor, const std::string& rel) -> bool {
        std::string sel = std::string(trimLocal(rel));
        if (sel.empty()) return false;

        char leadComb = ' ';            // default: descendant
        if (sel[0] == '>' || sel[0] == '+' || sel[0] == '~') {
            leadComb = sel[0];
            sel = std::string(trimLocal(std::string_view(sel).substr(1)));
            if (sel.empty()) return false;
        }

        // Build a rule for the (possibly compound/complex) relative selector.
        std::vector<std::string> parts;
        std::vector<char> combinators;
        splitSelectorChain(sel, parts, combinators);
        if (parts.empty()) return false;
        CSSRule rule;
        rule.selector = sel;
        rule.parts = parts;
        rule.combinators = combinators;

        auto matchNode = [&](const Widget* w) -> bool {
            if (!w) return false;
            std::vector<CSSSelectorNode> anc;
            for (const Widget* p = w->parent; p; p = p->parent) {
                anc.push_back({p->className, p->id, p->selectorType(), p});
            }
            return StyleSheet::selectorMatches(rule, w->className, w->id,
                                               w->selectorType(), anc, nullptr, w);
        };

        if (leadComb == '+' || leadComb == '~') {
            // Sibling axis: look at siblings after `anchor` in its parent.
            if (!anchor->parent) return false;
            const auto& sibs = anchor->parent->children;
            size_t idx = 0; bool found = false;
            for (size_t i = 0; i < sibs.size(); ++i) {
                if (sibs[i].get() == anchor) { idx = i; found = true; break; }
            }
            if (!found) return false;
            if (leadComb == '+') {
                return idx + 1 < sibs.size() && matchNode(sibs[idx + 1].get());
            }
            for (size_t i = idx + 1; i < sibs.size(); ++i) {
                if (matchNode(sibs[i].get())) return true;
            }
            return false;
        }

        // Descendant (' ') or direct-child ('>') axis. Iterative walk to avoid
        // a templated local struct (not allowed) / std::function dependency.
        if (leadComb == '>') {
            for (const auto& cs : anchor->children) {
                if (cs && matchNode(cs.get())) return true;
            }
            return false;
        }
        std::vector<const Widget*> stack;
        for (const auto& cs : anchor->children) {
            if (cs) stack.push_back(cs.get());
        }
        while (!stack.empty()) {
            const Widget* c = stack.back();
            stack.pop_back();
            if (matchNode(c)) return true;
            for (const auto& cs : c->children) {
                if (cs) stack.push_back(cs.get());
            }
        }
        return false;
    };

    // Forgiving selector list: any relative selector matching satisfies :has().
    for (const auto& rel : splitSelectorListLocal(selector)) {
        if (matchesAnchored(root, rel)) return true;
    }
    return false;
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
                    if (!selectorPseudoMatches(pseudoName, type, widget)) return false;
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
                if (parts.empty()) continue;
                if (parts.size() == 1) {
                    // Simple compound: just match against current element
                    if (matchCompoundSelector(parts[0], className, id, type, widget)) {
                        matchedAny = true;
                        break;
                    }
                } else if (widget) {
                    // Complex selector with combinators: build a temporary rule
                    // and use full selectorMatches with ancestor traversal (Blink parity).
                    CSSRule tempRule;
                    tempRule.selector = selector;
                    tempRule.parts = parts;
                    tempRule.combinators = combinators;
                    std::vector<CSSSelectorNode> ancestors;
                    for (const Widget* p = widget->parent; p; p = p->parent) {
                        ancestors.push_back({p->className, p->id, p->selectorType(), p});
                    }
                    if (StyleSheet::selectorMatches(tempRule, className, id, type, ancestors, nullptr, widget)) {
                        matchedAny = true;
                        break;
                    }
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
    // Detect double-colon (::) � cut position should be at the first colon.
    size_t cutPos = colon;
    if (colon > 0 && selector[colon - 1] == ':') {
        cutPos = colon - 1;
    }
    size_t nameStart = colon + 1;
    if (nameStart < selector.size() && selector[nameStart] == ':') {
        nameStart++;
    }
    std::string name = lowerAscii(trimLocal(selector.substr(nameStart)));
    // Strip trailing (�) for functional pseudo-elements like ::part(foo), ::slotted(div)
    auto parenPos = name.find('(');
    std::string baseName = (parenPos != std::string::npos) ? name.substr(0, parenPos) : name;

    // Pseudo-classes that create separate style resolution contexts
    if (baseName == "hover" || baseName == "focus" ||
        baseName == "focus-visible" || baseName == "active") {
        if (pseudo) *pseudo = baseName;
        selector = trimLocal(selector.substr(0, cutPos));
        return true;
    }
    // Pseudo-elements (Blink PseudoId parity: all W3C Level 4 pseudo-elements)
    static const char* const kPseudoElements[] = {
        "before", "after", "placeholder", "selection", "marker",
        "first-letter", "first-line", "backdrop", "file-selector-button",
        "part", "slotted", "highlight", "spelling-error", "grammar-error",
        "target-text", "cue", "progress-bar", "progress-value",
        "scrollbar", "scrollbar-thumb", "scrollbar-track",
        nullptr
    };
    for (int i = 0; kPseudoElements[i]; ++i) {
        if (baseName == kPseudoElements[i]) {
            if (pseudo) *pseudo = name; // keep full name including (arg) if present
            selector = trimLocal(selector.substr(0, cutPos));
            return true;
        }
    }
    return false;
}
// CSS color channel parsers moved to css_internal.h (detail::); parseColor /
// parseGradient implementations live in css_color.cpp.

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
    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
        const auto& ancestor = *it;
        h1 ^= 0xDDULL; h2 ^= 0xDDULL;
        hashStr(ancestor.className);
        h1 ^= 0xCCULL; h2 ^= 0xCCULL;
        hashStr(ancestor.id);
        h1 ^= 0xBBULL; h2 ^= 0xBBULL;
        hashStr(ancestor.type);
    }
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
void StyleSheet::reset() {
    // Rebuild from a freshly-constructed sheet (which loads the UA stylesheet),
    // preserving the current viewport + color-scheme so layout/media queries
    // don't flip on reload. The cache epoch advances so every resolved style is
    // recomputed against the new rules.
    float vpW = viewportWidth_;
    float vpH = viewportHeight_;
    ColorScheme scheme = colorScheme_;
    bool forced = forcedColors_;
    *this = StyleSheet();
    viewportWidth_ = vpW;
    viewportHeight_ = vpH;
    colorScheme_ = scheme;
    forcedColors_ = forced;
    currentEpoch_++;
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
        return (int)layersOrder.size();
    }
    for (size_t i = 0; i < layersOrder.size(); ++i) {
        if (layersOrder[i] == layerName) {
            return (int)i;
        }
    }
    return -1;
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
        index++;
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
            index++;
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
                    index++;
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
                } else if (lowerPrelude.rfind("@keyframes", 0) == 0 ||
                           lowerPrelude.rfind("@-webkit-keyframes", 0) == 0) {
                    size_t offset = (lowerPrelude.rfind("@-webkit-keyframes", 0) == 0) ? 18 : 10;
                    std::string name = trim(preludeStr.substr(offset));
                    if (!name.empty()) {
                        parseKeyframes(name, blockContent);
                    }
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
                } else if (lowerPrelude.rfind("@scope", 0) == 0) {
                    // @scope (<root>) [to (<limit>)] { ... }
                    // CSS Cascading & Inheritance Level 6 / Blink CSSScopeRule parity
                    std::string args = trim(preludeStr.substr(6));
                    std::string scopeRoot, scopeLimit;
                    // Parse: (<root-selector>) [to (<limit-selector>)]
                    size_t openParen = args.find('(');
                    if (openParen != std::string::npos) {
                        // Find matching close for root selector
                        int depth = 0; size_t closeRoot = openParen;
                        for (size_t k = openParen; k < args.size(); k++) {
                            if (args[k] == '(') ++depth;
                            else if (args[k] == ')') { --depth; if (depth == 0) { closeRoot = k; break; } }
                        }
                        scopeRoot = trim(args.substr(openParen + 1, closeRoot - openParen - 1));
                        // Check for "to (...)"
                        std::string rest = lowerAscii(trim(args.substr(closeRoot + 1)));
                        if (rest.rfind("to", 0) == 0) {
                            std::string afterTo = trim(args.substr(closeRoot + 1));
                            size_t toOpen = afterTo.find('(');
                            if (toOpen != std::string::npos) {
                                int d2 = 0; size_t toClose = toOpen;
                                for (size_t k = toOpen; k < afterTo.size(); k++) {
                                    if (afterTo[k] == '(') ++d2;
                                    else if (afterTo[k] == ')') { --d2; if (d2 == 0) { toClose = k; break; } }
                                }
                                scopeLimit = trim(afterTo.substr(toOpen + 1, toClose - toOpen - 1));
                            }
                        }
                    }
                    // Parse inner rules and tag them with scope metadata
                    size_t rulesBefore = rules.size();
                    parseRulesFromTokens(blockContent, mediaQuery, currentLayer);
                    // Annotate all newly added rules with scope root/limit
                    for (size_t r = rulesBefore; r < rules.size(); r++) {
                        rules[r].scopeRoot = scopeRoot;
                        rules[r].scopeLimit = scopeLimit;
                    }
                } else if (lowerPrelude.rfind("@starting-style", 0) == 0) {
                    // @starting-style { ... }
                    // CSS Transitions Level 2: rules that apply before the first style update
                    size_t rulesBefore = rules.size();
                    parseRulesFromTokens(blockContent, mediaQuery, currentLayer);
                    // Mark and move to startingStyleRules
                    for (size_t r = rulesBefore; r < rules.size(); r++) {
                        rules[r].isStartingStyle = true;
                        startingStyleRules.push_back(rules[r]);
                    }
                    // Remove from main rules (they shouldn't apply during normal resolve)
                    rules.erase(rules.begin() + (ptrdiff_t)rulesBefore, rules.end());
                } else if (lowerPrelude.rfind("@view-transition", 0) == 0) {
                    // @view-transition { navigation: auto; types: slide; }
                    // CSS View Transitions Level 2
                    std::string bodyStr;
                    for (const auto& t : blockContent) bodyStr += t.text;
                    // Parse declarations
                    std::istringstream ss(bodyStr);
                    std::string line;
                    while (std::getline(ss, line, ';')) {
                        line = trim(line);
                        auto colonPos = line.find(':');
                        if (colonPos == std::string::npos) continue;
                        std::string prop = lowerAscii(trim(line.substr(0, colonPos)));
                        std::string val  = trim(line.substr(colonPos + 1));
                        if (prop == "navigation") {
                            viewTransition.navigation = lowerAscii(val);
                        } else if (prop == "types") {
                            viewTransition.types.clear();
                            std::istringstream ts(val);
                            std::string t; while (ts >> t) viewTransition.types.push_back(t);
                        }
                    }
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
                if (lowerPrelude.rfind("@import", 0) == 0) {
                    // @import url("...") [layer] [media];
                    // CSS Cascading Level 5 / Blink CSSImportRule parity.
                    // Parse the URL and optional layer/media list.
                    std::string args = trim(preludeStr.substr(7));
                    std::string url;
                    std::string importLayer;
                    std::string importMedia;
                    // Extract URL (url("...") or "..." or '...')
                    if (args.rfind("url(", 0) == 0) {
                        size_t close = args.find(')', 4);
                        if (close != std::string::npos) {
                            url = trim(args.substr(4, close - 4));
                            args = trim(args.substr(close + 1));
                        }
                    } else if (!args.empty() && (args[0] == '"' || args[0] == '\'')) {
                        char q = args[0];
                        size_t close = args.find(q, 1);
                        if (close != std::string::npos) {
                            url = args.substr(1, close - 1);
                            args = trim(args.substr(close + 1));
                        }
                    }
                    // Strip quotes from url
                    if (!url.empty() && (url[0] == '"' || url[0] == '\'')) url = url.substr(1);
                    if (!url.empty() && (url.back() == '"' || url.back() == '\'')) url.pop_back();
                    // Check for "layer" or "layer(name)" keyword
                    std::string argsLower = lowerAscii(args);
                    if (argsLower.rfind("layer", 0) == 0) {
                        if (argsLower.size() > 5 && argsLower[5] == '(') {
                            size_t cp = argsLower.find(')', 6);
                            importLayer = trim(args.substr(6, cp != std::string::npos ? cp - 6 : std::string::npos));
                            args = (cp != std::string::npos) ? trim(args.substr(cp + 1)) : "";
                        } else {
                            // Anonymous import layer
                            static int importLayerCount = 0;
                            importLayer = "::import_layer_" + std::to_string(++importLayerCount);
                            args = trim(args.substr(5));
                        }
                        if (!importLayer.empty()) {
                            std::string fullLayer = currentLayer.empty() ? importLayer : currentLayer + "." + importLayer;
                            registerLayer(fullLayer);
                            importLayer = fullLayer;
                        }
                    }
                    // Remaining args = media query (if any)
                    if (!args.empty()) importMedia = args;
                    // Store import for external loading (FluxUI loads via loadFile() externally).
                    // Record it so the host can resolve imports after parsing.
                    CSSProperty importProp;
                    importProp.name = "@import";
                    importProp.value = url;
                    importProp.sourceOrder = nextPropertyOrder_++;
                    // Store as a special internal record (accessible via rules for import order)
                    CSSRule importRule;
                    importRule.selector = "@import";
                    importRule.mediaQuery = importMedia;
                    importRule.layer = importLayer;
                    importRule.properties.push_back(importProp);
                    importRule.specificity = -1; // marker for import rules
                    rules.push_back(importRule);
                } else if (lowerPrelude.rfind("@layer", 0) == 0) {
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
    std::vector<std::string> nestedRules;
    for (std::string line : splitDeclarations(body)) {
        line = trim(line);
        if (line.empty()) continue;
        auto brace = line.find('{');
        if (brace != std::string::npos) {
            nestedRules.push_back(line);
            continue;
        }
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        CSSProperty prop;
        prop.name = lowerAscii(trim(line.substr(0, colon)));
        prop.value = trim(line.substr(colon + 1));
        prop.sourceOrder = nextPropertyOrder_++;
        properties.push_back(prop);
    }
    for (const std::string& nestedStr : nestedRules) {
        auto brace = nestedStr.find('{');
        std::string nestedSelector = trim(nestedStr.substr(0, brace));
        auto rbrace = nestedStr.rfind('}');
        if (rbrace == std::string::npos || rbrace <= brace) continue;
        std::string nestedBody = nestedStr.substr(brace + 1, rbrace - brace - 1);

        // Handle nested at-rules: @media, @supports, @layer within a style rule
        // (CSS Nesting Level 1 �3 � nested group rules)
        std::string nestedLower = lowerAscii(nestedSelector);
        if (!nestedSelector.empty() && nestedSelector[0] == '@') {
            if (nestedLower.rfind("@media", 0) == 0) {
                std::string query = trim(nestedSelector.substr(6));
                std::string combinedQuery = mediaQuery.empty() ? query : mediaQuery + " and " + query;
                // Re-wrap the body with the current parent selector so rules inherit context
                parseRule(selector, nestedBody, combinedQuery, currentLayer);
            } else if (nestedLower.rfind("@supports", 0) == 0) {
                parseRule(selector, nestedBody, mediaQuery, currentLayer);
            } else if (nestedLower.rfind("@layer", 0) == 0) {
                std::string layerName = trim(nestedSelector.substr(6));
                std::string nestedLayerName = currentLayer.empty() ? layerName : currentLayer + "." + layerName;
                parseRule(selector, nestedBody, mediaQuery, nestedLayerName);
            }
            continue;
        }

        std::string resolvedSelector;
        std::vector<std::string> parentParts = splitTopLevel(selector, ',');
        std::vector<std::string> nestedParts = splitTopLevel(nestedSelector, ',');
        for (size_t i = 0; i < parentParts.size(); ++i) {
            std::string parentPart = trim(parentParts[i]);
            for (size_t j = 0; j < nestedParts.size(); ++j) {
                std::string nestedPart = trim(nestedParts[j]);
                if (!resolvedSelector.empty()) resolvedSelector += ", ";
                // Replace ALL occurrences of & with parent (Blink CSSNestingType parity)
                if (nestedPart.find('&') != std::string::npos) {
                    std::string resolved;
                    for (size_t k = 0; k < nestedPart.size(); ++k) {
                        if (nestedPart[k] == '&') {
                            resolved += parentPart;
                        } else {
                            resolved += nestedPart[k];
                        }
                    }
                    resolvedSelector += resolved;
                } else {
                    // Implicit descendant nesting (CSS Nesting �2.1)
                    resolvedSelector += parentPart + " " + nestedPart;
                }
            }
        }
        parseRule(resolvedSelector, nestedBody, mediaQuery, currentLayer);
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
        // Track pseudo-element rule presence for fast-path skip during resolution.
        if (!rule.pseudoState.empty()) {
            const std::string& ps = rule.pseudoState;
            if (ps == "before") hasBeforeRules_ = true;
            else if (ps == "after") hasAfterRules_ = true;
            else if (ps == "placeholder") hasPlaceholderRules_ = true;
            else if (ps == "selection") hasSelectionRules_ = true;
            else if (ps == "marker") hasMarkerRules_ = true;
        }
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
                                             int recursionDepth) const {
    // Blink CSSVariableResolver parity: maximum recursion depth guard.
    // CSS Variables Level 1 �3: "if a custom property has a cycle, all values
    // in that cycle are treated as if they had their initial value."
    if (recursionDepth > 32) {
        if (valid) *valid = false;
        return "";
    }

    // Thread-local cycle detection set (Blink's ResolutionState / visiting_ set).
    // Tracks which variable names are currently being resolved on this call stack.
    // If we encounter a var(--x) while --x is already in the set ? cycle ? invalid.
    thread_local std::vector<std::string> resolving_;

    std::string out;
    size_t pos = 0;
    while (pos < value.size()) {
        size_t varStart = value.find("var(", pos);
        if (varStart == std::string::npos) {
            out += value.substr(pos);
            break;
        }
        out += value.substr(pos, varStart - pos);

        // Find matching ')' for this var() � depth-aware
        size_t cursor = varStart + 4;
        int parenDepth = 1;
        while (cursor < value.size() && parenDepth > 0) {
            if (value[cursor] == '(') parenDepth++;
            if (value[cursor] == ')') parenDepth--;
            if (parenDepth > 0) cursor++;
        }
        if (cursor >= value.size()) {
            out += value.substr(varStart);
            break;
        }

        // Parse var(--name [, fallback])
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

        // -- Cycle detection (Blink CSSVariableResolver::ResolvePendingSubstitutions parity) --
        // Check if this variable is already being resolved on this call stack.
        bool isCycle = false;
        for (const auto& v : resolving_) {
            if (v == name) { isCycle = true; break; }
        }
        if (isCycle) {
            // CSS Variables �3: cycle detected ? use fallback if available, else invalid.
            if (!fallback.empty()) {
                out += resolveValueInternal(fallback, customProperties, valid, recursionDepth + 1);
            } else {
                if (valid) *valid = false;
                return "";
            }
            pos = cursor + 1;
            continue;
        }

        // Push this variable onto the resolution stack
        resolving_.push_back(name);

        // Look up the variable value
        std::string resolved;
        bool found = false;
        bool innerValid = true;

        if (customProperties) {
            auto customIt = customProperties->find(name);
            if (customIt != customProperties->end()) {
                bool iv = true;
                resolved = resolveValueInternal(customIt->second, customProperties, &iv, recursionDepth + 1);
                if (iv) { found = true; }
                else { innerValid = false; }
            }
        }
        if (!found && innerValid) {
            auto it = variables_.find(name);
            if (it != variables_.end()) {
                bool iv = true;
                resolved = resolveValueInternal(it->second, customProperties, &iv, recursionDepth + 1);
                if (iv) { found = true; }
                else { innerValid = false; }
            }
        }
        // If resolution failed (cycle or invalid), try the fallback
        if (!found || !innerValid) {
            if (!fallback.empty()) {
                bool iv = true;
                resolved = resolveValueInternal(fallback, customProperties, &iv, recursionDepth + 1);
                if (iv) { found = true; innerValid = true; }
                else { found = false; }
            }
        }
        if (!found) {
            // Last resort: @property initial value
            auto defIt = propertyDefinitions_.find(name);
            if (defIt != propertyDefinitions_.end() && !defIt->second.initialValue.empty()) {
                bool iv = true;
                resolved = resolveValueInternal(defIt->second.initialValue, customProperties, &iv, recursionDepth + 1);
                if (iv) { found = true; }
            }
        }
        if (!found) {
            if (valid) *valid = false;
            resolving_.pop_back();
            return "";
        }

        // Pop from resolution stack
        resolving_.pop_back();

        out += resolved;
        pos = cursor + 1;
    }
    return trim(out);
}
static bool applyCSSWideProperty(Style& target,
                                 const std::string& name,
                                 const std::string& keyword,
                                 const Style* parentStyle,
                                 const Style& initialStyle);
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
    auto animationName              = std::move(target.rare().animationName);
    auto animationDuration          = std::move(target.rare().animationDuration);
    auto animationDelay             = std::move(target.rare().animationDelay);
    auto animationIterationCount    = std::move(target.rare().animationIterationCount);
    auto animationDirection         = std::move(target.rare().animationDirection);
    auto animationFillMode          = std::move(target.rare().animationFillMode);
    auto animationPlayState         = std::move(target.rare().animationPlayState);
    auto animationTimingFunction    = std::move(target.rare().animationTimingFunction);
    auto animationComposition       = std::move(target.rare().animationComposition);
    auto transitionProperty         = std::move(target.rare().transitionProperty);
    auto transitionDurations        = std::move(target.rare().transitionDurations);
    auto transitionDelays           = std::move(target.rare().transitionDelays);
    auto transitionTimingFunctions  = std::move(target.rare().transitionTimingFunctions);
    auto transitionBehavior         = std::move(target.rare().transitionBehavior);
    target = source;
    target.customProperties = std::move(customProperties);
    target.hoverCustomProperties = std::move(hoverCustomProperties);
    target.focusCustomProperties = std::move(focusCustomProperties);
    target.activeCustomProperties = std::move(activeCustomProperties);
    target.unresolvedBackgroundColor = std::move(unresolvedBackgroundColor);
    target.unresolvedColor = std::move(unresolvedColor);
    target.unresolvedBorderColor = std::move(unresolvedBorderColor);
    target.unresolvedBackgroundGradient = std::move(unresolvedBackgroundGradient);
    target.rare().animationName              = std::move(animationName);
    target.rare().animationDuration          = std::move(animationDuration);
    target.rare().animationDelay             = std::move(animationDelay);
    target.rare().animationIterationCount    = std::move(animationIterationCount);
    target.rare().animationDirection         = std::move(animationDirection);
    target.rare().animationFillMode          = std::move(animationFillMode);
    target.rare().animationPlayState         = std::move(animationPlayState);
    target.rare().animationTimingFunction    = std::move(animationTimingFunction);
    target.rare().animationComposition       = std::move(animationComposition);
    target.rare().transitionProperty         = std::move(transitionProperty);
    target.rare().transitionDurations        = std::move(transitionDurations);
    target.rare().transitionDelays           = std::move(transitionDelays);
    target.rare().transitionTimingFunctions  = std::move(transitionTimingFunctions);
    target.rare().transitionBehavior         = std::move(transitionBehavior);
}
static bool applyCSSWideProperty(Style& target,
                                 const std::string& name,
                                 const std::string& keyword,
                                 const Style* parentStyle,
                                 const Style& initialStyle) {
    // CSS-wide keywords: inherit, initial, unset, revert, revert-layer
    // (CSS Cascading Level 5 / Blink StyleCascade parity)
    if (keyword != "inherit" && keyword != "initial" && keyword != "unset" &&
        keyword != "revert" && keyword != "revert-layer") {
        return false;
    }

    // Determine the source style for this keyword.
    // revert: roll back to the previous cascade origin (author ? user ? UA).
    //   In practice, since we don't track per-origin resolved styles separately,
    //   we approximate by using the UA default (initialStyle) for author revert,
    //   or parentStyle for inherited properties. This matches Blink's behavior
    //   where revert in author style falls back to UA computed value.
    // revert-layer: roll back to the previous cascade layer.
    //   Since we apply all layers in a single pass, we approximate this by
    //   using the same logic as revert (fallback to initial/parent).
    //   Full fidelity would require per-layer style snapshots.
    const Style& source = [&]() -> const Style& {
        if (keyword == "inherit") {
            return parentStyle ? *parentStyle : initialStyle;
        }
        if (keyword == "unset") {
            if (isInheritedCSSProperty(name))
                return parentStyle ? *parentStyle : initialStyle;
            return initialStyle;
        }
        if (keyword == "revert" || keyword == "revert-layer") {
            // Blink: revert in author origin ? use user/UA value.
            // For inherited properties, use parent (inheriting from parent
            // effectively gives the "previous origin" value for inherited props).
            // For non-inherited, use the initial value.
            if (isInheritedCSSProperty(name))
                return parentStyle ? *parentStyle : initialStyle;
            return initialStyle;
        }
        // initial
        return initialStyle;
    }();

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
    } else if (name == "column-count") {
        target.columnCount = source.columnCount;
    } else if (name == "column-width") {
        target.columnWidth = source.columnWidth;
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
        target.rare().transitionDurations = source.rare().transitionDurations;
        target.rare().transitionProperty = source.rare().transitionProperty;
        target.rare().transitionDelays = source.rare().transitionDelays;
        target.rare().transitionTimingFunctions = source.rare().transitionTimingFunctions;
        target.rare().transitionBehavior = source.rare().transitionBehavior;
        target.transitionDuration = source.transitionDuration;
    } else if (name == "transition-property") {
        target.rare().transitionProperty = source.rare().transitionProperty;
    } else if (name == "transition-duration") {
        target.rare().transitionDurations = source.rare().transitionDurations;
        target.transitionDuration = source.transitionDuration;
    } else if (name == "transition-delay") {
        target.rare().transitionDelays = source.rare().transitionDelays;
    } else if (name == "transition-timing-function") {
        target.rare().transitionTimingFunctions = source.rare().transitionTimingFunctions;
    } else if (name == "transition-behavior") {
        target.rare().transitionBehavior = source.rare().transitionBehavior;
    } else if (name == "animation") {
        target.rare().animationName = source.rare().animationName;
        target.rare().animationDuration = source.rare().animationDuration;
        target.rare().animationDelay = source.rare().animationDelay;
        target.rare().animationIterationCount = source.rare().animationIterationCount;
        target.rare().animationDirection = source.rare().animationDirection;
        target.rare().animationFillMode = source.rare().animationFillMode;
        target.rare().animationPlayState = source.rare().animationPlayState;
        target.rare().animationTimingFunction = source.rare().animationTimingFunction;
        target.rare().animationComposition = source.rare().animationComposition;
    } else if (name == "animation-name") {
        target.rare().animationName = source.rare().animationName;
    } else if (name == "animation-duration") {
        target.rare().animationDuration = source.rare().animationDuration;
    } else if (name == "animation-delay") {
        target.rare().animationDelay = source.rare().animationDelay;
    } else if (name == "animation-iteration-count") {
        target.rare().animationIterationCount = source.rare().animationIterationCount;
    } else if (name == "animation-direction") {
        target.rare().animationDirection = source.rare().animationDirection;
    } else if (name == "animation-fill-mode") {
        target.rare().animationFillMode = source.rare().animationFillMode;
    } else if (name == "animation-play-state") {
        target.rare().animationPlayState = source.rare().animationPlayState;
    } else if (name == "animation-timing-function") {
        target.rare().animationTimingFunction = source.rare().animationTimingFunction;
    } else if (name == "animation-composition") {
        target.rare().animationComposition = source.rare().animationComposition;
    } else if (name == "scale") {
        target.scale = source.scale;
    } else if (name == "transform") {
        target.transform = source.transform;
        target.hasTransform = source.hasTransform;
        target.scale = source.scale;
    } else if (name == "transform-origin") {
        target.transformOrigin = source.transformOrigin;
        target.hasTransformOrigin = source.hasTransformOrigin;
    } else if (name == "transform-style") {
        target.transformStyle = source.transformStyle;
        target.hasTransformStyle = source.hasTransformStyle;
    } else if (name == "transform-box") {
        target.transformBox = source.transformBox;
        target.hasTransformBox = source.hasTransformBox;
    } else if (name == "perspective") {
        target.perspective = source.perspective;
        target.hasPerspective = source.hasPerspective;
    } else if (name == "perspective-origin") {
        target.perspectiveOrigin = source.perspectiveOrigin;
        target.hasPerspectiveOrigin = source.hasPerspectiveOrigin;
    } else if (name == "backface-visibility") {
        target.backfaceVisibility = source.backfaceVisibility;
        target.hasBackfaceVisibility = source.hasBackfaceVisibility;
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
    // direction / writingMode are inherited too (see widget_cascade.cpp), so
    // include them here to invalidate descendants when they change.
    hashBytes(&style.direction, sizeof(style.direction));
    hashBytes(&style.writingMode, sizeof(style.writingMode));
    // text-shadow is inherited; fold each layer into the hash.
    for (const auto& sh : style.textShadows) {
        hashBytes(&sh.offsetX, sizeof(sh.offsetX));
        hashBytes(&sh.offsetY, sizeof(sh.offsetY));
        hashBytes(&sh.blur, sizeof(sh.blur));
        hashBytes(&sh.color, sizeof(sh.color));
    }
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
enum class CascadeOrigin {
    UserAgentNormal = 0,
    UserNormal,
    AuthorNormal,
    InlineNormal,
    Animation,
    InlineImportant,
    AuthorImportant,
    UserImportant,
    UserAgentImportant,
    Transition
};
static bool isHighPriorityProperty(const std::string& name) {
    return name == "font-size" ||
           name == "font" ||
           name == "font-weight" ||
           name == "font-style" ||
           name == "font-family" ||
           name == "direction" ||
           name == "line-height";
}
struct StyleCascade {
    struct Entry {
        const CSSProperty* property = nullptr;
        std::string customValue;
        int specificity = 0;
        CascadeOrigin origin = CascadeOrigin::AuthorNormal;
        uint32_t sourceOrder = 0;
        int layerPriority = 0;
        bool isWorseThan(const Entry& o) const {
            if (origin != o.origin) return origin < o.origin;
            if (layerPriority != o.layerPriority) {
                bool isImportant = (origin == CascadeOrigin::AuthorImportant ||
                                    origin == CascadeOrigin::InlineImportant ||
                                    origin == CascadeOrigin::UserImportant ||
                                    origin == CascadeOrigin::UserAgentImportant);
                if (isImportant) {
                    return layerPriority > o.layerPriority;
                } else {
                    return layerPriority < o.layerPriority;
                }
            }
            if (specificity != o.specificity) return specificity < o.specificity;
            return sourceOrder < o.sourceOrder;
        }
    };
    std::unordered_map<std::string, Entry> baseProperties;
    std::unordered_map<std::string, Entry> hoverProperties;
    std::unordered_map<std::string, Entry> focusProperties;
    std::unordered_map<std::string, Entry> activeProperties;
    void add(const std::string& name, const CSSProperty* prop, const std::string& customVal, int specificity, CascadeOrigin origin, uint32_t sourceOrder, std::string_view pseudo, int layerPri = 0) {
        Entry entry{prop, customVal, specificity, origin, sourceOrder, layerPri};
        std::unordered_map<std::string, Entry>* targetMap = &baseProperties;
        if (pseudo == "hover") targetMap = &hoverProperties;
        else if (pseudo == "focus" || pseudo == "focus-visible") targetMap = &focusProperties;
        else if (pseudo == "active") targetMap = &activeProperties;
        auto it = targetMap->find(name);
        if (it == targetMap->end() || it->second.isWorseThan(entry)) {
            (*targetMap)[name] = entry;
        }
    }
};
Style StyleSheet::resolve(std::string_view className,
                          std::string_view id,
                          std::string_view type,
                          const std::vector<CSSSelectorNode>& ancestors,
                          const Style* parentStyle,
                          const Widget* widget,
                          std::string_view targetPseudo) const {
    const auto* inheritedCustomProperties = parentStyle ? &parentStyle->customProperties : nullptr;
    StyleCacheKey key = buildCacheKey(className, id, type, ancestors, parentStyle);
    // Mix widget pointer and targetPseudo into cache key to avoid
    // false cache hits for structural pseudo-classes (:nth-child etc.)
    // and pseudo-element targets (Blink StyleResolverState parity).
    if (widget) {
        auto wp = reinterpret_cast<uintptr_t>(widget);
        key.h1 ^= wp * 2654435761ULL;
        key.h2 ^= wp * 40503ULL;
    }
    if (!targetPseudo.empty()) {
        for (char c : targetPseudo) {
            key.h1 ^= static_cast<uint64_t>(c);
            key.h1 *= 1099511628211ULL;
        }
    }
#if FLUXUI_STYLE_CACHE_SIZE > 0
    {
        size_t cacheIdx = (key.h1 ^ key.h2) % FLUXUI_STYLE_CACHE_SIZE;
        if (resolvedCache_[cacheIdx].epoch == currentEpoch_ && resolvedCache_[cacheIdx].key == key && resolvedCache_[cacheIdx].style) {
            return *resolvedCache_[cacheIdx].style;
        }
    }
#endif
    Style style;
    if (parentStyle) {
        style.fontSize = parentStyle->fontSize;
    } else {
        style.fontSize = 16.0f;
    }
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
    StyleCascade cascade;
    const StyleSheet& uaSheet = getUaSheet();
    thread_local std::vector<size_t> uaCandidateRules;
    uaSheet.collectCandidateRules("", "", type, uaCandidateRules);
    for (size_t ruleIndex : uaCandidateRules) {
        if (ruleIndex >= uaSheet.rules.size()) continue;
        const auto& rule = uaSheet.rules[ruleIndex];
        std::string_view pseudo;
        if (selectorMatches(rule, "", "", type, ancestors, &pseudo, widget)) {
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
                CascadeOrigin origin = isImp ? CascadeOrigin::UserAgentImportant : CascadeOrigin::UserAgentNormal;
                cascade.add(prop.name.getString(), &prop, value, rule.specificity, origin, prop.sourceOrder, pseudo);
            }
        }
    }
    std::vector<size_t> candidateRules;
    collectCandidateRules(className, id, type, candidateRules);
    bool insideBlinkDoc = false;
    if (className.find("blink-native-doc") != std::string_view::npos) {
        insideBlinkDoc = true;
    } else {
        for (const auto& node : ancestors) {
            if (node.className.find("blink-native-doc") != std::string_view::npos) {
                insideBlinkDoc = true;
                break;
            }
        }
    }
    if (!insideBlinkDoc && widget) {
        const Widget* cur = widget;
        while (cur) {
            if (cur->className.find("blink-native-doc") != std::string::npos) {
                insideBlinkDoc = true;
                break;
            }
            cur = cur->parent;
        }
    }
    for (size_t ruleIndex : candidateRules) {
        if (ruleIndex >= rules.size()) continue;
        const auto& rule = rules[ruleIndex];
        if (insideBlinkDoc) {
            const std::string& sel = rule.selector;
            bool isAuthorRule = (sel.find('.') != std::string::npos ||
                                 sel.find('#') != std::string::npos);
            if (isAuthorRule &&
                sel.find("blink-native-doc") == std::string::npos) {
                continue;
            }
        }
        if (!mediaQueryMatches(rule.mediaQuery)) continue;
        // @scope check: if rule has a scopeRoot, verify an ancestor matches it.
        // If scopeLimit is set, reject if any ancestor between element and root matches limit.
        if (!rule.scopeRoot.empty()) {
            bool inScope = false;
            bool hitLimit = false;
            for (const auto& anc : ancestors) {
                // Check if this ancestor matches the scope root
                CSSRule rootRule;
                rootRule.selector = rule.scopeRoot;
                rootRule.selectorWithoutPseudo = rule.scopeRoot;
                splitSelectorChain(rule.scopeRoot, rootRule.parts, rootRule.combinators);
                if (selectorMatches(rootRule, anc.className, anc.id, anc.type, {}, nullptr, anc.widget)) {
                    inScope = true;
                    break;
                }
                // Check scope limit (lower boundary)
                if (!rule.scopeLimit.empty()) {
                    CSSRule limitRule;
                    limitRule.selector = rule.scopeLimit;
                    limitRule.selectorWithoutPseudo = rule.scopeLimit;
                    splitSelectorChain(rule.scopeLimit, limitRule.parts, limitRule.combinators);
                    if (selectorMatches(limitRule, anc.className, anc.id, anc.type, {}, nullptr, anc.widget)) {
                        hitLimit = true;
                        break;
                    }
                }
            }
            if (!inScope || hitLimit) continue;
        }
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
                CascadeOrigin origin = isImp ? CascadeOrigin::AuthorImportant : CascadeOrigin::AuthorNormal;
                int layerPri = getLayerPriority(rule.layer);
                cascade.add(prop.name.getString(), &prop, value, rule.specificity, origin, prop.sourceOrder, pseudo, layerPri);
            }
        }
    }
    if (widget) {
        for (const auto& prop : widget->inlineProperties) {
            std::string value = prop.value;
            bool isImp = stripImportant(value);
            CascadeOrigin origin = isImp ? CascadeOrigin::InlineImportant : CascadeOrigin::InlineNormal;
            cascade.add(prop.name.getString(), &prop, value, 1000000, origin, prop.sourceOrder, "");
        }
    }
    Style initialStyle;
    auto applyProperties = [&](const std::unordered_map<std::string, StyleCascade::Entry>& map,
                               auto mergeFn,
                               FastCustomProperties& customProperties,
                               int pass) {
        for (const auto& item : map) {
            const auto& entry = item.second;
            bool isCustom = item.first.rfind("--", 0) == 0;
            if (pass == 1) {
                if (!isCustom) continue;
            } else {
                if (isCustom) continue;
                bool isHigh = isHighPriorityProperty(item.first);
                if (pass == 2 && !isHigh) continue;
                if (pass == 3 && isHigh) continue;
            }
            bool valid = true;
            std::string value = resolveValueInternal(entry.customValue, customProperties.getMapPointer(), &valid);
            if (!valid) continue;
            if (pass == 1) {
                auto defIt = propertyDefinitions_.find(item.first);
                if (defIt != propertyDefinitions_.end()) {
                    if (!isValidSyntax(value, defIt->second.syntax)) {
                        value = defIt->second.initialValue;
                    }
                }
                customProperties[item.first] = value;
            } else {
                if (applyCSSWideProperty(style,
                                         item.first,
                                         lowerAscii(value),
                                         parentStyle,
                                         initialStyle)) {
                    continue;
                }
                mergeFn(style, item.first, value);
            }
        }
    };
    applyProperties(cascade.baseProperties, [](Style&, const std::string&, const std::string&){}, style.customProperties, 1);
    applyProperties(cascade.baseProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeProperty(target, name, value, target.fontSize);
    }, style.customProperties, 2);
    applyProperties(cascade.baseProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeProperty(target, name, value, target.fontSize);
    }, style.customProperties, 3);
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
    applyProperties(cascade.hoverProperties, [](Style&, const std::string&, const std::string&){}, hoverCustomProperties, 1);
    applyProperties(cascade.hoverProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeHoverProperty(target, name, value);
    }, hoverCustomProperties, 3);
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
    applyProperties(cascade.focusProperties, [](Style&, const std::string&, const std::string&){}, focusCustomProperties, 1);
    applyProperties(cascade.focusProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeFocusProperty(target, name, value);
    }, focusCustomProperties, 3);
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
    applyProperties(cascade.activeProperties, [](Style&, const std::string&, const std::string&){}, activeCustomProperties, 1);
    applyProperties(cascade.activeProperties, [](Style& target, const std::string& name, const std::string& value) {
        StyleSheet::mergeActiveProperty(target, name, value);
    }, activeCustomProperties, 3);
    style.hoverCustomProperties = hoverCustomProperties;
    style.focusCustomProperties = focusCustomProperties;
    style.activeCustomProperties = activeCustomProperties;
    style.inheritedHash = computeInheritedHash(style);
#if FLUXUI_STYLE_CACHE_SIZE > 0
    {
        size_t cacheIdx = (key.h1 ^ key.h2) % FLUXUI_STYLE_CACHE_SIZE;
        resolvedCache_[cacheIdx].key = key;
        resolvedCache_[cacheIdx].style = std::make_shared<const Style>(style);
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
    if (rules.empty()) {
        return;
    }
    // Reusable scratch buffers (thread-local) to avoid per-resolve heap allocation.
    thread_local std::vector<uint8_t> flagArray;
    thread_local std::vector<std::string_view> classes;
    flagArray.assign(rules.size(), 0);
    classes.clear();

    size_t minIdx = rules.size();
    size_t maxIdx = 0;
    auto markRule = [&](size_t idx) {
        if (idx < rules.size()) {
            flagArray[idx] = 1;
            if (idx < minIdx) minIdx = idx;
            if (idx > maxIdx) maxIdx = idx;
        }
    };
    auto markRules = [&](const std::vector<size_t>& rulesForKey) {
        for (size_t idx : rulesForKey) {
            markRule(idx);
        }
    };
    markRules(universalRuleIndex_);
    if (!id.empty()) {
        std::string idKey(id);
        auto it = idRuleIndex_.find(idKey);
        if (it != idRuleIndex_.end()) markRules(it->second);
    }
    appendClassTokens(className, classes);
    for (const auto& cls : classes) {
        std::string clsKey(cls);
        auto it = classRuleIndex_.find(clsKey);
        if (it != classRuleIndex_.end()) markRules(it->second);
    }
    if (!type.empty()) {
        std::string typeKey;
        std::string_view baseType = selectorBaseType(type);
        typeKey.resize(baseType.size());
        for (size_t i = 0; i < baseType.size(); ++i) {
            typeKey[i] = (char)std::tolower((unsigned char)baseType[i]);
        }
        auto it = typeRuleIndex_.find(typeKey);
        if (it != typeRuleIndex_.end()) markRules(it->second);
        std::string_view inputType = selectorAttributeValue(type, "type");
        if (baseType == "input" && !inputType.empty()) {
            typeKey.resize(inputType.size());
            for (size_t i = 0; i < inputType.size(); ++i) {
                typeKey[i] = (char)std::tolower((unsigned char)inputType[i]);
            }
            auto typedIt = typeRuleIndex_.find(typeKey);
            if (typedIt != typeRuleIndex_.end()) markRules(typedIt->second);
        }
    }
    if (minIdx <= maxIdx) {
        out.reserve(maxIdx - minIdx + 1);
        for (size_t i = minIdx; i <= maxIdx; ++i) {
            if (flagArray[i]) {
                out.push_back(i);
            }
        }
    }
}

} // namespace FluxUI
