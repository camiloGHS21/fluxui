// FluxUI - CSS transform / grid / filter value parsers.
// Extracted from css_parser.cpp: StyleSheet members that parse the geometry
// value grammars (transform list, transform-origin/style/box, perspective,
// grid-template tracks/areas/placement, filter functions).
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
using detail::splitWhitespace;

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
//  parseGridTrackList � Blink NGGridLayoutAlgorithm parity
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
//  parseGridPlacement � Blink GridPosition parity
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

    // [name] � named line
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
//  parseGridTemplateAreas � Blink GridTemplateAreas parity
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
//  parseFilterOperations � Blink FilterOperationResolver parity
//
//  Parses the CSS <filter-value-list> grammar including:
//    blur(<length>)                     � calc() resolved to px, clamped =0
//    brightness(<number-or-percent>)    � calc() resolved, clamped =0 (may exceed 1)
//    contrast(<number-or-percent>)      � calc() resolved, clamped =0
//    drop-shadow(<shadow>)              � offset-x offset-y [blur] [color]
//    grayscale(<number-or-percent>)     � calc() resolved, clamped [0,1]
//    hue-rotate(<angle>)                � deg/rad/grad/turn + calc(), unclamped
//    invert(<number-or-percent>)        � calc() resolved, clamped [0,1]
//    opacity(<number-or-percent>)       � calc() resolved, clamped [0,1]
//    saturate(<number-or-percent>)      � calc() resolved, clamped =0
//    sepia(<number-or-percent>)         � calc() resolved, clamped [0,1]
//    url(<string>)                      � SVG reference filter
//    luminance-to-alpha()               � Blink kLuminanceToAlpha
//    color-matrix(<20 numbers>)         � Blink kColorMatrix (feColorMatrix matrix)
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

        // -- url(<string>) � SVG reference filter ------------
        } else if (funcName == "url") {
            op.type = FilterOperationType::Reference;
            std::string u = args;
            if (!u.empty() && (u.front() == '"' || u.front() == '\'')) u = u.substr(1);
            if (!u.empty() && (u.back()  == '"' || u.back()  == '\'')) u.pop_back();
            op.url = u;
            countUse(FilterFeature::Reference);

        // -- luminance-to-alpha() � Blink kLuminanceToAlpha --
        //    SVG feColorMatrix type="luminanceToAlpha". No arguments.
        } else if (funcName == "luminance-to-alpha") {
            op.type   = FilterOperationType::LuminanceToAlpha;
            op.amount = 0.0f;
            countUse(FilterFeature::LuminanceToAlpha);

        // -- color-matrix(<20 values>) � Blink kColorMatrix --
        //    SVG feColorMatrix type="matrix". Expects 20 space/comma-
        //    separated numbers (4 rows � 5 columns). Each value supports
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
            // Unknown function � skip (Blink silently ignores unknown filter functions)
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

} // namespace FluxUI