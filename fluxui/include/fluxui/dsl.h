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
#include <unordered_map>
#include <type_traits>

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
//
//  Similar to React's useState. Holds a value; calling set() triggers
//  re-render of any reactive Text bound to it. Use it like:
//
//      auto count = State<int>(0);
//      Text([&]{ return std::to_string(count.get()); })   // auto-updates
//      Button("+1").onClick([&]{ count.set(count.get() + 1); })
//
//  Shorthand for common patterns:
//      count++;          // operator++ for numeric types
//      count += 5;       // operator+= for numeric types
//      count = 42;       // operator= calls set()
//
// ============================================================
template <typename T>
class State {
    T value_;
    std::vector<std::function<void()>> listeners_;
public:
    explicit State(T initial = {}) : value_(std::move(initial)) {}

    // Non-copyable: copying would duplicate listeners and detach observers from
    // the original. Share a State by reference or pointer instead. Movable so it
    // can still be returned from factories / stored in containers.
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) noexcept = default;

    const T& get() const { return value_; }
    void set(T newValue) {
        value_ = std::move(newValue);
        for (auto& fn : listeners_) fn();
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
    }
    void onChange(std::function<void()> fn) { listeners_.push_back(std::move(fn)); }
    operator const T&() const { return value_; }

    // Shorthand operators for numeric/common types.
    State& operator=(const T& v) { set(v); return *this; }
    template <typename U = T, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    State& operator++() { set(value_ + 1); return *this; }
    template <typename U = T, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    State& operator--() { set(value_ - 1); return *this; }
    template <typename U = T, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    State& operator+=(const T& v) { set(value_ + v); return *this; }
    template <typename U = T, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
    State& operator-=(const T& v) { set(value_ - v); return *this; }

    // Toggle helper for bool states.
    template <typename U = T, typename = std::enable_if_t<std::is_same_v<U, bool>>>
    void toggle() { set(!value_); }
};

// ============================================================
//  Ref<T> — capture a native widget pointer after mount (like React's useRef)
//
//      auto myBtn = Ref<FluxUI::Button>();
//      Button("Go").onMount(myBtn)          // captures the widget
//      // later: myBtn->label = "Done";
//
//  Uses a shared slot so copies (e.g. when stored in a std::function) all
//  point to the same captured widget.
// ============================================================
template <typename T = FluxUI::Widget>
class Ref {
    std::shared_ptr<T*> slot_ = std::make_shared<T*>(nullptr);
public:
    Ref() = default;
    T* get() const { return *slot_; }
    T* operator->() const { return *slot_; }
    T& operator*() const { return **slot_; }
    explicit operator bool() const { return *slot_ != nullptr; }

    // Use as an onMount handler: Button("X").onMount(myRef)
    void operator()(FluxUI::Widget* w) { *slot_ = static_cast<T*>(w); }
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

