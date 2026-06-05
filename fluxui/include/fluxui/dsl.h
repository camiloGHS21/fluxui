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
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX   // prevent windows.h min/max macros clobbering Rule::min/max etc.
#endif
#include "fluxui/FluxUI.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <initializer_list>
#include <algorithm>
#include <unordered_map>
#include <type_traits>
#include <optional>
#include <map>
#include <atomic>
#include <memory>
#include <thread>

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
    std::vector<std::string> routeOrder_;
    std::function<Element(const Element& content)> layout_;
    FluxUI::Widget* contentSlot_ = nullptr;
    static inline App* s_instance = nullptr;
    std::function<void(float)> onTick_;

    // GPU preference to apply before the window/renderer initializes. Set via the
    // static App::preferGpu() before constructing the App (the constructor inits
    // the renderer immediately, so a fluent setter would otherwise be too late).
    static inline FluxUI::GpuPreference s_pendingGpu = FluxUI::GpuPreference::Auto;

    void ensureWindow(int width, int height, const std::string& title) {
        if (windowReady_) return;
        app_.setGpuPreference(s_pendingGpu);   // apply pre-init GPU choice
        if (app_.init(title, width, height)) {
            windowReady_ = true;
            if (!app_.renderer().loadDefaultFont(16.0f)) {
#ifdef _WIN32
                app_.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
#endif
            }
        }
    }

    // Match a registered route pattern (e.g. "/user/:id") against a concrete
    // path (e.g. "/user/42"). On match, fills params and returns the builder.
    // Public so apps/tests can reuse the matcher.
public:
    static bool matchPattern(const std::string& pattern, const std::string& path,
                             std::map<std::string, std::string>& params) {
        auto split = [](const std::string& s) {
            std::vector<std::string> parts;
            size_t i = 0;
            while (i < s.size()) {
                if (s[i] == '/') { ++i; continue; }
                size_t j = s.find('/', i);
                if (j == std::string::npos) j = s.size();
                parts.push_back(s.substr(i, j - i));
                i = j;
            }
            return parts;
        };
        // Strip query string from the concrete path before matching.
        std::string cleanPath = path;
        size_t q = cleanPath.find('?');
        if (q != std::string::npos) cleanPath = cleanPath.substr(0, q);

        auto pp = split(pattern);
        auto cp = split(cleanPath);
        if (pp.size() != cp.size()) return false;
        std::map<std::string, std::string> found;
        for (size_t i = 0; i < pp.size(); ++i) {
            if (!pp[i].empty() && pp[i][0] == ':') {
                found[pp[i].substr(1)] = cp[i];
            } else if (pp[i] != cp[i]) {
                return false;
            }
        }
        params = std::move(found);
        return true;
    }

private:
    // Resolve currentRoute_ to a builder, filling currentParams_.
    std::function<Element()> resolveRoute() {
        // Exact match first.
        auto it = routes_.find(currentRoute_);
        if (it != routes_.end()) {
            currentParams().clear();
            parseQuery();
            return it->second;
        }
        // Pattern match (params).
        for (const auto& path : routeOrder_) {
            std::map<std::string, std::string> params;
            if (matchPattern(path, currentRoute_, params)) {
                currentParams() = std::move(params);
                parseQuery();
                return routes_[path];
            }
        }
        return nullptr;
    }

    void parseQuery() {
        currentQuery().clear();
        size_t q = currentRoute_.find('?');
        if (q == std::string::npos) return;
        std::string qs = currentRoute_.substr(q + 1);
        size_t i = 0;
        while (i < qs.size()) {
            size_t amp = qs.find('&', i);
            if (amp == std::string::npos) amp = qs.size();
            std::string pair = qs.substr(i, amp - i);
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                currentQuery()[pair.substr(0, eq)] = pair.substr(eq + 1);
            } else if (!pair.empty()) {
                currentQuery()[pair] = "";
            }
            i = amp + 1;
        }
    }

    void mountRoute() {
        if (!contentSlot_) return;
        contentSlot_->clearChildren();
        auto builder = resolveRoute();
        if (builder) {
            Element view = builder();
            view.mount(contentSlot_);
        }
        app_.requestRedraw();
    }

