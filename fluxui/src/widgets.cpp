#include "fluxui/widgets.h"
#include "fluxui/compositor.h"
#include "fluxui/layout.h"
#include "fluxui/layout_object.h"
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <thread>
#include <future>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <type_traits>
#include <stb_image.h>
#include "fluxui/platform.h"
#include <nlohmann/json.hpp>
namespace FluxUI {

class ThreadPool {
public:
    ThreadPool(size_t threads) : stop(false) {
        for(size_t i = 0; i<threads; ++i)
            workers.emplace_back(
                [this] {
                    for(;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty(); });
                            if(this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                }
            );
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            if(worker.joinable())
                worker.join();
    }
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

static ThreadPool& getLayoutThreadPool() {
    static ThreadPool pool(std::max(1u, std::thread::hardware_concurrency()));
    return pool;
}
static Application* g_activeApp = nullptr;
Application* Application::instance() {
    return g_activeApp;
}

void Application::registerResizeObserver(ResizeObserver* observer) {
    if (!observer) return;
    for (auto* obs : resizeObservers_) {
        if (obs == observer) return;
    }
    resizeObservers_.push_back(observer);
}

void Application::unregisterResizeObserver(ResizeObserver* observer) {
    resizeObservers_.erase(
        std::remove(resizeObservers_.begin(), resizeObservers_.end(), observer),
        resizeObservers_.end()
    );
}
void Application::onWidgetDestroyed(Widget* w) {
    if (!w) return;
    for (auto* obs : resizeObservers_) {
        obs->unobserve(w);
    }
}

ResizeObserver::ResizeObserver(ResizeObserverCallback callback) : callback_(callback) {
    if (Application::instance()) {
        Application::instance()->registerResizeObserver(this);
    }
}

ResizeObserver::~ResizeObserver() {
    if (Application::instance()) {
        Application::instance()->unregisterResizeObserver(this);
    }
}

void ResizeObserver::observe(Widget* target) {
    if (!target) return;
    for (const auto& obs : observedTargets_) {
        if (obs.target == target) return;
    }
    observedTargets_.push_back({ target, -1.0f, -1.0f });
}

void ResizeObserver::unobserve(Widget* target) {
    observedTargets_.erase(
        std::remove_if(observedTargets_.begin(), observedTargets_.end(),
            [target](const ObservedTarget& obs) { return obs.target == target; }),
        observedTargets_.end()
    );
}

void ResizeObserver::disconnect() {
    observedTargets_.clear();
}

void ResizeObserver::gatherObservations(std::vector<ResizeObserverEntry>& activeObservations) {
    for (auto& obs : observedTargets_) {
        if (!obs.target) continue;
        float w = obs.target->bounds.w;
        float h = obs.target->bounds.h;
        if (w != obs.lastWidth || h != obs.lastHeight) {
            ResizeObserverEntry entry;
            entry.target = obs.target;
            entry.contentRect = Rect(0, 0, w, h);
            
            ResizeObserverSize size;
            size.inlineSize = w;
            size.blockSize = h;
            entry.borderBoxSize.push_back(size);
            entry.contentBoxSize.push_back(size);
            
            activeObservations.push_back(entry);
            
            obs.lastWidth = w;
            obs.lastHeight = h;
        }
    }
}

void ResizeObserver::deliverObservations(const std::vector<ResizeObserverEntry>& activeObservations) {
    if (callback_ && !activeObservations.empty()) {
        callback_(activeObservations, *this);
    }
}
static bool isUtf8Continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}
static size_t clampToUtf8Boundary(const std::string& text, size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && index < text.size() &&
           isUtf8Continuation((unsigned char)text[index])) {
        --index;
    }
    return index;
}
static size_t previousCodepoint(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index == 0) return 0;
    --index;
    while (index > 0 && isUtf8Continuation((unsigned char)text[index])) {
        --index;
    }
    return index;
}
static size_t nextCodepoint(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index >= text.size()) return text.size();
    ++index;
    while (index < text.size() && isUtf8Continuation((unsigned char)text[index])) {
        ++index;
    }
    return index;
}
static bool isAsciiWordChar(char c) {
    unsigned char uc = (unsigned char)c;
    return std::isalnum(uc) || c == '_';
}
static size_t previousWordBoundary(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    while (index > 0 && std::isspace((unsigned char)text[previousCodepoint(text, index)])) {
        index = previousCodepoint(text, index);
    }
    if (index == 0) return 0;
    size_t current = previousCodepoint(text, index);
    bool word = isAsciiWordChar(text[current]);
    while (current > 0) {
        size_t prev = previousCodepoint(text, current);
        if (std::isspace((unsigned char)text[prev]) || isAsciiWordChar(text[prev]) != word) break;
        current = prev;
    }
    return current;
}
static size_t nextWordBoundary(const std::string& text, size_t index) {
    index = clampToUtf8Boundary(text, index);
    if (index >= text.size()) return text.size();
    bool word = isAsciiWordChar(text[index]);
    while (index < text.size() &&
           !std::isspace((unsigned char)text[index]) &&
           isAsciiWordChar(text[index]) == word) {
        index = nextCodepoint(text, index);
    }
    while (index < text.size() && std::isspace((unsigned char)text[index])) {
        index = nextCodepoint(text, index);
    }
    return index;
}
static void wordRangeAt(const std::string& text, size_t index, size_t& start, size_t& end) {
    index = clampToUtf8Boundary(text, index);
    if (text.empty()) {
        start = end = 0;
        return;
    }
    if (index >= text.size()) index = previousCodepoint(text, text.size());
    if (std::isspace((unsigned char)text[index]) && index > 0) {
        index = previousCodepoint(text, index);
    }
    bool word = index < text.size() && isAsciiWordChar(text[index]);
    start = index;
    while (start > 0) {
        size_t prev = previousCodepoint(text, start);
        if (std::isspace((unsigned char)text[prev]) || isAsciiWordChar(text[prev]) != word) break;
        start = prev;
    }
    end = index;
    while (end < text.size()) {
        if (std::isspace((unsigned char)text[end]) || isAsciiWordChar(text[end]) != word) break;
        end = nextCodepoint(text, end);
    }
}
static float approximateGlyphAdvance(unsigned char c, float fontSize) {
    if (c == ' ') return fontSize * 0.32f;
    if (c == 'i' || c == 'l' || c == 'I' || c == '.' || c == ',' || c == ':' ||
        c == ';' || c == '!' || c == '\'' || c == '|') {
        return fontSize * 0.28f;
    }
    if (c == 'm' || c == 'w' || c == 'M' || c == 'W' || c == '@' || c == '#') {
        return fontSize * 0.82f;
    }
    if (c >= 128) return fontSize * 0.72f;
    return fontSize * 0.55f;
}
static std::string approximateLigatures(const std::string& text) {
    std::string result = text;
    const std::vector<std::string> ligs = {"ffi", "ffl", "ff", "fi", "fl", "ft", "st"};
    for (const auto& lig : ligs) {
        size_t pos = 0;
        while ((pos = result.find(lig, pos)) != std::string::npos) {
            result.replace(pos, lig.length(), "_");
            pos += 1;
        }
    }
    return result;
}

static float approximateTextWidth(const std::string& text, float fontSize) {
    std::string temp = approximateLigatures(text);
    float width = 0.0f;
    for (size_t i = 0; i < temp.size(); ) {
        unsigned char c = (unsigned char)temp[i];
        width += approximateGlyphAdvance(c, fontSize);
        i = nextCodepoint(temp, i);
    }
    return width;
}
static std::vector<std::string> wrapText(const std::string& text, float fontSize, float maxWidth) {
    std::vector<std::string> lines;
    if (maxWidth <= 0.0f || text.empty()) {
        lines.push_back(text);
        return lines;
    }
    std::string currentLine;
    float currentWidth = 0.0f;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
            currentWidth = 0.0f;
            i++;
            continue;
        }
        size_t start = i;
        if (std::isspace((unsigned char)text[i])) {
            while (i < text.size() && text[i] != '\n' && std::isspace((unsigned char)text[i])) {
                i = nextCodepoint(text, i);
            }
        } else {
            while (i < text.size() && text[i] != '\n' && !std::isspace((unsigned char)text[i])) {
                i = nextCodepoint(text, i);
            }
        }
        std::string chunk = text.substr(start, i - start);
        float chunkWidth = approximateTextWidth(chunk, fontSize);
        if (currentLine.empty()) {
            currentLine = chunk;
            currentWidth = chunkWidth;
        } else if (currentWidth + chunkWidth <= maxWidth) {
            currentLine += chunk;
            currentWidth += chunkWidth;
        } else {
            if (std::isspace((unsigned char)chunk[0])) {
                lines.push_back(currentLine);
                currentLine.clear();
                currentWidth = 0.0f;
            } else {
                lines.push_back(currentLine);
                currentLine = chunk;
                currentWidth = chunkWidth;
            }
        }
    }
    if (!currentLine.empty() || lines.empty()) {
        lines.push_back(currentLine);
    }
    return lines;
}
static std::string trimAsciiLocal(std::string value) {
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isWs((unsigned char)value.front())) value.erase(value.begin());
    while (!value.empty() && isWs((unsigned char)value.back())) value.pop_back();
    return value;
}
static float parseSvgLengthLocal(const std::string& value, float fallback = 0.0f) {
    std::string s = trimAsciiLocal(value);
    if (s.empty()) return fallback;
    char* end = nullptr;
    float number = std::strtof(s.c_str(), &end);
    if (end == s.c_str()) return fallback;
    return number;
}
static std::string attrFromTagLocal(const std::string& tag, const std::string& name) {
    size_t pos = tag.find(name);
    while (pos != std::string::npos) {
        bool leftOk = pos == 0 || std::isspace((unsigned char)tag[pos - 1]) || tag[pos - 1] == '<';
        size_t after = pos + name.size();
        bool rightOk = after < tag.size() && (std::isspace((unsigned char)tag[after]) || tag[after] == '=');
        if (leftOk && rightOk) break;
        pos = tag.find(name, pos + 1);
    }
    if (pos == std::string::npos) return {};
    size_t eq = tag.find('=', pos + name.size());
    if (eq == std::string::npos) return {};
    size_t start = eq + 1;
    while (start < tag.size() && std::isspace((unsigned char)tag[start])) ++start;
    if (start >= tag.size()) return {};
    char quote = tag[start];
    if (quote == '"' || quote == '\'') {
        size_t end = tag.find(quote, start + 1);
        if (end == std::string::npos) return {};
        return tag.substr(start + 1, end - start - 1);
    }
    size_t end = start;
    while (end < tag.size() && !std::isspace((unsigned char)tag[end]) && tag[end] != '>') ++end;
    return tag.substr(start, end - start);
}
static Vec2 probeImageNaturalSize(const std::string& source) {
    if (source.empty()) return {0, 0};
    std::ifstream file(source, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {0, 0};
    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0 || fileSize > 32 * 1024 * 1024) return {0, 0};
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes((size_t)fileSize);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), fileSize).good()) return {0, 0};
    std::string head(reinterpret_cast<const char*>(bytes.data()),
                     std::min<size_t>(bytes.size(), 4096));
    size_t svgPos = head.find("<svg");
    if (svgPos != std::string::npos) {
        size_t end = head.find('>', svgPos);
        std::string tag = end == std::string::npos ? head.substr(svgPos) : head.substr(svgPos, end - svgPos + 1);
        float width = parseSvgLengthLocal(attrFromTagLocal(tag, "width"), 0.0f);
        float height = parseSvgLengthLocal(attrFromTagLocal(tag, "height"), 0.0f);
        if ((width <= 0.0f || height <= 0.0f)) {
            std::istringstream viewBox(attrFromTagLocal(tag, "viewBox"));
            float x = 0, y = 0, w = 0, h = 0;
            if (viewBox >> x >> y >> w >> h) {
                if (width <= 0.0f) width = w;
                if (height <= 0.0f) height = h;
            }
        }
        if (width > 0.0f && height > 0.0f) return {width, height};
        return {300.0f, 150.0f};
    }
    int w = 0, h = 0, comp = 0;
    if (stbi_info_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &comp) && w > 0 && h > 0) {
        return {(float)w, (float)h};
    }
    return {0, 0};
}
static bool parentUsesRowFlex(const Widget* widget) {
    if (!widget || !widget->parent) return false;
    const Style& parentStyle = widget->parent->computedStyle;
    if (parentStyle.display != Display::Flex) return false;
    return parentStyle.flexDirection == FlexDirection::Row ||
           parentStyle.flexDirection == FlexDirection::RowReverse;
}
static size_t approximateTextIndexAtX(const std::string& text, float x, float fontSize) {
    if (x <= 0 || text.empty()) return 0;
    float cursor = 0;
    for (size_t i = 0; i < text.size(); ) {
        size_t current = i;
        unsigned char c = (unsigned char)text[i];
        size_t next = nextCodepoint(text, i);
        float advance = approximateGlyphAdvance(c, fontSize);
        if (x < cursor + advance * 0.5f) return current;
        if (x < cursor + advance) return next;
        cursor += advance;
        i = next;
    }
    return text.size();
}
static size_t passwordTextIndexAtX(const std::string& text, float x, float fontSize) {
    if (x <= 0 || text.empty()) return 0;
    float cursor = 0;
    float advance = approximateGlyphAdvance('*', fontSize);
    for (size_t i = 0; i < text.size(); ) {
        size_t current = i;
        size_t next = nextCodepoint(text, i);
        if (x < cursor + advance * 0.5f) return current;
        if (x < cursor + advance) return next;
        cursor += advance;
        i = next;
    }
    return text.size();
}
static size_t codepointCountInRange(const std::string& text, size_t start, size_t end) {
    start = clampToUtf8Boundary(text, start);
    end = clampToUtf8Boundary(text, std::min(end, text.size()));
    if (end < start) std::swap(start, end);
    size_t count = 0;
    for (size_t i = start; i < end; i = nextCodepoint(text, i)) {
        ++count;
    }
    return count;
}
static std::string maskedPasswordRange(const std::string& text, size_t start, size_t end) {
    return std::string(codepointCountInRange(text, start, end), '*');
}
static std::string inputVisibleRange(TextInputType type,
                                     const std::string& text,
                                     size_t start,
                                     size_t end) {
    if (type == TextInputType::Password) {
        return maskedPasswordRange(text, start, end);
    }
    start = clampToUtf8Boundary(text, start);
    end = clampToUtf8Boundary(text, std::min(end, text.size()));
    if (end < start) std::swap(start, end);
    return text.substr(start, end - start);
}
static bool isTextEditingInputType(TextInputType type) {
    switch (type) {
    case TextInputType::Text:
    case TextInputType::Password:
    case TextInputType::Search:
    case TextInputType::Email:
    case TextInputType::Url:
    case TextInputType::Tel:
    case TextInputType::Number:
    case TextInputType::Date:
    case TextInputType::Time:
    case TextInputType::Month:
    case TextInputType::Week:
    case TextInputType::DateTimeLocal:
        return true;
    default:
        return false;
    }
}
static bool isButtonLikeInputType(TextInputType type) {
    switch (type) {
    case TextInputType::Button:
    case TextInputType::Submit:
    case TextInputType::Reset:
    case TextInputType::File:
    case TextInputType::Color:
    case TextInputType::Image:
        return true;
    default:
        return false;
    }
}
static int normalizeTextEditingKey(int keyCode) {
    switch (keyCode) {
    case 0x4000004a: return 0x24; // SDL home
    case 0x4000004d: return 0x23; // SDL end
    case 0x4000004f: return 0x27; // SDL right
    case 0x40000050: return 0x25; // SDL left
    case 0x40000051: return 0x28; // SDL down
    case 0x40000052: return 0x26; // SDL up
    case 0x4000004b: return 0x21; // SDL page up
    case 0x4000004e: return 0x22; // SDL page down
    case 0x7f:       return 0x2E; // POSIX delete
    default:         return keyCode;
    }
}
static bool consumesParentMainAxisHeight(const Widget* widget, const Style& style) {
    if (!widget || !widget->parent || style.flexGrow <= 0.0f) return false;
    const Style& parentStyle = widget->parent->computedStyle;
    if (parentStyle.display != Display::Flex) return false;
    return parentStyle.flexDirection == FlexDirection::Column ||
           parentStyle.flexDirection == FlexDirection::ColumnReverse;
}
static bool isOutOfFlow(const Widget* widget) {
    if (!widget) return false;
    return widget->computedStyle.position == Position::Absolute ||
           widget->computedStyle.position == Position::Fixed;
}
static bool rectIntersects(const Rect& a, const Rect& b, float padding = 0.0f) {
    return a.x + a.w >= b.x - padding &&
           a.x <= b.x + b.w + padding &&
           a.y + a.h >= b.y - padding &&
           a.y <= b.y + b.h + padding;
}
static bool isDisplayNone(const Widget* widget) {
    return widget && widget->computedStyle.display == Display::None;
}
static bool canPaintWidget(const Widget* widget) {
    return widget && widget->visible &&
           widget->computedStyle.display != Display::None &&
           widget->computedStyle.visibility == Visibility::Visible;
}
static bool canHitTestWidget(const Widget* widget) {
    return canPaintWidget(widget) &&
           widget->computedStyle.pointerEvents != PointerEvents::None;
}
static bool isKeyboardFocusableWidget(const Widget* widget) {
    if (!canHitTestWidget(widget)) return false;
    return widget->type == "input" ||
           widget->type == "textarea" ||
           widget->type == "button" ||
           widget->type == "checkbox" ||
           widget->type == "radio" ||
           widget->type == "range" ||
           widget->type == "select" ||
           widget->type == "summary" ||
           widget->type == "a" ||
           widget->onClick ||
           widget->computedStyle.cursor == CursorType::Pointer;
}
static void collectKeyboardFocusableWidgets(Widget* widget, std::vector<Widget*>& out) {
    if (!widget) return;
    if (isKeyboardFocusableWidget(widget)) {
        out.push_back(widget);
    }
    for (auto& child : widget->children) {
        collectKeyboardFocusableWidgets(child.get(), out);
    }
}
static void clearFocusRecursive(Widget* widget) {
    if (!widget) return;
    widget->focused = false;
    for (auto& child : widget->children) {
        clearFocusRecursive(child.get());
    }
}
static Widget* focusedWidgetInSubtree(Widget* widget) {
    if (!widget) return nullptr;
    if (widget->focused) return widget;
    for (auto& child : widget->children) {
        if (Widget* focused = focusedWidgetInSubtree(child.get())) {
            return focused;
        }
    }
    return nullptr;
}
static bool moveDocumentFocus(Widget* root, bool backwards) {
    if (!root) return false;
    std::vector<Widget*> focusables;
    collectKeyboardFocusableWidgets(root, focusables);
    if (focusables.empty()) return false;

    Widget* current = focusedWidgetInSubtree(root);
    size_t nextIndex = backwards ? focusables.size() - 1 : 0;
    if (current) {
        auto it = std::find(focusables.begin(), focusables.end(), current);
        if (it != focusables.end()) {
            size_t index = static_cast<size_t>(std::distance(focusables.begin(), it));
            nextIndex = backwards
                ? (index == 0 ? focusables.size() - 1 : index - 1)
                : (index + 1) % focusables.size();
        }
    }
    clearFocusRecursive(root);
    focusables[nextIndex]->focused = true;
    return true;
}
static Widget* rootOfWidget(Widget* widget) {
    if (!widget) return nullptr;
    while (widget->parent) {
        widget = widget->parent;
    }
    return widget;
}
static void clearRadioGroup(Widget* widget, Radio* active, const std::string& group) {
    if (!widget) return;
    if (auto* radio = dynamic_cast<Radio*>(widget)) {
        bool sameGroup = group.empty() ? (radio->parent == active->parent) : (radio->group == group);
        if (radio != active && sameGroup) {
            radio->checked = false;
            radio->markStyleDirty();
        }
    }
    for (auto& child : widget->children) {
        clearRadioGroup(child.get(), active, group);
    }
}
static std::vector<Option*> selectOptions(Select* select) {
    std::vector<Option*> options;
    if (!select) return options;
    options.reserve(select->children.size());
    for (auto& child : select->children) {
        if (auto* option = dynamic_cast<Option*>(child.get())) {
            options.push_back(option);
        }
    }
    return options;
}
static const char* textInputTypeSelector(TextInputType type) {
    switch (type) {
    case TextInputType::Password: return "password";
    case TextInputType::Search: return "search";
    case TextInputType::Email: return "email";
    case TextInputType::Url: return "url";
    case TextInputType::Tel: return "tel";
    case TextInputType::Number: return "number";
    case TextInputType::Hidden: return "hidden";
    case TextInputType::Button: return "button";
    case TextInputType::Submit: return "submit";
    case TextInputType::Reset: return "reset";
    case TextInputType::File: return "file";
    case TextInputType::Color: return "color";
    case TextInputType::Date: return "date";
    case TextInputType::Time: return "time";
    case TextInputType::Month: return "month";
    case TextInputType::Week: return "week";
    case TextInputType::DateTimeLocal: return "datetime-local";
    case TextInputType::Image: return "image";
    case TextInputType::Text:
    default: return "text";
    }
}
Widget::Widget() {}
Widget::~Widget() {
    if (auto* app = Application::instance()) {
        app->onWidgetDestroyed(this);
    }
}
void Widget::attachLayoutTree() {
    if (computedStyle.display == Display::None) {
        layoutObject.reset();
        return;
    }

    layoutObject = createLayoutObject();
    if (!layoutObject) return;

    layoutObject->clearChildren();

    if (beforePseudoNode) {
        beforePseudoNode->attachLayoutTree();
        if (beforePseudoNode->layoutObject) {
            layoutObject->addChild(beforePseudoNode->layoutObject.get());
        }
    }

    for (auto& child : children) {
        child->attachLayoutTree();
        if (child->layoutObject) {
            layoutObject->addChild(child->layoutObject.get());
        }
    }

    if (afterPseudoNode) {
        afterPseudoNode->attachLayoutTree();
        if (afterPseudoNode->layoutObject) {
            layoutObject->addChild(afterPseudoNode->layoutObject.get());
        }
    }
}
std::unique_ptr<LayoutObject> Widget::createLayoutObject() {
    if (type == "text") {
        auto* textWidget = static_cast<Text*>(this);
        return std::make_unique<LayoutText>(this, textWidget->content);
    }
    if (computedStyle.display == Display::Flex) {
        return std::make_unique<LayoutFlexibleBox>(this);
    } else if (computedStyle.display == Display::Grid) {
        return std::make_unique<LayoutGrid>(this);
    } else if (computedStyle.display == Display::Block || 
               computedStyle.display == Display::InlineBlock) {
        return std::make_unique<LayoutBlock>(this);
    }
    return std::make_unique<LayoutBox>(this);
}
const std::string& Widget::selectorType() const {
    if (!cachedSelectorType.empty()) {
        return cachedSelectorType;
    }
    cachedSelectorType.clear();
    if (dynamic_cast<const TextArea*>(this)) {
        cachedSelectorType = "textarea";
    } else if (auto* input = dynamic_cast<const TextInput*>(this)) {
        if (this->type == "textarea") cachedSelectorType = "textarea";
        else {
            cachedSelectorType = "input|type=";
            cachedSelectorType += textInputTypeSelector(input->inputType);
        }
    } else if (auto* checkbox = dynamic_cast<const Checkbox*>(this)) {
        cachedSelectorType = "input|type=checkbox";
        if (checkbox->checked) cachedSelectorType += "|checked";
    } else if (auto* radio = dynamic_cast<const Radio*>(this)) {
        cachedSelectorType = "input|type=radio";
        if (radio->checked) cachedSelectorType += "|checked";
    } else if (dynamic_cast<const RangeInput*>(this)) {
        cachedSelectorType = "input|type=range";
    } else if (auto* select = dynamic_cast<const Select*>(this)) {
        cachedSelectorType = "select";
        if (select->expanded) cachedSelectorType += "|open";
    } else if (auto* details = dynamic_cast<const Details*>(this)) {
        cachedSelectorType = "details";
        if (details->open) cachedSelectorType += "|open";
    } else if (auto* dialog = dynamic_cast<const Dialog*>(this)) {
        cachedSelectorType = "dialog";
        if (dialog->open) cachedSelectorType += "|open";
        if (dialog->modal) cachedSelectorType += "|modal";
    } else if (auto* progress = dynamic_cast<const Progress*>(this)) {
        cachedSelectorType = "progress";
        if (progress->value < 0.0f) cachedSelectorType += "|indeterminate";
        else cachedSelectorType += "|value";
    } else {
        cachedSelectorType = this->type;
    }
    // Append dir attribute for CSS [dir="rtl"] / [dir="ltr"] matching
    if (!dir.empty()) {
        cachedSelectorType += "|dir=";
        cachedSelectorType += dir;
    }
    return cachedSelectorType;
}

static const std::string& widgetSelectorType(const Widget* widget) {
    if (!widget) {
        static const std::string empty;
        return empty;
    }
    return widget->selectorType();
}
static bool clipsOverflow(Overflow overflow) {
    return overflow == Overflow::Hidden || overflow == Overflow::Scroll ||
           overflow == Overflow::Auto || overflow == Overflow::Clip;
}
static bool isOverflowVisibleOrClip(Overflow overflow) {
    return overflow == Overflow::Visible || overflow == Overflow::Clip;
}
static Overflow normalizedOverflowAxis(Overflow axis, Overflow otherAxis) {
    if (!isOverflowVisibleOrClip(otherAxis)) {
        if (axis == Overflow::Visible) return Overflow::Auto;
        if (axis == Overflow::Clip) return Overflow::Hidden;
    }
    return axis;
}
static Overflow effectiveOverflowX(const Style& style) {
    return normalizedOverflowAxis(style.overflowX, style.overflowY);
}
static Overflow effectiveOverflowY(const Style& style) {
    return normalizedOverflowAxis(style.overflowY, style.overflowX);
}
static bool clipsOverflow(const Style& style) {
    return clipsOverflow(effectiveOverflowX(style)) ||
           clipsOverflow(effectiveOverflowY(style));
}
static bool scrollsOverflowY(const Style& style, float contentHeight, float boundsHeight) {
    Overflow overflow = effectiveOverflowY(style);
    if (overflow == Overflow::Scroll) return contentHeight > boundsHeight + 1.0f;
    if (overflow == Overflow::Auto) return contentHeight > boundsHeight + 1.0f;
    return false;
}
bool Widget::isScrollableY() const {
    return scrollsOverflowY(computedStyle, contentHeight, bounds.h);
}
bool Widget::isClippingOverflow() const {
    return clipsOverflow(computedStyle);
}
static float usedBorderTopWidth(const Style& style) {
    return style.hasBorderTop ? style.borderTop.width : style.border.width;
}
static float usedBorderRightWidth(const Style& style) {
    return style.hasBorderRight ? style.borderRight.width : style.border.width;
}
static float usedBorderBottomWidth(const Style& style) {
    return style.hasBorderBottom ? style.borderBottom.width : style.border.width;
}
static float usedBorderLeftWidth(const Style& style) {
    return style.hasBorderLeft ? style.borderLeft.width : style.border.width;
}
static float usedBorderHorizontal(const Style& style) {
    return usedBorderLeftWidth(style) + usedBorderRightWidth(style);
}
static float usedBorderVertical(const Style& style) {
    return usedBorderTopWidth(style) + usedBorderBottomWidth(style);
}
static const std::string& renderFontName(const Style& style) {
    static const std::string s_defaultFont = "default";
    return style.fontFamily.empty() ? s_defaultFont : style.fontFamily;
}
static std::string applyTextTransform(const std::string& text, TextTransform transform) {
    if (transform == TextTransform::None || text.empty()) return text;
    std::string out = text;
    if (transform == TextTransform::Uppercase) {
        for (char& c : out) c = (char)std::toupper((unsigned char)c);
    } else if (transform == TextTransform::Lowercase) {
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
    } else if (transform == TextTransform::Capitalize) {
        bool startWord = true;
        for (char& c : out) {
            unsigned char uc = (unsigned char)c;
            if (std::isspace(uc)) {
                startWord = true;
            } else if (startWord) {
                c = (char)std::toupper(uc);
                startWord = false;
            } else {
                c = (char)std::tolower(uc);
            }
        }
    }
    return out;
}
static void renderTextDecoration(Renderer& renderer,
                                 const std::string& text,
                                 const Rect& rect,
                                 const Color& textColor,
                                 const Style& style) {
    if (style.textDecoration == TextDecoration::None || text.empty()) return;
    std::string fontName = renderFontName(style);
    float textWidth = renderer.measureText(text, style.fontSize, fontName).x;
    float x = rect.x;
    if (style.textAlign == TextAlign::Center) {
        x = rect.x + (rect.w - textWidth) * 0.5f;
    } else if (style.textAlign == TextAlign::Right) {
        x = rect.x + rect.w - textWidth;
    }
    Color lineColor = style.hasTextDecorationColor ? style.textDecorationColor : textColor;
    float thickness = std::max(1.0f, std::round(style.fontSize / 14.0f));
    float y = rect.y + rect.h * 0.5f;
    if (style.textDecoration == TextDecoration::Underline) {
        y += style.fontSize * 0.36f;
    } else if (style.textDecoration == TextDecoration::Overline) {
        y -= style.fontSize * 0.44f;
    }
    renderer.drawRoundedRect({x, y, std::max(0.0f, textWidth), thickness},
                             lineColor, BorderRadius(thickness * 0.5f));
}
static std::string ellipsizeText(Renderer& renderer,
                                 const std::string& text,
                                 float maxWidth,
                                 float fontSize,
                                 const std::string& fontName) {
    if (text.empty() || maxWidth <= 0.0f) return "";
    if (renderer.measureText(text, fontSize, fontName).x <= maxWidth) return text;
    constexpr const char* ellipsis = "...";
    float ellipsisWidth = renderer.measureText(ellipsis, fontSize, fontName).x;
    if (ellipsisWidth >= maxWidth) return "";
    size_t lo = 0;
    size_t hi = text.size();
    size_t best = 0;
    while (lo <= hi) {
        size_t mid = clampToUtf8Boundary(text, (lo + hi) / 2);
        std::string candidate = text.substr(0, mid) + ellipsis;
        float width = renderer.measureText(candidate, fontSize, fontName).x;
        if (width <= maxWidth) {
            best = mid;
            lo = mid + 1;
        } else {
            if (mid == 0) break;
            hi = mid - 1;
        }
    }
    return text.substr(0, clampToUtf8Boundary(text, best)) + ellipsis;
}
static bool rectEqual(const Rect& a, const Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
static bool intersectRects(const Rect& a, const Rect& b, Rect& out) {
    float x0 = std::max(a.x, b.x);
    float y0 = std::max(a.y, b.y);
    float x1 = std::min(a.x + a.w, b.x + b.w);
    float y1 = std::min(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        out = Rect();
        return false;
    }
    out = {x0, y0, x1 - x0, y1 - y0};
    return true;
}
static void hashCombine(size_t& seed, size_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}
static void hashFloat(size_t& seed, float value) {
    hashCombine(seed, std::hash<int>{}((int)std::round(value * 1000.0f)));
}
static void hashCSSValue(size_t& seed, const CSSValue& value) {
    hashCombine(seed, std::hash<int>{}((int)value.unit));
    hashFloat(seed, value.value);
}
static size_t layoutStyleSignature(const Style& s) {
    size_t seed = 0;
    hashCombine(seed, std::hash<int>{}((int)s.display));
    hashCombine(seed, std::hash<int>{}((int)s.flexDirection));
    hashCombine(seed, std::hash<int>{}((int)s.justifyContent));
    hashCombine(seed, std::hash<int>{}((int)s.alignItems));
    hashCombine(seed, std::hash<int>{}((int)s.overflow));
    hashCombine(seed, std::hash<int>{}((int)s.overflowX));
    hashCombine(seed, std::hash<int>{}((int)s.overflowY));
    hashCombine(seed, std::hash<int>{}((int)s.boxSizing));
    hashCombine(seed, std::hash<bool>{}(s.hasBoxSizing));
    hashFloat(seed, s.flexGrow);
    hashFloat(seed, s.flexShrink);
    hashCSSValue(seed, s.flexBasis);
    hashFloat(seed, s.gap);
    hashFloat(seed, s.rowGap);
    hashFloat(seed, s.columnGap);
    hashFloat(seed, s.aspectRatio);
    hashCombine(seed, std::hash<int>{}((int)s.objectFit));
    hashFloat(seed, s.objectPosition.x);
    hashFloat(seed, s.objectPosition.y);
    hashFloat(seed, s.objectPositionOffset.x);
    hashFloat(seed, s.objectPositionOffset.y);
    hashCSSValue(seed, s.width);
    hashCSSValue(seed, s.height);
    hashCSSValue(seed, s.minWidth);
    hashCSSValue(seed, s.minHeight);
    hashCSSValue(seed, s.maxWidth);
    hashCSSValue(seed, s.maxHeight);
    hashFloat(seed, s.padding.top);
    hashFloat(seed, s.padding.right);
    hashFloat(seed, s.padding.bottom);
    hashFloat(seed, s.padding.left);
    hashFloat(seed, s.margin.top);
    hashFloat(seed, s.margin.right);
    hashFloat(seed, s.margin.bottom);
    hashFloat(seed, s.margin.left);
    hashCSSValue(seed, s.top);
    hashCSSValue(seed, s.right);
    hashCSSValue(seed, s.bottom);
    hashCSSValue(seed, s.left);
    hashFloat(seed, s.fontSize);
    hashFloat(seed, s.lineHeight);
    hashCombine(seed, std::hash<int>{}((int)s.fontWeight));
    hashCombine(seed, std::hash<int>{}((int)s.fontStyle));
    hashCombine(seed, std::hash<int>{}((int)s.textAlign));
    hashCombine(seed, std::hash<int>{}((int)s.verticalAlign));
    hashCombine(seed, std::hash<int>{}((int)s.direction));
    hashCombine(seed, std::hash<int>{}((int)s.unicodeBidi));
    return seed;
}
size_t Widget::addEventListener(const std::string& type, DOMEventListener callback, bool useCapture) {
    EventListenerEntry entry;
    entry.id = nextDomListenerId++;
    entry.type = type;
    entry.callback = callback;
    entry.useCapture = useCapture;
    domEventListeners.push_back(entry);
    return entry.id;
}

void Widget::removeEventListener(size_t listenerId) {
    domEventListeners.erase(
        std::remove_if(domEventListeners.begin(), domEventListeners.end(),
                       [listenerId](const EventListenerEntry& entry) {
                           return entry.id == listenerId;
                       }),
        domEventListeners.end());
}

void Widget::dispatchEvent(Event& event) {
    if (event.type.empty()) return;
    std::vector<Widget*> path;
    Widget* curr = this;
    while (curr) {
        path.push_back(curr);
        curr = curr->parent;
    }
    std::reverse(path.begin(), path.end());
    if (!event.target) {
        event.target = this;
    }
    event.phase = EventPhase::Capture;
    for (size_t i = 0; i < path.size() - 1; ++i) {
        Widget* w = path[i];
        event.currentTarget = w;
        auto listenersCopy = w->domEventListeners;
        for (auto& entry : listenersCopy) {
            if (entry.type == event.type && entry.useCapture) {
                entry.callback(event);
                if (event.propagationStopped) break;
            }
        }
        if (event.propagationStopped) break;
    }
    if (!event.propagationStopped && !path.empty()) {
        Widget* w = path.back();
        event.phase = EventPhase::AtTarget;
        event.currentTarget = w;
        auto listenersCopy = w->domEventListeners;
        for (auto& entry : listenersCopy) {
            if (entry.type == event.type) {
                entry.callback(event);
                if (event.propagationStopped) break;
            }
        }
        if (event.type == "click" && !event.defaultPrevented && w->onClick) {
            w->onClick();
        }
    }
    if (event.bubbles && !event.propagationStopped && path.size() > 1) {
        event.phase = EventPhase::Bubble;
        for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
            Widget* w = path[i];
            event.currentTarget = w;
            auto listenersCopy = w->domEventListeners;
            for (auto& entry : listenersCopy) {
                if (entry.type == event.type && !entry.useCapture) {
                    entry.callback(event);
                    if (event.propagationStopped) break;
                }
            }
            if (event.propagationStopped) break;
        }
    }
}

