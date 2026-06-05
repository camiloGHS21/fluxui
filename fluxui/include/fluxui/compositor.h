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

    // Keyframe-animation override layer (Blink animation-compositor). These store
    // instantaneous values written by an @keyframes animation effect (no tween) and are
    // preferred over the tweened "animations_" entries when read from paint code.
    void setKeyframeFloat(uintptr_t widgetId, const std::string& propName, float value);
    void setKeyframeValue(uintptr_t widgetId, const std::string& propName, const std::string& value);
    bool getKeyframeFloat(uintptr_t widgetId, const std::string& propName, float& outValue) const;
    bool getKeyframeValue(uintptr_t widgetId, const std::string& propName, std::string& outValue) const;
    void clearKeyframeOverrides(uintptr_t widgetId);

    // Check if widget is currently being animated on the compositor
    bool hasAnimations(uintptr_t widgetId);

    // Compositor thread off-main-thread scrolling interface (Blink style)
    void registerScrollableWidget(uintptr_t widgetId, const Rect& bounds, float contentHeight,
                                  float scrollY, float targetScrollY, int depth, bool isScrollable);
    bool handleMouseWheel(float x, float y, float dy);
    float getScrollY(uintptr_t widgetId);
    float getTargetScrollY(uintptr_t widgetId);
    void setScrollY(uintptr_t widgetId, float scrollY, float targetScrollY);
    void unregisterWidget(uintptr_t widgetId);

private:
    CompositorEngine();
    ~CompositorEngine();

    void threadLoop();
    // Advance all animations/scrolls by dt. Returns true if any are still
    // in motion (used to throttle the background thread when idle).
    bool tick(float dt);

    std::thread thread_;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CompositorAnimation> animations_; // Key: "widgetId:propName"

    // Keyframe-driven @keyframes override layer. The set/get path takes the same lock
    // as `animations_` so a write here is visible to the read path immediately.
    struct KeyframeOverride {
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, std::string> values;
    };
    std::unordered_map<uintptr_t, KeyframeOverride> keyframeOverrides_;

    struct CompositorScroll {
        uintptr_t widgetId = 0;
        Rect bounds;
        float contentHeight = 0.0f;
        float scrollY = 0.0f;
        float targetScrollY = 0.0f;
        float maxScrollY = 0.0f;
        int depth = 0;
        bool isScrollable = false;
    };
    std::unordered_map<uintptr_t, CompositorScroll> scrolls_;
};

} // namespace FluxUI
