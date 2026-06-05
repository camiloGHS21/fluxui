#pragma once
// FluxUI public API - App: owns Application and drives the reactive loop.
// Auto-split from dsl.h; do not include directly, use <fluxui/dsl.h>.
#include "fluxui/detail/dsl_core.h"
namespace fluxui {
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
    // When set (via App::gameMode()), also run the frame loop uncapped.
    static inline bool s_pendingGameMode = false;

    void ensureWindow(int width, int height, const std::string& title) {
        if (windowReady_) return;
        app_.setGpuPreference(s_pendingGpu);   // apply pre-init GPU choice
        if (app_.init(title, width, height)) {
            windowReady_ = true;
            // Game mode: max-FPS pacing to pair with the discrete GPU.
            if (s_pendingGameMode) {
                app_.setPowerProfile(FluxUI::Application::PowerProfile::HighPerformance);
            }
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
    // RTX laptop), FluxUI drives the UI on the INTEGRATED GPU by default so the
    // discrete card stays free and cool — exactly what a desktop app/UI wants.
    // If no GPU is usable the renderer falls back to the CPU software rasterizer.
    //
    //   default                 -> integrated GPU (or CPU if none)
    //   App::useDiscreteGpu()    -> force the discrete GPU (e.g. RTX)
    //   App::gameMode()          -> discrete GPU + uncapped HighPerformance pacing
    //                               (for games / high-performance apps)
    //
    // (The FLUXUI_GPU env var — integrated | discrete | game — overrides this.)
    //
    // IMPORTANT: the renderer initializes when the App is constructed, so to
    // pick a GPU you must set it BEFORE creating the App:
    //
    //     App::gameMode();                       // before constructing App
    //     App app(1920, 1080, "My Game");
    //
    // The instance methods also work but only take effect before the window is
    // created (e.g. for the no-arg App()).
    enum class Gpu { Auto, Integrated, Discrete, Game };
    static FluxUI::GpuPreference toPref(Gpu pref) {
        return pref == Gpu::Integrated ? FluxUI::GpuPreference::PowerSaving :
               (pref == Gpu::Discrete || pref == Gpu::Game) ? FluxUI::GpuPreference::Performance :
                                         FluxUI::GpuPreference::Auto;
    }
    // Set the GPU preference to apply at the next App construction.
    static void preferGpu(Gpu pref) {
        s_pendingGpu = toPref(pref);
        // Game mode also asks the run loop for uncapped, full-speed pacing.
        s_pendingGameMode = (pref == Gpu::Game);
    }
    static void useIntegratedGpu()  { preferGpu(Gpu::Integrated); }
    static void useDiscreteGpu()    { preferGpu(Gpu::Discrete); }
    // Game / high-performance mode: pick the discrete GPU AND run the frame loop
    // at maximum FPS (no battery/idle throttling). Call before constructing App.
    static void gameMode()          { preferGpu(Gpu::Game); }
    // Instance setter (effective only before the window is created).
    App& gpu(Gpu pref) {
        app_.setGpuPreference(toPref(pref));
        if (pref == Gpu::Game) app_.setPowerProfile(FluxUI::Application::PowerProfile::HighPerformance);
        return *this;
    }
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
} // namespace fluxui