void Widget::markLayoutDirty() {
    if (layoutDirty && (!parent || parent->layoutDirty)) return;
    layoutDirty = true;
    lifecycleState = WidgetLifecycle::LayoutDirty;
    if (parent) parent->markLayoutDirty();
    if (auto* app = Application::instance()) {
        app->requestRedraw();
    }
}
void Widget::markSubtreeStyleDirty() {
    if (subtreeStyleDirty && (!parent || parent->subtreeStyleDirty)) return;
    subtreeStyleDirty = true;
    if (lifecycleState < WidgetLifecycle::StyleDirty) {
        lifecycleState = WidgetLifecycle::StyleDirty;
    }
    if (parent) parent->markSubtreeStyleDirty();
}
void Widget::markStyleDirty() {
    styleDirty = true;
    subtreeStyleDirty = true;
    lifecycleState = WidgetLifecycle::StyleDirty;
    markLayoutDirty();
    if (parent) parent->markSubtreeStyleDirty();
}
void Widget::markStyleDirtyRecursive() {
    styleDirty = true;
    subtreeStyleDirty = true;
    lifecycleState = WidgetLifecycle::StyleDirty;
    markLayoutDirty();
    for (auto& child : children) {
        child->markStyleDirtyRecursive();
    }
    if (parent) parent->markSubtreeStyleDirty();
}

void Widget::invalidateStyleOnClassListChange(const std::string& oldClassName, const std::string& newClassName) {
    styleDirty = true;
    markSubtreeStyleDirty();
    
    const StyleSheet* sheet = nullptr;
    if (auto* app = Application::instance()) {
        sheet = &app->stylesheet();
    }
    if (!sheet) {
        markStyleDirtyRecursive();
        return;
    }
    
    auto splitClasses = [](const std::string& s, std::unordered_set<std::string>& out) {
        std::istringstream stream(s);
        std::string cls;
        while (stream >> cls) {
            out.insert(cls);
        }
    };
    
    std::unordered_set<std::string> oldSet, newSet;
    splitClasses(oldClassName, oldSet);
    splitClasses(newClassName, newSet);
    
    std::vector<std::string> changedClasses;
    for (const auto& c : oldSet) {
        if (newSet.find(c) == newSet.end()) {
            changedClasses.push_back(c);
        }
    }
    for (const auto& c : newSet) {
        if (oldSet.find(c) == oldSet.end()) {
            changedClasses.push_back(c);
        }
    }
    
    if (changedClasses.empty()) return;
    
    for (const auto& c : changedClasses) {
        const auto* invalidationSet = sheet->getClassInvalidationSet(c);
        if (!invalidationSet) continue;
        
        if (invalidationSet->invalidateAllDescendants) {
            markStyleDirtyRecursive();
        } else if (!invalidationSet->descendantClasses.empty() ||
                   !invalidationSet->descendantIds.empty() ||
                   !invalidationSet->descendantTypes.empty()) {
            
            std::function<void(Widget*)> invalidateDescendants = [&](Widget* w) {
                for (auto& child : w->children) {
                    bool match = false;
                    
                    if (!invalidationSet->descendantClasses.empty() && !child->className.empty()) {
                        std::istringstream stream(child->className);
                        std::string cc;
                        while (stream >> cc) {
                            if (invalidationSet->descendantClasses.find(cc) != invalidationSet->descendantClasses.end()) {
                                match = true;
                                break;
                            }
                        }
                    }
                    
                    if (!match && !invalidationSet->descendantIds.empty()) {
                        if (invalidationSet->descendantIds.find(child->id) != invalidationSet->descendantIds.end()) {
                            match = true;
                        }
                    }
                    
                    if (!match && !invalidationSet->descendantTypes.empty()) {
                        if (invalidationSet->descendantTypes.find(child->type) != invalidationSet->descendantTypes.end()) {
                            match = true;
                        }
                    }
                    
                    if (match) {
                        child->styleDirty = true;
                        child->markSubtreeStyleDirty();
                    }
                    
                    invalidateDescendants(child.get());
                }
            };
            invalidateDescendants(this);
        }
        
        if (parent) {
            if (invalidationSet->invalidateAllSiblings) {
                for (auto& sibling : parent->children) {
                    if (sibling.get() != this) {
                        sibling->markStyleDirtyRecursive();
                    }
                }
            } else if (!invalidationSet->siblingClasses.empty() ||
                       !invalidationSet->siblingIds.empty() ||
                       !invalidationSet->siblingTypes.empty()) {
                for (auto& sibling : parent->children) {
                    if (sibling.get() == this) continue;
                    
                    bool match = false;
                    
                    if (!invalidationSet->siblingClasses.empty() && !sibling->className.empty()) {
                        std::istringstream stream(sibling->className);
                        std::string cc;
                        while (stream >> cc) {
                            if (invalidationSet->siblingClasses.find(cc) != invalidationSet->siblingClasses.end()) {
                                match = true;
                                break;
                            }
                        }
                    }
                    
                    if (!match && !invalidationSet->siblingIds.empty()) {
                        if (invalidationSet->siblingIds.find(sibling->id) != invalidationSet->siblingIds.end()) {
                            match = true;
                        }
                    }
                    
                    if (!match && !invalidationSet->siblingTypes.empty()) {
                        if (invalidationSet->siblingTypes.find(sibling->type) != invalidationSet->siblingTypes.end()) {
                            match = true;
                        }
                    }
                    
                    if (match) {
                        sibling->markStyleDirtyRecursive();
                    }
                }
            }
        }
    }
}

void Widget::invalidateStyleOnIdChange(const std::string& oldId, const std::string& newId) {
    styleDirty = true;
    markSubtreeStyleDirty();
    
    const StyleSheet* sheet = nullptr;
    if (auto* app = Application::instance()) {
        sheet = &app->stylesheet();
    }
    if (!sheet) {
        markStyleDirtyRecursive();
        return;
    }
    
    std::vector<std::string> changedIds;
    if (!oldId.empty()) changedIds.push_back(oldId);
    if (!newId.empty()) changedIds.push_back(newId);
    
    for (const auto& idVal : changedIds) {
        const auto* invalidationSet = sheet->getIdInvalidationSet(idVal);
        if (!invalidationSet) continue;
        
        if (invalidationSet->invalidateAllDescendants) {
            markStyleDirtyRecursive();
        } else if (!invalidationSet->descendantClasses.empty() ||
                   !invalidationSet->descendantIds.empty() ||
                   !invalidationSet->descendantTypes.empty()) {
            
            std::function<void(Widget*)> invalidateDescendants = [&](Widget* w) {
                for (auto& child : w->children) {
                    bool match = false;
                    
                    if (!invalidationSet->descendantClasses.empty() && !child->className.empty()) {
                        std::istringstream stream(child->className);
                        std::string cc;
                        while (stream >> cc) {
                            if (invalidationSet->descendantClasses.find(cc) != invalidationSet->descendantClasses.end()) {
                                match = true;
                                break;
                            }
                        }
                    }
                    
                    if (!match && !invalidationSet->descendantIds.empty()) {
                        if (invalidationSet->descendantIds.find(child->id) != invalidationSet->descendantIds.end()) {
                            match = true;
                        }
                    }
                    
                    if (!match && !invalidationSet->descendantTypes.empty()) {
                        if (invalidationSet->descendantTypes.find(child->type) != invalidationSet->descendantTypes.end()) {
                            match = true;
                        }
                    }
                    
                    if (match) {
                        child->styleDirty = true;
                        child->markSubtreeStyleDirty();
                    }
                    
                    invalidateDescendants(child.get());
                }
            };
            invalidateDescendants(this);
        }
        
        if (parent) {
            if (invalidationSet->invalidateAllSiblings) {
                for (auto& sibling : parent->children) {
                    if (sibling.get() != this) {
                        sibling->markStyleDirtyRecursive();
                    }
                }
            } else if (!invalidationSet->siblingClasses.empty() ||
                       !invalidationSet->siblingIds.empty() ||
                       !invalidationSet->siblingTypes.empty()) {
                for (auto& sibling : parent->children) {
                    if (sibling.get() == this) continue;
                    
                    bool match = false;
                    
                    if (!invalidationSet->siblingClasses.empty() && !sibling->className.empty()) {
                        std::istringstream stream(sibling->className);
                        std::string cc;
                        while (stream >> cc) {
                            if (invalidationSet->siblingClasses.find(cc) != invalidationSet->siblingClasses.end()) {
                                match = true;
                                break;
                            }
                        }
                    }
                    
                    if (!match && !invalidationSet->siblingIds.empty()) {
                        if (invalidationSet->siblingIds.find(sibling->id) != invalidationSet->siblingIds.end()) {
                            match = true;
                        }
                    }
                    
                    if (!match && !invalidationSet->siblingTypes.empty()) {
                        if (invalidationSet->siblingTypes.find(sibling->type) != invalidationSet->siblingTypes.end()) {
                            match = true;
                        }
                    }
                    
                    if (match) {
                        sibling->markStyleDirtyRecursive();
                    }
                }
            }
        }
    }
}

void Widget::resolveStyles(const StyleSheet& sheet) {
    if (!subtreeStyleDirty) {
        return;
    }
    if (styleDirty) {
        // High-Fidelity Sibling Style Sharing (inspired by Chromium Blink's Peer ComputedStyle Sharing)
        // If an already-resolved sibling shares identical classes, inline styles, and constraints,
        // we can copy its computedStyle directly, completely bypassing expensive stylesheet matches.
        if (parent) {
            std::string_view selectorType = widgetSelectorType(this);
            for (const auto& sibling : parent->children) {
                if (sibling.get() == this) break;
                if (!sibling->styleDirty &&
                    sibling->className == className &&
                    sibling->id == id &&
                    std::string_view(widgetSelectorType(sibling.get())) == selectorType &&
                    sibling->style.width == style.width &&
                    sibling->style.height == style.height &&
                    sibling->style.minWidth == style.minWidth &&
                    sibling->style.minHeight == style.minHeight &&
                    sibling->style.maxWidth == style.maxWidth &&
                    sibling->style.maxHeight == style.maxHeight &&
                    sibling->style.top == style.top &&
                    sibling->style.right == style.right &&
                    sibling->style.bottom == style.bottom &&
                    sibling->style.left == style.left &&
                    sibling->style.padding == style.padding &&
                    sibling->style.margin == style.margin &&
                    sibling->style.position == style.position &&
                    sibling->style.flexGrow == style.flexGrow &&
                    sibling->style.flexShrink == style.flexShrink &&
                    sibling->style.flexBasis == style.flexBasis &&
                    sibling->style.aspectRatio == style.aspectRatio &&
                    sibling->style.backgroundColor == style.backgroundColor &&
                    sibling->style.color == style.color &&
                    sibling->style.fontSize == style.fontSize &&
                    sibling->style.fontFamily == style.fontFamily &&
                    sibling->inlineProperties.size() == inlineProperties.size() &&
                    sibling->inlinePropertyEpoch == inlinePropertyEpoch) {
                    
                    bool inlinePropsMatch = true;
                    for (size_t i = 0; i < inlineProperties.size(); ++i) {
                        if (inlineProperties[i].name != sibling->inlineProperties[i].name ||
                            inlineProperties[i].value != sibling->inlineProperties[i].value) {
                            inlinePropsMatch = false;
                            break;
                        }
                    }
                    
                    if (inlinePropsMatch) {
                        computedStyle = sibling->computedStyle;
                        lastResolveKey = sibling->lastResolveKey;
                        lastStyleSheetEpoch = sibling->lastStyleSheetEpoch;
                        hasLastResolveKey = sibling->hasLastResolveKey;
                        ancestorH1 = sibling->ancestorH1;
                        ancestorH2 = sibling->ancestorH2;
                        
                        size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
                        if (nextLayoutSignature != layoutSignature) {
                            layoutSignature = nextLayoutSignature;
                            markLayoutDirty();
                        }
                        
                        styleDirty = false;
                        break;
                    }
                }
            }
        }
    }
    if (styleDirty) {
        cachedSelectorType.clear();
        if (parent) {
            uint64_t h1 = parent->ancestorH1;
            uint64_t h2 = parent->ancestorH2;

            auto hashStr = [&](std::string_view sv) {
                for (char c : sv) {
                    h1 ^= static_cast<uint64_t>(c);
                    h1 *= 1099511628211ULL;
                }
                for (char c : sv) {
                    h2 = ((h2 << 5) + h2) + static_cast<uint64_t>(c);
                }
            };

            h1 ^= 0xDDULL; h2 ^= 0xDDULL;
            hashStr(parent->className);
            h1 ^= 0xCCULL; h2 ^= 0xCCULL;
            hashStr(parent->id);
            h1 ^= 0xBBULL; h2 ^= 0xBBULL;
            hashStr(widgetSelectorType(parent));

            ancestorH1 = h1;
            ancestorH2 = h2;
        } else {
            ancestorH1 = 14695981039346656037ULL;
            ancestorH2 = 5381ULL;
        }

        const Style* parentStyle = parent ? &parent->computedStyle : nullptr;
        std::string_view selectorType = widgetSelectorType(this);

        uint64_t h1 = ancestorH1;
        uint64_t h2 = ancestorH2;
        auto hashStr = [&](std::string_view sv) {
            for (char c : sv) {
                h1 ^= static_cast<uint64_t>(c);
                h1 *= 1099511628211ULL;
            }
            for (char c : sv) {
                h2 = ((h2 << 5) + h2) + static_cast<uint64_t>(c);
            }
        };
        hashStr(className);
        h1 ^= 0xFFULL; h2 ^= 0xFFULL;
        hashStr(id);
        h1 ^= 0xEEULL; h2 ^= 0xEEULL;
        hashStr(selectorType);

        if (parentStyle) {
            h1 ^= parentStyle->inheritedHash;
            h2 ^= ~parentStyle->inheritedHash;
        }
        StyleCacheKey currentKey{h1, h2};

        if (hasLastResolveKey && lastResolveKey == currentKey && lastStyleSheetEpoch == sheet.getEpoch()
            && inlineProperties.size() == lastInlinePropertyCount && inlinePropertyEpoch == lastInlinePropertyEpoch) {
            styleDirty = false;
        }

        if (styleDirty) {
            auto getAncestors = [this]() -> const std::vector<CSSSelectorNode>& {
                thread_local std::vector<CSSSelectorNode> t_ancestors;
                t_ancestors.clear();
                size_t ancestorCount = 0;
                for (Widget* node = parent; node; node = node->parent) {
                    ++ancestorCount;
                }
                t_ancestors.reserve(ancestorCount);
                for (Widget* node = parent; node; node = node->parent) {
                    t_ancestors.push_back({node->className, node->id, widgetSelectorType(node), node});
                }
                return t_ancestors;
            };

            computedStyle = sheet.resolveLazy(className, id, selectorType, ancestorH1, ancestorH2, parentStyle, getAncestors, this);
        if (!parent && computedStyle.overflowY == Overflow::Visible) {
            computedStyle.overflowY = Overflow::Auto;
        }
        if (parent) {
            const Style& inherited = parent->computedStyle;
            if (!computedStyle.hasColor) computedStyle.color = inherited.color;
            if (!computedStyle.hasFontSize) computedStyle.fontSize = inherited.fontSize;
            if (!computedStyle.hasFontWeight) computedStyle.fontWeight = inherited.fontWeight;
            if (!computedStyle.hasFontStyle) computedStyle.fontStyle = inherited.fontStyle;
            if (!computedStyle.hasTextAlign) computedStyle.textAlign = inherited.textAlign;
            if (!computedStyle.hasLineHeight) computedStyle.lineHeight = inherited.lineHeight;
            if (!computedStyle.hasFontFamily) computedStyle.fontFamily = inherited.fontFamily;
            if (!computedStyle.hasDirection) computedStyle.direction = inherited.direction;
            if (!computedStyle.hasWritingMode) computedStyle.writingMode = inherited.writingMode;
            if (!computedStyle.hasListStyleType) {
                computedStyle.listStyleType = inherited.listStyleType;
                computedStyle.hasListStyleType = inherited.hasListStyleType;
            }
        }
        if (style.width.isSet()) computedStyle.width = style.width;
        if (style.height.isSet()) computedStyle.height = style.height;
        if (style.minWidth.isSet()) computedStyle.minWidth = style.minWidth;
        if (style.minHeight.isSet()) computedStyle.minHeight = style.minHeight;
        if (style.maxWidth.isSet()) computedStyle.maxWidth = style.maxWidth;
        if (style.maxHeight.isSet()) computedStyle.maxHeight = style.maxHeight;
        if (style.top.isSet()) computedStyle.top = style.top;
        if (style.right.isSet()) computedStyle.right = style.right;
        if (style.bottom.isSet()) computedStyle.bottom = style.bottom;
        if (style.left.isSet()) computedStyle.left = style.left;
        if (style.position != Position::Static) computedStyle.position = style.position;
        if (style.overflow != Overflow::Visible) computedStyle.overflow = style.overflow;
        if (style.overflowX != Overflow::Visible) computedStyle.overflowX = style.overflowX;
        if (style.overflowY != Overflow::Visible) computedStyle.overflowY = style.overflowY;
        if (style.fontSize > 0 && style.fontSize != 14.0f) computedStyle.fontSize = style.fontSize;
        if (style.hasFontStyle) {
            computedStyle.fontStyle = style.fontStyle;
            computedStyle.hasFontStyle = true;
        }
        if (style.padding.top > 0 || style.padding.right > 0 ||
            style.padding.bottom > 0 || style.padding.left > 0)
            computedStyle.padding = style.padding;
        if (style.margin.top > 0 || style.margin.right > 0 ||
            style.margin.bottom > 0 || style.margin.left > 0)
            computedStyle.margin = style.margin;
        if (style.gap > 0) computedStyle.gap = style.gap;
        if (style.rowGap > 0) computedStyle.rowGap = style.rowGap;
        if (style.columnGap > 0) computedStyle.columnGap = style.columnGap;
        if (style.aspectRatio > 0) computedStyle.aspectRatio = style.aspectRatio;
        if (style.hasObjectFit) {
            computedStyle.objectFit = style.objectFit;
            computedStyle.hasObjectFit = true;
        }
        if (style.hasAppearance) {
            computedStyle.appearance = style.appearance;
            computedStyle.hasAppearance = true;
        }
        if (style.hasObjectPosition) {
            computedStyle.objectPosition = style.objectPosition;
            computedStyle.objectPositionOffset = style.objectPositionOffset;
            computedStyle.hasObjectPosition = true;
        }
        if (style.hasVerticalAlign) {
            computedStyle.verticalAlign = style.verticalAlign;
            computedStyle.hasVerticalAlign = true;
        }
        if (style.hasBoxSizing || style.boxSizing != BoxSizing::ContentBox) {
            computedStyle.boxSizing = style.boxSizing;
            computedStyle.hasBoxSizing = true;
        }
        if (style.flexGrow > 0) computedStyle.flexGrow = style.flexGrow;
        if (style.flexBasis.isSet()) computedStyle.flexBasis = style.flexBasis;
        if (style.borderRadius.maxRadius() > 0) computedStyle.borderRadius = style.borderRadius;
        if (style.hasBackdropFilterBlur) {
            computedStyle.backdropFilterBlur = style.backdropFilterBlur;
            computedStyle.hasBackdropFilterBlur = true;
        }
        if (style.backgroundColor.a > 0) computedStyle.backgroundColor = style.backgroundColor;
        if (style.cursor != CursorType::Default) computedStyle.cursor = style.cursor;
        if (style.hasHoverBg) {
            computedStyle.hoverBackgroundColor = style.hoverBackgroundColor;
            computedStyle.hasHoverBg = true;
        }
        if (style.hasHoverColor) {
            computedStyle.hoverColor = style.hoverColor;
            computedStyle.hasHoverColor = true;
        }
        if (style.hasHoverBorder) {
            computedStyle.hoverBorderColor = style.hoverBorderColor;
            computedStyle.hasHoverBorder = true;
        }
        if (style.hoverScale >= 0) computedStyle.hoverScale = style.hoverScale;
        if (style.hoverOpacity >= 0) computedStyle.hoverOpacity = style.hoverOpacity;
        if (style.scale != 1.0f) computedStyle.scale = style.scale;
        for (const auto& prop : inlineProperties) {
            if (prop.name.rfind("--", 0) == 0) {
                bool valid = true;
                std::string value = sheet.resolveValue(prop.value, computedStyle.customProperties, &valid);
                if (!valid) continue;
                computedStyle.customProperties[prop.name] =
                    std::move(value);
            }
        }
        for (const auto& prop : inlineProperties) {
            if (prop.name.rfind("--", 0) == 0) continue;
            bool valid = true;
            std::string value = sheet.resolveValue(prop.value, computedStyle.customProperties, &valid);
            if (!valid) continue;

            std::string lowerValue = value;
            for (char& c : lowerValue) c = (char)std::tolower((unsigned char)c);
            if (lowerValue == "inherit" || lowerValue == "initial" || lowerValue == "unset") {
                Style initialStyle;
                bool inherited = prop.name == "color" || prop.name == "font-size" ||
                                 prop.name == "font-weight" || prop.name == "font-style" ||
                                 prop.name == "font-family" ||
                                 prop.name == "line-height" || prop.name == "text-align";
                const Style& source = (parent && (lowerValue == "inherit" ||
                                      (lowerValue == "unset" && inherited)))
                    ? parent->computedStyle
                    : initialStyle;
                if (prop.name == "all") {
                    auto customProperties = std::move(computedStyle.customProperties);
                    computedStyle = source;
                    computedStyle.customProperties = std::move(customProperties);
                    continue;
                }
                if (prop.name == "color") { computedStyle.color = source.color; computedStyle.hasColor = true; continue; }
                if (prop.name == "font-size") { computedStyle.fontSize = source.fontSize; computedStyle.hasFontSize = true; continue; }
                if (prop.name == "font-weight") { computedStyle.fontWeight = source.fontWeight; computedStyle.hasFontWeight = true; continue; }
                if (prop.name == "font-style") { computedStyle.fontStyle = source.fontStyle; computedStyle.hasFontStyle = true; continue; }
                if (prop.name == "font-family") { computedStyle.fontFamily = source.fontFamily; computedStyle.hasFontFamily = true; continue; }
                if (prop.name == "line-height") { computedStyle.lineHeight = source.lineHeight; computedStyle.hasLineHeight = true; continue; }
                if (prop.name == "text-align") { computedStyle.textAlign = source.textAlign; computedStyle.hasTextAlign = true; continue; }
                if (prop.name == "vertical-align") { computedStyle.verticalAlign = source.verticalAlign; computedStyle.hasVerticalAlign = source.hasVerticalAlign; continue; }
                if (prop.name == "display") { computedStyle.display = source.display; continue; }
                if (prop.name == "position") { computedStyle.position = source.position; continue; }
                if (prop.name == "opacity") { computedStyle.opacity = source.opacity; continue; }
                if (prop.name == "backdrop-filter") {
                    computedStyle.backdropFilterBlur = source.backdropFilterBlur;
                    computedStyle.hasBackdropFilterBlur = source.hasBackdropFilterBlur;
                    continue;
                }
                if (prop.name == "margin") { computedStyle.margin = source.margin; continue; }
                if (prop.name == "padding") { computedStyle.padding = source.padding; continue; }
                if (prop.name == "background" || prop.name == "background-color") {
                    computedStyle.backgroundColor = source.backgroundColor;
                    computedStyle.backgroundGradient = source.backgroundGradient;
                    continue;
                }
                if (prop.name == "border") { computedStyle.border = source.border; continue; }
                if (prop.name == "object-fit") {
                    computedStyle.objectFit = source.objectFit;
                    computedStyle.hasObjectFit = source.hasObjectFit;
                    continue;
                }
                if (prop.name == "object-position") {
                    computedStyle.objectPosition = source.objectPosition;
                    computedStyle.objectPositionOffset = source.objectPositionOffset;
                    computedStyle.hasObjectPosition = source.hasObjectPosition;
                    continue;
                }
            }
            StyleSheet::mergeProperty(computedStyle,
                                      prop.name,
                                      value);
        }

        computedStyle.resolveLogicalProperties();

        // Resolve dynamic color/gradient properties using the active custom properties and parent/viewport styles
        if (!computedStyle.unresolvedColor.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.customProperties, &valid);
            if (valid) {
                computedStyle.color = StyleSheet::parseColor(resolved);
                computedStyle.hasColor = true;
            }
        }
        if (!computedStyle.unresolvedBackgroundColor.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.customProperties, &valid);
            if (valid) {
                computedStyle.backgroundColor = StyleSheet::parseColor(resolved);
            }
        }
        if (!computedStyle.unresolvedBackgroundGradient.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.customProperties, &valid);
            if (valid && resolved.find("linear-gradient") != std::string::npos) {
                computedStyle.backgroundGradient = StyleSheet::parseGradient(resolved);
            }
        }
        if (!computedStyle.unresolvedBorderColor.empty()) {
            bool valid = true;
            std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.customProperties, &valid);
            if (valid) {
                computedStyle.border.color = StyleSheet::parseColor(resolved);
            }
        }

        // Evaluate hover, focus, active custom properties
        // Hover state:
        {
            if (!computedStyle.unresolvedColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.hoverCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.color) {
                        computedStyle.hoverColor = c;
                        computedStyle.hasHoverColor = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.hoverCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.backgroundColor) {
                        computedStyle.hoverBackgroundColor = c;
                        computedStyle.hasHoverBg = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundGradient.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.hoverCustomProperties, &valid);
                if (valid && resolved.find("linear-gradient") != std::string::npos) {
                    computedStyle.hoverBackgroundGradient = StyleSheet::parseGradient(resolved);
                    computedStyle.hasHoverGradient = true;
                }
            }
            if (!computedStyle.unresolvedBorderColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.hoverCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.border.color) {
                        computedStyle.hoverBorderColor = c;
                        computedStyle.hasHoverBorder = true;
                    }
                }
            }
        }

        // Focus state:
        {
            if (!computedStyle.unresolvedColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.focusCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.color) {
                        computedStyle.focusColor = c;
                        computedStyle.hasFocusColor = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.focusCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.backgroundColor) {
                        computedStyle.focusBackgroundColor = c;
                        computedStyle.hasFocusBg = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundGradient.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.focusCustomProperties, &valid);
                if (valid && resolved.find("linear-gradient") != std::string::npos) {
                    computedStyle.focusBackgroundGradient = StyleSheet::parseGradient(resolved);
                    computedStyle.hasFocusGradient = true;
                }
            }
            if (!computedStyle.unresolvedBorderColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.focusCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.border.color) {
                        computedStyle.focusBorderColor = c;
                        computedStyle.hasFocusBorder = true;
                    }
                }
            }
        }

        // Active state:
        {
            if (!computedStyle.unresolvedColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedColor, computedStyle.activeCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.color) {
                        computedStyle.activeColor = c;
                        computedStyle.hasActiveColor = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundColor, computedStyle.activeCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.backgroundColor) {
                        computedStyle.activeBackgroundColor = c;
                        computedStyle.hasActiveBg = true;
                    }
                }
            }
            if (!computedStyle.unresolvedBackgroundGradient.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBackgroundGradient, computedStyle.activeCustomProperties, &valid);
                if (valid && resolved.find("linear-gradient") != std::string::npos) {
                    computedStyle.activeBackgroundGradient = StyleSheet::parseGradient(resolved);
                    computedStyle.hasActiveGradient = true;
                }
            }
            if (!computedStyle.unresolvedBorderColor.empty()) {
                bool valid = true;
                std::string resolved = sheet.resolveValue(computedStyle.unresolvedBorderColor, computedStyle.activeCustomProperties, &valid);
                if (valid) {
                    Color c = StyleSheet::parseColor(resolved);
                    if (c != computedStyle.border.color) {
                        computedStyle.activeBorderColor = c;
                        computedStyle.hasActiveBorder = true;
                    }
                }
            }
        }

        uint64_t oldInheritedHash = computedStyle.inheritedHash;
        computedStyle.inheritedHash = StyleSheet::computeInheritedHash(computedStyle);
        if (computedStyle.inheritedHash != oldInheritedHash) {
            for (auto& child : children) {
                child->styleDirty = true;
                child->markSubtreeStyleDirty();
            }
        }
        
        size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
        if (nextLayoutSignature != layoutSignature) {
            layoutSignature = nextLayoutSignature;
            markLayoutDirty();
        }
        styleDirty = false;
        lastInlinePropertyEpoch = inlinePropertyEpoch;
        lastInlinePropertyCount = inlineProperties.size();
        
        Style beforeStyle = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "before");
        beforeStyle.resolveLogicalProperties();
        if (!beforeStyle.content.empty()) {
            if (!beforePseudoNode) {
                beforePseudoNode = std::make_shared<Text>(beforeStyle.content, "");
                beforePseudoNode->parent = this;
                beforePseudoNode->type = "pseudo-before";
                children.insert(children.begin(), beforePseudoNode);
            } else {
                static_cast<Text*>(beforePseudoNode.get())->content = beforeStyle.content;
            }
            beforePseudoNode->computedStyle = beforeStyle;
            beforePseudoNode->styleDirty = false;
            beforePseudoNode->subtreeStyleDirty = true;
        } else {
            if (beforePseudoNode) {
                children.erase(std::remove(children.begin(), children.end(), beforePseudoNode), children.end());
                beforePseudoNode.reset();
            }
        }

        Style afterStyle = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "after");
        afterStyle.resolveLogicalProperties();
        if (!afterStyle.content.empty()) {
            if (!afterPseudoNode) {
                afterPseudoNode = std::make_shared<Text>(afterStyle.content, "");
                afterPseudoNode->parent = this;
                afterPseudoNode->type = "pseudo-after";
                children.push_back(afterPseudoNode);
            } else {
                static_cast<Text*>(afterPseudoNode.get())->content = afterStyle.content;
            }
            afterPseudoNode->computedStyle = afterStyle;
            afterPseudoNode->styleDirty = false;
            afterPseudoNode->subtreeStyleDirty = true;
        } else {
            if (afterPseudoNode) {
                children.erase(std::remove(children.begin(), children.end(), afterPseudoNode), children.end());
                afterPseudoNode.reset();
            }
        }

        lastResolveKey = currentKey;
        lastStyleSheetEpoch = sheet.getEpoch();
        hasLastResolveKey = true;
        styleDirty = false;
        }
    }

    // Inline C++ style overrides: Programmatic style changes set in C++ should always override the matched stylesheet styles
    if (style.width.isSet()) computedStyle.width = style.width;
    if (style.height.isSet()) computedStyle.height = style.height;
    if (style.minWidth.isSet()) computedStyle.minWidth = style.minWidth;
    if (style.minHeight.isSet()) computedStyle.minHeight = style.minHeight;
    if (style.maxWidth.isSet()) computedStyle.maxWidth = style.maxWidth;
    if (style.maxHeight.isSet()) computedStyle.maxHeight = style.maxHeight;
    if (style.top.isSet()) computedStyle.top = style.top;
    if (style.right.isSet()) computedStyle.right = style.right;
    if (style.bottom.isSet()) computedStyle.bottom = style.bottom;
    if (style.left.isSet()) computedStyle.left = style.left;
    if (style.position != Position::Static) computedStyle.position = style.position;
    if (style.overflow != Overflow::Visible) computedStyle.overflow = style.overflow;
    if (style.overflowX != Overflow::Visible) computedStyle.overflowX = style.overflowX;
    if (style.overflowY != Overflow::Visible) computedStyle.overflowY = style.overflowY;
    if (style.fontSize > 0 && style.fontSize != 14.0f) computedStyle.fontSize = style.fontSize;
    if (style.hasFontStyle) {
        computedStyle.fontStyle = style.fontStyle;
        computedStyle.hasFontStyle = true;
    }
    if (style.padding.top > 0 || style.padding.right > 0 ||
        style.padding.bottom > 0 || style.padding.left > 0)
        computedStyle.padding = style.padding;
    if (style.margin.top > 0 || style.margin.right > 0 ||
        style.margin.bottom > 0 || style.margin.left > 0)
        computedStyle.margin = style.margin;
    if (style.gap > 0) computedStyle.gap = style.gap;
    if (style.rowGap > 0) computedStyle.rowGap = style.rowGap;
    if (style.columnGap > 0) computedStyle.columnGap = style.columnGap;
    if (style.aspectRatio > 0) computedStyle.aspectRatio = style.aspectRatio;
    if (style.hasObjectFit) {
        computedStyle.objectFit = style.objectFit;
        computedStyle.hasObjectFit = true;
    }
    if (style.hasAppearance) {
        computedStyle.appearance = style.appearance;
        computedStyle.hasAppearance = true;
    }
    if (style.hasObjectPosition) {
        computedStyle.objectPosition = style.objectPosition;
        computedStyle.objectPositionOffset = style.objectPositionOffset;
        computedStyle.hasObjectPosition = true;
    }
    if (style.hasVerticalAlign) {
        computedStyle.verticalAlign = style.verticalAlign;
        computedStyle.hasVerticalAlign = true;
    }
    if (style.hasBoxSizing || style.boxSizing != BoxSizing::ContentBox) {
        computedStyle.boxSizing = style.boxSizing;
        computedStyle.hasBoxSizing = true;
    }
    if (style.flexGrow > 0) computedStyle.flexGrow = style.flexGrow;
    if (style.flexBasis.isSet()) computedStyle.flexBasis = style.flexBasis;
    if (style.borderRadius.maxRadius() > 0) computedStyle.borderRadius = style.borderRadius;
    if (style.hasBackdropFilterBlur) {
        computedStyle.backdropFilterBlur = style.backdropFilterBlur;
        computedStyle.hasBackdropFilterBlur = true;
    }
    if (style.backgroundColor.a > 0) computedStyle.backgroundColor = style.backgroundColor;
    if (style.cursor != CursorType::Default) computedStyle.cursor = style.cursor;
    if (style.hasHoverBg) {
        computedStyle.hoverBackgroundColor = style.hoverBackgroundColor;
        computedStyle.hasHoverBg = true;
    }
    if (style.hasHoverColor) {
        computedStyle.hoverColor = style.hoverColor;
        computedStyle.hasHoverColor = true;
    }
    if (style.hasHoverBorder) {
        computedStyle.hoverBorderColor = style.hoverBorderColor;
        computedStyle.hasHoverBorder = true;
    }
    if (style.hoverScale >= 0) computedStyle.hoverScale = style.hoverScale;
    if (style.hoverOpacity >= 0) computedStyle.hoverOpacity = style.hoverOpacity;
    if (style.scale != 1.0f) computedStyle.scale = style.scale;

    size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
    if (nextLayoutSignature != layoutSignature) {
        layoutSignature = nextLayoutSignature;
        markLayoutDirty();
    }

    for (auto& child : children) {
        if (child->subtreeStyleDirty) {
            child->resolveStyles(sheet);
        }
    }
    subtreeStyleDirty = false;
    lifecycleState = WidgetLifecycle::StyleClean;
}

