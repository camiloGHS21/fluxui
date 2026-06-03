// FluxUI Declarative DSL — React/SwiftUI-style functional UI builder
// Works in C++, and the same pattern maps 1:1 to Rust/Go/Java via bindings.
//
// Usage:
//   #include <fluxui/dsl.h>
//   using namespace fluxui;
//
//   int main() {
//       App app;
//       app.loadCSS("style.css");
//       auto count = State<int>(0);
//       app.setRoot(
//           Row({
//               Sidebar({
//                   NavItem("Dashboard"),
//                   NavItem("Settings")
//               }).className("sidebar"),
//               Column({
//                   Text("Hello").className("title"),
//                   Text([&]{ return std::to_string(count.get()); }),   // reactive!
//                   Button("Click me").onClick([&]{ count.set(count.get() + 1); })
//               }).className("content")
//           }).className("app")
//       );
//       return app.run();
//   }
//
#pragma once
#include "fluxui/FluxUI.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <initializer_list>
#include <algorithm>

namespace fluxui {

// ============================================================
//  Reactive binding registry (Blink reactive/signals pattern)
//
//  Reactive Text() builders register a {widget, fn, lastValue} binding here.
//  The App's update loop re-evaluates each binding once per frame and, when
//  the produced string differs from the previous value, mutates the widget's
//  content and flags it dirty so style + layout + paint refresh.
//
//  This is intentionally a process-global registry so the free-standing
//  builder functions (which have no reference to the App) can register without
//  threading an App handle through every call. Dead widgets are pruned via
//  weak_ptr expiry, so the registry is self-cleaning.
// ============================================================
namespace detail {

struct ReactiveBinding {
    std::weak_ptr<FluxUI::Text> widget;
    std::function<std::string()> fn;
    std::string lastValue;
};

inline std::vector<ReactiveBinding>& reactiveBindings() {
    static std::vector<ReactiveBinding> bindings;
    return bindings;
}

inline void registerReactiveText(const std::shared_ptr<FluxUI::Text>& w,
                                 std::function<std::string()> fn,
                                 std::string initial) {
    ReactiveBinding b;
    b.widget = w;
    b.fn = std::move(fn);
    b.lastValue = std::move(initial);
    reactiveBindings().push_back(std::move(b));
}

// Re-evaluate every live reactive binding. Returns true if anything changed.
// Safe to call without an active window (used by headless tests too).
inline bool pumpReactiveBindings() {
    auto& binds = reactiveBindings();
    bool changed = false;
    bool hasDead = false;
    for (auto& b : binds) {
        auto w = b.widget.lock();
        if (!w) { hasDead = true; continue; }
        std::string v = b.fn();
        if (v != b.lastValue) {
            b.lastValue = std::move(v);
            w->content = b.lastValue;
            w->markStyleDirty();   // marks layout dirty + requests redraw
            changed = true;
        }
    }
    // Opportunistic prune of expired bindings.
    if (hasDead) {
        binds.erase(std::remove_if(binds.begin(), binds.end(),
                        [](const ReactiveBinding& b) { return b.widget.expired(); }),
                    binds.end());
    }
    return changed;
}

} // namespace detail

// ============================================================
//  State<T> — lightweight reactive primitive
// ============================================================
template <typename T>
class State {
    T value_;
    std::vector<std::function<void()>> listeners_;
public:
    explicit State(T initial = {}) : value_(std::move(initial)) {}
    const T& get() const { return value_; }
    void set(T newValue) {
        value_ = std::move(newValue);
        for (auto& fn : listeners_) fn();
        // Wake the render loop so the reactive pump runs and the UI refreshes.
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
    }
    // Convenience mutators for common types.
    template <typename U = T>
    void update(std::function<void(T&)> mutator) { mutator(value_); set(value_); }
    void onChange(std::function<void()> fn) { listeners_.push_back(std::move(fn)); }
    operator const T&() const { return value_; }
};

// ============================================================
//  WidgetBuilder — wraps a shared_ptr<Widget> with method chaining
// ============================================================
class WidgetBuilder {
    std::shared_ptr<FluxUI::Widget> widget_;
public:
    WidgetBuilder() = default;
    explicit WidgetBuilder(std::shared_ptr<FluxUI::Widget> w) : widget_(std::move(w)) {}

    // Chaining setters
    WidgetBuilder& className(const std::string& cls) { if (widget_) widget_->className = cls; return *this; }
    WidgetBuilder& id(const std::string& id) { if (widget_) widget_->id = id; return *this; }
    WidgetBuilder& onClick(std::function<void()> fn) { if (widget_) widget_->onClick = std::move(fn); return *this; }
    WidgetBuilder& style(const std::string& prop, const std::string& val) {
        if (widget_) {
            FluxUI::CSSProperty p;
            p.name = prop;
            p.value = val;
            widget_->inlineProperties.push_back(p);
        }
        return *this;
    }
    WidgetBuilder& attr(const std::string& name, const std::string& value) {
        if (widget_) widget_->setAttribute(name, value);
        return *this;
    }
    WidgetBuilder& visible(bool v) { if (widget_) widget_->visible = v; return *this; }