    // --- Typed style helpers (sugar over style(prop, val)) ---
    Element& color(const std::string& c)            { return style("color", c); }
    Element& background(const std::string& c)       { return style("background-color", c); }
    Element& width(const std::string& v)            { return style("width", v); }
    Element& height(const std::string& v)           { return style("height", v); }
    Element& padding(const std::string& v)          { return style("padding", v); }
    Element& margin(const std::string& v)           { return style("margin", v); }
    Element& gap(const std::string& v)              { return style("gap", v); }
    Element& fontSize(const std::string& v)         { return style("font-size", v); }
    Element& fontWeight(const std::string& v)       { return style("font-weight", v); }
    Element& borderRadius(const std::string& v)     { return style("border-radius", v); }
    Element& flexGrow(const std::string& v)         { return style("flex-grow", v); }
    Element& flexDirection(const std::string& v)    { return style("flex-direction", v); }
    Element& display(const std::string& v)          { return style("display", v); }
    Element& justify(const std::string& v)          { return style("justify-content", v); }
    Element& align(const std::string& v)            { return style("align-items", v); }

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
        } else if (tag == "stat-card") {
            // Special: create a StatCard directly (not in element() factory).
            std::string val, sub, accent = "#6C5CE7";
            for (const auto& a : attrs_) {
                if (a.first == "__value") val = a.second;
                else if (a.first == "__subtitle") sub = a.second;
                else if (a.first == "__accent") accent = a.second;
            }
            auto sc = std::make_shared<FluxUI::StatCard>(content, val, sub, FluxUI::Color::fromHex(accent));
            sc->parent = parent;
            if (!className_.empty()) sc->className = className_;
            parent->children.push_back(sc);
            w = sc.get();
        } else if (tag == "progress-bar") {
            // Special: create a ProgressBar directly.
            float val = 0.0f;
            std::string colorHex;
            try { val = std::stof(content); } catch (...) {}
            for (const auto& a : attrs_) {
                if (a.first == "__color") colorHex = a.second;
            }
            auto pb = std::make_shared<FluxUI::ProgressBar>();
            pb->progress = val;
            if (!colorHex.empty()) pb->barColor = FluxUI::Color::fromHex(colorHex);
            pb->parent = parent;
            if (!className_.empty()) pb->className = className_;
            parent->children.push_back(pb);
            w = pb.get();
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
            if (a.first.empty() || a.first[0] == '_') continue; // skip internal attrs
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
inline Element tagOnly(const char* tag) {
    return Element(tag);
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
inline Element Video(const std::string& src) { return detail::leaf("video", src); }
inline Element Canvas()                      { return detail::tagOnly("canvas"); }
inline Element Hr()                          { return detail::tagOnly("hr"); }
inline Element Br()                          { return detail::tagOnly("br"); }

// Icon glyph (configured on mount; routes to the Icon widget).
inline Element Icon(const std::string& glyph) {
    Element e = detail::tagOnly("icon");
    e.onMount<FluxUI::Icon>([glyph](FluxUI::Icon* i) { i->glyph = glyph; });
    return e;
}

// Animated progress bar with a value in [0,1] and optional accent color.
inline Element ProgressBar(float value, const std::string& colorHex = "") {
    Element e = detail::tagOnly("progress-bar");
    e.content = std::to_string(value);
    if (!colorHex.empty()) e.attr("__color", colorHex);
    return e;
}

// Statistic card (title / value / subtitle / accent), a common dashboard widget.
inline Element StatCard(const std::string& title, const std::string& value,
                        const std::string& subtitle = "", const std::string& accentHex = "#6C5CE7") {
    Element e = detail::tagOnly("stat-card");
    e.content = title;
    e.attr("__value", value);
    e.attr("__subtitle", subtitle);
    e.attr("__accent", accentHex);
    return e;
}


// Checkbox / Radio with an initial checked state via post-mount hook.
inline Element Checkbox(bool checked = false) {
    Element e = detail::tagOnly("checkbox");
    if (checked) e.onMount<FluxUI::Checkbox>([](FluxUI::Checkbox* c) { c->setChecked(true); });
    return e;
}
inline Element Radio(bool checked = false, const std::string& group = "") {
    Element e = detail::tagOnly("radio");
    e.onMount<FluxUI::Radio>([checked, group](FluxUI::Radio* r) {
        if (!group.empty()) r->group = group;
        if (checked) r->setChecked(true);
    });
    return e;
}

// Range slider (min/max/step/value) configured on mount.
inline Element Range(float value = 0.5f, float min = 0.0f, float max = 1.0f, float step = 0.01f) {
    Element e = detail::tagOnly("range");
    e.onMount<FluxUI::RangeInput>([=](FluxUI::RangeInput* r) {
        r->min = min; r->max = max; r->step = step; r->setValue(value, false);
    });
    return e;
}

// Typed <input> (text/email/password/search/number/...).
inline Element Input(const std::string& type, const std::string& placeholder) {
    Element e = detail::leaf("input", placeholder);
    e.onMount<FluxUI::TextInput>([type](FluxUI::TextInput* in) { in->setInputType(type); });
    return e;
}

// Meter and Progress with their value range.
inline Element Meter(float value, float min = 0.0f, float max = 1.0f) {
    Element e = detail::tagOnly("meter");
    e.onMount<FluxUI::Meter>([=](FluxUI::Meter* m) { m->min = min; m->max = max; m->value = value; });
    return e;
}
inline Element Progress(float value = -1.0f, float max = 1.0f) {
    Element e = detail::tagOnly("progress");
    e.onMount<FluxUI::Progress>([=](FluxUI::Progress* p) { p->max = max; p->value = value; });
    return e;
}

// Disclosure widgets.
inline Element Details(std::initializer_list<Element> kids = {}) { return detail::container("details", kids); }
inline Element Summary(const std::string& content) { return detail::leaf("summary", content); }
inline Element Dialog(std::initializer_list<Element> kids = {}) { return detail::container("dialog", kids); }

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
//
//  Supports declarative routing: register views as functions that return an
//  Element, navigate between them with navigate(). The layout is:
//
//      Shell (permanent: sidebar/header/footer) + Content (swapped per route)
//
//  Use setLayout() for the shell, and addRoute()/navigate() for views.
//  Or use setRoot() for a simple single-page app without routing.
// ============================================================
class App {
    FluxUI::Application app_;
    bool windowReady_ = false;
    std::string currentRoute_;
    std::unordered_map<std::string, std::function<Element()>> routes_;
    std::function<Element(const Element& content)> layout_;
    FluxUI::Widget* contentSlot_ = nullptr;
    static inline App* s_instance = nullptr;
    std::function<void(float)> onTick_;

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

    void mountRoute() {
        if (!contentSlot_) return;
        contentSlot_->clearChildren();
        auto it = routes_.find(currentRoute_);
        if (it != routes_.end()) {
            Element view = it->second();
            view.mount(contentSlot_);
        }
        app_.requestRedraw();
    }

public:
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    App() { ensureWindow(1200, 800, "FluxUI App"); s_instance = this; }
    App(int width, int height, const std::string& title) { ensureWindow(width, height, title); s_instance = this; }

    // Global access to the current app (there's only one per process).
    static App& current() { return *s_instance; }

    void loadCSS(const std::string& path) { app_.loadStylesheet(path); }
    void addCSS(const std::string& css)   { app_.addStylesheet(css); }
    void loadStyle(const std::string& path) { loadCSS(path); }

    // --- Simple single-page mode (no routing) ---
    void setRoot(const Element& root) {
        auto rootWidget = app_.root();
        rootWidget->clearChildren();
        root.mount(rootWidget);
    }

    // --- Multi-view routing mode ---

    // Register a named route (e.g. "/dashboard") with a view builder function.
    void addRoute(const std::string& path, std::function<Element()> builder) {
        routes_[path] = std::move(builder);
    }

    // Set the app shell layout. It receives the view content as an argument.
    // The shell should contain a Div({}).id("__content__") where the view mounts.
    // Alternatively, use setLayout with a slot callback.
    void setLayout(std::function<Element(const Element& content)> layout) {
        layout_ = std::move(layout);
    }

    // Navigate to a registered route; rebuilds the shell so nav highlights update.
    void navigate(const std::string& path) {
        currentRoute_ = path;
        if (layout_) {
            build(path);  // full rebuild so sidebar nav highlights refresh
        } else {
            mountRoute();
        }
    }

    const std::string& route() const { return currentRoute_; }

    // Build the shell + initial route. Call after addRoute/setLayout.
    // If no initialRoute given, uses the first registered route.
    void build(const std::string& initialRoute = "") {
        auto rootWidget = app_.root();
        rootWidget->clearChildren();

        // The shell layout owns the flex-row split (sidebar + content). The root
        // must not scroll — its content area handles its own scrolling — and must
        // not clip, so child scrollbars reach the window edge (browser-like).
        auto& rootStyle = rootWidget->style;
        rootStyle.overflowY = FluxUI::Overflow::Visible;
        rootStyle.overflowX = FluxUI::Overflow::Visible;
        rootStyle.display = FluxUI::Display::Flex;
        rootStyle.flexDirection = FluxUI::FlexDirection::Row;
        // init() force-sets computedStyle directly; override it too.
        auto& rootComputed = rootWidget->computedStyle.ensureMutable();
        rootComputed.overflowY = FluxUI::Overflow::Visible;
        rootComputed.overflowX = FluxUI::Overflow::Visible;
        rootComputed.display = FluxUI::Display::Flex;
        rootComputed.flexDirection = FluxUI::FlexDirection::Row;

        if (!initialRoute.empty()) {
            currentRoute_ = initialRoute;
        } else if (currentRoute_.empty() && !routes_.empty()) {
            currentRoute_ = routes_.begin()->first;
        }

        if (layout_) {
            // Build the content element for the initial route.
            Element content;
            auto it = routes_.find(currentRoute_);
            if (it != routes_.end()) content = it->second();

            Element shell = layout_(content);
            // Mount the shell's children directly onto root (root already has
            // the .root class with display:flex from the theme CSS).
            for (const auto& child : shell.children_) {
                child.mount(rootWidget);
            }
            // If shell has no children, mount it as-is (single-element layout).
            if (shell.children_.empty()) {
                shell.mount(rootWidget);
            }

            // Find the content slot: the widget with id "__content__"
            std::function<FluxUI::Widget*(FluxUI::Widget*)> findSlot;
            findSlot = [&](FluxUI::Widget* w) -> FluxUI::Widget* {
                if (w->id == "__content__") return w;
                for (auto& child : w->children) {
                    if (auto* found = findSlot(child.get())) return found;
                }
                return nullptr;
            };
            contentSlot_ = findSlot(rootWidget);
        } else if (!routes_.empty()) {
            // No layout, just mount the route directly into root.
            contentSlot_ = rootWidget;
            mountRoute();
        }
    }

    int run() {
        // If build() was never called, do it now (convenience for simple apps).
        if (!contentSlot_ && !routes_.empty()) build();
        app_.onUpdate = [this](float dt) {
            detail::pumpReactiveBindings();
            if (onTick_) onTick_(dt);
        };
        app_.run();
        return 0;
    }

    // Register a per-frame callback for animations/timers (receives delta seconds).
    void onTick(std::function<void(float)> fn) { onTick_ = std::move(fn); }

    void stop() { app_.running = false; }
    FluxUI::Application& raw() { return app_; }
};

} // namespace fluxui