void Widget::prePaint(const PaintProperties& parentProps) {
    paintProperties.translation = parentProps.translation;
    paintProperties.scale = parentProps.scale;
    paintProperties.clipRect = parentProps.clipRect;
    paintProperties.hasClip = parentProps.hasClip;
    paintProperties.opacity = parentProps.opacity * computedStyle.opacity;

    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    if (scrollable) {
        paintProperties.translation.y -= scrollY;
    }

    bool clip = clipsOverflow(computedStyle);
    if (clip) {
        Rect localClip = bounds;
        if (paintProperties.hasClip) {
            float x1 = std::max(paintProperties.clipRect.x, localClip.x);
            float y1 = std::max(paintProperties.clipRect.y, localClip.y);
            float x2 = std::min(paintProperties.clipRect.x + paintProperties.clipRect.w, localClip.x + localClip.w);
            float y2 = std::min(paintProperties.clipRect.y + paintProperties.clipRect.h, localClip.y + localClip.h);
            paintProperties.clipRect = { x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1) };
        } else {
            paintProperties.clipRect = localClip;
            paintProperties.hasClip = true;
        }
    }

    lifecycleState = WidgetLifecycle::PrePaintClean;

    for (auto& child : children) {
        if (child) {
            child->prePaint(paintProperties);
        }
    }
}

void Widget::translateLayout(float dx, float dy) {
    bounds.x += dx;
    bounds.y += dy;
    lastLayoutParentBounds.x += dx;
    lastLayoutParentBounds.y += dy;
    for (auto& child : children) {
        if (child) {
            child->translateLayout(dx, dy);
        }
    }
}
void Widget::layout(const Rect& parentBounds) {
    if (!layoutDirty && lastLayoutParentBounds.w == parentBounds.w && lastLayoutParentBounds.h == parentBounds.h) {
        float dx = parentBounds.x - lastLayoutParentBounds.x;
        float dy = parentBounds.y - lastLayoutParentBounds.y;
        if (dx != 0.0f || dy != 0.0f) {
            translateLayout(dx, dy);
        }
        return;
    }
    if (!layoutDirty && rectEqual(lastLayoutParentBounds, parentBounds)) {
        return;
    }
    lastLayoutParentBounds = parentBounds;
    auto& s = computedStyle;
    if (s.display == Display::None) {
        bounds = {parentBounds.x, parentBounds.y, 0.0f, 0.0f};
        contentHeight = 0.0f;
        layoutDirty = false;
        return;
    }
    float vpW = 1920.0f;
    float vpH = 1080.0f;
    const Widget* r = this;
    while (r->parent) r = r->parent;
    if (r) {
        vpW = r->bounds.w;
        vpH = r->bounds.h;
    }

    bool heightProvidedByParentFlex = consumesParentMainAxisHeight(this, s);
    float x = parentBounds.x + s.margin.left;
    float y = parentBounds.y + s.margin.top;
    float w = s.width.isSet() ? s.width.resolve(parentBounds.w, vpW, vpH) :
              (parentBounds.w < 9999 ? parentBounds.w - s.margin.horizontal() : 0);
    float h = s.height.isSet() ? s.height.resolve(parentBounds.h, vpW, vpH) :
              (parentBounds.h < 9999 ? parentBounds.h - s.margin.vertical() : 0);
    bool widthControlsRatio = s.aspectRatio > 0.0f && s.width.isSet() && !s.height.isSet();
    bool heightControlsRatio = s.aspectRatio > 0.0f && s.height.isSet() && !s.width.isSet();
    if (s.minWidth.isSet()) w = std::max(w, s.minWidth.resolve(parentBounds.w, vpW, vpH));
    if (s.maxWidth.isSet()) w = std::min(w, s.maxWidth.resolve(parentBounds.w, vpW, vpH));
    if (widthControlsRatio) h = w / s.aspectRatio;
    if (s.minHeight.isSet()) h = std::max(h, s.minHeight.resolve(parentBounds.h, vpW, vpH));
    if (s.maxHeight.isSet()) h = std::min(h, s.maxHeight.resolve(parentBounds.h, vpW, vpH));
    if (heightControlsRatio) w = h * s.aspectRatio;
    if (s.hasBoxSizing && s.boxSizing == BoxSizing::ContentBox) {
        if (s.width.isSet()) w += s.padding.horizontal() + usedBorderHorizontal(s);
        if (s.height.isSet()) h += s.padding.vertical() + usedBorderVertical(s);
    }
    bounds = {x, y, w, h};
    if (s.display == Display::Flex) {
        LayoutConstraints constraints;
        constraints.availableWidth = parentBounds.w;
        constraints.availableHeight = parentBounds.h;
        constraints.parentWidth = vpW;
        constraints.parentHeight = vpH;
        constraints.emBase = 16.0f;

        FlexLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(this, constraints);
        bounds.w = res.width;
        bounds.h = res.height;
        contentHeight = res.contentHeight;
    } else if (s.display == Display::Grid) {
        LayoutConstraints constraints;
        constraints.availableWidth = parentBounds.w;
        constraints.availableHeight = parentBounds.h;
        constraints.parentWidth = vpW;
        constraints.parentHeight = vpH;
        constraints.emBase = 16.0f;

        GridLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(this, constraints);
        bounds.w = res.width;
        bounds.h = res.height;
        contentHeight = res.contentHeight;
    } else if (s.display == Display::Table) {
        std::vector<Widget*> rows;
        std::function<void(Widget*)> collectRows = [&](Widget* w) {
            for (auto& child : w->children) {
                if (!child->visible || isDisplayNone(child.get())) continue;
                if (child->computedStyle.display == Display::TableRow) {
                    rows.push_back(child.get());
                } else if (child->computedStyle.display == Display::TableRowGroup ||
                           child->computedStyle.display == Display::TableHeaderGroup ||
                           child->computedStyle.display == Display::TableFooterGroup) {
                    collectRows(child.get());
                }
            }
        };
        collectRows(this);

        std::vector<std::vector<Widget*>> matrix;
        size_t maxCols = 0;
        for (Widget* row : rows) {
            std::vector<Widget*> cells;
            for (auto& child : row->children) {
                if (!child->visible || isDisplayNone(child.get())) continue;
                if (child->computedStyle.display == Display::TableCell) {
                    cells.push_back(child.get());
                }
            }
            maxCols = std::max(maxCols, cells.size());
            matrix.push_back(cells);
        }

        std::vector<float> colWidths(maxCols, 0.0f);
        float availW = bounds.w - s.padding.horizontal();
        for (size_t col = 0; col < maxCols; ++col) {
            float maxCellW = 0.0f;
            for (auto& rowCells : matrix) {
                if (col < rowCells.size()) {
                    Widget* cell = rowCells[col];
                    Rect measureArea = { 0, 0, 10000.0f, 10000.0f };
                    cell->layout(measureArea);
                    maxCellW = std::max(maxCellW, cell->bounds.w);
                }
            }
            colWidths[col] = std::max(5.0f, maxCellW);
        }

        float totalMeasuredW = 0.0f;
        for (float w : colWidths) totalMeasuredW += w;
        if (totalMeasuredW > 0.0f && totalMeasuredW > availW) {
            for (size_t col = 0; col < maxCols; ++col) {
                colWidths[col] = (colWidths[col] / totalMeasuredW) * availW;
            }
        } else if (totalMeasuredW > 0.0f && totalMeasuredW < availW) {
            float extra = (availW - totalMeasuredW) / maxCols;
            for (size_t col = 0; col < maxCols; ++col) {
                colWidths[col] += extra;
            }
        } else if (maxCols > 0) {
            for (size_t col = 0; col < maxCols; ++col) {
                colWidths[col] = availW / maxCols;
            }
        }

        float currentY = bounds.y + s.padding.top;
        float startX = bounds.x + s.padding.left;

        for (size_t r = 0; r < matrix.size(); ++r) {
            Widget* rowWidget = rows[r];
            auto& rowCells = matrix[r];
            float rowH = 0.0f;

            float currentX = startX;
            for (size_t col = 0; col < rowCells.size(); ++col) {
                Widget* cell = rowCells[col];
                float colW = colWidths[col];
                Rect cellArea = { currentX, currentY, colW, 10000.0f };
                cell->layout(cellArea);
                rowH = std::max(rowH, cell->bounds.h);
                currentX += colW;
            }

            currentX = startX;
            for (size_t col = 0; col < rowCells.size(); ++col) {
                Widget* cell = rowCells[col];
                float colW = colWidths[col];
                Rect cellArea = { currentX, currentY, colW, rowH };
                cell->layout(cellArea);
                currentX += colW;
            }

            rowWidget->bounds = { startX, currentY, availW, rowH };
            rowWidget->layoutDirty = false;

            currentY += rowH;
        }

        if (!s.height.isSet() && !heightProvidedByParentFlex && parent != nullptr) {
            bounds.h = std::max(bounds.h, currentY - bounds.y + s.padding.bottom);
        }
        contentHeight = currentY - bounds.y + s.padding.bottom;
    } else {
        float cy = bounds.y + s.padding.top;
        struct ActiveFloat {
            float x;
            float y;
            float w;
            float h;
            bool isLeft;
        };
        std::vector<ActiveFloat> floats;

        for (auto& child : children) {
            if (!child->visible) continue;
            if (isDisplayNone(child.get())) continue;
            if (isOutOfFlow(child.get())) continue;

            auto& cs = child->computedStyle;
            if (cs.cssClear == CSSClear::Left || cs.cssClear == CSSClear::Both) {
                for (const auto& f : floats) {
                    if (f.isLeft) cy = std::max(cy, f.y + f.h);
                }
            }
            if (cs.cssClear == CSSClear::Right || cs.cssClear == CSSClear::Both) {
                for (const auto& f : floats) {
                    if (!f.isLeft) cy = std::max(cy, f.y + f.h);
                }
            }

            if (cs.cssFloat == CSSFloat::Left) {
                float currentLeftX = bounds.x + s.padding.left;
                for (const auto& f : floats) {
                    if (f.isLeft && f.y + f.h > cy && f.y <= cy) {
                        currentLeftX = std::max(currentLeftX, f.x + f.w);
                    }
                }
                float targetW = cs.width.isSet() ? cs.width.resolve(bounds.w, 1920.0f, 1080.0f) : 100.0f;
                Rect childArea = { currentLeftX, cy, targetW, 10000.0f };
                child->layout(childArea);
                floats.push_back({ currentLeftX, cy, child->bounds.w, child->bounds.h, true });
            } else if (cs.cssFloat == CSSFloat::Right) {
                float currentRightX = bounds.x + bounds.w - s.padding.right;
                for (const auto& f : floats) {
                    if (!f.isLeft && f.y + f.h > cy && f.y <= cy) {
                        currentRightX = std::min(currentRightX, f.x);
                    }
                }
                float targetW = cs.width.isSet() ? cs.width.resolve(bounds.w, 1920.0f, 1080.0f) : 100.0f;
                float cellX = currentRightX - targetW;
                Rect childArea = { cellX, cy, targetW, 10000.0f };
                child->layout(childArea);
                floats.push_back({ cellX, cy, child->bounds.w, child->bounds.h, false });
            } else {
                float currentLeftX = bounds.x + s.padding.left;
                float currentRightX = bounds.x + bounds.w - s.padding.right;
                for (const auto& f : floats) {
                    if (f.y + f.h > cy && f.y <= cy) {
                        if (f.isLeft) {
                            currentLeftX = std::max(currentLeftX, f.x + f.w);
                        } else {
                            currentRightX = std::min(currentRightX, f.x);
                        }
                    }
                }
                float availChildW = std::max(0.0f, currentRightX - currentLeftX);
                Rect childArea = {
                    currentLeftX,
                    cy,
                    availChildW,
                    bounds.h > 0 ? bounds.h - s.padding.vertical() : 10000
                };
                child->layout(childArea);
                cy = child->bounds.y + child->bounds.h + cs.margin.bottom;
            }
        }

        float maxFloatY = cy;
        for (const auto& f : floats) {
            maxFloatY = std::max(maxFloatY, f.y + f.h);
        }
        cy = maxFloatY;

        if (!s.height.isSet() && !heightProvidedByParentFlex && !children.empty() && parent != nullptr) {
            bounds.h = std::max(bounds.h, cy - bounds.y + s.padding.bottom);
        }
        contentHeight = cy - bounds.y + s.padding.bottom;
    }
    layoutPositionedChildren();
    layoutDirty = false;
    lifecycleState = WidgetLifecycle::LayoutClean;
}
void Widget::layoutFlexChildren() {
    float vpW = 1920.0f;
    float vpH = 1080.0f;
    const Widget* r = this;
    while (r->parent) r = r->parent;
    if (r) {
        vpW = r->bounds.w;
        vpH = r->bounds.h;
    }
    LayoutConstraints constraints;
    constraints.availableWidth = bounds.w;
    constraints.availableHeight = bounds.h;
    constraints.parentWidth = vpW;
    constraints.parentHeight = vpH;
    constraints.emBase = 16.0f;
    
    FlexLayoutAlgorithm algorithm;
    LayoutResult res = algorithm.layout(this, constraints);
    bounds.w = res.width;
    bounds.h = res.height;
    contentHeight = res.contentHeight;
    return;

    auto& s = computedStyle;
    bool isRow = (s.flexDirection == FlexDirection::Row ||
                  s.flexDirection == FlexDirection::RowReverse);
    float contentX = bounds.x + s.padding.left;
    float contentY = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    float contentH = std::max(0.0f, bounds.h - s.padding.vertical());
    float mainGap = isRow
        ? (s.columnGap > 0.0f ? s.columnGap : s.gap)
        : (s.rowGap > 0.0f ? s.rowGap : s.gap);

    if (s.flexWrap == FlexWrap::NoWrap) {
        int visibleCount = 0;
        float totalFlexGrow = 0;
        float fixedSize = 0;
        float measuredMainStack[64];
        std::vector<float> measuredMainHeap;
        float* measuredMain = measuredMainStack;
        if (children.size() > 64) {
            measuredMainHeap.resize(children.size(), 0.0f);
            measuredMain = measuredMainHeap.data();
        } else {
            std::fill(measuredMainStack, measuredMainStack + children.size(), 0.0f);
        }
        if (!isRow) {
            for (size_t i = 0; i < children.size(); i++) {
                auto& child = children[i];
                if (!child->visible) continue;
                if (isDisplayNone(child.get())) continue;
                if (isOutOfFlow(child.get())) continue;
                visibleCount++;
                auto& cs = child->computedStyle;
                totalFlexGrow += cs.flexGrow;
                float childW = cs.width.isSet() ? cs.width.resolve(contentW) : contentW;
                if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                    measuredMain[i] = cs.flexBasis.resolve(contentH);
                    fixedSize += measuredMain[i] + cs.margin.vertical();
                } else if (cs.height.isSet()) {
                    measuredMain[i] = cs.height.resolve(contentH);
                    fixedSize += measuredMain[i] + cs.margin.vertical();
                } else if (cs.flexGrow <= 0) {
                    Rect measureArea = {contentX, contentY, childW, 0};
                    child->layout(measureArea);
                    measuredMain[i] = child->bounds.h;
                    fixedSize += measuredMain[i] + cs.margin.vertical();
                }
            }
        } else {
            for (size_t i = 0; i < children.size(); i++) {
                auto& child = children[i];
                if (!child->visible) continue;
                if (isDisplayNone(child.get())) continue;
                if (isOutOfFlow(child.get())) continue;
                visibleCount++;
                auto& cs = child->computedStyle;
                totalFlexGrow += cs.flexGrow;
                if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                    measuredMain[i] = cs.flexBasis.resolve(contentW);
                    fixedSize += measuredMain[i] + cs.margin.horizontal();
                } else if (cs.width.isSet()) {
                    measuredMain[i] = cs.width.resolve(contentW);
                    fixedSize += measuredMain[i] + cs.margin.horizontal();
                } else if (cs.flexGrow <= 0) {
                    measuredMain[i] = 100;
                    fixedSize += measuredMain[i] + cs.margin.horizontal();
                }
            }
        }
        float totalGap = mainGap * std::max(0, visibleCount - 1);
        float availableSpace = (isRow ? contentW : contentH) - fixedSize - totalGap;
        availableSpace = std::max(0.0f, availableSpace);
        float mainAxisAvailable = availableSpace;
        if (totalFlexGrow > 0) mainAxisAvailable = 0;
        float startOffset = 0;
        float gapOffset = mainGap;
        if (mainAxisAvailable > 0) {
            switch (s.justifyContent) {
                case JustifyContent::FlexStart:
                    break;
                case JustifyContent::FlexEnd:
                    startOffset = mainAxisAvailable;
                    break;
                case JustifyContent::Center:
                    startOffset = mainAxisAvailable / 2.0f;
                    break;
                case JustifyContent::SpaceBetween:
                    if (visibleCount > 1) {
                        gapOffset += mainAxisAvailable / (visibleCount - 1);
                        startOffset = 0;
                    }
                    break;
                case JustifyContent::SpaceAround:
                    if (visibleCount > 0) {
                        float space = mainAxisAvailable / visibleCount;
                        startOffset = space / 2.0f;
                        gapOffset += space;
                    }
                    break;
                case JustifyContent::SpaceEvenly:
                    if (visibleCount > 0) {
                        float space = mainAxisAvailable / (visibleCount + 1);
                        startOffset = space;
                        gapOffset += space;
                    }
                    break;
            }
        }
        bool rtlRow = isRow && s.direction == Direction::Rtl;
        float cursor;
        if (rtlRow) {
            cursor = contentX + contentW - startOffset;
        } else {
            cursor = (isRow ? contentX : contentY) + startOffset;
        }
        float maxCross = 0;
        float contentEnd = cursor;
        int laidOut = 0;
        const bool parallelLayout = children.size() > 32;
        std::vector<std::future<void>> futures;
        if (parallelLayout) futures.reserve(children.size());
        struct ChildLayoutTask {
            Widget* widget;
            Rect area;
        };
        std::vector<ChildLayoutTask> tasks;
        if (parallelLayout) tasks.reserve(children.size());
        for (size_t i = 0; i < children.size(); i++) {
            auto& child = children[i];
            if (!child->visible) continue;
            if (isDisplayNone(child.get())) continue;
            if (isOutOfFlow(child.get())) continue;
            auto& cs = child->computedStyle;
            float nextGap = (++laidOut < visibleCount) ? gapOffset : 0.0f;
            float childW, childH;
            if (isRow) {
                if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                    childW = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
                } else {
                    childW = measuredMain[i];
                }
                childH = cs.height.isSet() ? cs.height.resolve(contentH) : contentH;
                if (childH <= 0 && !cs.height.isSet()) childH = 0;
                AlignItems effectiveAlign = s.alignItems;
                if (cs.alignSelf != AlignSelf::Auto) {
                    switch (cs.alignSelf) {
                        case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                        case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                        case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                        case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                        case AlignSelf::Baseline: effectiveAlign = AlignItems::Baseline; break;
                        default: break;
                    }
                }
                float cy = contentY;
                if (effectiveAlign == AlignItems::Center && contentH > childH) {
                    cy = contentY + (contentH - childH) / 2;
                } else if (effectiveAlign == AlignItems::FlexEnd && contentH > childH) {
                    cy = contentY + contentH - childH;
                }
                Rect childArea;
                if (rtlRow) {
                    childArea = {cursor - childW - cs.margin.right, cy + cs.margin.top,
                                 std::max(0.0f, childW), std::max(0.0f, childH)};
                } else {
                    childArea = {cursor + cs.margin.left, cy + cs.margin.top,
                                 std::max(0.0f, childW), std::max(0.0f, childH)};
                }
                if (parallelLayout) {
                    tasks.push_back({child.get(), childArea});
                    if (rtlRow)
                        cursor -= childW + cs.margin.horizontal() + nextGap;
                    else
                        cursor += childW + cs.margin.horizontal() + nextGap;
                } else {
                    child->layout(childArea);
                    if (!cs.height.isSet() && !clipsOverflow(cs) &&
                        child->contentHeight > child->bounds.h) {
                        child->bounds.h = child->contentHeight;
                    }
                    if (rtlRow)
                        cursor -= child->bounds.w + cs.margin.horizontal() + nextGap;
                    else
                        cursor += child->bounds.w + cs.margin.horizontal() + nextGap;
                    maxCross = std::max(maxCross, child->bounds.h + cs.margin.vertical());
                }
                contentEnd = rtlRow ? std::min(contentEnd, cursor + nextGap)
                                    : std::max(contentEnd, cursor - nextGap);
            } else {
                childW = cs.width.isSet() ? cs.width.resolve(contentW) : contentW;
                if (cs.flexGrow > 0 && totalFlexGrow > 0) {
                    childH = measuredMain[i] + availableSpace * (cs.flexGrow / totalFlexGrow);
                } else {
                    childH = measuredMain[i];
                }
                AlignItems effectiveAlign = s.alignItems;
                if (cs.alignSelf != AlignSelf::Auto) {
                    switch (cs.alignSelf) {
                        case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                        case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                        case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                        case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                        case AlignSelf::Baseline: effectiveAlign = AlignItems::Baseline; break;
                        default: break;
                    }
                }
                float cx = contentX;
                if (effectiveAlign == AlignItems::Center && contentW > childW) {
                    cx = contentX + (contentW - childW) / 2;
                } else if (effectiveAlign == AlignItems::FlexEnd && contentW > childW) {
                    cx = contentX + contentW - childW;
                }
                Rect childArea = {cx + cs.margin.left, cursor + cs.margin.top,
                                  std::max(0.0f, childW), std::max(0.0f, childH)};
                if (parallelLayout) {
                    tasks.push_back({child.get(), childArea});
                    cursor += childH + cs.margin.vertical() + nextGap;
                } else {
                    child->layout(childArea);
                    cursor += child->bounds.h + cs.margin.vertical() + nextGap;
                    maxCross = std::max(maxCross, child->bounds.w + cs.margin.horizontal());
                }
                contentEnd = cursor;
            }
        }
        if (parallelLayout) {
            for (auto& task : tasks) {
                futures.push_back(getLayoutThreadPool().enqueue([task]() {
                    task.widget->layout(task.area);
                    if (!task.widget->computedStyle.height.isSet() &&
                        !clipsOverflow(task.widget->computedStyle) &&
                        task.widget->contentHeight > task.widget->bounds.h) {
                        task.widget->bounds.h = task.widget->contentHeight;
                    }
                }));
            }
            for (auto& f : futures) f.get();
            for (auto& child : children) {
                if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                maxCross = std::max(maxCross, (isRow ? child->bounds.h : child->bounds.w) +
                                              (isRow ? child->computedStyle.margin.vertical() : child->computedStyle.margin.horizontal()));
            }
        }
        if (!s.height.isSet() && !isRow && bounds.h <= s.padding.vertical()) {
            bounds.h = (contentEnd - contentY) + s.padding.vertical();
        }
        if (!s.height.isSet() && isRow && bounds.h <= s.padding.vertical() && !children.empty()) {
            bounds.h = maxCross + s.padding.vertical();
        }
        contentHeight = isRow ?
            maxCross + s.padding.vertical() :
            std::max(0.0f, contentEnd - bounds.y + s.padding.bottom);
        if (!s.height.isSet() && !consumesParentMainAxisHeight(this, s)) {
            bounds.h = std::max(bounds.h, contentHeight);
        }
    } else {
        float crossGap = isRow
            ? (s.rowGap > 0.0f ? s.rowGap : s.gap)
            : (s.columnGap > 0.0f ? s.columnGap : s.gap);

        struct FlexItem {
            Widget* widget;
            float mainSize = 0.0f;
            float crossSize = 0.0f;
            float flexGrow = 0.0f;
            float flexShrink = 0.0f;
            float mainMargin = 0.0f;
            float crossMargin = 0.0f;
        };
        std::vector<FlexItem> items;
        items.reserve(children.size());

        if (isRow) {
            for (auto& child : children) {
                if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                auto& cs = child->computedStyle;
                FlexItem item;
                item.widget = child.get();
                item.flexGrow = cs.flexGrow;
                item.flexShrink = cs.flexShrink;
                item.mainMargin = cs.margin.horizontal();
                item.crossMargin = cs.margin.vertical();
                if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                    item.mainSize = cs.flexBasis.resolve(contentW);
                } else if (cs.width.isSet()) {
                    item.mainSize = cs.width.resolve(contentW);
                } else if (cs.flexGrow <= 0) {
                    item.mainSize = 100.0f;
                }
                if (cs.height.isSet()) {
                    item.crossSize = cs.height.resolve(contentH);
                } else {
                    child->layout({contentX, contentY, item.mainSize, 0});
                    item.crossSize = child->bounds.h;
                }
                items.push_back(item);
            }
        } else {
            for (auto& child : children) {
                if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;
                auto& cs = child->computedStyle;
                FlexItem item;
                item.widget = child.get();
                item.flexGrow = cs.flexGrow;
                item.flexShrink = cs.flexShrink;
                item.mainMargin = cs.margin.vertical();
                item.crossMargin = cs.margin.horizontal();
                float childW = cs.width.isSet() ? cs.width.resolve(contentW) : contentW;
                if (cs.flexBasis.isSet() && !cs.flexBasis.isAuto()) {
                    item.mainSize = cs.flexBasis.resolve(contentH);
                } else if (cs.height.isSet()) {
                    item.mainSize = cs.height.resolve(contentH);
                } else if (cs.flexGrow <= 0) {
                    child->layout({contentX, contentY, childW, 0});
                    item.mainSize = child->bounds.h;
                }
                item.crossSize = childW;
                items.push_back(item);
            }
        }

        struct FlexLine {
            std::vector<FlexItem> items;
            float mainSize = 0.0f;
            float crossSize = 0.0f;
            float totalFlexGrow = 0.0f;
            float fixedSize = 0.0f;
        };
        std::vector<FlexLine> lines;
        if (!items.empty()) {
            FlexLine currentLine;
            float currentLineMain = 0.0f;
            float maxLineLimit = isRow ? contentW : contentH;

            for (const auto& item : items) {
                float itemMainOuter = item.mainSize + item.mainMargin;
                float gapValue = currentLine.items.empty() ? 0.0f : mainGap;
                if (!currentLine.items.empty() && currentLineMain + gapValue + itemMainOuter > maxLineLimit) {
                    lines.push_back(currentLine);
                    currentLine = FlexLine();
                    currentLineMain = 0.0f;
                    gapValue = 0.0f;
                }
                currentLine.items.push_back(item);
                currentLine.totalFlexGrow += item.flexGrow;
                currentLine.fixedSize += item.mainSize + item.mainMargin;
                currentLine.crossSize = std::max(currentLine.crossSize, item.crossSize + item.crossMargin);
                currentLineMain += gapValue + itemMainOuter;
                currentLine.mainSize = currentLineMain - item.mainMargin;
            }
            if (!currentLine.items.empty()) {
                lines.push_back(currentLine);
            }
        }

        if (s.flexWrap == FlexWrap::WrapReverse) {
            std::reverse(lines.begin(), lines.end());
        }

        float lineCrossStart = isRow ? contentY : contentX;
        float containerMainLimit = isRow ? contentW : contentH;

        for (auto& line : lines) {
            int lineVisibleCount = static_cast<int>(line.items.size());
            float lineGapTotal = mainGap * std::max(0, lineVisibleCount - 1);
            float availableSpace = containerMainLimit - line.fixedSize - lineGapTotal;
            availableSpace = std::max(0.0f, availableSpace);

            float mainAxisAvailable = availableSpace;
            if (line.totalFlexGrow > 0) mainAxisAvailable = 0;
            float startOffset = 0;
            float gapOffset = mainGap;
            if (mainAxisAvailable > 0) {
                switch (s.justifyContent) {
                    case JustifyContent::FlexStart: break;
                    case JustifyContent::FlexEnd: startOffset = mainAxisAvailable; break;
                    case JustifyContent::Center: startOffset = mainAxisAvailable / 2.0f; break;
                    case JustifyContent::SpaceBetween:
                        if (lineVisibleCount > 1) {
                            gapOffset += mainAxisAvailable / (lineVisibleCount - 1);
                            startOffset = 0;
                        }
                        break;
                    case JustifyContent::SpaceAround:
                        if (lineVisibleCount > 0) {
                            float space = mainAxisAvailable / lineVisibleCount;
                            startOffset = space / 2.0f;
                            gapOffset += space;
                        }
                        break;
                    case JustifyContent::SpaceEvenly:
                        if (lineVisibleCount > 0) {
                            float space = mainAxisAvailable / (lineVisibleCount + 1);
                            startOffset = space;
                            gapOffset += space;
                        }
                        break;
                }
            }

            bool rtlRow = isRow && s.direction == Direction::Rtl;
            float cursor = rtlRow
                ? (contentX + contentW - startOffset)
                : ((isRow ? contentX : contentY) + startOffset);

            int laidOut = 0;
            for (auto& item : line.items) {
                auto* child = item.widget;
                auto& cs = child->computedStyle;
                float nextGap = (++laidOut < lineVisibleCount) ? gapOffset : 0.0f;

                AlignItems effectiveAlign = s.alignItems;
                if (cs.alignSelf != AlignSelf::Auto) {
                    switch (cs.alignSelf) {
                        case AlignSelf::FlexStart: effectiveAlign = AlignItems::FlexStart; break;
                        case AlignSelf::FlexEnd: effectiveAlign = AlignItems::FlexEnd; break;
                        case AlignSelf::Center: effectiveAlign = AlignItems::Center; break;
                        case AlignSelf::Stretch: effectiveAlign = AlignItems::Stretch; break;
                        case AlignSelf::Baseline: effectiveAlign = AlignItems::Baseline; break;
                        default: break;
                    }
                }

                float childW, childH;
                if (isRow) {
                    if (item.flexGrow > 0 && line.totalFlexGrow > 0) {
                        childW = item.mainSize + availableSpace * (item.flexGrow / line.totalFlexGrow);
                    } else {
                        childW = item.mainSize;
                    }
                    childH = cs.height.isSet() ? cs.height.resolve(contentH) : line.crossSize - cs.margin.vertical();
                    if (childH <= 0 && !cs.height.isSet()) childH = 0;

                    float cy = lineCrossStart;
                    float crossLimit = line.crossSize - cs.margin.vertical();
                    if (effectiveAlign == AlignItems::Center && crossLimit > childH) {
                        cy = lineCrossStart + (crossLimit - childH) / 2;
                    } else if (effectiveAlign == AlignItems::FlexEnd && crossLimit > childH) {
                        cy = lineCrossStart + crossLimit - childH;
                    }

                    Rect childArea;
                    if (rtlRow) {
                        childArea = {cursor - childW - cs.margin.right, cy + cs.margin.top,
                                     std::max(0.0f, childW), std::max(0.0f, childH)};
                        cursor -= childW + cs.margin.horizontal() + nextGap;
                    } else {
                        childArea = {cursor + cs.margin.left, cy + cs.margin.top,
                                     std::max(0.0f, childW), std::max(0.0f, childH)};
                        cursor += childW + cs.margin.horizontal() + nextGap;
                    }
                    child->layout(childArea);
                    if (!cs.height.isSet() && !clipsOverflow(cs) && child->contentHeight > child->bounds.h) {
                        child->bounds.h = child->contentHeight;
                    }
                } else {
                    childW = cs.width.isSet() ? cs.width.resolve(contentW) : line.crossSize - cs.margin.horizontal();
                    if (item.flexGrow > 0 && line.totalFlexGrow > 0) {
                        childH = item.mainSize + availableSpace * (item.flexGrow / line.totalFlexGrow);
                    } else {
                        childH = item.mainSize;
                    }

                    float cx = lineCrossStart;
                    float crossLimit = line.crossSize - cs.margin.horizontal();
                    if (effectiveAlign == AlignItems::Center && crossLimit > childW) {
                        cx = lineCrossStart + (crossLimit - childW) / 2;
                    } else if (effectiveAlign == AlignItems::FlexEnd && crossLimit > childW) {
                        cx = lineCrossStart + crossLimit - childW;
                    }

                    Rect childArea = {cx + cs.margin.left, cursor + cs.margin.top,
                                      std::max(0.0f, childW), std::max(0.0f, childH)};
                    child->layout(childArea);
                    cursor += childH + cs.margin.vertical() + nextGap;
                }
            }
            lineCrossStart += line.crossSize + crossGap;
        }

        if (isRow) {
            contentHeight = (lineCrossStart - contentY) + s.padding.vertical();
            if (!lines.empty()) {
                contentHeight -= crossGap;
            }
        } else {
            float maxLineMain = 0.0f;
            for (const auto& line : lines) {
                float lineGapsSum = mainGap * std::max<int>(0, static_cast<int>(line.items.size()) - 1);
                maxLineMain = std::max(maxLineMain, line.mainSize + lineGapsSum);
            }
            contentHeight = maxLineMain + s.padding.vertical();
        }

        if (!s.height.isSet() && !consumesParentMainAxisHeight(this, s) && parent != nullptr) {
            bounds.h = std::max(bounds.h, contentHeight);
        }
    }
}
void Widget::layoutPositionedChildren() {
    auto& s = computedStyle;
    float contentX = bounds.x + s.padding.left;
    float contentY = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    float contentH = std::max(0.0f, bounds.h - s.padding.vertical());

    float vpW = 1920.0f;
    float vpH = 1080.0f;
    const Widget* r = this;
    while (r->parent) r = r->parent;
    if (r) {
        vpW = r->bounds.w;
        vpH = r->bounds.h;
    }

    for (auto& child : children) {
        if (!child->visible || isDisplayNone(child.get()) || !isOutOfFlow(child.get())) continue;
        
        float cx = contentX;
        float cy = contentY;
        float cw = contentW;
        float ch = contentH;
        
        if (child->computedStyle.position == Position::Fixed) {
            Widget* root = this;
            while (root->parent) {
                root = root->parent;
            }
            cx = root->bounds.x;
            cy = root->bounds.y;
            cw = root->bounds.w;
            ch = root->bounds.h;
        }
        
        auto& cs = child->computedStyle;
        bool hasLeft = cs.left.isSet();
        bool hasRight = cs.right.isSet();
        bool hasTop = cs.top.isSet();
        bool hasBottom = cs.bottom.isSet();
        float left = hasLeft ? cs.left.resolve(cw, vpW, vpH) : 0.0f;
        float right = hasRight ? cs.right.resolve(cw, vpW, vpH) : 0.0f;
        float top = hasTop ? cs.top.resolve(ch, vpW, vpH) : 0.0f;
        float bottom = hasBottom ? cs.bottom.resolve(ch, vpW, vpH) : 0.0f;
        float childW = cs.width.isSet() ? cs.width.resolve(cw, vpW, vpH) :
            (hasLeft && hasRight ? cw - left - right - cs.margin.horizontal()
                                 : cw - cs.margin.horizontal());
        float childH = cs.height.isSet() ? cs.height.resolve(ch, vpW, vpH) :
            (hasTop && hasBottom ? ch - top - bottom - cs.margin.vertical() : 0.0f);
        float childX = hasLeft ? cx + left :
            (hasRight ? cx + cw - right - std::max(0.0f, childW) - cs.margin.horizontal()
                      : cx);
        float childY = hasTop ? cy + top :
            (hasBottom && childH > 0.0f
                ? cy + ch - bottom - std::max(0.0f, childH) - cs.margin.vertical()
                : cy);
        Rect childArea = {
            childX + cs.margin.left,
            childY + cs.margin.top,
            std::max(0.0f, childW),
            std::max(0.0f, childH)
        };
        child->layout(childArea);
        if (!hasTop && hasBottom && !cs.height.isSet()) {
            childY = cy + ch - bottom - child->bounds.h - cs.margin.bottom;
            childArea.y = childY + cs.margin.top;
            child->layout(childArea);
        }
        if (!hasLeft && hasRight && !cs.width.isSet()) {
            childX = cx + cw - right - child->bounds.w - cs.margin.right;
            childArea.x = childX + cs.margin.left;
            child->layout(childArea);
        }
    }
}
float Widget::maxScrollY() const {
    return std::max(0.0f, contentHeight - bounds.h);
}
bool Widget::getScrollBarRects(Rect& track, Rect& thumb) const {
    float maxScroll = maxScrollY();
    if (!scrollsOverflowY(computedStyle, contentHeight, bounds.h) || maxScroll <= 1.0f) {
        track = {};
        thumb = {};
        return false;
    }
    float trackW = 14.0f;
    float inset = 2.0f;
    float trackH = std::max(24.0f, bounds.h - inset * 2.0f);
    float visibleRatio = bounds.h / std::max(contentHeight, bounds.h);
    float thumbH = std::clamp(trackH * visibleRatio, 32.0f, trackH);
    float thumbTravel = std::max(0.0f, trackH - thumbH);
    float thumbY = bounds.y + inset + thumbTravel * (scrollY / maxScroll);
    track = {bounds.x + bounds.w - trackW, bounds.y + inset, trackW, trackH};
    thumb = {bounds.x + bounds.w - 12.0f, thumbY, 10.0f, thumbH};
    return true;
}
void Widget::clampScroll() {
    float maxScroll = maxScrollY();
    float clampedTarget = std::clamp(targetScrollY, 0.0f, maxScroll);
    float clampedScroll = std::clamp(scrollY, 0.0f, maxScroll);
    if (clampedTarget != targetScrollY || clampedScroll != scrollY) {
        scrollVelocity = 0.0f;
    }
    targetScrollY = clampedTarget;
    scrollY = clampedScroll;
}
bool Widget::hasActiveAnimations() const {
    if (CompositorEngine::instance().hasAnimations(reinterpret_cast<uintptr_t>(this))) {
        return true;
    }
    if (std::abs(scrollY - targetScrollY) > 0.1f || std::abs(scrollVelocity) > 0.1f) {
        return true;
    }
    for (auto& child : children) {
        if (child && child->hasActiveAnimations()) return true;
    }
    return false;
}
void Widget::resetTransientMotion() {
    hoverVelocity = 0.0f;
    scrollVelocity = 0.0f;
    targetScrollY = scrollY;
    clampScroll();
    scrollbarDragging = false;
    scrollbarHovered = false;
    pressed = false;
    for (auto& child : children) {
        if (child) child->resetTransientMotion();
    }
}
void Widget::update(const InputState& input) {
    if (!visible || computedStyle.display == Display::None ||
        computedStyle.visibility != Visibility::Visible) {
        hovered = false;
        pressed = false;
        return;
    }
    float dt = std::clamp(input.deltaTime, 0.001f, 0.1f);
    bool oldHovered = hovered;
    hovered = bounds.contains(input.mousePos);
    if (hovered != oldHovered) {
        float from = hoverAnim;
        float to = hovered ? 1.0f : 0.0f;
        CompositorEngine::instance().animatePropertyFloat(
            reinterpret_cast<uintptr_t>(this), "hoverAnim",
            from, to, computedStyle.transitionDuration,
            TimingFunctionType::Spring,
            computedStyle.springStiffness, computedStyle.springDamping
        );
    }

    float compositedHoverAnim = 0.0f;
    if (CompositorEngine::instance().getAnimatedFloat(reinterpret_cast<uintptr_t>(this), "hoverAnim", compositedHoverAnim)) {
        hoverAnim = compositedHoverAnim;
    } else {
        hoverAnim = hovered ? 1.0f : 0.0f;
    }

    if (input.mouseClicked[0]) {
        if (hovered && (type == "button" || computedStyle.cursor == CursorType::Pointer)) {
            focused = true;
        } else if (focused) {
            focused = false;
        }
    }
    pressed = hovered && input.mouseDown[0];
    int widgetKeyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardActivate = focused && type == "button" &&
        (widgetKeyCode == 0x0D || widgetKeyCode == 0x20) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    if (keyboardActivate) {
        pressed = true;
        Event clickEv;
        clickEv.type = "click";
        clickEv.target = this;
        dispatchEvent(clickEv);
    }
    float currentScale = computedStyle.scale;
    if (computedStyle.hoverScale >= 0) {
        currentScale = computedStyle.scale + (computedStyle.hoverScale - computedStyle.scale) * hoverAnim;
    }
    if (focused && computedStyle.focusScale >= 0) {
        currentScale = computedStyle.focusScale;
    }
    if (pressed && computedStyle.activeScale >= 0) {
        currentScale = computedStyle.activeScale;
    }
    renderScale = currentScale;
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        clampScroll();
        Rect track, thumb;
        bool hasScrollbar = getScrollBarRects(track, thumb);
        scrollbarHovered = hasScrollbar && hovered &&
            (track.contains(input.mousePos) || thumb.contains(input.mousePos));
        if (hasScrollbar && hovered && input.mouseClicked[0]) {
            if (thumb.contains(input.mousePos)) {
                scrollbarDragging = true;
                scrollbarDragOffset = input.mousePos.y - thumb.y;
                scrollVelocity = 0.0f;
            } else if (track.contains(input.mousePos)) {
                float maxScroll = maxScrollY();
                float travel = std::max(1.0f, track.h - thumb.h);
                float requestedY = input.mousePos.y - track.y - thumb.h * 0.5f;
                targetScrollY = std::clamp((requestedY / travel) * maxScroll, 0.0f, maxScroll);
                scrollY = targetScrollY;
                scrollVelocity = 0.0f;
                scrollbarDragging = true;
                scrollbarDragOffset = thumb.h * 0.5f;
            }
        }
        if (scrollbarDragging) {
            if (input.mouseDown[0]) {
                float maxScroll = maxScrollY();
                float travel = std::max(1.0f, track.h - thumb.h);
                float requestedY = input.mousePos.y - track.y - scrollbarDragOffset;
                targetScrollY = std::clamp((requestedY / travel) * maxScroll, 0.0f, maxScroll);
                scrollY = targetScrollY;
                scrollVelocity = 0.0f;
            } else {
                scrollbarDragging = false;
            }
        }
        if (hovered && !scrollbarDragging && input.scroll.y != 0) {
            targetScrollY -= input.scroll.y * 72.0f;
            clampScroll();
        }
        int scrollKeyCode = normalizeTextEditingKey(input.keyCode);
        if (hovered && !scrollbarDragging && scrollKeyCode != 0) {
            float maxScroll = maxScrollY();
            switch (scrollKeyCode) {
            case 0x26:
                targetScrollY -= 48.0f;
                break;
            case 0x28:
                targetScrollY += 48.0f;
                break;
            case 0x21:
                targetScrollY -= bounds.h * 0.88f;
                break;
            case 0x22:
                targetScrollY += bounds.h * 0.88f;
                break;
            case 0x24:
                targetScrollY = 0.0f;
                break;
            case 0x23:
                targetScrollY = maxScroll;
                break;
            default:
                break;
            }
            clampScroll();
        }
        if (scrollY != targetScrollY || scrollVelocity != 0.0f) {
            float previousScroll = scrollY;
            float scrollBlend = 1.0f - std::exp(-dt * 22.0f);
            scrollY += (targetScrollY - scrollY) * scrollBlend;
            scrollVelocity = (scrollY - previousScroll) / std::max(dt, 0.001f);
            if (std::abs(scrollY - targetScrollY) < 0.08f || std::abs(scrollVelocity) < 0.02f) {
                scrollY = targetScrollY;
                scrollVelocity = 0.0f;
            }
            clampScroll();
        }
    } else {
        scrollbarHovered = false;
        scrollbarDragging = false;
    }
    InputState childInput = input;
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        if (scrollbarHovered || scrollbarDragging) {
            childInput.mouseClicked[0] = false;
            childInput.mouseReleased[0] = false;
            childInput.scroll = {0, 0};
        }
        childInput.mousePos.y += scrollY;
    }
    Rect visibleContent = bounds;
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        visibleContent.y += scrollY;
    }
    size_t startIndex = 0;
    size_t endIndex = children.size();
    const bool isColumn = (computedStyle.flexDirection == FlexDirection::Column);
    if (children.size() > 256 && isColumn &&
        scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        auto itStart = std::lower_bound(children.begin(), children.end(), visibleContent.y - 128.0f,
            [](const std::shared_ptr<Widget>& w, float y) {
                return w->bounds.y + w->bounds.h < y;
            });
        startIndex = std::distance(children.begin(), itStart);
        auto itEnd = std::upper_bound(itStart, children.end(), visibleContent.y + visibleContent.h + 128.0f,
            [](float y, const std::shared_ptr<Widget>& w) {
                return y < w->bounds.y;
            });
        endIndex = std::distance(children.begin(), itEnd);
    }
    for (size_t i = 0; i < children.size(); i++) {
        auto& child = children[i];
        if (!child->visible || isDisplayNone(child.get())) continue;
        if (child->computedStyle.position == Position::Fixed) continue;
        if (i < startIndex || i >= endIndex) {
            if (!child->focused && !child->scrollbarDragging) {
                child->hovered = false;
                child->pressed = false;
                continue;
            }
        }
        if (scrollsOverflowY(computedStyle, contentHeight, bounds.h) &&
            !rectIntersects(child->bounds, visibleContent, 128.0f) &&
            !child->focused && !child->scrollbarDragging) {
            child->hovered = false;
            child->pressed = false;
            continue;
        }
        child->update(childInput);
    }
}
CursorType Widget::cursorAt(Vec2 point) const {
    if (!canHitTestWidget(this)) {
        return CursorType::Default;
    }
    if (scrollbarDragging) {
        return CursorType::Pointer;
    }
    if (!bounds.contains(point)) {
        return CursorType::Default;
    }
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        Rect track, thumb;
        if (getScrollBarRects(track, thumb) &&
            (track.contains(point) || thumb.contains(point) || scrollbarDragging)) {
            return CursorType::Pointer;
        }
    }
    Vec2 childPoint = point;
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        childPoint.y += scrollY;
    }
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->computedStyle.position == Position::Fixed) continue;
        CursorType childCursor = (*it)->cursorAt(childPoint);
        if (childCursor != CursorType::Default) return childCursor;
    }
    return computedStyle.cursor;
}
Widget* Widget::hitTest(Vec2 point, bool interactiveOnly) {
    if (!canHitTestWidget(this) || !bounds.contains(point)) {
        return nullptr;
    }
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        Rect track, thumb;
        if (getScrollBarRects(track, thumb) && (track.contains(point) || thumb.contains(point))) {
            return this;
        }
    }
    Vec2 childPoint = point;
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        childPoint.y += scrollY;
    }
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->computedStyle.position == Position::Fixed) continue;
        if (Widget* child = (*it)->hitTest(childPoint, interactiveOnly)) {
            return child;
        }
    }
    if (!interactiveOnly || type == "button" || computedStyle.cursor == CursorType::Pointer || onClick) {
        return this;
    }
    return nullptr;
}
static int getListItemIndex(const Widget* widget) {
    if (!widget || !widget->parent) return 1;
    int index = 1;
    for (const auto& child : widget->parent->children) {
        if (child.get() == widget) {
            break;
        }
        if (child->computedStyle.display == Display::ListItem) {
            index++;
        }
    }
    return index;
}

