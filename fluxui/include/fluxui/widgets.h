// FluxUI Widget System - CSS-styled, GPU-rendered widgets
#pragma once

#include "core.h"
#include "css_parser.h"
#include "renderer.h"
#include <string>
#include <vector>
#include <memory>
#include <memory_resource>
#include <functional>
#include <cstddef>
#include <utility>
#include <unordered_map>
#include <sstream>
#include <cctype>

// Forward declarations (handles moved to core.h)

namespace FluxUI {

namespace detail {

inline std::shared_ptr<std::pmr::memory_resource> makeWidgetArena() {
    auto arena = std::make_shared<std::pmr::unsynchronized_pool_resource>();
    return std::static_pointer_cast<std::pmr::memory_resource>(arena);
}

template<typename T>
class WidgetArenaAllocator {
public:
    using value_type = T;

    std::shared_ptr<std::pmr::memory_resource> resource;

    WidgetArenaAllocator() = default;
    explicit WidgetArenaAllocator(std::shared_ptr<std::pmr::memory_resource> res)
        : resource(std::move(res)) {}

    template<typename U>
    WidgetArenaAllocator(const WidgetArenaAllocator<U>& other)
        : resource(other.resource) {}

    T* allocate(std::size_t count) {
        return static_cast<T*>(resource->allocate(count * sizeof(T), alignof(T)));
    }

    void deallocate(T* ptr, std::size_t count) noexcept {
        resource->deallocate(ptr, count * sizeof(T), alignof(T));
    }

    template<typename U>
    struct rebind {
        using other = WidgetArenaAllocator<U>;
    };
};

template<typename T, typename U>
inline bool operator==(const WidgetArenaAllocator<T>& a, const WidgetArenaAllocator<U>& b) noexcept {
    return a.resource.get() == b.resource.get();
}

template<typename T, typename U>
inline bool operator!=(const WidgetArenaAllocator<T>& a, const WidgetArenaAllocator<U>& b) noexcept {
    return !(a == b);
}

} // namespace detail

class Panel;
class Text;
class Button;
class TextInput;
class Icon;
class ProgressBar;

// ============================================================
//  Widget Base Class
// ============================================================

class Widget {
public:
    std::string id;
    std::string className;
    std::string type = "widget";

    Style style;
    Style computedStyle;  // after CSS resolution
    std::vector<CSSProperty> inlineProperties; // parsed style="" declarations
    Rect bounds;          // computed layout bounds

    bool visible = true;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;

    // Animation & Scroll state
    float hoverAnim = 0;  // 0..1 for smooth transitions
    float hoverVelocity = 0; // for spring physics
    float renderScale = 1.0f;
    float scrollY = 0;
    float targetScrollY = 0;
    float scrollVelocity = 0;
    float contentHeight = 0;
    bool layoutDirty = true;
    bool styleDirty = true;
    bool subtreeStyleDirty = true;
    Rect lastLayoutParentBounds;
    size_t layoutSignature = 0;
    bool scrollbarHovered = false;
    bool scrollbarDragging = false;
    float scrollbarDragOffset = 0;

    // Parent/children
    Widget* parent = nullptr;
    std::shared_ptr<std::pmr::memory_resource> childArena;
    std::vector<std::shared_ptr<Widget>> children;

    // Callbacks
    std::function<void()> onClick;
    std::function<void()> onHover;

    Widget() = default;
    virtual ~Widget() = default;

    // Add child widget
    Widget* addChild(std::shared_ptr<Widget> child) {
        child->parent = this;
        children.push_back(child);
        markLayoutDirty();
        markSubtreeStyleDirty();
        return child.get();
    }

    void reserveChildren(size_t count) { children.reserve(count); }
    void clearChildren(bool releaseArena = false) {
        children.clear();
        if (releaseArena) childArena.reset();
        markLayoutDirty();
        markSubtreeStyleDirty();
    }

    template<typename T, typename... Args>
    T* add(Args&&... args) {
        if (!childArena) {
            childArena = detail::makeWidgetArena();
        }
        detail::WidgetArenaAllocator<T> allocator(childArena);
        auto w = std::allocate_shared<T>(allocator, std::forward<Args>(args)...);
        addChild(w);
        return static_cast<T*>(children.back().get());
    }

