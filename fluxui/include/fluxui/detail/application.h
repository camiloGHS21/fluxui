#pragma once
// FluxUI public API - UI events, ResizeObserver, ViewTransition, Application.
// Auto-split from widgets.h; do not include directly, use <fluxui/widgets.h>.
#include "fluxui/detail/widget_elements.h"
namespace FluxUI {
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

struct ResizeObserverSize {
    float inlineSize = 0.0f;
    float blockSize = 0.0f;
};

struct ResizeObserverEntry {
    Widget* target = nullptr;
    Rect contentRect;
    std::vector<ResizeObserverSize> borderBoxSize;
    std::vector<ResizeObserverSize> contentBoxSize;
};

class ResizeObserver;
using ResizeObserverCallback = std::function<void(const std::vector<ResizeObserverEntry>&, ResizeObserver&)>;

class ResizeObserver {
public:
    ResizeObserver(ResizeObserverCallback callback);
    ~ResizeObserver();

    void observe(Widget* target);
    void unobserve(Widget* target);
    void disconnect();

    void gatherObservations(std::vector<ResizeObserverEntry>& activeObservations);
    void deliverObservations(const std::vector<ResizeObserverEntry>& activeObservations);

private:
    ResizeObserverCallback callback_;
    struct ObservedTarget {
        Widget* target = nullptr;
        float lastWidth = -1.0f;
        float lastHeight = -1.0f;
    };
    std::vector<ObservedTarget> observedTargets_;
};

struct ViewTransition {
    bool active = false;
    std::vector<RenderCommand> oldCommands;
    std::vector<RenderCommand> newCommands;
    std::chrono::high_resolution_clock::time_point startTime;
    float duration = 0.25f;
};

class Application {
public:
    void startViewTransition(std::function<void()> mutationCallback);

    using EventCallback = std::function<void(UIEvent&)>;
    using RouteBuilder = std::function<void(Application&, Widget*)>;
    using ActionCallback = std::function<void(Application&, const std::string&)>;
    static Application* instance();
    void registerResizeObserver(ResizeObserver* observer);
    void unregisterResizeObserver(ResizeObserver* observer);
    void onWidgetDestroyed(Widget* w);
    bool init(const std::string& title, int width, int height);
    bool init(const std::string& title, int width, int height, RenderBackendType backend);
    void run();
    void renderFrame();
    void shutdown();
    void setBackend(RenderBackendType backend);
    RenderBackendType backendPreference() const { return backendPreference_; }
    RenderBackendType activeBackend() const { return renderer_.activeBackend(); }
    const char* activeBackendName() const { return renderer_.activeBackendName(); }

    // GPU device selection. Call before init(). Defaults to PowerSaving so the
    // UI runs on the integrated GPU (leaving a discrete RTX free for games);
    // FLUXUI_GPU env var overrides. Falls back to CPU/software if no GPU works.
    void setGpuPreference(GpuPreference pref) { renderer_.setGpuPreference(pref); }
    GpuPreference gpuPreference() const { return renderer_.gpuPreference(); }
    const std::string& activeDeviceName() const { return renderer_.activeDeviceName(); }

    bool loadStylesheet(const std::string& path);
    void addStylesheet(const std::string& css);

