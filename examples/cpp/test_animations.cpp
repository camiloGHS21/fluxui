// FluxUI CSS Animations & @keyframes Parity Test
// Mirrors W3C CSS Animations Level 1 examples and Blink's keyframe_resolver tests.
//
// Reference: https://www.w3.org/TR/css-animations-1/
//            chromium/src/third_party/blink/renderer/core/css/keyframes_test.cc

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace FluxUI;

static void testParseKeyframes() {
    std::cout << "[1] Parsing @keyframes rules..." << std::endl;
    StyleSheet sheet;
    sheet.parse(
        "@keyframes fadeIn {"
        "  from { opacity: 0; transform: scale(0.8); }"
        "  50%  { opacity: 0.5; }"
        "  to   { opacity: 1; transform: scale(1); }"
        "}"
        "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }"
    );

    assert(sheet.keyframesRules.size() == 2);

    const auto* fadeIn = sheet.findKeyframes("fadeIn");
    assert(fadeIn != nullptr);
    assert(fadeIn->keyframes.size() == 3);
    // Sorted ascending by min keyTime: 0% (from), 50%, 100% (to)
    assert(std::abs(fadeIn->keyframes[0].keyTimes[0] - 0.0f) < 1e-6f);
    assert(std::abs(fadeIn->keyframes[1].keyTimes[0] - 0.5f) < 1e-6f);
    assert(std::abs(fadeIn->keyframes[2].keyTimes[0] - 1.0f) < 1e-6f);

    const auto* pulse = sheet.findKeyframes("pulse");
    assert(pulse != nullptr);
    assert(pulse->keyframes.size() == 2);
    // Comma-grouped: 0%, 100% in the first keyframe
    assert(pulse->keyframes[0].keyTimes.size() == 2);
    assert(std::abs(pulse->keyframes[0].keyTimes[0] - 0.0f) < 1e-6f);
    assert(std::abs(pulse->keyframes[0].keyTimes[1] - 1.0f) < 1e-6f);

    std::cout << "  PASS - @keyframes rules parsed with sort + comma-grouping." << std::endl;
}

static void testParseAnimationShorthand() {
    std::cout << "[2] Parsing `animation` shorthand..." << std::endl;
    StyleSheet sheet;
    sheet.parse(
        ".box { animation: fadeIn 0.4s ease-in 0.1s 3 alternate both paused; }"
    );

    auto widget = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, widget.get());

    assert(s.rare().animationName.size() == 1);
    assert(s.rare().animationName[0] == "fadeIn");
    assert(s.rare().animationDuration.size() == 1);
    assert(std::abs(s.rare().animationDuration[0] - 0.4f) < 1e-6f);
    assert(s.rare().animationDelay.size() == 1);
    assert(std::abs(s.rare().animationDelay[0] - 0.1f) < 1e-6f);
    assert(s.rare().animationIterationCount.size() == 1);
    assert(std::abs(s.rare().animationIterationCount[0] - 3.0f) < 1e-6f);
    assert(s.rare().animationDirection.size() == 1);
    assert(s.rare().animationDirection[0] == AnimationDirection::Alternate);
    assert(s.rare().animationFillMode.size() == 1);
    assert(s.rare().animationFillMode[0] == AnimationFillMode::Both);
    assert(s.rare().animationPlayState.size() == 1);
    assert(s.rare().animationPlayState[0] == AnimationPlayState::Paused);
    assert(s.rare().animationTimingFunction.size() == 1);
    assert(s.rare().animationTimingFunction[0].kind == TimingFunction::Kind::EaseIn);

    std::cout << "  PASS - shorthand decomposed into all 8 lists." << std::endl;
}

static void testParseTransitionShorthand() {
    std::cout << "[3] Parsing `transition` shorthand..." << std::endl;
    StyleSheet sheet;
    sheet.parse(
        ".box { transition: opacity 200ms ease-in-out 50ms, "
        "transform 400ms cubic-bezier(0.4, 0, 0.2, 1) allow-discrete; }"
    );

    auto widget = std::make_shared<Panel>("box");
    Style s = sheet.resolve("box", "", "panel", {}, nullptr, widget.get());

    std::cout << "DEBUG: transitionProperty size = " << s.rare().transitionProperty.size() << std::endl;
    for (size_t i = 0; i < s.rare().transitionProperty.size(); ++i) {
        std::cout << "DEBUG: transitionProperty[" << i << "] = '" << s.rare().transitionProperty[i] << "'" << std::endl;
    }
    assert(s.rare().transitionProperty.size() == 2);
    assert(s.rare().transitionProperty[0] == "opacity");
    assert(s.rare().transitionProperty[1] == "transform");
    assert(std::abs(s.rare().transitionDurations[0] - 0.2f) < 1e-6f);
    assert(std::abs(s.rare().transitionDurations[1] - 0.4f) < 1e-6f);
    assert(std::abs(s.rare().transitionDelays[0] - 0.05f) < 1e-6f);
    assert(std::abs(s.rare().transitionDelays[1] - 0.0f) < 1e-6f);
    assert(s.rare().transitionTimingFunctions[0].kind == TimingFunction::Kind::EaseInOut);
    assert(s.rare().transitionTimingFunctions[1].kind == TimingFunction::Kind::CubicBezier);
    assert(std::abs(s.rare().transitionTimingFunctions[1].params[0] - 0.4f) < 1e-6f);
    assert(std::abs(s.rare().transitionTimingFunctions[1].params[2] - 0.2f) < 1e-6f);
    assert(s.rare().transitionBehavior[0] == TransitionBehavior::Normal);
    assert(s.rare().transitionBehavior[1] == TransitionBehavior::AllowDiscrete);

    // Legacy scalar should be populated from the first duration.
    assert(std::abs(s.transitionDuration - 0.2f) < 1e-6f);

    std::cout << "  PASS - transition shorthand with cubic-bezier + allow-discrete." << std::endl;
}