static std::string toRoman(int val, bool upper) {
    if (val <= 0) return std::to_string(val);
    struct RomanMapping { int value; const char* symbol; };
    const RomanMapping mapping[] = {
        {1000, upper ? "M" : "m"}, {900, upper ? "CM" : "cm"},
        {500, upper ? "D" : "d"}, {400, upper ? "CD" : "cd"},
        {100, upper ? "C" : "c"}, {90, upper ? "XC" : "xc"},
        {50, upper ? "L" : "l"}, {40, upper ? "XL" : "xl"},
        {10, upper ? "X" : "x"}, {9, upper ? "IX" : "ix"},
        {5, upper ? "V" : "v"}, {4, upper ? "IV" : "iv"},
        {1, upper ? "I" : "i"}
    };
    std::string result;
    for (const auto& m : mapping) {
        while (val >= m.value) {
            result += m.symbol;
            val -= m.value;
        }
    }
    return result;
}

static std::string toAlpha(int val, bool upper) {
    if (val <= 0) return std::to_string(val);
    std::string result;
    int temp = val;
    while (temp > 0) {
        int rem = (temp - 1) % 26;
        char c = static_cast<char>((upper ? 'A' : 'a') + rem);
        result = c + result;
        temp = (temp - 1) / 26;
    }
    return result;
}

void Widget::renderListMarker(Renderer& renderer) {
    if (computedStyle.display != Display::ListItem) return;
    if (computedStyle.listStyleType == ListStyleType::None) return;

    std::string markerText;
    int index = getListItemIndex(this);
    switch (computedStyle.listStyleType) {
        case ListStyleType::Decimal:
            markerText = std::to_string(index) + ".";
            break;
        case ListStyleType::DecimalLeadingZero:
            markerText = (index < 10 ? "0" : "") + std::to_string(index) + ".";
            break;
        case ListStyleType::LowerRoman:
            markerText = toRoman(index, false) + ".";
            break;
        case ListStyleType::UpperRoman:
            markerText = toRoman(index, true) + ".";
            break;
        case ListStyleType::LowerAlpha:
            markerText = toAlpha(index, false) + ".";
            break;
        case ListStyleType::UpperAlpha:
            markerText = toAlpha(index, true) + ".";
            break;
        default:
            break;
    }

    Color textColor = computedStyle.color;
    const std::string& fontName = renderFontName(computedStyle);
    if (!markerText.empty()) {
        Vec2 textSize = renderer.measureText(markerText, computedStyle.fontSize, fontName);
        float x = 0.0f;
        float y = bounds.y + computedStyle.padding.top;
        if (computedStyle.direction == Direction::Ltr) {
            x = bounds.x - 8.0f - textSize.x;
        } else {
            x = bounds.x + bounds.w + 8.0f;
        }
        renderer.drawText(markerText, Vec2(x, y), textColor, computedStyle.fontSize,
                          computedStyle.fontWeight, fontName, computedStyle.fontStyle,
                          computedStyle.direction, computedStyle.unicodeBidi);
    } else {
        // Bullet shapes: Disc, Circle, Square
        float bulletRadius = computedStyle.fontSize * 0.2f;
        float d = bulletRadius * 2.0f;
        float x = 0.0f;
        float y = bounds.y + computedStyle.padding.top + computedStyle.fontSize * 0.5f - bulletRadius;
        if (computedStyle.direction == Direction::Ltr) {
            x = bounds.x - 15.0f - bulletRadius;
        } else {
            x = bounds.x + bounds.w + 15.0f - bulletRadius;
        }
        if (computedStyle.listStyleType == ListStyleType::Disc) {
            renderer.drawRoundedRect(Rect(x, y, d, d), textColor, BorderRadius(bulletRadius));
        } else if (computedStyle.listStyleType == ListStyleType::Circle) {
            renderer.drawBorder(Rect(x, y, d, d), Border(1.2f, textColor), BorderRadius(bulletRadius));
        } else if (computedStyle.listStyleType == ListStyleType::Square) {
            renderer.drawRoundedRect(Rect(x, y, d, d), textColor, BorderRadius(0.0f));
        }
    }
}

