#pragma once
// FluxUI public API - concrete widget elements + inline Widget factory methods.
// Auto-split from widgets.h; do not include directly, use <fluxui/widgets.h>.
#include "fluxui/detail/widget_base.h"
namespace FluxUI {
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
class TextArea : public Widget {
public:
    std::string value;
    std::string placeholder;
    int rows = 3;
    int cols = 20;
    bool wrap = true;

    TextArea() { type = "textarea"; style.overflow = Overflow::Auto; }
    TextArea(const std::string& ph, const std::string& cls = "")
        : placeholder(ph) { type = "textarea"; className = cls; style.overflow = Overflow::Auto; }

    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    CursorType cursorAt(Vec2 point) const override;
    void render(Renderer& renderer) override;

private:
    size_t caretIndex_ = 0;
    size_t selectionAnchor_ = 0;
    size_t selectionFocus_ = 0;
    bool selecting_ = false;
    float scrollX_ = 0;
    float focusAnim_ = 0;
    float caretBlinkTime_ = 0;

    bool resizing_ = false;
    Vec2 resizeStartMousePos_ = {0.0f, 0.0f};
    Vec2 resizeStartSize_ = {0.0f, 0.0f};
    bool isOverResizeHandle(Vec2 point) const;

    struct LineInfo {
        size_t start;
        size_t end;
        float width;
    };
    std::vector<LineInfo> layoutLines(float fontSize, float maxWidth, const std::string& fontName = "default") const;
    void getLineAndColumnOfOffset(const std::vector<LineInfo>& lines, size_t offset, size_t& outLine, size_t& outCol) const;
    
    bool hasSelection() const;
    size_t selectionStart() const;
    size_t selectionEnd() const;
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
class Video : public Widget {
public:
    enum NetworkState {
        NETWORK_EMPTY = 0,
        NETWORK_IDLE = 1,
        NETWORK_LOADING = 2,
        NETWORK_NO_SOURCE = 3
    };
    enum ReadyState {
        HAVE_NOTHING = 0,
        HAVE_METADATA = 1,
        HAVE_CURRENT_DATA = 2,
        HAVE_FUTURE_DATA = 3,
        HAVE_ENOUGH_DATA = 4
    };

    std::string source;
    bool autoplay = false;
    bool controls = true;
    bool loop = false;
    bool muted = false;
    float volume = 1.0f;
    float currentTime = 0.0f;
    float duration = 60.0f;
    float playbackRate = 1.0f;
    bool paused = true;
    
    NetworkState networkState = NETWORK_EMPTY;
    ReadyState readyState = HAVE_NOTHING;
    bool seeking = false;

    // Callbacks
    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onTimeUpdate;
    std::function<void()> onEnded;
    std::function<void()> onSeeking;
    std::function<void()> onSeeked;
    std::function<void()> onVolumeChange;

    Video();
    virtual ~Video() override;

    void play();
    void pause();
    void setMuted(bool m);
    void setVolume(float v);
    void setCurrentTime(float t);
    
    void layout(const Rect& parentBounds) override;
    void update(const InputState& input) override;
    void render(Renderer& renderer) override;
    void setAttribute(const std::string& name, const std::string& value) override;

private:
    std::chrono::high_resolution_clock::time_point lastUpdateTime_;
    bool hasInitializedAudio_ = false;
    bool audioThreadRunning_ = false;
    std::thread audioThread_;
    std::mutex audioMutex_;
    
    // Internal interactive control state
    bool playButtonHovered_ = false;
    bool volumeButtonHovered_ = false;
    bool progressBarHovered_ = false;
    bool volumeSliderHovered_ = false;
    bool draggingProgress_ = false;
    bool draggingVolume_ = false;
    float controlsTimer_ = 0.0f; // fade out controls if mouse inactive

    void startAudioThread();
    void stopAudioThread();
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
class OffscreenCanvas {
public:
    int width = 0;
    int height = 0;
    std::vector<RenderCommand> commands;
    uint32_t fbo = 0;
    uint32_t textureId = 0;
    bool hardwareInitialized = false;

    OffscreenCanvas(int w, int h) : width(w), height(h) {}
    ~OffscreenCanvas() {}

    void startPaint() {
        commands.clear();
    }

