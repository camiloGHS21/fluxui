#pragma once
// FluxUI public API - Widget base class, events, lifecycle, paint properties.
// Auto-split from widgets.h; do not include directly, use <fluxui/widgets.h>.
#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/renderer.h"
#include "fluxui/accessibility.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <memory_resource>
#include <functional>
#include <cstddef>
#include <utility>
#include <future>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <cctype>
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
}
class Widget;
enum class EventPhase {
    None = 0,
    Capture = 1,
    AtTarget = 2,
    Bubble = 3
};
struct Event {
    std::string type;
    Widget* target = nullptr;
    Widget* currentTarget = nullptr;
    EventPhase phase = EventPhase::None;
    bool bubbles = true;
    bool cancelable = true;
    bool propagationStopped = false;
    bool defaultPrevented = false;
    Vec2 mousePos{0.0f, 0.0f};
    Vec2 mouseDelta{0.0f, 0.0f};
    Vec2 scroll{0.0f, 0.0f};
    int button = 0;
    int clickCount = 0;
    int keyCode = 0;
    int modifiers = 0;
    std::string text;
    void stopPropagation() { propagationStopped = true; }
    void preventDefault() { defaultPrevented = true; }
};
using DOMEventListener = std::function<void(Event&)>;
class Panel;
class Text;
class Button;
class TextInput;
class TextArea;
class Checkbox;
class Radio;
class RangeInput;
class Select;
class Option;
class Icon;
class Image;
class Video;
class ProgressBar;
class Placeholder;
class Canvas;
class LazyPanel;
class VirtualList;
class Anchor;
class Details;
class Summary;
class Dialog;
class Meter;
class Progress;
class Hr;
class Br;
class Svg;
class SvgElement;
class SvgG;
class SvgPath;
class SvgRect;
class SvgCircle;
class SvgEllipse;
class SvgLine;
class SvgPolyline;
class SvgPolygon;
class LayoutObject;
enum class VirtualListScrollStrategy {
    Start = 0,
    Center = 1,
    End = 2,
    Nearest = 3
};

enum class DocumentLifecycleState {
    Uninitialized = 0,
    InStyleRecalc,
    StyleClean,
    InLayout,
    LayoutClean,
    InPrePaint,
    PrePaintClean,
    InPaint,
    PaintClean
};

class DocumentLifecycle {
public:
    using State = DocumentLifecycleState;

    DocumentLifecycle() : state_(DocumentLifecycleState::Uninitialized) {}
    DocumentLifecycle(DocumentLifecycleState state) : state_(state) {}

    operator DocumentLifecycleState() const { return state_; }

    DocumentLifecycle& operator=(DocumentLifecycleState nextState) {
        checkTransition(nextState);
        state_ = nextState;
        return *this;
    }

    bool operator==(DocumentLifecycleState other) const { return state_ == other; }
    bool operator!=(DocumentLifecycleState other) const { return state_ != other; }
    bool operator<(DocumentLifecycleState other) const { return state_ < other; }
    bool operator<=(DocumentLifecycleState other) const { return state_ <= other; }
    bool operator>(DocumentLifecycleState other) const { return state_ > other; }
    bool operator>=(DocumentLifecycleState other) const { return state_ >= other; }

    bool operator==(const DocumentLifecycle& other) const { return state_ == other.state_; }
    bool operator!=(const DocumentLifecycle& other) const { return state_ != other.state_; }

    const char* toString() const {
        return toString(state_);
    }

    static const char* toString(DocumentLifecycleState state) {
        switch (state) {
            case DocumentLifecycleState::Uninitialized: return "Uninitialized";
            case DocumentLifecycleState::InStyleRecalc: return "InStyleRecalc";
            case DocumentLifecycleState::StyleClean: return "StyleClean";
            case DocumentLifecycleState::InLayout: return "InLayout";
            case DocumentLifecycleState::LayoutClean: return "LayoutClean";
            case DocumentLifecycleState::InPrePaint: return "InPrePaint";
            case DocumentLifecycleState::PrePaintClean: return "PrePaintClean";
            case DocumentLifecycleState::InPaint: return "InPaint";
            case DocumentLifecycleState::PaintClean: return "PaintClean";
            default: return "Unknown";
        }
    }