void Widget::renderBackground(Renderer& renderer) {
    auto& s = computedStyle;
    if (s.hasBackdropFilterBlur && s.backdropFilterBlur > 0.0f) {
        renderer.drawBackdropFilterBlur(bounds, s.backdropFilterBlur, s.borderRadius);
    }
    if (s.boxShadow.blur > 0 || s.boxShadow.spread > 0) {
        renderer.drawBoxShadow(bounds, s.boxShadow, s.borderRadius);
    }
    Color bgColor = s.backgroundColor;
    if (s.hasHoverBg && hoverAnim > 0) {
        bgColor = Color::lerp(s.backgroundColor, s.hoverBackgroundColor, hoverAnim);
    }
    if (focused && s.hasFocusBg) {
        bgColor = s.focusBackgroundColor;
    }
    if (pressed && s.hasActiveBg) {
        bgColor = s.activeBackgroundColor;
    }
    float opacity = s.opacity;
    if (s.hoverOpacity >= 0 && hoverAnim > 0) {
        opacity = s.opacity + (s.hoverOpacity - s.opacity) * hoverAnim;
    }
    if (focused && s.focusOpacity >= 0) {
        opacity = s.focusOpacity;
    }
    if (pressed && s.activeOpacity >= 0) {
        opacity = s.activeOpacity;
    }
    Gradient bgGradient = s.backgroundGradient;
    if (s.hasHoverGradient && hoverAnim > 0) {
        bgGradient = Gradient::lerp(s.backgroundGradient, s.hoverBackgroundGradient, hoverAnim);
    }
    if (focused && s.hasFocusGradient) {
        bgGradient = s.focusBackgroundGradient;
    }
    if (pressed && s.hasActiveGradient) {
        bgGradient = s.activeBackgroundGradient;
    }
    if (bgGradient.type != Gradient::None) {
        renderer.drawRoundedRectGradient(bounds, bgGradient, s.borderRadius, opacity);
    } else if (bgColor.a > 0.001f) {
        renderer.drawRoundedRect(bounds, bgColor, s.borderRadius, opacity);
    }
    if (s.border.width > 0) {
        Border b = s.border;
        if (s.hasHoverBorder && hoverAnim > 0) {
            b.color = Color::lerp(s.border.color, s.hoverBorderColor, hoverAnim);
        }
        if (focused && s.hasFocusBorder) {
            b.color = s.focusBorderColor;
        }
        if (pressed && s.hasActiveBorder) {
            b.color = s.activeBorderColor;
        }
        renderer.drawBorder(bounds, b, s.borderRadius);
    }
    auto drawEdgeBorder = [&](const Border& border, const Rect& rect) {
        if (border.width <= 0) return;
        renderer.drawRoundedRect(rect, border.color, BorderRadius(0));
    };
    if (s.hasBorderTop) {
        drawEdgeBorder(s.borderTop, {bounds.x, bounds.y, bounds.w, s.borderTop.width});
    }
    if (s.hasBorderRight) {
        drawEdgeBorder(s.borderRight,
                       {bounds.x + bounds.w - s.borderRight.width, bounds.y,
                        s.borderRight.width, bounds.h});
    }
    if (s.hasBorderBottom) {
        drawEdgeBorder(s.borderBottom,
                       {bounds.x, bounds.y + bounds.h - s.borderBottom.width,
                        bounds.w, s.borderBottom.width});
    }
    if (s.hasBorderLeft) {
        drawEdgeBorder(s.borderLeft, {bounds.x, bounds.y, s.borderLeft.width, bounds.h});
    }
    Border outline = s.outline;
    if (focused && s.hasFocusOutline) outline = s.focusOutline;
    if (pressed && s.hasActiveOutline) outline = s.activeOutline;
    if (outline.width > 0) {
        float expand = s.outlineOffset + outline.width;
        Rect outlineRect = {
            bounds.x - expand,
            bounds.y - expand,
            bounds.w + expand * 2.0f,
            bounds.h + expand * 2.0f
        };
        renderer.drawBorder(outlineRect, outline,
                            BorderRadius(s.borderRadius.uniform() + expand));
    }
}
void Widget::renderChildren(Renderer& renderer) {
    if (skipDOMChildrenPaint) return;
    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    bool clip = clipsOverflow(computedStyle);
    Rect visibleContent = bounds;
    if (scrollable) {
        visibleContent.y += scrollY;
    }
    if (clip) renderer.pushScissor(bounds);
    if (scrollable) {
        renderer.pushTranslation({0, -scrollY});
    }
    size_t startIndex = 0;
    size_t endIndex = children.size();
    const bool isColumn = (computedStyle.flexDirection == FlexDirection::Column);
    if (children.size() > 256 && isColumn && scrollable) {
        auto itStart = std::lower_bound(children.begin(), children.end(), visibleContent.y - 64.0f,
            [](const std::shared_ptr<Widget>& w, float y) {
                return w->bounds.y + w->bounds.h < y;
            });
        startIndex = std::distance(children.begin(), itStart);
        auto itEnd = std::upper_bound(itStart, children.end(), visibleContent.y + visibleContent.h + 64.0f,
            [](float y, const std::shared_ptr<Widget>& w) {
                return y < w->bounds.y;
            });
        endIndex = std::distance(children.begin(), itEnd);
    }
    for (size_t i = startIndex; i < endIndex; i++) {
        auto& child = children[i];
        if (!canPaintWidget(child.get())) continue;
        if (child->computedStyle.position == Position::Fixed) continue;
        if (clip && !rectIntersects(child->bounds, visibleContent, 64.0f)) continue;
        child->render(renderer);
    }
    if (scrollable) {
        renderer.popTranslation();
        Rect track, thumb;
        if (getScrollBarRects(track, thumb)) {
            float active = (scrollbarHovered || scrollbarDragging) ? 1.0f : 0.0f;
            float pressed = scrollbarDragging ? 1.0f : 0.0f;
            Rect visualTrack = {
                track.x,
                track.y,
                track.w,
                track.h
            };
            Rect visualThumb = thumb;
            if (!scrollbarHovered && !scrollbarDragging) {
                visualThumb.x += 2.0f;
                visualThumb.w -= 4.0f;
            }
            renderer.drawRoundedRect(visualTrack,
                                     Color(0.13f, 0.14f, 0.16f, 0.32f + active * 0.22f),
                                     BorderRadius(0));
            renderer.drawRoundedRect(visualThumb,
                                     Color(0.47f + pressed * 0.16f,
                                           0.49f + pressed * 0.16f,
                                           0.53f + pressed * 0.16f,
                                           0.72f + active * 0.18f),
                                     BorderRadius(5));
        }
    }
    if (clip) renderer.popScissor();
}
void Widget::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    bool hasScale = (renderScale != 1.0f && !layoutObject);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }
    renderBackground(renderer);
    renderListMarker(renderer);
    renderChildren(renderer);
    if (hasScale) {
        renderer.popScale();
    }
}
void Text::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    if (!s.width.isSet() && s.flexGrow <= 0.0f && parentUsesRowFlex(this)) {
        bounds.w = std::max(1.0f, approximateTextWidth(content, s.fontSize) + s.padding.horizontal());
    }
    if (!s.height.isSet()) {
        float availableW = std::max(0.0f, bounds.w - s.padding.horizontal());
        if (s.whiteSpace == WhiteSpace::NoWrap || availableW <= 0.0f) {
            bounds.h = std::max(1.0f, s.fontSize * s.lineHeight + s.padding.vertical());
        } else {
            std::vector<std::string> lines = wrapText(content, s.fontSize, availableW);
            float lineCount = static_cast<float>(lines.size());
            bounds.h = std::max(1.0f, lineCount * (s.fontSize * s.lineHeight) + s.padding.vertical());
        }
    }
}
void Text::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    renderListMarker(renderer);
    Color textColor = computedStyle.color;
    if (computedStyle.hasHoverColor && hoverAnim > 0) {
        textColor = Color::lerp(computedStyle.color, computedStyle.hoverColor, hoverAnim);
    }
    if (focused && computedStyle.hasFocusColor) {
        textColor = computedStyle.focusColor;
    }
    if (pressed && computedStyle.hasActiveColor) {
        textColor = computedStyle.activeColor;
    }
    Rect textRect = {
        bounds.x + computedStyle.padding.left,
        bounds.y + computedStyle.padding.top,
        std::max(0.0f, bounds.w - computedStyle.padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle.padding.vertical())
    };
    if (computedStyle.verticalAlign == VerticalAlign::Super) {
        textRect.y -= computedStyle.fontSize * 0.32f;
    } else if (computedStyle.verticalAlign == VerticalAlign::Sub) {
        textRect.y += computedStyle.fontSize * 0.22f;
    }
    const std::string* displayTextPtr = &content;
    std::string transformedText;
    if (computedStyle.textTransform != TextTransform::None && !content.empty()) {
        transformedText = applyTextTransform(content, computedStyle.textTransform);
        displayTextPtr = &transformedText;
    }
    const std::string& fontName = renderFontName(computedStyle);
    if (computedStyle.whiteSpace != WhiteSpace::NoWrap && textRect.w > 0.0f) {
        std::vector<std::string> lines = wrapText(*displayTextPtr, computedStyle.fontSize, textRect.w);
        float lineHeight = computedStyle.fontSize * computedStyle.lineHeight;
        float totalTextH = lines.size() * lineHeight;
        float startY = textRect.y;
        if (textRect.h > totalTextH) {
            startY += (textRect.h - totalTextH) / 2.0f;
        }
        for (size_t i = 0; i < lines.size(); ++i) {
            Rect lineRect = {
                textRect.x,
                startY + i * lineHeight,
                textRect.w,
                lineHeight
            };
            std::string ellipsizedText = lines[i];
            if (computedStyle.textOverflow == TextOverflow::Ellipsis && i == lines.size() - 1) {
                ellipsizedText = ellipsizeText(renderer, lines[i], textRect.w,
                                              computedStyle.fontSize, fontName);
            }
            renderer.drawTextInRect(ellipsizedText, lineRect, textColor,
                                    computedStyle.fontSize, computedStyle.textAlign,
                                    computedStyle.fontWeight, fontName,
                                    computedStyle.fontStyle,
                                    computedStyle.direction,
                                    computedStyle.unicodeBidi);
            renderTextDecoration(renderer, ellipsizedText, lineRect, textColor, computedStyle);
        }
    } else {
        std::string ellipsizedText;
        if (computedStyle.textOverflow == TextOverflow::Ellipsis) {
            ellipsizedText = ellipsizeText(renderer, *displayTextPtr, textRect.w,
                                          computedStyle.fontSize, fontName);
            displayTextPtr = &ellipsizedText;
        }
        renderer.drawTextInRect(*displayTextPtr, textRect, textColor,
                                computedStyle.fontSize, computedStyle.textAlign,
                                computedStyle.fontWeight, fontName,
                                computedStyle.fontStyle,
                                computedStyle.direction,
                                computedStyle.unicodeBidi);
        renderTextDecoration(renderer, *displayTextPtr, textRect, textColor, computedStyle);
    }
    renderChildren(renderer);
}
void Button::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    if (!s.height.isSet()) {
        bounds.h = std::max(22.0f, s.fontSize * s.lineHeight +
            s.padding.vertical() + usedBorderVertical(s));
    }
}
void Button::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    bool hasScale = (renderScale != 1.0f && !layoutObject);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }
    Rect drawBounds = bounds;
    if (pressed) {
        drawBounds.x += 1;
        drawBounds.y += 1;
        drawBounds.w -= 2;
        drawBounds.h -= 2;
    }
    auto& s = computedStyle;
    if (s.boxShadow.blur > 0) {
        renderer.drawBoxShadow(drawBounds, s.boxShadow, s.borderRadius);
    }
    Color bgColor = s.backgroundColor;
    if (s.hasHoverBg && hoverAnim > 0) {
        bgColor = Color::lerp(s.backgroundColor, s.hoverBackgroundColor, hoverAnim);
    }
    if (focused && s.hasFocusBg) {
        bgColor = s.focusBackgroundColor;
    }
    if (pressed && s.hasActiveBg) {
        bgColor = s.activeBackgroundColor;
    }
    Gradient bgGradient = s.backgroundGradient;
    if (s.hasHoverGradient && hoverAnim > 0) {
        bgGradient = Gradient::lerp(s.backgroundGradient, s.hoverBackgroundGradient, hoverAnim);
    }
    if (focused && s.hasFocusGradient) {
        bgGradient = s.focusBackgroundGradient;
    }
    if (pressed && s.hasActiveGradient) {
        bgGradient = s.activeBackgroundGradient;
    }
    if (bgGradient.type != Gradient::None) {
        renderer.drawRoundedRectGradient(drawBounds, bgGradient, s.borderRadius, s.opacity);
    } else {
        renderer.drawRoundedRect(drawBounds, bgColor, s.borderRadius, s.opacity);
    }
    if (pressed) {
        renderer.drawRoundedRect(drawBounds, Color(0, 0, 0, 0.16f), s.borderRadius);
    }
    if (s.border.width > 0) {
        Border b = s.border;
        if (s.hasHoverBorder && hoverAnim > 0) {
            b.color = Color::lerp(s.border.color, s.hoverBorderColor, hoverAnim);
        }
        if (focused && s.hasFocusBorder) {
            b.color = s.focusBorderColor;
        }
        if (pressed && s.hasActiveBorder) {
            b.color = s.activeBorderColor;
        }
        renderer.drawBorder(drawBounds, b, s.borderRadius);
    }
    Border outline = s.outline;
    if (focused && s.hasFocusOutline) outline = s.focusOutline;
    if (pressed && s.hasActiveOutline) outline = s.activeOutline;
    if (outline.width > 0) {
        float expand = s.outlineOffset + outline.width;
        renderer.drawBorder({drawBounds.x - expand, drawBounds.y - expand,
                             drawBounds.w + expand * 2.0f, drawBounds.h + expand * 2.0f},
                            outline, BorderRadius(s.borderRadius.uniform() + expand));
    }
    Color textColor = s.color;
    if (s.hasHoverColor && hoverAnim > 0) {
        textColor = Color::lerp(s.color, s.hoverColor, hoverAnim);
    }
    if (focused && s.hasFocusColor) {
        textColor = s.focusColor;
    }
    if (pressed && s.hasActiveColor) {
        textColor = s.activeColor;
    }
    Rect textRect = {
        drawBounds.x + s.padding.left,
        drawBounds.y + s.padding.top,
        std::max(0.0f, drawBounds.w - s.padding.horizontal()),
        std::max(0.0f, drawBounds.h - s.padding.vertical())
    };
    const std::string* displayLabelPtr = &label;
    std::string transformedLabel;
    if (s.textTransform != TextTransform::None && !label.empty()) {
        transformedLabel = applyTextTransform(label, s.textTransform);
        displayLabelPtr = &transformedLabel;
    }
    const std::string& fontName = renderFontName(s);
    std::string ellipsizedLabel;
    if (s.textOverflow == TextOverflow::Ellipsis) {
        ellipsizedLabel = ellipsizeText(renderer, *displayLabelPtr, textRect.w, s.fontSize, fontName);
        displayLabelPtr = &ellipsizedLabel;
    }
    renderer.drawTextInRect(*displayLabelPtr, textRect, textColor,
                            s.fontSize, s.textAlign, s.fontWeight, fontName,
                            s.fontStyle, s.direction, s.unicodeBidi);
    renderTextDecoration(renderer, *displayLabelPtr, textRect, textColor, s);
    renderChildren(renderer);
    if (hasScale) {
        renderer.popScale();
    }
}
void TextInput::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (inputType == TextInputType::Hidden) {
        bounds.w = 0.0f;
        bounds.h = 0.0f;
        contentHeight = 0.0f;
        return;
    }
    auto& s = computedStyle;
    if (!s.height.isSet()) {
        float browserMinHeight = type == "textarea" ? 54.0f : 20.0f;
        if (inputType == TextInputType::Color) browserMinHeight = 23.0f;
        bounds.h = std::max(browserMinHeight, s.fontSize * s.lineHeight +
            s.padding.vertical() + usedBorderVertical(s));
    }
}
bool TextInput::hasSelection() const {
    return selectionAnchor_ != selectionFocus_;
}
size_t TextInput::selectionStart() const {
    return std::min(selectionAnchor_, selectionFocus_);
}
size_t TextInput::selectionEnd() const {
    return std::max(selectionAnchor_, selectionFocus_);
}
Rect TextInput::clearButtonRect() const {
    float size = std::min(18.0f, std::max(12.0f, bounds.h - 16.0f));
    return {
        bounds.x + bounds.w - computedStyle.padding.right - size - 8.0f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
}
TextInput* TextInput::setInputType(TextInputType kind) {
    if (inputType == kind) return this;
    inputType = kind;
    clearHovered_ = false;
    clearPressed_ = false;
    selecting_ = false;
    markStyleDirty();
    return this;
}
TextInput* TextInput::setInputType(const std::string& kind) {
    std::string lower = kind;
    for (char& c : lower) {
        c = (char)std::tolower((unsigned char)c);
    }
    if (lower == "password") return setInputType(TextInputType::Password);
    if (lower == "search") return setInputType(TextInputType::Search);
    if (lower == "email") return setInputType(TextInputType::Email);
    if (lower == "url") return setInputType(TextInputType::Url);
    if (lower == "tel" || lower == "telephone") return setInputType(TextInputType::Tel);
    if (lower == "number") return setInputType(TextInputType::Number);
    if (lower == "hidden") return setInputType(TextInputType::Hidden);
    if (lower == "button") return setInputType(TextInputType::Button);
    if (lower == "submit") return setInputType(TextInputType::Submit);
    if (lower == "reset") return setInputType(TextInputType::Reset);
    if (lower == "file") return setInputType(TextInputType::File);
    if (lower == "color") return setInputType(TextInputType::Color);
    if (lower == "date") return setInputType(TextInputType::Date);
    if (lower == "time") return setInputType(TextInputType::Time);
    if (lower == "month") return setInputType(TextInputType::Month);
    if (lower == "week") return setInputType(TextInputType::Week);
    if (lower == "datetime-local" || lower == "datetimelocal") {
        return setInputType(TextInputType::DateTimeLocal);
    }
    if (lower == "image") return setInputType(TextInputType::Image);
    return setInputType(TextInputType::Text);
}
void TextInput::update(const InputState& input) {
    Widget::update(input);
    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);
    auto updateFocusAnimation = [&]() {
        float focusTarget = focused ? 1.0f : 0.0f;
        float focusSpeed = 16.0f;
        if (focusAnim_ < focusTarget) {
            focusAnim_ = std::min(focusAnim_ + input.deltaTime * focusSpeed, focusTarget);
        }
        if (focusAnim_ > focusTarget) {
            focusAnim_ = std::max(focusAnim_ - input.deltaTime * focusSpeed, focusTarget);
        }
    };
    bool textEditing = type == "textarea" || isTextEditingInputType(inputType);
    if (inputType == TextInputType::Hidden || !textEditing) {
        clearHovered_ = false;
        clearPressed_ = false;
        selecting_ = false;
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
        if (hovered && input.mouseClicked[0]) {
            focused = true;
        } else if (!hovered && input.mouseClicked[0]) {
            focused = false;
        }
        int keyCode = normalizeTextEditingKey(input.keyCode);
        bool keyboardActivate = focused && isButtonLikeInputType(inputType) &&
            (keyCode == 0x0D || keyCode == 0x20) &&
            (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
        if (keyboardActivate) {
            pressed = true;
            if (onClick) onClick();
        }
        updateFocusAnimation();
        return;
    }
    auto setCaret = [&](size_t index, bool extendSelection) {
        caretIndex_ = clampToUtf8Boundary(value, index);
        if (extendSelection) {
            selectionFocus_ = caretIndex_;
        } else {
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
        }
        caretBlinkTime_ = 0;
    };
    auto eraseSelection = [&]() -> bool {
        if (!hasSelection()) return false;
        size_t start = selectionStart();
        size_t end = selectionEnd();
        value.erase(start, end - start);
        caretIndex_ = start;
        selectionAnchor_ = start;
        selectionFocus_ = start;
        caretBlinkTime_ = 0;
        return true;
    };
    auto insertText = [&](const std::string& text) {
        if (text.empty()) return;
        eraseSelection();
        value.insert(caretIndex_, text);
        caretIndex_ += text.size();
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
        caretBlinkTime_ = 0;
    };
    auto indexAtMouse = [&]() {
        float localX = input.mousePos.x - bounds.x - computedStyle.padding.left + scrollX_;
        if (inputType == TextInputType::Password) {
            return passwordTextIndexAtX(value, localX, computedStyle.fontSize);
        }
        return approximateTextIndexAtX(value, localX, computedStyle.fontSize);
    };
    bool shift = (input.modifiers & MOD_SHIFT) != 0;
    bool ctrl = (input.modifiers & MOD_CTRL) != 0;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    int commandKey = keyCode;
    if (commandKey >= 'a' && commandKey <= 'z') {
        commandKey = commandKey - 'a' + 'A';
    }
    bool canShowClear = inputType == TextInputType::Search && !value.empty();
    Rect clearRect = clearButtonRect();
    clearHovered_ = canShowClear && clearRect.contains(input.mousePos);
    clearPressed_ = clearHovered_ && input.mouseDown[0];
    if (hovered && input.mouseClicked[0]) {
        focused = true;
        if (clearHovered_) {
            value.clear();
            scrollX_ = 0;
            selecting_ = false;
            setCaret(0, false);
        } else {
            size_t index = indexAtMouse();
            if (!shift && input.mouseClickCount[0] >= 2) {
                size_t start = 0;
                size_t end = 0;
                wordRangeAt(value, index, start, end);
                selectionAnchor_ = start;
                selectionFocus_ = end;
                caretIndex_ = end;
                caretBlinkTime_ = 0;
                selecting_ = false;
            } else {
                selecting_ = true;
                if (!shift) {
                    selectionAnchor_ = index;
                }
                setCaret(index, shift);
            }
        }
    } else if (!hovered && input.mouseClicked[0]) {
        focused = false;
        selecting_ = false;
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
    }
    if (selecting_ && focused && input.mouseDown[0]) {
        setCaret(indexAtMouse(), true);
    }
    if (input.mouseReleased[0]) {
        selecting_ = false;
    }
    updateFocusAnimation();
    if (!focused) return;
    caretBlinkTime_ += input.deltaTime;
    if (keyCode != 0) {
        if (ctrl && commandKey == 'A') {
            selectionAnchor_ = 0;
            selectionFocus_ = value.size();
            caretIndex_ = value.size();
            caretBlinkTime_ = 0;
            return;
        }
        if (ctrl && (commandKey == 'C' || commandKey == 'X')) {
            if (hasSelection()) {
                std::string selected = value.substr(selectionStart(), selectionEnd() - selectionStart());
                Platform::setClipboardText(selected.c_str());
                if (commandKey == 'X') eraseSelection();
            }
            return;
        }
        if (ctrl && commandKey == 'V') {
            std::string clip = Platform::getClipboardText();
            if (!clip.empty()) {
                insertText(clip);
            }
            return;
        }
        switch (keyCode) {
        case 0x08:
            if (!eraseSelection() && caretIndex_ > 0) {
                size_t prev = ctrl ? previousWordBoundary(value, caretIndex_) :
                    previousCodepoint(value, caretIndex_);
                value.erase(prev, caretIndex_ - prev);
                setCaret(prev, false);
            }
            return;
        case 0x2E:
            if (!eraseSelection() && caretIndex_ < value.size()) {
                size_t next = ctrl ? nextWordBoundary(value, caretIndex_) :
                    nextCodepoint(value, caretIndex_);
                value.erase(caretIndex_, next - caretIndex_);
                setCaret(caretIndex_, false);
            }
            return;
        case 0x25:
        {
            size_t target = hasSelection() && !shift ? selectionStart() :
                (ctrl ? previousWordBoundary(value, caretIndex_) : previousCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x27:
        {
            size_t target = hasSelection() && !shift ? selectionEnd() :
                (ctrl ? nextWordBoundary(value, caretIndex_) : nextCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x24:
            setCaret(0, shift);
            return;
        case 0x23:
            setCaret(value.size(), shift);
            return;
        case 0x1B:
            focused = false;
            selecting_ = false;
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
            return;
        case 0x0D:
            focused = false;
            selecting_ = false;
            return;
        default:
            break;
        }
    }
    if (!ctrl && !input.text.empty()) {
        insertText(input.text);
    }
}
CursorType TextInput::cursorAt(Vec2 point) const {
    if (!canHitTestWidget(this) || !bounds.contains(point)) {
        return CursorType::Default;
    }
    if (inputType == TextInputType::Hidden) {
        return CursorType::Default;
    }
    if (inputType == TextInputType::Search && !value.empty() && clearButtonRect().contains(point)) {
        return CursorType::Pointer;
    }
    if (type != "textarea" && !isTextEditingInputType(inputType)) {
        return inputType == TextInputType::Image ? CursorType::Pointer : CursorType::Default;
    }
    return CursorType::Text;
}
void TextInput::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    if (inputType == TextInputType::Hidden) return;
    renderBackground(renderer);
    auto& s = computedStyle;
    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);
    bool cssHandlesFocus = s.hasFocusOutline || s.hasFocusBorder || s.hasFocusBg;
    bool cssHandlesHover = s.hasHoverBorder || s.hasHoverBg;
    if (!cssHandlesFocus && focusAnim_ > 0.001f) {
        Rect ring = {bounds.x - focusAnim_, bounds.y - focusAnim_,
                     bounds.w + focusAnim_ * 2.0f, bounds.h + focusAnim_ * 2.0f};
        Color focusColor = Color(0.54f, 0.70f, 0.98f, 0.42f + focusAnim_ * 0.42f);
        renderer.drawBorder(ring, Border(1.0f + focusAnim_, focusColor), s.borderRadius);
    } else if (!cssHandlesHover && hoverAnim > 0.001f) {
        renderer.drawBorder(bounds, Border(1.0f, Color(0.50f, 0.53f, 0.57f, 0.44f * hoverAnim)),
                            s.borderRadius);
    }
    bool textEditing = type == "textarea" || isTextEditingInputType(inputType);
    if (!textEditing) {
        std::string fontName = renderFontName(s);
        Color textColor = s.color;
        if (inputType == TextInputType::Color) {
            Color swatch = value.empty() ? Color(0.0f, 0.0f, 0.0f, 1.0f) : Color::fromHex(value);
            Rect swatchRect = {
                bounds.x + std::max(2.0f, s.padding.left + 2.0f),
                bounds.y + std::max(3.0f, s.padding.top + 3.0f),
                std::max(0.0f, bounds.w - s.padding.horizontal() - 8.0f),
                std::max(0.0f, bounds.h - s.padding.vertical() - 8.0f)
            };
            renderer.drawRoundedRect(swatchRect, swatch, BorderRadius(0.0f));
            renderer.drawBorder(swatchRect,
                                Border(1.0f, Color(0.466f, 0.466f, 0.466f, 1.0f)),
                                BorderRadius(0.0f));
        } else if (inputType == TextInputType::File) {
            const std::string chooseText = placeholder.empty() ? "Choose File" : placeholder;
            const std::string fileText = value.empty() ? "No file chosen" : value;
            float buttonW = std::min(std::max(84.0f,
                renderer.measureText(chooseText, s.fontSize, fontName).x + 22.0f),
                std::max(84.0f, bounds.w * 0.62f));
            Rect buttonRect = {
                bounds.x + s.padding.left,
                bounds.y + s.padding.top,
                std::max(0.0f, buttonW),
                std::max(0.0f, bounds.h - s.padding.vertical())
            };
            renderer.drawRoundedRect(buttonRect,
                                     pressed ? Color(0.82f, 0.82f, 0.82f, 1.0f)
                                             : Color(0.94f, 0.94f, 0.94f, 1.0f),
                                     BorderRadius(2.0f));
            renderer.drawBorder(buttonRect,
                                Border(2.0f, Color(0.46f, 0.46f, 0.46f, 1.0f)),
                                BorderRadius(2.0f));
            renderer.drawTextInRect(chooseText, buttonRect, Color(0, 0, 0, 1),
                                    s.fontSize, TextAlign::Center, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
            Rect labelRect = {
                buttonRect.x + buttonRect.w + 6.0f,
                bounds.y,
                std::max(0.0f, bounds.x + bounds.w - (buttonRect.x + buttonRect.w + 6.0f)),
                bounds.h
            };
            renderer.drawTextInRect(fileText, labelRect, textColor,
                                    s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
        } else {
            std::string label = value.empty() ? placeholder : value;
            if (label.empty()) {
                if (inputType == TextInputType::Submit) label = "Submit";
                else if (inputType == TextInputType::Reset) label = "Reset";
            }
            Rect textRect = {
                bounds.x + s.padding.left,
                bounds.y + s.padding.top,
                std::max(0.0f, bounds.w - s.padding.horizontal()),
                std::max(0.0f, bounds.h - s.padding.vertical())
            };
            renderer.drawTextInRect(label, textRect, textColor,
                                    s.fontSize, TextAlign::Center, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
        }
        renderChildren(renderer);
        return;
    }
    const bool passwordMode = inputType == TextInputType::Password;
    std::string visibleValue = inputVisibleRange(inputType, value, 0, value.size());
    auto visiblePrefix = [&](size_t index) {
        return inputVisibleRange(inputType, value, 0, index);
    };
    auto visibleRange = [&](size_t start, size_t end) {
        return inputVisibleRange(inputType, value, start, end);
    };
    const bool canShowClear = inputType == TextInputType::Search && !value.empty();
    float clearSpace = canShowClear ? 28.0f : 0.0f;
    Rect clipRect = {
        bounds.x + s.padding.left,
        bounds.y,
        std::max(0.0f, bounds.w - s.padding.horizontal() - clearSpace),
        bounds.h
    };
    std::string fontName = renderFontName(s);
    if (value.empty()) {
        scrollX_ = 0;
    } else {
        float caretX = renderer.measureText(visiblePrefix(caretIndex_), s.fontSize, fontName).x;
        float rightPadding = 10.0f;
        if (caretX - scrollX_ > clipRect.w - rightPadding) {
            scrollX_ = caretX - clipRect.w + rightPadding;
        } else if (caretX - scrollX_ < 0) {
            scrollX_ = std::max(0.0f, caretX - 4.0f);
        }
        scrollX_ = std::max(0.0f, scrollX_);
    }
    renderer.pushScissor(clipRect);
    if (hasSelection() && !value.empty()) {
        size_t start = selectionStart();
        size_t end = selectionEnd();
        float startX = renderer.measureText(visiblePrefix(start), s.fontSize, fontName).x - scrollX_;
        float width = renderer.measureText(visibleRange(start, end), s.fontSize, fontName).x;
        float selH = std::min(bounds.h - 10.0f, s.fontSize + 10.0f);
        Rect selectionRect = {
            clipRect.x + startX,
            bounds.y + (bounds.h - selH) * 0.5f,
            width,
            selH
        };
        renderer.drawRoundedRect(selectionRect, Color(0.54f, 0.70f, 0.98f, 0.56f), BorderRadius(2));
    }
    const std::string* displayTextPtr = value.empty() ? &placeholder : &visibleValue;
    std::string transformedText;
    if (s.textTransform != TextTransform::None && !passwordMode && !displayTextPtr->empty()) {
        transformedText = applyTextTransform(*displayTextPtr, s.textTransform);
        displayTextPtr = &transformedText;
    }
    float bgLum = s.backgroundColor.r * 0.2126f +
                  s.backgroundColor.g * 0.7152f +
                  s.backgroundColor.b * 0.0722f;
    Color placeholderColor = bgLum > 0.45f
        ? Color(0.459f, 0.459f, 0.459f, 1.0f)
        : Color(0.604f, 0.627f, 0.659f, 0.92f);
    Color textColor = value.empty() ? placeholderColor : s.color;
    Rect textRect = {
        clipRect.x - scrollX_,
        bounds.y,
        std::max(clipRect.w + scrollX_, renderer.measureText(*displayTextPtr, s.fontSize, fontName).x + 8.0f),
        bounds.h
    };
    renderer.drawTextInRect(*displayTextPtr, textRect, textColor,
                            s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                            s.fontStyle, s.direction, s.unicodeBidi);
    renderTextDecoration(renderer, *displayTextPtr, textRect, textColor, s);
    if (focused) {
        float caretX = clipRect.x + renderer.measureText(visiblePrefix(caretIndex_), s.fontSize, fontName).x - scrollX_;
        float cursorH = std::min(bounds.h - 12.0f, s.fontSize + 8.0f);
        float cursorY = bounds.y + (bounds.h - cursorH) * 0.5f;
        float blink = std::fmod(caretBlinkTime_, 1.0f);
        if ((blink < 0.55f || selecting_) && caretX >= clipRect.x - 1 && caretX <= clipRect.x + clipRect.w + 1) {
            renderer.drawRoundedRect({caretX, cursorY, 1.5f, cursorH},
                                     Color(0.54f, 0.70f, 0.98f, 1.0f), BorderRadius(1));
        }
    }
    renderer.popScissor();
    if (canShowClear) {
        Rect clear = clearButtonRect();
        Color iconColor = clearPressed_ ? Color(0.91f, 0.93f, 0.96f, 1.0f) :
            Color(0.74f, 0.77f, 0.81f, clearHovered_ ? 0.96f : 0.74f);
        if (clearHovered_ || clearPressed_) {
            renderer.drawRoundedRect(clear,
                                     Color(0.54f, 0.56f, 0.60f, clearPressed_ ? 0.36f : 0.22f),
                                     BorderRadius(clear.w * 0.5f));
        }
        renderer.drawTextInRect("x", clear, iconColor,
                                std::max(11.0f, s.fontSize - 1.0f),
                                TextAlign::Center, FontWeight::Bold);
    }
    renderChildren(renderer);
}

void TextArea::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    float charW = approximateGlyphAdvance('m', s.fontSize);
    float lineH = s.fontSize * 1.2f;
    
    float minHeight = bounds.h;
    if (!s.height.isSet()) {
        minHeight = rows * lineH + s.padding.vertical() + s.margin.vertical();
    }
    
    if (!s.width.isSet()) {
        bounds.w = cols * charW + s.padding.horizontal() + s.margin.horizontal();
    }
    
    float clipW = std::max(0.0f, bounds.w - s.padding.horizontal());
    auto lines = layoutLines(s.fontSize, clipW);
    contentHeight = lines.size() * lineH + s.padding.vertical();
    
    float requiredHeight = contentHeight + s.margin.vertical();
    if (s.height.isSet()) {
        bounds.h = minHeight;
    } else {
        bounds.h = std::max(minHeight, requiredHeight);
    }
}

bool TextArea::isOverResizeHandle(Vec2 point) const {
    float size = 14.0f;
    Rect handle = {
        bounds.x + bounds.w - size,
        bounds.y + bounds.h - size,
        size,
        size
    };
    return point.x >= handle.x && point.x <= handle.x + handle.w &&
           point.y >= handle.y && point.y <= handle.y + handle.h;
}

bool TextArea::hasSelection() const {
    return selectionAnchor_ != selectionFocus_;
}

size_t TextArea::selectionStart() const {
    return std::min(selectionAnchor_, selectionFocus_);
}

size_t TextArea::selectionEnd() const {
    return std::max(selectionAnchor_, selectionFocus_);
}

std::vector<TextArea::LineInfo> TextArea::layoutLines(float fontSize, float maxWidth) const {
    std::vector<LineInfo> lines;
    if (value.empty()) {
        lines.push_back({0, 0, 0.0f});
        return lines;
    }
    size_t start = 0;
    while (start < value.size()) {
        size_t nextNewline = value.find('\n', start);
        size_t paragraphEnd = (nextNewline == std::string::npos) ? value.size() : nextNewline;
        if (wrap && maxWidth > 0) {
            size_t curr = start;
            while (curr < paragraphEnd) {
                size_t step = curr;
                float accumulatedWidth = 0.0f;
                size_t lastSpace = std::string::npos;
                while (step < paragraphEnd) {
                    size_t nextCp = clampToUtf8Boundary(value, step + 1);
                    if (nextCp <= step) nextCp = step + 1;
                    float advance = 0.0f;
                    for (size_t k = step; k < nextCp; ++k) {
                        advance += approximateGlyphAdvance(static_cast<unsigned char>(value[k]), fontSize);
                    }
                    if (accumulatedWidth + advance > maxWidth) {
                        break;
                    }
                    accumulatedWidth += advance;
                    if (value[step] == ' ' || value[step] == '\t') {
                        lastSpace = step;
                    }
                    step = nextCp;
                }
                size_t lineEnd = step;
                if (lineEnd < paragraphEnd && lastSpace != std::string::npos && lastSpace > curr) {
                    lineEnd = lastSpace + 1;
                }
                if (lineEnd == curr) {
                    lineEnd = clampToUtf8Boundary(value, curr + 1);
                    if (lineEnd <= curr) lineEnd = curr + 1;
                }
                float finalW = 0.0f;
                for (size_t k = curr; k < lineEnd; ++k) {
                    finalW += approximateGlyphAdvance(static_cast<unsigned char>(value[k]), fontSize);
                }
                lines.push_back({curr, lineEnd, finalW});
                curr = lineEnd;
            }
        } else {
            float finalW = 0.0f;
            for (size_t k = start; k < paragraphEnd; ++k) {
                finalW += approximateGlyphAdvance(static_cast<unsigned char>(value[k]), fontSize);
            }
            lines.push_back({start, paragraphEnd, finalW});
        }
        start = paragraphEnd + 1;
    }
    if (!value.empty() && value.back() == '\n') {
        lines.push_back({value.size(), value.size(), 0.0f});
    }
    return lines;
}

void TextArea::getLineAndColumnOfOffset(const std::vector<LineInfo>& lines, size_t offset, size_t& outLine, size_t& outCol) const {
    outLine = 0;
    outCol = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (offset >= lines[i].start && offset <= lines[i].end) {
            outLine = i;
            outCol = offset - lines[i].start;
            return;
        }
    }
    if (!lines.empty()) {
        outLine = lines.size() - 1;
        outCol = lines.back().end - lines.back().start;
    }
}

void TextArea::update(const InputState& input) {
    Widget::update(input);
    
    auto updateFocusAnimation = [&]() {
        float focusTarget = focused ? 1.0f : 0.0f;
        float focusSpeed = 16.0f;
        if (focusAnim_ < focusTarget) {
            focusAnim_ = std::min(focusAnim_ + input.deltaTime * focusSpeed, focusTarget);
        }
        if (focusAnim_ > focusTarget) {
            focusAnim_ = std::max(focusAnim_ - input.deltaTime * focusSpeed, focusTarget);
        }
    };

    if (resizing_) {
        if (input.mouseDown[0]) {
            float minW = 50.0f;
            float minH = 30.0f;
            float maxW = 9999.0f;
            float maxH = 9999.0f;
            if (computedStyle.minWidth.isSet()) {
                minW = std::max(minW, computedStyle.minWidth.resolve(bounds.w));
            }
            if (computedStyle.minHeight.isSet()) {
                minH = std::max(minH, computedStyle.minHeight.resolve(bounds.h));
            }
            if (computedStyle.maxWidth.isSet()) {
                maxW = computedStyle.maxWidth.resolve(bounds.w);
            }
            if (computedStyle.maxHeight.isSet()) {
                maxH = computedStyle.maxHeight.resolve(bounds.h);
            }
            float newWidth = resizeStartSize_.x + (input.mousePos.x - resizeStartMousePos_.x);
            float newHeight = resizeStartSize_.y + (input.mousePos.y - resizeStartMousePos_.y);
            newWidth = std::clamp(newWidth, minW, maxW);
            newHeight = std::clamp(newHeight, minH, maxH);
            style.width = CSSValue::px(newWidth);
            style.height = CSSValue::px(newHeight);
            markStyleDirty();
        } else {
            resizing_ = false;
        }
        if (input.mouseReleased[0]) {
            resizing_ = false;
        }
        updateFocusAnimation();
        return;
    }
    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);
    
    auto setCaret = [&](size_t index, bool extendSelection) {
        caretIndex_ = clampToUtf8Boundary(value, index);
        if (extendSelection) {
            selectionFocus_ = caretIndex_;
        } else {
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
        }
        caretBlinkTime_ = 0;
    };
    
    auto eraseSelection = [&]() -> bool {
        if (!hasSelection()) return false;
        size_t start = selectionStart();
        size_t end = selectionEnd();
        value.erase(start, end - start);
        caretIndex_ = start;
        selectionAnchor_ = start;
        selectionFocus_ = start;
        caretBlinkTime_ = 0;
        return true;
    };
    
    auto insertText = [&](const std::string& text) {
        if (text.empty()) return;
        eraseSelection();
        value.insert(caretIndex_, text);
        caretIndex_ += text.size();
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
        caretBlinkTime_ = 0;
    };
    
    auto indexAtMouse = [&]() {
        float localX = input.mousePos.x - bounds.x - computedStyle.padding.left + scrollX_;
        float localY = input.mousePos.y - bounds.y - computedStyle.padding.top + scrollY;
        float lineHeight = computedStyle.fontSize * 1.2f;
        float maxWidth = std::max(0.0f, bounds.w - computedStyle.padding.horizontal());
        auto lines = layoutLines(computedStyle.fontSize, maxWidth);
        int lineIdx = static_cast<int>(localY / lineHeight);
        if (lineIdx < 0) lineIdx = 0;
        if (lineIdx >= static_cast<int>(lines.size())) lineIdx = static_cast<int>(lines.size()) - 1;
        if (lines.empty()) return static_cast<size_t>(0);
        const auto& line = lines[lineIdx];
        std::string lineStr = value.substr(line.start, line.end - line.start);
        size_t relativeIndex = approximateTextIndexAtX(lineStr, localX, computedStyle.fontSize);
        return line.start + relativeIndex;
    };
    
    bool shift = (input.modifiers & MOD_SHIFT) != 0;
    bool ctrl = (input.modifiers & MOD_CTRL) != 0;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    int commandKey = keyCode;
    if (commandKey >= 'a' && commandKey <= 'z') {
        commandKey = commandKey - 'a' + 'A';
    }
    
    if (hovered && input.mouseClicked[0]) {
        if (isOverResizeHandle(input.mousePos)) {
            resizing_ = true;
            resizeStartMousePos_ = input.mousePos;
            resizeStartSize_ = { bounds.w, bounds.h };
            focused = true;
            updateFocusAnimation();
            return;
        }
        focused = true;
        size_t index = indexAtMouse();
        if (!shift && input.mouseClickCount[0] >= 2) {
            size_t start = 0;
            size_t end = 0;
            wordRangeAt(value, index, start, end);
            selectionAnchor_ = start;
            selectionFocus_ = end;
            caretIndex_ = end;
            caretBlinkTime_ = 0;
            selecting_ = false;
        } else {
            selecting_ = true;
            if (!shift) {
                selectionAnchor_ = index;
            }
            setCaret(index, shift);
        }
    } else if (!hovered && input.mouseClicked[0]) {
        focused = false;
        selecting_ = false;
        selectionAnchor_ = caretIndex_;
        selectionFocus_ = caretIndex_;
    }
    
    if (selecting_ && focused && input.mouseDown[0]) {
        setCaret(indexAtMouse(), true);
    }
    if (input.mouseReleased[0]) {
        selecting_ = false;
    }
    
    updateFocusAnimation();
    if (!focused) return;
    caretBlinkTime_ += input.deltaTime;
    
    if (keyCode != 0) {
        if (ctrl && commandKey == 'A') {
            selectionAnchor_ = 0;
            selectionFocus_ = value.size();
            caretIndex_ = value.size();
            caretBlinkTime_ = 0;
            return;
        }
        if (ctrl && (commandKey == 'C' || commandKey == 'X')) {
            if (hasSelection()) {
                std::string selected = value.substr(selectionStart(), selectionEnd() - selectionStart());
                Platform::setClipboardText(selected.c_str());
                if (commandKey == 'X') eraseSelection();
            }
            return;
        }
        if (ctrl && commandKey == 'V') {
            std::string clip = Platform::getClipboardText();
            if (!clip.empty()) {
                insertText(clip);
            }
            return;
        }
        
        switch (keyCode) {
        case 0x08: // Backspace
            if (!eraseSelection() && caretIndex_ > 0) {
                size_t prev = ctrl ? previousWordBoundary(value, caretIndex_) :
                    previousCodepoint(value, caretIndex_);
                value.erase(prev, caretIndex_ - prev);
                setCaret(prev, false);
            }
            return;
        case 0x2E: // Delete
            if (!eraseSelection() && caretIndex_ < value.size()) {
                size_t next = ctrl ? nextWordBoundary(value, caretIndex_) :
                    nextCodepoint(value, caretIndex_);
                value.erase(caretIndex_, next - caretIndex_);
                setCaret(caretIndex_, false);
            }
            return;
        case 0x25: // Left Arrow
        {
            size_t target = hasSelection() && !shift ? selectionStart() :
                (ctrl ? previousWordBoundary(value, caretIndex_) : previousCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x27: // Right Arrow
        {
            size_t target = hasSelection() && !shift ? selectionEnd() :
                (ctrl ? nextWordBoundary(value, caretIndex_) : nextCodepoint(value, caretIndex_));
            setCaret(target, shift);
            return;
        }
        case 0x26: // Up Arrow
        {
            float maxWidth = std::max(0.0f, bounds.w - computedStyle.padding.horizontal());
            auto lines = layoutLines(computedStyle.fontSize, maxWidth);
            size_t currLine = 0, currCol = 0;
            getLineAndColumnOfOffset(lines, caretIndex_, currLine, currCol);
            if (currLine > 0) {
                size_t targetLine = currLine - 1;
                size_t targetCol = std::min(currCol, lines[targetLine].end - lines[targetLine].start);
                setCaret(lines[targetLine].start + targetCol, shift);
            } else {
                setCaret(0, shift);
            }
            return;
        }
        case 0x28: // Down Arrow
        {
            float maxWidth = std::max(0.0f, bounds.w - computedStyle.padding.horizontal());
            auto lines = layoutLines(computedStyle.fontSize, maxWidth);
            size_t currLine = 0, currCol = 0;
            getLineAndColumnOfOffset(lines, caretIndex_, currLine, currCol);
            if (currLine + 1 < lines.size()) {
                size_t targetLine = currLine + 1;
                size_t targetCol = std::min(currCol, lines[targetLine].end - lines[targetLine].start);
                setCaret(lines[targetLine].start + targetCol, shift);
            } else {
                setCaret(value.size(), shift);
            }
            return;
        }
        case 0x24: // Home
            setCaret(0, shift);
            return;
        case 0x23: // End
            setCaret(value.size(), shift);
            return;
        case 0x1B: // Escape
            focused = false;
            selecting_ = false;
            selectionAnchor_ = caretIndex_;
            selectionFocus_ = caretIndex_;
            return;
        case 0x0D: // Enter
            insertText("\n");
            return;
        default:
            break;
        }
    }
    if (!ctrl && !input.text.empty()) {
        insertText(input.text);
    }
    float maxWidth = std::max(0.0f, bounds.w - computedStyle.padding.horizontal());
    auto lines = layoutLines(computedStyle.fontSize, maxWidth);
    size_t caretLine = 0, caretCol = 0;
    getLineAndColumnOfOffset(lines, caretIndex_, caretLine, caretCol);
    float lineHeight = computedStyle.fontSize * 1.2f;
    float caretY = caretLine * lineHeight;
    float clipH = std::max(0.0f, bounds.h - computedStyle.padding.vertical());
    if (caretY - scrollY > clipH - lineHeight) {
        targetScrollY = caretY - clipH + lineHeight;
        scrollY = targetScrollY;
    } else if (caretY - scrollY < 0) {
        targetScrollY = caretY;
        scrollY = targetScrollY;
    }
    float maxScroll = maxScrollY();
    targetScrollY = std::clamp(targetScrollY, 0.0f, maxScroll);
    scrollY = std::clamp(scrollY, 0.0f, maxScroll);
}

CursorType TextArea::cursorAt(Vec2 point) const {
    if (resizing_) {
        return CursorType::ResizeNWSE;
    }
    if (!canHitTestWidget(this) || !bounds.contains(point)) {
        return CursorType::Default;
    }
    if (isOverResizeHandle(point)) {
        return CursorType::ResizeNWSE;
    }
    return CursorType::Text;
}

void TextArea::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    auto& s = computedStyle;
    caretIndex_ = clampToUtf8Boundary(value, caretIndex_);
    selectionAnchor_ = clampToUtf8Boundary(value, selectionAnchor_);
    selectionFocus_ = clampToUtf8Boundary(value, selectionFocus_);
    bool cssHandlesFocus = s.hasFocusOutline || s.hasFocusBorder || s.hasFocusBg;
    bool cssHandlesHover = s.hasHoverBorder || s.hasHoverBg;
    if (!cssHandlesFocus && focusAnim_ > 0.001f) {
        Rect ring = {bounds.x - focusAnim_, bounds.y - focusAnim_,
                     bounds.w + focusAnim_ * 2.0f, bounds.h + focusAnim_ * 2.0f};
        Color focusColor = Color(0.54f, 0.70f, 0.98f, 0.42f + focusAnim_ * 0.42f);
        renderer.drawBorder(ring, Border(1.0f + focusAnim_, focusColor), s.borderRadius);
    } else if (!cssHandlesHover && hoverAnim > 0.001f) {
        renderer.drawBorder(bounds, Border(1.0f, Color(0.50f, 0.53f, 0.57f, 0.44f * hoverAnim)),
                            s.borderRadius);
    }
    Rect clipRect = {
        bounds.x + s.padding.left,
        bounds.y + s.padding.top,
        std::max(0.0f, bounds.w - s.padding.horizontal()),
        std::max(0.0f, bounds.h - s.padding.vertical())
    };
    std::string fontName = renderFontName(s);
    float lineHeight = s.fontSize * 1.2f;
    auto lines = layoutLines(s.fontSize, clipRect.w);
    renderer.pushScissor(clipRect);
    if (hasSelection() && !value.empty()) {
        size_t start = selectionStart();
        size_t end = selectionEnd();
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            float lineY = bounds.y + s.padding.top + i * lineHeight - scrollY;
            if (lineY + lineHeight < bounds.y || lineY > bounds.y + bounds.h) {
                continue;
            }
            if (end > line.start && start < line.end) {
                size_t s_in_line = std::max(start, line.start);
                size_t e_in_line = std::min(end, line.end);
                std::string lineStr = value.substr(line.start, line.end - line.start);
                std::string prefix = lineStr.substr(0, s_in_line - line.start);
                std::string selectionText = lineStr.substr(s_in_line - line.start, e_in_line - s_in_line);
                float startX = renderer.measureText(prefix, s.fontSize, fontName).x;
                float width = renderer.measureText(selectionText, s.fontSize, fontName).x;
                if (e_in_line == line.end && e_in_line < end) {
                    width += approximateGlyphAdvance(' ', s.fontSize);
                }
                Rect selectionRect = {
                    clipRect.x + startX,
                    lineY + (lineHeight - s.fontSize) * 0.5f,
                    width,
                    s.fontSize + 2.0f
                };
                renderer.drawRoundedRect(selectionRect, Color(0.54f, 0.70f, 0.98f, 0.56f), BorderRadius(2));
            }
        }
    }
    float bgLum = s.backgroundColor.r * 0.2126f +
                  s.backgroundColor.g * 0.7152f +
                  s.backgroundColor.b * 0.0722f;
    Color placeholderColor = bgLum > 0.45f
        ? Color(0.459f, 0.459f, 0.459f, 1.0f)
        : Color(0.604f, 0.627f, 0.659f, 0.92f);
    if (value.empty()) {
        size_t start = 0;
        int lineIdx = 0;
        while (start < placeholder.size()) {
            size_t nextNewline = placeholder.find('\n', start);
            size_t lineEnd = (nextNewline == std::string::npos) ? placeholder.size() : nextNewline;
            std::string lineStr = placeholder.substr(start, lineEnd - start);
            
            float lineY = bounds.y + s.padding.top + lineIdx * lineHeight - scrollY;
            if (lineY + lineHeight >= bounds.y && lineY <= bounds.y + bounds.h) {
                Rect textRect = {
                    clipRect.x - scrollX_,
                    lineY,
                    clipRect.w + scrollX_,
                    lineHeight
                };
                renderer.drawTextInRect(lineStr, textRect, placeholderColor,
                                        s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                                        s.fontStyle, s.direction, s.unicodeBidi);
            }
            
            start = lineEnd + 1;
            lineIdx++;
        }
    } else {
        Color textColor = s.color;
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            float lineY = bounds.y + s.padding.top + i * lineHeight - scrollY;
            if (lineY + lineHeight < bounds.y || lineY > bounds.y + bounds.h) {
                continue;
            }
            std::string lineStr = value.substr(line.start, line.end - line.start);
            if (s.textTransform != TextTransform::None) {
                lineStr = applyTextTransform(lineStr, s.textTransform);
            }
            Rect textRect = {
                clipRect.x - scrollX_,
                lineY,
                std::max(clipRect.w + scrollX_, line.width + 8.0f),
                lineHeight
            };
            renderer.drawTextInRect(lineStr, textRect, textColor,
                                    s.fontSize, TextAlign::Left, s.fontWeight, fontName,
                                    s.fontStyle, s.direction, s.unicodeBidi);
            renderTextDecoration(renderer, lineStr, textRect, textColor, s);
        }
    }
    if (focused) {
        size_t caretLine = 0, caretCol = 0;
        getLineAndColumnOfOffset(lines, caretIndex_, caretLine, caretCol);
        const auto& line = lines[caretLine];
        std::string lineStr = value.substr(line.start, line.end - line.start);
        std::string prefix = lineStr.substr(0, caretCol);
        float caretX = clipRect.x + renderer.measureText(prefix, s.fontSize, fontName).x - scrollX_;
        float caretY = bounds.y + s.padding.top + caretLine * lineHeight - scrollY;
        float cursorH = s.fontSize + 4.0f;
        float cursorY = caretY + (lineHeight - cursorH) * 0.5f;
        float blink = std::fmod(caretBlinkTime_, 1.0f);
        if ((blink < 0.55f || selecting_) && caretX >= clipRect.x - 1 && caretX <= clipRect.x + clipRect.w + 1 &&
            caretY + lineHeight >= clipRect.y && caretY <= clipRect.y + clipRect.h) {
            renderer.drawRoundedRect({caretX, cursorY, 1.5f, cursorH},
                                     Color(0.54f, 0.70f, 0.98f, 1.0f), BorderRadius(1));
        }
    }
    renderer.popScissor();

    // Draw Scrollbar (matching Widget::render scrollbar drawing)
    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    if (scrollable) {
        Rect track, thumb;
        if (getScrollBarRects(track, thumb)) {
            float active = (scrollbarHovered || scrollbarDragging) ? 1.0f : 0.0f;
            float pressed = scrollbarDragging ? 1.0f : 0.0f;
            Rect visualTrack = {
                track.x,
                track.y,
                track.w,
                track.h
            };
            Rect visualThumb = thumb;
            if (!scrollbarHovered && !scrollbarDragging) {
                visualThumb.x += 2.0f;
                visualThumb.w -= 4.0f;
            }
            renderer.drawRoundedRect(visualTrack,
                                     Color(0.13f, 0.14f, 0.16f, 0.32f + active * 0.22f),
                                     BorderRadius(0));
            renderer.drawRoundedRect(visualThumb,
                                     Color(0.47f + pressed * 0.16f,
                                           0.49f + pressed * 0.16f,
                                           0.53f + pressed * 0.16f,
                                           0.72f + active * 0.18f),
                                     BorderRadius(5));
        }
    }

    // Draw Resize Handle in the bottom-right corner of the whole widget bounds
    float rx = bounds.x + bounds.w;
    float ry = bounds.y + bounds.h;
    Color dotColor = Color(0.50f, 0.53f, 0.57f, 0.8f);
    renderer.drawRoundedRect({rx - 4.5f, ry - 4.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    renderer.drawRoundedRect({rx - 7.5f, ry - 4.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    renderer.drawRoundedRect({rx - 10.5f, ry - 4.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    
    renderer.drawRoundedRect({rx - 4.5f, ry - 7.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    renderer.drawRoundedRect({rx - 7.5f, ry - 7.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));
    
    renderer.drawRoundedRect({rx - 4.5f, ry - 10.5f, 1.5f, 1.5f}, dotColor, BorderRadius(0.5f));

    renderChildren(renderer);
}

void Checkbox::setChecked(bool value) {
    if (checked == value) return;
    checked = value;
    markStyleDirty();
    if (onChange) onChange(checked);
}
void Checkbox::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    if (!s.width.isSet()) bounds.w = 13.0f + s.margin.horizontal();
    if (!s.height.isSet()) bounds.h = 13.0f + s.margin.vertical();
}
void Checkbox::update(const InputState& input) {
    Widget::update(input);
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardToggle = focused && (keyCode == 0x20 || keyCode == 0x0D) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    if ((hovered && input.mouseClicked[0]) || keyboardToggle) {
        focused = true;
        setChecked(!checked);
    }
}
void Checkbox::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    float size = std::max(4.0f, std::min(bounds.w, bounds.h));
    Rect box = {
        bounds.x + (bounds.w - size) * 0.5f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
    Color fill = checked ? Color::fromHex("#1a73e8") :
        (s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1));
    Color borderColor = checked ? Color::fromHex("#1a73e8") :
        (s.border.color.a > 0.0f ? s.border.color : Color(0.46f, 0.46f, 0.46f, 1));
    renderer.drawRoundedRect(box, fill, BorderRadius(2.0f));
    renderer.drawBorder(box, Border(std::max(1.0f, s.border.width), borderColor), BorderRadius(2.0f));
    if (checked) {
        renderer.drawTextInRect("\xE2\x9C\x93", box, Color(1, 1, 1, 1),
                                std::max(10.0f, size * 0.86f), TextAlign::Center,
                                FontWeight::Bold);
    }
    if (focused) {
        renderer.drawBorder({box.x - 3.0f, box.y - 3.0f, box.w + 6.0f, box.h + 6.0f},
                            Border(1.0f, Color(0.54f, 0.70f, 0.98f, 0.95f)),
                            BorderRadius(4.0f));
    }
}
void Radio::setChecked(bool value) {
    if (checked == value) return;
    checked = value;
    if (checked) {
        clearRadioGroup(rootOfWidget(this), this, group);
    }
    markStyleDirty();
    if (onChange) onChange(checked);
}
void Radio::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    if (!s.width.isSet()) bounds.w = 13.0f + s.margin.horizontal();
    if (!s.height.isSet()) bounds.h = 13.0f + s.margin.vertical();
}
void Radio::update(const InputState& input) {
    Widget::update(input);
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardToggle = focused && (keyCode == 0x20 || keyCode == 0x0D) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    if ((hovered && input.mouseClicked[0]) || keyboardToggle) {
        focused = true;
        setChecked(true);
    }
}
void Radio::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    float size = std::max(4.0f, std::min(bounds.w, bounds.h));
    Rect ring = {
        bounds.x + (bounds.w - size) * 0.5f,
        bounds.y + (bounds.h - size) * 0.5f,
        size,
        size
    };
    Color fill = s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1);
    Color accent = Color::fromHex("#1a73e8");
    Color borderColor = checked ? accent :
        (s.border.color.a > 0.0f ? s.border.color : Color(0.46f, 0.46f, 0.46f, 1));
    renderer.drawRoundedRect(ring, fill, BorderRadius(size * 0.5f));
    renderer.drawBorder(ring, Border(std::max(1.0f, s.border.width), borderColor),
                        BorderRadius(size * 0.5f));
    if (checked) {
        float dot = std::max(4.0f, size * 0.48f);
        renderer.drawRoundedRect({ring.x + (ring.w - dot) * 0.5f,
                                  ring.y + (ring.h - dot) * 0.5f,
                                  dot,
                                  dot},
                                 accent,
                                 BorderRadius(dot * 0.5f));
    }
    if (focused) {
        renderer.drawBorder({ring.x - 3.0f, ring.y - 3.0f, ring.w + 6.0f, ring.h + 6.0f},
                            Border(1.0f, Color(0.54f, 0.70f, 0.98f, 0.95f)),
                            BorderRadius((size + 6.0f) * 0.5f));
    }
}
void RangeInput::setValue(float newValue, bool notify) {
    if (max < min) std::swap(max, min);
    float clamped = std::clamp(newValue, min, max);
    if (step > 0.0f) {
        clamped = min + std::round((clamped - min) / step) * step;
        clamped = std::clamp(clamped, min, max);
    }
    if (std::abs(value - clamped) < 0.0001f) return;
    value = clamped;
    if (notify && onChange) onChange(value);
}
void RangeInput::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (!computedStyle.width.isSet()) bounds.w = 129.0f;
    if (!computedStyle.height.isSet()) bounds.h = 16.0f;
}
void RangeInput::update(const InputState& input) {
    Widget::update(input);
    auto setFromPoint = [&](float x) {
        float pad = 7.0f;
        float t = (x - (bounds.x + pad)) / std::max(1.0f, bounds.w - pad * 2.0f);
        setValue(min + std::clamp(t, 0.0f, 1.0f) * (max - min));
    };
    if (hovered && input.mouseClicked[0]) {
        focused = true;
        dragging_ = true;
        setFromPoint(input.mousePos.x);
    }
    if (dragging_ && input.mouseDown[0]) {
        setFromPoint(input.mousePos.x);
    }
    if (input.mouseReleased[0]) {
        dragging_ = false;
    }
    if (!focused) return;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    if (keyCode == 0) return;
    float delta = step > 0.0f ? step : (max - min) / 100.0f;
    if (keyCode == 0x25 || keyCode == 0x28) setValue(value - delta);
    else if (keyCode == 0x27 || keyCode == 0x26) setValue(value + delta);
    else if (keyCode == 0x21) setValue(value + delta * 10.0f);
    else if (keyCode == 0x22) setValue(value - delta * 10.0f);
    else if (keyCode == 0x24) setValue(min);
    else if (keyCode == 0x23) setValue(max);
}
void RangeInput::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    float pad = 7.0f;
    float trackH = 4.0f;
    Rect track = {bounds.x + pad, bounds.y + (bounds.h - trackH) * 0.5f,
                  std::max(1.0f, bounds.w - pad * 2.0f), trackH};
    float t = (max == min) ? 0.0f : std::clamp((value - min) / (max - min), 0.0f, 1.0f);
    Color trackColor = s.color.a > 0.0f ? s.color.withAlpha(0.42f) : Color(0.56f, 0.56f, 0.56f, 1);
    Color accent = Color::fromHex("#1a73e8");
    renderer.drawRoundedRect(track, trackColor, BorderRadius(trackH * 0.5f));
    renderer.drawRoundedRect({track.x, track.y, track.w * t, track.h},
                             accent.withAlpha(0.86f), BorderRadius(trackH * 0.5f));
    float thumb = 14.0f;
    Rect knob = {track.x + track.w * t - thumb * 0.5f,
                 bounds.y + (bounds.h - thumb) * 0.5f,
                 thumb,
                 thumb};
    renderer.drawRoundedRect(knob, accent, BorderRadius(thumb * 0.5f));
    renderer.drawBorder(knob, Border(1.0f, Color(1, 1, 1, 0.88f)),
                        BorderRadius(thumb * 0.5f));
    if (focused) {
        renderer.drawBorder({knob.x - 3.0f, knob.y - 3.0f, knob.w + 6.0f, knob.h + 6.0f},
                            Border(1.0f, Color(0.54f, 0.70f, 0.98f, 0.95f)),
                            BorderRadius((thumb + 6.0f) * 0.5f));
    }
}
void Option::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    if (!computedStyle.height.isSet()) {
        bounds.h = std::max(18.0f, computedStyle.fontSize * computedStyle.lineHeight +
            computedStyle.padding.vertical());
    }
}
void Option::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    Rect textRect = {
        bounds.x + computedStyle.padding.left,
        bounds.y + computedStyle.padding.top,
        std::max(0.0f, bounds.w - computedStyle.padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle.padding.vertical())
    };
    renderer.drawTextInRect(label, textRect, computedStyle.color,
                            computedStyle.fontSize, TextAlign::Left,
                            computedStyle.fontWeight, renderFontName(computedStyle),
                            computedStyle.fontStyle, computedStyle.direction,
                            computedStyle.unicodeBidi);
}
void Select::selectIndex(size_t index, bool notify) {
    auto options = selectOptions(this);
    if (options.empty()) {
        selectedIndex = 0;
        return;
    }
    size_t next = std::min(index, options.size() - 1);
    if (selectedIndex == next) return;
    selectedIndex = next;
    if (notify && onChange) onChange(selectedIndex, selectedValue());
}
std::string Select::selectedLabel() const {
    auto* self = const_cast<Select*>(this);
    auto options = selectOptions(self);
    if (options.empty()) return "";
    size_t index = std::min(selectedIndex, options.size() - 1);
    return options[index]->label;
}
std::string Select::selectedValue() const {
    auto* self = const_cast<Select*>(this);
    auto options = selectOptions(self);
    if (options.empty()) return "";
    size_t index = std::min(selectedIndex, options.size() - 1);
    return options[index]->value;
}
void Select::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    if (!s.width.isSet() && parentUsesRowFlex(this)) bounds.w = 128.0f;
    if (!s.height.isSet()) {
        bounds.h = std::max(22.0f, s.fontSize * s.lineHeight +
            s.padding.vertical() + usedBorderVertical(s));
    }
}
void Select::update(const InputState& input) {
    Widget::update(input);
    auto options = selectOptions(this);
    float rowH = std::max(20.0f, computedStyle.fontSize * computedStyle.lineHeight + 5.0f);
    Rect listRect(bounds.x, bounds.y + bounds.h, bounds.w, rowH * options.size());
    bool listHovered = expanded && listRect.contains(input.mousePos);
    if (input.mouseClicked[0]) {
        if (hovered) {
            focused = true;
            expanded = !expanded;
            markStyleDirty();
        } else if (listHovered) {
            size_t index = std::min(options.size() - 1,
                static_cast<size_t>((input.mousePos.y - listRect.y) / rowH));
            selectIndex(index);
            focused = true;
            expanded = false;
            markStyleDirty();
        } else if (expanded) {
            expanded = false;
            markStyleDirty();
        }
    }
    if (!focused) return;
    int keyCode = normalizeTextEditingKey(input.keyCode);
    if (keyCode == 0x1B) {
        expanded = false;
    } else if (keyCode == 0x20 || keyCode == 0x0D) {
        expanded = !expanded;
    } else if (!options.empty() && (keyCode == 0x28 || keyCode == 0x27)) {
        selectIndex(std::min(selectedIndex + 1, options.size() - 1));
    } else if (!options.empty() && (keyCode == 0x26 || keyCode == 0x25)) {
        selectIndex(selectedIndex == 0 ? 0 : selectedIndex - 1);
    } else if (!options.empty() && keyCode == 0x24) {
        selectIndex(0);
    } else if (!options.empty() && keyCode == 0x23) {
        selectIndex(options.size() - 1);
    }
}
void Select::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    Color bg = s.backgroundColor.a > 0.0f ? s.backgroundColor : Color(1, 1, 1, 1);
    renderer.drawRoundedRect(bounds, bg, s.borderRadius);
    renderer.drawBorder(bounds, s.border.width > 0.0f ? s.border : Border(1.0f, Color(0, 0, 0, 1)),
                        s.borderRadius);
    Rect textRect = {bounds.x + s.padding.left,
                     bounds.y + s.padding.top,
                     std::max(0.0f, bounds.w - s.padding.horizontal() - 18.0f),
                     std::max(0.0f, bounds.h - s.padding.vertical())};
    renderer.drawTextInRect(selectedLabel(), textRect, s.color,
                            s.fontSize, TextAlign::Left, s.fontWeight, renderFontName(s),
                            s.fontStyle, s.direction, s.unicodeBidi);
    Rect arrowRect = {bounds.x + bounds.w - 20.0f, bounds.y, 16.0f, bounds.h};
    renderer.drawTextInRect("v", arrowRect, s.color,
                            std::max(9.0f, s.fontSize - 1.0f),
                            TextAlign::Center, FontWeight::Bold, renderFontName(s),
                            FontStyle::Normal, Direction::Ltr, UnicodeBidi::Normal);
    if (focused) {
        renderer.drawBorder({bounds.x - 2.0f, bounds.y - 2.0f, bounds.w + 4.0f, bounds.h + 4.0f},
                            Border(1.0f, Color(0.54f, 0.70f, 0.98f, 0.95f)),
                            BorderRadius(s.borderRadius.uniform() + 2.0f));
    }
    if (expanded) {
        auto options = selectOptions(this);
        float rowH = std::max(20.0f, s.fontSize * s.lineHeight + 5.0f);
        Rect list = {bounds.x, bounds.y + bounds.h, bounds.w, rowH * options.size()};
        renderer.drawRoundedRect(list, bg, BorderRadius(4.0f));
        renderer.drawBorder(list, Border(1.0f, Color(0.36f, 0.38f, 0.42f, 1)),
                            BorderRadius(4.0f));
        for (size_t i = 0; i < options.size(); ++i) {
            Rect row = {list.x, list.y + rowH * i, list.w, rowH};
            if (i == selectedIndex) {
                renderer.drawRoundedRect(row, Color(0.54f, 0.70f, 0.98f, 0.34f),
                                         BorderRadius(0.0f));
            }
            Rect optionText = {row.x + s.padding.left, row.y, row.w - s.padding.horizontal(), row.h};
            renderer.drawTextInRect(options[i]->label, optionText, s.color,
                                    s.fontSize, TextAlign::Left, s.fontWeight,
                                    renderFontName(s), s.fontStyle, s.direction,
                                    s.unicodeBidi);
        }
    }
}
void Icon::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    Color c = computedStyle.color;
    if (computedStyle.hasHoverColor && hoverAnim > 0) {
        c = Color::lerp(computedStyle.color, computedStyle.hoverColor, hoverAnim);
    }
    float size = std::max(1.0f, std::min(bounds.w, bounds.h));
    float x = bounds.x + (bounds.w - size) * 0.5f;
    float y = bounds.y + (bounds.h - size) * 0.5f;
    auto rect = [&](float rx, float ry, float rw, float rh, float radius = 1.0f, float alpha = 1.0f) {
        renderer.drawRoundedRect({x + rx * size, y + ry * size, rw * size, rh * size},
                                 c.withAlpha(c.a * alpha), BorderRadius(radius));
    };
    auto border = [&](float rx, float ry, float rw, float rh, float radius = 2.0f, float width = 1.4f) {
        renderer.drawBorder({x + rx * size, y + ry * size, rw * size, rh * size},
                            Border(width, c), BorderRadius(radius));
    };
    auto textIcon = [&](const std::string& text, float scale = 0.66f, float alpha = 1.0f) {
        renderer.drawTextInRect(text, {x, y, size, size}, c.withAlpha(c.a * alpha),
                                std::max(9.0f, size * scale), TextAlign::Center,
                                FontWeight::Bold);
    };
    if (glyph == "dashboard") {
        rect(0.12f, 0.14f, 0.30f, 0.30f, 2);
        rect(0.56f, 0.14f, 0.32f, 0.22f, 2, 0.75f);
        rect(0.12f, 0.58f, 0.32f, 0.26f, 2, 0.75f);
        rect(0.58f, 0.50f, 0.30f, 0.34f, 2);
    } else if (glyph == "back") {
        textIcon("<", 0.76f);
    } else if (glyph == "forward") {
        rect(0.22f, 0.45f, 0.40f, 0.10f, 1.2f);
        rect(0.53f, 0.30f, 0.10f, 0.20f, 1.2f);
        rect(0.53f, 0.50f, 0.10f, 0.20f, 1.2f);
        rect(0.63f, 0.40f, 0.14f, 0.20f, 1.2f);
    } else if (glyph == "reload") {
        border(0.22f, 0.22f, 0.56f, 0.56f, 10, 1.45f);
        rect(0.58f, 0.13f, 0.22f, 0.08f, 1.2f);
        rect(0.73f, 0.18f, 0.08f, 0.18f, 1.2f);
    } else if (glyph == "search") {
        border(0.19f, 0.18f, 0.46f, 0.46f, 8, 1.55f);
        rect(0.58f, 0.61f, 0.25f, 0.09f, 1.5f);
    } else if (glyph == "lock") {
        border(0.28f, 0.42f, 0.44f, 0.34f, 3, 1.35f);
        border(0.35f, 0.22f, 0.30f, 0.32f, 6, 1.35f);
        rect(0.47f, 0.55f, 0.06f, 0.12f, 1.0f);
    } else if (glyph == "close") {
        textIcon("x", 0.58f, 0.88f);
    } else if (glyph == "minimize") {
        rect(0.26f, 0.54f, 0.48f, 0.08f, 1.0f, 0.92f);
    } else if (glyph == "maximize") {
        border(0.28f, 0.28f, 0.44f, 0.44f, 1.5f, 1.35f);
    } else if (glyph == "plus") {
        textIcon("+", 0.74f);
    } else if (glyph == "star") {
        textIcon("*", 0.82f);
    } else if (glyph == "menu") {
        rect(0.46f, 0.22f, 0.08f, 0.08f, 3);
        rect(0.46f, 0.46f, 0.08f, 0.08f, 3);
        rect(0.46f, 0.70f, 0.08f, 0.08f, 3);
    } else if (glyph == "shield") {
        border(0.22f, 0.14f, 0.56f, 0.64f, 5, 1.35f);
        rect(0.38f, 0.42f, 0.10f, 0.20f, 1.2f);
        rect(0.47f, 0.56f, 0.22f, 0.08f, 1.2f);
    } else if (glyph == "scanner") {
        border(0.13f, 0.13f, 0.52f, 0.52f, 6, 1.6f);
        rect(0.60f, 0.63f, 0.25f, 0.10f, 1.2f);
        rect(0.76f, 0.72f, 0.10f, 0.16f, 1.2f);
    } else if (glyph == "alert") {
        border(0.16f, 0.15f, 0.68f, 0.66f, 4, 1.5f);
        rect(0.47f, 0.31f, 0.08f, 0.27f, 1.0f);
        rect(0.47f, 0.65f, 0.08f, 0.08f, 2.0f);
    } else if (glyph == "rules") {
        rect(0.13f, 0.20f, 0.13f, 0.13f, 2);
        rect(0.35f, 0.23f, 0.48f, 0.07f, 1);
        rect(0.13f, 0.45f, 0.13f, 0.13f, 2, 0.8f);
        rect(0.35f, 0.48f, 0.42f, 0.07f, 1, 0.8f);
        rect(0.13f, 0.70f, 0.13f, 0.13f, 2, 0.65f);
        rect(0.35f, 0.73f, 0.54f, 0.07f, 1, 0.65f);
    } else if (glyph == "report") {
        border(0.20f, 0.10f, 0.58f, 0.78f, 3, 1.5f);
        rect(0.33f, 0.31f, 0.32f, 0.07f, 1);
        rect(0.33f, 0.48f, 0.34f, 0.07f, 1, 0.75f);
        rect(0.33f, 0.65f, 0.24f, 0.07f, 1, 0.6f);
    } else if (glyph == "settings") {
        border(0.28f, 0.28f, 0.44f, 0.44f, 8, 1.5f);
        rect(0.46f, 0.08f, 0.08f, 0.16f, 1);
        rect(0.46f, 0.76f, 0.08f, 0.16f, 1);
        rect(0.08f, 0.46f, 0.16f, 0.08f, 1);
        rect(0.76f, 0.46f, 0.16f, 0.08f, 1);
        rect(0.44f, 0.44f, 0.12f, 0.12f, 4);
    } else if (glyph == "block") {
        border(0.16f, 0.16f, 0.68f, 0.68f, 8, 1.5f);
        rect(0.30f, 0.46f, 0.40f, 0.09f, 1.5f);
    } else if (glyph == "card") {
        border(0.13f, 0.24f, 0.74f, 0.54f, 3, 1.4f);
        rect(0.19f, 0.36f, 0.62f, 0.08f, 1);
        rect(0.24f, 0.58f, 0.18f, 0.07f, 1, 0.7f);
    } else if (glyph == "id") {
        border(0.15f, 0.18f, 0.70f, 0.64f, 3, 1.4f);
        rect(0.25f, 0.34f, 0.18f, 0.18f, 5);
        rect(0.22f, 0.59f, 0.24f, 0.08f, 2);
        rect(0.55f, 0.38f, 0.20f, 0.06f, 1, 0.8f);
        rect(0.55f, 0.55f, 0.18f, 0.06f, 1, 0.55f);
    } else if (glyph == "check") {
        rect(0.18f, 0.50f, 0.11f, 0.22f, 1.5f);
        rect(0.26f, 0.63f, 0.43f, 0.10f, 1.5f);
        rect(0.64f, 0.30f, 0.11f, 0.43f, 1.5f);
    } else if (glyph == "download") {
        rect(0.46f, 0.15f, 0.08f, 0.42f, 1);
        rect(0.32f, 0.50f, 0.36f, 0.10f, 1);
        rect(0.24f, 0.72f, 0.52f, 0.09f, 1.5f);
    } else if (glyph == "usb") {
        rect(0.42f, 0.12f, 0.16f, 0.44f, 2);
        rect(0.34f, 0.50f, 0.32f, 0.30f, 3);
        rect(0.38f, 0.20f, 0.06f, 0.08f, 1, 0.75f);
        rect(0.56f, 0.20f, 0.06f, 0.08f, 1, 0.75f);
        rect(0.45f, 0.80f, 0.10f, 0.10f, 1);
    } else if (glyph == "mail") {
        border(0.14f, 0.24f, 0.72f, 0.52f, 4, 1.45f);
        rect(0.20f, 0.32f, 0.30f, 0.08f, 1, 0.72f);
        rect(0.50f, 0.32f, 0.30f, 0.08f, 1, 0.72f);
        rect(0.30f, 0.58f, 0.40f, 0.07f, 1, 0.56f);
    } else if (glyph == "cloud") {
        border(0.18f, 0.42f, 0.64f, 0.28f, 8, 1.45f);
        border(0.26f, 0.28f, 0.26f, 0.30f, 8, 1.25f);
        border(0.46f, 0.24f, 0.30f, 0.34f, 9, 1.25f);
    } else if (glyph == "database") {
        border(0.22f, 0.16f, 0.56f, 0.24f, 8, 1.35f);
        border(0.22f, 0.36f, 0.56f, 0.24f, 8, 1.35f);
        border(0.22f, 0.56f, 0.56f, 0.24f, 8, 1.35f);
    } else if (glyph == "clock") {
        border(0.18f, 0.18f, 0.64f, 0.64f, 10, 1.5f);
        rect(0.48f, 0.30f, 0.06f, 0.23f, 1.0f);
        rect(0.50f, 0.50f, 0.22f, 0.06f, 1.0f);
    } else if (glyph == "play") {
        textIcon(">", 0.82f);
    } else if (glyph == "pause") {
        rect(0.32f, 0.24f, 0.12f, 0.52f, 1.5f);
        rect(0.56f, 0.24f, 0.12f, 0.52f, 1.5f);
    } else {
        rect(0.25f, 0.25f, 0.50f, 0.50f, 8);
    }
    renderChildren(renderer);
}
void Image::updateCurrentSrc() {
    currentSrc = source;
    intrinsicDensity = 1.0f;
    if (srcset.empty()) return;
    std::istringstream stream(srcset);
    std::string token;
    float bestDensityDiff = 999.0f;
    std::string bestSrc = currentSrc;
    float bestDensity = 1.0f;
    while (std::getline(stream, token, ',')) {
        size_t start = 0;
        while (start < token.size() && std::isspace((unsigned char)token[start])) ++start;
        if (start >= token.size()) continue;
        size_t space = token.find(' ', start);
        std::string url = token.substr(start, space == std::string::npos ? std::string::npos : space - start);
        float density = 1.0f;
        if (space != std::string::npos) {
            size_t descStart = space + 1;
            while (descStart < token.size() && std::isspace((unsigned char)token[descStart])) ++descStart;
            size_t descEnd = token.size();
            while (descEnd > descStart && std::isspace((unsigned char)token[descEnd - 1])) --descEnd;
            if (descEnd > descStart && token[descEnd - 1] == 'x') {
                try {
                    density = std::stof(token.substr(descStart, descEnd - descStart - 1));
                } catch (...) {}
            }
        }
        float diff = std::abs(density - devicePixelRatio);
        if (diff < bestDensityDiff) {
            bestDensityDiff = diff;
            bestSrc = url;
            bestDensity = density;
        }
    }
    currentSrc = bestSrc;
    intrinsicDensity = bestDensity > 0.0f ? bestDensity : 1.0f;
    naturalSize = {0, 0};
    loadState = ImageWidgetState::Idle;
}
void Image::layout(const Rect& parentBounds) {
    if (!layoutDirty && lastLayoutParentBounds.w == parentBounds.w && lastLayoutParentBounds.h == parentBounds.h) {
        float dx = parentBounds.x - lastLayoutParentBounds.x;
        float dy = parentBounds.y - lastLayoutParentBounds.y;
        if (dx != 0.0f || dy != 0.0f) {
            translateLayout(dx, dy);
        }
        return;
    }
    Widget::layout(parentBounds);
    auto& s = computedStyle;
    if ((naturalSize.x <= 0.0f || naturalSize.y <= 0.0f) && !currentSrc.empty()) {
        naturalSize = probeImageNaturalSize(currentSrc);
    }
    float naturalW = naturalSize.x > 0.0f ? (naturalSize.x / intrinsicDensity) : 300.0f;
    float naturalH = naturalSize.y > 0.0f ? (naturalSize.y / intrinsicDensity) : 150.0f;
    float ratio = naturalH > 0.0f ? naturalW / naturalH : 1.0f;
    bool hasW = s.width.isSet();
    bool hasH = s.height.isSet();
    if (!hasW && !hasH) {
        bounds.w = naturalW + s.padding.horizontal();
        bounds.h = naturalH + s.padding.vertical();
    } else if (hasW && !hasH) {
        float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
        bounds.h = contentW / std::max(0.001f, ratio) + s.padding.vertical();
    } else if (!hasW && hasH) {
        float contentH = std::max(0.0f, bounds.h - s.padding.vertical());
        bounds.w = contentH * ratio + s.padding.horizontal();
    }
}
void Image::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    Rect content = {
        bounds.x + computedStyle.padding.left,
        bounds.y + computedStyle.padding.top,
        std::max(0.0f, bounds.w - computedStyle.padding.horizontal()),
        std::max(0.0f, bounds.h - computedStyle.padding.vertical())
    };
    if (content.w <= 0.0f || content.h <= 0.0f || currentSrc.empty()) {
        renderChildren(renderer);
        return;
    }
    if (loadState == ImageWidgetState::Idle) {
        loadState = ImageWidgetState::Loading;
    }
    Vec2 natural = { naturalSize.x / intrinsicDensity, naturalSize.y / intrinsicDensity };
    if (natural.x <= 0.0f || natural.y <= 0.0f) {
        Vec2 rawSize = renderer.imageSize(currentSrc);
        natural = { rawSize.x / intrinsicDensity, rawSize.y / intrinsicDensity };
        if (rawSize.x > 0.0f && rawSize.y > 0.0f) {
            naturalSize = rawSize;
            if (loadState == ImageWidgetState::Loading) {
                loadState = ImageWidgetState::Complete;
                if (onLoad) onLoad();
            }
        } else {
            if (loadState == ImageWidgetState::Loading) {
                loadState = ImageWidgetState::Error;
                if (onError) onError();
            }
            natural = {content.w, content.h};
        }
    }
    Rect draw = content;
    float scaleX = natural.x > 0.0f ? content.w / natural.x : 1.0f;
    float scaleY = natural.y > 0.0f ? content.h / natural.y : 1.0f;
    bool clipToContent = false;
    if (computedStyle.objectFit == ObjectFit::Contain ||
        computedStyle.objectFit == ObjectFit::ScaleDown) {
        float scale = std::min(scaleX, scaleY);
        if (computedStyle.objectFit == ObjectFit::ScaleDown) {
            scale = std::min(1.0f, scale);
        }
        draw.w = natural.x * scale;
        draw.h = natural.y * scale;
        draw.x = content.x + (content.w - draw.w) * computedStyle.objectPosition.x +
                 computedStyle.objectPositionOffset.x;
        draw.y = content.y + (content.h - draw.h) * computedStyle.objectPosition.y +
                 computedStyle.objectPositionOffset.y;
    } else if (computedStyle.objectFit == ObjectFit::Cover) {
        float scale = std::max(scaleX, scaleY);
        draw.w = natural.x * scale;
        draw.h = natural.y * scale;
        draw.x = content.x + (content.w - draw.w) * computedStyle.objectPosition.x +
                 computedStyle.objectPositionOffset.x;
        draw.y = content.y + (content.h - draw.h) * computedStyle.objectPosition.y +
                 computedStyle.objectPositionOffset.y;
        clipToContent = true;
    } else if (computedStyle.objectFit == ObjectFit::None) {
        draw.w = natural.x;
        draw.h = natural.y;
        draw.x = content.x + (content.w - draw.w) * computedStyle.objectPosition.x +
                 computedStyle.objectPositionOffset.x;
        draw.y = content.y + (content.h - draw.h) * computedStyle.objectPosition.y +
                 computedStyle.objectPositionOffset.y;
        clipToContent = true;
    }
    if (loadState == ImageWidgetState::Error && !alt.empty()) {
        renderer.pushScissor(content);
        Rect altDraw = content;
        float fontSize = computedStyle.fontSize > 0.0f ? computedStyle.fontSize : 14.0f;
        std::string fontName = computedStyle.fontFamily.empty() ? "sans-serif" : computedStyle.fontFamily;
        altDraw.y += (content.h - fontSize) * 0.5f;
        altDraw.x += 4.0f;
        renderer.drawText(alt, {altDraw.x, altDraw.y}, Color(0.5f, 0.5f, 0.5f, 1.0f), fontSize, FontWeight::Normal, fontName,
                          computedStyle.fontStyle, computedStyle.direction, computedStyle.unicodeBidi);
        renderer.popScissor();
        renderChildren(renderer);
        return;
    }
    if (clipToContent) {
        Rect visible;
        if (intersectRects(draw, content, visible)) {
            Rect sourceUv(
                (visible.x - draw.x) / std::max(1.0f, draw.w),
                (visible.y - draw.y) / std::max(1.0f, draw.h),
                visible.w / std::max(1.0f, draw.w),
                visible.h / std::max(1.0f, draw.h));
            renderer.drawImage(currentSrc, visible, sourceUv, computedStyle.opacity);
        }
    } else {
        renderer.drawImage(currentSrc, draw, computedStyle.opacity);
    }
    renderChildren(renderer);
}
void ProgressBar::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    renderer.drawRoundedRect(bounds, s.backgroundColor.a > 0 ?
        s.backgroundColor : Color(1, 1, 1, 0.1f), s.borderRadius);
    if (progress > 0) {
        Rect fillRect = bounds;
        fillRect.w = bounds.w * std::clamp(progress, 0.0f, 1.0f);
        renderer.drawRoundedRect(fillRect, barColor, s.borderRadius);
    }
    renderChildren(renderer);
}
void Canvas::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    if (s.backgroundColor.a > 0) {
        renderer.drawRoundedRect(bounds, s.backgroundColor, s.borderRadius);
    }
    if (onDraw) {
        renderer.flush();
        renderer.pushScissor(bounds);
        onDraw(renderer, bounds);
        renderer.flush();
        renderer.popScissor();
    }
    renderChildren(renderer);
}
void VirtualList::setItemCount(size_t count) {
    if (itemCount == count) return;
    itemCount = count;
    forceRebuild_ = true;
    markLayoutDirty();
}
void VirtualList::refresh() {
    forceRebuild_ = true;
    markLayoutDirty();
}
void VirtualList::scrollToIndex(size_t index, VirtualListScrollStrategy strategy) {
    if (itemCount == 0) return;
    itemHeight = std::max(1.0f, itemHeight);
    index = std::min(index, itemCount - 1);
    float viewportH = std::max(0.0f, bounds.h - computedStyle.padding.vertical());
    if (viewportH <= 0.0f) {
        targetScrollY = itemHeight * static_cast<float>(index);
        scrollY = targetScrollY;
        forceRebuild_ = true;
        markLayoutDirty();
        return;
    }

    float itemTop = itemHeight * static_cast<float>(index);
    float itemBottom = itemTop + itemHeight;
    float target = scrollY;
    switch (strategy) {
    case VirtualListScrollStrategy::Start:
        target = itemTop;
        break;
    case VirtualListScrollStrategy::Center:
        target = itemTop - (viewportH - itemHeight) * 0.5f;
        break;
    case VirtualListScrollStrategy::End:
        target = itemBottom - viewportH;
        break;
    case VirtualListScrollStrategy::Nearest:
    default:
        if (itemTop < scrollY) {
            target = itemTop;
        } else if (itemBottom > scrollY + viewportH) {
            target = itemBottom - viewportH;
        } else {
            return;
        }
        break;
    }

    float maxScroll = std::max(0.0f, contentHeight - bounds.h);
    targetScrollY = std::clamp(target, 0.0f, maxScroll);
    scrollY = targetScrollY;
    forceRebuild_ = true;
    rebuildVisibleItems();
}
void VirtualList::layout(const Rect& parentBounds) {
    if (itemHeight <= 0.0f) {
        itemHeight = 1.0f;
    }
    if (computedStyle.display == Display::None) {
        bounds = {parentBounds.x, parentBounds.y, 0.0f, 0.0f};
        contentHeight = 0.0f;
        children.clear();
        visibleStart_ = visibleEnd_ = 0;
        layoutDirty = false;
        return;
    }
    if (!layoutDirty && lastLayoutParentBounds.w == parentBounds.w && lastLayoutParentBounds.h == parentBounds.h) {
        float dx = parentBounds.x - lastLayoutParentBounds.x;
        float dy = parentBounds.y - lastLayoutParentBounds.y;
        if (dx != 0.0f || dy != 0.0f) {
            translateLayout(dx, dy);
        }
        contentHeight = computedStyle.padding.vertical() + itemHeight * static_cast<float>(itemCount);
        clampScroll();
        rebuildVisibleItems();
        return;
    }
    if (!layoutDirty && rectEqual(lastLayoutParentBounds, parentBounds)) {
        contentHeight = computedStyle.padding.vertical() + itemHeight * static_cast<float>(itemCount);
        clampScroll();
        rebuildVisibleItems();
        return;
    }

    lastLayoutParentBounds = parentBounds;
    auto& s = computedStyle;
    bool heightProvidedByParentFlex = consumesParentMainAxisHeight(this, s);
    float x = parentBounds.x + s.margin.left;
    float y = parentBounds.y + s.margin.top;
    float w = s.width.isSet() ? s.width.resolve(parentBounds.w) :
              (parentBounds.w < 9999 ? parentBounds.w - s.margin.horizontal() : 0);
    float h = s.height.isSet() ? s.height.resolve(parentBounds.h) :
              (parentBounds.h < 9999 ? parentBounds.h - s.margin.vertical() : 0);
    bool widthControlsRatio = s.aspectRatio > 0.0f && s.width.isSet() && !s.height.isSet();
    bool heightControlsRatio = s.aspectRatio > 0.0f && s.height.isSet() && !s.width.isSet();
    if (s.minWidth.isSet()) w = std::max(w, s.minWidth.resolve(parentBounds.w));
    if (s.maxWidth.isSet()) w = std::min(w, s.maxWidth.resolve(parentBounds.w));
    if (widthControlsRatio) h = w / s.aspectRatio;
    if (s.minHeight.isSet()) h = std::max(h, s.minHeight.resolve(parentBounds.h));
    if (s.maxHeight.isSet()) h = std::min(h, s.maxHeight.resolve(parentBounds.h));
    if (heightControlsRatio) w = h * s.aspectRatio;
    if (s.hasBoxSizing && s.boxSizing == BoxSizing::ContentBox) {
        if (s.width.isSet()) w += s.padding.horizontal() + usedBorderHorizontal(s);
        if (s.height.isSet()) h += s.padding.vertical() + usedBorderVertical(s);
    }

    bounds = {x, y, std::max(0.0f, w), std::max(0.0f, h)};
    contentHeight = s.padding.vertical() + itemHeight * static_cast<float>(itemCount);
    if (!s.height.isSet() && !heightProvidedByParentFlex && !clipsOverflow(s)) {
        bounds.h = std::max(bounds.h, contentHeight);
    }
    clampScroll();
    rebuildVisibleItems();
    layoutPositionedChildren();
    layoutDirty = false;
    lifecycleState = WidgetLifecycle::LayoutClean;
}
void VirtualList::update(const InputState& input) {
    float previousScroll = scrollY;
    float previousTargetScroll = targetScrollY;
    Widget::update(input);
    if (std::abs(previousScroll - scrollY) > 0.01f ||
        std::abs(previousTargetScroll - targetScrollY) > 0.01f) {
        rebuildVisibleItems();
    }
}
void VirtualList::rebuildVisibleItems() {
    itemHeight = std::max(1.0f, itemHeight);
    const Style& s = computedStyle;
    float contentX = bounds.x + s.padding.left;
    float contentY = bounds.y + s.padding.top;
    float contentW = std::max(0.0f, bounds.w - s.padding.horizontal());
    float viewportH = std::max(0.0f, bounds.h - s.padding.vertical());
    float listHeight = itemHeight * static_cast<float>(itemCount);

    size_t nextStart = 0;
    size_t nextEnd = 0;
    if (itemCount > 0 && viewportH > 0.0f) {
        float startPx = std::max(0.0f, scrollY - overdraw);
        float endPx = std::min(listHeight, scrollY + viewportH + overdraw);
        nextStart = std::min(itemCount, static_cast<size_t>(std::floor(startPx / itemHeight)));
        nextEnd = std::min(itemCount, static_cast<size_t>(std::ceil(endPx / itemHeight)));
        if (nextEnd <= nextStart) {
            nextEnd = std::min(itemCount, nextStart + 1);
        }
    }

    bool rangeChanged = nextStart != visibleStart_ || nextEnd != visibleEnd_;
    bool widthChanged = std::abs(contentW - lastBuildWidth_) > 0.5f;
    bool itemHeightChanged = std::abs(itemHeight - lastBuildItemHeight_) > 0.01f;
    if (forceRebuild_ || rangeChanged || widthChanged || itemHeightChanged) {
        children.clear();
        children.reserve(nextEnd - nextStart);
        if (!childArena) {
            childArena = detail::makeWidgetArena();
        }
        for (size_t index = nextStart; index < nextEnd; ++index) {
            detail::WidgetArenaAllocator<Panel> allocator(childArena);
            auto row = std::allocate_shared<Panel>(allocator, itemClassName);
            row->type = "virtual-list-item";
            row->parent = this;
            row->style.height = CSSValue::px(itemHeight);
            row->style.width = CSSValue::pct(100.0f);
            row->reserveChildren(2);
            if (itemBuilder) {
                itemBuilder(row.get(), index);
            }
            children.push_back(std::move(row));
        }
        visibleStart_ = nextStart;
        visibleEnd_ = nextEnd;
        lastBuildWidth_ = contentW;
        lastBuildItemHeight_ = itemHeight;
        forceRebuild_ = false;
    }

    Application* app = Application::instance();
    StyleSheet* sheet = app ? &app->stylesheet() : nullptr;
    for (size_t offset = 0; offset < children.size(); ++offset) {
        auto& row = children[offset];
        if (!row) continue;
        if (sheet) {
            row->resolveStyles(*sheet);
        }
        size_t index = visibleStart_ + offset;
        Rect rowArea = {
            contentX,
            contentY + static_cast<float>(index) * itemHeight,
            contentW,
            itemHeight
        };
        row->layout(rowArea);
    }
    subtreeStyleDirty = false;
}
void StatCard::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    auto& s = computedStyle;
    bool hasScale = (renderScale != 1.0f && !layoutObject);
    if (hasScale) {
        renderer.pushScale(renderScale, bounds.center());
    }
    if (s.boxShadow.blur > 0) {
        renderer.drawBoxShadow(bounds, s.boxShadow, s.borderRadius);
    }
    Color bgColor = s.backgroundColor;
    if (bgColor.a < 0.01f) bgColor = Color::fromHex("#131420");
    if (s.hasHoverBg && hoverAnim > 0) {
        bgColor = Color::lerp(bgColor, s.hoverBackgroundColor, hoverAnim);
    }
    renderer.drawRoundedRect(bounds, bgColor, s.borderRadius, s.opacity);
    BoxShadow glow;
    glow.blur = 15 + hoverAnim * 10;
    glow.color = accentColor;
    glow.color.a = 0.1f + hoverAnim * 0.1f;
    renderer.drawBoxShadow(bounds, glow, s.borderRadius);
    Rect accentBar = {bounds.x + 2, bounds.y + 12, 4, bounds.h - 24};
    renderer.drawRoundedRect(accentBar, accentColor, BorderRadius(2));
    if (s.border.width > 0) {
        Border b = s.border;
        if (s.hasHoverBorder && hoverAnim > 0) {
            b.color = Color::lerp(s.border.color, s.hoverBorderColor, hoverAnim);
        }
        renderer.drawBorder(bounds, b, s.borderRadius);
    }
    float px = (s.padding.left > 0 ? s.padding.left : 24) + 10;
    float py = s.padding.top > 0 ? s.padding.top : 24;
    renderer.drawText(title, {bounds.x + px, bounds.y + py},
                      Color(1, 1, 1, 0.78f), 13, FontWeight::Bold);
    renderer.drawText(value, {bounds.x + px, bounds.y + py + 22},
                      Color(1, 1, 1, 1.0f), 32, FontWeight::Bold);
    renderer.drawText(subtitle, {bounds.x + px, bounds.y + bounds.h - 35},
                      accentColor, 12);
    renderChildren(renderer);
    if (hasScale) {
        renderer.popScale();
    }
}
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
void Internal_OnWindowEvent(void* appPtr, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* app = static_cast<Application*>(appPtr);
    if (!app) return;
    switch (msg) {
    case WM_CLOSE:
        app->running = false;
        break;
    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        app->input().windowSize = {(float)w, (float)h};
        if (app->root()) {
            app->root()->resetTransientMotion();
        }
        UIEvent event;
        event.type = UIEventType::WindowResized;
        event.position = app->input().windowSize;
        app->emit(std::move(event));
        app->requestRedraw();
        app->renderFrame();
        break;
    }
    case WM_MOUSEMOVE: {
        Vec2 oldPos = app->input().mousePos;
        app->input().mousePos = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        app->input().mouseDelta = app->input().mousePos - oldPos;
        app->dispatchMouseMove(app->input().mousePos.x, app->input().mousePos.y, app->input().mouseDelta.x, app->input().mouseDelta.y);
        UIEvent event;
        event.type = UIEventType::MouseMove;
        event.position = app->input().mousePos;
        event.delta = app->input().mouseDelta;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        int btn = (msg == WM_LBUTTONDOWN) ? 0 : (msg == WM_RBUTTONDOWN ? 1 : 2);
        app->input().mouseDown[btn] = true;
        app->input().mouseClicked[btn] = true;
        app->input().mouseClickCount[btn] = 1;
        app->dispatchMouseDown(btn + 1, (float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), 1);
        UIEvent event;
        event.type = UIEventType::MouseDown;
        event.position = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        event.button = btn + 1;
        event.clickCount = 1;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        int btn = (msg == WM_LBUTTONUP) ? 0 : (msg == WM_RBUTTONUP ? 1 : 2);
        app->input().mouseDown[btn] = false;
        app->input().mouseReleased[btn] = true;
        app->dispatchMouseUp(btn + 1, (float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
        UIEvent event;
        event.type = UIEventType::MouseUp;
        event.position = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
        event.button = btn + 1;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_MOUSEWHEEL: {
        float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
        app->input().scroll.y += delta;
        app->dispatchMouseWheel(app->input().mousePos.x, app->input().mousePos.y, 0.0f, delta);
        UIEvent event;
        event.type = UIEventType::MouseWheel;
        event.delta = {0, delta};
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_CHAR: {
        if (wParam >= 32) {
            char utf8[5] = {0};
            WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)&wParam, 1, utf8, 4, nullptr, nullptr);
            app->input().text += utf8;
            app->dispatchTextInput(utf8);
            UIEvent event;
            event.type = UIEventType::TextInput;
            event.text = utf8;
            app->emit(std::move(event));
            app->requestRedraw();
        }
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        app->input().keyCode = (int)wParam;
        app->input().modifiers = MOD_NONE;
        if (GetKeyState(VK_SHIFT) & 0x8000) app->input().modifiers |= MOD_SHIFT;
        if (GetKeyState(VK_CONTROL) & 0x8000) app->input().modifiers |= MOD_CTRL;
        if (GetKeyState(VK_MENU) & 0x8000) app->input().modifiers |= MOD_ALT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) app->input().modifiers |= MOD_GUI;
        app->dispatchKeyDown(app->input().keyCode, app->input().modifiers);
        UIEvent event;
        event.type = UIEventType::KeyDown;
        event.keyCode = app->input().keyCode;
        event.modifiers = app->input().modifiers;
        app->emit(std::move(event));
        app->dispatchKeyAction(app->input().keyCode, app->input().modifiers);
        app->requestRedraw();
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        app->dispatchKeyUp((int)wParam, app->input().modifiers);
        UIEvent event;
        event.type = UIEventType::KeyUp;
        event.keyCode = (int)wParam;
        app->emit(std::move(event));
        app->requestRedraw();
        break;
    }
    case WM_PAINT:
        app->requestRedraw();
        app->renderFrame();
        break;
    }
}
#else
void Internal_OnWindowEvent(void* app, uint32_t msg, uint64_t w, uint64_t l) {}
static void fluxuiPlatformEventHandler(void* ctx, const PlatformInputEvent& event) {
    Application* app = static_cast<Application*>(ctx);
    if (!app) return;
    switch (event.type) {
    case PlatformInputEvent::Close:
        app->running = false;
        break;
    case PlatformInputEvent::Resize: {
        int w = (int)event.x;
        int h = (int)event.y;
        app->input().windowSize = {event.x, event.y};
        if (app->root()) {
            app->root()->resetTransientMotion();
        }
        UIEvent uiEvent;
        uiEvent.type = UIEventType::WindowResized;
        uiEvent.position = app->input().windowSize;
        app->emit(std::move(uiEvent));
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::MouseMove: {
        Vec2 oldPos = app->input().mousePos;
        app->input().mousePos = {event.x, event.y};
        app->input().mouseDelta = app->input().mousePos - oldPos;
        app->dispatchMouseMove(event.x, event.y, app->input().mouseDelta.x, app->input().mouseDelta.y);
        UIEvent uiEvent;
        uiEvent.type = UIEventType::MouseMove;
        uiEvent.position = app->input().mousePos;
        uiEvent.delta = app->input().mouseDelta;
        app->emit(std::move(uiEvent));
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::MouseDown: {
        int btn = event.button;
        if (btn >= 0 && btn < 3) {
            app->input().mouseDown[btn] = true;
            app->input().mouseClicked[btn] = true;
            app->input().mouseClickCount[btn] = 1;
        }
        app->input().mousePos = {event.x, event.y};
        app->dispatchMouseDown(btn + 1, event.x, event.y, 1);
        UIEvent uiEvent;
        uiEvent.type = UIEventType::MouseDown;
        uiEvent.position = {event.x, event.y};
        uiEvent.button = btn + 1;
        uiEvent.clickCount = 1;
        app->emit(std::move(uiEvent));
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::MouseUp: {
        int btn = event.button;
        if (btn >= 0 && btn < 3) {
            app->input().mouseDown[btn] = false;
            app->input().mouseReleased[btn] = true;
        }
        app->input().mousePos = {event.x, event.y};
        app->dispatchMouseUp(btn + 1, event.x, event.y);
        UIEvent uiEvent;
        uiEvent.type = UIEventType::MouseUp;
        uiEvent.position = {event.x, event.y};
        uiEvent.button = btn + 1;
        app->emit(std::move(uiEvent));
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::Scroll: {
        app->input().scroll.y += event.y;
        app->input().scroll.x += event.x;
        app->dispatchMouseWheel(app->input().mousePos.x, app->input().mousePos.y, event.x, event.y);
        UIEvent uiEvent;
        uiEvent.type = UIEventType::MouseWheel;
        uiEvent.delta = {event.x, event.y};
        app->emit(std::move(uiEvent));
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::KeyDown: {
        app->input().keyCode = event.button;
        app->input().modifiers = event.modifiers;
        app->dispatchKeyDown(event.button, event.modifiers);
        UIEvent uiEvent;
        uiEvent.type = UIEventType::KeyDown;
        uiEvent.keyCode = event.button;
        uiEvent.modifiers = event.modifiers;
        app->emit(std::move(uiEvent));
        app->dispatchKeyAction(event.button, event.modifiers);
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::KeyUp: {
        app->dispatchKeyUp(event.button, event.modifiers);
        UIEvent uiEvent;
        uiEvent.type = UIEventType::KeyUp;
        uiEvent.keyCode = event.button;
        uiEvent.modifiers = event.modifiers;
        app->emit(std::move(uiEvent));
        app->requestRedraw();
        break;
    }
    case PlatformInputEvent::TextInput: {
        if (event.text[0] != '\0') {
            app->input().text += event.text;
            app->dispatchTextInput(event.text);
            UIEvent uiEvent;
            uiEvent.type = UIEventType::TextInput;
            uiEvent.text = event.text;
            app->emit(std::move(uiEvent));
            app->requestRedraw();
        }
        break;
    }
    case PlatformInputEvent::Expose:
        app->requestRedraw();
        break;
    }
}
#endif
bool Application::init(const std::string& title, int width, int height) {
    g_activeApp = this;
    lastTime_ = std::chrono::high_resolution_clock::now();
    if (!Platform::init()) return false;
    PlatformWindowConfig config;
    config.title = title;
    config.width = width;
    config.height = height;
    window_ = Platform::createWindow(config);
    if (!window_) return false;
#ifdef _WIN32
    SetWindowLongPtr((HWND)window_, GWLP_USERDATA, (LONG_PTR)this);
#else
    Platform::setEventCallback(this, fluxuiPlatformEventHandler);
#endif
    renderer_.setBackend(backendPreference_);
    if (!renderer_.init(window_)) return false;
    defaultCursor_ = Platform::createSystemCursor(CursorType::Default);
    pointerCursor_ = Platform::createSystemCursor(CursorType::Pointer);
    textCursor_ = Platform::createSystemCursor(CursorType::Text);
    resizeNWSECursor_ = Platform::createSystemCursor(CursorType::ResizeNWSE);
    routes_.reserve(FLUXUI_PREALLOC_ROUTES);
    eventListeners_.reserve(FLUXUI_PREALLOC_EVENT_LISTENERS);
    root_ = std::make_shared<Panel>();
    root_->id = "root";
    root_->className = "root";
    root_->reserveChildren(FLUXUI_PREALLOC_ROOT_CHILDREN);
    root_->computedStyle.display = Display::Flex;
    root_->computedStyle.flexDirection = FlexDirection::Row;
    input_.windowSize = {(float)width, (float)height};
    return true;
}
bool Application::init(const std::string& title, int width, int height, RenderBackendType backend) {
    setBackend(backend);
    return init(title, width, height);
}
void Application::setBackend(RenderBackendType backend) {
    backendPreference_ = backend;
    renderer_.setBackend(backend);
}
void Application::runOnMainThread(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mainThreadTasksMutex_);
    mainThreadTasks_.push_back(std::move(task));
}
void Application::processMainThreadTasks() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(mainThreadTasksMutex_);
        tasks = std::move(mainThreadTasks_);
        mainThreadTasks_.clear();
    }
    for (const auto& task : tasks) {
        if (task) task();
    }
}
void Application::processEvents() {
    input_.mouseClicked[0] = input_.mouseClicked[1] = input_.mouseClicked[2] = false;
    input_.mouseReleased[0] = input_.mouseReleased[1] = input_.mouseReleased[2] = false;
    input_.mouseClickCount[0] = input_.mouseClickCount[1] = input_.mouseClickCount[2] = 0;
    input_.mouseDelta = {0, 0};
    input_.scroll = {0, 0};
    input_.text.clear();
    input_.keyCode = 0;
    Platform::processEvents(running);
    processMainThreadTasks();
}
void Application::updateCursor(CursorType cursor) {
    if (cursor == activeCursor_) return;
    Platform::setCursor((NativeCursorHandle)
        (cursor == CursorType::Pointer ? pointerCursor_ :
         (cursor == CursorType::Text ? textCursor_ :
          (cursor == CursorType::ResizeNWSE ? resizeNWSECursor_ : defaultCursor_))));
    activeCursor_ = cursor;
}
bool Application::loadStylesheet(const std::string& path) {
    bool ok = stylesheet_.loadFile(path);
    if (ok) {
        std::string dir;
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            dir = path.substr(0, lastSlash + 1);
        }
        for (const auto& ff : stylesheet_.fontFaces) {
            std::string fontPath = ff.src;
            if (!fontPath.empty() && fontPath[0] != '/' && fontPath[0] != '\\' && (fontPath.size() < 2 || fontPath[1] != ':')) {
                fontPath = dir + fontPath;
            }
            renderer_.registerCustomFont(ff.fontFamily, fontPath);
        }
    }
    if (ok && root_) root_->markStyleDirtyRecursive();
    if (ok) needsRedraw_ = true;
    return ok;
}
void Application::addStylesheet(const std::string& css) {
    stylesheet_.parse(css);
    for (const auto& ff : stylesheet_.fontFaces) {
        renderer_.registerCustomFont(ff.fontFamily, ff.src);
    }
    if (root_) root_->markStyleDirtyRecursive();
    needsRedraw_ = true;
}
size_t Application::on(UIEventType type, EventCallback callback) {
    if (!callback) return 0;
    size_t id = nextEventListenerId_++;
    eventListeners_.push_back({id, type, std::move(callback)});
    return id;
}
void Application::off(size_t listenerId) {
    eventListeners_.erase(
        std::remove_if(eventListeners_.begin(), eventListeners_.end(),
                       [listenerId](const EventListener& listener) {
                           return listener.id == listenerId;
                       }),
        eventListeners_.end());
}
void Application::emit(UIEvent event) {
    for (auto& listener : eventListeners_) {
        if (listener.type == UIEventType::Any || listener.type == event.type) {
            listener.callback(event);
            if (event.handled) break;
        }
    }
}
size_t Application::addAction(const std::string& name,
                              int keyCode,
                              int modifiers,
                              ActionCallback callback) {
    if (name.empty() || !callback) return 0;
    size_t id = nextActionId_++;
    actionBindings_.push_back({id, name, keyCode, modifiers, std::move(callback)});
    return id;
}
void Application::removeAction(size_t actionId) {
    actionBindings_.erase(
        std::remove_if(actionBindings_.begin(), actionBindings_.end(),
                       [actionId](const ActionBinding& binding) {
                           return binding.id == actionId;
                       }),
        actionBindings_.end());
}
static bool parseKeyStroke(const std::string& keyStr, int& keyCode, int& modifiers) {
    modifiers = MOD_NONE;
    keyCode = 0;
    
    std::string key = keyStr;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });
    
    std::vector<std::string> parts;
    std::string part;
    for (char c : key) {
        if (c == '-' || c == '+') {
            if (!part.empty()) {
                parts.push_back(part);
                part.clear();
            }
        } else {
            part += c;
        }
    }
    if (!part.empty()) {
        parts.push_back(part);
    }
    
    if (parts.empty()) return false;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& p = parts[i];
        if (p == "ctrl" || p == "control" || p == "cmd" || p == "command") {
            modifiers |= MOD_CTRL;
        } else if (p == "shift") {
            modifiers |= MOD_SHIFT;
        } else if (p == "alt" || p == "option") {
            modifiers |= MOD_ALT;
        } else if (p == "win" || p == "super" || p == "meta" || p == "gui") {
            modifiers |= MOD_GUI;
        } else {
            if (p.size() == 1) {
                char c = p[0];
                if (c >= 'a' && c <= 'z') {
                    keyCode = c - 'a' + 'A';
                } else if (c >= '0' && c <= '9') {
                    keyCode = c;
                } else {
                    switch (c) {
                        case ';': keyCode = 0xBA; break;
                        case '=': keyCode = 0xBB; break;
                        case ',': keyCode = 0xBC; break;
                        case '-': keyCode = 0xBD; break;
                        case '.': keyCode = 0xBE; break;
                        case '/': keyCode = 0xBF; break;
                        case '`': keyCode = 0xC0; break;
                        case '[': keyCode = 0xDB; break;
                        case '\\': keyCode = 0xDC; break;
                        case ']': keyCode = 0xDD; break;
                        case '\'': keyCode = 0xDE; break;
                        case ' ': keyCode = 0x20; break;
                        default: keyCode = c; break;
                    }
                }
            } else {
                if (p == "escape" || p == "esc") keyCode = 0x1B;
                else if (p == "enter" || p == "return") keyCode = 0x0D;
                else if (p == "tab") keyCode = 0x09;
                else if (p == "space") keyCode = 0x20;
                else if (p == "backspace") keyCode = 0x08;
                else if (p == "delete" || p == "del") keyCode = 0x2E;
                else if (p == "left") keyCode = 0x25;
                else if (p == "up") keyCode = 0x26;
                else if (p == "right") keyCode = 0x27;
                else if (p == "down") keyCode = 0x28;
                else if (p == "pageup" || p == "pgup") keyCode = 0x21;
                else if (p == "pagedown" || p == "pgdn") keyCode = 0x22;
                else if (p == "end") keyCode = 0x23;
                else if (p == "home") keyCode = 0x24;
                else if (p == "insert" || p == "ins") keyCode = 0x2D;
                else if (p.size() >= 2 && p[0] == 'f') {
                    int num = std::atoi(p.substr(1).c_str());
                    if (num >= 1 && num <= 12) {
                        keyCode = 0x70 + (num - 1);
                    }
                }
            }
        }
    }
    
    return keyCode != 0;
}