    void drawRoundedRect(const Rect& rect, const Color& color, const BorderRadius& radius) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::RoundedRect;
        cmd.rect = rect;
        cmd.color = color;
        cmd.radius = radius;
        cmd.opacity = 1.0f;
        cmd.hasGradient = false;
        commands.push_back(cmd);
    }

    void drawText(const std::string& text, const Vec2& pos, const Color& color, float fontSize) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Text;
        cmd.rect = {pos.x, pos.y, 0, 0};
        cmd.text = text;
        cmd.color = color;
        cmd.fontSize = fontSize;
        commands.push_back(cmd);
    }

    void drawImage(const std::string& key, const Rect& rect) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::TexturedQuad;
        cmd.rect = rect;
        cmd.text = key;
        cmd.opacity = 1.0f;
        commands.push_back(cmd);
    }

    void drawTo(Renderer& renderer, const Rect& targetRect) {
        renderer.pushTranslation({targetRect.x, targetRect.y});
        float sx = width > 0 ? targetRect.w / (float)width : 1.0f;
        float sy = height > 0 ? targetRect.h / (float)height : 1.0f;
        renderer.pushScale(sx, {0, 0});
        
        renderer.playback(commands);
        
        renderer.popScale();
        renderer.popTranslation();
    }
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
    void setAttribute(const std::string& name, const std::string& value) override;
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
    void layout(const Rect& parentBounds) override;
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

class SvgElement : public Widget {
public:
    std::string fill;
    std::string stroke;
    std::string strokeWidth;
    std::string transformAttr;
    std::string opacityAttr;
    std::string fillOpacity;
    std::string strokeOpacity;
    std::string fillRuleAttr;

    SvgElement() { type = "svg-element"; }
    virtual ~SvgElement() override = default;

    void markSvgDirty();
    void setAttribute(const std::string& name, const std::string& value) override;
};

class SvgG : public SvgElement {
public:
    SvgG() { type = "g"; }
    SvgG(const std::string& cls) { type = "g"; className = cls; }
};

class SvgPath : public SvgElement {
public:
    std::string d;
    SvgPath() { type = "path"; }
    SvgPath(const std::string& pathD, const std::string& cls = "") : d(pathD) { type = "path"; className = cls; }
};

class SvgRect : public SvgElement {
public:
    std::string x = "0";
    std::string y = "0";
    std::string width = "0";
    std::string height = "0";
    std::string rx = "0";
    std::string ry = "0";
    SvgRect() { type = "rect"; }
};

class SvgCircle : public SvgElement {
public:
    std::string cx = "0";
    std::string cy = "0";
    std::string r = "0";
    SvgCircle() { type = "circle"; }
};

class SvgEllipse : public SvgElement {
public:
    std::string cx = "0";
    std::string cy = "0";
    std::string rx = "0";
    std::string ry = "0";
    SvgEllipse() { type = "ellipse"; }
};

class SvgLine : public SvgElement {
public:
    std::string x1 = "0";
    std::string y1 = "0";
    std::string x2 = "0";
    std::string y2 = "0";
    SvgLine() { type = "line"; }
};

class SvgPolyline : public SvgElement {
public:
    std::string points;
    SvgPolyline() { type = "polyline"; }
};

class SvgPolygon : public SvgElement {
public:
    std::string points;
    SvgPolygon() { type = "polygon"; }
};

class Svg : public Widget {
public:
    std::string viewBox;
    std::string width;
    std::string height;
    std::string preserveAspectRatio = "xMidYMid meet";

    bool isRasterDirty = true;
    ImageData cachedImage;
    std::string loadedTextureKey;

    Svg() { type = "svg"; }
    Svg(const std::string& cls) { type = "svg"; className = cls; }
    ~Svg() override;

    void layout(const Rect& parentBounds) override;
    void render(Renderer& renderer) override;
    void setAttribute(const std::string& name, const std::string& value) override;
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
    if (lower == "video") return video(content, cls);
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
    if (lower == "icon") return addIcon(content, cls);
    if (lower == "svg") return svg(cls);
    if (lower == "g") return svgG(cls);
    if (lower == "path") return svgPath("", cls);
    if (lower == "rect") return svgRect(cls);
    if (lower == "circle") return svgCircle(cls);
    if (lower == "ellipse") return svgEllipse(cls);
    if (lower == "line") return svgLine(cls);
    if (lower == "polyline") return svgPolyline(cls);
    if (lower == "polygon") return svgPolygon(cls);