    // Keep enum compatibility for scopes/constants like DocumentLifecycle::InStyleRecalc
    static constexpr DocumentLifecycleState Uninitialized = DocumentLifecycleState::Uninitialized;
    static constexpr DocumentLifecycleState InStyleRecalc = DocumentLifecycleState::InStyleRecalc;
    static constexpr DocumentLifecycleState StyleClean = DocumentLifecycleState::StyleClean;
    static constexpr DocumentLifecycleState InLayout = DocumentLifecycleState::InLayout;
    static constexpr DocumentLifecycleState LayoutClean = DocumentLifecycleState::LayoutClean;
    static constexpr DocumentLifecycleState InPrePaint = DocumentLifecycleState::InPrePaint;
    static constexpr DocumentLifecycleState PrePaintClean = DocumentLifecycleState::PrePaintClean;
    static constexpr DocumentLifecycleState InPaint = DocumentLifecycleState::InPaint;
    static constexpr DocumentLifecycleState PaintClean = DocumentLifecycleState::PaintClean;

private:
    void checkTransition(DocumentLifecycleState nextState) {
        if (state_ == nextState) return;
        
        bool valid = false;
        switch (state_) {
            case DocumentLifecycleState::Uninitialized:
                valid = (nextState == DocumentLifecycleState::InStyleRecalc);
                break;
            case DocumentLifecycleState::InStyleRecalc:
                valid = (nextState == DocumentLifecycleState::StyleClean);
                break;
            case DocumentLifecycleState::StyleClean:
                valid = (nextState == DocumentLifecycleState::InLayout || 
                         nextState == DocumentLifecycleState::InStyleRecalc);
                break;
            case DocumentLifecycleState::InLayout:
                valid = (nextState == DocumentLifecycleState::LayoutClean);
                break;
            case DocumentLifecycleState::LayoutClean:
                valid = (nextState == DocumentLifecycleState::InPrePaint ||
                         nextState == DocumentLifecycleState::InStyleRecalc);
                break;
            case DocumentLifecycleState::InPrePaint:
                valid = (nextState == DocumentLifecycleState::PrePaintClean);
                break;
            case DocumentLifecycleState::PrePaintClean:
                valid = (nextState == DocumentLifecycleState::InPaint ||
                         nextState == DocumentLifecycleState::InStyleRecalc);
                break;
            case DocumentLifecycleState::InPaint:
                valid = (nextState == DocumentLifecycleState::PaintClean);
                break;
            case DocumentLifecycleState::PaintClean:
                valid = (nextState == DocumentLifecycleState::InStyleRecalc);
                break;
        }

        if (!valid) {
            std::cerr << "[DocumentLifecycle] WARNING: Strict lifecycle violation! Attempted transition from "
                      << toString(state_) << " to " << toString(nextState) << " is out-of-order!\n";
        }
    }

    DocumentLifecycleState state_;
};

enum class WidgetLifecycle {
    Uninitialized = 0,
    StyleDirty,
    StyleClean,
    LayoutDirty,
    LayoutClean,
    PrePaintDirty,
    PrePaintClean,
    PaintDirty,
    PaintClean
};

struct PaintProperties {
    Vec2 translation{0.0f, 0.0f};
    float scale = 1.0f;
    Rect clipRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool hasClip = false;
    float opacity = 1.0f;
    int transformNodeId = 0;
    int clipNodeId = 0;
    int effectNodeId = 0;

    bool operator==(const PaintProperties& o) const {
        return translation.x == o.translation.x &&
               translation.y == o.translation.y &&
               scale == o.scale &&
               clipRect.x == o.clipRect.x &&
               clipRect.y == o.clipRect.y &&
               clipRect.w == o.clipRect.w &&
               clipRect.h == o.clipRect.h &&
               hasClip == o.hasClip &&
               opacity == o.opacity &&
               transformNodeId == o.transformNodeId &&
               clipNodeId == o.clipNodeId &&
               effectNodeId == o.effectNodeId;
    }
    bool operator!=(const PaintProperties& o) const {
        return !(*this == o);
    }
};

class Widget {
public:
    friend class LayoutObject;
    friend class LayoutBox;
    friend class LayoutBlock;
    friend class LayoutFlexibleBox;
    friend class LayoutGrid;
    friend class LayoutText;