static bool widgetMatchesContext(const Widget* widget, const std::string& context) {
    if (context.empty() || context == "any" || context == "*") return true;
    if (!widget) return false;
    
    if (context[0] == '#') {
        std::string_view id(widget->id);
        return id.size() == context.size() - 1 && 
               id.compare(0, id.size(), context.data() + 1, context.size() - 1) == 0;
    }
    
    if (context[0] == '.') {
        std::string_view targetClass(context.data() + 1, context.size() - 1);
        std::string_view className(widget->className);
        size_t pos = 0;
        while (true) {
            pos = className.find(targetClass, pos);
            if (pos == std::string_view::npos) break;
            
            bool leftOk = (pos == 0 || std::isspace(static_cast<unsigned char>(className[pos - 1])));
            bool rightOk = (pos + targetClass.size() == className.size() || 
                            std::isspace(static_cast<unsigned char>(className[pos + targetClass.size()])));
            if (leftOk && rightOk) return true;
            pos += 1;
        }
        return false;
    }
    
    return widget->type == context;
}

static Widget* findFocusedWidgetHelper(Widget* w) {
    if (!w || !w->visible) return nullptr;
    for (auto& child : w->children) {
        Widget* f = findFocusedWidgetHelper(child.get());
        if (f) return f;
    }
    if (w->focused) return w;
    return nullptr;
}

