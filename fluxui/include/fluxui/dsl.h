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
//                   Button("Click me").onClick([&] { count.set(count.get() + 1); })
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

namespace fluxui {

// ============================================================
//  State<T> — lightweight reactive primitive (Blink reactive/signals pattern)
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
    }
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

// Reactive text — re-evaluates the function on each frame
inline WidgetBuilder Text(std::function<std::string()> fn) {
    auto w = std::make_shared<FluxUI::Text>(fn(), "");
    // TODO: wire into reactive update system for per-frame re-eval
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
//  App — simplified Application wrapper
// ============================================================
class App {
    FluxUI::Application* app_ = nullptr;
    bool ownsApp_ = false;
public:
    App() {
        app_ = FluxUI::Application::instance();
    }
    App(int width, int height, const std::string& title) {
        app_ = FluxUI::Application::instance();
        app_->init(title, width, height);
    }

    void loadCSS(const std::string& path) { app_->loadStylesheet(path); }
    void addCSS(const std::string& css) { app_->addStylesheet(css); }
    void loadStyle(const std::string& path) { loadCSS(path); }

    void setRoot(WidgetBuilder builder) {
        auto root = app_->root();
        auto w = builder.build();
        if (w) {
            root->children.clear();
            w->parent = root;
            root->children.push_back(w);
        }
    }

    int run() {
        app_->run();
        return 0;
    }

    FluxUI::Application& raw() { return *app_; }
};

} // namespace fluxui

// Convenience: bring the DSL into fluxui:: namespace by default
// Users can `using namespace fluxui;` for the cleanest syntax.
