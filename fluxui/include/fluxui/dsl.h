// FluxUI Declarative DSL — modern, HTML/Blink-faithful functional UI builder.
//
// Element names match HTML exactly (Div, Span, P, H1..H6, Nav, Section, Button,
// Input, A, Img, Ul, Li, ...), so a FluxUI tree reads like HTML and renders with
// Blink's UA stylesheet semantics. Layout is expressed in CSS (display:flex,
// grid, ...) exactly like the browser — there are no bespoke Row/Column nodes.
//
// The same pattern maps 1:1 to Rust/Go/Java/Python/Zig via the bindings.
//
// Usage:
//   #include <fluxui/dsl.h>
//   using namespace fluxui;
//
//   int main() {
//       App app(1200, 800, "CompanyGuard");
//       app.addCSS(".app{display:flex} .content{display:flex;flex-direction:column}");
//       auto devices = State<int>(128);
//       app.setRoot(
//           Div({
//               Nav({
//                   Button("Dashboard"),
//                   Button("Settings")
//               }).className("sidebar"),
//               Div({
//                   H1("CompanyGuard"),
//                   Text([&]{ return std::to_string(devices.get()); }),   // reactive!
//                   Button("Scan").onClick([&]{ devices.set(devices.get() + 1); })
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
//  The App's update loop re-evaluates each binding once per frame and, when the
//  produced string differs from the previous value, mutates the widget's content
//  and flags it dirty so style + layout + paint refresh. Dead widgets are pruned
//  via weak_ptr expiry, so the registry is self-cleaning.
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
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
    }
    void onChange(std::function<void()> fn) { listeners_.push_back(std::move(fn)); }
    operator const T&() const { return value_; }
};

// ============================================================
//  Element — a deferred, HTML-named node materialized on mount.
//
//  Every node carries an HTML tag name. mount() routes through
//  Widget::element(tag, ...) which is the single source of truth for the
//  tag -> widget mapping (Blink UA parity), so Div/Span/H1/Button/A/Img/... all
//  produce exactly the widget the browser would.
// ============================================================
class Element {
public:
    std::string tag = "div";
    std::string content;            // text/label/placeholder/src depending on tag
    std::string className_;
    std::string id_;
    std::function<void()> onClick_;
    std::function<std::string()> textFn_;   // set => reactive text node
    std::function<void(FluxUI::Widget*)> onMount_;  // post-mount configuration hook
    std::vector<std::pair<std::string, std::string>> inlineStyles_;
    std::vector<std::pair<std::string, std::string>> attrs_;
    std::vector<Element> children_;
    bool hasVisible_ = false;
    bool visible_ = true;

    Element() = default;
    explicit Element(std::string t) : tag(std::move(t)) {}

    // Chaining setters — mirror HTML attributes / DOM properties.
    Element& className(const std::string& v) { className_ = v; return *this; }
    Element& id(const std::string& v) { id_ = v; return *this; }
    Element& onClick(std::function<void()> fn) { onClick_ = std::move(fn); return *this; }
    Element& style(const std::string& prop, const std::string& val) {
        inlineStyles_.emplace_back(prop, val); return *this;
    }
    Element& attr(const std::string& name, const std::string& val) {
        attrs_.emplace_back(name, val); return *this;
    }
    Element& href(const std::string& url) { return attr("href", url); }
    Element& src(const std::string& url) { content = url; return *this; }
    Element& visible(bool v) { hasVisible_ = true; visible_ = v; return *this; }
    // Post-mount hook: receive the materialized widget for advanced setup
    // (e.g. configuring a Video, Canvas, or wiring extra event listeners).
    Element& onMount(std::function<void(FluxUI::Widget*)> fn) { onMount_ = std::move(fn); return *this; }
    template <typename T>
    Element& onMount(std::function<void(T*)> fn) {
        onMount_ = [fn = std::move(fn)](FluxUI::Widget* w) { fn(static_cast<T*>(w)); };
        return *this;
    }

    // Materialize this node (and subtree) under parent, returning the widget.
    FluxUI::Widget* mount(FluxUI::Widget* parent) const {
        FluxUI::Widget* w = nullptr;
        if (textFn_) {
            std::string initial = textFn_();
            w = parent->element("span", initial, className_);
            if (!parent->children.empty()) {
                auto sp = std::static_pointer_cast<FluxUI::Text>(parent->children.back());
                detail::registerReactiveText(sp, textFn_, initial);
            }
        } else {
            w = parent->element(tag, content, className_);
        }
        if (!w) return nullptr;

        if (!id_.empty()) w->id = id_;
        if (onClick_) w->onClick = onClick_;
        if (hasVisible_) w->visible = visible_;
        for (const auto& s : inlineStyles_) {
            FluxUI::CSSProperty p;
            p.name = s.first;
            p.value = s.second;
            w->inlineProperties.push_back(p);
        }
        for (const auto& a : attrs_) {
            w->setAttribute(a.first, a.second);
        }
        if (onMount_) onMount_(w);
        for (const auto& child : children_) {
            child.mount(w);
        }
        return w;
    }
};

namespace detail {
inline Element container(const char* tag, std::initializer_list<Element> kids) {
    Element e(tag);
    e.children_.assign(kids.begin(), kids.end());
    return e;
}
inline Element leaf(const char* tag, const std::string& content) {
    Element e(tag);
    e.content = content;
    return e;
}
} // namespace detail

// ============================================================
//  HTML element builders (names match HTML/Blink exactly).
// ============================================================

