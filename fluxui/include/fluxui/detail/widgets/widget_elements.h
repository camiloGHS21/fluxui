#pragma once
// FluxUI public API - concrete widget element classes (Panel/Text/Button/...).
// Auto-split from widgets.h; do not include directly, use <fluxui/widgets.h>.
#include "fluxui/detail/widgets/widget_base.h"
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
}