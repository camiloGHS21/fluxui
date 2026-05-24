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
#include <future>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <sstream>
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
class Checkbox;
class Radio;
class RangeInput;
class Select;
class Option;
class Icon;
class Image;
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
enum class VirtualListScrollStrategy {
    Start = 0,
    Center = 1,
    End = 2,
    Nearest = 3
};
class Widget {
public:
    std::string id;
    std::string className;
    std::string type = "widget";
    std::string dir;
    Style style;
    Style computedStyle;
    std::vector<CSSProperty> inlineProperties;
    Rect bounds;
    bool visible = true;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
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
    std::function<void()> onClick;
    std::function<void()> onHover;
    const std::string& selectorType() const;
    Widget() = default;
    virtual ~Widget() = default;
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
    Widget* setId(const std::string& value);
    Widget* classes(const std::string& value);
    Widget* addClass(const std::string& value);
    Widget* removeClass(const std::string& value);
    Widget* toggleClass(const std::string& value, bool enabled);
    Widget* css(const std::string& declarations);
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
    void markLayoutDirty();
    void markStyleDirty();
    void markStyleDirtyRecursive();
    void markSubtreeStyleDirty();
    void invalidateStyleOnClassListChange(const std::string& oldClassName, const std::string& newClassName);
    void invalidateStyleOnIdChange(const std::string& oldId, const std::string& newId);
    virtual void layout(const Rect& parentBounds);
    void translateLayout(float dx, float dy);
    bool hasActiveAnimations() const;
    void resetTransientMotion();
    virtual void update(const InputState& input);
    virtual CursorType cursorAt(Vec2 point) const;
    virtual Widget* hitTest(Vec2 point, bool interactiveOnly = false);
    virtual void render(Renderer& renderer);
protected:
    void layoutFlexChildren();
    void layoutPositionedChildren();
    void renderBackground(Renderer& renderer);
    void renderListMarker(Renderer& renderer);
    void renderChildren(Renderer& renderer);
    float maxScrollY() const;
    bool getScrollBarRects(Rect& track, Rect& thumb) const;
    void clampScroll();
};
class Panel : public Widget {
public:
    Panel() { type = "panel"; }
    Panel(const std::string& cls) { type = "panel"; className = cls; }
};
class Text : public Widget {
public:
    std::string content;
    Text() { type = "text"; }
    Text(const std::string& text, const std::string& cls = "")
        : content(text) { type = "text"; className = cls; }
    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
};
class Button : public Widget {
public:
    std::string label;
    std::string icon;
    Button() { type = "button"; }
    Button(const std::string& lbl, const std::string& cls = "")
        : label(lbl) { type = "button"; className = cls; }
    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
};
enum class TextInputType {
    Text,
    Password,
    Search,
    Email,
    Url,
    Tel,
    Number,
    Hidden,
    Button,
    Submit,
    Reset,
    File,
    Color,
    Date,
    Time,
    Month,
    Week,
    DateTimeLocal,
    Image
};
class TextInput : public Widget {
public:
    std::string value;
    std::string placeholder;
    TextInputType inputType = TextInputType::Text;
    TextInput() { type = "input"; }
    TextInput(const std::string& ph, const std::string& cls = "")
        : placeholder(ph) { type = "input"; className = cls; }
    TextInput* setInputType(TextInputType kind);
    TextInput* setInputType(const std::string& kind);
    bool isPassword() const { return inputType == TextInputType::Password; }
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
class Checkbox : public Widget {
public:
    bool checked = false;
    std::function<void(bool)> onChange;
    Checkbox() { type = "checkbox"; style.cursor = CursorType::Default; }
    Checkbox(bool isChecked, const std::string& cls = "")
        : checked(isChecked) { type = "checkbox"; className = cls; style.cursor = CursorType::Default; }
    void setChecked(bool value);
    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
};
class Radio : public Widget {
public:
    bool checked = false;
    std::string group;
    std::function<void(bool)> onChange;
    Radio() { type = "radio"; style.cursor = CursorType::Default; }
    Radio(bool isChecked, const std::string& groupName = "", const std::string& cls = "")
        : checked(isChecked), group(groupName) { type = "radio"; className = cls; style.cursor = CursorType::Default; }
    void setChecked(bool value);
    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
};
class RangeInput : public Widget {
public:
    float value = 0.5f;
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.01f;
    std::function<void(float)> onChange;
    RangeInput() { type = "range"; style.cursor = CursorType::Default; }
    RangeInput(float v, float minValue, float maxValue, float stepValue, const std::string& cls = "")
        : value(v), min(minValue), max(maxValue), step(stepValue) {
        type = "range";
        className = cls;
        style.cursor = CursorType::Default;
    }
    void setValue(float newValue, bool notify = true);
    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
private:
    bool dragging_ = false;
};
class Option : public Widget {
public:
    std::string label;
    std::string value;
    Option() { type = "option"; }
    Option(const std::string& optionLabel, const std::string& optionValue = "", const std::string& cls = "")
        : label(optionLabel), value(optionValue.empty() ? optionLabel : optionValue) {
        type = "option";
        className = cls;
    }
    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
};
class Select : public Widget {
public:
    size_t selectedIndex = 0;
    bool expanded = false;
    std::function<void(size_t, const std::string&)> onChange;
    Select() { type = "select"; style.cursor = CursorType::Default; }
    Select(const std::string& cls) { type = "select"; className = cls; style.cursor = CursorType::Default; }
    void selectIndex(size_t index, bool notify = true);
    std::string selectedLabel() const;
    std::string selectedValue() const;
    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
};
class Icon : public Widget {
public:
    std::string glyph;
    std::string fontName = "icons";
    Icon() { type = "icon"; }
    Icon(const std::string& g, const std::string& cls = "")
        : glyph(g) { type = "icon"; className = cls; }
    void render(Renderer& renderer) override;
};
enum class ImageWidgetState {
    Idle,
    Loading,
    Complete,
    Error
};
class Image : public Widget {
public:
    std::string source;
    std::string srcset;
    std::string sizes;
    std::string currentSrc;
    std::string alt;
    std::string crossOrigin;
    Vec2 naturalSize = {0, 0};
    ImageWidgetState loadState = ImageWidgetState::Idle;
    float devicePixelRatio = 1.0f;
    float intrinsicDensity = 1.0f;
    bool isGeneratedContent = false;
    bool lazyLoad = false;
    bool decoding_async = false;
    std::function<void()> onLoad;
    std::function<void()> onError;
    Image() { type = "img"; }
    Image(const std::string& src, const std::string& cls = "")
        : source(src), currentSrc(src) { type = "img"; className = cls; }
    void setSrc(const std::string& newSource) {
        if (source != newSource) {
            source = newSource;
            updateCurrentSrc();
        }
    }
    void setSrcset(const std::string& newSrcset) {
        if (srcset != newSrcset) {
            srcset = newSrcset;
            updateCurrentSrc();
        }
    }
    void updateCurrentSrc();
    Vec2 getNaturalSize() const {
        return { naturalSize.x / intrinsicDensity, naturalSize.y / intrinsicDensity };
    }
    bool isLoaded() const { return loadState == ImageWidgetState::Complete; }
    bool hasError() const { return loadState == ImageWidgetState::Error; }
    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
};
class ProgressBar : public Widget {
public:
    float progress = 0;
    Color barColor = Color::fromHex("#6C5CE7");
    ProgressBar() { type = "progress"; }
    void render(Renderer& renderer) override;
};
class Placeholder : public Widget {
public:
    std::string text = "Loading...";
    bool shimmer = false;
    float shimmerProgress = 0;
    Placeholder() { type = "placeholder"; }
    Placeholder(const std::string& t, const std::string& cls = "")
        : text(t) { type = "placeholder"; className = cls; }
    void render(Renderer& renderer) override;
    void update(const InputState& input) override;
};
class Canvas : public Widget {
public:
    std::function<void(Renderer&, const Rect&)> onDraw;
    Canvas() { type = "canvas"; }
    Canvas(const std::string& cls) { type = "canvas"; className = cls; }
    void render(Renderer& renderer) override;
};
class VirtualList : public Widget {
public:
    using ItemBuilder = std::function<void(Widget*, size_t)>;

