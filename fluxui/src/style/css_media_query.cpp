// FluxUI - CSS @media query evaluation.
// Extracted from css_parser.cpp: StyleSheet::mediaQueryMatches plus its
// helpers (splitMediaAndClauses, parseMediaFeature). Evaluates media features
// (width/height/orientation/prefers-* etc.) against the current viewport.
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
using detail::parseNumberToken;

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

} // namespace FluxUI