    AtomicString id;
    AtomicString className;
    AtomicString type = "widget";
    AtomicString dir;
    Style style;
    ComputedStyle computedStyle;
    std::vector<CSSProperty> inlineProperties;
    Rect bounds;
    bool visible = true;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    // Disabled form-control state. Drives :disabled / :enabled matching and
    // (like Blink's IsDisabledFormControl) suppresses hover/active/focus.
    bool disabled = false;
    // Required form-control state. Drives :required / :optional and feeds the
    // basic :valid / :invalid validity check.
    bool required = false;
    bool lastFrameFocused = false;
    bool useGPUCompositing = false;
    float hoverAnim = 0;
    float hoverVelocity = 0;
    float renderScale = 1.0f;
    float scrollY = 0;
    float targetScrollY = 0;
    float scrollVelocity = 0;
    float contentHeight = 0;
    bool layoutDirty = true;
    bool styleDirty = true;
    bool subtreeStyleDirty = true;
    WidgetLifecycle lifecycleState = WidgetLifecycle::Uninitialized;
    PaintProperties paintProperties;
    PaintProperties lastPaintProperties;
    int transformNodeId = 0;
    int clipNodeId = 0;
    int effectNodeId = 0;
    Rect lastPaintBounds{0.0f, 0.0f, 0.0f, 0.0f};
    StyleCacheKey lastResolveKey;
    uint32_t lastStyleSheetEpoch = 0;
    bool hasLastResolveKey = false;
    uint32_t inlinePropertyEpoch = 0;
    uint32_t lastInlinePropertyEpoch = 0;
    size_t lastInlinePropertyCount = 0;
    Rect lastLayoutParentBounds;
    size_t layoutSignature = 0;
    mutable std::string cachedSelectorType;
    uint64_t ancestorH1 = 14695981039346656037ULL;
    uint64_t ancestorH2 = 5381ULL;
    bool scrollbarHovered = false;
    bool scrollbarDragging = false;
    float scrollbarDragOffset = 0;
    Widget* parent = nullptr;
    std::shared_ptr<std::pmr::memory_resource> childArena;
    std::vector<std::shared_ptr<Widget>> children;
    std::shared_ptr<Widget> beforePseudoNode;
    std::shared_ptr<Widget> afterPseudoNode;
    // Pseudo-element resolved styles (Blink PseudoId parity).
    // Lazily allocated only when the stylesheet has matching pseudo rules,
    // avoiding 3Ã—sizeof(Style) (~18KB) overhead per widget in the common case.
    std::unique_ptr<Style> placeholderStyle;   // ::placeholder
    std::unique_ptr<Style> selectionStyle;     // ::selection
    std::unique_ptr<Style> markerStyle;        // ::marker
    bool hasPlaceholderStyle = false;
    bool hasSelectionStyle   = false;
    bool hasMarkerStyle      = false;
    std::unique_ptr<LayoutObject> layoutObject;
    bool skipDOMChildrenPaint = false;
    int colspan = 1;
    int rowspan = 1;

    // ============================================================
    //  CSS @keyframes animation runtime (Blink Animation/KeyframeEffect parity)
    // ============================================================
    struct ActiveAnimation {
        std::string name;                       // resolved @keyframes rule name
        std::string keyframesOrigin;            // cache key (last @keyframes pointer tag)
        float startTime = 0.0f;                 // local time when started
        float currentTime = 0.0f;               // accumulated since start, in seconds
        float duration = 0.0f;                  // total animation duration (one iteration)
        float delay = 0.0f;
        float iterationCount = 1.0f;            // -1 = infinite
        AnimationDirection direction = AnimationDirection::Normal;
        AnimationFillMode fillMode = AnimationFillMode::None;
        AnimationPlayState playState = AnimationPlayState::Running;
        AnimationComposition composition = AnimationComposition::Replace;
        TimingFunction timingFunction = TimingFunction::ease();
        bool started = false;
        bool finished = false;                  // one-shot completion (no more iterations left)
        int currentIteration = 0;               // 0-based current iteration
        bool hasStartedFilling = false;         // for fill-mode forwards/both past finish
    };
    std::vector<ActiveAnimation> activeAnimations;
    float localClock = 0.0f;                    // monotonic seconds since first update
    bool firstUpdate = true;
    std::string lastAnimationSignature;        // cache: serialized form of last applied animationName list
    const StyleSheet* currentSheet = nullptr;   // non-owning, set by resolveStyles

