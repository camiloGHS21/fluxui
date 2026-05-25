#include "fluxui/compositor.h"
#include "fluxui/css_parser.h"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace FluxUI {

// Blink-style Cubic Bezier Solver (UnitBezier parity)
static float evaluateCubicBezier(float t, float x1, float y1, float x2, float y2) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    // Polynomial coefficients
    float cx = 3.0f * x1;
    float bx = 3.0f * (x2 - x1) - cx;
    float ax = 1.0f - cx - bx;

    float cy = 3.0f * y1;
    float by = 3.0f * (y2 - y1) - cy;
    float ay = 1.0f - cy - by;

    auto sampleCurveX = [=](float t_val) {
        return ((ax * t_val + bx) * t_val + cx) * t_val;
    };
    auto sampleCurveY = [=](float t_val) {
        return ((ay * t_val + by) * t_val + cy) * t_val;
    };
    auto sampleCurveDerivativeX = [=](float t_val) {
        return (3.0f * ax * t_val + 2.0f * bx) * t_val + cx;
    };

    // Newton-Raphson method
    float x = t;
    float t_guess = t;
    for (int i = 0; i < 8; i++) {
        float x_est = sampleCurveX(t_guess) - x;
        if (std::abs(x_est) < 1e-6f) {
            return sampleCurveY(t_guess);
        }
        float d = sampleCurveDerivativeX(t_guess);
        if (std::abs(d) < 1e-6f) {
            break;
        }
        t_guess -= x_est / d;
    }

    // Binary search fallback if Newton-Raphson failed to converge
    float low = 0.0f;
    float high = 1.0f;
    t_guess = t;

    if (t_guess < low) return sampleCurveY(low);
    if (t_guess > high) return sampleCurveY(high);

    while (low < high) {
        float x_est = sampleCurveX(t_guess);
        if (std::abs(x_est - x) < 1e-4f) {
            return sampleCurveY(t_guess);
        }
        if (x > x_est) {
            low = t_guess;
        } else {
            high = t_guess;
        }
        t_guess = (high + low) * 0.5f;
    }

    return sampleCurveY(t_guess);
}

CompositorEngine& CompositorEngine::instance() {
    static CompositorEngine inst;
    return inst;
}

CompositorEngine::CompositorEngine() {
    start();
}

CompositorEngine::~CompositorEngine() {
    stop();
}

void CompositorEngine::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&CompositorEngine::threadLoop, this);
}

void CompositorEngine::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CompositorEngine::animatePropertyFloat(uintptr_t widgetId, const std::string& propName,
                                          float fromVal, float toVal, float duration,
                                          TimingFunctionType type,
                                          float springStiffness, float springDamping) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = std::to_string(widgetId) + ":" + propName;
    auto& anim = animations_[key];
    anim.widgetId = widgetId;
    anim.propertyName = propName;
    anim.timingFunction = type;
    anim.springStiffness = springStiffness;
    anim.springDamping = springDamping;
    anim.duration = std::max(0.001f, duration);
    anim.elapsed = 0.0f;
    anim.fromFloat = fromVal;
    anim.toFloat = toVal;
    anim.currentFloat = fromVal;
    anim.floatVelocity = 0.0f;
    anim.active = true;

    // Set standard Bezier params if applicable
    if (type == TimingFunctionType::Ease) {
        anim.cubicBezierParams[0] = 0.25f; anim.cubicBezierParams[1] = 0.1f;
        anim.cubicBezierParams[2] = 0.25f; anim.cubicBezierParams[3] = 1.0f;
    } else if (type == TimingFunctionType::EaseIn) {
        anim.cubicBezierParams[0] = 0.42f; anim.cubicBezierParams[1] = 0.0f;
        anim.cubicBezierParams[2] = 1.0f;  anim.cubicBezierParams[3] = 1.0f;
    } else if (type == TimingFunctionType::EaseOut) {
        anim.cubicBezierParams[0] = 0.0f;  anim.cubicBezierParams[1] = 0.0f;
        anim.cubicBezierParams[2] = 0.58f; anim.cubicBezierParams[3] = 1.0f;
    } else if (type == TimingFunctionType::EaseInOut) {
        anim.cubicBezierParams[0] = 0.42f; anim.cubicBezierParams[1] = 0.0f;
        anim.cubicBezierParams[2] = 0.58f; anim.cubicBezierParams[3] = 1.0f;
    }
}

