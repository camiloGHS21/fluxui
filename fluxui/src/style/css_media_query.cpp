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
// Parse a value that may be a ratio (W/H), a length, a resolution, or a number.
static float parseRatioValue(const std::string& v) {
    auto slash = v.find('/');
    if (slash != std::string::npos) {
        float w = parseNumberToken(trimLocal(v.substr(0, slash)));
        float h = parseNumberToken(trimLocal(v.substr(slash + 1)));
        return h != 0.0f ? w / h : 0.0f;
    }
    // bare number is a ratio of N/1
    return parseNumberToken(v);
}
static float parseResolutionDppx(const std::string& v) {
    float num = parseNumberToken(v);
    if (v.find("dppx") != std::string::npos || v.find("x") != std::string::npos) {
        return num;
    }
    if (v.find("dpcm") != std::string::npos) {
        return num * 2.54f / 96.0f;
    }
    // default unit dpi
    return num / 96.0f;
}
static bool parseMediaFeature(const std::string& clause,
                              const StyleSheet& sheet) {
    float viewportWidth = sheet.viewportWidth();
    float viewportHeight = sheet.viewportHeight();
    std::string c = trimLocal(lowerAscii(clause));
    if (c.size() >= 2 && c.front() == '(' && c.back() == ')') {
        c = trimLocal(c.substr(1, c.size() - 2));
    }
    if (c.empty()) return true;

    // ── Modern two-sided range syntax: (a <= width <= b), (a < height < b) ──
    {
        // Find a feature name surrounded by comparison operators.
        auto hasRange = [&](const char* feat) -> int {
            return (int)(c.find(feat) != std::string::npos);
        };
        if ((hasRange("width") || hasRange("height") || hasRange("aspect-ratio")) &&
            (c.find('<') != std::string::npos || c.find('>') != std::string::npos)) {
            // Tokenize around the feature name.
            std::string feat = c.find("aspect-ratio") != std::string::npos ? "aspect-ratio"
                              : (c.find("width") != std::string::npos ? "width" : "height");
            size_t fpos = c.find(feat);
            std::string left = trimLocal(c.substr(0, fpos));
            std::string right = trimLocal(c.substr(fpos + feat.size()));
            float metric = feat == "width" ? viewportWidth
                         : feat == "height" ? viewportHeight
                         : (viewportHeight != 0.0f ? viewportWidth / viewportHeight : 0.0f);
            auto toVal = [&](const std::string& s) {
                return feat == "aspect-ratio" ? parseRatioValue(s) : [&]{
                    float n = parseNumberToken(s);
                    if (s.find("rem") != std::string::npos || s.find("em") != std::string::npos) n *= 16.0f;
                    return n;
                }();
            };
            bool ok = true;
            // left side: "<value> <op>"
            if (!left.empty()) {
                if (left.size() >= 2 && left.substr(left.size()-2) == "<=") {
                    ok = ok && toVal(trimLocal(left.substr(0, left.size()-2))) <= metric;
                } else if (left.back() == '<') {
                    ok = ok && toVal(trimLocal(left.substr(0, left.size()-1))) < metric;
                } else if (left.size() >= 2 && left.substr(left.size()-2) == ">=") {
                    ok = ok && toVal(trimLocal(left.substr(0, left.size()-2))) >= metric;
                } else if (left.back() == '>') {
                    ok = ok && toVal(trimLocal(left.substr(0, left.size()-1))) > metric;
                }
            }
            if (!right.empty()) {
                if (right.rfind("<=", 0) == 0) {
                    ok = ok && metric <= toVal(trimLocal(right.substr(2)));
                } else if (right.rfind("<", 0) == 0) {
                    ok = ok && metric < toVal(trimLocal(right.substr(1)));
                } else if (right.rfind(">=", 0) == 0) {
                    ok = ok && metric >= toVal(trimLocal(right.substr(2)));
                } else if (right.rfind(">", 0) == 0) {
                    ok = ok && metric > toVal(trimLocal(right.substr(1)));
                }
            }
            if (!left.empty() || !right.empty()) return ok;
        }
    }

    auto colon = c.find(':');
    if (colon != std::string::npos) {
        std::string name = trimLocal(c.substr(0, colon));
        std::string value = trimLocal(c.substr(colon + 1));
        float numeric = parseNumberToken(value);
        if (value.find("rem") != std::string::npos ||
            value.find("em") != std::string::npos) {
            numeric *= 16.0f;
        }
        if (name == "min-width" || name == "min-device-width") return viewportWidth >= numeric;
        if (name == "max-width" || name == "max-device-width") return viewportWidth <= numeric;
        if (name == "width" || name == "device-width") return std::abs(viewportWidth - numeric) < 0.5f;
        if (name == "min-height" || name == "min-device-height") return viewportHeight >= numeric;
        if (name == "max-height" || name == "max-device-height") return viewportHeight <= numeric;
        if (name == "height" || name == "device-height") return std::abs(viewportHeight - numeric) < 0.5f;
        if (name == "orientation") {
            bool landscape = viewportWidth >= viewportHeight;
            return (value == "landscape" && landscape) ||
                   (value == "portrait" && !landscape);
        }
        // aspect-ratio family (W/H ratios)
        if (name == "aspect-ratio" || name == "min-aspect-ratio" || name == "max-aspect-ratio") {
            float want = parseRatioValue(value);
            float have = viewportHeight != 0.0f ? viewportWidth / viewportHeight : 0.0f;
            if (name == "min-aspect-ratio") return have >= want;
            if (name == "max-aspect-ratio") return have <= want;
            return std::abs(have - want) < 0.01f;
        }
        // resolution family (dppx after normalization)
        if (name == "resolution" || name == "min-resolution" || name == "max-resolution") {
            float want = parseResolutionDppx(value);
            float have = sheet.resolutionDppx();
            if (name == "min-resolution") return have >= want - 1e-4f;
            if (name == "max-resolution") return have <= want + 1e-4f;
            return std::abs(have - want) < 1e-3f;
        }
        if (name == "prefers-color-scheme") {
            return value == (sheet.prefersDark() ? "dark" : "light");
        }
        if (name == "forced-colors") {
            return value == (sheet.forcedColors() ? "active" : "none");
        }
        if (name == "prefers-reduced-motion") {
            return value == (sheet.prefersReducedMotion() ? "reduce" : "no-preference");
        }
        if (name == "prefers-reduced-transparency") {
            return value == (sheet.prefersReducedTransparency() ? "reduce" : "no-preference");
        }
        if (name == "prefers-reduced-data") {
            return value == (sheet.prefersReducedData() ? "reduce" : "no-preference");
        }
        if (name == "prefers-contrast") {
            return value == sheet.prefersContrast() ||
                   (value == "no-preference" && sheet.prefersContrast() == "no-preference");
        }
        if (name == "pointer" || name == "any-pointer") {
            return value == sheet.pointerType();
        }
        if (name == "hover" || name == "any-hover") {
            return value == (sheet.hoverCapable() ? "hover" : "none");
        }
        if (name == "scripting") {
            return value == "enabled";  // FluxUI always runs with scripting.
        }
        return false;
    }

    // ── Boolean feature context (no value): (hover), (pointer), (color) ──
    if (c == "hover") return sheet.hoverCapable();
    if (c == "pointer" || c == "any-pointer") return sheet.pointerType() != "none";
    if (c == "color") return true;          // assume a color display
    if (c == "monochrome") return false;
    if (c == "grid") return false;          // bitmap display

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
        // Each comma-alternative is a sequence of `and`-joined clauses, where a
        // clause may itself be an `or`-group of features (Media Queries 4).
        for (std::string clause : splitMediaAndClauses(alternative)) {
            clause = trimLocal(clause);
            if (clause == "all" || clause == "screen") {
                continue;
            }
            if (clause == "print" || clause == "speech") {
                matches = false;
                break;
            }
            // Split this clause on top-level ` or ` and OR the parts together.
            std::vector<std::string> orParts;
            {
                std::string cur;
                int depth = 0;
                const std::string& lc = clause;  // already lowercased
                for (size_t i = 0; i < clause.size();) {
                    char ch = clause[i];
                    if (ch == '(') depth++;
                    else if (ch == ')' && depth > 0) depth--;
                    if (depth == 0 && i + 4 <= clause.size() &&
                        lc.compare(i, 4, " or ") == 0) {
                        orParts.push_back(trimLocal(cur));
                        cur.clear();
                        i += 4;
                        continue;
                    }
                    cur += ch;
                    ++i;
                }
                orParts.push_back(trimLocal(cur));
            }
            bool clauseOk = false;
            for (const auto& part : orParts) {
                if (part.empty()) continue;
                if (parseMediaFeature(part, *this)) { clauseOk = true; break; }
            }
            if (!clauseOk) {
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
