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
static std::vector<std::string> splitWhitespaceTopLevel(std::string_view val) {
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
    if (pseudoName == "enabled") return true;
    if (pseudoName == "disabled") return false;
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
    // Detect double-colon (::) — cut position should be at the first colon.
    size_t cutPos = colon;
    if (colon > 0 && selector[colon - 1] == ':') {
        cutPos = colon - 1;
    }
    size_t nameStart = colon + 1;
    if (nameStart < selector.size() && selector[nameStart] == ':') {
        nameStart++;
    }
    std::string name = lowerAscii(trimLocal(selector.substr(nameStart)));
    // Strip trailing (…) for functional pseudo-elements like ::part(foo), ::slotted(div)
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
        // (CSS Nesting Level 1 §3 — nested group rules)
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
                    // Implicit descendant nesting (CSS Nesting §2.1)
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
    // CSS Variables Level 1 §3: "if a custom property has a cycle, all values
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

        // Find matching ')' for this var() — depth-aware
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
            // CSS Variables §3: cycle detected ? use fallback if available, else invalid.
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
                              float viewportHeight,
                              bool prefersDark,
                              bool forcedColors) {
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
            return value == (prefersDark ? "dark" : "light");
        }
        if (name == "forced-colors") {
            return value == (forcedColors ? "active" : "none");
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
            if (!parseMediaFeature(clause, viewportWidth_, viewportHeight_,
                                   prefersDark(), forcedColors())) {
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
    auto animationName              = std::move(target.animationName);
    auto animationDuration          = std::move(target.animationDuration);
    auto animationDelay             = std::move(target.animationDelay);
    auto animationIterationCount    = std::move(target.animationIterationCount);
    auto animationDirection         = std::move(target.animationDirection);
    auto animationFillMode          = std::move(target.animationFillMode);
    auto animationPlayState         = std::move(target.animationPlayState);
    auto animationTimingFunction    = std::move(target.animationTimingFunction);
    auto animationComposition       = std::move(target.animationComposition);
    auto transitionProperty         = std::move(target.transitionProperty);
    auto transitionDurations        = std::move(target.transitionDurations);
    auto transitionDelays           = std::move(target.transitionDelays);
    auto transitionTimingFunctions  = std::move(target.transitionTimingFunctions);
    auto transitionBehavior         = std::move(target.transitionBehavior);
    target = source;
    target.customProperties = std::move(customProperties);
    target.hoverCustomProperties = std::move(hoverCustomProperties);
    target.focusCustomProperties = std::move(focusCustomProperties);
    target.activeCustomProperties = std::move(activeCustomProperties);
    target.unresolvedBackgroundColor = std::move(unresolvedBackgroundColor);
    target.unresolvedColor = std::move(unresolvedColor);
    target.unresolvedBorderColor = std::move(unresolvedBorderColor);
    target.unresolvedBackgroundGradient = std::move(unresolvedBackgroundGradient);
    target.animationName              = std::move(animationName);
    target.animationDuration          = std::move(animationDuration);
    target.animationDelay             = std::move(animationDelay);
    target.animationIterationCount    = std::move(animationIterationCount);
    target.animationDirection         = std::move(animationDirection);
    target.animationFillMode          = std::move(animationFillMode);
    target.animationPlayState         = std::move(animationPlayState);
    target.animationTimingFunction    = std::move(animationTimingFunction);
    target.animationComposition       = std::move(animationComposition);
    target.transitionProperty         = std::move(transitionProperty);
    target.transitionDurations        = std::move(transitionDurations);
    target.transitionDelays           = std::move(transitionDelays);
    target.transitionTimingFunctions  = std::move(transitionTimingFunctions);
    target.transitionBehavior         = std::move(transitionBehavior);
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
        target.transitionDurations = source.transitionDurations;
        target.transitionProperty = source.transitionProperty;
        target.transitionDelays = source.transitionDelays;
        target.transitionTimingFunctions = source.transitionTimingFunctions;
        target.transitionBehavior = source.transitionBehavior;
        target.transitionDuration = source.transitionDuration;
    } else if (name == "transition-property") {
        target.transitionProperty = source.transitionProperty;
    } else if (name == "transition-duration") {
        target.transitionDurations = source.transitionDurations;
        target.transitionDuration = source.transitionDuration;
    } else if (name == "transition-delay") {
        target.transitionDelays = source.transitionDelays;
    } else if (name == "transition-timing-function") {
        target.transitionTimingFunctions = source.transitionTimingFunctions;
    } else if (name == "transition-behavior") {
        target.transitionBehavior = source.transitionBehavior;
    } else if (name == "animation") {
        target.animationName = source.animationName;
        target.animationDuration = source.animationDuration;
        target.animationDelay = source.animationDelay;
        target.animationIterationCount = source.animationIterationCount;
        target.animationDirection = source.animationDirection;
        target.animationFillMode = source.animationFillMode;
        target.animationPlayState = source.animationPlayState;
        target.animationTimingFunction = source.animationTimingFunction;
        target.animationComposition = source.animationComposition;
    } else if (name == "animation-name") {
        target.animationName = source.animationName;
    } else if (name == "animation-duration") {
        target.animationDuration = source.animationDuration;
    } else if (name == "animation-delay") {
        target.animationDelay = source.animationDelay;
    } else if (name == "animation-iteration-count") {
        target.animationIterationCount = source.animationIterationCount;
    } else if (name == "animation-direction") {
        target.animationDirection = source.animationDirection;
    } else if (name == "animation-fill-mode") {
        target.animationFillMode = source.animationFillMode;
    } else if (name == "animation-play-state") {
        target.animationPlayState = source.animationPlayState;
    } else if (name == "animation-timing-function") {
        target.animationTimingFunction = source.animationTimingFunction;
    } else if (name == "animation-composition") {
        target.animationComposition = source.animationComposition;
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
const StyleSheet& StyleSheet::getUaSheet() {
    static StyleSheet uaSheet;
    static bool initialized = false;
    if (!initialized) {
        uaSheet.parse(R"CSS(
            html { display: block; }
            body { display: block; margin: 8px; }
            p { display: block; margin-block-start: 1em; margin-block-end: 1em; }
            div, article, aside, footer, header, main, nav, section, address, fieldset, details, summary, dialog { display: block; }
            blockquote { display: block; margin-block-start: 1em; margin-block-end: 1em; margin-inline-start: 40px; margin-inline-end: 40px; }
            figure { display: block; margin-block-start: 1em; margin-block-end: 1em; margin-inline-start: 40px; margin-inline-end: 40px; }
            figcaption, dt, form, layer, hgroup, search, frameset, frame { display: block; }
            address { font-style: italic; }
            dl { display: block; margin-block-start: 1em; margin-block-end: 1em; }
            dd { display: block; margin-inline-start: 40px; }
            center { display: block; text-align: center; }
            hr { display: block; overflow: hidden; margin-block-start: 0.5em; margin-block-end: 0.5em; border: 1px solid rgb(128, 128, 128); }
            h1 { display: block; font-size: 2em; margin-block-start: 0.67em; margin-block-end: 0.67em; font-weight: bold; }
            h2 { display: block; font-size: 1.5em; margin-block-start: 0.83em; margin-block-end: 0.83em; font-weight: bold; }
            h3 { display: block; font-size: 1.17em; margin-block-start: 1em; margin-block-end: 1em; font-weight: bold; }
            h4 { display: block; font-size: 1em; margin-block-start: 1.33em; margin-block-end: 1.33em; font-weight: bold; }
            h5 { display: block; font-size: 0.83em; margin-block-start: 1.67em; margin-block-end: 1.67em; font-weight: bold; }
            h6 { display: block; font-size: 0.67em; margin-block-start: 2.33em; margin-block-end: 2.33em; font-weight: bold; }
            article h1, aside h1, nav h1, section h1 { font-size: 1.5em; margin-block-start: 0.83em; margin-block-end: 0.83em; }
            article article h1, article aside h1, article nav h1, article section h1,
            aside article h1, aside aside h1, aside nav h1, aside section h1,
            nav article h1, nav aside h1, nav nav h1, nav section h1,
            section article h1, section aside h1, section nav h1, section section h1 { font-size: 1.17em; margin-block-start: 1em; margin-block-end: 1em; }
            ul, ol, menu, dir { display: block; margin-block-start: 1em; margin-block-end: 1em; padding-inline-start: 40px; }
            ol { list-style-type: decimal; }
            ul { list-style-type: disc; }
            ul ul, ol ul { list-style-type: circle; }
            ul ul ul, ol ul ul, ul ol ul, ol ol ul { list-style-type: square; }
            li { display: list-item; }
            table { display: table; border: 0px solid rgb(128, 128, 128); }
            thead { display: table-header-group; vertical-align: middle; }
            tbody { display: table-row-group; vertical-align: middle; }
            tfoot { display: table-footer-group; vertical-align: middle; }
            tr { display: table-row; vertical-align: middle; }
            td, th { display: table-cell; vertical-align: middle; }
            th { font-weight: bold; text-align: center; }
            caption { display: table-caption; text-align: center; }
            col { display: table-column; }
            colgroup { display: table-column-group; }
            strong, b { display: inline; font-weight: bold; }
            a { display: inline; color: rgb(0, 0, 238); text-decoration: underline; cursor: pointer; }
            u, ins { display: inline; text-decoration: underline; }
            s, strike, del { display: inline; text-decoration: line-through; }
            mark { display: inline; background-color: rgb(255, 255, 0); color: rgb(0, 0, 0); }
            big { display: inline; font-size: 19.2px; }
            i, cite, em, var, dfn { display: inline; font-style: italic; }
            sub { display: inline; vertical-align: sub; font-size: 13.333px; }
            sup { display: inline; vertical-align: super; font-size: 13.333px; }
            small { display: inline; font-size: 13.333px; }
            pre, xmp, plaintext, listing { display: block; margin-block-start: 1em; margin-block-end: 1em; font-family: monospace; white-space: pre; }
            code, kbd, samp, tt { display: inline; font-family: monospace; }
            nobr { display: inline; white-space: nowrap; }
            img, svg, picture { display: inline-block; object-fit: fill; }
            canvas { display: inline-block; width: 300px; height: 150px; }
            video { display: inline-block; object-fit: contain; }
            rp, noframes { display: none; }
            span, q, map, area, abbr, acronym, bdi, bdo, data, time, output, rb, rtc, ruby, audio, embed, iframe, object { display: inline; }
            iframe { border: 2px solid rgb(118, 118, 118); }
            bdi, output { unicode-bidi: isolate; }
            bdo { unicode-bidi: bidi-override; }
            slot { display: contents; }
            rt { display: block; font-size: 8px; }
            label { display: inline; cursor: default; }
            fieldset { display: block; margin: 0px 2px; padding: 5.6px 12px 10px 12px; border: 2px solid rgb(192, 192, 192); min-width: 0px; }
            legend { display: block; padding: 0px 2px; }
            input[type="hidden"] {
                display: none !important;
                appearance: auto;
                cursor: default;
                padding: 0px;
                border: 0px solid rgba(0, 0, 0, 0);
                background-color: rgba(0, 0, 0, 0);
                width: 0px;
                height: 0px;
            }
            input[type="file"] {
                display: inline-block;
                appearance: auto;
                cursor: default;
                padding: 0px;
                border: 0px solid rgba(0, 0, 0, 0);
                background-color: rgba(0, 0, 0, 0);
                width: 253px;
                height: 21px;
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="image"] {
                display: inline-block;
                appearance: auto;
                cursor: pointer;
                padding: 0px;
                border: 0px solid rgba(0, 0, 0, 0);
                background-color: rgba(0, 0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="color"] {
                display: inline-block;
                appearance: square-button;
                cursor: default;
                width: 44px;
                height: 23px;
                padding: 1px 2px;
                border: 1px solid rgb(169, 169, 169);
                background-color: rgb(239, 239, 239);
                box-sizing: border-box;
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="text"], input[type="password"], input[type="search"], input[type="number"], input[type="email"], input[type="url"], input[type="tel"], input[type="date"], input[type="time"], input[type="month"], input[type="week"], input[type="datetime-local"] {
                display: inline-block;
                appearance: textfield;
                cursor: text;
                padding: 1px 2px;
                border: 2px solid rgb(118, 118, 118);
                border-radius: 2px;
                background-color: rgb(255, 255, 255);
                color: rgb(0, 0, 0);
                width: 170px;
                box-sizing: border-box;
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="search"] {
                appearance: searchfield;
            }
            textarea {
                display: inline-block;
                appearance: auto;
                cursor: text;
                overflow: auto;
                white-space: pre-wrap;
                word-break: break-word;
                font-family: monospace;
                padding: 2px;
                border: 1px solid rgb(118, 118, 118);
                background-color: rgb(255, 255, 255);
                color: rgb(0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            select {
                display: inline-block;
                appearance: menulist;
                cursor: default;
                padding: 1px 22px 1px 4px;
                border: 1px solid rgb(0, 0, 0);
                border-radius: 5px;
                background-color: rgb(255, 255, 255);
                color: rgb(0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            option {
                display: none;
                padding: 0px 2px 1px 2px;
                white-space: pre;
                color: rgb(0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="checkbox"] {
                display: inline-block;
                appearance: checkbox;
                cursor: default;
                margin: 3px 4px 3px 4px;
                width: 13px;
                height: 13px;
                border: 1px solid rgb(118, 118, 118);
                background-color: rgb(255, 255, 255);
                border-radius: 0px;
                padding: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="radio"] {
                display: inline-block;
                appearance: radio;
                cursor: default;
                margin: 3px 4px 3px 4px;
                width: 13px;
                height: 13px;
                border: 1px solid rgb(118, 118, 118);
                background-color: rgb(255, 255, 255);
                border-radius: 50%;
                padding: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="range"] {
                display: inline-block;
                appearance: slider-horizontal;
                cursor: default;
                margin: 2px;
                width: 129px;
                height: 16px;
                border: none;
                color: rgb(144, 144, 144);
                padding: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            button, input[type="button"], input[type="submit"], input[type="reset"] {
                display: inline-block;
                appearance: button;
                cursor: default;
                box-sizing: border-box;
                background-color: rgb(239, 239, 239);
                color: rgb(0, 0, 0);
                border: 2px solid rgb(118, 118, 118);
                border-radius: 2px;
                padding: 2px 6px 3px 6px;
                font-family: system-ui;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: center;
            }
            details { display: block; }
            summary { display: block; padding-left: 20px; cursor: pointer; }
            dialog { display: none; position: absolute; background-color: rgb(255, 255, 255); color: rgb(0, 0, 0); border: 1px solid rgb(0, 0, 0); padding: 20px; }
            meter { display: inline-block; width: 80px; height: 16px; background-color: rgb(229, 229, 229); border: 1px solid rgb(179, 179, 179); border-radius: 4px; }
            progress { display: inline-block; width: 160px; height: 16px; background-color: rgba(26, 26, 26, 0.1); border: 1px solid rgba(76, 76, 76, 0.3); border-radius: 8px; }
            br { display: block; width: 100%; height: 0px; }
            [dir="rtl"] { direction: rtl; unicode-bidi: isolate; }
            [dir="ltr"] { direction: ltr; unicode-bidi: isolate; }
        )CSS");
        initialized = true;
    }
    return uaSheet;
}
void StyleSheet::applyUserAgentDefaults(Style& style,
                                        std::string_view type,
                                        const std::vector<CSSSelectorNode>& ancestors,
                                        const Widget* widget) {
    const StyleSheet& uaSheet = getUaSheet();
    std::vector<size_t> candidateRules;
    uaSheet.collectCandidateRules("", "", type, candidateRules);
    struct WinningProperty {
        const CSSProperty* property = nullptr;
        int specificity = 0;
        int sourceOrder = 0;
        bool isWorseThan(const WinningProperty& o) const {
            if (specificity != o.specificity) return specificity < o.specificity;
            return sourceOrder < o.sourceOrder;
        }
    };
    std::unordered_map<std::string, WinningProperty> winningProperties;
    for (size_t ruleIndex : candidateRules) {
        if (ruleIndex >= uaSheet.rules.size()) continue;
        const auto& rule = uaSheet.rules[ruleIndex];
        std::string_view pseudo;
        if (selectorMatches(rule, "", "", type, ancestors, &pseudo, widget)) {
            for (const auto& prop : rule.properties) {
                WinningProperty wp{&prop, rule.specificity, (int)prop.sourceOrder};
                auto it = winningProperties.find(prop.name.getString());
                if (it == winningProperties.end() || it->second.isWorseThan(wp)) {
                    winningProperties[prop.name.getString()] = wp;
                }
            }
        }
    }
    for (const auto& entry : winningProperties) {
        StyleSheet::mergeProperty(style, entry.first, entry.second.property->value, style.fontSize);
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
    float value = parseLocaleIndependentFloat(token.c_str(), &end);
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
    if (mergePropertyPart1(style, name, value, emBase)) return;
    mergePropertyPart2(style, name, value, emBase);
    mergePropertyPart3(style, name, value, emBase);
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
    } else if (name == "filter") {
        if (value == "none") {
            style.filterOperations.clear();
            style.hasFilter = false;
        } else {
            style.filterOperations = parseFilterOperations(value, emBase);
            style.hasFilter = !style.filterOperations.empty();
        }
    } else if (name == "backdrop-filter") {
        if (value == "none") {
            style.backdropFilterOperations.clear();
            style.hasBackdropFilter = false;
            style.hasBackdropFilterBlur = false;
            style.backdropFilterBlur = 0.0f;
        } else {
            style.backdropFilterOperations = parseFilterOperations(value, emBase);
            style.hasBackdropFilter = !style.backdropFilterOperations.empty();
            // Sync the legacy scalar for the renderer drawBackdropFilterBlur() path.
            style.backdropFilterBlur = 0.0f;
            style.hasBackdropFilterBlur = false;
            for (const auto& op : style.backdropFilterOperations) {
                if (op.type == FilterOperationType::Blur) {
                    style.backdropFilterBlur = op.amount;
                    style.hasBackdropFilterBlur = true;
                    break;
                }
            }
        }
    } else if (name == "clip-path") {
        style.clipPath = value;
        style.hasClipPath = (value != "none" && !value.empty());
    } else if (name == "shape-outside") {
        style.shapeOutside = value;
        style.hasShapeOutside = (value != "none" && !value.empty());
    } else if (name == "mask" || name == "mask-image") {
        style.maskImage = value;
        style.hasMask = (value != "none" && !value.empty());
    } else if (name == "mask-mode") {
        style.maskMode = value;
    } else if (name == "mask-repeat") {
        style.maskRepeat = value;
    } else if (name == "mask-position") {
        style.maskPosition = value;
    } else if (name == "mask-size") {
        style.maskSize = value;
    } else if (name == "mask-clip") {
        style.maskClip = value;
    } else if (name == "mask-origin") {
        style.maskOrigin = value;
    } else if (name == "mask-composite") {
        style.maskComposite = value;
    } else if (name == "mix-blend-mode") {
        std::string v = lowerAscii(trim(value));
        style.hasMixBlendMode = true;
        if (v == "multiply")        style.mixBlendMode = Style::BlendMode::Multiply;
        else if (v == "screen")     style.mixBlendMode = Style::BlendMode::Screen;
        else if (v == "overlay")    style.mixBlendMode = Style::BlendMode::Overlay;
        else if (v == "darken")     style.mixBlendMode = Style::BlendMode::Darken;
        else if (v == "lighten")    style.mixBlendMode = Style::BlendMode::Lighten;
        else if (v == "color-dodge")style.mixBlendMode = Style::BlendMode::ColorDodge;
        else if (v == "color-burn") style.mixBlendMode = Style::BlendMode::ColorBurn;
        else if (v == "hard-light") style.mixBlendMode = Style::BlendMode::HardLight;
        else if (v == "soft-light") style.mixBlendMode = Style::BlendMode::SoftLight;
        else if (v == "difference") style.mixBlendMode = Style::BlendMode::Difference;
        else if (v == "exclusion")  style.mixBlendMode = Style::BlendMode::Exclusion;
        else if (v == "hue")        style.mixBlendMode = Style::BlendMode::Hue;
        else if (v == "saturation") style.mixBlendMode = Style::BlendMode::Saturation;
        else if (v == "color")      style.mixBlendMode = Style::BlendMode::Color;
        else if (v == "luminosity") style.mixBlendMode = Style::BlendMode::Luminosity;
        else { style.mixBlendMode = Style::BlendMode::Normal; style.hasMixBlendMode = (v != "normal"); }
    } else if (name == "isolation") {
        std::string v = lowerAscii(trim(value));
        style.hasIsolation = true;
        style.isolation = (v == "isolate") ? Style::Isolation::Isolate : Style::Isolation::Auto;
    } else if (name == "background-blend-mode") {
        std::string v = lowerAscii(trim(value));
        style.hasBackgroundBlendMode = true;
        if (v == "multiply")        style.backgroundBlendMode = Style::BlendMode::Multiply;
        else if (v == "screen")     style.backgroundBlendMode = Style::BlendMode::Screen;
        else if (v == "overlay")    style.backgroundBlendMode = Style::BlendMode::Overlay;
        else if (v == "darken")     style.backgroundBlendMode = Style::BlendMode::Darken;
        else if (v == "lighten")    style.backgroundBlendMode = Style::BlendMode::Lighten;
        else if (v == "color-dodge")style.backgroundBlendMode = Style::BlendMode::ColorDodge;
        else if (v == "color-burn") style.backgroundBlendMode = Style::BlendMode::ColorBurn;
        else if (v == "hard-light") style.backgroundBlendMode = Style::BlendMode::HardLight;
        else if (v == "soft-light") style.backgroundBlendMode = Style::BlendMode::SoftLight;
        else if (v == "difference") style.backgroundBlendMode = Style::BlendMode::Difference;
        else if (v == "exclusion")  style.backgroundBlendMode = Style::BlendMode::Exclusion;
        else if (v == "hue")        style.backgroundBlendMode = Style::BlendMode::Hue;
        else if (v == "saturation") style.backgroundBlendMode = Style::BlendMode::Saturation;
        else if (v == "color")      style.backgroundBlendMode = Style::BlendMode::Color;
        else if (v == "luminosity") style.backgroundBlendMode = Style::BlendMode::Luminosity;
        else                        style.backgroundBlendMode = Style::BlendMode::Normal;
    } else if (name == "scroll-snap-type") {
        style.scrollSnapType = value;
        style.hasScrollSnapType = (value != "none" && !value.empty());
    } else if (name == "scroll-snap-align") {
        style.scrollSnapAlign = value;
        style.hasScrollSnapAlign = (value != "none" && !value.empty());
    } else if (name == "scroll-snap-stop") {
        style.scrollSnapStop = lowerAscii(trim(value));
    } else if (name == "scroll-padding") {
        style.scrollPadding = parseEdgeInsets(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-top") {
        style.scrollPadding.top = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-right") {
        style.scrollPadding.right = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-bottom") {
        style.scrollPadding.bottom = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-padding-left") {
        style.scrollPadding.left = parseLengthPixels(value, emBase);
        style.hasScrollPadding = true;
    } else if (name == "scroll-margin") {
        style.scrollMargin = parseEdgeInsets(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-top") {
        style.scrollMargin.top = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-right") {
        style.scrollMargin.right = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-bottom") {
        style.scrollMargin.bottom = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "scroll-margin-left") {
        style.scrollMargin.left = parseLengthPixels(value, emBase);
        style.hasScrollMargin = true;
    } else if (name == "overscroll-behavior") {
        std::string v = lowerAscii(trim(value));
        style.hasOverscrollBehavior = true;
        auto parseOB = [](const std::string& s) -> Style::OverscrollBehavior {
            if (s == "contain") return Style::OverscrollBehavior::Contain;
            if (s == "none") return Style::OverscrollBehavior::None;
            return Style::OverscrollBehavior::Auto;
        };
        std::istringstream iss(v); std::string t1, t2;
        iss >> t1; iss >> t2;
        style.overscrollBehaviorX = parseOB(t1);
        style.overscrollBehaviorY = t2.empty() ? style.overscrollBehaviorX : parseOB(t2);
    } else if (name == "overscroll-behavior-x") {
        std::string v = lowerAscii(trim(value));
        style.hasOverscrollBehavior = true;
        if (v == "contain") style.overscrollBehaviorX = Style::OverscrollBehavior::Contain;
        else if (v == "none") style.overscrollBehaviorX = Style::OverscrollBehavior::None;
        else style.overscrollBehaviorX = Style::OverscrollBehavior::Auto;
    } else if (name == "overscroll-behavior-y") {
        std::string v = lowerAscii(trim(value));
        style.hasOverscrollBehavior = true;
        if (v == "contain") style.overscrollBehaviorY = Style::OverscrollBehavior::Contain;
        else if (v == "none") style.overscrollBehaviorY = Style::OverscrollBehavior::None;
        else style.overscrollBehaviorY = Style::OverscrollBehavior::Auto;
    } else if (name == "scrollbar-color") {
        std::string v = trim(value);
        if (lowerAscii(v) == "auto") {
            style.hasScrollbarColor = false;
        } else {
            // scrollbar-color: <thumb-color> <track-color>
            auto tokens = splitColorTokens(functionInner("(" + v + ")"));
            if (tokens.size() >= 2) {
                style.scrollbarThumbColor = parseColor(tokens[0]);
                style.scrollbarTrackColor = parseColor(tokens[1]);
                style.hasScrollbarColor = true;
            } else if (tokens.size() == 1) {
                style.scrollbarThumbColor = parseColor(tokens[0]);
                style.scrollbarTrackColor = parseColor(tokens[0]);
                style.hasScrollbarColor = true;
            }
        }
    } else if (name == "scrollbar-width") {
        std::string v = lowerAscii(trim(value));
        style.hasScrollbarWidth = true;
        if (v == "thin") style.scrollbarWidth = Style::ScrollbarWidth::Thin;
        else if (v == "none") style.scrollbarWidth = Style::ScrollbarWidth::None;
        else style.scrollbarWidth = Style::ScrollbarWidth::Auto;
    } else if (name == "overflow-anchor") {
        std::string v = lowerAscii(trim(value));
        style.hasOverflowAnchor = true;
        style.overflowAnchor = (v == "none") ? Style::OverflowAnchor::None : Style::OverflowAnchor::Auto;
    } else if (name == "scrollbar-gutter") {
        style.scrollbarGutter = lowerAscii(trim(value));
        style.hasScrollbarGutter = !style.scrollbarGutter.empty() && style.scrollbarGutter != "auto";
    } else if (name == "scroll-behavior") {
        std::string v = lowerAscii(trim(value));
        style.hasScrollBehavior = true;
        style.scrollBehavior = (v == "smooth") ? Style::ScrollBehavior::Smooth : Style::ScrollBehavior::Auto;
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
        Border b = parseBorder(value, emBase);
        style.borderT = b; style.hasBorderT = true; style.orderBorderTop = ++style.propertyOrder;
        style.borderR = b; style.hasBorderR = true; style.orderBorderRight = ++style.propertyOrder;
        style.borderB = b; style.hasBorderB = true; style.orderBorderBottom = ++style.propertyOrder;
        style.borderL = b; style.hasBorderL = true; style.orderBorderLeft = ++style.propertyOrder;
        style.border = b;
    } else if (name == "border-top") {
        style.borderT = parseBorder(value, emBase);
        style.hasBorderT = true;
        style.orderBorderTop = ++style.propertyOrder;
    } else if (name == "border-right") {
        style.borderR = parseBorder(value, emBase);
        style.hasBorderR = true;
        style.orderBorderRight = ++style.propertyOrder;
    } else if (name == "border-bottom") {
        style.borderB = parseBorder(value, emBase);
        style.hasBorderB = true;
        style.orderBorderBottom = ++style.propertyOrder;
    } else if (name == "border-left") {
        style.borderL = parseBorder(value, emBase);
        style.hasBorderL = true;
        style.orderBorderLeft = ++style.propertyOrder;
    } else if (name == "border-block-start") {
        style.borderBlockStart = parseBorder(value, emBase);
        style.hasBorderBlockStart = true;
        style.orderBorderBlockStart = ++style.propertyOrder;
    } else if (name == "border-block-end") {
        style.borderBlockEnd = parseBorder(value, emBase);
        style.hasBorderBlockEnd = true;
        style.orderBorderBlockEnd = ++style.propertyOrder;
    } else if (name == "border-inline-start") {
        style.borderInlineStart = parseBorder(value, emBase);
        style.hasBorderInlineStart = true;
        style.orderBorderInlineStart = ++style.propertyOrder;
    } else if (name == "border-inline-end") {
        style.borderInlineEnd = parseBorder(value, emBase);
        style.hasBorderInlineEnd = true;
        style.orderBorderInlineEnd = ++style.propertyOrder;
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
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.paddingT = inset.top; style.hasPaddingT = true; style.orderPaddingTop = ++style.propertyOrder;
        style.paddingR = inset.right; style.hasPaddingR = true; style.orderPaddingRight = ++style.propertyOrder;
        style.paddingB = inset.bottom; style.hasPaddingB = true; style.orderPaddingBottom = ++style.propertyOrder;
        style.paddingL = inset.left; style.hasPaddingL = true; style.orderPaddingLeft = ++style.propertyOrder;
    } else if (name == "padding-top") {
        style.paddingT = parseLengthPixels(value, emBase);
        style.hasPaddingT = true;
        style.orderPaddingTop = ++style.propertyOrder;
    } else if (name == "padding-right") {
        style.paddingR = parseLengthPixels(value, emBase);
        style.hasPaddingR = true;
        style.orderPaddingRight = ++style.propertyOrder;
    } else if (name == "padding-bottom") {
        style.paddingB = parseLengthPixels(value, emBase);
        style.hasPaddingB = true;
        style.orderPaddingBottom = ++style.propertyOrder;
    } else if (name == "padding-left") {
        style.paddingL = parseLengthPixels(value, emBase);
        style.hasPaddingL = true;
        style.orderPaddingLeft = ++style.propertyOrder;
    } else if (name == "padding-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.paddingBlockStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.paddingBlockEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.paddingBlockStart;
        style.hasPaddingBlockStart = count > 0;
        style.hasPaddingBlockEnd = count > 0;
        style.orderPaddingBlockStart = ++style.propertyOrder;
        style.orderPaddingBlockEnd = ++style.propertyOrder;
    } else if (name == "padding-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.paddingInlineStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.paddingInlineEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.paddingInlineStart;
        style.hasPaddingInlineStart = count > 0;
        style.hasPaddingInlineEnd = count > 0;
        style.orderPaddingInlineStart = ++style.propertyOrder;
        style.orderPaddingInlineEnd = ++style.propertyOrder;
    } else if (name == "padding-block-start") {
        style.paddingBlockStart = parseLengthPixels(value, emBase);
        style.hasPaddingBlockStart = true;
        style.orderPaddingBlockStart = ++style.propertyOrder;
    } else if (name == "padding-block-end") {
        style.paddingBlockEnd = parseLengthPixels(value, emBase);
        style.hasPaddingBlockEnd = true;
        style.orderPaddingBlockEnd = ++style.propertyOrder;
    } else if (name == "padding-inline-start") {
        style.paddingInlineStart = parseLengthPixels(value, emBase);
        style.hasPaddingInlineStart = true;
        style.orderPaddingInlineStart = ++style.propertyOrder;
    } else if (name == "padding-inline-end") {
        style.paddingInlineEnd = parseLengthPixels(value, emBase);
        style.hasPaddingInlineEnd = true;
        style.orderPaddingInlineEnd = ++style.propertyOrder;
    } else if (name == "margin") {
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.marginT = inset.top; style.hasMarginT = true; style.orderMarginTop = ++style.propertyOrder;
        style.marginR = inset.right; style.hasMarginR = true; style.orderMarginRight = ++style.propertyOrder;
        style.marginB = inset.bottom; style.hasMarginB = true; style.orderMarginBottom = ++style.propertyOrder;
        style.marginL = inset.left; style.hasMarginL = true; style.orderMarginLeft = ++style.propertyOrder;
    } else if (name == "margin-top") {
        style.marginT = parseLengthPixels(value, emBase);
        style.hasMarginT = true;
        style.orderMarginTop = ++style.propertyOrder;
    } else if (name == "margin-right") {
        style.marginR = parseLengthPixels(value, emBase);
        style.hasMarginR = true;
        style.orderMarginRight = ++style.propertyOrder;
    } else if (name == "margin-bottom") {
        style.marginB = parseLengthPixels(value, emBase);
        style.hasMarginB = true;
        style.orderMarginBottom = ++style.propertyOrder;
    } else if (name == "margin-left") {
        style.marginL = parseLengthPixels(value, emBase);
        style.hasMarginL = true;
        style.orderMarginLeft = ++style.propertyOrder;
    } else if (name == "margin-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.marginBlockStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.marginBlockEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.marginBlockStart;
        style.hasMarginBlockStart = count > 0;
        style.hasMarginBlockEnd = count > 0;
        style.orderMarginBlockStart = ++style.propertyOrder;
        style.orderMarginBlockEnd = ++style.propertyOrder;
    } else if (name == "margin-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.marginInlineStart = count > 0 ? parseLengthPixels(std::string(tokens[0]), emBase) : 0.0f;
        style.marginInlineEnd = count > 1 ? parseLengthPixels(std::string(tokens[1]), emBase) : style.marginInlineStart;
        style.hasMarginInlineStart = count > 0;
        style.hasMarginInlineEnd = count > 0;
        style.orderMarginInlineStart = ++style.propertyOrder;
        style.orderMarginInlineEnd = ++style.propertyOrder;
    } else if (name == "margin-block-start") {
        style.marginBlockStart = parseLengthPixels(value, emBase);
        style.hasMarginBlockStart = true;
        style.orderMarginBlockStart = ++style.propertyOrder;
    } else if (name == "margin-block-end") {
        style.marginBlockEnd = parseLengthPixels(value, emBase);
        style.hasMarginBlockEnd = true;
        style.orderMarginBlockEnd = ++style.propertyOrder;
    } else if (name == "margin-inline-start") {
        style.marginInlineStart = parseLengthPixels(value, emBase);
        style.hasMarginInlineStart = true;
        style.orderMarginInlineStart = ++style.propertyOrder;
    } else if (name == "margin-inline-end") {
        style.marginInlineEnd = parseLengthPixels(value, emBase);
        style.hasMarginInlineEnd = true;
        style.orderMarginInlineEnd = ++style.propertyOrder;
    } else if (name == "inset") {
        EdgeInsets inset = parseEdgeInsets(value, emBase);
        style.topVal = CSSValue::px(inset.top); style.hasTopVal = true; style.orderTop = ++style.propertyOrder;
        style.rightVal = CSSValue::px(inset.right); style.hasRightVal = true; style.orderRight = ++style.propertyOrder;
        style.bottomVal = CSSValue::px(inset.bottom); style.hasBottomVal = true; style.orderBottom = ++style.propertyOrder;
        style.leftVal = CSSValue::px(inset.left); style.hasLeftVal = true; style.orderLeft = ++style.propertyOrder;
    } else if (name == "inset-block") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.insetBlockStart = count > 0 ? parseCSSValue(std::string(tokens[0])) : CSSValue();
        style.insetBlockEnd = count > 1 ? parseCSSValue(std::string(tokens[1])) : style.insetBlockStart;
        style.hasInsetBlockStart = count > 0;
        style.hasInsetBlockEnd = count > 0;
        style.orderInsetBlockStart = ++style.propertyOrder;
        style.orderInsetBlockEnd = ++style.propertyOrder;
    } else if (name == "inset-inline") {
        std::string_view tokens[4];
        int count = 0;
        splitWhitespace(value, tokens, 4, count);
        style.insetInlineStart = count > 0 ? parseCSSValue(std::string(tokens[0])) : CSSValue();
        style.insetInlineEnd = count > 1 ? parseCSSValue(std::string(tokens[1])) : style.insetInlineStart;
        style.hasInsetInlineStart = count > 0;
        style.hasInsetInlineEnd = count > 0;
        style.orderInsetInlineStart = ++style.propertyOrder;
        style.orderInsetInlineEnd = ++style.propertyOrder;
    } else if (name == "inset-block-start") {
        style.insetBlockStart = parseCSSValue(value);
        style.hasInsetBlockStart = true;
        style.orderInsetBlockStart = ++style.propertyOrder;
    } else if (name == "inset-block-end") {
        style.insetBlockEnd = parseCSSValue(value);
        style.hasInsetBlockEnd = true;
        style.orderInsetBlockEnd = ++style.propertyOrder;
    } else if (name == "inset-inline-start") {
        style.insetInlineStart = parseCSSValue(value);
        style.hasInsetInlineStart = true;
        style.orderInsetInlineStart = ++style.propertyOrder;
    } else if (name == "inset-inline-end") {
        style.insetInlineEnd = parseCSSValue(value);
        style.hasInsetInlineEnd = true;
        style.orderInsetInlineEnd = ++style.propertyOrder;
    } else if (name == "width") {
        style.width = parseCSSValue(value);
        style.hasWidthVal = true;
        style.orderWidth = ++style.propertyOrder;
    } else if (name == "inline-size") {
        style.inlineSize = parseCSSValue(value);
        style.hasInlineSize = true;
        style.orderInlineSize = ++style.propertyOrder;
    } else if (name == "height") {
        style.height = parseCSSValue(value);
        style.hasHeightVal = true;
        style.orderHeight = ++style.propertyOrder;
    } else if (name == "block-size") {
        style.blockSize = parseCSSValue(value);
        style.hasBlockSize = true;
        style.orderBlockSize = ++style.propertyOrder;
    } else if (name == "min-width") {
        style.minWidth = parseCSSValue(value);
        style.hasMinWidthVal = true;
        style.orderMinWidth = ++style.propertyOrder;
    } else if (name == "min-inline-size") {
        style.minInlineSize = parseCSSValue(value);
        style.hasMinInlineSize = true;
        style.orderMinInlineSize = ++style.propertyOrder;
    } else if (name == "min-height") {
        style.minHeight = parseCSSValue(value);
        style.hasMinHeightVal = true;
        style.orderMinHeight = ++style.propertyOrder;
    } else if (name == "min-block-size") {
        style.minBlockSize = parseCSSValue(value);
        style.hasMinBlockSize = true;
        style.orderMinBlockSize = ++style.propertyOrder;
    } else if (name == "max-width") {
        style.maxWidth = parseCSSValue(value);
        style.hasMaxWidthVal = true;
        style.orderMaxWidth = ++style.propertyOrder;
    } else if (name == "max-inline-size") {
        style.maxInlineSize = parseCSSValue(value);
        style.hasMaxInlineSize = true;
        style.orderMaxInlineSize = ++style.propertyOrder;
    } else if (name == "max-height") {
        style.maxHeight = parseCSSValue(value);
        style.hasMaxHeightVal = true;
        style.orderMaxHeight = ++style.propertyOrder;
    } else if (name == "max-block-size") {
        style.maxBlockSize = parseCSSValue(value);
        style.hasMaxBlockSize = true;
        style.orderMaxBlockSize = ++style.propertyOrder;
    } else {
        return false; // not matched in Part1 — caller proceeds to Part2/Part3
    }
    return true;
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
    } else if (name == "contain") {
        uint8_t flags = 0;
        if (value == "none") {
            style.contain = kContainNone;
        } else if (value == "strict") {
            style.contain = kContainStrict;
        } else if (value == "content") {
            style.contain = kContainContent;
        } else {
            std::vector<std::string> tokens;
            size_t pos = 0;
            while (pos < value.size()) {
                while (pos < value.size() && std::isspace((unsigned char)value[pos])) pos++;
                if (pos >= value.size()) break;
                size_t start = pos;
                while (pos < value.size() && !std::isspace((unsigned char)value[pos])) pos++;
                tokens.push_back(value.substr(start, pos - start));
            }
            for (const auto& tok : tokens) {
                if (tok == "size") flags |= (uint8_t)kContainSize;
                else if (tok == "layout") flags |= (uint8_t)kContainLayout;
                else if (tok == "paint") flags |= (uint8_t)kContainPaint;
                else if (tok == "style") flags |= (uint8_t)kContainStyle;
            }
            style.contain = static_cast<ContainmentFlags>(flags);
        }
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
    } else if (name == "column-count") {
        std::string v = trim(value);
        if (v == "auto") style.columnCount = 0;
        else style.columnCount = (int)parseFloat(v);
    } else if (name == "column-width") {
        std::string v = trim(value);
        if (v == "auto") style.columnWidth = 0.0f;
        else style.columnWidth = parseLengthPixels(v, emBase);
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
        auto parts = splitTopLevel(value, ',');
        std::vector<std::string> props;
        std::vector<float> durations;
        std::vector<float> delays;
        std::vector<TimingFunction> tfs;
        std::vector<TransitionBehavior> behaviors;
        for (const auto& part : parts) {
            std::string p = trim(part);
            if (p.empty()) continue;
            std::string property = "all";
            float dur = 0.0f;
            float del = 0.0f;
            TimingFunction tf = TimingFunction::ease();
            TransitionBehavior behavior = TransitionBehavior::Normal;
            int timeSlots = 0;
            auto toks = splitWhitespaceTopLevel(p);
            for (const auto& tok : toks) {
                if (tok.empty()) continue;
                std::string tl = tok;
                for (char& c : tl) c = (char)std::tolower((unsigned char)c);
                bool isMs = tl.size() > 2 && tl.substr(tl.size() - 2) == "ms";
                bool isSec = tl.size() > 1 && tl.back() == 's' && !isMs;
                if (isMs || isSec) {
                    float t = parseDuration(tok);
                    if (timeSlots == 0) { dur = t; timeSlots++; }
                    else                { del = t; timeSlots++; }
                    continue;
                }
                if (tl == "linear" || tl == "ease" || tl == "ease-in" || tl == "ease-out" ||
                    tl == "ease-in-out" || tl == "step-start" || tl == "step-end" ||
                    tl.rfind("cubic-bezier(", 0) == 0 || tl.rfind("steps(", 0) == 0) {
                    tf = parseTimingFunction(tok);
                    continue;
                }
                if (tl == "allow-discrete") {
                    behavior = TransitionBehavior::AllowDiscrete;
                    continue;
                }
                property = tl;
            }
            props.push_back(property);
            durations.push_back(dur);
            delays.push_back(del);
            tfs.push_back(tf);
            behaviors.push_back(behavior);
        }
        style.transitionProperty         = std::move(props);
        style.transitionDurations        = std::move(durations);
        style.transitionDelays           = std::move(delays);
        style.transitionTimingFunctions  = std::move(tfs);
        style.transitionBehavior         = std::move(behaviors);
        if (!style.transitionDurations.empty()) {
            style.transitionDuration = style.transitionDurations[0];
        }
    } else if (name == "transition-property") {
        style.transitionProperty = parseTransitionPropertyList(value);
    } else if (name == "transition-duration") {
        style.transitionDurations = parseDurationList(value);
        if (!style.transitionDurations.empty()) {
            style.transitionDuration = style.transitionDurations[0];
        }
    } else if (name == "transition-delay") {
        style.transitionDelays = parseDurationList(value);
    } else if (name == "transition-timing-function") {
        style.transitionTimingFunctions = parseTimingFunctionList(value);
    } else if (name == "transition-behavior") {
        style.transitionBehavior = parseTransitionBehaviorList(value);
    } else if (name == "animation") {
        auto parts = splitTopLevel(value, ',');
        std::vector<std::string> names;
        std::vector<float> durations, delays, iterationCounts;
        std::vector<AnimationDirection> directions;
        std::vector<AnimationFillMode> fillModes;
        std::vector<AnimationPlayState> playStates;
        std::vector<TimingFunction> tfs;
        std::vector<AnimationComposition> compositions;
        for (const auto& part : parts) {
            std::string p = trim(part);
            if (p.empty()) continue;
            std::string name = "none";
            float dur = 0.0f;
            float del = 0.0f;
            float iter = 1.0f;
            AnimationDirection dir = AnimationDirection::Normal;
            AnimationFillMode fill = AnimationFillMode::None;
            AnimationPlayState play = AnimationPlayState::Running;
            TimingFunction tf = TimingFunction::ease();
            AnimationComposition comp = AnimationComposition::Replace;
            int timeSlots = 0;
            bool nameSet = false;
            auto toks = splitWhitespaceTopLevel(p);
            for (const auto& tok : toks) {
                if (tok.empty()) continue;
                std::string tl = tok;
                for (char& c : tl) c = (char)std::tolower((unsigned char)c);
                bool isMs = tl.size() > 2 && tl.substr(tl.size() - 2) == "ms";
                bool isSec = tl.size() > 1 && tl.back() == 's' && !isMs;
                if (isMs || isSec) {
                    float t = parseDuration(tok);
                    if (timeSlots == 0) { dur = t; timeSlots++; }
                    else                { del = t; timeSlots++; }
                    continue;
                }
                if (tl == "infinite") { iter = -1.0f; continue; }
                if (tl == "normal")             { dir   = AnimationDirection::Normal;            continue; }
                if (tl == "reverse")            { dir   = AnimationDirection::Reverse;           continue; }
                if (tl == "alternate")          { dir   = AnimationDirection::Alternate;         continue; }
                if (tl == "alternate-reverse")  { dir   = AnimationDirection::AlternateReverse;  continue; }
                if (tl == "none")      { fill = AnimationFillMode::None;      if (!nameSet) { name = "none"; nameSet = true; } continue; }
                if (tl == "forwards")  { fill = AnimationFillMode::Forwards;  continue; }
                if (tl == "backwards") { fill = AnimationFillMode::Backwards; continue; }
                if (tl == "both")      { fill = AnimationFillMode::Both;      continue; }
                if (tl == "running")  { play = AnimationPlayState::Running;  continue; }
                if (tl == "paused")   { play = AnimationPlayState::Paused;   continue; }
                if (tl == "add")        { comp = AnimationComposition::Add;        continue; }
                if (tl == "accumulate") { comp = AnimationComposition::Accumulate; continue; }
                if (tl == "linear" || tl == "ease" || tl == "ease-in" || tl == "ease-out" ||
                    tl == "ease-in-out" || tl == "step-start" || tl == "step-end" ||
                    tl.rfind("cubic-bezier(", 0) == 0 || tl.rfind("steps(", 0) == 0) {
                    tf = parseTimingFunction(tok);
                    continue;
                }
                bool isNumber = true;
                bool sawDigit = false;
                size_t idx = 0;
                if (idx < tl.size() && (tl[idx] == '+' || tl[idx] == '-')) idx++;
                for (; idx < tl.size(); ++idx) {
                    if (tl[idx] >= '0' && tl[idx] <= '9') sawDigit = true;
                    else if (tl[idx] == '.') {  }
                    else { isNumber = false; break; }
                }
                if (isNumber && sawDigit) {
                    iter = parseFloat(tok);
                    continue;
                }
                if (!nameSet) {
                    name = tok;
                    nameSet = true;
                }
            }
            names.push_back(name);
            durations.push_back(dur);
            delays.push_back(del);
            iterationCounts.push_back(iter);
            directions.push_back(dir);
            fillModes.push_back(fill);
            playStates.push_back(play);
            tfs.push_back(tf);
            compositions.push_back(comp);
        }
        style.animationName           = std::move(names);
        style.animationDuration       = std::move(durations);
        style.animationDelay          = std::move(delays);
        style.animationIterationCount = std::move(iterationCounts);
        style.animationDirection      = std::move(directions);
        style.animationFillMode       = std::move(fillModes);
        style.animationPlayState      = std::move(playStates);
        style.animationTimingFunction = std::move(tfs);
        style.animationComposition    = std::move(compositions);
    } else if (name == "animation-name") {
        style.animationName = parseAnimationNameList(value);
    } else if (name == "animation-duration") {
        style.animationDuration = parseDurationList(value);
    } else if (name == "animation-delay") {
        style.animationDelay = parseDurationList(value);
    } else if (name == "animation-iteration-count") {
        style.animationIterationCount.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string s = trim(part);
            for (char& c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "infinite") style.animationIterationCount.push_back(-1.0f);
            else                 style.animationIterationCount.push_back(parseFloat(s));
        }
    } else if (name == "animation-direction") {
        style.animationDirection = parseAnimationDirectionList(value);
    } else if (name == "animation-fill-mode") {
        style.animationFillMode = parseAnimationFillModeList(value);
    } else if (name == "animation-play-state") {
        style.animationPlayState = parseAnimationPlayStateList(value);
    } else if (name == "animation-timing-function") {
        style.animationTimingFunction = parseTimingFunctionList(value);
    } else if (name == "animation-composition") {
        style.animationComposition = parseAnimationCompositionList(value);
    } else if (name == "position") {
        if (value == "relative") style.position = Position::Relative;
        else if (value == "absolute") style.position = Position::Absolute;
        else if (value == "fixed") style.position = Position::Fixed;
        else if (value == "sticky") style.position = Position::Sticky;
        else style.position = Position::Static;
    } else if (name == "top") {
        style.topVal = parseCSSValue(value);
        style.hasTopVal = true;
        style.orderTop = ++style.propertyOrder;
    } else if (name == "right") {
        style.rightVal = parseCSSValue(value);
        style.hasRightVal = true;
        style.orderRight = ++style.propertyOrder;
    } else if (name == "bottom") {
        style.bottomVal = parseCSSValue(value);
        style.hasBottomVal = true;
        style.orderBottom = ++style.propertyOrder;
    } else if (name == "left") {
        style.leftVal = parseCSSValue(value);
        style.hasLeftVal = true;
        style.orderLeft = ++style.propertyOrder;
    } else if (name == "grid-template-columns") {
        style.gridTemplateColumns = value;
        if (value != "none" && !value.empty())
            style.gridTemplateColumnTracks = parseGridTrackList(value, emBase);
        else
            style.gridTemplateColumnTracks.clear();
    } else if (name == "grid-template-rows") {
        style.gridTemplateRows = value;
        if (value != "none" && !value.empty())
            style.gridTemplateRowTracks = parseGridTrackList(value, emBase);
        else
            style.gridTemplateRowTracks.clear();
    } else if (name == "grid-template-areas") {
        style.gridTemplateAreas = parseGridTemplateAreas(value);
        style.hasGridTemplateAreas = (style.gridTemplateAreas.rowCount > 0);
    } else if (name == "grid-template") {
        // grid-template: <rows> / <columns>  (no areas shorthand for now)
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            std::string rows = trim(value.substr(0, slashPos));
            std::string cols = trim(value.substr(slashPos + 1));
            style.gridTemplateRows = rows;
            style.gridTemplateColumns = cols;
            style.gridTemplateRowTracks    = parseGridTrackList(rows, emBase);
            style.gridTemplateColumnTracks = parseGridTrackList(cols, emBase);
        }
    } else if (name == "grid-auto-rows") {
        style.gridAutoRowTracks = parseGridTrackList(value, emBase);
    } else if (name == "grid-auto-columns") {
        style.gridAutoColumnTracks = parseGridTrackList(value, emBase);
    } else if (name == "grid-auto-flow") {
        std::string v = lowerAscii(trim(value));
        if (v == "column")             style.gridAutoFlow = GridAutoFlow::Column;
        else if (v == "row dense")     style.gridAutoFlow = GridAutoFlow::RowDense;
        else if (v == "column dense")  style.gridAutoFlow = GridAutoFlow::ColumnDense;
        else if (v == "dense")         style.gridAutoFlow = GridAutoFlow::RowDense;
        else                           style.gridAutoFlow = GridAutoFlow::Row;
    } else if (name == "grid") {
        // grid shorthand: <template> | <auto-flow> rows / cols — parse basic / form
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            std::string rows = trim(value.substr(0, slashPos));
            std::string cols = trim(value.substr(slashPos + 1));
            style.gridTemplateRows = rows;
            style.gridTemplateColumns = cols;
            style.gridTemplateRowTracks    = parseGridTrackList(rows, emBase);
            style.gridTemplateColumnTracks = parseGridTrackList(cols, emBase);
        }
    } else if (name == "grid-column") {
        style.gridColumn = value;
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            style.gridColumnStart = parseGridPlacement(trim(value.substr(0, slashPos)));
            style.gridColumnEnd   = parseGridPlacement(trim(value.substr(slashPos + 1)));
        } else {
            style.gridColumnStart = parseGridPlacement(trim(value));
            style.gridColumnEnd   = GridPlacement{};
        }
    } else if (name == "grid-row") {
        style.gridRow = value;
        auto slashPos = value.find('/');
        if (slashPos != std::string::npos) {
            style.gridRowStart = parseGridPlacement(trim(value.substr(0, slashPos)));
            style.gridRowEnd   = parseGridPlacement(trim(value.substr(slashPos + 1)));
        } else {
            style.gridRowStart = parseGridPlacement(trim(value));
            style.gridRowEnd   = GridPlacement{};
        }
    } else if (name == "grid-column-start") {
        style.gridColumnStart = parseGridPlacement(value);
    } else if (name == "grid-column-end") {
        style.gridColumnEnd   = parseGridPlacement(value);
    } else if (name == "grid-row-start") {
        style.gridRowStart    = parseGridPlacement(value);
    } else if (name == "grid-row-end") {
        style.gridRowEnd      = parseGridPlacement(value);
    } else if (name == "grid-area") {
        style.gridArea = value;
        // grid-area: row-start / col-start / row-end / col-end
        // or just a named area reference
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i <= value.size(); i++) {
            if (i == value.size() || value[i] == '/') {
                parts.push_back(trim(value.substr(start, i - start)));
                start = i + 1;
            }
        }
        if (parts.size() == 4) {
            style.gridRowStart    = parseGridPlacement(parts[0]);
            style.gridColumnStart = parseGridPlacement(parts[1]);
            style.gridRowEnd      = parseGridPlacement(parts[2]);
            style.gridColumnEnd   = parseGridPlacement(parts[3]);
        } else if (parts.size() == 1 && !parts[0].empty()) {
            // Named area — stored as named-line references
            style.gridRowStart.type = GridPlacement::PlacementType::NamedLine;
            style.gridRowStart.name = parts[0];
            style.gridColumnStart.type = GridPlacement::PlacementType::NamedLine;
            style.gridColumnStart.name = parts[0];
        }
    } else if (name == "justify-items") {
        std::string v = lowerAscii(trim(value));
        if (v == "start" || v == "flex-start") style.justifyItems = JustifyItems::FlexStart;
        else if (v == "end" || v == "flex-end") style.justifyItems = JustifyItems::FlexEnd;
        else if (v == "center")   style.justifyItems = JustifyItems::Center;
        else if (v == "stretch")  style.justifyItems = JustifyItems::Stretch;
        else if (v == "baseline") style.justifyItems = JustifyItems::Baseline;
        else                      style.justifyItems = JustifyItems::Normal;
        style.hasJustifyItems = true;
    } else if (name == "justify-self") {
        std::string v = lowerAscii(trim(value));
        if (v == "start" || v == "flex-start") style.justifySelf = JustifySelf::FlexStart;
        else if (v == "end" || v == "flex-end") style.justifySelf = JustifySelf::FlexEnd;
        else if (v == "center")   style.justifySelf = JustifySelf::Center;
        else if (v == "stretch")  style.justifySelf = JustifySelf::Stretch;
        else if (v == "baseline") style.justifySelf = JustifySelf::Baseline;
        else                      style.justifySelf = JustifySelf::Auto;
        style.hasJustifySelf = true;
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
        style.transform = parseTransformOperations(value);
        style.hasTransform = true;
        for (const auto& op : style.transform) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.scale = val;
                }
            }
        }
    } else if (name == "transform-origin") {
        style.transformOrigin = parseTransformOrigin(value);
        style.hasTransformOrigin = true;
    } else if (name == "transform-style") {
        style.transformStyle = parseTransformStyle(value);
        style.hasTransformStyle = true;
    } else if (name == "transform-box") {
        style.transformBox = parseTransformBox(value);
        style.hasTransformBox = true;
    } else if (name == "perspective") {
        style.perspective = parsePerspective(value);
        style.hasPerspective = true;
    } else if (name == "perspective-origin") {
        style.perspectiveOrigin = parsePerspectiveOrigin(value);
        style.hasPerspectiveOrigin = true;
    } else if (name == "backface-visibility") {
        style.backfaceVisibility = parseBackfaceVisibility(value);
        style.hasBackfaceVisibility = true;
    }
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
    } else if (name == "column-count") {
        std::string v = trim(value);
        if (v == "auto") style.columnCount = 0;
        else style.columnCount = (int)parseFloat(v);
    } else if (name == "column-width") {
        std::string v = trim(value);
        if (v == "auto") style.columnWidth = 0.0f;
        else style.columnWidth = parseLengthPixels(v, emBase);
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
// -- mergePropertyPart3: scroll-driven animations + timeline properties --
void StyleSheet::mergePropertyPart3(Style& style, const std::string& name, const std::string& value, float emBase) {
    if (name == "animation-timeline") {
        style.animationTimeline.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.animationTimeline.push_back(trim(part));
        }
        style.hasAnimationTimeline = !style.animationTimeline.empty();
    } else if (name == "animation-range" || name == "animation-range-start") {
        if (name == "animation-range") {
            auto parts = splitTopLevel(value, ' ');
            style.animationRangeStart = trim(parts.size() > 0 ? parts[0] : value);
            style.animationRangeEnd   = parts.size() > 1 ? trim(parts[1]) : "normal";
        } else {
            style.animationRangeStart = trim(value);
        }
    } else if (name == "animation-range-end") {
        style.animationRangeEnd = trim(value);
    } else if (name == "scroll-timeline") {
        style.scrollTimelineName.clear();
        style.scrollTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string s = trim(part);
            std::istringstream iss(s);
            std::string n, a;
            iss >> n; iss >> a;
            style.scrollTimelineName.push_back(n);
            style.scrollTimelineAxis.push_back(a.empty() ? "block" : lowerAscii(a));
        }
        style.hasScrollTimeline = !style.scrollTimelineName.empty();
    } else if (name == "scroll-timeline-name") {
        style.scrollTimelineName.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.scrollTimelineName.push_back(trim(part));
        }
        style.hasScrollTimeline = !style.scrollTimelineName.empty();
    } else if (name == "scroll-timeline-axis") {
        style.scrollTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.scrollTimelineAxis.push_back(lowerAscii(trim(part)));
        }
    } else if (name == "view-timeline") {
        style.viewTimelineName.clear();
        style.viewTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            std::string s = trim(part);
            std::istringstream iss(s);
            std::string n, a;
            iss >> n; iss >> a;
            style.viewTimelineName.push_back(n);
            style.viewTimelineAxis.push_back(a.empty() ? "block" : lowerAscii(a));
        }
        style.hasViewTimeline = !style.viewTimelineName.empty();
    } else if (name == "view-timeline-name") {
        style.viewTimelineName.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.viewTimelineName.push_back(trim(part));
        }
        style.hasViewTimeline = !style.viewTimelineName.empty();
    } else if (name == "view-timeline-axis") {
        style.viewTimelineAxis.clear();
        for (const auto& part : splitTopLevel(value, ',')) {
            style.viewTimelineAxis.push_back(lowerAscii(trim(part)));
        }
    } else if (name == "view-timeline-inset") {
        style.viewTimelineInset = trim(value);
    } else if (name == "timeline-scope") {
        style.timelineScope.clear();
        if (lowerAscii(trim(value)) != "none") {
            for (const auto& part : splitTopLevel(value, ',')) {
                style.timelineScope.push_back(trim(part));
            }
        }
        style.hasTimelineScope = !style.timelineScope.empty();
    } else if (name == "font-variant-caps") {
        style.fontVariantCaps = lowerAscii(trim(value));
        style.hasFontVariantCaps = (style.fontVariantCaps != "normal");
    } else if (name == "font-variant-numeric") {
        style.fontVariantNumeric = lowerAscii(trim(value));
        style.hasFontVariantNumeric = (style.fontVariantNumeric != "normal");
    } else if (name == "font-variant-ligatures") {
        style.fontVariantLigatures = lowerAscii(trim(value));
        style.hasFontVariantLigatures = (style.fontVariantLigatures != "normal");
    } else if (name == "font-variant-east-asian") {
        style.fontVariantEastAsian = lowerAscii(trim(value));
        style.hasFontVariantEastAsian = (style.fontVariantEastAsian != "normal");
    } else if (name == "font-variant-position") {
        style.fontVariantPosition = lowerAscii(trim(value));
    } else if (name == "font-variant-alternates") {
        style.fontVariantAlternates = trim(value);
    } else if (name == "font-variant") {
        // font-variant shorthand ? distribute to sub-properties
        std::string v = lowerAscii(trim(value));
        if (v == "normal" || v == "none") {
            style.fontVariantCaps = "normal";
            style.fontVariantNumeric = "normal";
            style.fontVariantLigatures = v;
            style.fontVariantEastAsian = "normal";
        } else if (v == "small-caps" || v == "all-small-caps" || v == "petite-caps" ||
                   v == "all-petite-caps" || v == "unicase" || v == "titling-caps") {
            style.fontVariantCaps = v;
            style.hasFontVariantCaps = true;
        } else {
            style.fontVariantCaps = v; // best-effort
        }
    } else if (name == "font-feature-settings") {
        style.fontFeatureSettings = trim(value);
        style.hasFontFeatureSettings = (lowerAscii(style.fontFeatureSettings) != "normal");
    } else if (name == "font-variation-settings") {
        style.fontVariationSettings = trim(value);
        style.hasFontVariationSettings = (lowerAscii(style.fontVariationSettings) != "normal");
    } else if (name == "font-optical-sizing") {
        style.fontOpticalSizing = lowerAscii(trim(value));
        style.hasFontOpticalSizing = (style.fontOpticalSizing != "auto");
    } else if (name == "font-palette") {
        style.fontPalette = trim(value);
    } else if (name == "font-stretch") {
        style.fontStretch = lowerAscii(trim(value));
        style.hasFontStretch = (style.fontStretch != "normal");
    } else if (name == "font-synthesis") {
        style.fontSynthesis = lowerAscii(trim(value));
    } else if (name == "font-language-override") {
        style.fontLanguageOverride = trim(value);
    } else if (name == "tab-size") {
        style.tabSize = parseLengthPixels(value, emBase);
        if (style.tabSize == 0.0f) {
            // Bare integer (number of spaces)
            try { style.tabSize = std::stof(trim(value)); } catch (...) {}
        }
        style.hasTabSize = true;
    } else if (name == "hyphens") {
        style.hyphens = lowerAscii(trim(value));
        style.hasHyphens = (style.hyphens != "manual");
    } else if (name == "line-break") {
        style.lineBreak = lowerAscii(trim(value));
        style.hasLineBreak = (style.lineBreak != "auto");
    } else if (name == "overflow-wrap" || name == "word-wrap") {
        style.overflowWrap = lowerAscii(trim(value));
        style.hasOverflowWrap = (style.overflowWrap != "normal");
    } else if (name == "text-justify") {
        style.textJustify = lowerAscii(trim(value));
        style.hasTextJustify = (style.textJustify != "auto");
    } else if (name == "text-indent") {
        style.textIndent = parseLengthPixels(value, emBase);
        style.hasTextIndent = true;
    } else if (name == "hanging-punctuation") {
        style.hangingPunctuation = lowerAscii(trim(value));
        style.hasHangingPunctuation = (style.hangingPunctuation != "none");
    } else if (name == "accent-color") {
        std::string v = lowerAscii(trim(value));
        if (v == "auto") { style.hasAccentColor = false; }
        else { style.accentColor = parseColor(value); style.hasAccentColor = true; }
    } else if (name == "caret-color") {
        std::string v = lowerAscii(trim(value));
        if (v == "auto") { style.hasCaretColor = false; }
        else { style.caretColor = parseColor(value); style.hasCaretColor = true; }
    } else if (name == "color-scheme") {
        style.colorScheme = lowerAscii(trim(value));
        style.hasColorScheme = (style.colorScheme != "normal" && !style.colorScheme.empty());
    } else if (name == "inert") {
        style.inert = (lowerAscii(trim(value)) != "false" && !value.empty());
    } else if (name == "field-sizing") {
        style.fieldSizing = lowerAscii(trim(value));
        style.hasFieldSizing = !style.fieldSizing.empty();
    } else if (name == "image-rendering") {
        style.imageRendering = lowerAscii(trim(value));
        style.hasImageRendering = (style.imageRendering != "auto");
    } else if (name == "image-orientation") {
        style.imageOrientation = lowerAscii(trim(value));
        style.hasImageOrientation = (style.imageOrientation != "from-image");
    } else if (name == "object-view-box") {
        style.objectViewBox = trim(value);
        style.hasObjectViewBox = (lowerAscii(style.objectViewBox) != "none" && !style.objectViewBox.empty());
    } else if (name == "touch-action") {
        style.touchAction = lowerAscii(trim(value));
        style.hasTouchAction = (style.touchAction != "auto");
    } else if (name == "user-select") {
        style.userSelect = lowerAscii(trim(value));
        style.hasUserSelect = (style.userSelect != "auto");
    } else if (name == "will-change") {
        style.willChange = trim(value);
        style.hasWillChange = (lowerAscii(style.willChange) != "auto" && !style.willChange.empty());
    } else if (name == "contain-intrinsic-size" || name == "contain-intrinsic-width" || name == "contain-intrinsic-height") {
        style.containIntrinsicSize = trim(value);
        style.hasContainIntrinsicSize = (lowerAscii(style.containIntrinsicSize) != "none");
    } else if (name == "content-visibility") {
        style.contentVisibility = lowerAscii(trim(value));
        style.hasContentVisibility = (style.contentVisibility != "visible");
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
        auto ops = parseTransformOperations(value);
        for (const auto& op : ops) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.hoverScale = val;
                }
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
        auto ops = parseTransformOperations(value);
        for (const auto& op : ops) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.focusScale = val;
                }
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
        auto ops = parseTransformOperations(value);
        for (const auto& op : ops) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].resolve(1.0f);
                    style.activeScale = val;
                }
            }
        }
    }
}
bool StyleSheet::isValidSyntax(const std::string& value, const std::string& syntax) {
    std::string val = trim(value);
    std::string syn = trim(syntax);
    if (syn == "*") return true;
    if (syn.empty()) return true;
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
        return unit == "px" || unit == "em" || unit == "rem" || unit == "vw" || unit == "vh" ||
               unit == "ch" || unit == "lh" || unit == "vi" || unit == "vb" || unit == "dvw" || unit == "dvh" ||
               unit == "pt" || unit == "%" || unit == "in" || unit == "cm" || unit == "mm" || unit == "pc";
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
    if (syn == "<transform-list>" || syn == "<transform-origin>" || syn == "<perspective-origin>") {
        return true;
    }
    return lowerAscii(val) == lowerAscii(syn);
}
static std::string cssValueToString(const CSSValue& val) {
    if (val.unit == CSSValue::Auto) return "auto";
    char buf[64];
    snprintf(buf, sizeof(buf), "%.4f", val.resolve(1.0f));
    std::string numStr = buf;
    if (numStr.find('.') != std::string::npos) {
        while (!numStr.empty() && numStr.back() == '0') numStr.pop_back();
        if (!numStr.empty() && numStr.back() == '.') numStr.pop_back();
    }
    if (numStr.empty()) numStr = "0";
    switch (val.unit) {
        case CSSValue::Px: return numStr + "px";
        case CSSValue::Percent: return numStr + "%";
        case CSSValue::Vw: return numStr + "vw";
        case CSSValue::Vh: return numStr + "vh";
        case CSSValue::Em: return numStr + "em";
        case CSSValue::Rem: return numStr + "rem";
        case CSSValue::Deg: return numStr + "deg";
        case CSSValue::Rad: return numStr + "rad";
        case CSSValue::Grad: return numStr + "grad";
        case CSSValue::Turn: return numStr + "turn";
        default: return numStr;
    }
}
static CSSValue interpolateCSSValue(const CSSValue& fromVal, const CSSValue& toVal, float t) {
    float f = fromVal.resolve(1.0f);
    float o = toVal.resolve(1.0f);
    if (fromVal.unit == toVal.unit) {
        return CSSValue(f + (o - f) * t, fromVal.unit);
    }
    if (fromVal.unit == CSSValue::None) {
        return CSSValue(f + (o - f) * t, toVal.unit);
    }
    if (toVal.unit == CSSValue::None) {
        return CSSValue(f + (o - f) * t, fromVal.unit);
    }
    return t < 0.5f ? fromVal : toVal;
}
static TransformOperation getIdentityTransformOperation(TransformOperationType type, const TransformOperation& matchingOp) {
    TransformOperation ident;
    ident.type = type;
    switch (type) {
        case TransformOperationType::Translate:
            ident.args = { CSSValue(0.0f, CSSValue::Px), CSSValue(0.0f, CSSValue::Px) };
            break;
        case TransformOperationType::Translate3d:
            ident.args = { CSSValue(0.0f, CSSValue::Px), CSSValue(0.0f, CSSValue::Px), CSSValue(0.0f, CSSValue::Px) };
            break;
        case TransformOperationType::TranslateX:
        case TransformOperationType::TranslateY:
        case TransformOperationType::TranslateZ:
            ident.args = { CSSValue(0.0f, CSSValue::Px) };
            break;
        case TransformOperationType::Scale:
            ident.args = { CSSValue(1.0f, CSSValue::None), CSSValue(1.0f, CSSValue::None) };
            break;
        case TransformOperationType::Scale3d:
            ident.args = { CSSValue(1.0f, CSSValue::None), CSSValue(1.0f, CSSValue::None), CSSValue(1.0f, CSSValue::None) };
            break;
        case TransformOperationType::ScaleX:
        case TransformOperationType::ScaleY:
        case TransformOperationType::ScaleZ:
            ident.args = { CSSValue(1.0f, CSSValue::None) };
            break;
        case TransformOperationType::Rotate:
        case TransformOperationType::RotateX:
        case TransformOperationType::RotateY:
        case TransformOperationType::RotateZ:
            ident.args = { CSSValue(0.0f, CSSValue::Deg) };
            break;
        case TransformOperationType::Rotate3d:
            if (matchingOp.args.size() == 4) {
                ident.args = { matchingOp.args[0], matchingOp.args[1], matchingOp.args[2], CSSValue(0.0f, CSSValue::Deg) };
            } else {
                ident.args = { CSSValue(0.0f, CSSValue::None), CSSValue(0.0f, CSSValue::None), CSSValue(1.0f, CSSValue::None), CSSValue(0.0f, CSSValue::Deg) };
            }
            break;
        case TransformOperationType::Skew:
            ident.args = { CSSValue(0.0f, CSSValue::Deg), CSSValue(0.0f, CSSValue::Deg) };
            break;
        case TransformOperationType::SkewX:
        case TransformOperationType::SkewY:
            ident.args = { CSSValue(0.0f, CSSValue::Deg) };
            break;
        case TransformOperationType::Matrix:
            ident.args = { CSSValue(1.0f, CSSValue::None), CSSValue(0.0f, CSSValue::None), CSSValue(0.0f, CSSValue::None),
                           CSSValue(1.0f, CSSValue::None), CSSValue(0.0f, CSSValue::None), CSSValue(0.0f, CSSValue::None) };
            break;
        case TransformOperationType::Matrix3d:
            ident.args.resize(16, CSSValue(0.0f, CSSValue::None));
            ident.args[0] = CSSValue(1.0f, CSSValue::None);
            ident.args[5] = CSSValue(1.0f, CSSValue::None);
            ident.args[10] = CSSValue(1.0f, CSSValue::None);
            ident.args[15] = CSSValue(1.0f, CSSValue::None);
            break;
        case TransformOperationType::Perspective:
            ident.args = { CSSValue(0.0f, CSSValue::None) };
            break;
    }
    ident.resolveFromArgs();
    return ident;
}
static std::string transformOperationsToString(const std::vector<TransformOperation>& ops) {
    std::string res;
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i > 0) res += " ";
        const auto& op = ops[i];
        switch (op.type) {
            case TransformOperationType::Translate: res += "translate"; break;
            case TransformOperationType::Translate3d: res += "translate3d"; break;
            case TransformOperationType::TranslateX: res += "translateX"; break;
            case TransformOperationType::TranslateY: res += "translateY"; break;
            case TransformOperationType::TranslateZ: res += "translateZ"; break;
            case TransformOperationType::Scale: res += "scale"; break;
            case TransformOperationType::Scale3d: res += "scale3d"; break;
            case TransformOperationType::ScaleX: res += "scaleX"; break;
            case TransformOperationType::ScaleY: res += "scaleY"; break;
            case TransformOperationType::ScaleZ: res += "scaleZ"; break;
            case TransformOperationType::Rotate: res += "rotate"; break;
            case TransformOperationType::Rotate3d: res += "rotate3d"; break;
            case TransformOperationType::RotateX: res += "rotateX"; break;
            case TransformOperationType::RotateY: res += "rotateY"; break;
            case TransformOperationType::RotateZ: res += "rotateZ"; break;
            case TransformOperationType::Skew: res += "skew"; break;
            case TransformOperationType::SkewX: res += "skewX"; break;
            case TransformOperationType::SkewY: res += "skewY"; break;
            case TransformOperationType::Matrix: res += "matrix"; break;
            case TransformOperationType::Matrix3d: res += "matrix3d"; break;
            case TransformOperationType::Perspective: res += "perspective"; break;
        }
        res += "(";
        for (size_t j = 0; j < op.args.size(); ++j) {
            if (j > 0) res += ", ";
            res += cssValueToString(op.args[j]);
        }
        res += ")";
    }
    return res;
}
std::string StyleSheet::interpolateTransformList(const std::string& fromStr, const std::string& toStr, float t) {
    std::vector<TransformOperation> fromOps = StyleSheet::parseTransformOperations(fromStr);
    std::vector<TransformOperation> toOps = StyleSheet::parseTransformOperations(toStr);
    if (fromOps.size() != toOps.size()) {
        return t < 0.5f ? fromStr : toStr;
    }
    bool typesMatch = true;
    for (size_t i = 0; i < fromOps.size(); ++i) {
        if (fromOps[i].type != toOps[i].type) {
            typesMatch = false;
            break;
        }
    }
    if (!typesMatch) {
        return t < 0.5f ? fromStr : toStr;
    }
    std::vector<TransformOperation> resultOps;
    for (size_t i = 0; i < fromOps.size(); ++i) {
        const auto& fOp = fromOps[i];
        const auto& tOp = toOps[i];
        size_t maxArgs = std::max(fOp.args.size(), tOp.args.size());
        std::vector<CSSValue> fArgs = fOp.args;
        std::vector<CSSValue> tArgs = tOp.args;
        fArgs.resize(maxArgs, CSSValue(0.0f, CSSValue::None));
        tArgs.resize(maxArgs, CSSValue(0.0f, CSSValue::None));
        std::vector<CSSValue> rArgs;
        for (size_t j = 0; j < maxArgs; ++j) {
            rArgs.push_back(interpolateCSSValue(fArgs[j], tArgs[j], t));
        }
        TransformOperation op;
        op.type = fOp.type;
        op.args = rArgs;
        op.resolveFromArgs();
        resultOps.push_back(op);
    }
    return transformOperationsToString(resultOps);
}
static std::string interpolateTransformOrigin(const std::string& fromStr, const std::string& toStr, float t) {
    TransformOrigin fromOrig = StyleSheet::parseTransformOrigin(fromStr);
    TransformOrigin toOrig = StyleSheet::parseTransformOrigin(toStr);
    CSSValue rX = interpolateCSSValue(fromOrig.x, toOrig.x, t);
    CSSValue rY = interpolateCSSValue(fromOrig.y, toOrig.y, t);
    CSSValue rZ = interpolateCSSValue(fromOrig.z, toOrig.z, t);
    return cssValueToString(rX) + " " + cssValueToString(rY) + " " + cssValueToString(rZ);
}
static std::string interpolatePerspectiveOrigin(const std::string& fromStr, const std::string& toStr, float t) {
    PerspectiveOrigin fromOrig = StyleSheet::parsePerspectiveOrigin(fromStr);
    PerspectiveOrigin toOrig = StyleSheet::parsePerspectiveOrigin(toStr);
    CSSValue rX = interpolateCSSValue(fromOrig.x, toOrig.x, t);
    CSSValue rY = interpolateCSSValue(fromOrig.y, toOrig.y, t);
    return cssValueToString(rX) + " " + cssValueToString(rY);
}
std::string StyleSheet::interpolateTypedValue(const std::string& from,
                                              const std::string& to,
                                              float t,
                                              const std::string& syntax) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    if (from == to) return from;
    std::string syn = trim(syntax);
    if (syn == "*" || syn.empty()) {
        return t < 0.5f ? from : to;
    }
    if (syn == "<transform-list>") {
        return interpolateTransformList(from, to, t);
    }
    if (syn == "<transform-origin>") {
        return interpolateTransformOrigin(from, to, t);
    }
    if (syn == "<perspective-origin>") {
        return interpolatePerspectiveOrigin(from, to, t);
    }
    if (syn.find('|') != std::string::npos) {
        std::istringstream ss(syn);
        std::string part;
        while (std::getline(ss, part, '|')) {
            part = trim(part);
            if (part.size() > 0 && part[0] == '<' &&
                isValidSyntax(from, part) && isValidSyntax(to, part)) {
                return interpolateTypedValue(from, to, t, part);
            }
        }
        return t < 0.5f ? from : to;
    }
    if (syn == "<color>") {
        Color cFrom = parseColor(from);
        Color cTo = parseColor(to);
        Color result = Color::lerp(cFrom, cTo, t);
        int r = static_cast<int>(result.r * 255.0f + 0.5f);
        int g = static_cast<int>(result.g * 255.0f + 0.5f);
        int b = static_cast<int>(result.b * 255.0f + 0.5f);
        if (std::abs(result.a - 1.0f) < 0.001f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "rgb(%d, %d, %d)", r, g, b);
            return buf;
        } else {
            char buf[48];
            snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, %.3f)", r, g, b, result.a);
            return buf;
        }
    }
    if (syn == "<number>" || syn == "<integer>") {
        float vFrom = parseFloat(from);
        float vTo = parseFloat(to);
        float result = vFrom + (vTo - vFrom) * t;
        if (syn == "<integer>") {
            return std::to_string(static_cast<int>(result + 0.5f));
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g", result);
        return buf;
    }
    if (syn == "<length>") {
        std::string fromLower = lowerAscii(trim(from));
        std::string toLower = lowerAscii(trim(to));
        auto extractUnit = [](const std::string& v) -> std::pair<float, std::string> {
            if (v == "0") return {0.0f, "px"};
            size_t lastNum = v.find_last_of("0123456789.");
            if (lastNum == std::string::npos) return {0.0f, "px"};
            std::string num = v.substr(0, lastNum + 1);
            std::string unit = v.substr(lastNum + 1);
            if (unit.empty()) unit = "px";
            float val = 0;
            try { val = std::stof(num); } catch (...) {}
            return {val, unit};
        };
        auto [vFrom, unitFrom] = extractUnit(fromLower);
        auto [vTo, unitTo] = extractUnit(toLower);
        std::string unit = unitFrom;
        if (vFrom == 0.0f) unit = unitTo;
        else if (vTo == 0.0f) unit = unitFrom;
        else if (unitFrom != unitTo) {
            float pxFrom = parseLengthPixels(from);
            float pxTo = parseLengthPixels(to);
            float result = pxFrom + (pxTo - pxFrom) * t;
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4gpx", result);
            return buf;
        }
        float result = vFrom + (vTo - vFrom) * t;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g%s", result, unit.c_str());
        return buf;
    }
    if (syn == "<percentage>") {
        float vFrom = parseFloat(from);
        float vTo = parseFloat(to);
        float result = vFrom + (vTo - vFrom) * t;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g%%", result);
        return buf;
    }
    if (syn == "<length-percentage>") {
        if (isValidSyntax(from, "<length>") && isValidSyntax(to, "<length>")) {
            return interpolateTypedValue(from, to, t, "<length>");
        }
        if (isValidSyntax(from, "<percentage>") && isValidSyntax(to, "<percentage>")) {
            return interpolateTypedValue(from, to, t, "<percentage>");
        }
        float pxFrom = parseLengthPixels(from);
        float pxTo = parseLengthPixels(to);
        float result = pxFrom + (pxTo - pxFrom) * t;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4gpx", result);
        return buf;
    }
    if (syn == "<angle>") {
        auto extractAngle = [](const std::string& v) -> std::pair<float, std::string> {
            if (v == "0") return {0.0f, "deg"};
            std::string lower = v;
            for (char& c : lower) c = (char)std::tolower((unsigned char)c);
            size_t lastNum = lower.find_last_of("0123456789.");
            if (lastNum == std::string::npos) return {0.0f, "deg"};
            std::string num = lower.substr(0, lastNum + 1);
            std::string unit = lower.substr(lastNum + 1);
            if (unit.empty()) unit = "deg";
            float val = 0;
            try { val = std::stof(num); } catch (...) {}
            return {val, unit};
        };
        auto [vFrom, unitFrom] = extractAngle(trim(from));
        auto [vTo, unitTo] = extractAngle(trim(to));
        std::string unit = unitFrom;
        if (unitFrom != unitTo) {
            auto toDeg = [](float v, const std::string& u) -> float {
                if (u == "rad") return v * 180.0f / 3.14159265358979f;
                if (u == "grad") return v * 0.9f;
                if (u == "turn") return v * 360.0f;
                return v;
            };
            vFrom = toDeg(vFrom, unitFrom);
            vTo = toDeg(vTo, unitTo);
            unit = "deg";
        }
        float result = vFrom + (vTo - vFrom) * t;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g%s", result, unit.c_str());
        return buf;
    }
    if (syn == "<time>") {
        float msFrom = parseDuration(from) * 1000.0f;
        float msTo = parseDuration(to) * 1000.0f;
        float result = msFrom + (msTo - msFrom) * t;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4gms", result);
        return buf;
    }
    return t < 0.5f ? from : to;
}
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
std::vector<TransformOperation> StyleSheet::parseTransformOperations(const std::string& value) {
    std::vector<TransformOperation> operations;
    std::string trimmedVal = trim(value);
    if (trimmedVal == "none" || trimmedVal.empty()) {
        return operations;
    }
    size_t pos = 0;
    while (pos < trimmedVal.size()) {
        while (pos < trimmedVal.size() && std::isspace(static_cast<unsigned char>(trimmedVal[pos]))) {
            pos++;
        }
        if (pos >= trimmedVal.size()) break;
        size_t nameStart = pos;
        while (pos < trimmedVal.size() && (std::isalnum(static_cast<unsigned char>(trimmedVal[pos])) || trimmedVal[pos] == '-')) {
            pos++;
        }
        std::string name = trimmedVal.substr(nameStart, pos - nameStart);
        while (pos < trimmedVal.size() && std::isspace(static_cast<unsigned char>(trimmedVal[pos]))) {
            pos++;
        }
        if (pos >= trimmedVal.size() || trimmedVal[pos] != '(') {
            break;
        }
        pos++;
        size_t argsStart = pos;
        int depth = 1;
        while (pos < trimmedVal.size() && depth > 0) {
            if (trimmedVal[pos] == '(') depth++;
            else if (trimmedVal[pos] == ')') depth--;
            pos++;
        }
        if (depth > 0) {
            break;
        }
        std::string argsStr = trimmedVal.substr(argsStart, pos - 1 - argsStart);
        std::vector<std::string> rawArgs;
        size_t argPos = 0;
        int argDepth = 0;
        size_t lastStart = 0;
        while (argPos < argsStr.size()) {
            if (argsStr[argPos] == '(') argDepth++;
            else if (argsStr[argPos] == ')') argDepth--;
            else if (argsStr[argPos] == ',' && argDepth == 0) {
                rawArgs.push_back(trim(argsStr.substr(lastStart, argPos - lastStart)));
                lastStart = argPos + 1;
            }
            argPos++;
        }
        rawArgs.push_back(trim(argsStr.substr(lastStart)));
        std::vector<CSSValue> args;
        for (const auto& rawArg : rawArgs) {
            if (!rawArg.empty()) {
                std::string lowerArg = lowerAscii(rawArg);
                if (lowerArg == "0") {
                    args.push_back(CSSValue::px(0.0f));
                } else if (lowerArg.find("deg") != std::string::npos ||
                           lowerArg.find("rad") != std::string::npos ||
                           lowerArg.find("grad") != std::string::npos ||
                           lowerArg.find("turn") != std::string::npos) {
                    float deg = parseAngleDegrees(rawArg);
                    args.push_back(CSSValue{deg, CSSValue::Deg});
                } else {
                    args.push_back(parseCSSValue(rawArg));
                }
            }
        }
        TransformOperationType type;
        bool valid = true;
        if (name == "translate") {
            type = TransformOperationType::Translate;
            if (args.empty() || args.size() > 2) valid = false;
            if (args.size() == 1) args.push_back(CSSValue::px(0.0f));
        } else if (name == "translate3d") {
            type = TransformOperationType::Translate3d;
            if (args.size() != 3) valid = false;
        } else if (name == "translateX") {
            type = TransformOperationType::TranslateX;
            if (args.size() != 1) valid = false;
        } else if (name == "translateY") {
            type = TransformOperationType::TranslateY;
            if (args.size() != 1) valid = false;
        } else if (name == "translateZ") {
            type = TransformOperationType::TranslateZ;
            if (args.size() != 1) valid = false;
        } else if (name == "scale") {
            type = TransformOperationType::Scale;
            if (args.empty() || args.size() > 2) valid = false;
            if (args.size() == 1) {
                args.push_back(args[0]);
            }
        } else if (name == "scale3d") {
            type = TransformOperationType::Scale3d;
            if (args.size() != 3) valid = false;
        } else if (name == "scaleX") {
            type = TransformOperationType::ScaleX;
            if (args.size() != 1) valid = false;
        } else if (name == "scaleY") {
            type = TransformOperationType::ScaleY;
            if (args.size() != 1) valid = false;
        } else if (name == "scaleZ") {
            type = TransformOperationType::ScaleZ;
            if (args.size() != 1) valid = false;
        } else if (name == "rotate") {
            type = TransformOperationType::Rotate;
            if (args.size() != 1) valid = false;
        } else if (name == "rotate3d") {
            type = TransformOperationType::Rotate3d;
            if (args.size() != 4) valid = false;
        } else if (name == "rotateX") {
            type = TransformOperationType::RotateX;
            if (args.size() != 1) valid = false;
        } else if (name == "rotateY") {
            type = TransformOperationType::RotateY;
            if (args.size() != 1) valid = false;
        } else if (name == "rotateZ") {
            type = TransformOperationType::RotateZ;
            if (args.size() != 1) valid = false;
        } else if (name == "skew") {
            type = TransformOperationType::Skew;
            if (args.empty() || args.size() > 2) valid = false;
            if (args.size() == 1) args.push_back(CSSValue{0.0f, CSSValue::Deg});
        } else if (name == "skewX") {
            type = TransformOperationType::SkewX;
            if (args.size() != 1) valid = false;
        } else if (name == "skewY") {
            type = TransformOperationType::SkewY;
            if (args.size() != 1) valid = false;
        } else if (name == "matrix") {
            type = TransformOperationType::Matrix;
            if (args.size() != 6) valid = false;
        } else if (name == "matrix3d") {
            type = TransformOperationType::Matrix3d;
            if (args.size() != 16) valid = false;
        } else if (name == "perspective") {
            type = TransformOperationType::Perspective;
            if (args.size() != 1) valid = false;
        } else {
            valid = false;
        }
        if (valid) {
            TransformOperation op;
            op.type = type;
            op.args = args;
            op.resolveFromArgs();
            operations.push_back(op);
        }
    }
    return operations;
}
TransformOrigin StyleSheet::parseTransformOrigin(const std::string& value) {
    TransformOrigin origin;
    std::string_view tokens[3];
    int count = 0;
    splitWhitespace(value, tokens, 3, count);
    if (count == 0) return origin;
    auto parseX = [](std::string_view tok, bool& ok) -> CSSValue {
        std::string s = lowerAscii(tok);
        ok = true;
        if (s == "left") return CSSValue::pct(0.0f);
        if (s == "right") return CSSValue::pct(100.0f);
        if (s == "center") return CSSValue::pct(50.0f);
        ok = false;
        return CSSValue::pct(50.0f);
    };
    auto parseY = [](std::string_view tok, bool& ok) -> CSSValue {
        std::string s = lowerAscii(tok);
        ok = true;
        if (s == "top") return CSSValue::pct(0.0f);
        if (s == "bottom") return CSSValue::pct(100.0f);
        if (s == "center") return CSSValue::pct(50.0f);
        ok = false;
        return CSSValue::pct(50.0f);
    };
    if (count == 1) {
        bool ok = false;
        CSSValue val = parseX(tokens[0], ok);
        if (!ok) {
            val = parseY(tokens[0], ok);
            if (ok) {
                origin.x = CSSValue::pct(50.0f);
                origin.y = val;
                origin.z = CSSValue::px(0.0f);
                return origin;
            } else {
                val = parseCSSValue(std::string(tokens[0]));
            }
        }
        origin.x = val;
        origin.y = CSSValue::pct(50.0f);
        origin.z = CSSValue::px(0.0f);
    } else if (count == 2) {
        bool firstIsY = false;
        bool secondIsX = false;
        bool firstIsX = false;
        bool secondIsY = false;
        CSSValue xVal, yVal;
        bool ok;
        CSSValue testX1 = parseX(tokens[0], ok);
        if (ok) { firstIsX = true; xVal = testX1; }
        CSSValue testY1 = parseY(tokens[0], ok);
        if (ok) { firstIsY = true; yVal = testY1; }
        CSSValue testX2 = parseX(tokens[1], ok);
        if (ok) { secondIsX = true; xVal = testX2; }
        CSSValue testY2 = parseY(tokens[1], ok);
        if (ok) { secondIsY = true; yVal = testY2; }
        if (firstIsY && secondIsX) {
            origin.x = xVal;
            origin.y = yVal;
        } else {
            if (firstIsX) {
                origin.x = xVal;
            } else {
                origin.x = parseCSSValue(std::string(tokens[0]));
            }
            if (secondIsY) {
                origin.y = yVal;
            } else {
                origin.y = parseCSSValue(std::string(tokens[1]));
            }
        }
        origin.z = CSSValue::px(0.0f);
    } else if (count == 3) {
        bool firstIsY = false;
        bool secondIsX = false;
        bool firstIsX = false;
        bool secondIsY = false;
        CSSValue xVal, yVal;
        bool ok;
        CSSValue testX1 = parseX(tokens[0], ok);
        if (ok) { firstIsX = true; xVal = testX1; }
        CSSValue testY1 = parseY(tokens[0], ok);
        if (ok) { firstIsY = true; yVal = testY1; }
        CSSValue testX2 = parseX(tokens[1], ok);
        if (ok) { secondIsX = true; xVal = testX2; }
        CSSValue testY2 = parseY(tokens[1], ok);
        if (ok) { secondIsY = true; yVal = testY2; }
        if (firstIsY && secondIsX) {
            origin.x = xVal;
            origin.y = yVal;
        } else {
            if (firstIsX) {
                origin.x = xVal;
            } else {
                origin.x = parseCSSValue(std::string(tokens[0]));
            }
            if (secondIsY) {
                origin.y = yVal;
            } else {
                origin.y = parseCSSValue(std::string(tokens[1]));
            }
        }
        origin.z = parseCSSValue(std::string(tokens[2]));
    }
    return origin;
}
TransformStyle StyleSheet::parseTransformStyle(const std::string& value) {
    std::string s = lowerAscii(trim(value));
    if (s == "preserve-3d") return TransformStyle::Preserve3D;
    return TransformStyle::Flat;
}
TransformBox StyleSheet::parseTransformBox(const std::string& value) {
    std::string s = lowerAscii(trim(value));
    if (s == "content-box") return TransformBox::ContentBox;
    if (s == "fill-box") return TransformBox::FillBox;
    if (s == "stroke-box") return TransformBox::StrokeBox;
    if (s == "view-box") return TransformBox::ViewBox;
    return TransformBox::BorderBox;
}
CSSValue StyleSheet::parsePerspective(const std::string& value) {
    std::string s = lowerAscii(trim(value));
    if (s == "none" || s.empty()) {
        return CSSValue{0.0f, CSSValue::None};
    }
    return parseCSSValue(s);
}
PerspectiveOrigin StyleSheet::parsePerspectiveOrigin(const std::string& value) {
    PerspectiveOrigin origin;
    std::string_view tokens[2];
    int count = 0;
    splitWhitespace(value, tokens, 2, count);
    if (count == 0) return origin;
    auto parseX = [](std::string_view tok, bool& ok) -> CSSValue {
        std::string s = lowerAscii(tok);
        ok = true;
        if (s == "left") return CSSValue::pct(0.0f);
        if (s == "right") return CSSValue::pct(100.0f);
        if (s == "center") return CSSValue::pct(50.0f);
        ok = false;
        return CSSValue::pct(50.0f);
    };
    auto parseY = [](std::string_view tok, bool& ok) -> CSSValue {
        std::string s = lowerAscii(tok);
        ok = true;
        if (s == "top") return CSSValue::pct(0.0f);
        if (s == "bottom") return CSSValue::pct(100.0f);
        if (s == "center") return CSSValue::pct(50.0f);
        ok = false;
        return CSSValue::pct(50.0f);
    };
    if (count == 1) {
        bool ok = false;
        CSSValue val = parseX(tokens[0], ok);
        if (!ok) {
            val = parseY(tokens[0], ok);
            if (ok) {
                origin.x = CSSValue::pct(50.0f);
                origin.y = val;
                return origin;
            } else {
                val = parseCSSValue(std::string(tokens[0]));
            }
        }
        origin.x = val;
        origin.y = CSSValue::pct(50.0f);
    } else if (count == 2) {
        bool firstIsY = false;
        bool secondIsX = false;
        bool firstIsX = false;
        bool secondIsY = false;
        CSSValue xVal, yVal;
        bool ok;
        CSSValue testX1 = parseX(tokens[0], ok);
        if (ok) { firstIsX = true; xVal = testX1; }
        CSSValue testY1 = parseY(tokens[0], ok);
        if (ok) { firstIsY = true; yVal = testY1; }
        CSSValue testX2 = parseX(tokens[1], ok);
        if (ok) { secondIsX = true; xVal = testX2; }
        CSSValue testY2 = parseY(tokens[1], ok);
        if (ok) { secondIsY = true; yVal = testY2; }
        if (firstIsY && secondIsX) {
            origin.x = xVal;
            origin.y = yVal;
        } else {
            if (firstIsX) {
                origin.x = xVal;
            } else {
                origin.x = parseCSSValue(std::string(tokens[0]));
            }
            if (secondIsY) {
                origin.y = yVal;
            } else {
                origin.y = parseCSSValue(std::string(tokens[1]));
            }
        }
    }
    return origin;
}
BackfaceVisibility StyleSheet::parseBackfaceVisibility(const std::string& value) {
    std::string s = lowerAscii(trim(value));
    if (s == "hidden") return BackfaceVisibility::Hidden;
    return BackfaceVisibility::Visible;
}
float StyleSheet::parseAngleDegrees(const std::string& value) {
    std::string lower = lowerAscii(trim(value));
    if (lower == "0") return 0.0f;
    size_t lastNum = lower.find_last_of("0123456789.-+");
    if (lastNum == std::string::npos) return 0.0f;
    float num = std::stof(lower.substr(0, lastNum + 1));
    std::string unit = lower.substr(lastNum + 1);
    if (unit == "deg" || unit.empty()) {
        return num;
    } else if (unit == "rad") {
        return num * 180.0f / 3.141592653589793f;
    } else if (unit == "grad") {
        return num * 0.9f;
    } else if (unit == "turn") {
        return num * 360.0f;
    }
    return 0.0f;
}
std::vector<TransformOperation> StyleSheet::parseTransformList(const std::string& value) {
    return parseTransformOperations(value);
}