    // Access the underlying widget
    std::shared_ptr<FluxUI::Widget> build() const { return widget_; }
    FluxUI::Widget* get() const { return widget_.get(); }
    operator std::shared_ptr<FluxUI::Widget>() const { return widget_; }
};

// ============================================================
//  Free-standing builder functions (declarative tree construction)
// ============================================================

// Helper: attach children to a parent widget
inline void attachChildren(FluxUI::Widget* parent, std::initializer_list<WidgetBuilder> children) {
    for (auto& child : children) {
        auto w = child.build();
        if (w) {
            w->parent = parent;
            parent->children.push_back(w);
        }
    }
}

// Layout containers
inline WidgetBuilder Row(std::initializer_list<WidgetBuilder> children) {
    auto w = std::make_shared<FluxUI::Panel>();
    w->type = "div";
    w->style.display = FluxUI::Display::Flex;
    w->style.flexDirection = FluxUI::FlexDirection::Row;
    attachChildren(w.get(), children);
    return WidgetBuilder(w);
}

inline WidgetBuilder Column(std::initializer_list<WidgetBuilder> children) {
    auto w = std::make_shared<FluxUI::Panel>();
    w->type = "div";
    w->style.display = FluxUI::Display::Flex;
    w->style.flexDirection = FluxUI::FlexDirection::Column;
    attachChildren(w.get(), children);
    return WidgetBuilder(w);
}

inline WidgetBuilder Sidebar(std::initializer_list<WidgetBuilder> children) {
    auto w = std::make_shared<FluxUI::Panel>("sidebar");
    w->type = "nav";
    w->style.display = FluxUI::Display::Flex;
    w->style.flexDirection = FluxUI::FlexDirection::Column;
    attachChildren(w.get(), children);
    return WidgetBuilder(w);
}

inline WidgetBuilder Card(std::initializer_list<WidgetBuilder> children) {
    auto w = std::make_shared<FluxUI::Panel>("card");
    w->type = "div";
    w->style.display = FluxUI::Display::Flex;
    w->style.flexDirection = FluxUI::FlexDirection::Column;
    attachChildren(w.get(), children);
    return WidgetBuilder(w);
}

inline WidgetBuilder Grid(std::initializer_list<WidgetBuilder> children) {
    auto w = std::make_shared<FluxUI::Panel>();
    w->type = "div";
    w->style.display = FluxUI::Display::Grid;
    attachChildren(w.get(), children);
    return WidgetBuilder(w);
}

// Content widgets
inline WidgetBuilder Text(const std::string& content) {
    auto w = std::make_shared<FluxUI::Text>(content, "");
    return WidgetBuilder(w);
}

// Reactive text — re-evaluates the function whenever bound State changes.
inline WidgetBuilder Text(std::function<std::string()> fn) {
    std::string initial = fn();
    auto w = std::make_shared<FluxUI::Text>(initial, "");
    detail::registerReactiveText(w, std::move(fn), std::move(initial));
    return WidgetBuilder(w);
}

inline WidgetBuilder Button(const std::string& label) {
    auto w = std::make_shared<FluxUI::Button>(label, "");
    return WidgetBuilder(w);
}

inline WidgetBuilder NavItem(const std::string& label) {
    auto w = std::make_shared<FluxUI::Button>(label, "nav-item");
    w->type = "button";
    return WidgetBuilder(w);
}

inline WidgetBuilder Input(const std::string& placeholder = "") {
    auto w = std::make_shared<FluxUI::TextInput>(placeholder);
    return WidgetBuilder(w);
}

inline WidgetBuilder Img(const std::string& src) {
    auto w = std::make_shared<FluxUI::Image>();
    w->source = src;
    return WidgetBuilder(w);
}

inline WidgetBuilder Checkbox(bool checked = false) {
    auto w = std::make_shared<FluxUI::Checkbox>(checked);
    return WidgetBuilder(w);
}

// Generic container with custom tag
inline WidgetBuilder Element(const std::string& tag, std::initializer_list<WidgetBuilder> children = {}) {
    auto w = std::make_shared<FluxUI::Panel>();
    w->type = tag;
    attachChildren(w.get(), children);
    return WidgetBuilder(w);
}

// ============================================================
//  App — owns a FluxUI::Application and drives the reactive loop
// ============================================================
class App {
    FluxUI::Application app_;
    bool windowReady_ = false;

    void ensureWindow(int width, int height, const std::string& title) {
        if (windowReady_) return;
        if (app_.init(title, width, height)) {
            windowReady_ = true;
            // Load a usable default font so text renders out of the box.
            if (!app_.renderer().loadDefaultFont(16.0f)) {
#ifdef _WIN32
                app_.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
#endif
            }
        }
    }

public:
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    App() { ensureWindow(1200, 800, "FluxUI App"); }
    App(int width, int height, const std::string& title) { ensureWindow(width, height, title); }

    void loadCSS(const std::string& path) { app_.loadStylesheet(path); }
    void addCSS(const std::string& css)   { app_.addStylesheet(css); }
    void loadStyle(const std::string& path) { loadCSS(path); }

    void setRoot(WidgetBuilder builder) {
        auto root = app_.root();
        auto w = builder.build();
        root->clearChildren();
        if (w) {
            root->addChild(w);
        }
    }

    int run() {
        // Install the reactive pump into the per-frame update hook.
        app_.onUpdate = [](float /*dt*/) {
            detail::pumpReactiveBindings();
        };
        app_.run();
        return 0;
    }

    FluxUI::Application& raw() { return app_; }
};

} // namespace fluxui

// Convenience: bring the DSL into fluxui:: namespace by default.
// Users can `using namespace fluxui;` for the cleanest syntax.
