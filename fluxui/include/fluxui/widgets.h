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

struct SDL_Cursor;

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
    Rect bounds;          // computed layout bounds

    bool visible = true;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;

    // Animation & Scroll state
    float hoverAnim = 0;  // 0..1 for smooth transitions
    float renderScale = 1.0f;
    float scrollY = 0;
    float targetScrollY = 0;
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

    // Resolve styles from stylesheet
    void resolveStyles(const StyleSheet& sheet);
    void markLayoutDirty();
    void markStyleDirty();
    void markStyleDirtyRecursive();
    void markSubtreeStyleDirty();

    // Layout
    virtual void layout(const Rect& parentBounds);

    // Update (animations, hover state)
    virtual void update(const InputState& input);

    // Hit-tested cursor for native pointer feedback
    virtual CursorType cursorAt(Vec2 point) const;
    virtual Widget* hitTest(Vec2 point, bool interactiveOnly = false);

    // Render
    virtual void render(Renderer& renderer);

protected:
    void layoutFlexChildren();
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

    bool running = true;

private:
    SDL_Window* window_ = nullptr;
    SDL_Cursor* defaultCursor_ = nullptr;
    SDL_Cursor* pointerCursor_ = nullptr;
    SDL_Cursor* textCursor_ = nullptr;
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