// ============================================================
//  parseGridTrackList — Blink NGGridLayoutAlgorithm parity
//
//  Parses a CSS <track-list> such as:
//    100px 1fr auto
//    repeat(3, 1fr)
//    minmax(100px, 1fr) fit-content(200px)
//    [header] 80px [content] 1fr [footer] 60px
//    subgrid
//
//  Returns a flat vector of GridTrackSize (repeat() is expanded).
// ============================================================

// Forward-declare so resolveGridToken can call parseGridTrackList recursively.
static GridTrackSize resolveGridToken(const std::string& tok, float emBase);

static GridTrackSize resolveGridToken(const std::string& tok, float emBase) {
    GridTrackSize s;
    std::string t  = StyleSheet::trim(tok);
    std::string tl = lowerAscii(t);

    if (tl.empty() || tl == "auto")     { s.type = GridTrackSizeType::Auto;       return s; }
    if (tl == "min-content")            { s.type = GridTrackSizeType::MinContent;  return s; }
    if (tl == "max-content")            { s.type = GridTrackSizeType::MaxContent;  return s; }
    if (tl == "subgrid")                { s.type = GridTrackSizeType::Subgrid;     return s; }

    // fr unit
    if (t.size() > 2 && t[t.size()-1] == 'r' && t[t.size()-2] == 'f') {
        s.type  = GridTrackSizeType::Flex;
        s.value = parseLocaleIndependentFloat(t.substr(0, t.size() - 2).c_str(), nullptr);
        return s;
    }

    // minmax(<min>, <max>)
    if (tl.size() >= 7 && tl.substr(0, 7) == "minmax(") {
        s.type = GridTrackSizeType::MinMax;
        size_t open  = t.find('(');
        size_t close = t.rfind(')');
        if (open != std::string::npos && close != std::string::npos && close > open) {
            std::string inner_s = StyleSheet::trim(t.substr(open + 1, close - open - 1));
            // Find top-level comma (not inside nested parens)
            int depth = 0;
            size_t comma = std::string::npos;
            for (size_t k = 0; k < inner_s.size(); k++) {
                if (inner_s[k] == '(') ++depth;
                else if (inner_s[k] == ')') --depth;
                else if (inner_s[k] == ',' && depth == 0) { comma = k; break; }
            }
            if (comma != std::string::npos) {
                GridTrackSize minT = resolveGridToken(StyleSheet::trim(inner_s.substr(0, comma)), emBase);
                GridTrackSize maxT = resolveGridToken(StyleSheet::trim(inner_s.substr(comma + 1)), emBase);
                s.minType  = minT.type;  s.minValue = minT.value;
                s.maxType  = maxT.type;  s.maxValue = maxT.value;
            }
        }
        return s;
    }

    // fit-content(<length>)
    if (tl.size() >= 11 && tl.substr(0, 11) == "fit-content") {
        s.type = GridTrackSizeType::FitContent;
        size_t open  = t.find('(');
        size_t close = t.rfind(')');
        if (open != std::string::npos && close != std::string::npos)
            s.value = StyleSheet::parseLengthPixels(StyleSheet::trim(t.substr(open + 1, close - open - 1)), emBase);
        return s;
    }

    // <length> / <percentage> / calc()
    s.type  = GridTrackSizeType::Fixed;
    s.value = StyleSheet::parseLengthPixels(t, emBase);
    return s;
}

