#pragma once
// FluxUI public API - inline Widget factory helpers + StatCard / LazyPanel.
// These need every concrete element type visible, so they live after the
// element declarations. Do not include directly, use <fluxui/widgets.h>.
#include "fluxui/detail/widgets/widget_elements.h"
namespace FluxUI {
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
inline Widget* Widget::setDisabled(bool value) {
    if (disabled == value) return this;
    disabled = value;
    // selectorType() encodes |disabled and is memoized — invalidate it so the
    // next resolve sees the new state (also done by resolveStyles, but callers
    // may query selectorType() directly, e.g. sibling/:disabled matching).
    cachedSelectorType.clear();
    if (value) {
        // Going inert: drop any active interactive state so :hover/:active/:focus
        // stop matching alongside the now-active :disabled.
        hovered = false;
        pressed = false;
        focused = false;
    }
    markStyleDirty();
    return this;
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