void CompositorEngine::animatePropertyValue(uintptr_t widgetId, const std::string& propName,
                                          const std::string& fromVal, const std::string& toVal,
                                          const std::string& syntax, float duration,
                                          TimingFunctionType type,
                                          float springStiffness, float springDamping) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = std::to_string(widgetId) + ":" + propName;
    auto& anim = animations_[key];
    anim.widgetId = widgetId;
    anim.propertyName = propName;
    anim.timingFunction = type;
    anim.springStiffness = springStiffness;
    anim.springDamping = springDamping;
    anim.duration = std::max(0.001f, duration);
    anim.elapsed = 0.0f;
    anim.fromValue = fromVal;
    anim.toValue = toVal;
    anim.currentValue = fromVal;
    anim.syntax = syntax;
    anim.active = true;

    // Set standard Bezier params if applicable
    if (type == TimingFunctionType::Ease) {
        anim.cubicBezierParams[0] = 0.25f; anim.cubicBezierParams[1] = 0.1f;
        anim.cubicBezierParams[2] = 0.25f; anim.cubicBezierParams[3] = 1.0f;
    } else if (type == TimingFunctionType::EaseIn) {
        anim.cubicBezierParams[0] = 0.42f; anim.cubicBezierParams[1] = 0.0f;
        anim.cubicBezierParams[2] = 1.0f;  anim.cubicBezierParams[3] = 1.0f;
    } else if (type == TimingFunctionType::EaseOut) {
        anim.cubicBezierParams[0] = 0.0f;  anim.cubicBezierParams[1] = 0.0f;
        anim.cubicBezierParams[2] = 0.58f; anim.cubicBezierParams[3] = 1.0f;
    } else if (type == TimingFunctionType::EaseInOut) {
        anim.cubicBezierParams[0] = 0.42f; anim.cubicBezierParams[1] = 0.0f;
        anim.cubicBezierParams[2] = 0.58f; anim.cubicBezierParams[3] = 1.0f;
    }
}

bool CompositorEngine::getAnimatedFloat(uintptr_t widgetId, const std::string& propName, float& outValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = std::to_string(widgetId) + ":" + propName;
    auto it = animations_.find(key);
    if (it != animations_.end()) {
        outValue = it->second.currentFloat;
        return true;
    }
    return false;
}

bool CompositorEngine::getAnimatedValue(uintptr_t widgetId, const std::string& propName, std::string& outValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = std::to_string(widgetId) + ":" + propName;
    auto it = animations_.find(key);
    if (it != animations_.end()) {
        outValue = it->second.currentValue;
        return true;
    }
    return false;
}

bool CompositorEngine::hasAnimations(uintptr_t widgetId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string prefix = std::to_string(widgetId) + ":";
    for (const auto& [key, anim] : animations_) {
        if (anim.active && key.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

void CompositorEngine::threadLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (running_) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        // Cap dt to avoid massive leaps on frame drops
        dt = std::clamp(dt, 0.001f, 0.05f);

        tick(dt);

        // Target ~120 FPS ticking loop
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

void CompositorEngine::tick(float dt) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, anim] : animations_) {
        if (!anim.active) continue;

        anim.elapsed += dt;
        float progress = std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f);

        // Apply Timing Function / Easing curves
        float curveT = progress;
        if (anim.timingFunction == TimingFunctionType::Linear) {
            curveT = progress;
        } else if (anim.timingFunction == TimingFunctionType::Spring) {
            // Semi-implicit Euler numerical integration for spring physics
            float force = -anim.springStiffness * (anim.currentFloat - anim.toFloat) - anim.springDamping * anim.floatVelocity;
            anim.floatVelocity += force * dt;
            anim.currentFloat += anim.floatVelocity * dt;

            // Spring progress completion threshold check
            if (std::abs(anim.currentFloat - anim.toFloat) < 0.0001f && std::abs(anim.floatVelocity) < 0.0001f) {
                anim.currentFloat = anim.toFloat;
                anim.active = false;
            }
            continue;
        } else {
            curveT = evaluateCubicBezier(progress, anim.cubicBezierParams[0], anim.cubicBezierParams[1],
                                         anim.cubicBezierParams[2], anim.cubicBezierParams[3]);
        }

        // Interpolate Floats
        if (anim.fromValue.empty() && anim.toValue.empty()) {
            anim.currentFloat = anim.fromFloat + (anim.toFloat - anim.fromFloat) * curveT;
            if (progress >= 1.0f) {
                anim.currentFloat = anim.toFloat;
                anim.active = false;
            }
        }
        // Interpolate Complex Typed Values (Blink @property parities)
        else {
            anim.currentValue = StyleSheet::interpolateTypedValue(anim.fromValue, anim.toValue, curveT, anim.syntax);
            if (progress >= 1.0f) {
                anim.currentValue = anim.toValue;
                anim.active = false;
            }
        }
    }
}

} // namespace FluxUI