    // ── CSS Hot-Reload (developer experience) ──────────────────────────────
    // When enabled, the run loop polls every watched stylesheet file's
    // modification time and, on change, rebuilds the whole cascade from the
    // recorded style sources (files + inline CSS, in original order) and
    // re-styles the tree live — no recompile or relaunch. loadStylesheet()
    // automatically watches the file it loads. Inline addStylesheet() CSS is
    // replayed in order during a reload so the cascade is identical.
    void enableHotReload(bool enable = true, float pollIntervalSeconds = 0.25f);
    bool hotReloadEnabled() const { return hotReloadEnabled_; }
    // Watch an extra CSS file for changes (e.g. one pulled in via @import or a
    // partial that loadStylesheet() didn't see). Safe to call repeatedly.
    void watchStylesheet(const std::string& path);
    // Force an immediate rebuild of the cascade from all recorded sources and
    // re-style the tree. Returns false if a watched file could not be read.
    bool reloadStyles();
    // Poll watched files once; reloads if any changed. Called automatically by
    // the run loop when hot-reload is on; exposed for custom/embedded loops.
    bool pollStyleHotReload();
    Widget* root() {
        if (!root_) {
            root_ = std::make_shared<Panel>();
            root_->id = "root";
            root_->className = "root";
            root_->computedStyle.ensureMutable().display = Display::Flex;
            root_->computedStyle.ensureMutable().flexDirection = FlexDirection::Row;
        }
        return root_.get();
    }
    AXObjectCache* axObjectCache() {
        if (!axObjectCache_) {
            axObjectCache_ = std::make_unique<AXObjectCache>();
        }
        return axObjectCache_.get();
    }
    Renderer& renderer() { return renderer_; }
    StyleSheet& stylesheet() { return stylesheet_; }
    InputState& input() { return input_; }
    void* getWindowHandle() const { return window_; }
    size_t on(UIEventType type, EventCallback callback);
    void off(size_t listenerId);
    void emit(UIEvent event);
    size_t addAction(const std::string& name,
                     int keyCode,
                     int modifiers,
                     ActionCallback callback);
    void removeAction(size_t actionId);
    bool dispatchAction(const std::string& name);
    void registerAction(const std::string& name, ActionCallback callback);
    void addKeymap(const std::string& jsonContent);
    bool loadKeymap(const std::string& path);
    Widget* focusedWidget();
    bool dispatchKeyAction(int keyCode, int modifiers);
    void dispatchMouseDown(int button, float x, float y, int clickCount);
    void dispatchMouseUp(int button, float x, float y);
    void dispatchMouseMove(float x, float y, float dx, float dy);
    void dispatchMouseWheel(float x, float y, float dx, float dy);
    void dispatchKeyDown(int keyCode, int modifiers);
    void dispatchKeyUp(int keyCode, int modifiers);
    void dispatchTextInput(const std::string& text);
    void addRoute(const std::string& path, RouteBuilder builder);
    void setNotFoundRoute(RouteBuilder builder);
    bool navigate(const std::string& path);
    bool renderRoute(Widget* container);
    const std::string& currentRoute() const { return currentRoute_; }
    bool routeDirty() const { return routeDirty_; }
    std::function<void(float dt)> onUpdate;
    std::function<void()> onRender;
    void requestRedraw() { needsRedraw_ = true; }
    bool needsRedraw() const { return needsRedraw_; }

    // ── Adaptive power-aware frame pacing ──────────────────────────────────
    // When enabled (default), FluxUI lowers the frame rate automatically to
    // save CPU/GPU and battery: full rate on AC power with the window focused,
    // a reduced rate on battery, and a low idle rate when the window is in the
    // background or minimized. The CPU/software backend is always capped. This
    // makes the app light on laptops on battery and on machines without a GPU.
    enum class PowerProfile {
        Auto,            // pick the cap from power source + focus (default)
        HighPerformance, // always target the max FPS (ignore battery)
        Balanced,        // moderate caps even on AC
        PowerSaver       // aggressive low caps (great for battery / no-GPU)
    };
    void setPowerProfile(PowerProfile profile) { powerProfile_ = profile; }
    PowerProfile powerProfile() const { return powerProfile_; }
    // Override the FPS used for each pacing tier. 0 keeps the built-in default.
    void setFrameRateLimits(int activeFps, int batteryFps, int backgroundFps);

