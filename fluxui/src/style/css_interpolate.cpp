// FluxUI - CSS animation interpolation + @property syntax validation.
// Extracted from css_parser.cpp: StyleSheet::isValidSyntax (the @property
// <syntax> matcher) and the value/transform/transform-origin interpolation
// used to animate between two computed values.
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include "css_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace FluxUI {
using detail::trimLocal;
using detail::lowerAscii;
using detail::splitWhitespace;
using detail::splitWhitespaceTopLevel;

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


} // namespace FluxUI