static void testTimingFunctionSampling() {
    std::cout << "[4] Sampling timing functions..." << std::endl;
    // Linear: y == x
    auto linear = TimingFunction::linear();
    assert(std::abs(StyleSheet::sampleTimingFunction(linear, 0.0f) - 0.0f) < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(linear, 0.5f) - 0.5f) < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(linear, 1.0f) - 1.0f) < 1e-6f);

    // Step-end: 0 for x < 1, 1 for x == 1
    auto stepEnd = TimingFunction::stepEnd();
    assert(std::abs(StyleSheet::sampleTimingFunction(stepEnd, 0.0f) - 0.0f) < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(stepEnd, 0.999f) - 0.0f) < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(stepEnd, 1.0f) - 1.0f) < 1e-6f);

    // Steps(4, jump-end): 0.0, 0.25, 0.5, 0.75 buckets
    auto steps4 = TimingFunction::steps(4, TimingFunction::JumpEnd);
    assert(std::abs(StyleSheet::sampleTimingFunction(steps4, 0.0f)  - 0.0f)  < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(steps4, 0.24f) - 0.0f)  < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(steps4, 0.25f) - 0.25f) < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(steps4, 0.5f)  - 0.5f)  < 1e-6f);
    assert(std::abs(StyleSheet::sampleTimingFunction(steps4, 0.99f) - 0.75f) < 1e-6f);

    // Cubic-bezier(.42, 0, 1, 1) is the CSS "ease-in" curve, monotone increasing.
    auto easeIn = TimingFunction::easeIn();
    float y025 = StyleSheet::sampleTimingFunction(easeIn, 0.25f);
    float y050 = StyleSheet::sampleTimingFunction(easeIn, 0.5f);
    float y075 = StyleSheet::sampleTimingFunction(easeIn, 0.75f);
    assert(y025 < y050);
    assert(y050 < y075);
    assert(y025 < 0.25f); // ease-in is slower than linear at the start
    assert(y075 < 0.75f); // and also below diagonal (slower) because it starts slow

    std::cout << "  PASS - linear, step-end, steps(4), cubic-bezier all correct." << std::endl;
}

static void testAnimationSubpropertyParse() {
    std::cout << "[5] Parsing all animation-* sub-properties..." << std::endl;
    StyleSheet sheet;
    sheet.parse(
        ".x {"
        "  animation-name: spin, fadeIn;"
        "  animation-duration: 1s, 0.4s;"
        "  animation-delay: 0.2s, 0s;"
        "  animation-iteration-count: infinite, 2;"
        "  animation-direction: normal, alternate-reverse;"
        "  animation-fill-mode: forwards, both;"
        "  animation-play-state: running, paused;"
        "  animation-timing-function: ease-in, steps(3, jump-start);"
        "  animation-composition: replace, add;"
        "}"
    );

    auto widget = std::make_shared<Panel>("x");
    Style s = sheet.resolve("x", "", "panel", {}, nullptr, widget.get());

    assert(s.rare().animationName.size() == 2);
    assert(s.rare().animationName[0] == "spin");
    assert(s.rare().animationName[1] == "fadeIn");
    assert(std::abs(s.rare().animationDuration[0] - 1.0f) < 1e-6f);
    assert(std::abs(s.rare().animationDuration[1] - 0.4f) < 1e-6f);
    assert(s.rare().animationIterationCount[0] < 0.0f); // infinite
    assert(std::abs(s.rare().animationIterationCount[1] - 2.0f) < 1e-6f);
    assert(s.rare().animationDirection[0] == AnimationDirection::Normal);
    assert(s.rare().animationDirection[1] == AnimationDirection::AlternateReverse);
    assert(s.rare().animationFillMode[0] == AnimationFillMode::Forwards);
    assert(s.rare().animationFillMode[1] == AnimationFillMode::Both);
    assert(s.rare().animationPlayState[0] == AnimationPlayState::Running);
    assert(s.rare().animationPlayState[1] == AnimationPlayState::Paused);
    assert(s.rare().animationTimingFunction[0].kind == TimingFunction::Kind::EaseIn);
    assert(s.rare().animationTimingFunction[1].kind == TimingFunction::Kind::Steps);
    assert(s.rare().animationTimingFunction[1].stepCount == 3);
    assert(s.rare().animationComposition[1] == AnimationComposition::Add);

    std::cout << "  PASS - all 9 sub-properties parsed as parallel lists." << std::endl;
}