    Panel* panel(const std::string& cls = "", size_t reserve = 0);
    Text* text(const std::string& content, const std::string& cls = "");
    Button* button(const std::string& label = "",
                   const std::string& cls = "",
                   std::function<void()> onClick = {});
    TextInput* textInput(const std::string& placeholder = "",
                         const std::string& cls = "");
    Icon* addIcon(const std::string& glyph, const std::string& cls = "");
    Icon* icon(const std::string& glyph, const std::string& cls = "");
    ProgressBar* progress(float value,
                          const std::string& cls = "",
                          const Color& color = Color(0.42f, 0.36f, 0.91f, 1.0f));
    Panel* div(const std::string& cls = "", size_t reserve = 0);
    Panel* section(const std::string& cls = "", size_t reserve = 0);
    Panel* article(const std::string& cls = "", size_t reserve = 0);
    Panel* aside(const std::string& cls = "", size_t reserve = 0);
    Panel* header(const std::string& cls = "", size_t reserve = 0);
    Panel* footer(const std::string& cls = "", size_t reserve = 0);
    Panel* main(const std::string& cls = "", size_t reserve = 0);
    Panel* nav(const std::string& cls = "", size_t reserve = 0);
    Text* span(const std::string& content, const std::string& cls = "");
    Text* p(const std::string& content, const std::string& cls = "");
    Text* h1(const std::string& content, const std::string& cls = "");
    Text* h2(const std::string& content, const std::string& cls = "");
    Text* h3(const std::string& content, const std::string& cls = "");
    Text* h4(const std::string& content, const std::string& cls = "");
    Text* h5(const std::string& content, const std::string& cls = "");
    Text* h6(const std::string& content, const std::string& cls = "");
    TextInput* input(const std::string& placeholder = "", const std::string& cls = "");
    Widget* setId(const std::string& value);
    Widget* classes(const std::string& value);
    Widget* addClass(const std::string& value);
    Widget* removeClass(const std::string& value);
    Widget* toggleClass(const std::string& value, bool enabled);
    Widget* css(const std::string& declarations);

    // Resolve styles from stylesheet
    void resolveStyles(const StyleSheet& sheet);
    void markLayoutDirty();
    void markStyleDirty();
    void markStyleDirtyRecursive();
    void markSubtreeStyleDirty();

    // Layout
    virtual void layout(const Rect& parentBounds);

    // Returns true if any spring animation (hover, scroll) is still settling
    bool hasActiveAnimations() const;
    void resetTransientMotion();
    virtual void update(const InputState& input);

    // Hit-tested cursor for native pointer feedback
    virtual CursorType cursorAt(Vec2 point) const;
    virtual Widget* hitTest(Vec2 point, bool interactiveOnly = false);

    // Render
    virtual void render(Renderer& renderer);

protected:
    void layoutFlexChildren();
    void layoutPositionedChildren();
    void renderBackground(Renderer& renderer);
    void renderChildren(Renderer& renderer);
    float maxScrollY() const;
    bool getScrollBarRects(Rect& track, Rect& thumb) const;
    void clampScroll();
};

// ============================================================
//  Panel - Container widget with background
// ============================================================

class Panel : public Widget {
public:
    Panel() { type = "panel"; }
    Panel(const std::string& cls) { type = "panel"; className = cls; }
};

// ============================================================
//  Text - Text label
// ============================================================

class Text : public Widget {
public:
    std::string content;

    Text() { type = "text"; }
    Text(const std::string& text, const std::string& cls = "")
        : content(text) { type = "text"; className = cls; }

    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
};

// ============================================================
//  Button - Clickable button
// ============================================================

class Button : public Widget {
public:
    std::string label;
    std::string icon; // icon character (FontAwesome)

    Button() { type = "button"; style.cursor = CursorType::Pointer; }
    Button(const std::string& lbl, const std::string& cls = "")
        : label(lbl) { type = "button"; className = cls; style.cursor = CursorType::Pointer; }

    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
};

// ============================================================
//  TextInput - Text input field
// ============================================================

class TextInput : public Widget {
public:
    std::string value;
    std::string placeholder;

    TextInput() { type = "input"; style.cursor = CursorType::Text; }
    TextInput(const std::string& ph, const std::string& cls = "")
        : placeholder(ph) { type = "input"; className = cls; style.cursor = CursorType::Text; }

    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    CursorType cursorAt(Vec2 point) const override;
    void render(Renderer& renderer) override;

private:
    size_t caretIndex_ = 0;
    size_t selectionAnchor_ = 0;
    size_t selectionFocus_ = 0;
    bool selecting_ = false;
    bool clearHovered_ = false;
    bool clearPressed_ = false;
    float scrollX_ = 0;
    float focusAnim_ = 0;
    float caretBlinkTime_ = 0;

    bool hasSelection() const;
    size_t selectionStart() const;
    size_t selectionEnd() const;
    Rect clearButtonRect() const;
};

// ============================================================
//  Icon - Single icon character
// ============================================================

class Icon : public Widget {
public:
    std::string glyph;
    std::string fontName = "icons";

