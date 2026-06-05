#pragma once
// FluxUI public API - CSSValue, units, calc() math expressions, timing.
// Auto-split from core.h; do not include directly, use <fluxui/core.h>.
#include "fluxui/config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "fluxui/detail/core/geometry.h"
namespace FluxUI {
struct TimingFunction {
    enum Kind {
        Linear,
        Ease,
        EaseIn,
        EaseOut,
        EaseInOut,
        CubicBezier,
        StepStart,
        StepEnd,
        Steps
    };
    Kind kind = Kind::Ease;
    float params[4] = {0.25f, 0.1f, 0.25f, 1.0f};
    int stepCount = 1;
    enum StepPosition { JumpStart, JumpEnd, JumpNone, JumpBoth, Start, End };
    StepPosition stepPosition = JumpEnd;
    static TimingFunction linear()    { TimingFunction t; t.kind = Linear;    return t; }
    static TimingFunction ease()      { TimingFunction t; t.kind = Ease;      return t; }
    static TimingFunction easeIn()    { TimingFunction t; t.kind = EaseIn;    t.params[0]=0.42f; t.params[1]=0.0f; t.params[2]=1.0f; t.params[3]=1.0f; return t; }
    static TimingFunction easeOut()   { TimingFunction t; t.kind = EaseOut;   t.params[0]=0.0f;  t.params[1]=0.0f; t.params[2]=0.58f; t.params[3]=1.0f; return t; }
    static TimingFunction easeInOut() { TimingFunction t; t.kind = EaseInOut; t.params[0]=0.42f; t.params[1]=0.0f; t.params[2]=0.58f; t.params[3]=1.0f; return t; }
    static TimingFunction stepStart() { TimingFunction t; t.kind = StepStart; return t; }
    static TimingFunction stepEnd()   { TimingFunction t; t.kind = StepEnd;   return t; }
    static TimingFunction steps(int n, StepPosition pos = JumpEnd) {
        TimingFunction t; t.kind = Steps; t.stepCount = std::max(1, n); t.stepPosition = pos; return t;
    }
    static TimingFunction bezier(float x1, float y1, float x2, float y2) {
        TimingFunction t; t.kind = CubicBezier;
        t.params[0] = x1; t.params[1] = y1; t.params[2] = x2; t.params[3] = y2;
        return t;
    }
};
class CSSMathExpressionNode;
struct CSSValue {
    enum Unit {
        Px, Percent, Auto, None, Vw, Vh, Em, Rem, MinContent, MaxContent, FitContent, Ch, Lh, Vi, Vb, Dvw, Dvh,
        Vmin, Vmax, Rlh, Ex, Ic, Cap,
        SvW, SvH, SvMin, SvMax,
        LvW, LvH, LvMin, LvMax,
        DvMin, DvMax,
        Cqw, Cqh, Cqi, Cqb, Cqmin, Cqmax,
        Deg, Rad, Grad, Turn
    };
    float value = 0;
    Unit unit = None;
    enum CalcOp { CalcNone, CalcAdd, CalcSub, CalcMul, CalcDiv, CalcMin, CalcMax, CalcClamp };
    CalcOp calcOp = CalcNone;
    float calcValue2 = 0;
    Unit calcUnit2 = None;
    float calcValue3 = 0;
    Unit calcUnit3 = None;
    std::shared_ptr<CSSMathExpressionNode> mathExpr;
    CSSValue() = default;
    CSSValue(float v, Unit u = Px) : value(v), unit(u) {}
    CSSValue(std::shared_ptr<CSSMathExpressionNode> expr);
    operator float() const { return resolve(1.0f); }
    static CSSValue px(float v) { return {v, Px}; }
    static CSSValue pct(float v) { return {v, Percent}; }
    static CSSValue autoVal() { return {0, Auto}; }
    static CSSValue vw(float v) { return {v, Vw}; }
    static CSSValue vh(float v) { return {v, Vh}; }
    static CSSValue em(float v) { return {v, Em}; }
    static CSSValue rem(float v) { return {v, Rem}; }
    static CSSValue ch(float v) { return {v, Ch}; }
    static CSSValue lh(float v) { return {v, Lh}; }
    static CSSValue vi(float v) { return {v, Vi}; }
    static CSSValue vb(float v) { return {v, Vb}; }
    static CSSValue dvw(float v) { return {v, Dvw}; }
    static CSSValue dvh(float v) { return {v, Dvh}; }
    static CSSValue vmin(float v) { return {v, Vmin}; }
    static CSSValue vmax(float v) { return {v, Vmax}; }
    static CSSValue rlh(float v) { return {v, Rlh}; }
    static CSSValue ex(float v) { return {v, Ex}; }
    static CSSValue ic(float v) { return {v, Ic}; }
    static CSSValue cap(float v) { return {v, Cap}; }
    static CSSValue svw(float v) { return {v, SvW}; }
    static CSSValue svh(float v) { return {v, SvH}; }
    static CSSValue svmin(float v) { return {v, SvMin}; }
    static CSSValue svmax(float v) { return {v, SvMax}; }
    static CSSValue lvw(float v) { return {v, LvW}; }
    static CSSValue lvh(float v) { return {v, LvH}; }
    static CSSValue lvmin(float v) { return {v, LvMin}; }
    static CSSValue lvmax(float v) { return {v, LvMax}; }
    static CSSValue dvmin(float v) { return {v, DvMin}; }
    static CSSValue dvmax(float v) { return {v, DvMax}; }
    static CSSValue cqw(float v) { return {v, Cqw}; }
    static CSSValue cqh(float v) { return {v, Cqh}; }
    static CSSValue cqi(float v) { return {v, Cqi}; }
    static CSSValue cqb(float v) { return {v, Cqb}; }
    static CSSValue cqmin(float v) { return {v, Cqmin}; }
    static CSSValue cqmax(float v) { return {v, Cqmax}; }
    static CSSValue minContent() { return {0, MinContent}; }
    static CSSValue maxContent() { return {0, MaxContent}; }
    static CSSValue fitContent() { return {0, FitContent}; }
    float resolve(float parentSize, float viewportW = 1920.0f, float viewportH = 1080.0f, float emBase = 16.0f) const;
    bool isAuto() const { return unit == Auto; }
    bool isSet() const;
    bool isIntrinsic() const { return unit == MinContent || unit == MaxContent || unit == FitContent; }
    bool operator==(const CSSValue& o) const;
    bool operator!=(const CSSValue& o) const { return !(*this == o); }
    static float resolveUnit(float val, Unit u, float parentSize, float vpW, float vpH, float emBase) {
        float vminVal = std::min(vpW, vpH);
        float vmaxVal = std::max(vpW, vpH);
        switch (u) {
            case Percent: return val * parentSize / 100.0f;
            case Px: return val;
            case Vw: return val * vpW / 100.0f;
            case Vh: return val * vpH / 100.0f;
            case Em: return val * emBase;
            case Rem: return val * 16.0f;
            case Ch: return val * emBase * 0.5f;
            case Lh: return val * emBase * 1.2f;
            case Vi: return val * vpW / 100.0f;
            case Vb: return val * vpH / 100.0f;
            case Dvw: return val * vpW / 100.0f;
            case Dvh: return val * vpH / 100.0f;
            case Vmin: return val * vminVal / 100.0f;
            case Vmax: return val * vmaxVal / 100.0f;
            case Rlh: return val * 16.0f * 1.2f;
            case Ex: return val * emBase * 0.5f;
            case Ic: return val * emBase;
            case Cap: return val * emBase * 0.7f;
            case SvW: return val * vpW / 100.0f;
            case SvH: return val * vpH / 100.0f;
            case SvMin: return val * vminVal / 100.0f;
            case SvMax: return val * vmaxVal / 100.0f;
            case LvW: return val * vpW / 100.0f;
            case LvH: return val * vpH / 100.0f;
            case LvMin: return val * vminVal / 100.0f;
            case LvMax: return val * vmaxVal / 100.0f;
            case DvMin: return val * vminVal / 100.0f;
            case DvMax: return val * vmaxVal / 100.0f;
            case Cqw: return val * (parentSize > 0.0f ? parentSize : vpW) / 100.0f;
            case Cqh: return val * (parentSize > 0.0f ? parentSize : vpH) / 100.0f;
            case Cqi: return val * (parentSize > 0.0f ? parentSize : vpW) / 100.0f;
            case Cqb: return val * (parentSize > 0.0f ? parentSize : vpH) / 100.0f;
            case Cqmin: {
                float cSize = parentSize > 0.0f ? parentSize : vminVal;
                return val * cSize / 100.0f;
            }
            case Cqmax: {
                float cSize = parentSize > 0.0f ? parentSize : vmaxVal;
                return val * cSize / 100.0f;
            }
            default: return val;
        }
    }
};
enum class MathNodeType {
    Value,
    BinaryOp,
    Function
};
class CSSMathExpressionNode {
public:
    virtual ~CSSMathExpressionNode() = default;
    virtual float resolve(float parentSize, float viewportW, float viewportH, float emBase) const = 0;
    virtual MathNodeType type() const = 0;
    virtual bool isEqual(const CSSMathExpressionNode& other) const = 0;
};
class CSSMathExpressionValue : public CSSMathExpressionNode {
public:
    float value;
    CSSValue::Unit unit;
    CSSMathExpressionValue(float v, CSSValue::Unit u) : value(v), unit(u) {}
    float resolve(float parentSize, float viewportW, float viewportH, float emBase) const override {
        return CSSValue::resolveUnit(value, unit, parentSize, viewportW, viewportH, emBase);
    }
    MathNodeType type() const override { return MathNodeType::Value; }
    bool isEqual(const CSSMathExpressionNode& other) const override {
        if (other.type() != MathNodeType::Value) return false;
        auto& o = static_cast<const CSSMathExpressionValue&>(other);
        return value == o.value && unit == o.unit;
    }
};
class CSSMathExpressionBinaryOp : public CSSMathExpressionNode {
public:
    enum Op { Add, Sub, Mul, Div };
    Op op;
    std::shared_ptr<CSSMathExpressionNode> left;
    std::shared_ptr<CSSMathExpressionNode> right;
    CSSMathExpressionBinaryOp(Op o, std::shared_ptr<CSSMathExpressionNode> l, std::shared_ptr<CSSMathExpressionNode> r)
        : op(o), left(l), right(r) {}
    float resolve(float parentSize, float viewportW, float viewportH, float emBase) const override {
        float lVal = left ? left->resolve(parentSize, viewportW, viewportH, emBase) : 0.0f;
        float rVal = right ? right->resolve(parentSize, viewportW, viewportH, emBase) : 0.0f;
        switch (op) {
            case Add: return lVal + rVal;
            case Sub: return lVal - rVal;
            case Mul: return lVal * rVal;
            case Div: return rVal != 0.0f ? lVal / rVal : 0.0f;
            default: return 0.0f;
        }
    }
    MathNodeType type() const override { return MathNodeType::BinaryOp; }
    bool isEqual(const CSSMathExpressionNode& other) const override {
        if (other.type() != MathNodeType::BinaryOp) return false;
        auto& o = static_cast<const CSSMathExpressionBinaryOp&>(other);
        if (op != o.op) return false;
        bool leftEqual = (!left && !o.left) || (left && o.left && left->isEqual(*o.left));
        bool rightEqual = (!right && !o.right) || (right && o.right && right->isEqual(*o.right));
        return leftEqual && rightEqual;
    }
};
class CSSMathExpressionFunction : public CSSMathExpressionNode {
public:
    enum Func { Min, Max, Clamp };
    Func func;
    std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
    CSSMathExpressionFunction(Func f, std::vector<std::shared_ptr<CSSMathExpressionNode>> a)
        : func(f), args(a) {}
    float resolve(float parentSize, float viewportW, float viewportH, float emBase) const override {
        if (args.empty()) return 0.0f;
        if (func == Min) {
            float minVal = args[0]->resolve(parentSize, viewportW, viewportH, emBase);
            for (size_t i = 1; i < args.size(); ++i) {
                minVal = std::min(minVal, args[i]->resolve(parentSize, viewportW, viewportH, emBase));
            }
            return minVal;
        } else if (func == Max) {
            float maxVal = args[0]->resolve(parentSize, viewportW, viewportH, emBase);
            for (size_t i = 1; i < args.size(); ++i) {
                maxVal = std::max(maxVal, args[i]->resolve(parentSize, viewportW, viewportH, emBase));
            }
            return maxVal;
        } else if (func == Clamp) {
            if (args.size() < 3) return 0.0f;
            float minVal = args[0]->resolve(parentSize, viewportW, viewportH, emBase);
            float prefVal = args[1]->resolve(parentSize, viewportW, viewportH, emBase);
            float maxVal = args[2]->resolve(parentSize, viewportW, viewportH, emBase);
            return prefVal < minVal ? minVal : (prefVal > maxVal ? maxVal : prefVal);
        }
        return 0.0f;
    }
    MathNodeType type() const override { return MathNodeType::Function; }
    bool isEqual(const CSSMathExpressionNode& other) const override {
        if (other.type() != MathNodeType::Function) return false;
        auto& o = static_cast<const CSSMathExpressionFunction&>(other);
        if (func != o.func) return false;
        if (args.size() != o.args.size()) return false;
        for (size_t i = 0; i < args.size(); ++i) {
            if (!args[i]->isEqual(*o.args[i])) return false;
        }
        return true;
    }
};
class CSSMathExpressionParser {
public:
    static std::shared_ptr<CSSMathExpressionNode> parse(const std::string& str) {
        CSSMathExpressionParser parser(str);
        return parser.parseExpression();
    }
private:
    std::string src;
    size_t pos = 0;
    CSSMathExpressionParser(const std::string& s) : src(s) {}
    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) {
            pos++;
        }
    }
    char peek() {
        skipWhitespace();
        if (pos >= src.size()) return '\0';
        return src[pos];
    }
    char next() {
        skipWhitespace();
        if (pos >= src.size()) return '\0';
        return src[pos++];
    }
    bool consume(char expected) {
        skipWhitespace();
        if (pos < src.size() && src[pos] == expected) {
            pos++;
            return true;
        }
        return false;
    }
    bool consumeWord(const std::string& word) {
        skipWhitespace();
        if (pos + word.size() <= src.size() && src.compare(pos, word.size(), word) == 0) {
            char nextChar = pos + word.size() < src.size() ? src[pos + word.size()] : '\0';
            if (nextChar == '(' || nextChar == ' ' || nextChar == '\t' || nextChar == '\0' || nextChar == ',' || nextChar == ')') {
                pos += word.size();
                return true;
            }
        }
        return false;
    }
    std::shared_ptr<CSSMathExpressionNode> parseExpression() {
        auto node = parseTerm();
        if (!node) return nullptr;
        while (true) {
            skipWhitespace();
            char opChar = peek();
            if (opChar == '+' || opChar == '-') {
                next();
                CSSMathExpressionBinaryOp::Op op = (opChar == '+') ? CSSMathExpressionBinaryOp::Add : CSSMathExpressionBinaryOp::Sub;
                auto right = parseTerm();
                if (!right) return nullptr;
                node = std::make_shared<CSSMathExpressionBinaryOp>(op, node, right);
            } else {
                break;
            }
        }
        return node;
    }
    std::shared_ptr<CSSMathExpressionNode> parseTerm() {
        auto node = parseFactor();
        if (!node) return nullptr;
        while (true) {
            skipWhitespace();
            char opChar = peek();
            if (opChar == '*' || opChar == '/') {
                next();
                CSSMathExpressionBinaryOp::Op op = (opChar == '*') ? CSSMathExpressionBinaryOp::Mul : CSSMathExpressionBinaryOp::Div;
                auto right = parseFactor();
                if (!right) return nullptr;
                node = std::make_shared<CSSMathExpressionBinaryOp>(op, node, right);
            } else {
                break;
            }
        }
        return node;
    }
    std::shared_ptr<CSSMathExpressionNode> parseFactor() {
        skipWhitespace();
        char c = peek();
        if (c == '\0') return nullptr;
        if (consume('(')) {
            auto node = parseExpression();
            consume(')');
            return node;
        }
        if (consumeWord("calc")) {
            if (consume('(')) {
                auto node = parseExpression();
                consume(')');
                return node;
            }
        }
        if (consumeWord("min")) {
            if (consume('(')) {
                std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
                do {
                    auto arg = parseExpression();
                    if (arg) args.push_back(arg);
                } while (consume(','));
                consume(')');
                return std::make_shared<CSSMathExpressionFunction>(CSSMathExpressionFunction::Min, args);
            }
        }
        if (consumeWord("max")) {
            if (consume('(')) {
                std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
                do {
                    auto arg = parseExpression();
                    if (arg) args.push_back(arg);
                } while (consume(','));
                consume(')');
                return std::make_shared<CSSMathExpressionFunction>(CSSMathExpressionFunction::Max, args);
            }
        }
        if (consumeWord("clamp")) {
            if (consume('(')) {
                std::vector<std::shared_ptr<CSSMathExpressionNode>> args;
                do {
                    auto arg = parseExpression();
                    if (arg) args.push_back(arg);
                } while (consume(','));
                consume(')');
                return std::make_shared<CSSMathExpressionFunction>(CSSMathExpressionFunction::Clamp, args);
            }
        }
        return parseValue();
    }
    std::shared_ptr<CSSMathExpressionNode> parseValue() {
        skipWhitespace();
        if (pos >= src.size()) return nullptr;
        size_t start = pos;
        if (src[pos] == '+' || src[pos] == '-') {
            pos++;
        }
        bool hasDigits = false;
        while (pos < src.size() && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '.')) {
            hasDigits = true;
            pos++;
        }
        if (!hasDigits) {
            return nullptr;
        }
        std::string numStr = src.substr(start, pos - start);
        float val = parseLocaleIndependentFloat(numStr, 0.0f);
        CSSValue::Unit unit = CSSValue::None;
        if (pos < src.size() && src[pos] == '%') {
            unit = CSSValue::Percent;
            pos++;
        } else {
            std::string suffix;
            while (pos < src.size() && ((src[pos] >= 'a' && src[pos] <= 'z') || (src[pos] >= 'A' && src[pos] <= 'Z'))) {
                suffix += src[pos++];
            }
            if (!suffix.empty()) {
                std::string lowerSuffix = lowerAscii(suffix);
                if (lowerSuffix == "px") unit = CSSValue::Px;
                else if (lowerSuffix == "em") unit = CSSValue::Em;
                else if (lowerSuffix == "rem") unit = CSSValue::Rem;
                else if (lowerSuffix == "vw") unit = CSSValue::Vw;
                else if (lowerSuffix == "vh") unit = CSSValue::Vh;
                else if (lowerSuffix == "ch") unit = CSSValue::Ch;
                else if (lowerSuffix == "lh") unit = CSSValue::Lh;
                else if (lowerSuffix == "vi") unit = CSSValue::Vi;
                else if (lowerSuffix == "vb") unit = CSSValue::Vb;
                else if (lowerSuffix == "dvw") unit = CSSValue::Dvw;
                else if (lowerSuffix == "dvh") unit = CSSValue::Dvh;
                else if (lowerSuffix == "vmin") unit = CSSValue::Vmin;
                else if (lowerSuffix == "vmax") unit = CSSValue::Vmax;
                else if (lowerSuffix == "rlh") unit = CSSValue::Rlh;
                else if (lowerSuffix == "ex") unit = CSSValue::Ex;
                else if (lowerSuffix == "ic") unit = CSSValue::Ic;
                else if (lowerSuffix == "cap") unit = CSSValue::Cap;
                else if (lowerSuffix == "svw") unit = CSSValue::SvW;
                else if (lowerSuffix == "svh") unit = CSSValue::SvH;
                else if (lowerSuffix == "svmin") unit = CSSValue::SvMin;
                else if (lowerSuffix == "svmax") unit = CSSValue::SvMax;
                else if (lowerSuffix == "lvw") unit = CSSValue::LvW;
                else if (lowerSuffix == "lvh") unit = CSSValue::LvH;
                else if (lowerSuffix == "lvmin") unit = CSSValue::LvMin;
                else if (lowerSuffix == "lvmax") unit = CSSValue::LvMax;
                else if (lowerSuffix == "dvmin") unit = CSSValue::DvMin;
                else if (lowerSuffix == "dvmax") unit = CSSValue::DvMax;
                else if (lowerSuffix == "cqw") unit = CSSValue::Cqw;
                else if (lowerSuffix == "cqh") unit = CSSValue::Cqh;
                else if (lowerSuffix == "cqi") unit = CSSValue::Cqi;
                else if (lowerSuffix == "cqb") unit = CSSValue::Cqb;
                else if (lowerSuffix == "cqmin") unit = CSSValue::Cqmin;
                else if (lowerSuffix == "cqmax") unit = CSSValue::Cqmax;
                else {
                    unit = CSSValue::Px;
                }
            } else {
                unit = CSSValue::Px;
            }
        }
        return std::make_shared<CSSMathExpressionValue>(val, unit);
    }
    static std::string lowerAscii(const std::string& str) {
        std::string res = str;
        for (char& c : res) {
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }
        return res;
    }
};
inline CSSValue::CSSValue(std::shared_ptr<CSSMathExpressionNode> expr) : value(0), unit(None), mathExpr(expr) {}
inline float CSSValue::resolve(float parentSize, float viewportW, float viewportH, float emBase) const {
    if (mathExpr) {
        return mathExpr->resolve(parentSize, viewportW, viewportH, emBase);
    }
    float primary = resolveUnit(value, unit, parentSize, viewportW, viewportH, emBase);
    if (calcOp == CalcNone) return primary;
    float secondary = resolveUnit(calcValue2, calcUnit2, parentSize, viewportW, viewportH, emBase);
    switch (calcOp) {
        case CalcAdd: return primary + secondary;
        case CalcSub: return primary - secondary;
        case CalcMul: return primary * secondary;
        case CalcDiv: return secondary != 0.0f ? primary / secondary : 0.0f;
        case CalcMin: return std::min(primary, secondary);
        case CalcMax: return std::max(primary, secondary);
        case CalcClamp: {
            float tertiary = resolveUnit(calcValue3, calcUnit3, parentSize, viewportW, viewportH, emBase);
            return primary < secondary ? secondary : (primary > tertiary ? tertiary : primary);
        }
        default: return primary;
    }
}
inline bool CSSValue::isSet() const {
    return unit != None || mathExpr != nullptr;
}
inline bool CSSValue::operator==(const CSSValue& o) const {
    if (unit != o.unit || value != o.value || calcOp != o.calcOp ||
        calcValue2 != o.calcValue2 || calcUnit2 != o.calcUnit2 ||
        calcValue3 != o.calcValue3 || calcUnit3 != o.calcUnit3) {
        return false;
    }
    if (!mathExpr && !o.mathExpr) return true;
    if (!mathExpr || !o.mathExpr) return false;
    return mathExpr->isEqual(*o.mathExpr);
}
struct FastCustomProperties {
    std::shared_ptr<std::unordered_map<std::string, std::string>> map;
    FastCustomProperties() = default;
    FastCustomProperties(const FastCustomProperties& other) : map(other.map) {}
    FastCustomProperties& operator=(const FastCustomProperties& other) {
        if (this != &other) {
            map = other.map;
        }
        return *this;
    }
    FastCustomProperties(FastCustomProperties&& other) noexcept : map(std::move(other.map)) {}
    FastCustomProperties& operator=(FastCustomProperties&& other) noexcept {
        map = std::move(other.map);
        return *this;
    }
    FastCustomProperties(const std::unordered_map<std::string, std::string>& other) {
        if (!other.empty()) {
            map = std::make_shared<std::unordered_map<std::string, std::string>>(other);
        }
    }
    FastCustomProperties& operator=(const std::unordered_map<std::string, std::string>& other) {
        if (other.empty()) {
            map.reset();
        } else {
            map = std::make_shared<std::unordered_map<std::string, std::string>>(other);
        }
        return *this;
    }
    const std::unordered_map<std::string, std::string>* getMapPointer() const {
        return map.get();
    }
    std::unordered_map<std::string, std::string>* getMapPointer() {
        return map.get();
    }
    void ensureUnique() {
        if (!map) {
            map = std::make_shared<std::unordered_map<std::string, std::string>>();
        } else if (map.use_count() > 1) {
            map = std::make_shared<std::unordered_map<std::string, std::string>>(*map);
        }
    }
    std::unordered_map<std::string, std::string>& getOrCreateMap() {
        ensureUnique();
        return *map;
    }
    operator const std::unordered_map<std::string, std::string>&() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? *map : emptyMap;
    }
    auto begin() {
        ensureUnique();
        return map->begin();
    }
    auto begin() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->begin() : emptyMap.begin();
    }
    auto end() {
        ensureUnique();
        return map->end();
    }
    auto end() const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->end() : emptyMap.end();
    }
    size_t size() const { return map ? map->size() : 0; }
    bool empty() const { return !map || map->empty(); }
    void clear() { map.reset(); }
    std::string& operator[](const std::string& key) {
        ensureUnique();
        return (*map)[key];
    }
    auto find(const std::string& key) {
        ensureUnique();
        return map->find(key);
    }
    auto find(const std::string& key) const {
        static const std::unordered_map<std::string, std::string> emptyMap;
        return map ? map->find(key) : emptyMap.find(key);
    }
};
}