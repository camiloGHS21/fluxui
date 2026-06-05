#pragma once
// FluxUI public API - input events & state (depends on geometry).
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
#include "fluxui/detail/geometry.h"
namespace FluxUI {
enum class EventType {
    MouseMove, MouseDown, MouseUp, MouseScroll,
    KeyDown, KeyUp, TextInput,
    WindowResize, WindowClose
};
struct InputEvent {
    EventType type;
    Vec2 mousePos;
    Vec2 mouseDelta;
    Vec2 scroll;
    int mouseButton = 0;
    int keyCode = 0;
    int modifiers = 0;
    std::string text;
    Vec2 windowSize;
    bool consumed = false;
};
struct InputState {
    Vec2 mousePos;
    Vec2 mouseDelta;
    bool mouseDown[3] = {};
    bool mouseClicked[3] = {};
    bool mouseReleased[3] = {};
    int mouseClickCount[3] = {};
    Vec2 scroll;
    float deltaTime = 0.016f;
    Vec2 windowSize;
    int keyCode = 0;
    int modifiers = 0;
    std::string text;
};
}