// --- Flow containers (Blink: LayoutBlock / flex via CSS) ---
inline Element Div(std::initializer_list<Element> kids = {})       { return detail::container("div", kids); }
inline Element Section(std::initializer_list<Element> kids = {})   { return detail::container("section", kids); }
inline Element Article(std::initializer_list<Element> kids = {})   { return detail::container("article", kids); }
inline Element Aside(std::initializer_list<Element> kids = {})     { return detail::container("aside", kids); }
inline Element Header(std::initializer_list<Element> kids = {})    { return detail::container("header", kids); }
inline Element Footer(std::initializer_list<Element> kids = {})    { return detail::container("footer", kids); }
inline Element Main(std::initializer_list<Element> kids = {})      { return detail::container("main", kids); }
inline Element Nav(std::initializer_list<Element> kids = {})       { return detail::container("nav", kids); }
inline Element Form(std::initializer_list<Element> kids = {})      { return detail::container("form", kids); }
inline Element Fieldset(std::initializer_list<Element> kids = {})  { return detail::container("fieldset", kids); }
inline Element Blockquote(std::initializer_list<Element> kids = {}){ return detail::container("blockquote", kids); }
inline Element Figure(std::initializer_list<Element> kids = {})    { return detail::container("figure", kids); }
inline Element Ul(std::initializer_list<Element> kids = {})        { return detail::container("ul", kids); }
inline Element Ol(std::initializer_list<Element> kids = {})        { return detail::container("ol", kids); }
inline Element Li(std::initializer_list<Element> kids = {})        { return detail::container("li", kids); }
inline Element Table(std::initializer_list<Element> kids = {})     { return detail::container("table", kids); }
inline Element Thead(std::initializer_list<Element> kids = {})     { return detail::container("thead", kids); }
inline Element Tbody(std::initializer_list<Element> kids = {})     { return detail::container("tbody", kids); }
inline Element Tr(std::initializer_list<Element> kids = {})        { return detail::container("tr", kids); }

// --- Text content (Blink: LayoutText / inline) ---
inline Element Text(const std::string& content) { return detail::leaf("span", content); }
inline Element Span(const std::string& content) { return detail::leaf("span", content); }
inline Element P(const std::string& content)    { return detail::leaf("p", content); }
inline Element H1(const std::string& content)   { return detail::leaf("h1", content); }
inline Element H2(const std::string& content)   { return detail::leaf("h2", content); }
inline Element H3(const std::string& content)   { return detail::leaf("h3", content); }
inline Element H4(const std::string& content)   { return detail::leaf("h4", content); }
inline Element H5(const std::string& content)   { return detail::leaf("h5", content); }
inline Element H6(const std::string& content)   { return detail::leaf("h6", content); }
inline Element Strong(const std::string& content){ return detail::leaf("strong", content); }
inline Element B(const std::string& content)    { return detail::leaf("b", content); }
inline Element Em(const std::string& content)   { return detail::leaf("em", content); }
inline Element I(const std::string& content)    { return detail::leaf("i", content); }
inline Element Small(const std::string& content){ return detail::leaf("small", content); }
inline Element Label(const std::string& content){ return detail::leaf("label", content); }
inline Element Legend(const std::string& content){ return detail::leaf("legend", content); }
inline Element Code(const std::string& content) { return detail::leaf("code", content); }
inline Element Pre(const std::string& content)  { return detail::leaf("pre", content); }
inline Element Td(const std::string& content)   { return detail::leaf("td", content); }
inline Element Th(const std::string& content)   { return detail::leaf("th", content); }

// Reactive text — re-evaluates the function whenever bound State changes.
inline Element Text(std::function<std::string()> fn) {
    Element e("span");
    e.textFn_ = std::move(fn);
    return e;
}

// --- Interactive controls ---
inline Element Button(const std::string& label)            { return detail::leaf("button", label); }
inline Element Input(const std::string& placeholder = "")  { return detail::leaf("input", placeholder); }
inline Element TextArea(const std::string& placeholder = ""){ return detail::leaf("textarea", placeholder); }
inline Element A(const std::string& content, const std::string& href = "") {
    Element e = detail::leaf("a", content);
    if (!href.empty()) e.attr("href", href);
    return e;
}
inline Element Img(const std::string& src)   { return detail::leaf("img", src); }
inline Element Video(const std::string& src) { Element e("video"); e.content = src; return e; }
inline Element Checkbox()                    { return Element("checkbox"); }
inline Element Radio()                       { return Element("radio"); }
inline Element Canvas()                      { return Element("canvas"); }
inline Element Hr()                          { return Element("hr"); }
inline Element Br()                          { return Element("br"); }
inline Element Select(std::initializer_list<Element> options = {}) { return detail::container("select", options); }
inline Element Option(const std::string& label) { return detail::leaf("option", label); }

// --- Generic escape hatch for any tag ---
inline Element El(const std::string& tag, std::initializer_list<Element> kids = {}) {
    Element e(tag);
    e.children_.assign(kids.begin(), kids.end());
    return e;
}
inline Element El(const std::string& tag, const std::string& content) {
    Element e(tag);
    e.content = content;
    return e;
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

    void setRoot(const Element& root) {
        auto rootWidget = app_.root();
        rootWidget->clearChildren();
        root.mount(rootWidget);
    }

    int run() {
        app_.onUpdate = [](float /*dt*/) {
            detail::pumpReactiveBindings();
        };
        app_.run();
        return 0;
    }

    FluxUI::Application& raw() { return app_; }
};

} // namespace fluxui