    Icon() { type = "icon"; }
    Icon(const std::string& g, const std::string& cls = "")
        : glyph(g) { type = "icon"; className = cls; }

    void render(Renderer& renderer) override;
};

// ============================================================
//  ProgressBar
// ============================================================

class ProgressBar : public Widget {
public:
    float progress = 0; // 0..1
    Color barColor = Color::fromHex("#6C5CE7");

    ProgressBar() { type = "progress"; }

    void render(Renderer& renderer) override;
};

inline Panel* Widget::panel(const std::string& cls, size_t reserve) {
    auto* widget = add<Panel>(cls);
    if (reserve > 0) {
        widget->reserveChildren(reserve);
    }
    return widget;
}

inline Text* Widget::text(const std::string& content, const std::string& cls) {
    return add<Text>(content, cls);
}

inline Button* Widget::button(const std::string& label,
                              const std::string& cls,
                              std::function<void()> onClick) {
    auto* widget = add<Button>(label, cls);
    if (onClick) {
        widget->onClick = std::move(onClick);
    }
    return widget;
}

inline TextInput* Widget::textInput(const std::string& placeholder, const std::string& cls) {
    return add<TextInput>(placeholder, cls);
}

inline Icon* Widget::addIcon(const std::string& glyph, const std::string& cls) {
    return add<Icon>(glyph, cls);
}

inline Icon* Widget::icon(const std::string& glyph, const std::string& cls) {
    return addIcon(glyph, cls);
}

inline ProgressBar* Widget::progress(float value,
                                     const std::string& cls,
                                     const Color& color) {
    auto* widget = add<ProgressBar>();
    widget->className = cls;
    widget->progress = value;
    widget->barColor = color;
    return widget;
}

inline Panel* Widget::div(const std::string& cls, size_t reserve) {
    return panel(cls, reserve);
}

inline Panel* Widget::section(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "section";
    return widget;
}

inline Panel* Widget::article(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "article";
    return widget;
}

inline Panel* Widget::aside(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "aside";
    return widget;
}

inline Panel* Widget::header(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "header";
    return widget;
}

inline Panel* Widget::footer(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "footer";
    return widget;
}

inline Panel* Widget::main(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "main";
    return widget;
}

inline Panel* Widget::nav(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "nav";
    return widget;
}

inline Text* Widget::span(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "span";
    return widget;
}

inline Text* Widget::p(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "p";
    return widget;
}

inline Text* Widget::h1(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "h1";
    return widget;
}

inline Text* Widget::h2(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "h2";
    return widget;
}

inline Text* Widget::h3(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "h3";
    return widget;
}

inline Text* Widget::h4(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "h4";
    return widget;
}

inline Text* Widget::h5(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "h5";
    return widget;
}

inline Text* Widget::h6(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "h6";
    return widget;
}

inline TextInput* Widget::input(const std::string& placeholder, const std::string& cls) {
    return textInput(placeholder, cls);
}

inline Widget* Widget::setId(const std::string& value) {
    id = value;
    markStyleDirtyRecursive();
    return this;
}

inline Widget* Widget::classes(const std::string& value) {
    className = value;
    markStyleDirtyRecursive();
    return this;
}

inline Widget* Widget::addClass(const std::string& value) {
    if (value.empty()) return this;
    std::istringstream stream(className);
    std::string cls;
    while (stream >> cls) {
        if (cls == value) return this;
    }
    if (!className.empty()) className += ' ';
    className += value;
    markStyleDirtyRecursive();
    return this;
}

inline Widget* Widget::removeClass(const std::string& value) {
    if (value.empty() || className.empty()) return this;
    std::istringstream stream(className);
    std::string next;
    std::string updated;
    while (stream >> next) {
        if (next == value) continue;
        if (!updated.empty()) updated += ' ';
        updated += next;
    }
    if (updated != className) {
        className = std::move(updated);
        markStyleDirtyRecursive();
    }
    return this;
}

inline Widget* Widget::toggleClass(const std::string& value, bool enabled) {
    return enabled ? addClass(value) : removeClass(value);
}

inline Widget* Widget::css(const std::string& declarations) {
    inlineProperties.clear();

    auto trimLocal = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? std::string() : s.substr(start, end - start + 1);
    };