public:
    // Route params from the current matched route (e.g. {"id": "42"}).
    static std::map<std::string, std::string>& currentParams() {
        static std::map<std::string, std::string> params;
        return params;
    }
    // Query string params (e.g. ?tab=info -> {"tab": "info"}).
    static std::map<std::string, std::string>& currentQuery() {
        static std::map<std::string, std::string> query;
        return query;
    }
    // Convenience: read a route param (":id") with optional default.
    static std::string param(const std::string& name, const std::string& def = "") {
        auto& p = currentParams();
        auto it = p.find(name);
        return it != p.end() ? it->second : def;
    }
    static std::string query(const std::string& name, const std::string& def = "") {
        auto& q = currentQuery();
        auto it = q.find(name);
        return it != q.end() ? it->second : def;
    }

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    App() { ensureWindow(1200, 800, "FluxUI App"); s_instance = this; }
    App(int width, int height, const std::string& title) { ensureWindow(width, height, title); s_instance = this; }

    // Global access to the current app (there's only one per process).
    static App& current() { return *s_instance; }

    void loadCSS(const std::string& path) { app_.loadStylesheet(path); }
    void addCSS(const std::string& css)   { app_.addStylesheet(css); }
    void loadStyle(const std::string& path) { loadCSS(path); }

    // ── CSS Hot-Reload ─────────────────────────────────────────────────────
    // Enable live CSS reloading: any stylesheet loaded via loadCSS()/loadStyle()
    // is watched, and edits on disk re-style the running app instantly — no
    // recompile, no relaunch. Inline addCSS() is replayed in order on reload.
    //
    //   App app(1200, 800, "MyApp");
    //   app.loadStyle("assets/styles/theme.css");
    //   app.hotReload();          // watch + live-reload theme.css
    //
    // pollIntervalSeconds tunes how often the file mtime is checked.
    App& hotReload(bool enable = true, float pollIntervalSeconds = 0.25f) {
        app_.enableHotReload(enable, pollIntervalSeconds);
        return *this;
    }
    // Watch an extra CSS file (e.g. a partial or @import target).
    App& watchCSS(const std::string& path) { app_.watchStylesheet(path); return *this; }
    // Force an immediate reload of all CSS sources from disk.
    void reloadCSS() { app_.reloadStyles(); }

    // ── Power / performance profile ────────────────────────────────────────
    // FluxUI adapts its frame rate to save battery and CPU automatically. By
    // default (Auto) it runs full-speed on AC power with a GPU, throttles on
    // battery, caps the software (no-GPU) renderer, and idles in the background.
    // Pick a profile to bias that behavior:
    //
    //   app.powerSaver();        // best battery life / lightest on weak HW
    //   app.highPerformance();   // always max FPS
    //   app.balanced();          // moderate caps
    //   app.powerProfile(App::Profile::Auto);  // default adaptive behavior
    enum class Profile { Auto, HighPerformance, Balanced, PowerSaver };
    App& powerProfile(Profile p) {
        app_.setPowerProfile(static_cast<FluxUI::Application::PowerProfile>(p));
        return *this;
    }
    App& powerSaver()       { return powerProfile(Profile::PowerSaver); }
    App& highPerformance()  { return powerProfile(Profile::HighPerformance); }
    App& balanced()         { return powerProfile(Profile::Balanced); }
    // Tune the FPS tiers (active / on-battery / background). 0 keeps defaults.
    App& frameRateLimits(int activeFps, int batteryFps, int backgroundFps) {
        app_.setFrameRateLimits(activeFps, batteryFps, backgroundFps);
        return *this;
    }

    // ── GPU device selection ───────────────────────────────────────────────
    // On machines with both an integrated GPU and a discrete card (e.g. an
    // RTX laptop), FluxUI drives the UI on the integrated GPU by default so the
    // discrete card stays free and cool for games/compute. If no GPU is usable
    // the renderer falls back to the CPU software rasterizer.
    // (The FLUXUI_GPU env var overrides this at runtime.)
    //
    // IMPORTANT: the renderer initializes when the App is constructed, so to
    // pick a GPU you must set it BEFORE creating the App:
    //
    //     App::preferGpu(App::Gpu::Discrete);   // before constructing App
    //     App app(1200, 800, "MyGame UI");
    //
    // The instance methods below also work but only take effect if called
    // before the window is created (e.g. for the no-arg App()).
    enum class Gpu { Auto, Integrated, Discrete };
    static FluxUI::GpuPreference toPref(Gpu pref) {
        return pref == Gpu::Integrated ? FluxUI::GpuPreference::PowerSaving :
               pref == Gpu::Discrete   ? FluxUI::GpuPreference::Performance :
                                         FluxUI::GpuPreference::Auto;
    }
    // Set the GPU preference to apply at the next App construction.
    static void preferGpu(Gpu pref) { s_pendingGpu = toPref(pref); }
    static void useIntegratedGpu()  { preferGpu(Gpu::Integrated); }
    static void useDiscreteGpu()    { preferGpu(Gpu::Discrete); }
    // Instance setter (effective only before the window is created).
    App& gpu(Gpu pref) { app_.setGpuPreference(toPref(pref)); return *this; }
    // Name of the GPU actually in use (valid after the window is created).
    std::string gpuName() const { return app_.activeDeviceName(); }

    // --- Simple single-page mode (no routing) ---
    void setRoot(const Element& root) {
        auto rootWidget = app_.root();
        rootWidget->clearChildren();
        root.mount(rootWidget);
    }

    // --- Multi-view routing mode ---

    // Register a named route. Supports params: "/user/:id" and exact paths.
    void addRoute(const std::string& path, std::function<Element()> builder) {
        if (routes_.find(path) == routes_.end()) routeOrder_.push_back(path);
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
        } else if (currentRoute_.empty() && !routeOrder_.empty()) {
            currentRoute_ = routeOrder_.front();
        }

        if (layout_) {
            // Build the content element for the initial route.
            Element content;
            auto builder = resolveRoute();
            if (builder) content = builder();

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

// ============================================================
//  Store<T> — global state container (Zustand-style)
//
//  Like Zustand: a single shared store with state + actions. Components read
//  via get()/select() and subscribe to changes. Mutations notify subscribers
//  and request a redraw. Use it as a process-global singleton:
//
//      struct CartState { int count = 0; };
//      auto& cart = useStore<CartState>();
//      cart.set([](CartState& s){ s.count++; });
//      Text([&]{ return std::to_string(cart.get().count); });
// ============================================================
template <typename T>
class Store {
    T state_;
    std::vector<std::function<void()>> subscribers_;
public:
    Store() = default;
    explicit Store(T initial) : state_(std::move(initial)) {}

    const T& get() const { return state_; }

    // Mutate via a reducer-style function; notifies all subscribers.
    void set(std::function<void(T&)> mutator) {
        mutator(state_);
        notify();
    }
    // Replace the whole state.
    void replace(T next) { state_ = std::move(next); notify(); }

    // Read a derived value.
    template <typename R>
    R select(std::function<R(const T&)> selector) const { return selector(state_); }

    void subscribe(std::function<void()> fn) { subscribers_.push_back(std::move(fn)); }

    void notify() {
        for (auto& fn : subscribers_) fn();
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
    }
};

// Process-global store accessor (Zustand-style singleton per type).
template <typename T>
inline Store<T>& useStore() {
    static Store<T> store;
    return store;
}

// ============================================================
//  Schema — runtime validation (Zod-style)
//
//  Build a schema and validate values, collecting human-readable errors:
//
//      auto schema = Schema()
//          .field("email", Rule::string().email())
//          .field("age",   Rule::number().min(18));
//      auto result = schema.validate({{"email","a@b.com"},{"age","20"}});
//      if (!result.ok) for (auto& e : result.errors) ...
// ============================================================
struct Rule {
    enum class Kind { String, Number, Bool };
    Kind kind = Kind::String;
    bool required_ = true;
    std::optional<double> min_, max_;
    std::optional<size_t> minLen_, maxLen_;
    bool email_ = false;

    static Rule string() { Rule r; r.kind = Kind::String; return r; }
    static Rule number() { Rule r; r.kind = Kind::Number; return r; }
    static Rule boolean() { Rule r; r.kind = Kind::Bool; return r; }

    Rule& optional() { required_ = false; return *this; }
    Rule& min(double v) { min_ = v; return *this; }
    Rule& max(double v) { max_ = v; return *this; }
    Rule& minLength(size_t v) { minLen_ = v; return *this; }
    Rule& maxLength(size_t v) { maxLen_ = v; return *this; }
    Rule& email() { email_ = true; return *this; }

    // Validate a string value; returns an error message or empty if valid.
    std::string check(const std::string& field, const std::string& value) const {
        if (value.empty()) {
            return required_ ? (field + " is required") : "";
        }
        if (kind == Kind::Number) {
            try {
                double d = std::stod(value);
                if (min_ && d < *min_) return field + " must be >= " + std::to_string((long long)*min_);
                if (max_ && d > *max_) return field + " must be <= " + std::to_string((long long)*max_);
            } catch (...) {
                return field + " must be a number";
            }
        } else if (kind == Kind::String) {
            if (minLen_ && value.size() < *minLen_) return field + " is too short";
            if (maxLen_ && value.size() > *maxLen_) return field + " is too long";
            if (email_ && value.find('@') == std::string::npos) return field + " must be a valid email";
        } else if (kind == Kind::Bool) {
            if (value != "true" && value != "false") return field + " must be true/false";
        }
        return "";
    }
};

struct ValidationResult {
    bool ok = true;
    std::map<std::string, std::string> errors;  // field -> message
    std::string first() const { return errors.empty() ? "" : errors.begin()->second; }
};

class Schema {
    std::vector<std::pair<std::string, Rule>> fields_;
public:
    Schema& field(const std::string& name, Rule rule) {
        fields_.emplace_back(name, std::move(rule));
        return *this;
    }
    ValidationResult validate(const std::map<std::string, std::string>& data) const {
        ValidationResult res;
        for (const auto& [name, rule] : fields_) {
            auto it = data.find(name);
            std::string val = it != data.end() ? it->second : "";
            std::string err = rule.check(name, val);
            if (!err.empty()) { res.ok = false; res.errors[name] = err; }
        }
        return res;
    }
};

// ============================================================
//  Query<T> — async data fetching with loading/error/data states
//  (React Query / SWR-style). Runs a worker on a background thread and exposes
//  reactive status. Render different UI per state with .view():
//
//      static auto users = Query<std::string>([]{ return httpGet("/api/users"); });
//      users.start();
//      // in a view:
//      users.view(
//          []{ return Skeleton(3); },                     // loading
//          [](const std::string& data){ return Text(data); }, // success
//          [](const std::string& err){ return Text("Error: " + err); } // error
//      )
// ============================================================
enum class QueryStatus { Idle, Loading, Success, Error };

template <typename T>
class Query {
    std::function<T()> fetcher_;
    std::shared_ptr<std::atomic<int>> status_ = std::make_shared<std::atomic<int>>((int)QueryStatus::Idle);
    std::shared_ptr<T> data_ = std::make_shared<T>();
    std::shared_ptr<std::string> error_ = std::make_shared<std::string>();
public:
    explicit Query(std::function<T()> fetcher) : fetcher_(std::move(fetcher)) {}

    QueryStatus status() const { return (QueryStatus)status_->load(); }
    bool isLoading() const { return status() == QueryStatus::Loading; }
    bool isSuccess() const { return status() == QueryStatus::Success; }
    bool isError() const { return status() == QueryStatus::Error; }
    const T& data() const { return *data_; }
    const std::string& error() const { return *error_; }

    // Kick off the fetch on a background thread (idempotent while loading).
    void start() {
        if (status() == QueryStatus::Loading) return;
        status_->store((int)QueryStatus::Loading);
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
        auto status = status_; auto data = data_; auto error = error_;
        auto fetcher = fetcher_;
        std::thread([status, data, error, fetcher]() {
            try {
                T result = fetcher();
                if (auto* app = FluxUI::Application::instance()) {
                    app->runOnMainThread([status, data, result]() {
                        *data = result;
                        status->store((int)QueryStatus::Success);
                        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
                    });
                }
            } catch (const std::exception& e) {
                std::string msg = e.what();
                if (auto* app = FluxUI::Application::instance()) {
                    app->runOnMainThread([status, error, msg]() {
                        *error = msg;
                        status->store((int)QueryStatus::Error);
                        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
                    });
                }
            } catch (...) {
                if (auto* app = FluxUI::Application::instance()) {
                    app->runOnMainThread([status, error]() {
                        *error = "unknown error";
                        status->store((int)QueryStatus::Error);
                        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
                    });
                }
            }
        }).detach();
    }

    void refetch() { status_->store((int)QueryStatus::Idle); start(); }

    // Render the right Element for the current state. Loading state defaults to
    // a skeleton if no loading builder is provided.
    Element view(std::function<Element()> onLoading,
                 std::function<Element(const T&)> onSuccess,
                 std::function<Element(const std::string&)> onError = nullptr) const {
        switch (status()) {
            case QueryStatus::Success: return onSuccess(*data_);
            case QueryStatus::Error:   return onError ? onError(*error_)
                                                      : detail::leaf("p", "Error: " + *error_);
            default:                   return onLoading ? onLoading()
                                                        : detail::leaf("div", "");
        }
    }
};

// ============================================================
//  Skeleton — easy loading placeholders (shimmer lines / blocks)
// ============================================================
inline Element Skeleton(int lines = 3) {
    Element e("div");
    e.className_ = "skeleton";
    for (int i = 0; i < lines; ++i) {
        e.children_.push_back(detail::tagOnly("div").className("skeleton-line"));
    }
    return e;
}
inline Element SkeletonBox(const std::string& w = "100%", const std::string& h = "120px") {
    return detail::tagOnly("div").className("skeleton skeleton-box").style("width", w).style("height", h);
}

} // namespace fluxui