    void attachLayoutTree();
    void detachLayoutTree();
    virtual std::unique_ptr<LayoutObject> createLayoutObject();
    std::function<void()> onClick;
    std::function<void()> onHover;
    const std::string& selectorType() const;
    Widget();
    virtual ~Widget();
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
    Widget* element(const std::string& tag,
                    const std::string& content = "",
                    const std::string& cls = "",
                    size_t reserve = 0);
    Text* text(const std::string& content, const std::string& cls = "");
    Button* button(const std::string& label = "",
                   const std::string& cls = "",
                   std::function<void()> onClick = {});
    TextInput* textInput(const std::string& placeholder = "",
                         const std::string& cls = "");
    TextInput* passwordInput(const std::string& placeholder = "",
                             const std::string& cls = "");
    Checkbox* checkbox(bool checked = false,
                       const std::string& cls = "",
                       std::function<void(bool)> onChange = {});
    Radio* radio(bool checked = false,
                 const std::string& group = "",
                 const std::string& cls = "",
                 std::function<void(bool)> onChange = {});
    RangeInput* range(float value = 0.5f,
                      float min = 0.0f,
                      float max = 1.0f,
                      float step = 0.01f,
                      const std::string& cls = "",
                      std::function<void(float)> onChange = {});
    Select* select(const std::string& cls = "",
                   std::function<void(size_t, const std::string&)> onChange = {});
    Option* option(const std::string& label,
                   const std::string& value = "",
                   const std::string& cls = "");
    Icon* addIcon(const std::string& glyph, const std::string& cls = "");
    Icon* icon(const std::string& glyph, const std::string& cls = "");
    Image* image(const std::string& source, const std::string& cls = "");
    Image* img(const std::string& source, const std::string& cls = "");
    Video* video(const std::string& source, const std::string& cls = "");
    ProgressBar* progress(float value,
                          const std::string& cls = "",
                          const Color& color = Color(0.42f, 0.36f, 0.91f, 1.0f));
    Placeholder* placeholder(const std::string& text = "Loading...", const std::string& cls = "");
    Placeholder* skeleton(const std::string& cls = "", size_t lines = 3);
    Canvas* canvas(const std::string& cls = "");
    VirtualList* virtualList(size_t itemCount,
                             float itemHeight,
                             std::function<void(Widget*, size_t)> itemBuilder,
                             const std::string& cls = "");
    LazyPanel* lazyPanel(std::function<void()> worker,
                         std::function<void(Widget*)> skeleton,
                         std::function<void(Widget*)> content);
    Panel* div(const std::string& cls = "", size_t reserve = 0);
    Panel* section(const std::string& cls = "", size_t reserve = 0);
    Panel* article(const std::string& cls = "", size_t reserve = 0);
    Panel* aside(const std::string& cls = "", size_t reserve = 0);
    Panel* header(const std::string& cls = "", size_t reserve = 0);
    Panel* footer(const std::string& cls = "", size_t reserve = 0);
    Panel* main(const std::string& cls = "", size_t reserve = 0);
    Panel* nav(const std::string& cls = "", size_t reserve = 0);
    Panel* body(const std::string& cls = "", size_t reserve = 0);
    Panel* form(const std::string& cls = "", size_t reserve = 0);
    Panel* fieldset(const std::string& cls = "", size_t reserve = 0);
    Panel* blockquote(const std::string& cls = "", size_t reserve = 0);
    Panel* ul(const std::string& cls = "", size_t reserve = 0);
    Panel* ol(const std::string& cls = "", size_t reserve = 0);
    Panel* li(const std::string& cls = "", size_t reserve = 0);
    Text* span(const std::string& content, const std::string& cls = "");
    Text* p(const std::string& content, const std::string& cls = "");
    Text* strong(const std::string& content, const std::string& cls = "");
    Text* b(const std::string& content, const std::string& cls = "");
    Text* small(const std::string& content, const std::string& cls = "");
    Text* label(const std::string& content, const std::string& cls = "");
    Text* legend(const std::string& content, const std::string& cls = "");
    Text* code(const std::string& content, const std::string& cls = "");
    Text* pre(const std::string& content, const std::string& cls = "");
    Text* h1(const std::string& content, const std::string& cls = "");
    Text* h2(const std::string& content, const std::string& cls = "");
    Text* h3(const std::string& content, const std::string& cls = "");
    Text* h4(const std::string& content, const std::string& cls = "");
    Text* h5(const std::string& content, const std::string& cls = "");
    Text* h6(const std::string& content, const std::string& cls = "");
    TextInput* input(const std::string& placeholder = "", const std::string& cls = "");
    TextInput* input(const std::string& type,
                     const std::string& placeholder,
                     const std::string& cls);
    TextInput* textarea(const std::string& placeholder = "", const std::string& cls = "");
    Anchor* anchor(const std::string& content, const std::string& href = "", const std::string& cls = "");
    Anchor* a(const std::string& content, const std::string& href = "", const std::string& cls = "");
    Details* details(const std::string& cls = "");
    Summary* summary(const std::string& content = "", const std::string& cls = "");
    Dialog* dialog(const std::string& cls = "");
    Meter* meter(float value = 0.0f, float min = 0.0f, float max = 1.0f, const std::string& cls = "");
    Progress* progressElement(float value = -1.0f, float max = 1.0f, const std::string& cls = "");
    Progress* htmlProgress(float value = -1.0f, float max = 1.0f, const std::string& cls = "");
    Widget* hr(const std::string& cls = "");
    Widget* br(const std::string& cls = "");
    Svg* svg(const std::string& cls = "");
    SvgG* svgG(const std::string& cls = "");
    SvgPath* svgPath(const std::string& d = "", const std::string& cls = "");
    SvgRect* svgRect(const std::string& cls = "");
    SvgCircle* svgCircle(const std::string& cls = "");
    SvgEllipse* svgEllipse(const std::string& cls = "");
    SvgLine* svgLine(const std::string& cls = "");
    SvgPolyline* svgPolyline(const std::string& cls = "");
    SvgPolygon* svgPolygon(const std::string& cls = "");
    Widget* setId(const std::string& value);
    Widget* classes(const std::string& value);
    Widget* addClass(const std::string& value);
    Widget* removeClass(const std::string& value);
    Widget* toggleClass(const std::string& value, bool enabled);
    Widget* css(const std::string& declarations);
    // Enable/disable this widget (drives :disabled / :enabled and makes the
    // widget inert to pointer/keyboard input). Marks style dirty so the
    // :disabled rules re-resolve immediately.
    Widget* setDisabled(bool value = true);
    bool isDisabled() const { return disabled; }
    // Mark this control as required (drives :required / :optional / :invalid).
    Widget* setRequired(bool value = true);
    bool isRequired() const { return required; }
    template<typename T = Widget>
    T* as() {
        return static_cast<T*>(this);
    }
    Widget* click(std::function<void()> callback) {
        onClick = std::move(callback);
        return this;
    }
    Widget* hover(std::function<void()> callback) {
        onHover = std::move(callback);
        return this;
    }
    struct EventListenerEntry {
        size_t id = 0;
        std::string type;
        DOMEventListener callback;
        bool useCapture = false;
    };
    std::vector<EventListenerEntry> domEventListeners;
    size_t nextDomListenerId = 1;
    size_t addEventListener(const std::string& type, DOMEventListener callback, bool useCapture = false);
    void removeEventListener(size_t listenerId);
    void dispatchEvent(Event& event);
    virtual void resolveStyles(const StyleSheet& sheet);
    void updateStyleAndLayout();
    void markLayoutDirty();
    void markStyleDirty();
    void markStyleDirtyRecursive();
    void markSubtreeStyleDirty();
    void invalidateStyleOnClassListChange(const std::string& oldClassName, const std::string& newClassName);
    void invalidateStyleOnIdChange(const std::string& oldId, const std::string& newId);
    virtual void layout(const Rect& parentBounds);
    virtual void prePaint(const PaintProperties& parentProps);
    void updatePaintProperties(const PaintProperties& parentProps);
    void invalidatePaintIfNeeded();
    void translateLayout(float dx, float dy);
    void checkFocusChanges();
    bool hasActiveAnimations() const;
    void resetTransientMotion();
    // Drive all @keyframes animation effects for this widget. Called from update().
    void tickAnimations(const InputState& input);
    // Cancel any active @keyframes overrides in the compositor (used when a widget
    // is removed from the tree or its animation list changes).
    void clearAnimationOverrides();
    // Apply a keyframe-defined property override to the local style without mutating
    // shared computedStyle. Returns true if the property was a known animatable prop.
    bool applyKeyframePropertyOverride(const std::string& name, const std::string& value);

    virtual void update(const InputState& input);
    virtual CursorType cursorAt(Vec2 point) const;
    virtual Widget* hitTest(Vec2 point, bool interactiveOnly = false);
    virtual void render(Renderer& renderer);
    virtual void setAttribute(const std::string& name, const std::string& value);
    bool isScrollableY() const;
    bool isClippingOverflow() const;
    bool getScrollBarRects(Rect& track, Rect& thumb) const;
protected:
    void layoutFlexChildren();
    void layoutPositionedChildren();
    void renderBackground(Renderer& renderer);
    void renderListMarker(Renderer& renderer);
    void renderChildren(Renderer& renderer);
    float maxScrollY() const;
    void clampScroll();
};
}