    // Inputs that drive the pacing decision (kept small + POD so the policy is
    // unit-testable in isolation, without a window).
    struct PacingInputs {
        PowerProfile profile = PowerProfile::Auto;
        bool onBattery = false;
        bool batterySaver = false;
        bool windowActive = true;
        bool softwareBackend = false;
        bool hasAnimations = false;
        int activeFps = 120;
        int batteryFps = 60;
        int backgroundFps = 10;
    };
    // Pure policy: returns the target FPS for the given conditions (0 = uncapped).
    static int computeTargetFps(const PacingInputs& in);
    void updateStyleAndLayout();
    bool running = true;
    DocumentLifecycle documentLifecycle = DocumentLifecycle::Uninitialized;
    template<typename T>
    std::future<T> async(std::function<T()> task) {
        return std::async(std::launch::async, std::move(task));
    }
    void lazyLoad(std::function<void()> loader, std::function<void()> onComplete = nullptr);
    void runOnMainThread(std::function<void()> task);
private:
    void* window_ = nullptr;
    void* defaultCursor_ = nullptr;
    void* pointerCursor_ = nullptr;
    void* textCursor_ = nullptr;
    void* resizeNWSECursor_ = nullptr;
    CursorType activeCursor_ = CursorType::Default;
    Renderer renderer_;
    RenderBackendType backendPreference_ = Renderer::defaultBackend();
    StyleSheet stylesheet_;

    // ── CSS hot-reload state ──
    // Ordered record of every style source applied to the cascade, so a reload
    // can rebuild it identically. Files also carry the last-seen mtime.
    struct StyleSource {
        bool isFile = false;
        std::string pathOrCss;                 // file path, or inline CSS text
        int64_t lastWriteNs = 0;               // file mtime (files only)
    };
    std::vector<StyleSource> styleSources_;
    bool hotReloadEnabled_ = false;
    float hotReloadInterval_ = 0.25f;
    float hotReloadAccum_ = 0.0f;
    static int64_t fileWriteTimeNs(const std::string& path);

    // ── Adaptive frame pacing state ──
    PowerProfile powerProfile_ = PowerProfile::Auto;
    int activeFps_ = 0;        // 0 => use FLUXUI_TARGET_FPS build default
    int batteryFps_ = 60;      // cap while running on battery
    int backgroundFps_ = 10;   // cap while window is unfocused/minimized
    float powerPollAccum_ = 0.0f;       // seconds since last power-status poll
    bool cachedOnBattery_ = false;      // last polled power source
    bool cachedBatterySaver_ = false;   // last polled battery-saver flag
    InputState input_;
    std::shared_ptr<Widget> root_;
    std::unique_ptr<AXObjectCache> axObjectCache_;
    std::chrono::high_resolution_clock::time_point lastTime_;
    std::unordered_map<std::string, RouteBuilder> routes_;
    RouteBuilder notFoundRoute_;
    std::string currentRoute_;
    ViewTransition activeViewTransition_;
    bool routeDirty_ = false;
    bool needsRedraw_ = true;
    Widget* lastHoveredWidget_ = nullptr;
    Widget* lastMouseDownTarget_[3] = { nullptr, nullptr, nullptr };
    struct EventListener {
        size_t id = 0;
        UIEventType type = UIEventType::Any;
        EventCallback callback;
    };
    std::vector<EventListener> eventListeners_;
    size_t nextEventListenerId_ = 1;
    struct ActionBinding {
        size_t id = 0;
        std::string name;
        int keyCode = 0;
        int modifiers = 0;
        ActionCallback callback;
    };
    std::vector<ActionBinding> actionBindings_;
    size_t nextActionId_ = 1;
    struct KeymapEntry {
        int keyCode = 0;
        int modifiers = 0;
        std::string context;
        std::string actionName;
    };
    std::vector<KeymapEntry> keymapEntries_;
    std::unordered_map<std::string, ActionCallback> actionHandlers_;
    std::vector<std::function<void()>> mainThreadTasks_;
    std::mutex mainThreadTasksMutex_;
    void processMainThreadTasks();
    void processEvents();
    void updateCursor(CursorType cursor);
    std::vector<ResizeObserver*> resizeObservers_;
};
}