Widget* Application::focusedWidget() {
    return findFocusedWidgetHelper(root_.get());
}

void Application::dispatchMouseDown(int button, float x, float y, int clickCount) {
    Widget* target = nullptr;
    std::vector<Widget*> fixedWidgets;
    std::function<void(Widget*)> gatherFixed = [&](Widget* w) {
        if (!w || !w->visible || w->computedStyle.display == Display::None) return;
        if (w->computedStyle.position == Position::Fixed) {
            fixedWidgets.push_back(w);
            return;
        }
        for (auto& child : w->children) {
            gatherFixed(child.get());
        }
    };
    if (root_) {
        gatherFixed(root_.get());
    }
    for (auto it = fixedWidgets.rbegin(); it != fixedWidgets.rend(); ++it) {
        if (Widget* t = (*it)->hitTest({x, y}, true)) {
            target = t;
            break;
        }
    }
    if (!target && root_) {
        target = root_->hitTest({x, y}, true);
    }
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "mousedown";
    ev.target = target;
    ev.mousePos = {x, y};
    ev.button = button;
    ev.clickCount = clickCount;
    target->dispatchEvent(ev);
    if (button >= 1 && button <= 3) {
        lastMouseDownTarget_[button - 1] = target;
    }
    if (button == 1 && !ev.defaultPrevented) {
        Widget* focusTarget = target;
        while (focusTarget) {
            if (focusTarget->type == "button" || focusTarget->computedStyle.cursor == CursorType::Pointer ||
                focusTarget->type == "textarea" || focusTarget->type == "input" ||
                dynamic_cast<TextInput*>(focusTarget) || dynamic_cast<TextArea*>(focusTarget) || dynamic_cast<Checkbox*>(focusTarget) ||
                dynamic_cast<Radio*>(focusTarget) || dynamic_cast<RangeInput*>(focusTarget) ||
                dynamic_cast<Select*>(focusTarget)) {
                break;
            }
            focusTarget = focusTarget->parent;
        }
        if (root_) {
            std::function<void(Widget*)> clearFocus = [&](Widget* w) {
                if (!w) return;
                if (w != focusTarget) {
                    w->focused = false;
                }
                for (auto& child : w->children) {
                    clearFocus(child.get());
                }
            };
            clearFocus(root_.get());
        }
        if (focusTarget) {
            focusTarget->focused = true;
        }
    }
}

