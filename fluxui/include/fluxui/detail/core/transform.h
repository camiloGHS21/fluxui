#pragma once
// FluxUI public API - transforms (depends on geometry + css_value).
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
#include "fluxui/detail/core/css_value.h"
namespace FluxUI {
struct Transform2D {
    float m00 = 1.0f, m01 = 0.0f, m02 = 0.0f;
    float m10 = 0.0f, m11 = 1.0f, m12 = 0.0f;
    Transform2D() = default;
    Transform2D(float m00, float m01, float m02, float m10, float m11, float m12)
        : m00(m00), m01(m01), m02(m02), m10(m10), m11(m11), m12(m12) {}
    static Transform2D identity() {
        return Transform2D(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    static Transform2D fromTranslate(float tx, float ty) {
        return Transform2D(1.0f, 0.0f, tx, 0.0f, 1.0f, ty);
    }
    static Transform2D fromScale(float sx, float sy) {
        return Transform2D(sx, 0.0f, 0.0f, 0.0f, sy, 0.0f);
    }
    static Transform2D fromRotate(float angleRad) {
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        return Transform2D(c, -s, 0.0f, s, c, 0.0f);
    }
    static Transform2D fromSkew(float axRad, float ayRad) {
        return Transform2D(1.0f, std::tan(axRad), 0.0f, std::tan(ayRad), 1.0f, 0.0f);
    }
    bool isIdentity() const {
        return m00 == 1.0f && m01 == 0.0f && m02 == 0.0f &&
               m10 == 0.0f && m11 == 1.0f && m12 == 0.0f;
    }
    Transform2D multiplied(const Transform2D& o) const {
        return Transform2D(
            m00 * o.m00 + m01 * o.m10,
            m00 * o.m01 + m01 * o.m11,
            m00 * o.m02 + m01 * o.m12 + m02,
            m10 * o.m00 + m11 * o.m10,
            m10 * o.m01 + m11 * o.m11,
            m10 * o.m02 + m11 * o.m12 + m12
        );
    }
    Vec2 mapPoint(const Vec2& p) const {
        return Vec2(
            m00 * p.x + m01 * p.y + m02,
            m10 * p.x + m11 * p.y + m12
        );
    }
    Transform2D inverse() const {
        float det = m00 * m11 - m01 * m10;
        if (std::abs(det) < 1e-9f) {
            return identity();
        }
        float invDet = 1.0f / det;
        return Transform2D(
            m11 * invDet, -m01 * invDet, (m01 * m12 - m11 * m02) * invDet,
            -m10 * invDet, m00 * invDet, (m10 * m02 - m00 * m12) * invDet
        );
    }
    Vec2 inverseMapPoint(const Vec2& p) const {
        return inverse().mapPoint(p);
    }
};
enum class TransformOperationType {
    Translate,
    Translate3d,
    TranslateX,
    TranslateY,
    TranslateZ,
    Scale,
    Scale3d,
    ScaleX,
    ScaleY,
    ScaleZ,
    Rotate,
    Rotate3d,
    RotateX,
    RotateY,
    RotateZ,
    Skew,
    SkewX,
    SkewY,
    Matrix,
    Matrix3d,
    Perspective
};
struct TransformOperation {
    enum Kind {
        Translate,
        Scale,
        Rotate,
        Skew,
        Matrix,
        Perspective
    };
    TransformOperationType type;
    std::vector<CSSValue> args;
    Kind kind = Translate;
    float v[16] = {0};
    int dim = 2;
    TransformOperation() = default;
    bool operator==(const TransformOperation& o) const {
        return type == o.type && args == o.args;
    }
    bool operator!=(const TransformOperation& o) const {
        return !(*this == o);
    }
    void resolveFromArgs() {
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        std::fill(std::begin(v), std::end(v), 0.0f);
        int t = (int)type;
        if (t <= 4) {
            kind = Translate;
            dim = (t == 1 || t == 4) ? 3 : 2;
            if (t == 0) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f);
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f);
            } else if (t == 1) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f);
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f);
                if (args.size() >= 3) v[2] = args[2].resolve(1.0f);
            } else if (t == 2) {
                if (!args.empty()) v[0] = args[0].resolve(1.0f);
            } else if (t == 3) {
                if (!args.empty()) v[1] = args[0].resolve(1.0f);
            } else if (t == 4) {
                if (!args.empty()) v[2] = args[0].resolve(1.0f);
            }
        } else if (t <= 9) {
            kind = Scale;
            dim = (t == 6 || t == 9) ? 3 : 2;
            v[0] = 1.0f; v[1] = 1.0f; v[2] = 1.0f;
            if (t == 5) {
                if (args.size() >= 1) {
                    v[0] = args[0].resolve(1.0f);
                    v[1] = (args.size() >= 2) ? args[1].resolve(1.0f) : args[0].resolve(1.0f);
                }
            } else if (t == 6) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f);
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f);
                if (args.size() >= 3) v[2] = args[2].resolve(1.0f);
            } else if (t == 7) {
                if (!args.empty()) v[0] = args[0].resolve(1.0f);
            } else if (t == 8) {
                if (!args.empty()) v[1] = args[0].resolve(1.0f);
            } else if (t == 9) {
                if (!args.empty()) v[2] = args[0].resolve(1.0f);
            }
        } else if (t <= 14) {
            kind = Rotate;
            dim = (t == 11) ? 3 : 2;
            if (t == 11) {
                if (args.size() >= 4) {
                    v[0] = args[0].resolve(1.0f);
                    v[1] = args[1].resolve(1.0f);
                    v[2] = args[2].resolve(1.0f);
                    v[3] = args[3].resolve(1.0f) * kDegToRad;
                }
            } else {
                if (!args.empty()) v[0] = args[0].resolve(1.0f) * kDegToRad;
            }
        } else if (t <= 17) {
            kind = Skew;
            dim = 2;
            if (t == 15) {
                if (args.size() >= 1) v[0] = args[0].resolve(1.0f) * kDegToRad;
                if (args.size() >= 2) v[1] = args[1].resolve(1.0f) * kDegToRad;
            } else if (t == 16) {
                if (!args.empty()) v[0] = args[0].resolve(1.0f) * kDegToRad;
            } else if (t == 17) {
                if (!args.empty()) v[1] = args[0].resolve(1.0f) * kDegToRad;
            }
        } else if (t <= 19) {
            kind = Matrix;
            dim = (t == 19) ? 3 : 2;
            for (size_t i = 0; i < args.size() && i < 16; ++i) {
                v[i] = args[i].resolve(1.0f);
            }
        } else {
            kind = Perspective;
            dim = 2;
            if (!args.empty()) v[0] = args[0].resolve(1.0f);
        }
    }
    static TransformOperation scaleOp(float sx, float sy) {
        TransformOperation op;
        op.type = TransformOperationType::Scale;
        op.kind = Scale;
        op.dim = 2;
        op.v[0] = sx;
        op.v[1] = sy;
        op.args = { CSSValue(sx, CSSValue::None), CSSValue(sy, CSSValue::None) };
        return op;
    }
    static TransformOperation rotate(float angleRad) {
        TransformOperation op;
        op.type = TransformOperationType::Rotate;
        op.kind = Rotate;
        op.dim = 2;
        op.v[0] = angleRad;
        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
        op.args = { CSSValue(angleRad * kRadToDeg, CSSValue::Deg) };
        return op;
    }
    static TransformOperation blend(const TransformOperation& a, const TransformOperation& b, float t) {
        if (a.kind != b.kind) {
            return t < 0.5f ? a : b;
        }
        TransformOperation res = a;
        if (a.kind == Rotate) {
            constexpr float kPi = 3.14159265358979323846f;
            float diff = b.v[0] - a.v[0];
            diff = std::remainder(diff, 2.0f * kPi);
            res.v[0] = a.v[0] + diff * t;
            constexpr float kRadToDeg = 180.0f / kPi;
            res.args = { CSSValue(res.v[0] * kRadToDeg, CSSValue::Deg) };
        } else {
            int limit = (a.kind == Matrix) ? 16 : 4;
            for (int i = 0; i < limit; ++i) {
                res.v[i] = a.v[i] + (b.v[i] - a.v[i]) * t;
            }
            res.args.clear();
            if (res.kind == Translate) {
                if (res.dim == 3) {
                    res.args = { CSSValue(res.v[0], CSSValue::Px), CSSValue(res.v[1], CSSValue::Px), CSSValue(res.v[2], CSSValue::Px) };
                } else {
                    res.args = { CSSValue(res.v[0], CSSValue::Px), CSSValue(res.v[1], CSSValue::Px) };
                }
            } else if (res.kind == Scale) {
                if (res.dim == 3) {
                    res.args = { CSSValue(res.v[0], CSSValue::None), CSSValue(res.v[1], CSSValue::None), CSSValue(res.v[2], CSSValue::None) };
                } else {
                    res.args = { CSSValue(res.v[0], CSSValue::None), CSSValue(res.v[1], CSSValue::None) };
                }
            } else if (res.kind == Skew) {
                res.args = { CSSValue(res.v[0] * (180.0f / 3.14159265f), CSSValue::Deg), CSSValue(res.v[1] * (180.0f / 3.14159265f), CSSValue::Deg) };
            } else if (res.kind == Matrix) {
                if (res.dim == 3) {
                    for (int i = 0; i < 16; ++i) res.args.push_back(CSSValue(res.v[i], CSSValue::None));
                } else {
                    for (int i = 0; i < 6; ++i) res.args.push_back(CSSValue(res.v[i], CSSValue::None));
                }
            } else if (res.kind == Perspective) {
                res.args = { CSSValue(res.v[0], CSSValue::Px) };
            }
        }
        return res;
    }
    Transform2D toTransform2D() const {
        if (kind == Translate) {
            return Transform2D::fromTranslate(v[0], v[1]);
        } else if (kind == Scale) {
            return Transform2D::fromScale(v[0], v[1]);
        } else if (kind == Rotate) {
            return Transform2D::fromRotate(v[0]);
        } else if (kind == Skew) {
            return Transform2D::fromSkew(v[0], v[1]);
        } else if (kind == Matrix) {
            return Transform2D(v[0], v[2], v[4], v[1], v[3], v[5]);
        }
        return Transform2D::identity();
    }
    Transform2D apply(const Transform2D& matrix, const Vec2& origin) const {
        Transform2D opMatrix = this->toTransform2D();
        Transform2D t = Transform2D::fromTranslate(origin.x, origin.y)
                          .multiplied(opMatrix)
                          .multiplied(Transform2D::fromTranslate(-origin.x, -origin.y));
        return matrix.multiplied(t);
    }
};
struct TransformOrigin {
    CSSValue x = CSSValue::pct(50.0f);
    CSSValue y = CSSValue::pct(50.0f);
    CSSValue z = CSSValue::px(0.0f);
    bool operator==(const TransformOrigin& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const TransformOrigin& o) const {
        return !(*this == o);
    }
};
enum class TransformStyle {
    Flat,
    Preserve3D,
    Preserve3d = Preserve3D
};
enum class TransformBox {
    ContentBox,
    BorderBox,
    FillBox,
    StrokeBox,
    ViewBox
};
struct PerspectiveOrigin {
    CSSValue x = CSSValue::pct(50.0f);
    CSSValue y = CSSValue::pct(50.0f);
    bool operator==(const PerspectiveOrigin& o) const {
        return x == o.x && y == o.y;
    }
    bool operator!=(const PerspectiveOrigin& o) const {
        return !(*this == o);
    }
};
enum class BackfaceVisibility {
    Visible,
    Hidden
};
}