static void testAnimationRuntimeTick() {
    std::cout << "[6] Driving an animation effect..." << std::endl;

    // Use a minimal CSS that drives `opacity` from 0 -> 1 over 0.5s.
    StyleSheet sheet;
    sheet.parse(
        "@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }"
        ".box { animation: fadeIn 0.5s linear; }"
    );

    auto widget = std::make_shared<Panel>("box");
    widget->resolveStyles(sheet);
    std::cout << "DEBUG base style opacity: " << widget->computedStyle->opacity << std::endl;
    assert(std::abs(widget->computedStyle->opacity - 1.0f) < 1e-6f); // base style

    // First update: spawn the animation, no time elapsed -> at delay window or t=0
    InputState input{};
    input.deltaTime = 0.0f;
    widget->update(input);
    std::cout << "DEBUG step 1 active animations size: " << widget->activeAnimations.size() << std::endl;
    if (!widget->activeAnimations.empty()) {
        std::cout << "DEBUG step 1 active animation name: " << widget->activeAnimations[0].name << ", finished: " << widget->activeAnimations[0].finished << ", startTime: " << widget->activeAnimations[0].startTime << ", localClock: " << widget->localClock << std::endl;
    }
    std::cout << "DEBUG step 1 opacity: " << widget->computedStyle->opacity << std::endl;
    assert(widget->activeAnimations.size() == 1);
    assert(widget->activeAnimations[0].name == "fadeIn");
    // At t=0 the keyframe at from (opacity 0) should be applied.
    assert(std::abs(widget->computedStyle->opacity - 0.0f) < 1e-6f);

    // Advance halfway: 0.25s elapsed -> opacity should be ~0.5
    input.deltaTime = 0.25f;
    widget->update(input);
    std::cout << "DEBUG step 2 active animations size: " << widget->activeAnimations.size() << std::endl;
    if (!widget->activeAnimations.empty()) {
        std::cout << "DEBUG step 2 active animation finished: " << widget->activeAnimations[0].finished << ", localClock: " << widget->localClock << std::endl;
    }
    std::cout << "DEBUG step 2 opacity: " << widget->computedStyle->opacity << std::endl;
    assert(std::abs(widget->computedStyle->opacity - 0.5f) < 1e-3f);

    // Advance past the end: 0.3s more -> opacity should be 1.0, animation finished
    input.deltaTime = 0.3f;
    widget->update(input);
    std::cout << "DEBUG step 3 opacity: " << widget->computedStyle->opacity << std::endl;
    assert(std::abs(widget->computedStyle->opacity - 1.0f) < 1e-3f);
    assert(widget->activeAnimations[0].finished);

    std::cout << "  PASS - animation effect runs from -> to with correct timing." << std::endl;
}

static void testAnimationAlternateDirection() {
    std::cout << "[7] Animation direction: alternate..." << std::endl;

    StyleSheet sheet;
    sheet.parse(
        "@keyframes slide { from { opacity: 0; } to { opacity: 1; } }"
        ".box { animation: slide 0.4s linear 2 alternate; }"
    );

    auto widget = std::make_shared<Panel>("box");
    widget->resolveStyles(sheet);
    InputState input{};
    input.deltaTime = 0.0f;
    widget->update(input);

    // Iteration 0 forward: at t=0, opacity = 0
    assert(std::abs(widget->computedStyle->opacity - 0.0f) < 1e-6f);

    // 0.1s in iteration 0 (1/4 of duration) -> opacity ~0.25
    input.deltaTime = 0.1f;
    widget->update(input);
    assert(std::abs(widget->computedStyle->opacity - 0.25f) < 1e-2f);

    // 0.4s in -> end of iteration 0, beginning of iteration 1 (reversed) -> opacity=1
    input.deltaTime = 0.3f;
    widget->update(input);
    assert(std::abs(widget->computedStyle->opacity - 1.0f) < 1e-3f);

    // 0.1s into iteration 1 (reversed) -> opacity ~0.75
    input.deltaTime = 0.1f;
    widget->update(input);
    assert(std::abs(widget->computedStyle->opacity - 0.75f) < 1e-2f);

    std::cout << "  PASS - alternate direction reverses odd iterations." << std::endl;
}

int main() {
    std::cout << "=== FluxUI CSS Animations & @keyframes Parity Tests ===" << std::endl;
    testParseKeyframes();
    testParseAnimationShorthand();
    testParseTransitionShorthand();
    testTimingFunctionSampling();
    testAnimationSubpropertyParse();
    testAnimationRuntimeTick();
    testAnimationAlternateDirection();
    std::cout << "All animation tests passed!" << std::endl;
    return 0;
}