    // Headline Elements
    if (lower == "h1") return h1(content, cls);
    if (lower == "h2") return h2(content, cls);
    if (lower == "h3") return h3(content, cls);
    if (lower == "h4") return h4(content, cls);
    if (lower == "h5") return h5(content, cls);
    if (lower == "h6") return h6(content, cls);

    // Common Text Elements
    if (lower == "p") return p(content, cls);
    if (lower == "span") return span(content, cls);
    if (lower == "strong") return strong(content, cls);
    if (lower == "b") return b(content, cls);
    if (lower == "small") return small(content, cls);
    if (lower == "label") return label(content, cls);
    if (lower == "legend") return legend(content, cls);
    if (lower == "code") return code(content, cls);
    if (lower == "pre") return pre(content, cls);

    // Other Inline Text Semantics tags mapped to Text widget
    if (lower == "i" || lower == "em" || lower == "u" || lower == "ins" ||
        lower == "s" || lower == "strike" || lower == "del" || lower == "mark" ||
        lower == "big" || lower == "sub" || lower == "sup" ||
        lower == "kbd" || lower == "samp" || lower == "tt" ||
        lower == "cite" || lower == "var" || lower == "dfn" || lower == "abbr" ||
        lower == "acronym" || lower == "q" || lower == "bdi" || lower == "bdo" ||
        lower == "data" || lower == "time" || lower == "nobr" || lower == "rt" ||
        lower == "rp" || lower == "address" || lower == "figcaption" ||
        lower == "dt" || lower == "dd" || lower == "td" || lower == "th" ||
        lower == "caption") {
        auto* widget = text(content, cls);
        widget->type = lower;
        return widget;
    }

    if (!content.empty()) {
        auto* widget = text(content, cls);
        widget->type = lower;
        return widget;
    }

    // Structural/Container HTML elements mapped to Panel widget
    if (lower == "div") return div(cls, reserve);
    if (lower == "section") return section(cls, reserve);
    if (lower == "article") return article(cls, reserve);
    if (lower == "aside") return aside(cls, reserve);
    if (lower == "header") return header(cls, reserve);
    if (lower == "footer") return footer(cls, reserve);
    if (lower == "main") return main(cls, reserve);
    if (lower == "nav") return nav(cls, reserve);
    if (lower == "body") return body(cls, reserve);
    if (lower == "form") return form(cls, reserve);
    if (lower == "fieldset") return fieldset(cls, reserve);
    if (lower == "blockquote") return blockquote(cls, reserve);
    if (lower == "ul") return ul(cls, reserve);
    if (lower == "ol") return ol(cls, reserve);
    if (lower == "li") return li(cls, reserve);