void Application::dispatchMouseUp(int button, float x, float y) {
    Widget* target = nullptr;
    std::vector<Widget*> fixedWidgets;
    std::function<void(Widget*)> gatherFixed = [&](Widget* w) {
        if (!w || !w->visible || w->computedStyle.display == Display::None) return;
        if (w->computedStyle.position == Position::Fixed) {
            fixedWidgets.push_back(w);
            return;
        }
        for (auto& child : w->children) {
            gatherFixed(child.get());
        }
    };
    if (root_) {
        gatherFixed(root_.get());
    }
    for (auto it = fixedWidgets.rbegin(); it != fixedWidgets.rend(); ++it) {
        if (Widget* t = (*it)->hitTest({x, y}, true)) {
            target = t;
            break;
        }
    }
    if (!target && root_) {
        target = root_->hitTest({x, y}, true);
    }
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "mouseup";
    ev.target = target;
    ev.mousePos = {x, y};
    ev.button = button;
    target->dispatchEvent(ev);
    if (button == 1) {
        Widget* downTarget = (button >= 1 && button <= 3) ? lastMouseDownTarget_[button - 1] : nullptr;
        bool clickMatched = false;
        if (downTarget) {
            Widget* p = target;
            while (p) {
                if (p == downTarget) {
                    clickMatched = true;
                    break;
                }
                p = p->parent;
            }
        }
        if (clickMatched) {
            Event clickEv;
            clickEv.type = "click";
            clickEv.target = target;
            clickEv.mousePos = {x, y};
            clickEv.button = 1;
            target->dispatchEvent(clickEv);
        }
    }
}

void Application::dispatchMouseMove(float x, float y, float dx, float dy) {
    Widget* target = nullptr;
    std::vector<Widget*> fixedWidgets;
    std::function<void(Widget*)> gatherFixed = [&](Widget* w) {
        if (!w || !w->visible || w->computedStyle.display == Display::None) return;
        if (w->computedStyle.position == Position::Fixed) {
            fixedWidgets.push_back(w);
            return;
        }
        for (auto& child : w->children) {
            gatherFixed(child.get());
        }
    };
    if (root_) {
        gatherFixed(root_.get());
    }
    for (auto it = fixedWidgets.rbegin(); it != fixedWidgets.rend(); ++it) {
        if (Widget* t = (*it)->hitTest({x, y}, true)) {
            target = t;
            break;
        }
    }
    if (!target && root_) {
        target = root_->hitTest({x, y}, true);
    }
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "mousemove";
    ev.target = target;
    ev.mousePos = {x, y};
    ev.mouseDelta = {dx, dy};
    target->dispatchEvent(ev);
    if (target != lastHoveredWidget_) {
        if (lastHoveredWidget_) {
            Event outEv;
            outEv.type = "mouseout";
            outEv.target = lastHoveredWidget_;
            outEv.mousePos = {x, y};
            outEv.mouseDelta = {dx, dy};
            lastHoveredWidget_->dispatchEvent(outEv);
            Widget* p = lastHoveredWidget_;
            while (p && p != target && !p->bounds.contains({x, y})) {
                Event leaveEv;
                leaveEv.type = "mouseleave";
                leaveEv.target = p;
                leaveEv.bubbles = false;
                leaveEv.mousePos = {x, y};
                p->dispatchEvent(leaveEv);
                p = p->parent;
            }
        }
        if (target) {
            Event overEv;
            overEv.type = "mouseover";
            overEv.target = target;
            overEv.mousePos = {x, y};
            overEv.mouseDelta = {dx, dy};
            target->dispatchEvent(overEv);
            Widget* p = target;
            std::vector<Widget*> enterChain;
            while (p && p != lastHoveredWidget_ && p->bounds.contains({x, y})) {
                enterChain.push_back(p);
                p = p->parent;
            }
            for (auto it = enterChain.rbegin(); it != enterChain.rend(); ++it) {
                Event enterEv;
                enterEv.type = "mouseenter";
                enterEv.target = *it;
                enterEv.bubbles = false;
                enterEv.mousePos = {x, y};
                (*it)->dispatchEvent(enterEv);
            }
        }
        lastHoveredWidget_ = target;
    }
}

void Application::dispatchMouseWheel(float x, float y, float dx, float dy) {
    Widget* target = nullptr;
    std::vector<Widget*> fixedWidgets;
    std::function<void(Widget*)> gatherFixed = [&](Widget* w) {
        if (!w || !w->visible || w->computedStyle.display == Display::None) return;
        if (w->computedStyle.position == Position::Fixed) {
            fixedWidgets.push_back(w);
            return;
        }
        for (auto& child : w->children) {
            gatherFixed(child.get());
        }
    };
    if (root_) {
        gatherFixed(root_.get());
    }
    for (auto it = fixedWidgets.rbegin(); it != fixedWidgets.rend(); ++it) {
        if (Widget* t = (*it)->hitTest({x, y}, true)) {
            target = t;
            break;
        }
    }
    if (!target && root_) {
        target = root_->hitTest({x, y}, true);
    }
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "wheel";
    ev.target = target;
    ev.mousePos = {x, y};
    ev.scroll = {dx, dy};
    target->dispatchEvent(ev);
    if (ev.defaultPrevented) {
        input_.scroll = {0.0f, 0.0f};
    }
}

void Application::dispatchKeyDown(int keyCode, int modifiers) {
    Widget* target = focusedWidget();
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "keydown";
    ev.target = target;
    ev.keyCode = keyCode;
    ev.modifiers = modifiers;
    target->dispatchEvent(ev);
    if (ev.defaultPrevented) {
        input_.keyCode = 0;
    }
}

void Application::dispatchKeyUp(int keyCode, int modifiers) {
    Widget* target = focusedWidget();
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "keyup";
    ev.target = target;
    ev.keyCode = keyCode;
    ev.modifiers = modifiers;
    target->dispatchEvent(ev);
}

void Application::dispatchTextInput(const std::string& text) {
    Widget* target = focusedWidget();
    if (!target) target = root_.get();
    if (!target) return;
    Event ev;
    ev.type = "textinput";
    ev.target = target;
    ev.text = text;
    target->dispatchEvent(ev);
    if (ev.defaultPrevented) {
        input_.text.clear();
    }
}

void Application::registerAction(const std::string& name, ActionCallback callback) {
    actionHandlers_[name] = std::move(callback);
}

void Application::addKeymap(const std::string& jsonContent) {
    try {
        auto j = nlohmann::json::parse(jsonContent);
        if (j.is_array()) {
            for (const auto& item : j) {
                std::string context = "";
                if (item.contains("context") && item["context"].is_string()) {
                    context = item["context"].get<std::string>();
                }
                
                if (item.contains("bindings") && item["bindings"].is_object()) {
                    for (auto& el : item["bindings"].items()) {
                        std::string keyStr = el.key();
                        std::string actionName = el.value().get<std::string>();
                        
                        int keyCode = 0;
                        int modifiers = MOD_NONE;
                        if (parseKeyStroke(keyStr, keyCode, modifiers)) {
                            keymapEntries_.push_back({keyCode, modifiers, context, actionName});
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse keymap JSON: " << e.what() << std::endl;
    }
}

bool Application::loadKeymap(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    addKeymap(buffer.str());
    return true;
}

bool Application::dispatchAction(const std::string& name) {
    if (name.empty()) return false;
    
    auto it = actionHandlers_.find(name);
    if (it != actionHandlers_.end() && it->second) {
        it->second(*this, name);
        requestRedraw();
        return true;
    }
    
    for (auto& binding : actionBindings_) {
        if (binding.name == name && binding.callback) {
            binding.callback(*this, binding.name);
            requestRedraw();
            return true;
        }
    }
    return false;
}

bool Application::dispatchKeyAction(int keyCode, int modifiers) {
    if (keyCode == 0) return false;
    
    // 1. Locate focused widget
    Widget* startWidget = focusedWidget();
    
    // 2. Walk up the parent chain and try to trigger a keymap entry matching keyCode + modifiers
    Widget* current = startWidget;
    while (current != nullptr) {
        for (const auto& entry : keymapEntries_) {
            if (entry.keyCode == keyCode && (entry.modifiers & modifiers) == entry.modifiers) {
                if (widgetMatchesContext(current, entry.context)) {
                    if (dispatchAction(entry.actionName)) {
                        return true;
                    }
                }
            }
        }
        current = current->parent;
    }
    
    // 3. Fallback: try global keymap entries (context empty or "any" or "*")
    for (const auto& entry : keymapEntries_) {
        if (entry.keyCode == keyCode && (entry.modifiers & modifiers) == entry.modifiers) {
            if (entry.context.empty() || entry.context == "any" || entry.context == "*") {
                if (dispatchAction(entry.actionName)) {
                    return true;
                }
            }
        }
    }
    
    // 4. Fallback 2: Check traditional hardcoded actionBindings_ to preserve FFI mappings
    for (auto& binding : actionBindings_) {
        if (binding.keyCode == keyCode &&
            (binding.modifiers & modifiers) == binding.modifiers &&
            binding.callback) {
            binding.callback(*this, binding.name);
            requestRedraw();
            return true;
        }
    }
    
    return false;
}
void Application::addRoute(const std::string& path, RouteBuilder builder) {
    if (path.empty() || !builder) return;
    routes_[path] = std::move(builder);
    if (currentRoute_.empty()) {
        currentRoute_ = path;
        routeDirty_ = true;
    }
}
void Application::setNotFoundRoute(RouteBuilder builder) {
    notFoundRoute_ = std::move(builder);
}
bool Application::navigate(const std::string& path) {
    if (path.empty()) return false;
    bool found = routes_.find(path) != routes_.end();
    if (!found && !notFoundRoute_) return false;
    if (path == currentRoute_ && !routeDirty_) return found;
    std::string previous = currentRoute_;
    currentRoute_ = path;
    routeDirty_ = true;
    UIEvent event;
    event.type = UIEventType::RouteChanged;
    event.route = currentRoute_;
    event.previousRoute = previous;
    emit(std::move(event));
    needsRedraw_ = true;
    return found;
}
bool Application::renderRoute(Widget* container) {
    if (!container) return false;
    auto route = routes_.find(currentRoute_);
    if (route == routes_.end()) {
        if (!notFoundRoute_) return false;
        container->clearChildren();
        notFoundRoute_(*this, container);
        routeDirty_ = false;
        container->markStyleDirtyRecursive();
        return false;
    }
    container->clearChildren();
    route->second(*this, container);
    routeDirty_ = false;
    container->markStyleDirtyRecursive();
    return true;
}
#ifdef _WIN32
#include <windows.h>
#endif
void Application::renderFrame() {
    int w = 0, h = 0;
    Platform::getWindowSize(window_, w, h);
    if (w <= 0 || h <= 0) {
        if (root_) root_->resetTransientMotion();
        needsRedraw_ = false;
        return;
    }
    needsRedraw_ = false;
    if (onUpdate) onUpdate(input_.deltaTime);
    if (stylesheet_.setViewportSize((float)w, (float)h)) {
        root_->markStyleDirtyRecursive();
    }

    // High-Fidelity chromium style rendering pipeline
    documentLifecycle = DocumentLifecycle::InStyleRecalc;
    root_->resolveStyles(stylesheet_);
    documentLifecycle = DocumentLifecycle::StyleClean;

    // Attach / Rebuild decoupled Blink-style Layout Object tree
    root_->attachLayoutTree();

    documentLifecycle = DocumentLifecycle::InLayout;
    if (root_->layoutObject) {
        LayoutConstraints constraints;
        constraints.availableWidth = (float)w;
        constraints.availableHeight = (float)h;
        constraints.parentWidth = (float)w;
        constraints.parentHeight = (float)h;
        constraints.emBase = 16.0f;
        root_->layoutObject->layout(constraints);
    } else {
        root_->layout({0, 0, (float)w, (float)h});
    }
    documentLifecycle = DocumentLifecycle::LayoutClean;

    // ResizeObserver processing cycle (matches Chromium Blink high-fidelity resize observations)
    int resizeIteration = 0;
    constexpr int maxResizeIterations = 10;
    bool sizeChanged = true;

    while (sizeChanged && resizeIteration < maxResizeIterations) {
        sizeChanged = false;
        
        // Deliver observations for each registered observer
        for (auto* obs : resizeObservers_) {
            std::vector<ResizeObserverEntry> activeObservations;
            obs->gatherObservations(activeObservations);
            if (!activeObservations.empty()) {
                obs->deliverObservations(activeObservations);
            }
        }

        // If a callback dirty-marked the style or layout, re-resolve and re-layout sequentially
        if (root_->layoutDirty || root_->styleDirty || root_->subtreeStyleDirty) {
            documentLifecycle = DocumentLifecycle::InStyleRecalc;
            root_->resolveStyles(stylesheet_);
            documentLifecycle = DocumentLifecycle::StyleClean;

            // Rebuild Layout Tree
            root_->attachLayoutTree();

            documentLifecycle = DocumentLifecycle::InLayout;
            if (root_->layoutObject) {
                LayoutConstraints constraints;
                constraints.availableWidth = (float)w;
                constraints.availableHeight = (float)h;
                constraints.parentWidth = (float)w;
                constraints.parentHeight = (float)h;
                constraints.emBase = 16.0f;
                root_->layoutObject->layout(constraints);
            } else {
                root_->layout({0, 0, (float)w, (float)h});
            }
            documentLifecycle = DocumentLifecycle::LayoutClean;

            sizeChanged = true;
            resizeIteration++;
        }
    }

    // Pre-Paint Phase: Compute paint property tree
    documentLifecycle = DocumentLifecycle::InPrePaint;
    PaintProperties rootProps;
    rootProps.translation = {0.0f, 0.0f};
    rootProps.scale = 1.0f;
    rootProps.clipRect = {0.0f, 0.0f, (float)w, (float)h};
    rootProps.hasClip = true;
    rootProps.opacity = 1.0f;
    root_->prePaint(rootProps);
    documentLifecycle = DocumentLifecycle::PrePaintClean;
    int documentKeyCode = normalizeTextEditingKey(input_.keyCode);
    if (documentKeyCode == 0x09) {
        bool backwards = (input_.modifiers & MOD_SHIFT) != 0;
        if (moveDocumentFocus(root_.get(), backwards)) {
            input_.keyCode = 0;
            input_.text.clear();
            needsRedraw_ = true;
        }
    }
    // Gather fixed widgets
    std::vector<Widget*> fixedWidgets;
    std::function<void(Widget*)> gatherFixed = [&](Widget* widget) {
        if (!widget || !widget->visible || widget->computedStyle.display == Display::None) return;
        if (widget->computedStyle.position == Position::Fixed) {
            fixedWidgets.push_back(widget);
            return;
        }
        for (auto& child : widget->children) {
            gatherFixed(child.get());
        }
    };
    gatherFixed(root_.get());

    // Update fixed widgets
    for (auto* fw : fixedWidgets) {
        fw->update(input_);
    }
    root_->update(input_);

    Widget* hitTarget = nullptr;
    for (auto it = fixedWidgets.rbegin(); it != fixedWidgets.rend(); ++it) {
        if (Widget* t = (*it)->hitTest(input_.mousePos, true)) {
            hitTarget = t;
            break;
        }
    }
    if (!hitTarget) {
        hitTarget = root_->hitTest(input_.mousePos, true);
    }

    if (input_.mouseClicked[0] && hitTarget) {
        UIEvent clickEvent;
        clickEvent.type = UIEventType::WidgetClick;
        clickEvent.target = hitTarget;
        clickEvent.position = input_.mousePos;
        clickEvent.button = 1;
        clickEvent.clickCount = input_.mouseClickCount[0];
        if (clickEvent.target) {
            emit(std::move(clickEvent));
        }
    }

    CursorType currentCursor = CursorType::Default;
    for (auto it = fixedWidgets.rbegin(); it != fixedWidgets.rend(); ++it) {
        CursorType c = (*it)->cursorAt(input_.mousePos);
        if (c != CursorType::Default) {
            currentCursor = c;
            break;
        }
    }
    if (currentCursor == CursorType::Default) {
        currentCursor = root_->cursorAt(input_.mousePos);
    }
    updateCursor(currentCursor);

    documentLifecycle = DocumentLifecycle::InPaint;
    renderer_.beginFrame(w, h);
    if (root_->layoutObject) {
        root_->layoutObject->paint(renderer_);
    } else {
        root_->render(renderer_);
    }
    for (auto* fw : fixedWidgets) {
        if (fw->layoutObject) {
            fw->layoutObject->paint(renderer_);
        } else {
            fw->render(renderer_);
        }
    }
    if (onRender) onRender();
    renderer_.endFrame();
    documentLifecycle = DocumentLifecycle::PaintClean;

    // If the Vulkan swapchain was marked dirty during presentation (e.g.
    // VK_SUBOPTIMAL_KHR), schedule another redraw so the main loop does not
    // stall inside WaitMessage() with a stale/mismatched swapchain.
    if (renderer_.needsRepaint()) {
        needsRedraw_ = true;
    }
}

void Application::run() {
    bool firstFrame = true;
    bool windowVisible = false;
#if FLUXUI_SHOW_WINDOW_ON_INIT
    if (window_) {
        Platform::showWindow(window_);
        windowVisible = true;
    }
#endif
    lastTime_ = std::chrono::high_resolution_clock::now();
    while (running) {
        processEvents();
        if (!running) {
            break;
        }
        bool hasAnimations = false;
        if (!needsRedraw_ && !firstFrame && root_) {
            hasAnimations = root_->hasActiveAnimations();
        }
        if (!needsRedraw_ && !hasAnimations && !firstFrame) {
#ifdef _WIN32
            WaitMessage();
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
            lastTime_ = std::chrono::high_resolution_clock::now();
            continue;
        }
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - lastTime_;
        input_.deltaTime = std::clamp(elapsed.count(), 0.001f, 1.0f / 30.0f);
        lastTime_ = now;

        renderFrame();

#if FLUXUI_REVEAL_WINDOW_ON_FIRST_FRAME
        if (firstFrame && window_ && !windowVisible) {
            Platform::showWindow(window_);
            windowVisible = true;
        }
#endif
        if (firstFrame) {
            firstFrame = false;
        }
#if FLUXUI_TARGET_FPS > 0
        constexpr float targetFrameSeconds = 1.0f / static_cast<float>(FLUXUI_TARGET_FPS);
        auto frameElapsed = std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - now).count();
        if (frameElapsed < targetFrameSeconds) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(targetFrameSeconds - frameElapsed));
        } else {
            std::this_thread::yield();
        }
#else
        std::this_thread::yield();
#endif
    }
}
void Application::lazyLoad(std::function<void()> loader, std::function<void()> onComplete) {
    static std::atomic<size_t> taskCounter{0};
    std::thread([loader = std::move(loader), onComplete = std::move(onComplete), this]() mutable {
        try {
            loader();
            if (onComplete) {
                onComplete();
            }
        } catch (...) {}
        requestRedraw();
    }).detach();
}
void LazyPanel::update(const InputState& input) {
    if (!initialized) {
        initialized = true;
        if (skeletonBuilder) {
            clearChildren();
            skeletonBuilder(this);
        }
    }
    bool currentLoaded = loaded.load(std::memory_order_acquire);
    if (currentLoaded && !lastLoadedState) {
        lastLoadedState = true;
        clearChildren();
        if (contentBuilder) {
            contentBuilder(this);
        }
        markLayoutDirty();
        markStyleDirtyRecursive();
    }
    Widget::update(input);
}
void Anchor::update(const InputState& input) {
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardActivate = focused && keyCode == 0x0D &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    bool clicked = hovered && input.mouseClicked[0];
    Text::update(input);
    if ((clicked || keyboardActivate) && !href.empty()) {
        Platform::openSystemURL(href);
    }
}

void Details::layout(const Rect& parentBounds) {
    for (auto& child : children) {
        if (child->type != "summary") {
            child->visible = open;
        }
    }
    Widget::layout(parentBounds);
}

void Summary::update(const InputState& input) {
    bool clicked = hovered && input.mouseClicked[0];
    int keyCode = normalizeTextEditingKey(input.keyCode);
    bool keyboardToggle = focused && (keyCode == 0x0D || keyCode == 0x20) &&
        (input.modifiers & (MOD_CTRL | MOD_ALT | MOD_GUI)) == 0;
    Text::update(input);
    if ((clicked || keyboardToggle) && parent && parent->type == "details") {
        if (auto* details = dynamic_cast<Details*>(parent)) {
            details->open = !details->open;
            details->markLayoutDirty();
            details->markStyleDirtyRecursive();
            if (auto* app = Application::instance()) {
                app->requestRedraw();
            }
        }
    }
}

void Summary::render(Renderer& renderer) {
    Text::render(renderer);
    if (!visible) return;
    bool isOpen = false;
    if (parent && parent->type == "details") {
        if (auto* details = dynamic_cast<Details*>(parent)) {
            isOpen = details->open;
        }
    }
    float mSize = computedStyle.fontSize * 0.8f;
    float mX = bounds.x + computedStyle.padding.left * 0.3f;
    float mY = bounds.y + (bounds.h - mSize) * 0.5f;
    renderer.drawText(isOpen ? "v" : ">", {mX, mY}, computedStyle.color, mSize, FontWeight::Bold);
}

void Dialog::show() {
    open = true;
    modal = false;
    style.display = Display::Block;
    markStyleDirty();
    if (auto* app = Application::instance()) app->requestRedraw();
}

void Dialog::showModal() {
    open = true;
    modal = true;
    style.display = Display::Block;
    markStyleDirty();
    if (auto* app = Application::instance()) app->requestRedraw();
}

void Dialog::close() {
    open = false;
    modal = false;
    style.display = Display::None;
    markStyleDirty();
    if (auto* app = Application::instance()) app->requestRedraw();
}

void Dialog::resolveStyles(const StyleSheet& sheet) {
    Widget::resolveStyles(sheet);
    computedStyle.display = open ? Display::Block : Display::None;
    size_t nextLayoutSignature = layoutStyleSignature(computedStyle);
    if (nextLayoutSignature != layoutSignature) {
        layoutSignature = nextLayoutSignature;
        markLayoutDirty();
    }
}

void Dialog::update(const InputState& input) {
    if (!open) return;
    Widget::update(input);
    int keyCode = normalizeTextEditingKey(input.keyCode);
    if (keyCode == 0x1B) {
        close();
    }
}

void Dialog::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    if (open && modal) {
        Vec2 winSize = renderer.getWindowSize();
        renderer.drawRoundedRect({0.0f, 0.0f, winSize.x, winSize.y},
                                 Color(0.0f, 0.0f, 0.0f, 0.55f),
                                 BorderRadius(0.0f),
                                 1.0f);
    }
    Widget::render(renderer);
}

void Meter::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    float rangeVal = max - min;
    float fraction = 0.0f;
    if (rangeVal > 0.0f) {
        fraction = (value - min) / rangeVal;
    }
    fraction = std::max(0.0f, std::min(1.0f, fraction));
    if (fraction > 0.0f) {
        Color barColor(0.2f, 0.8f, 0.2f, 1.0f); // Default green
        if (value < low || value > high) {
            barColor = Color(0.9f, 0.2f, 0.2f, 1.0f); // Red
        } else if (optimum < low && value > low) {
            barColor = Color(0.9f, 0.7f, 0.1f, 1.0f); // Yellow
        }
        Rect barRect = bounds;
        barRect.x += computedStyle.padding.left;
        barRect.y += computedStyle.padding.top;
        barRect.w = (bounds.w - computedStyle.padding.left - computedStyle.padding.right) * fraction;
        barRect.h = bounds.h - computedStyle.padding.top - computedStyle.padding.bottom;
        BorderRadius barRadius = computedStyle.borderRadius;
        renderer.drawRoundedRect(barRect, barColor, barRadius);
    }
    if (computedStyle.border.width > 0) {
        renderer.drawBorder(bounds, computedStyle.border, computedStyle.borderRadius);
    }
}

void Progress::render(Renderer& renderer) {
    if (!canPaintWidget(this)) return;
    renderBackground(renderer);
    
    Rect fillRect = bounds;
    fillRect.x += computedStyle.padding.left;
    fillRect.y += computedStyle.padding.top;
    fillRect.h = bounds.h - computedStyle.padding.top - computedStyle.padding.bottom;
    float maxW = bounds.w - computedStyle.padding.left - computedStyle.padding.right;

    Color progressColor(0.2f, 0.6f, 0.95f, 1.0f); // Sleek modern blue

    if (value < 0.0f) {
        // Indeterminate state: animate sliding bar
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        double ms = std::chrono::duration<double, std::milli>(now).count();
        float pulseFraction = std::sin(ms * 0.005) * 0.5f + 0.5f;
        fillRect.w = maxW * 0.3f;
        fillRect.x += (maxW - fillRect.w) * pulseFraction;
        renderer.drawRoundedRect(fillRect, progressColor, computedStyle.borderRadius);
        if (auto* app = Application::instance()) {
            app->requestRedraw();
        }
    } else {
        // Determinate state
        float fraction = max > 0.0f ? std::max(0.0f, std::min(1.0f, value / max)) : 0.0f;
        if (fraction > 0.0f) {
            fillRect.w = maxW * fraction;
            renderer.drawRoundedRect(fillRect, progressColor, computedStyle.borderRadius);
        }
    }

    if (computedStyle.border.width > 0) {
        renderer.drawBorder(bounds, computedStyle.border, computedStyle.borderRadius);
    }
}

void Application::shutdown() {
    renderer_.shutdown();
    if (window_) {
        Platform::destroyWindow(window_);
        window_ = nullptr;
    }
    Platform::shutdown();
}
}