std::vector<GridTrackSize> StyleSheet::parseGridTrackList(const std::string& value, float emBase) {
    std::vector<GridTrackSize> tracks;
    if (value.empty() || value == "none") return tracks;

    std::string v = trim(value);

    // subgrid keyword
    if (lowerAscii(v) == "subgrid") {
        GridTrackSize s;
        s.type = GridTrackSizeType::Subgrid;
        tracks.push_back(s);
        return tracks;
    }

    // Tokenise top-level (depth-aware: split on whitespace but not inside parens/brackets)
    std::vector<std::string> tokens;
    {
        std::string cur;
        int depth = 0;
        for (char c : v) {
            if (c == '(' || c == '[') { ++depth; cur += c; }
            else if (c == ')' || c == ']') { --depth; cur += c; }
            else if ((c == ' ' || c == '\t') && depth == 0) {
                if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) tokens.push_back(cur);
    }

    // Process tokens
    std::string pendingName; // named line to attach to the next track
    for (size_t i = 0; i < tokens.size(); i++) {
        const std::string& tok = tokens[i];
        std::string tl = lowerAscii(tok);

        // Named line: [line-name]
        if (!tok.empty() && tok.front() == '[' && tok.back() == ']') {
            pendingName = tok.substr(1, tok.size() - 2);
            continue;
        }

        // repeat(<count | auto-fill | auto-fit>, <track-list>)
        if (tl.size() >= 7 && tl.substr(0, 7) == "repeat(") {
            // Find the matching closing paren (depth-aware)
            size_t close = std::string::npos;
            {
                int d = 0;
                for (size_t k = 7; k < tok.size(); k++) {
                    if (tok[k] == '(') ++d;
                    else if (tok[k] == ')') { if (d == 0) { close = k; break; } --d; }
                }
            }
            if (close == std::string::npos) { pendingName.clear(); continue; }
            std::string inner = trim(tok.substr(7, close - 7));
            // Find top-level comma in inner
            size_t comma = std::string::npos;
            { int d = 0;
              for (size_t k = 0; k < inner.size(); k++) {
                if (inner[k] == '(') ++d;
                else if (inner[k] == ')') --d;
                else if (inner[k] == ',' && d == 0) { comma = k; break; }
              }
            }
            if (comma == std::string::npos) { pendingName.clear(); continue; }
            std::string countStr = trim(inner.substr(0, comma));
            std::string trackStr = trim(inner.substr(comma + 1));

            int count = 1;
            bool autoFill = false;
            std::string cl = lowerAscii(countStr);
            if (cl == "auto-fill" || cl == "auto-fit") { autoFill = true; count = 1; }
            else { try { count = std::stoi(countStr); } catch (...) {} }
            count = std::max(1, count);

            auto repeatTracks = parseGridTrackList(trackStr, emBase);
            int reps = autoFill ? 1 : count;
            for (int r = 0; r < reps; r++) {
                for (auto& rt : repeatTracks) tracks.push_back(rt);
            }
            pendingName.clear();
            continue;
        }

        GridTrackSize s = resolveGridToken(tok, emBase);
        if (!pendingName.empty()) { s.namedLine = pendingName; pendingName.clear(); }
        tracks.push_back(s);
    }
    return tracks;
}

// ============================================================
//  parseGridPlacement — Blink GridPosition parity
//
//  Parses one side of grid-column / grid-row:
//    auto | <integer> | span <integer> | <integer> span | [name]
// ============================================================
GridPlacement StyleSheet::parseGridPlacement(const std::string& value) {
    GridPlacement p;
    std::string v = trim(lowerAscii(value));
    if (v.empty() || v == "auto") return p; // PlacementType::Auto

    // span N  or  N span
    size_t spanPos = v.find("span");
    if (spanPos != std::string::npos) {
        p.type = GridPlacement::PlacementType::Span;
        std::string numPart;
        if (spanPos == 0) numPart = trim(v.substr(4));
        else              numPart = trim(v.substr(0, spanPos));
        try { p.span = std::max(1, std::stoi(numPart)); } catch (...) { p.span = 1; }
        return p;
    }

    // [name] — named line
    if (!v.empty() && v.front() == '[' && v.back() == ']') {
        p.type = GridPlacement::PlacementType::NamedLine;
        p.name = trim(value.substr(1, value.size() - 2));
        return p;
    }

    // <integer>  (positive or negative)
    try {
        p.type = GridPlacement::PlacementType::Line;
        p.line = std::stoi(v);
        return p;
    } catch (...) {}

    // plain name (area reference)
    p.type = GridPlacement::PlacementType::NamedLine;
    p.name = trim(value);
    return p;
}

// ============================================================
//  parseGridTemplateAreas — Blink GridTemplateAreas parity
//
//  Parses grid-template-areas: "header header" "nav main" "footer footer"
//  Returns a GridTemplateAreas struct with flat row-major area names.
// ============================================================
GridTemplateAreas StyleSheet::parseGridTemplateAreas(const std::string& value) {
    GridTemplateAreas result;
    if (value.empty() || value == "none") return result;

    // Split into quoted strings
    std::string v = value;
    size_t pos = 0;
    while (pos < v.size()) {
        while (pos < v.size() && (v[pos] == ' ' || v[pos] == '\t' || v[pos] == '\n' || v[pos] == '\r')) ++pos;
        if (pos >= v.size()) break;
        char quote = v[pos];
        if (quote != '"' && quote != '\'') { ++pos; continue; }
        ++pos;
        size_t start = pos;
        while (pos < v.size() && v[pos] != quote) ++pos;
        std::string row = trim(v.substr(start, pos - start));
        ++pos; // skip closing quote

        // Split row into cell names
        std::istringstream ss(row);
        std::string cell;
        int cellCount = 0;
        while (ss >> cell) {
            result.areas.push_back(cell);
            cellCount++;
        }
        if (result.columnCount == 0) result.columnCount = cellCount;
        result.rowCount++;
    }
    return result;
}

// ============================================================
//  parseFilterOperations — Blink FilterOperationResolver parity
//
//  Parses the CSS <filter-value-list> grammar including:
//    blur(<length>)                     — calc() resolved to px, clamped =0
//    brightness(<number-or-percent>)    — calc() resolved, clamped =0 (may exceed 1)
//    contrast(<number-or-percent>)      — calc() resolved, clamped =0
//    drop-shadow(<shadow>)              — offset-x offset-y [blur] [color]
//    grayscale(<number-or-percent>)     — calc() resolved, clamped [0,1]
//    hue-rotate(<angle>)                — deg/rad/grad/turn + calc(), unclamped
//    invert(<number-or-percent>)        — calc() resolved, clamped [0,1]
//    opacity(<number-or-percent>)       — calc() resolved, clamped [0,1]
//    saturate(<number-or-percent>)      — calc() resolved, clamped =0
//    sepia(<number-or-percent>)         — calc() resolved, clamped [0,1]
//    url(<string>)                      — SVG reference filter
//    luminance-to-alpha()               — Blink kLuminanceToAlpha
//    color-matrix(<20 numbers>)         — Blink kColorMatrix (feColorMatrix matrix)
//
//  calc() / min() / max() / clamp() is supported in all numeric
//  arguments via parseCSSValue() ? CSSValue::resolve(), matching
//  Blink's FilterOperationResolver::ResolveNumericArgumentForFunction.
//
//  Clamping mirrors Blink exactly (filter_operation_resolver.cc):
//    [0,1]  : grayscale, sepia, invert, opacity
//    [0,8)  : brightness, contrast, saturate (>1 is valid)
//    blur   : clamped = 0
//    hue-rotate : no clamp
//
//  UseCounter: FilterUseCounter::instance().count(FilterFeature::*)
//  is called once per parsed operation, matching Blink's
//  CountFilterUse(operationType, document).
// ============================================================
std::vector<FilterOperation> StyleSheet::parseFilterOperations(const std::string& value, float emBase) {
    std::vector<FilterOperation> ops;
    if (value.empty() || value == "none") return ops;

    // -- Resolve a numeric/percentage argument with calc() support.
    //    Mirrors Blink ResolveNumericArgumentForFunction:
    //      IsPercentage() ? computePercentage/100
    //      else           ? computeNumber
    auto resolveAmount = [&](const std::string& s) -> float {
        if (s.empty()) return 1.0f; // omitted arg ? default 1 (CSS spec)
        std::string v = trim(s);
        // Percentage in a filter function maps directly to a [0,n] ratio
        // (e.g. 180% ? 1.8), NOT a percentage of em (Blink ResolveNumericArgument).
        if (!v.empty() && v.back() == '%') {
            char* end = nullptr;
            float pct = parseLocaleIndependentFloat(v.substr(0, v.size() - 1).c_str(), &end);
            return pct / 100.0f;
        }
        // calc()/min()/max()/clamp() or bare number ? resolve numerically.
        CSSValue cv = parseCSSValue(v);
        return cv.resolve(emBase, 1920.0f, 1080.0f, emBase);
    };

    // -- Resolve an angle with calc() support (deg/rad/grad/turn).
    auto resolveAngle = [&](const std::string& s) -> float {
        if (s.empty()) return 0.0f;
        std::string v = trim(s);
        bool hasMath = v.find("calc(") != std::string::npos ||
                       v.find("min(")  != std::string::npos ||
                       v.find("max(")  != std::string::npos ||
                       v.find("clamp(")!= std::string::npos;
        if (hasMath) {
            // Route through CSSValue so calc() is resolved numerically.
            CSSValue cv = parseCSSValue(v);
            return cv.resolve(emBase, 1920.0f, 1080.0f, emBase);
        }
        return parseAngleDegrees(v);
    };

    // -- CountFilterUse (Blink CountFilterUse parity) ------
    auto countUse = [](FilterFeature f) {
        FilterUseCounter::instance().count(f);
    };

    // -- Walk the value string extracting function calls ----
    size_t pos = 0;
    const size_t len = value.size();

    while (pos < len) {
        while (pos < len && (value[pos] == ' ' || value[pos] == '\t' ||
                              value[pos] == '\r' || value[pos] == '\n'))
            ++pos;
        if (pos >= len) break;

        size_t parenOpen = value.find('(', pos);
        if (parenOpen == std::string::npos) break;

        std::string funcName = trim(value.substr(pos, parenOpen - pos));

        // Depth-aware scan for matching ')'
        size_t parenClose = parenOpen + 1;
        int depth = 1;
        while (parenClose < len && depth > 0) {
            if (value[parenClose] == '(') ++depth;
            else if (value[parenClose] == ')') --depth;
            if (depth > 0) ++parenClose;
        }
        if (depth != 0) break; // malformed

        std::string args = trim(value.substr(parenOpen + 1, parenClose - parenOpen - 1));
        pos = parenClose + 1;

        FilterOperation op;

        // -- blur(<length>) ----------------------------------
        if (funcName == "blur") {
            op.type   = FilterOperationType::Blur;
            op.amount = std::max(0.0f, parseLengthPixels(args, emBase));
            countUse(FilterFeature::Blur);

        // -- brightness(<number-or-percent>) ----------------
        } else if (funcName == "brightness") {
            op.type   = FilterOperationType::Brightness;
            op.amount = std::max(0.0f, resolveAmount(args));
            countUse(FilterFeature::Brightness);

        // -- contrast(<number-or-percent>) ------------------
        } else if (funcName == "contrast") {
            op.type   = FilterOperationType::Contrast;
            op.amount = std::max(0.0f, resolveAmount(args));
            countUse(FilterFeature::Contrast);

        // -- grayscale(<number-or-percent>) -----------------
        } else if (funcName == "grayscale") {
            op.type   = FilterOperationType::Grayscale;
            op.amount = std::clamp(resolveAmount(args), 0.0f, 1.0f); // Blink [0,1]
            countUse(FilterFeature::Grayscale);

        // -- hue-rotate(<angle>) -----------------------------
        } else if (funcName == "hue-rotate") {
            op.type   = FilterOperationType::HueRotate;
            op.amount = resolveAngle(args); // unclamped (Blink parity)
            countUse(FilterFeature::HueRotate);

        // -- invert(<number-or-percent>) ---------------------
        } else if (funcName == "invert") {
            op.type   = FilterOperationType::Invert;
            op.amount = std::clamp(resolveAmount(args), 0.0f, 1.0f); // Blink [0,1]
            countUse(FilterFeature::Invert);

        // -- opacity(<number-or-percent>) --------------------
        } else if (funcName == "opacity") {
            op.type   = FilterOperationType::Opacity;
            op.amount = std::clamp(resolveAmount(args), 0.0f, 1.0f); // Blink [0,1]
            countUse(FilterFeature::Opacity);

        // -- saturate(<number-or-percent>) -------------------
        } else if (funcName == "saturate") {
            op.type   = FilterOperationType::Saturate;
            op.amount = std::max(0.0f, resolveAmount(args));
            countUse(FilterFeature::Saturate);

        // -- sepia(<number-or-percent>) ----------------------
        } else if (funcName == "sepia") {
            op.type   = FilterOperationType::Sepia;
            op.amount = std::clamp(resolveAmount(args), 0.0f, 1.0f); // Blink [0,1]
            countUse(FilterFeature::Sepia);

        // -- drop-shadow(<shadow>) ----------------------------
        //    offset-x and offset-y required, blur-radius and color optional.
        //    Color can appear before or after the length values.
        //    Each length supports calc() via parseLengthPixels.
        } else if (funcName == "drop-shadow") {
            op.type        = FilterOperationType::DropShadow;
            op.shadowColor = Color(0, 0, 0, 1.0f); // default black (currentColor ? black)

            std::string remaining = args;

            // Extract functional color tokens depth-aware (rgb/rgba/hsl/oklch/etc.)
            auto tryExtractFuncColor = [&](std::string& s) -> bool {
                static const char* const kFuncs[] = {
                    "oklch(","oklab(","lch(","lab(","color(","hwb(",
                    "rgba(","rgb(","hsla(","hsl(",nullptr
                };
                for (int i = 0; kFuncs[i]; ++i) {
                    size_t cp = s.find(kFuncs[i]);
                    if (cp == std::string::npos) continue;
                    size_t ep = cp + strlen(kFuncs[i]);
                    int d = 1;
                    while (ep < s.size() && d > 0) {
                        if (s[ep] == '(') ++d;
                        else if (s[ep] == ')') --d;
                        if (d > 0) ++ep;
                    }
                    op.shadowColor = parseColor(trim(s.substr(cp, ep - cp + 1)));
                    s = s.substr(0, cp) + s.substr(ep + 1);
                    return true;
                }
                return false;
            };

            bool hadColor = tryExtractFuncColor(remaining);

            // Extract hex color
            if (!hadColor) {
                size_t hp = remaining.find('#');
                if (hp != std::string::npos) {
                    size_t ep = hp + 1;
                    while (ep < remaining.size() && std::isalnum((unsigned char)remaining[ep])) ++ep;
                    op.shadowColor = parseColor(remaining.substr(hp, ep - hp));
                    remaining = remaining.substr(0, hp) + remaining.substr(ep);
                    hadColor = true;
                }
            }

            // Extract named color keyword (last whitespace-token)
            if (!hadColor) {
                static const char* const kNamed[] = {
                    "black","white","red","green","blue","yellow","cyan","magenta",
                    "transparent","currentcolor","gray","grey","orange","purple",
                    "pink","brown","navy","teal","maroon","olive","lime","aqua",
                    "silver","fuchsia","coral","salmon","khaki","lavender",
                    "turquoise","violet","indigo","gold","beige",nullptr
                };
                std::string rv = trim(remaining);
                size_t lastSp = rv.rfind(' ');
                std::string lastTok = (lastSp != std::string::npos)
                                        ? trim(rv.substr(lastSp + 1)) : rv;
                std::string ltLow = lastTok;
                for (char& c : ltLow) c = (char)std::tolower((unsigned char)c);
                for (int i = 0; kNamed[i]; ++i) {
                    if (ltLow == kNamed[i]) {
                        op.shadowColor = parseColor(ltLow);
                        remaining = (lastSp != std::string::npos)
                                      ? trim(rv.substr(0, lastSp)) : "";
                        break;
                    }
                }
            }

            // Parse remaining length tokens: offset-x  offset-y  [blur-radius]
            // Each supports calc() via parseLengthPixels.
            {
                std::vector<std::string> ltoks;
                std::istringstream ss(trim(remaining));
                std::string tok;
                while (ss >> tok) ltoks.push_back(tok);
                if (ltoks.size() >= 1) op.shadowOffsetX = parseLengthPixels(ltoks[0], emBase);
                if (ltoks.size() >= 2) op.shadowOffsetY = parseLengthPixels(ltoks[1], emBase);
                if (ltoks.size() >= 3) op.shadowBlur    = std::max(0.0f, parseLengthPixels(ltoks[2], emBase));
            }
            countUse(FilterFeature::DropShadow);

        // -- url(<string>) — SVG reference filter ------------
        } else if (funcName == "url") {
            op.type = FilterOperationType::Reference;
            std::string u = args;
            if (!u.empty() && (u.front() == '"' || u.front() == '\'')) u = u.substr(1);
            if (!u.empty() && (u.back()  == '"' || u.back()  == '\'')) u.pop_back();
            op.url = u;
            countUse(FilterFeature::Reference);

        // -- luminance-to-alpha() — Blink kLuminanceToAlpha --
        //    SVG feColorMatrix type="luminanceToAlpha". No arguments.
        } else if (funcName == "luminance-to-alpha") {
            op.type   = FilterOperationType::LuminanceToAlpha;
            op.amount = 0.0f;
            countUse(FilterFeature::LuminanceToAlpha);

        // -- color-matrix(<20 values>) — Blink kColorMatrix --
        //    SVG feColorMatrix type="matrix". Expects 20 space/comma-
        //    separated numbers (4 rows × 5 columns). Each value supports
        //    calc() via parseCSSValue. Padded/trimmed to exactly 20.
        } else if (funcName == "color-matrix") {
            op.type = FilterOperationType::ColorMatrix;
            std::vector<float> vals;
            {
                std::string s = args;
                for (char& c : s) if (c == ',') c = ' ';
                std::istringstream ss(s);
                std::string tok;
                float vpW = 1920.0f, vpH = 1080.0f;
                if (auto* app = Application::instance()) {
                    vpW = app->stylesheet().viewportWidth();
                    vpH = app->stylesheet().viewportHeight();
                }
                while (ss >> tok) {
                    CSSValue cv = parseCSSValue(tok);
                    vals.push_back(cv.resolve(emBase, vpW, vpH, emBase));
                }
            }
            vals.resize(20, 0.0f); // Blink requires exactly 20 values
            op.colorMatrixValues = std::move(vals);
            countUse(FilterFeature::ColorMatrix);

        } else {
            // Unknown function — skip (Blink silently ignores unknown filter functions)
            continue;
        }

        ops.push_back(std::move(op));
    }
    return ops;
}
bool StyleSheet::parseTransformOrigin(const std::string& value, Vec2& xy, float& z) {
    TransformOrigin origin = parseTransformOrigin(value);
    auto toFloat = [](const CSSValue& val) -> float {
        return val.resolve(1.0f);
    };
    xy.x = toFloat(origin.x);
    xy.y = toFloat(origin.y);
    z = toFloat(origin.z);
    return true;
}
bool StyleSheet::parseTransformStyle(const std::string& value, TransformStyle& out) {
    std::string s = lowerAscii(trim(value));
    if (s == "preserve-3d") {
        out = TransformStyle::Preserve3D;
        return true;
    }
    if (s == "flat") {
        out = TransformStyle::Flat;
        return true;
    }
    return false;
}
bool StyleSheet::parseTransformBox(const std::string& value, TransformBox& out) {
    std::string s = lowerAscii(trim(value));
    if (s == "content-box") { out = TransformBox::ContentBox; return true; }
    if (s == "fill-box") { out = TransformBox::FillBox; return true; }
    if (s == "stroke-box") { out = TransformBox::StrokeBox; return true; }
    if (s == "view-box") { out = TransformBox::ViewBox; return true; }
    if (s == "border-box") { out = TransformBox::BorderBox; return true; }
    return false;
}
bool StyleSheet::parsePerspective(const std::string& value, float& out) {
    std::string s = lowerAscii(trim(value));
    if (s == "none") {
        out = 0.0f;
        return true;
    }
    CSSValue val = parseCSSValue(s);
    if (val.isSet()) {
        out = val.resolve(1.0f);
        return true;
    }
    return false;
}
bool StyleSheet::parseBackfaceVisibility(const std::string& value, BackfaceVisibility& out) {
    std::string s = lowerAscii(trim(value));
    if (s == "visible") { out = BackfaceVisibility::Visible; return true; }
    if (s == "hidden") { out = BackfaceVisibility::Hidden; return true; }
    return false;
}
}