    // Extra structural/table tags
    if (lower == "figure" || lower == "hgroup" || lower == "search" ||
        lower == "menu" || lower == "dir" || lower == "center" ||
        lower == "table" || lower == "thead" || lower == "tbody" ||
        lower == "tfoot" || lower == "tr" || lower == "col" ||
        lower == "colgroup") {
        auto* widget = panel(cls, reserve);
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
inline Video* Widget::video(const std::string& source, const std::string& cls) {
    auto* widget = add<Video>();
    widget->source = source;
    widget->className = cls;
    return widget;
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
    auto* widget = panel(cls, reserve);
    widget->type = "div";
    return widget;
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
inline Svg* Widget::svg(const std::string& cls) {
    return add<Svg>(cls);
}
inline SvgG* Widget::svgG(const std::string& cls) {
    return add<SvgG>(cls);
}
inline SvgPath* Widget::svgPath(const std::string& d, const std::string& cls) {
    return add<SvgPath>(d, cls);
}
inline SvgRect* Widget::svgRect(const std::string& cls) {
    auto* w = add<SvgRect>();
    w->className = cls;
    return w;
}
inline SvgCircle* Widget::svgCircle(const std::string& cls) {
    auto* w = add<SvgCircle>();
    w->className = cls;
    return w;
}
inline SvgEllipse* Widget::svgEllipse(const std::string& cls) {
    auto* w = add<SvgEllipse>();
    w->className = cls;
    return w;
}
inline SvgLine* Widget::svgLine(const std::string& cls) {
    auto* w = add<SvgLine>();
    w->className = cls;
    return w;
}
inline SvgPolyline* Widget::svgPolyline(const std::string& cls) {
    auto* w = add<SvgPolyline>();
    w->className = cls;
    return w;
}
inline SvgPolygon* Widget::svgPolygon(const std::string& cls) {
    auto* w = add<SvgPolygon>();
    w->className = cls;
    return w;
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
    std::istringstream stream(className.getString());
    std::string cls;
    while (stream >> cls) {
        if (cls == value) return this;
    }
    std::string oldClassName = className.getString();
    std::string newClassName = className.getString();
    if (!newClassName.empty()) newClassName += ' ';
    newClassName += value;
    className = newClassName;
    invalidateStyleOnClassListChange(oldClassName, className.getString());
    return this;
}
inline Widget* Widget::removeClass(const std::string& value) {
    if (value.empty() || className.empty()) return this;
    std::istringstream stream(className.getString());
    std::string next;
    std::string updated;
    while (stream >> next) {
        if (next == value) continue;
        if (!updated.empty()) updated += ' ';
        updated += next;
    }
    if (updated != className.getString()) {
        std::string oldClassName = className.getString();
        className = std::move(updated);
        invalidateStyleOnClassListChange(oldClassName, className.getString());
    }
    return this;
}
inline Widget* Widget::toggleClass(const std::string& value, bool enabled) {
    return enabled ? addClass(value) : removeClass(value);
}
inline Widget* Widget::css(const std::string& declarations) {
    inlineProperties.clear();
    ++inlinePropertyEpoch;

    auto trimSV = [](std::string_view s) -> std::string_view {
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string_view::npos) return {};
        size_t end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    };

    std::string_view decls = declarations;
    size_t start = 0;
    int depth = 0;
    char quote = 0;
    bool escaped = false;
    for (size_t i = 0; i <= decls.size(); ++i) {
        char c = (i < decls.size()) ? decls[i] : ';';
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

        if ((c == ';' && depth == 0) || i == decls.size()) {
            std::string_view decl = trimSV(decls.substr(start, i - start));
            start = i + 1;
            if (decl.empty()) continue;

            auto colon = decl.find(':');
            if (colon == std::string_view::npos) continue;

            std::string_view nameView = trimSV(decl.substr(0, colon));
            std::string_view valueView = trimSV(decl.substr(colon + 1));

            char localNameBuf[64];
            size_t nameLen = std::min(nameView.size(), sizeof(localNameBuf) - 1);
            for (size_t k = 0; k < nameLen; ++k) {
                localNameBuf[k] = (char)std::tolower((unsigned char)nameView[k]);
            }
            localNameBuf[nameLen] = '\0';
            std::string_view lowName(localNameBuf, nameLen);

            std::string_view cleanValue = valueView;
            if (valueView.size() > 10) {
                size_t bang = valueView.rfind('!');
                if (bang != std::string_view::npos) {
                    std::string_view tail = trimSV(valueView.substr(bang + 1));
                    if (tail.size() == 9) {
                        char importantBuf[10];
                        for (int k = 0; k < 9; ++k) importantBuf[k] = (char)std::tolower((unsigned char)tail[k]);
                        importantBuf[9] = '\0';
                        if (std::string_view(importantBuf, 9) == "important") {
                            cleanValue = trimSV(valueView.substr(0, bang));
                        }
                    }
                }
            }

            inlineProperties.push_back({AtomicString(lowName), std::string(cleanValue), 0});
        }
    }
    markStyleDirtyRecursive();
    return this;
}
inline void Widget::setAttribute(const std::string& name, const std::string& value) {
    if (name == "id") {
        setId(value);
    } else if (name == "class") {
        classes(value);
    } else if (name == "style") {
        css(value);
    } else if (name == "colspan") {
        try { colspan = std::stoi(value); } catch (...) { colspan = 1; }
        markLayoutDirty();
    } else if (name == "rowspan") {
        try { rowspan = std::stoi(value); } catch (...) { rowspan = 1; }
        markLayoutDirty();
    }
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
}