    size_t start = 0;
    int depth = 0;
    char quote = 0;
    bool escaped = false;
    for (size_t i = 0; i <= declarations.size(); ++i) {
        char c = (i < declarations.size()) ? declarations[i] : ';';
        if (quote != 0) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') depth++;
        else if ((c == ')' || c == ']' || c == '}') && depth > 0) depth--;
        if ((c == ';' && depth == 0) || i == declarations.size()) {
            std::string decl = trimLocal(declarations.substr(start, i - start));
            start = i + 1;
            if (decl.empty()) continue;
            auto colon = decl.find(':');
            if (colon == std::string::npos) continue;
            std::string name = trimLocal(decl.substr(0, colon));
            std::string value = trimLocal(decl.substr(colon + 1));
            for (char& ch : name) {
                ch = (char)std::tolower((unsigned char)ch);
            }
            std::string loweredValue = value;
            for (char& ch : loweredValue) {
                ch = (char)std::tolower((unsigned char)ch);
            }
            size_t bang = loweredValue.rfind('!');
            if (bang != std::string::npos) {
                std::string tail = trimLocal(loweredValue.substr(bang + 1));
                if (tail == "important") {
                    value = trimLocal(value.substr(0, bang));
                }
            }
            inlineProperties.push_back({name, value, 0});
        }
    }
    markStyleDirtyRecursive();
    return this;
}

// ============================================================
//  StatCard - Dashboard stat card (custom widget)
// ============================================================

class StatCard : public Widget {
public:
    std::string title;
    std::string value;
    std::string subtitle;
    Color accentColor = Color::fromHex("#6C5CE7");

    StatCard() { type = "stat-card"; }
    StatCard(const std::string& t, const std::string& v,
             const std::string& sub, Color accent)
        : title(t), value(v), subtitle(sub), accentColor(accent) {
        type = "stat-card";
        className = "stat-card";
    }

    void render(Renderer& renderer) override;
};

// ============================================================
//  Application
// ============================================================

enum class UIEventType {
    Any,
    Quit,
    WindowResized,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    KeyDown,
    KeyUp,
    TextInput,
    WidgetClick,
    RouteChanged,
    Custom
};

struct UIEvent {
    UIEventType type = UIEventType::Custom;
    Widget* target = nullptr;
    Vec2 position;
    Vec2 delta;
    std::string name;
    std::string route;
    std::string previousRoute;
    std::string text;
    int keyCode = 0;
    int modifiers = 0;
    int button = 0;
    int clickCount = 0;
    bool handled = false;
};

class Application {
public:
    using EventCallback = std::function<void(UIEvent&)>;
    using RouteBuilder = std::function<void(Application&, Widget*)>;

    bool init(const std::string& title, int width, int height);
    bool init(const std::string& title, int width, int height, RenderBackendType backend);
    void run();
    void shutdown();

    // Renderer backend. Call setBackend before init().
    void setBackend(RenderBackendType backend);
    RenderBackendType backendPreference() const { return backendPreference_; }
    RenderBackendType activeBackend() const { return renderer_.activeBackend(); }
    const char* activeBackendName() const { return renderer_.activeBackendName(); }

    // CSS
    bool loadStylesheet(const std::string& path);
    void addStylesheet(const std::string& css);

    // Root widget
    Widget* root() { return root_.get(); }
    Renderer& renderer() { return renderer_; }
    StyleSheet& stylesheet() { return stylesheet_; }
    InputState& input() { return input_; }

    // Events
    size_t on(UIEventType type, EventCallback callback);
    void off(size_t listenerId);
    void emit(UIEvent event);

    // Retained page router
    void addRoute(const std::string& path, RouteBuilder builder);
    void setNotFoundRoute(RouteBuilder builder);
    bool navigate(const std::string& path);
    bool renderRoute(Widget* container);
    const std::string& currentRoute() const { return currentRoute_; }
    bool routeDirty() const { return routeDirty_; }

    // Set custom update/render callback
    std::function<void(float dt)> onUpdate;
    std::function<void()> onRender;

    // Frame pacing: request a redraw when state changes
    void requestRedraw() { needsRedraw_ = true; }
    bool needsRedraw() const { return needsRedraw_; }

    bool running = true;

private:
    void* window_ = nullptr;
    void* defaultCursor_ = nullptr;
    void* pointerCursor_ = nullptr;
    void* textCursor_ = nullptr;
    CursorType activeCursor_ = CursorType::Default;
    Renderer renderer_;
    RenderBackendType backendPreference_ = Renderer::defaultBackend();
    StyleSheet stylesheet_;
    InputState input_;
    std::shared_ptr<Widget> root_;
    std::unordered_map<std::string, RouteBuilder> routes_;
    RouteBuilder notFoundRoute_;
    std::string currentRoute_;
    bool routeDirty_ = false;
    bool needsRedraw_ = true;  // Start true for first frame

    struct EventListener {
        size_t id = 0;
        UIEventType type = UIEventType::Any;
        EventCallback callback;
    };
    std::vector<EventListener> eventListeners_;
    size_t nextEventListenerId_ = 1;

    void processEvents();
    void updateCursor(CursorType cursor);
};

} // namespace FluxUI
