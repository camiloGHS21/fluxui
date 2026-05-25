// FluxUI Compositor Thread / Timing Engine (Blink style)
// Delegates and runs layout-independent transitions/animations on a separate thread,
// ensuring fluid rendering even if the main thread is blocked.
#pragma once

#include "core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace FluxUI {

enum class TimingFunctionType {
    Linear,
    Ease,
    EaseIn,
    EaseOut,
    EaseInOut,
    CubicBezier,
    Spring
};

struct CompositorAnimation {
    uintptr_t widgetId = 0;
    std::string propertyName; // "opacity", "scale", "translation", or custom @property name
    TimingFunctionType timingFunction = TimingFunctionType::Ease;
    float cubicBezierParams[4] = {0.25f, 0.1f, 0.25f, 1.0f}; // x1, y1, x2, y2
    float springStiffness = 180.0f;
    float springDamping = 18.0f;

    // Animation state
    float duration = 0.15f; // seconds
    float elapsed = 0.0f;   // seconds
    bool active = false;

    // Float transitions
    float fromFloat = 0.0f;
    float toFloat = 0.0f;
    float currentFloat = 0.0f;
    float floatVelocity = 0.0f;

    // Typed string transitions (for colors, lengths, custom properties)
    std::string fromValue;
    std::string toValue;
    std::string currentValue;
    std::string syntax; // Registered @property syntax type
};

class CompositorEngine {
public:
    static CompositorEngine& instance();

    void start();
    void stop();

    // Main thread registers compositor animation
    void animatePropertyFloat(uintptr_t widgetId, const std::string& propName,
                              float fromVal, float toVal, float duration,
                              TimingFunctionType type = TimingFunctionType::Ease,
                              float springStiffness = 180.0f, float springDamping = 18.0f);

    void animatePropertyValue(uintptr_t widgetId, const std::string& propName,
                              const std::string& fromVal, const std::string& toVal,
                              const std::string& syntax, float duration,
                              TimingFunctionType type = TimingFunctionType::Ease,
                              float springStiffness = 180.0f, float springDamping = 18.0f);

    // Compositor/GPU thread queries animated overrides for painting
    bool getAnimatedFloat(uintptr_t widgetId, const std::string& propName, float& outValue);
    bool getAnimatedValue(uintptr_t widgetId, const std::string& propName, std::string& outValue);

    // Check if widget is currently being animated on the compositor
    bool hasAnimations(uintptr_t widgetId);

private:
    CompositorEngine();
    ~CompositorEngine();

    void threadLoop();
    void tick(float dt);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::unordered_map<std::string, CompositorAnimation> animations_; // Key: "widgetId:propName"
};

} // namespace FluxUI