    size_t itemCount = 0;
    float itemHeight = 32.0f;
    float overdraw = 96.0f;
    std::string itemClassName = "virtual-list-item";
    ItemBuilder itemBuilder;

    VirtualList() { type = "virtual-list"; }
    VirtualList(size_t count,
                float rowHeight,
                ItemBuilder builder,
                const std::string& cls = "")
        : itemCount(count),
          itemHeight(rowHeight),
          itemBuilder(std::move(builder)) {
        type = "virtual-list";
        className = cls;
        style.overflowY = Overflow::Auto;
        style.display = Display::Block;
    }

    void setItemCount(size_t count);
    void refresh();
    void scrollToIndex(size_t index,
                       VirtualListScrollStrategy strategy = VirtualListScrollStrategy::Nearest);
    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;

private:
    size_t visibleStart_ = 0;
    size_t visibleEnd_ = 0;
    float lastBuildWidth_ = -1.0f;
    float lastBuildItemHeight_ = -1.0f;
    bool forceRebuild_ = true;
    void rebuildVisibleItems();
};
class Anchor : public Text {
public:
    std::string href;
    Anchor() { type = "a"; style.cursor = CursorType::Pointer; }
    Anchor(const std::string& txt, const std::string& hrefUrl = "", const std::string& cls = "")
        : Text(txt, cls), href(hrefUrl) {
        type = "a";
        style.cursor = CursorType::Pointer;
    }
    void update(const InputState& input) override;
};
class Details : public Widget {
public:
    bool open = false;
    Details() { type = "details"; }
    Details(bool isOpen, const std::string& cls = "") : open(isOpen) {
        type = "details";
        className = cls;
    }
    void layout(const Rect& parentBounds) override;
};
class Summary : public Text {
public:
    Summary() { type = "summary"; style.cursor = CursorType::Pointer; }
    Summary(const std::string& text, const std::string& cls = "")
        : Text(text, cls) {
        type = "summary";
        style.cursor = CursorType::Pointer;
    }
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
};
class Dialog : public Widget {
public:
    bool open = false;
    bool modal = false;
    Dialog() { type = "dialog"; }
    Dialog(const std::string& cls) { type = "dialog"; className = cls; }
    void show();
    void showModal();
    void close();
    void resolveStyles(const StyleSheet& sheet) override;
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
};
class Meter : public Widget {
public:
    float value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;
    float low = 0.0f;
    float high = 1.0f;
    float optimum = 0.0f;
    Meter() { type = "meter"; }
    Meter(float val, float minValue, float maxValue, const std::string& cls = "")
        : value(val), min(minValue), max(maxValue) {
        type = "meter";
        className = cls;
        low = minValue;
        high = maxValue;
        optimum = (minValue + maxValue) * 0.5f;
    }
    void render(Renderer& renderer) override;
};
class Progress : public Widget {
public:
    float value = -1.0f; // negative value represents indeterminate state
    float max = 1.0f;
    Progress() { type = "progress"; }
    Progress(float val, float maxVal, const std::string& cls = "")
        : value(val), max(maxVal) {
        type = "progress";
        className = cls;
    }
    void render(Renderer& renderer) override;
};
class Hr : public Widget {
public:
    Hr() { type = "hr"; }
    Hr(const std::string& cls) { type = "hr"; className = cls; }
};
class Br : public Widget {
public:
    Br() {
        type = "br";
        style.display = Display::Block;
        style.width = CSSValue::pct(100.0f);
        style.height = CSSValue::px(0.0f);
    }
    Br(const std::string& cls) : Br() {
        className = cls;
    }
};
inline Panel* Widget::panel(const std::string& cls, size_t reserve) {
    auto* widget = add<Panel>(cls);
    if (reserve > 0) {
        widget->reserveChildren(reserve);
    }
    return widget;
}
inline Widget* Widget::element(const std::string& tag,
                               const std::string& content,
                               const std::string& cls,
                               size_t reserve) {
    std::string lower = tag;
    for (char& c : lower) {
        c = (char)std::tolower((unsigned char)c);
    }
    if (lower.empty()) lower = "div";

    if (lower == "button") return button(content, cls);
    if (lower == "input") return input(content, cls);
    if (lower == "textarea") return textarea(content, cls);
    if (lower == "select") return select(cls);
    if (lower == "option") return option(content, "", cls);
    if (lower == "a") return anchor(content, "", cls);
    if (lower == "img" || lower == "image") return image(content, cls);
    if (lower == "canvas") return canvas(cls);
    if (lower == "details") return details(cls);
    if (lower == "summary") return summary(content, cls);
    if (lower == "dialog") return dialog(cls);
    if (lower == "meter") return meter(0.0f, 0.0f, 1.0f, cls);
    if (lower == "progress") return progressElement(-1.0f, 1.0f, cls);
    if (lower == "hr") return hr(cls);
    if (lower == "br" || lower == "wbr") return br(cls);
    if (lower == "checkbox") return checkbox(false, cls);
    if (lower == "radio") return radio(false, "", cls);
    if (lower == "range") return range(0.5f, 0.0f, 1.0f, 0.01f, cls);

    if (!content.empty()) {
        auto* widget = text(content, cls);
        widget->type = lower;
        return widget;
    }

    auto* widget = panel(cls, reserve);
    widget->type = lower;
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
inline TextInput* Widget::passwordInput(const std::string& placeholder, const std::string& cls) {
    auto* widget = textInput(placeholder, cls);
    widget->setInputType(TextInputType::Password);
    return widget;
}
inline Checkbox* Widget::checkbox(bool checked,
                                  const std::string& cls,
                                  std::function<void(bool)> onChange) {
    auto* widget = add<Checkbox>(checked, cls);
    if (onChange) widget->onChange = std::move(onChange);
    return widget;
}
inline Radio* Widget::radio(bool checked,
                            const std::string& group,
                            const std::string& cls,
                            std::function<void(bool)> onChange) {
    auto* widget = add<Radio>(checked, group, cls);
    if (onChange) widget->onChange = std::move(onChange);
    return widget;
}
inline RangeInput* Widget::range(float value,
                                 float min,
                                 float max,
                                 float step,
                                 const std::string& cls,
                                 std::function<void(float)> onChange) {
    auto* widget = add<RangeInput>(value, min, max, step, cls);
    if (onChange) widget->onChange = std::move(onChange);
    return widget;
}
inline Select* Widget::select(const std::string& cls,
                              std::function<void(size_t, const std::string&)> onChange) {
    auto* widget = add<Select>(cls);
    if (onChange) widget->onChange = std::move(onChange);
    return widget;
}
inline Option* Widget::option(const std::string& label,
                              const std::string& value,
                              const std::string& cls) {
    return add<Option>(label, value, cls);
}
inline Icon* Widget::addIcon(const std::string& glyph, const std::string& cls) {
    return add<Icon>(glyph, cls);
}
inline Icon* Widget::icon(const std::string& glyph, const std::string& cls) {
    return addIcon(glyph, cls);
}
inline Image* Widget::image(const std::string& source, const std::string& cls) {
    return add<Image>(source, cls);
}
inline Image* Widget::img(const std::string& source, const std::string& cls) {
    return image(source, cls);
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
inline Canvas* Widget::canvas(const std::string& cls) {
    return add<Canvas>(cls);
}
inline VirtualList* Widget::virtualList(size_t itemCount,
                                        float itemHeight,
                                        std::function<void(Widget*, size_t)> itemBuilder,
                                        const std::string& cls) {
    return add<VirtualList>(itemCount, itemHeight, std::move(itemBuilder), cls);
}
inline LazyPanel* Widget::lazyPanel(std::function<void()> worker,
                                    std::function<void(Widget*)> skeleton,
                                    std::function<void(Widget*)> content) {
    return add<LazyPanel>(std::move(worker), std::move(skeleton), std::move(content));
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
inline Panel* Widget::body(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "body";
    return widget;
}
inline Panel* Widget::form(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "form";
    return widget;
}
inline Panel* Widget::fieldset(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "fieldset";
    return widget;
}
inline Panel* Widget::blockquote(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "blockquote";
    return widget;
}
inline Panel* Widget::ul(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "ul";
    return widget;
}
inline Panel* Widget::ol(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "ol";
    return widget;
}
inline Panel* Widget::li(const std::string& cls, size_t reserve) {
    auto* widget = panel(cls, reserve);
    widget->type = "li";
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
inline Text* Widget::strong(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "strong";
    return widget;
}
inline Text* Widget::b(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "b";
    return widget;
}
inline Text* Widget::small(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "small";
    return widget;
}
inline Text* Widget::label(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "label";
    return widget;
}
inline Text* Widget::legend(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "legend";
    return widget;
}
inline Text* Widget::code(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "code";
    return widget;
}
inline Text* Widget::pre(const std::string& content, const std::string& cls) {
    auto* widget = text(content, cls);
    widget->type = "pre";
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
inline TextInput* Widget::input(const std::string& type,
                                const std::string& placeholder,
                                const std::string& cls) {
    auto* widget = textInput(placeholder, cls);
    widget->setInputType(type);
    return widget;
}
inline TextInput* Widget::textarea(const std::string& placeholder, const std::string& cls) {
    auto* widget = textInput(placeholder, cls);
    widget->type = "textarea";
    return widget;
}
inline Anchor* Widget::anchor(const std::string& content, const std::string& href, const std::string& cls) {
    return add<Anchor>(content, href, cls);
}
inline Anchor* Widget::a(const std::string& content, const std::string& href, const std::string& cls) {
    return anchor(content, href, cls);
}
inline Details* Widget::details(const std::string& cls) {
    return add<Details>(false, cls);
}
inline Summary* Widget::summary(const std::string& content, const std::string& cls) {
    return add<Summary>(content, cls);
}
inline Dialog* Widget::dialog(const std::string& cls) {
    return add<Dialog>(cls);
}
inline Meter* Widget::meter(float value, float min, float max, const std::string& cls) {
    return add<Meter>(value, min, max, cls);
}
inline Progress* Widget::progressElement(float value, float max, const std::string& cls) {
    return add<Progress>(value, max, cls);
}
inline Progress* Widget::htmlProgress(float value, float max, const std::string& cls) {
    return progressElement(value, max, cls);
}
inline Widget* Widget::hr(const std::string& cls) {
    return add<Hr>(cls);
}
inline Widget* Widget::br(const std::string& cls) {
    return add<Br>(cls);
}
inline Widget* Widget::setId(const std::string& value) {
    std::string oldId = id;
    id = value;
    invalidateStyleOnIdChange(oldId, id);
    return this;
}
inline Widget* Widget::classes(const std::string& value) {
    std::string oldClassName = className;
    className = value;
    invalidateStyleOnClassListChange(oldClassName, className);
    return this;
}
inline Widget* Widget::addClass(const std::string& value) {
    if (value.empty()) return this;
    std::istringstream stream(className);
    std::string cls;
    while (stream >> cls) {
        if (cls == value) return this;
    }
    std::string oldClassName = className;
    if (!className.empty()) className += ' ';
    className += value;
    invalidateStyleOnClassListChange(oldClassName, className);
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
        std::string oldClassName = className;
        className = std::move(updated);
        invalidateStyleOnClassListChange(oldClassName, className);
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
class LazyPanel : public Widget {
public:
    std::function<void()> worker;
    std::function<void(Widget*)> skeletonBuilder;
    std::function<void(Widget*)> contentBuilder;
    std::atomic<bool> loaded{false};
    bool initialized = false;
    bool lastLoadedState = false;
    LazyPanel() { type = "lazy-panel"; }
    LazyPanel(std::function<void()> workerFunc,
              std::function<void(Widget*)> skeleton,
              std::function<void(Widget*)> content)
        : worker(std::move(workerFunc)), skeletonBuilder(std::move(skeleton)), contentBuilder(std::move(content)) {
        type = "lazy-panel";
        std::thread([this]() {
            if (worker) worker();
            loaded = true;
        }).detach();
    }
    void update(const InputState& input) override;
};
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
    using ActionCallback = std::function<void(Application&, const std::string&)>;
    static Application* instance();
    bool init(const std::string& title, int width, int height);
    bool init(const std::string& title, int width, int height, RenderBackendType backend);
    void run();
    void shutdown();
    void setBackend(RenderBackendType backend);
    RenderBackendType backendPreference() const { return backendPreference_; }
    RenderBackendType activeBackend() const { return renderer_.activeBackend(); }
    const char* activeBackendName() const { return renderer_.activeBackendName(); }
    bool loadStylesheet(const std::string& path);
    void addStylesheet(const std::string& css);
    Widget* root() { return root_.get(); }
    Renderer& renderer() { return renderer_; }
    StyleSheet& stylesheet() { return stylesheet_; }
    InputState& input() { return input_; }
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
    bool running = true;
    template<typename T>
    std::future<T> async(std::function<T()> task) {
        return std::async(std::launch::async, std::move(task));
    }
    void lazyLoad(std::function<void()> loader, std::function<void()> onComplete = nullptr);
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
    void processEvents();
    void updateCursor(CursorType cursor);
};
}
