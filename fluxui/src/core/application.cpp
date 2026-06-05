#include "fluxui/widgets.h"
#include "widget_internal.h"   // shared FluxUI::detail widget helpers
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#endif
#include "fluxui/compositor.h"
#include "fluxui/layout.h"
#include "fluxui/layout_object.h"
#include "fluxui/accessibility.h"
#include "fluxui/property_trees.h"
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
#include <filesystem>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <type_traits>
#include <stb_image.h>
#include "fluxui/platform.h"
#include <nlohmann/json.hpp>
#include <unordered_map>
namespace FluxUI {
using namespace FluxUI::detail;

PropertyTrees g_activePropertyTrees;

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
    if (axObjectCache_) {
        axObjectCache_->remove(w);
    }
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
Widget::Widget() {}
Widget::~Widget() {
    CompositorEngine::instance().unregisterWidget(reinterpret_cast<uintptr_t>(this));
    if (auto* app = Application::instance()) {
        app->onWidgetDestroyed(this);
    }
}
void Widget::detachLayoutTree() {
    layoutObject.reset();
    if (beforePseudoNode) {
        beforePseudoNode->detachLayoutTree();
    }
    for (auto& child : children) {
        child->detachLayoutTree();
    }
    if (afterPseudoNode) {
        afterPseudoNode->detachLayoutTree();
    }
}
void Widget::attachLayoutTree() {
    if (computedStyle->display == Display::None) {
        detachLayoutTree();
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
    if (dynamic_cast<Text*>(this)) {
        auto* textWidget = static_cast<Text*>(this);
        return std::make_unique<LayoutText>(this, textWidget->content);
    }
    if (computedStyle->display == Display::Flex) {
        return std::make_unique<LayoutFlexibleBox>(this);
    } else if (computedStyle->display == Display::Grid) {
        return std::make_unique<LayoutGrid>(this);
    } else if (computedStyle->display == Display::Block || 
               computedStyle->display == Display::InlineBlock) {
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

bool Widget::isScrollableY() const {
    return scrollsOverflowY(computedStyle, contentHeight, bounds.h);
}
bool Widget::isClippingOverflow() const {
    return clipsOverflow(computedStyle);
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
    
    Vec2 originalMousePos = event.mousePos;

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
        if (!w || !w->visible || w->style.display == Display::None) {
            continue;
        }
        event.currentTarget = w;

        if (w->renderScale != 1.0f && w->renderScale != 0.0f) {
            Vec2 c = w->bounds.center();
            event.mousePos.x = c.x + (originalMousePos.x - c.x) / w->renderScale;
            event.mousePos.y = c.y + (originalMousePos.y - c.y) / w->renderScale;
        } else {
            event.mousePos = originalMousePos;
        }

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
        if (w && w->visible && w->style.display != Display::None) {
            event.phase = EventPhase::AtTarget;
            event.currentTarget = w;

            if (w->renderScale != 1.0f && w->renderScale != 0.0f) {
                Vec2 c = w->bounds.center();
                event.mousePos.x = c.x + (originalMousePos.x - c.x) / w->renderScale;
                event.mousePos.y = c.y + (originalMousePos.y - c.y) / w->renderScale;
            } else {
                event.mousePos = originalMousePos;
            }

            auto listenersCopy = w->domEventListeners;
            for (auto& entry : listenersCopy) {
                if (entry.type == event.type) {
                    entry.callback(event);
                    if (event.propagationStopped) break;
                }
            }
            if (event.type == "click" && !event.defaultPrevented && w->onClick) {
                w->onClick();
                event.stopPropagation();
            }
        }
    }
    if (event.bubbles && !event.propagationStopped && path.size() > 1) {
        event.phase = EventPhase::Bubble;
        for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
            Widget* w = path[i];
            if (!w || !w->visible || w->style.display == Display::None) {
                continue;
            }
            event.currentTarget = w;

            if (w->renderScale != 1.0f && w->renderScale != 0.0f) {
                Vec2 c = w->bounds.center();
                event.mousePos.x = c.x + (originalMousePos.x - c.x) / w->renderScale;
                event.mousePos.y = c.y + (originalMousePos.y - c.y) / w->renderScale;
            } else {
                event.mousePos = originalMousePos;
            }

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

    event.mousePos = originalMousePos;
}

void Widget::markLayoutDirty() {
    if (layoutDirty && (!parent || parent->layoutDirty)) return;
    layoutDirty = true;
    lifecycleState = WidgetLifecycle::LayoutDirty;
    if (layoutObject) {
        layoutObject->invalidateCache();
    }
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
void Widget::checkFocusChanges() {
    if (focused != lastFrameFocused) {
        lastFrameFocused = focused;
        markStyleDirty();
        Widget* p = parent;
        while (p) {
            p->markStyleDirty();
            p = p->parent;
        }
    }
    for (auto& child : children) {
        if (child) {
            child->checkFocusChanges();
        }
    }
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

        // 1. Descendant invalidation
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

        // 2. Direct Child invalidation (extremely optimized - no subtree traversal!)
        if (invalidationSet->invalidateAllChildren) {
            for (auto& child : children) {
                child->styleDirty = true;
                child->markSubtreeStyleDirty();
            }
        } else if (!invalidationSet->childClasses.empty() ||
                   !invalidationSet->childIds.empty() ||
                   !invalidationSet->childTypes.empty()) {
            for (auto& child : children) {
                bool match = false;

                if (!invalidationSet->childClasses.empty() && !child->className.empty()) {
                    std::istringstream stream(child->className);
                    std::string cc;
                    while (stream >> cc) {
                        if (invalidationSet->childClasses.find(cc) != invalidationSet->childClasses.end()) {
                            match = true;
                            break;
                        }
                    }
                }

                if (!match && !invalidationSet->childIds.empty()) {
                    if (invalidationSet->childIds.find(child->id) != invalidationSet->childIds.end()) {
                        match = true;
                    }
                }

                if (!match && !invalidationSet->childTypes.empty()) {
                    if (invalidationSet->childTypes.find(child->type) != invalidationSet->childTypes.end()) {
                        match = true;
                    }
                }

                if (match) {
                    child->styleDirty = true;
                    child->markSubtreeStyleDirty();
                }
            }
        }

        // 3. Sibling invalidation (only subsequent siblings are evaluated!)
        if (parent) {
            // General siblings
            if (invalidationSet->invalidateAllSiblings) {
                bool foundSelf = false;
                for (auto& sibling : parent->children) {
                    if (sibling.get() == this) {
                        foundSelf = true;
                        continue;
                    }
                    if (!foundSelf) continue;
                    sibling->markStyleDirtyRecursive();
                }
            } else if (!invalidationSet->siblingClasses.empty() ||
                       !invalidationSet->siblingIds.empty() ||
                       !invalidationSet->siblingTypes.empty()) {
                bool foundSelf = false;
                for (auto& sibling : parent->children) {
                    if (sibling.get() == this) {
                        foundSelf = true;
                        continue;
                    }
                    if (!foundSelf) continue;

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
                        sibling->styleDirty = true;
                        sibling->markSubtreeStyleDirty();
                    }
                }
            }

            // Adjacent sibling: check ONLY the single immediate next sibling! O(1) performance!
            Widget* adjacentSibling = nullptr;
            for (size_t idx = 0; idx < parent->children.size(); ++idx) {
                if (parent->children[idx].get() == this) {
                    if (idx + 1 < parent->children.size()) {
                        adjacentSibling = parent->children[idx + 1].get();
                    }
                    break;
                }
            }

            if (adjacentSibling) {
                if (invalidationSet->invalidateAllAdjacentSiblings) {
                    adjacentSibling->markStyleDirtyRecursive();
                } else if (!invalidationSet->adjacentSiblingClasses.empty() ||
                           !invalidationSet->adjacentSiblingIds.empty() ||
                           !invalidationSet->adjacentSiblingTypes.empty()) {
                    bool match = false;

                    if (!invalidationSet->adjacentSiblingClasses.empty() && !adjacentSibling->className.empty()) {
                        std::istringstream stream(adjacentSibling->className);
                        std::string cc;
                        while (stream >> cc) {
                            if (invalidationSet->adjacentSiblingClasses.find(cc) != invalidationSet->adjacentSiblingClasses.end()) {
                                match = true;
                                break;
                            }
                        }
                    }

                    if (!match && !invalidationSet->adjacentSiblingIds.empty()) {
                        if (invalidationSet->adjacentSiblingIds.find(adjacentSibling->id) != invalidationSet->adjacentSiblingIds.end()) {
                            match = true;
                        }
                    }

                    if (!match && !invalidationSet->adjacentSiblingTypes.empty()) {
                        if (invalidationSet->adjacentSiblingTypes.find(adjacentSibling->type) != invalidationSet->adjacentSiblingTypes.end()) {
                            match = true;
                        }
                    }

                    if (match) {
                        adjacentSibling->styleDirty = true;
                        adjacentSibling->markSubtreeStyleDirty();
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

        // 1. Descendant invalidation
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

        // 2. Direct Child invalidation (extremely optimized - no subtree traversal!)
        if (invalidationSet->invalidateAllChildren) {
            for (auto& child : children) {
                child->styleDirty = true;
                child->markSubtreeStyleDirty();
            }
        } else if (!invalidationSet->childClasses.empty() ||
                   !invalidationSet->childIds.empty() ||
                   !invalidationSet->childTypes.empty()) {
            for (auto& child : children) {
                bool match = false;

                if (!invalidationSet->childClasses.empty() && !child->className.empty()) {
                    std::istringstream stream(child->className);
                    std::string cc;
                    while (stream >> cc) {
                        if (invalidationSet->childClasses.find(cc) != invalidationSet->childClasses.end()) {
                            match = true;
                            break;
                        }
                    }
                }

                if (!match && !invalidationSet->childIds.empty()) {
                    if (invalidationSet->childIds.find(child->id) != invalidationSet->childIds.end()) {
                        match = true;
                    }
                }

                if (!match && !invalidationSet->childTypes.empty()) {
                    if (invalidationSet->childTypes.find(child->type) != invalidationSet->childTypes.end()) {
                        match = true;
                    }
                }

                if (match) {
                    child->styleDirty = true;
                    child->markSubtreeStyleDirty();
                }
            }
        }

        // 3. Sibling invalidation (only subsequent siblings are evaluated!)
        if (parent) {
            // General siblings
            if (invalidationSet->invalidateAllSiblings) {
                bool foundSelf = false;
                for (auto& sibling : parent->children) {
                    if (sibling.get() == this) {
                        foundSelf = true;
                        continue;
                    }
                    if (!foundSelf) continue;
                    sibling->markStyleDirtyRecursive();
                }
            } else if (!invalidationSet->siblingClasses.empty() ||
                       !invalidationSet->siblingIds.empty() ||
                       !invalidationSet->siblingTypes.empty()) {
                bool foundSelf = false;
                for (auto& sibling : parent->children) {
                    if (sibling.get() == this) {
                        foundSelf = true;
                        continue;
                    }
                    if (!foundSelf) continue;

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
                        sibling->styleDirty = true;
                        sibling->markSubtreeStyleDirty();
                    }
                }
            }

            // Adjacent sibling: check ONLY the single immediate next sibling! O(1) performance!
            Widget* adjacentSibling = nullptr;
            for (size_t idx = 0; idx < parent->children.size(); ++idx) {
                if (parent->children[idx].get() == this) {
                    if (idx + 1 < parent->children.size()) {
                        adjacentSibling = parent->children[idx + 1].get();
                    }
                    break;
                }
            }

            if (adjacentSibling) {
                if (invalidationSet->invalidateAllAdjacentSiblings) {
                    adjacentSibling->markStyleDirtyRecursive();
                } else if (!invalidationSet->adjacentSiblingClasses.empty() ||
                           !invalidationSet->adjacentSiblingIds.empty() ||
                           !invalidationSet->adjacentSiblingTypes.empty()) {
                    bool match = false;

                    if (!invalidationSet->adjacentSiblingClasses.empty() && !adjacentSibling->className.empty()) {
                        std::istringstream stream(adjacentSibling->className);
                        std::string cc;
                        while (stream >> cc) {
                            if (invalidationSet->adjacentSiblingClasses.find(cc) != invalidationSet->adjacentSiblingClasses.end()) {
                                match = true;
                                break;
                            }
                        }
                    }

                    if (!match && !invalidationSet->adjacentSiblingIds.empty()) {
                        if (invalidationSet->adjacentSiblingIds.find(adjacentSibling->id) != invalidationSet->adjacentSiblingIds.end()) {
                            match = true;
                        }
                    }

                    if (!match && !invalidationSet->adjacentSiblingTypes.empty()) {
                        if (invalidationSet->adjacentSiblingTypes.find(adjacentSibling->type) != invalidationSet->adjacentSiblingTypes.end()) {
                            match = true;
                        }
                    }

                    if (match) {
                        adjacentSibling->styleDirty = true;
                        adjacentSibling->markSubtreeStyleDirty();
                    }
                }
            }
        }
    }
}

void Widget::updateStyleAndLayout() {
    if (auto* app = Application::instance()) {
        app->updateStyleAndLayout();
    }
}

void Widget::resolveStyles(const StyleSheet& sheet) {
    currentSheet = &sheet;
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

        const Style* parentStyle = parent ? parent->computedStyle.get() : nullptr;
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
            auto getAncestors = [this]() -> std::vector<CSSSelectorNode> {
                std::vector<CSSSelectorNode> t_ancestors;
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

            this->computedStyle = sheet.resolveLazy(className, id, selectorType, ancestorH1, ancestorH2, parentStyle, getAncestors, this);
            Style& computedStyle = this->computedStyle.ensureMutable();
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
        if (style.columnCount > 0) computedStyle.columnCount = style.columnCount;
        if (style.columnWidth > 0.0f) computedStyle.columnWidth = style.columnWidth;
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
        if (style.hasBackdropFilter) {
            computedStyle.backdropFilterOperations = style.backdropFilterOperations;
            computedStyle.hasBackdropFilter = true;
            computedStyle.backdropFilterBlur = style.backdropFilterBlur;
            computedStyle.hasBackdropFilterBlur = style.hasBackdropFilterBlur;
        }
        if (style.hasFilter) {
            computedStyle.filterOperations = style.filterOperations;
            computedStyle.hasFilter = true;
        }
        // Grid layout cascade
        if (!style.gridTemplateColumnTracks.empty() || !style.gridTemplateColumns.empty()) {
            computedStyle.gridTemplateColumnTracks = style.gridTemplateColumnTracks;
            computedStyle.gridTemplateColumns      = style.gridTemplateColumns;
        }
        if (!style.gridTemplateRowTracks.empty() || !style.gridTemplateRows.empty()) {
            computedStyle.gridTemplateRowTracks = style.gridTemplateRowTracks;
            computedStyle.gridTemplateRows      = style.gridTemplateRows;
        }
        if (style.hasGridTemplateAreas) {
            computedStyle.gridTemplateAreas    = style.gridTemplateAreas;
            computedStyle.hasGridTemplateAreas = true;
        }
        if (!style.gridAutoRowTracks.empty())    computedStyle.gridAutoRowTracks    = style.gridAutoRowTracks;
        if (!style.gridAutoColumnTracks.empty()) computedStyle.gridAutoColumnTracks = style.gridAutoColumnTracks;
        if (style.gridAutoFlow != GridAutoFlow::Row) computedStyle.gridAutoFlow = style.gridAutoFlow;
        if (!style.gridColumnStart.isAuto()) computedStyle.gridColumnStart = style.gridColumnStart;
        if (!style.gridColumnEnd.isAuto())   computedStyle.gridColumnEnd   = style.gridColumnEnd;
        if (!style.gridRowStart.isAuto())    computedStyle.gridRowStart    = style.gridRowStart;
        if (!style.gridRowEnd.isAuto())      computedStyle.gridRowEnd      = style.gridRowEnd;
        if (!style.gridColumn.empty())       computedStyle.gridColumn      = style.gridColumn;
        if (!style.gridRow.empty())          computedStyle.gridRow         = style.gridRow;
        if (!style.gridArea.empty())         computedStyle.gridArea        = style.gridArea;
        if (style.hasJustifyItems) {
            computedStyle.justifyItems    = style.justifyItems;
            computedStyle.hasJustifyItems = true;
        }
        if (style.hasJustifySelf) {
            computedStyle.justifySelf    = style.justifySelf;
            computedStyle.hasJustifySelf = true;
        }
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
                if (prop.name == "filter") {
                    computedStyle.filterOperations = source.filterOperations;
                    computedStyle.hasFilter = source.hasFilter;
                    continue;
                }
                if (prop.name == "backdrop-filter") {
                    computedStyle.backdropFilterOperations = source.backdropFilterOperations;
                    computedStyle.hasBackdropFilter = source.hasBackdropFilter;
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
        
        // ::before — only resolve if the stylesheet has ::before rules, or if a
        // before node already exists (so it can be torn down when rules are removed).
        if (sheet.hasBeforeRules() || beforePseudoNode) {
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
        }

        // ::after — same fast-path gating as ::before.
        if (sheet.hasAfterRules() || afterPseudoNode) {
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
        }

        // ── Resolve additional pseudo-element styles (Blink PseudoId parity) ──
        // Fast-path: skip entirely when the stylesheet has no such rules.
        // ::placeholder — for input/textarea placeholder text color/font
        if (sheet.hasPlaceholderRules()) {
            Style ps = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "placeholder");
            if (ps.hasColor || ps.hasFontSize || ps.hasFontWeight || ps.opacity != 1.0f) {
                if (!placeholderStyle) placeholderStyle = std::make_unique<Style>();
                *placeholderStyle = std::move(ps);
                hasPlaceholderStyle = true;
            } else {
                hasPlaceholderStyle = false;
            }
        } else {
            hasPlaceholderStyle = false;
        }
        // ::selection — selection highlight color/background
        if (sheet.hasSelectionRules()) {
            Style ss = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "selection");
            if (ss.hasColor || ss.backgroundColor.a > 0) {
                if (!selectionStyle) selectionStyle = std::make_unique<Style>();
                *selectionStyle = std::move(ss);
                hasSelectionStyle = true;
            } else {
                hasSelectionStyle = false;
            }
        } else {
            hasSelectionStyle = false;
        }
        // ::marker — list-item marker color/font
        if (sheet.hasMarkerRules()) {
            Style ms = sheet.resolve(className, id, selectorType, getAncestors(), &computedStyle, this, "marker");
            if (ms.hasColor || ms.hasFontSize || ms.hasFontWeight) {
                if (!markerStyle) markerStyle = std::make_unique<Style>();
                *markerStyle = std::move(ms);
                hasMarkerStyle = true;
            } else {
                hasMarkerStyle = false;
            }
        } else {
            hasMarkerStyle = false;
        }

        lastResolveKey = currentKey;
        lastStyleSheetEpoch = sheet.getEpoch();
        hasLastResolveKey = true;
        styleDirty = false;
        }
    }

    // Inline C++ style overrides: Programmatic style changes set in C++ should always override the matched stylesheet styles
    Style& computedStyle = this->computedStyle.ensureMutable();
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
    if (style.columnCount > 0) computedStyle.columnCount = style.columnCount;
    if (style.columnWidth > 0.0f) computedStyle.columnWidth = style.columnWidth;
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
    if (style.hasBackdropFilter) {
        computedStyle.backdropFilterOperations = style.backdropFilterOperations;
        computedStyle.hasBackdropFilter = true;
        computedStyle.backdropFilterBlur = style.backdropFilterBlur;
        computedStyle.hasBackdropFilterBlur = style.hasBackdropFilterBlur;
    }
    if (style.hasFilter) {
        computedStyle.filterOperations = style.filterOperations;
        computedStyle.hasFilter = true;
    }
    // Grid layout cascade (second block)
    if (!style.gridTemplateColumnTracks.empty() || !style.gridTemplateColumns.empty()) {
        computedStyle.gridTemplateColumnTracks = style.gridTemplateColumnTracks;
        computedStyle.gridTemplateColumns      = style.gridTemplateColumns;
    }
    if (!style.gridTemplateRowTracks.empty() || !style.gridTemplateRows.empty()) {
        computedStyle.gridTemplateRowTracks = style.gridTemplateRowTracks;
        computedStyle.gridTemplateRows      = style.gridTemplateRows;
    }
    if (style.hasGridTemplateAreas) {
        computedStyle.gridTemplateAreas    = style.gridTemplateAreas;
        computedStyle.hasGridTemplateAreas = true;
    }
    if (!style.gridAutoRowTracks.empty())    computedStyle.gridAutoRowTracks    = style.gridAutoRowTracks;
    if (!style.gridAutoColumnTracks.empty()) computedStyle.gridAutoColumnTracks = style.gridAutoColumnTracks;
    if (style.gridAutoFlow != GridAutoFlow::Row) computedStyle.gridAutoFlow = style.gridAutoFlow;
    if (!style.gridColumnStart.isAuto()) computedStyle.gridColumnStart = style.gridColumnStart;
    if (!style.gridColumnEnd.isAuto())   computedStyle.gridColumnEnd   = style.gridColumnEnd;
    if (!style.gridRowStart.isAuto())    computedStyle.gridRowStart    = style.gridRowStart;
    if (!style.gridRowEnd.isAuto())      computedStyle.gridRowEnd      = style.gridRowEnd;
    if (!style.gridColumn.empty())       computedStyle.gridColumn      = style.gridColumn;
    if (!style.gridRow.empty())          computedStyle.gridRow         = style.gridRow;
    if (!style.gridArea.empty())         computedStyle.gridArea        = style.gridArea;
    if (style.hasJustifyItems) {
        computedStyle.justifyItems    = style.justifyItems;
        computedStyle.hasJustifyItems = true;
    }
    if (style.hasJustifySelf) {
        computedStyle.justifySelf    = style.justifySelf;
        computedStyle.hasJustifySelf = true;
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
    if (style.contain != ContainmentFlags::kContainNone) computedStyle.contain = style.contain;

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
    if (!parent) {
        g_activePropertyTrees.clear();
    }
    lifecycleState = WidgetLifecycle::PrePaintDirty;

    // 1. Property Tree Builder phase:
    updatePaintProperties(parentProps);

    // 2. Paint Invalidation phase:
    invalidatePaintIfNeeded();

    lifecycleState = WidgetLifecycle::PrePaintClean;

    for (auto& child : children) {
        if (child) {
            child->prePaint(paintProperties);
        }
    }
}

void Widget::updatePaintProperties(const PaintProperties& parentProps) {
    paintProperties.translation = parentProps.translation;
    paintProperties.scale = parentProps.scale;
    paintProperties.clipRect = parentProps.clipRect;
    paintProperties.hasClip = parentProps.hasClip;
    paintProperties.opacity = parentProps.opacity * computedStyle->opacity;

    // 1. Resolve TransformNode
    bool scrollable = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    if (scrollable) {
        scrollY = CompositorEngine::instance().getScrollY(reinterpret_cast<uintptr_t>(this));
        targetScrollY = CompositorEngine::instance().getTargetScrollY(reinterpret_cast<uintptr_t>(this));
        paintProperties.translation.y -= scrollY;

        Vec2 scrollOffset = {0.0f, -scrollY};
        transformNodeId = g_activePropertyTrees.insertTransformNode(
            parentProps.transformNodeId,
            1.0f,
            scrollOffset,
            {0.0f, 0.0f}
        );
    } else {
        transformNodeId = parentProps.transformNodeId;
    }
    paintProperties.transformNodeId = transformNodeId;

    // 2. Resolve ClipNode
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

        clipNodeId = g_activePropertyTrees.insertClipNode(
            parentProps.clipNodeId,
            bounds,
            transformNodeId
        );
    } else {
        clipNodeId = parentProps.clipNodeId;
    }
    paintProperties.clipNodeId = clipNodeId;

    // 3. Resolve EffectNode
    if (computedStyle->opacity != 1.0f) {
        effectNodeId = g_activePropertyTrees.insertEffectNode(
            parentProps.effectNodeId,
            computedStyle->opacity,
            0.0f,
            transformNodeId
        );
    } else {
        effectNodeId = parentProps.effectNodeId;
    }
    paintProperties.effectNodeId = effectNodeId;
}

void Widget::invalidatePaintIfNeeded() {
    bool propertyTreeChanged = (paintProperties != lastPaintProperties) ||
                               (bounds.x != lastPaintBounds.x ||
                                bounds.y != lastPaintBounds.y ||
                                bounds.w != lastPaintBounds.w ||
                                bounds.h != lastPaintBounds.h);

    if (propertyTreeChanged) {
        if (layoutObject) {
            layoutObject->markPaintDirty();
        }
    }

    lastPaintProperties = paintProperties;
    lastPaintBounds = bounds;
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
    const Style& s = *computedStyle;
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
    bool isTableCellFinal = (s.display == Display::TableCell && parentBounds.w < 9999);
    float w = 0.0f;
    float h = 0.0f;
    if (isTableCellFinal) {
        w = parentBounds.w;
        h = parentBounds.h;
    } else {
         w = (s.width.isSet() && !s.width.isAuto()) ? s.width.resolve(parentBounds.w, vpW, vpH) :
            (parentBounds.w < 9999 ? parentBounds.w - s.margin.horizontal() : 0);
        h = (s.height.isSet() && !s.height.isAuto()) ? s.height.resolve(parentBounds.h, vpW, vpH) :
            (parentBounds.h < 9999 ? parentBounds.h - s.margin.vertical() : 0);
        bool widthControlsRatio = s.aspectRatio > 0.0f && s.width.isSet() && !s.width.isAuto() && !s.height.isSet();
        bool heightControlsRatio = s.aspectRatio > 0.0f && s.height.isSet() && !s.height.isAuto() && !s.width.isSet();
        if (s.minWidth.isSet()) w = std::max(w, s.minWidth.resolve(parentBounds.w, vpW, vpH));
        if (s.maxWidth.isSet()) w = std::min(w, s.maxWidth.resolve(parentBounds.w, vpW, vpH));
        if (widthControlsRatio) h = w / s.aspectRatio;
        if (s.minHeight.isSet()) h = std::max(h, s.minHeight.resolve(parentBounds.h, vpW, vpH));
        if (s.maxHeight.isSet()) h = std::min(h, s.maxHeight.resolve(parentBounds.h, vpW, vpH));
        if (heightControlsRatio) w = h * s.aspectRatio;
        if (s.hasBoxSizing && s.boxSizing == BoxSizing::ContentBox) {
            if (s.width.isSet() && !s.width.isAuto()) w += s.padding.horizontal() + usedBorderHorizontal(s);
            if (s.height.isSet() && !s.height.isAuto()) h += s.padding.vertical() + usedBorderVertical(s);
        }
    }
    bool hasSizeContainment = (s.contain & kContainSize);
    if (hasSizeContainment) {
        if (!s.width.isSet()) {
            if (s.display == Display::Block) {
                w = parentBounds.w < 9999 ? parentBounds.w - s.margin.horizontal() : 0;
            } else {
                w = s.padding.horizontal() + usedBorderHorizontal(s);
            }
        }
        if (!s.height.isSet()) {
            h = s.padding.vertical() + usedBorderVertical(s);
        }
    }
    bounds = {parent ? x : 0.0f, parent ? y : 0.0f, w, h};
    auto resolveAutoBlockHeight = [&](float measuredHeight) {
        float resolvedHeight = measuredHeight;
        if (s.minHeight.isSet()) {
            resolvedHeight = std::max(resolvedHeight, s.minHeight.resolve(parentBounds.h, vpW, vpH));
        }
        if (s.maxHeight.isSet()) {
            resolvedHeight = std::min(resolvedHeight, s.maxHeight.resolve(parentBounds.h, vpW, vpH));
        }
        return std::max(0.0f, resolvedHeight);
    };
    if (s.display == Display::Flex) {
        LayoutConstraints constraints;
        constraints.availableWidth = parentBounds.w;
        constraints.availableHeight = parentBounds.h;
        constraints.parentWidth = vpW;
        constraints.parentHeight = vpH;
        constraints.emBase = 16.0f;

        FlexLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(this, constraints);
        bounds.w = hasSizeContainment && !s.width.isSet() ? s.padding.horizontal() + usedBorderHorizontal(s) : res.width;
        bounds.h = hasSizeContainment && !s.height.isSet() ? s.padding.vertical() + usedBorderVertical(s) : res.height;
        contentHeight = hasSizeContainment ? s.padding.vertical() + usedBorderVertical(s) : res.contentHeight;
    } else if (s.display == Display::Grid) {
        LayoutConstraints constraints;
        constraints.availableWidth = parentBounds.w;
        constraints.availableHeight = parentBounds.h;
        constraints.parentWidth = vpW;
        constraints.parentHeight = vpH;
        constraints.emBase = 16.0f;

        GridLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(this, constraints);
        bounds.w = hasSizeContainment && !s.width.isSet() ? s.padding.horizontal() + usedBorderHorizontal(s) : res.width;
        bounds.h = hasSizeContainment && !s.height.isSet() ? s.padding.vertical() + usedBorderVertical(s) : res.height;
        contentHeight = hasSizeContainment ? s.padding.vertical() + usedBorderVertical(s) : res.contentHeight;
    } else if (s.display == Display::Table) {
        LayoutConstraints constraints;
        constraints.availableWidth = parentBounds.w;
        constraints.availableHeight = parentBounds.h;
        constraints.parentWidth = vpW;
        constraints.parentHeight = vpH;
        constraints.emBase = 16.0f;

        TableLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(this, constraints);
        bounds.w = hasSizeContainment && !s.width.isSet() ? s.padding.horizontal() + usedBorderHorizontal(s) : res.width;
        bounds.h = hasSizeContainment && !s.height.isSet() ? s.padding.vertical() + usedBorderVertical(s) : res.height;
        contentHeight = hasSizeContainment ? s.padding.vertical() + usedBorderVertical(s) : res.contentHeight;
    } else {
        if (s.columnCount > 1 || s.columnWidth > 0.0f) {
            float columnGap = s.columnGap > 0.0f ? s.columnGap : 16.0f;
            float availW = bounds.w - s.padding.horizontal();
            int colCount = s.columnCount;
            float colWidth = s.columnWidth;

            if (colWidth > 0.0f && colCount <= 0) {
                colCount = std::max(1, (int)std::floor((availW + columnGap) / (colWidth + columnGap)));
            } else if (colCount > 0 && colWidth <= 0.0f) {
                colWidth = (availW - (colCount - 1) * columnGap) / colCount;
            } else if (colCount <= 0 && colWidth <= 0.0f) {
                colCount = 1;
                colWidth = availW;
            } else {
                colWidth = (availW - (colCount - 1) * columnGap) / colCount;
            }

            colCount = std::max(1, colCount);
            colWidth = std::max(1.0f, colWidth);

            // 1. Column Balancing: Measure content heights in a single column pass
            float virtualY = 0.0f;
            std::vector<float> childHeights;
            for (auto& child : children) {
                if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) {
                    childHeights.push_back(0.0f);
                    continue;
                }
                const Style& cs = *(child->computedStyle);
                float availableChildWidth = colWidth - cs.margin.horizontal();
                Rect childArea = { 0.0f, virtualY + cs.margin.top, availableChildWidth, 10000.0f };
                child->layout(childArea);
                float h = child->bounds.h + cs.margin.vertical();
                childHeights.push_back(h);
                virtualY += h;
            }

            float totalHeight = virtualY;
            float targetColumnHeight = std::max(100.0f, totalHeight / colCount);

            // 2. Distribute layout fragments across columns
            float colStartX = bounds.x + s.padding.left;
            float colStartY = bounds.y + s.padding.top;
            int currentColumn = 0;
            float currentColY = 0.0f;

            for (size_t i = 0; i < children.size(); ++i) {
                auto& child = children[i];
                if (!child->visible || isDisplayNone(child.get()) || isOutOfFlow(child.get())) continue;

                float childH = childHeights[i];
                const Style& cs = *(child->computedStyle);

                if (currentColY > 0.0f && currentColY + childH > targetColumnHeight && currentColumn + 1 < colCount) {
                    currentColumn++;
                    currentColY = 0.0f;
                }

                float cx = colStartX + currentColumn * (colWidth + columnGap) + cs.margin.left;
                float cy = colStartY + currentColY + cs.margin.top;

                Rect childArea = { cx, cy, colWidth - cs.margin.horizontal(), child->bounds.h };
                child->layout(childArea);

                currentColY += childH;
            }

            float totalColContainerHeight = targetColumnHeight + s.padding.vertical();
            if (!s.height.isSet() && !heightProvidedByParentFlex && parent != nullptr) {
                if (hasSizeContainment) {
                    bounds.h = s.padding.vertical() + usedBorderVertical(s);
                } else {
                    bounds.h = resolveAutoBlockHeight(totalColContainerHeight);
                }
            }
            if (hasSizeContainment) {
                contentHeight = s.padding.vertical() + usedBorderVertical(s);
            } else {
                contentHeight = totalColContainerHeight;
            }
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
            float lineX = bounds.x + s.padding.left;
            float lineY = cy;
            float lineHeight = 0.0f;
            bool lineActive = false;

            auto flushInlineLine = [&]() {
                if (!lineActive) return;
                cy = lineY + std::max(lineHeight, computedStyle->fontSize * computedStyle->lineHeight);
                lineX = bounds.x + s.padding.left;
                lineY = cy;
                lineHeight = 0.0f;
                lineActive = false;
            };

            for (auto& child : children) {
                if (!child->visible) continue;
                if (isDisplayNone(child.get())) continue;
                if (isOutOfFlow(child.get())) continue;

                const Style& cs = *(child->computedStyle);
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
                if (isInlineFlowItem(child.get()) && cs.cssFloat == CSSFloat::None) {
                    float currentLeftX = bounds.x + s.padding.left;
                    float currentRightX = bounds.x + bounds.w - s.padding.right;
                    for (const auto& f : floats) {
                        if (f.y + f.h > lineY && f.y <= lineY) {
                            if (f.isLeft) {
                                currentLeftX = std::max(currentLeftX, f.x + f.w);
                            } else {
                                currentRightX = std::min(currentRightX, f.x);
                            }
                        }
                    }
                    if (!lineActive || lineX < currentLeftX) {
                        lineX = currentLeftX;
                        lineY = cy;
                    }

                    auto layoutInlineAt = [&](float x, float y) {
                        float availableW = std::max(1.0f, currentRightX - x - cs.margin.horizontal());
                        Rect childArea = {
                            x,
                            y,
                            availableW,
                            10000.0f
                        };
                        child->layout(childArea);
                    };

                    layoutInlineAt(lineX, lineY);
                    float outerW = child->bounds.w + cs.margin.horizontal();
                    bool wraps = lineActive &&
                        lineX + outerW > currentRightX &&
                        currentRightX > currentLeftX + 1.0f;
                    if (wraps) {
                        flushInlineLine();
                        currentLeftX = bounds.x + s.padding.left;
                        currentRightX = bounds.x + bounds.w - s.padding.right;
                        for (const auto& f : floats) {
                            if (f.y + f.h > lineY && f.y <= lineY) {
                                if (f.isLeft) {
                                    currentLeftX = std::max(currentLeftX, f.x + f.w);
                                } else {
                                    currentRightX = std::min(currentRightX, f.x);
                                }
                            }
                        }
                        lineX = currentLeftX;
                        layoutInlineAt(lineX, lineY);
                        outerW = child->bounds.w + cs.margin.horizontal();
                    }
                    lineHeight = std::max(lineHeight, child->bounds.h + cs.margin.vertical());
                    lineX = child->bounds.x + child->bounds.w + cs.margin.right;
                    lineActive = true;
                    continue;
                }

                if (cs.cssFloat == CSSFloat::Left) {
                    flushInlineLine();
                    float currentLeftX = bounds.x + s.padding.left;
                    for (const auto& f : floats) {
                        if (f.isLeft && f.y + f.h > cy && f.y <= cy) {
                            currentLeftX = std::max(currentLeftX, f.x + f.w);
                        }
                    }
                    float targetW = cs.width.isSet() ? cs.width.resolve(bounds.w, 1920.0f, 1080.0f) : 100.0f;
                    float floatStartX = currentLeftX + cs.margin.left;
                    float floatStartY = cy + cs.margin.top;
                    Rect childArea = { floatStartX, floatStartY, targetW, 10000.0f };
                    child->layout(childArea);
                    if (!cs.height.isSet() && child->contentHeight > child->bounds.h) {
                        child->bounds.h = child->contentHeight;
                    }
                    floats.push_back({ currentLeftX, cy, child->bounds.w + cs.margin.horizontal(), child->bounds.h + cs.margin.vertical(), true });
                } else if (cs.cssFloat == CSSFloat::Right) {
                    flushInlineLine();
                    float currentRightX = bounds.x + bounds.w - s.padding.right;
                    for (const auto& f : floats) {
                        if (!f.isLeft && f.y + f.h > cy && f.y <= cy) {
                            currentRightX = std::min(currentRightX, f.x);
                        }
                    }
                    float targetW = cs.width.isSet() ? cs.width.resolve(bounds.w, 1920.0f, 1080.0f) : 100.0f;
                    float marginBoxW = targetW + cs.margin.horizontal();
                    float cellStartX = currentRightX - marginBoxW;
                    float floatStartX = cellStartX + cs.margin.left;
                    float floatStartY = cy + cs.margin.top;
                    Rect childArea = { floatStartX, floatStartY, targetW, 10000.0f };
                    child->layout(childArea);
                    if (!cs.height.isSet() && child->contentHeight > child->bounds.h) {
                        child->bounds.h = child->contentHeight;
                    }
                    floats.push_back({ cellStartX, cy, child->bounds.w + cs.margin.horizontal(), child->bounds.h + cs.margin.vertical(), false });
                } else {
                    flushInlineLine();
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
                    float blockW = cs.width.isSet()
                        ? std::min(availChildW - cs.margin.horizontal(), cs.width.resolve(bounds.w, 1920.0f, 1080.0f))
                        : (availChildW - cs.margin.horizontal());
                    blockW = std::max(0.0f, blockW);

                    float availableChildH = (!s.height.isSet() && !heightProvidedByParentFlex)
                        ? 10000.0f
                        : (bounds.h > 0 ? bounds.h - s.padding.vertical() : 10000.0f);
                    float blockStartX = currentLeftX + cs.margin.left;
                    float blockStartY = cy + cs.margin.top;
                    Rect childArea = {
                        blockStartX,
                        blockStartY,
                        blockW,
                        std::max(0.0f, availableChildH)
                    };
                    child->layout(childArea);
                    if (!cs.height.isSet() && child->contentHeight > child->bounds.h) {
                        child->bounds.h = child->contentHeight;
                    }
                    cy = child->bounds.y + child->bounds.h + cs.margin.bottom;
                }
            }
            flushInlineLine();

            float maxFloatY = cy;
            for (const auto& f : floats) {
                maxFloatY = std::max(maxFloatY, f.y + f.h);
            }
            cy = maxFloatY;

            if (!s.height.isSet() && !heightProvidedByParentFlex && !children.empty() && parent != nullptr && (s.display != Display::TableCell || parentBounds.h >= 9999.0f)) {
                if (hasSizeContainment) {
                    bounds.h = s.padding.vertical() + usedBorderVertical(s);
                } else {
                    bounds.h = resolveAutoBlockHeight(cy - bounds.y + s.padding.bottom);
                }
            }
            if (hasSizeContainment) {
                contentHeight = s.padding.vertical() + usedBorderVertical(s);
            } else {
                contentHeight = cy - bounds.y + s.padding.bottom;
            }
        }
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
}
void Widget::layoutPositionedChildren() {
    const Style& s = *computedStyle;
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
        
        if (child->computedStyle->position == Position::Fixed) {
            Widget* cb = this;
            while (cb->parent) {
                const Style& cbs = *cb->computedStyle;
                if ((cbs.contain & kContainLayout) || (cbs.contain & kContainPaint)) {
                    break;
                }
                cb = cb->parent;
            }
            cx = cb->bounds.x;
            cy = cb->bounds.y;
            cw = cb->bounds.w;
            ch = cb->bounds.h;
        } else if (child->computedStyle->position == Position::Absolute) {
            Widget* cb = this;
            while (cb->parent) {
                if (cb->computedStyle->position != Position::Static) {
                    break;
                }
                cb = cb->parent;
            }
            cx = cb->bounds.x + usedBorderLeftWidth(*cb->computedStyle);
            cy = cb->bounds.y + usedBorderTopWidth(*cb->computedStyle);
            cw = std::max(0.0f, cb->bounds.w - usedBorderHorizontal(*cb->computedStyle));
            ch = std::max(0.0f, cb->bounds.h - usedBorderVertical(*cb->computedStyle));
        }
        
        const Style& cs = *(child->computedStyle);
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
    if (!scrollsOverflowY(*computedStyle, contentHeight, bounds.h) || maxScroll <= 1.0f) {
        track = {};
        thumb = {};
        return false;
    }
    // Browser-style scrollbar: flush to the right edge of the padding/border box.
    // The layout stores bounds as the content box, so extend by padding to reach
    // the true outer edge (where a browser paints the scrollbar).
    float padRight = computedStyle->padding.right;
    float padTop = computedStyle->padding.top;
    float padBottom = computedStyle->padding.bottom;
    float boxRight = bounds.x + bounds.w + padRight;
    float boxTop = bounds.y - padTop;
    float boxH = bounds.h + padTop + padBottom;
    float trackW = 12.0f;
    float trackH = boxH;
    float visibleRatio = bounds.h / std::max(contentHeight, bounds.h);
    float thumbH = std::clamp(trackH * visibleRatio, 32.0f, trackH);
    float thumbTravel = std::max(0.0f, trackH - thumbH);
    float thumbY = boxTop + thumbTravel * (scrollY / maxScroll);
    float thumbInset = 2.0f;
    track = {boxRight - trackW, boxTop, trackW, trackH};
    thumb = {boxRight - trackW + thumbInset, thumbY, trackW - thumbInset * 2.0f, thumbH};
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
    bool snapshotHovered = hovered;
    bool snapshotPressed = pressed;
    bool snapshotFocused = focused;
    float snapshotHoverAnim = hoverAnim;
    float snapshotRenderScale = renderScale;
    bool snapshotScrollbarHovered = scrollbarHovered;
    bool snapshotScrollbarDragging = scrollbarDragging;
    float snapshotScrollY = scrollY;

    if (!visible || computedStyle->display == Display::None ||
        computedStyle->visibility != Visibility::Visible) {
        hovered = false;
        pressed = false;
        return;
    }
    float dt = std::max(0.0f, input.deltaTime);
    bool oldHovered = hovered;
    hovered = bounds.contains(input.mousePos);
    if (hovered != oldHovered) {
        float from = hoverAnim;
        float to = hovered ? 1.0f : 0.0f;
        CompositorEngine::instance().animatePropertyFloat(
            reinterpret_cast<uintptr_t>(this), "hoverAnim",
            from, to, computedStyle->transitionDuration,
            TimingFunctionType::Spring,
            computedStyle->springStiffness, computedStyle->springDamping
        );
    }

    float compositedHoverAnim = 0.0f;
    if (CompositorEngine::instance().getAnimatedFloat(reinterpret_cast<uintptr_t>(this), "hoverAnim", compositedHoverAnim)) {
        hoverAnim = compositedHoverAnim;
    } else {
        hoverAnim = hovered ? 1.0f : 0.0f;
    }

    if (input.mouseClicked[0]) {
        if (hovered && (type == "button" || computedStyle->cursor == CursorType::Pointer)) {
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
    float currentScale = computedStyle->scale;
    if (computedStyle->hoverScale >= 0) {
        currentScale = computedStyle->scale + (computedStyle->hoverScale - computedStyle->scale) * hoverAnim;
    }
    if (focused && computedStyle->focusScale >= 0) {
        currentScale = computedStyle->focusScale;
    }
    if (pressed && computedStyle->activeScale >= 0) {
        currentScale = computedStyle->activeScale;
    }
    renderScale = currentScale;
    bool isScrollableWidget = scrollsOverflowY(computedStyle, contentHeight, bounds.h);
    int depth = 0;
    Widget* p = parent;
    while (p) {
        depth++;
        p = p->parent;
    }
    if (isScrollableWidget) {
        scrollY = CompositorEngine::instance().getScrollY(reinterpret_cast<uintptr_t>(this));
        targetScrollY = CompositorEngine::instance().getTargetScrollY(reinterpret_cast<uintptr_t>(this));
    }

    if (isScrollableWidget) {
        clampScroll();
        Rect track, thumb;
        bool hasScrollbar = getScrollBarRects(track, thumb);
        scrollbarHovered = hasScrollbar && hovered &&
            (track.contains(input.mousePos) || thumb.contains(input.mousePos));
        bool scrollChangedOnMainThread = false;

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
                scrollChangedOnMainThread = true;
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
                scrollChangedOnMainThread = true;
            } else {
                scrollbarDragging = false;
            }
        }

        // Note: Mouse wheel is handled off-main-thread by the CompositorEngine!

        int scrollKeyCode = normalizeTextEditingKey(input.keyCode);
        if (hovered && !scrollbarDragging && scrollKeyCode != 0) {
            float maxScroll = maxScrollY();
            switch (scrollKeyCode) {
            case 0x26:
                targetScrollY -= 48.0f;
                scrollChangedOnMainThread = true;
                break;
            case 0x28:
                targetScrollY += 48.0f;
                scrollChangedOnMainThread = true;
                break;
            case 0x21:
                targetScrollY -= bounds.h * 0.88f;
                scrollChangedOnMainThread = true;
                break;
            case 0x22:
                targetScrollY += bounds.h * 0.88f;
                scrollChangedOnMainThread = true;
                break;
            case 0x24:
                targetScrollY = 0.0f;
                scrollChangedOnMainThread = true;
                break;
            case 0x23:
                targetScrollY = maxScroll;
                scrollChangedOnMainThread = true;
                break;
            default:
                break;
            }
            clampScroll();
        }

        if (scrollChangedOnMainThread) {
            CompositorEngine::instance().setScrollY(reinterpret_cast<uintptr_t>(this), scrollY, targetScrollY);
        }
    } else {
        scrollbarHovered = false;
        scrollbarDragging = false;
    }

    // Always register/update our layout geometry with the CompositorEngine
    CompositorEngine::instance().registerScrollableWidget(
        reinterpret_cast<uintptr_t>(this),
        bounds,
        contentHeight,
        scrollY,
        targetScrollY,
        depth,
        isScrollableWidget
    );
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
    const bool isColumn = (computedStyle->flexDirection == FlexDirection::Column);
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
        if (child->computedStyle->position == Position::Fixed) continue;
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

    // A hover/press state flip only needs a repaint if this widget's appearance
    // actually depends on that state (a :hover / :active color, background,
    // border, gradient, scale, or opacity). Plain layout containers (the bulk of
    // a UI tree) have no hover styling, so their transient hover flag toggling —
    // which can happen every frame from sub-pixel hit-test noise — must NOT pin
    // the app awake. This is the difference between idling and burning a core.
    const Style& cs = *computedStyle;
    bool hasInteractiveStyling =
        cs.hasHoverColor || cs.hasHoverBg || cs.hasHoverBorder || cs.hasHoverGradient ||
        cs.hoverScale >= 0 || cs.hoverOpacity >= 0 ||
        cs.focusScale >= 0 || cs.activeScale >= 0 ||
        type == "button" || type == "input" || type == "textarea" ||
        type == "checkbox" || type == "radio" || type == "select" || type == "a";

    bool interactiveChanged =
        hasInteractiveStyling &&
        (hovered != snapshotHovered || pressed != snapshotPressed || focused != snapshotFocused);
    bool structuralChanged =
        renderScale != snapshotRenderScale ||
        scrollbarHovered != snapshotScrollbarHovered ||
        scrollbarDragging != snapshotScrollbarDragging ||
        scrollY != snapshotScrollY;
    // A changing hoverAnim only warrants a redraw while its spring is genuinely
    // in flight. Once the compositor GCs the settled tween, getAnimatedFloat()
    // returns false and hoverAnim snaps to its exact target, so we must NOT keep
    // requesting redraws off tiny residual float deltas.
    bool hoverAnimLive = hasInteractiveStyling && (hoverAnim != snapshotHoverAnim) &&
        CompositorEngine::instance().hasAnimations(reinterpret_cast<uintptr_t>(this));
    (void)snapshotHovered;
    if ((interactiveChanged || structuralChanged || hoverAnimLive) && layoutObject) {
        layoutObject->markPaintDirty();
        if (auto* app = Application::instance()) {
            app->requestRedraw();
        }
    }

    // Drive CSS @keyframes animations (CSS Animations Level 1, 4).
    // Each frame, advance every active animation effect, interpolate the relevant
    // properties along the resolved keyframe timeline, and write the instantaneous
    // value back into this widget's computed style + the compositor (Blink
    // animation-compositor pattern). Subclasses that override update() should call
    // tickAnimations() at the end of their frame loop.
    tickAnimations(input);
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
        if ((*it)->computedStyle->position == Position::Fixed) continue;
        CursorType childCursor = (*it)->cursorAt(childPoint);
        if (childCursor != CursorType::Default) return childCursor;
    }
    return computedStyle->cursor;
}
Widget* Widget::hitTest(Vec2 point, bool interactiveOnly) {
    Vec2 transformedPoint = point;
    if (renderScale != 1.0f && renderScale != 0.0f) {
        Vec2 c = bounds.center();
        transformedPoint.x = c.x + (point.x - c.x) / renderScale;
        transformedPoint.y = c.y + (point.y - c.y) / renderScale;
    }

    if (!canHitTestWidget(this) || !bounds.contains(transformedPoint)) {
        return nullptr;
    }
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        Rect track, thumb;
        if (getScrollBarRects(track, thumb) && (track.contains(transformedPoint) || thumb.contains(transformedPoint))) {
            return this;
        }
    }
    Vec2 childPoint = transformedPoint;
    if (scrollsOverflowY(computedStyle, contentHeight, bounds.h)) {
        childPoint.y += scrollY;
    }
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->computedStyle->position == Position::Fixed) continue;
        if (Widget* child = (*it)->hitTest(childPoint, interactiveOnly)) {
            return child;
        }
    }
    if (!interactiveOnly || type == "button" || computedStyle->cursor == CursorType::Pointer || onClick) {
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
        if (child->computedStyle->display == Display::ListItem) {
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
    if (computedStyle->display != Display::ListItem) return;
    if (computedStyle->listStyleType == ListStyleType::None) return;

    std::string markerText;
    int index = getListItemIndex(this);
    switch (computedStyle->listStyleType) {
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

    Color textColor = hasMarkerStyle && markerStyle->hasColor
        ? markerStyle->color : computedStyle->color;
    float markerFontSize = hasMarkerStyle && markerStyle->hasFontSize
        ? markerStyle->fontSize : computedStyle->fontSize;
    FontWeight markerFontWeight = hasMarkerStyle && markerStyle->hasFontWeight
        ? markerStyle->fontWeight : computedStyle->fontWeight;
    const std::string& fontName = renderFontName(computedStyle);
    if (!markerText.empty()) {
        Vec2 textSize = renderer.measureText(markerText, markerFontSize, fontName);
        float x = 0.0f;
        float y = bounds.y + computedStyle->padding.top;
        if (computedStyle->direction == Direction::Ltr) {
            x = bounds.x - 8.0f - textSize.x;
        } else {
            x = bounds.x + bounds.w + 8.0f;
        }
        renderer.drawText(markerText, Vec2(x, y), textColor, markerFontSize,
                          markerFontWeight, fontName, computedStyle->fontStyle,
                          computedStyle->direction, computedStyle->unicodeBidi);
    } else {
        // Bullet shapes: Disc, Circle, Square
        float bulletRadius = computedStyle->fontSize * 0.2f;
        float d = bulletRadius * 2.0f;
        float x = 0.0f;
        float y = bounds.y + computedStyle->padding.top + computedStyle->fontSize * 0.5f - bulletRadius;
        if (computedStyle->direction == Direction::Ltr) {
            x = bounds.x - 15.0f - bulletRadius;
        } else {
            x = bounds.x + bounds.w + 15.0f - bulletRadius;
        }
        if (computedStyle->listStyleType == ListStyleType::Disc) {
            renderer.drawRoundedRect(Rect(x, y, d, d), textColor, BorderRadius(bulletRadius));
        } else if (computedStyle->listStyleType == ListStyleType::Circle) {
            renderer.drawBorder(Rect(x, y, d, d), Border(1.2f, textColor), BorderRadius(bulletRadius));
        } else if (computedStyle->listStyleType == ListStyleType::Square) {
            renderer.drawRoundedRect(Rect(x, y, d, d), textColor, BorderRadius(0.0f));
        }
    }
}

void Widget::renderBackground(Renderer& renderer) {
    const Style& s = *computedStyle;
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
    const bool isColumn = (computedStyle->flexDirection == FlexDirection::Column);
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
        if (child->computedStyle->position == Position::Fixed) continue;
        if (clip && !rectIntersects(child->bounds, visibleContent, 64.0f)) continue;
        if (isExpandedSelectWidget(child.get())) continue;
        child->render(renderer);
    }
    for (size_t i = startIndex; i < endIndex; i++) {
        auto& child = children[i];
        if (!canPaintWidget(child.get())) continue;
        if (child->computedStyle->position == Position::Fixed) continue;
        if (clip && !rectIntersects(child->bounds, visibleContent, 64.0f)) continue;
        if (!isExpandedSelectWidget(child.get())) continue;
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

    const Style& s = *computedStyle;

    // ── Push compositing layer for filter/blend/isolation (Blink cc::PaintOp parity) ──
    bool needsCompositingLayer = s.hasMixBlendMode || s.hasFilter || s.hasIsolation;
    if (needsCompositingLayer && renderer.isRecording()) {
        RenderCommand saveCmd;
        saveCmd.type = RenderCommandType::SaveLayer;
        saveCmd.rect = bounds;
        saveCmd.opacity = s.opacity;
        saveCmd.blendMode = s.mixBlendMode;
        saveCmd.filterOps = s.filterOperations;
        saveCmd.isolate   = (s.isolation == Style::Isolation::Isolate);
        renderer.recordCommand(saveCmd);
    }

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

    // ── Pop compositing layer ──
    if (needsCompositingLayer && renderer.isRecording()) {
        RenderCommand restoreCmd;
        restoreCmd.type = RenderCommandType::RestoreLayer;
        restoreCmd.rect = bounds;
        renderer.recordCommand(restoreCmd);
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
    // Give the (still-hidden/cloaked) window its final geometry now, so the very
    // first rendered frame is produced at the correct size and the reveal is
    // flicker-free — no black flash before the UI appears.
    Platform::prepareWindow(window_);
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
    root_->computedStyle.ensureMutable().display = Display::Flex;
    root_->computedStyle.ensureMutable().flexDirection = FlexDirection::Column;
    root_->computedStyle.ensureMutable().overflowY = Overflow::Auto;
    axObjectCache_ = std::make_unique<AXObjectCache>();
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
    if (ok) {
        // Record the file as a style source for hot-reload (dedup by path).
        bool found = false;
        for (auto& s : styleSources_) {
            if (s.isFile && s.pathOrCss == path) { s.lastWriteNs = fileWriteTimeNs(path); found = true; break; }
        }
        if (!found) {
            styleSources_.push_back({true, path, fileWriteTimeNs(path)});
        }
    }
    return ok;
}
void Application::addStylesheet(const std::string& css) {
    stylesheet_.parse(css);
    for (const auto& ff : stylesheet_.fontFaces) {
        renderer_.registerCustomFont(ff.fontFamily, ff.src);
    }
    if (root_) root_->markStyleDirtyRecursive();
    needsRedraw_ = true;
    // Record inline CSS so a hot-reload replays it in the same cascade order.
    styleSources_.push_back({false, css, 0});
}
int64_t Application::fileWriteTimeNs(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               t.time_since_epoch()).count();
}
void Application::setFrameRateLimits(int activeFps, int batteryFps, int backgroundFps) {
    if (activeFps > 0) activeFps_ = activeFps;
    if (batteryFps > 0) batteryFps_ = batteryFps;
    if (backgroundFps > 0) backgroundFps_ = backgroundFps;
}
// Pure, unit-testable pacing policy. Returns the target FPS for the given
// conditions; 0 means "uncapped" (let the caller run as fast as it draws).
int Application::computeTargetFps(const PacingInputs& in) {
    // A backgrounded / minimized window only needs a trickle to stay responsive
    // to OS events; this is the single biggest battery win.
    if (!in.windowActive) {
        return in.backgroundFps > 0 ? in.backgroundFps : 10;
    }

    // The CPU/software rasterizer is expensive per pixel — never let it run
    // above the battery tier, even on AC, regardless of profile.
    int activeCap = in.activeFps > 0 ? in.activeFps : 120;
    int batteryCap = in.batteryFps > 0 ? in.batteryFps : 60;

    switch (in.profile) {
        case PowerProfile::HighPerformance:
            return in.softwareBackend ? std::min(activeCap, batteryCap) : activeCap;
        case PowerProfile::PowerSaver:
            // Aggressive: battery tier when idle, halve it further with no anim.
            return in.hasAnimations ? batteryCap : std::max(15, batteryCap / 2);
        case PowerProfile::Balanced: {
            int cap = batteryCap;
            if (in.softwareBackend) cap = std::min(cap, batteryCap);
            return cap;
        }
        case PowerProfile::Auto:
        default: {
            if (in.softwareBackend) return batteryCap;
            if (in.batterySaver) return std::max(15, batteryCap / 2);
            if (in.onBattery) return batteryCap;
            return activeCap;  // AC + GPU + focused => full speed
        }
    }
}
void Application::enableHotReload(bool enable, float pollIntervalSeconds) {
    hotReloadEnabled_ = enable;
    hotReloadInterval_ = pollIntervalSeconds > 0.0f ? pollIntervalSeconds : 0.25f;
    hotReloadAccum_ = 0.0f;
    if (enable) {
        // Refresh mtimes so the first poll only fires on a genuine change.
        for (auto& s : styleSources_) {
            if (s.isFile) s.lastWriteNs = fileWriteTimeNs(s.pathOrCss);
        }
    }
}
void Application::watchStylesheet(const std::string& path) {
    for (auto& s : styleSources_) {
        if (s.isFile && s.pathOrCss == path) return;  // already watched
    }
    // Watch-only entries are loaded during reloadStyles() like any other file.
    styleSources_.push_back({true, path, fileWriteTimeNs(path)});
}
bool Application::reloadStyles() {
    // Rebuild the cascade from scratch: reset to the UA sheet, then replay every
    // recorded source (files re-read from disk, inline CSS re-parsed) in order.
    stylesheet_.reset();
    bool allOk = true;
    for (auto& s : styleSources_) {
        if (s.isFile) {
            bool ok = stylesheet_.loadFile(s.pathOrCss);
            if (ok) {
                std::string dir;
                size_t lastSlash = s.pathOrCss.find_last_of("/\\");
                if (lastSlash != std::string::npos) dir = s.pathOrCss.substr(0, lastSlash + 1);
                for (const auto& ff : stylesheet_.fontFaces) {
                    std::string fontPath = ff.src;
                    if (!fontPath.empty() && fontPath[0] != '/' && fontPath[0] != '\\' &&
                        (fontPath.size() < 2 || fontPath[1] != ':')) {
                        fontPath = dir + fontPath;
                    }
                    renderer_.registerCustomFont(ff.fontFamily, fontPath);
                }
                s.lastWriteNs = fileWriteTimeNs(s.pathOrCss);
            } else {
                allOk = false;
            }
        } else {
            stylesheet_.parse(s.pathOrCss);
            for (const auto& ff : stylesheet_.fontFaces) {
                renderer_.registerCustomFont(ff.fontFamily, ff.src);
            }
        }
    }
    if (root_) root_->markStyleDirtyRecursive();
    needsRedraw_ = true;
    return allOk;
}
bool Application::pollStyleHotReload() {
    if (!hotReloadEnabled_) return false;
    bool changed = false;
    for (auto& s : styleSources_) {
        if (!s.isFile) continue;
        int64_t now = fileWriteTimeNs(s.pathOrCss);
        if (now != 0 && now != s.lastWriteNs) {
            changed = true;
            // mtime is refreshed inside reloadStyles().
        }
    }
    if (changed) {
        reloadStyles();
    }
    return changed;
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
        if (!w || !w->visible || w->computedStyle->display == Display::None) return;
        if (w->computedStyle->position == Position::Fixed) {
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
            if (focusTarget->type == "button" || focusTarget->computedStyle->cursor == CursorType::Pointer ||
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
        if (!w || !w->visible || w->computedStyle->display == Display::None) return;
        if (w->computedStyle->position == Position::Fixed) {
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
        if (!w || !w->visible || w->computedStyle->display == Display::None) return;
        if (w->computedStyle->position == Position::Fixed) {
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
    if (CompositorEngine::instance().handleMouseWheel(x, y, dy)) {
        input_.scroll = {0.0f, 0.0f};
        return;
    }
    Widget* target = nullptr;
    std::vector<Widget*> fixedWidgets;
    std::function<void(Widget*)> gatherFixed = [&](Widget* w) {
        if (!w || !w->visible || w->computedStyle->display == Display::None) return;
        if (w->computedStyle->position == Position::Fixed) {
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
void Application::updateStyleAndLayout() {
    if (documentLifecycle >= DocumentLifecycle::LayoutClean && root_ && !root_->subtreeStyleDirty && !root_->layoutDirty) {
        return;
    }
    // Avoid re-entrancy / infinite recursion
    if (documentLifecycle == DocumentLifecycle::InStyleRecalc ||
        documentLifecycle == DocumentLifecycle::InLayout) {
        return;
    }

    int w = 0, h = 0;
    Platform::getWindowSize(window_, w, h);
    if (w <= 0 || h <= 0) {
        w = 1920; h = 1080;
    }

    if (root_) {
        root_->checkFocusChanges();
    }

    documentLifecycle = DocumentLifecycle::InStyleRecalc;
    root_->resolveStyles(stylesheet_);
    documentLifecycle = DocumentLifecycle::StyleClean;

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

    if (axObjectCache_) {
        axObjectCache_->update(root_.get());
    }
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

    if (axObjectCache_) {
        axObjectCache_->update(root_.get());
    }
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
        if (!widget || !widget->visible || widget->computedStyle->display == Display::None) return;
        if (widget->computedStyle->position == Position::Fixed) {
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
    if (activeViewTransition_.active) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = now - activeViewTransition_.startTime;
        float progress = std::clamp(elapsed.count() / activeViewTransition_.duration, 0.0f, 1.0f);

        renderer_.playback(activeViewTransition_.oldCommands, 1.0f - progress);
        renderer_.playback(activeViewTransition_.newCommands, progress);

        if (progress >= 1.0f) {
            activeViewTransition_.active = false;
            activeViewTransition_.oldCommands.clear();
            activeViewTransition_.newCommands.clear();
        } else {
            needsRedraw_ = true;
        }
    } else {
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
    // Seed the power-source cache so the first frame already paces correctly.
    {
        PowerStatus ps = Platform::getPowerStatus();
        cachedOnBattery_ = (ps.source == PowerSource::Battery);
        cachedBatterySaver_ = ps.batterySaver;
    }
    while (running) {
        processEvents();
        if (!running) {
            break;
        }
        // CSS hot-reload: poll watched files on an interval. Accumulate real
        // elapsed time so the poll cadence is independent of frame rate.
        if (hotReloadEnabled_) {
            auto nowHR = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> sinceLast = nowHR - lastTime_;
            hotReloadAccum_ += sinceLast.count();
            if (hotReloadAccum_ >= hotReloadInterval_) {
                hotReloadAccum_ = 0.0f;
                pollStyleHotReload();  // sets needsRedraw_ if a file changed
            }
        }
        bool windowActive = Platform::isWindowActive(window_);
        bool hasAnimations = false;
        if (!needsRedraw_ && !firstFrame && root_) {
            hasAnimations = root_->hasActiveAnimations();
        }
        if (!needsRedraw_ && !hasAnimations && !firstFrame) {
#ifdef _WIN32
            if (hotReloadEnabled_) {
                // Don't block indefinitely: wake at least once per poll interval
                // so file changes are picked up even with no input/animations.
                DWORD waitMs = (DWORD)std::max(1.0f, hotReloadInterval_ * 1000.0f);
                MsgWaitForMultipleObjects(0, nullptr, FALSE, waitMs, QS_ALLINPUT);
            } else {
                WaitMessage();
            }
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

        // Refresh the power source on a slow cadence (~2s); GetSystemPowerStatus
        // and friends are cheap but there's no need to hit them every frame.
        powerPollAccum_ += input_.deltaTime;
        if (powerPollAccum_ >= 2.0f) {
            powerPollAccum_ = 0.0f;
            PowerStatus ps = Platform::getPowerStatus();
            cachedOnBattery_ = (ps.source == PowerSource::Battery);
            cachedBatterySaver_ = ps.batterySaver;
        }

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

        // ── Adaptive, power-aware frame pacing ──
        // Choose the target FPS from the power source, window focus, backend and
        // animation state. Sleeping the leftover time keeps the CPU/GPU cool and
        // the battery happy — and keeps software (no-GPU) rendering responsive
        // without spinning a core at 100%.
        PacingInputs pacing;
        pacing.profile = powerProfile_;
        pacing.onBattery = cachedOnBattery_;
        pacing.batterySaver = cachedBatterySaver_;
        pacing.windowActive = windowActive;
        pacing.softwareBackend = (renderer_.activeBackend() == RenderBackendType::Compatibility);
        pacing.hasAnimations = hasAnimations || (root_ && root_->hasActiveAnimations());
#if FLUXUI_TARGET_FPS > 0
        pacing.activeFps = activeFps_ > 0 ? activeFps_ : FLUXUI_TARGET_FPS;
#else
        pacing.activeFps = activeFps_;  // 0 => uncapped on AC + GPU
#endif
        pacing.batteryFps = batteryFps_;
        pacing.backgroundFps = backgroundFps_;

        int targetFPS = computeTargetFps(pacing);
        float targetFrameSeconds = targetFPS > 0 ? 1.0f / (float)targetFPS : 0.0f;
        auto frameElapsed = std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - now).count();
        if (targetFrameSeconds > 0.0f && frameElapsed < targetFrameSeconds) {
            std::this_thread::sleep_for(
                std::chrono::duration<float>(targetFrameSeconds - frameElapsed));
        } else {
            std::this_thread::yield();
        }
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

void Application::shutdown() {
#ifdef _WIN32
    if (window_) {
        SetWindowLongPtr((HWND)window_, GWLP_USERDATA, 0);
    }
#endif
    renderer_.shutdown();
    if (window_) {
        Platform::destroyWindow(window_);
        window_ = nullptr;
    }
    Platform::shutdown();
}

void Application::startViewTransition(std::function<void()> mutationCallback) {
    if (!root_) return;

    if (root_->layoutDirty || root_->lifecycleState < WidgetLifecycle::LayoutClean) {
        root_->layout(root_->bounds);
    }

    std::vector<RenderCommand> oldCommands;
    renderer_.startRecording(oldCommands);
    if (root_->layoutObject) {
        root_->layoutObject->paint(renderer_);
    } else {
        root_->render(renderer_);
    }
    renderer_.stopRecording();

    mutationCallback();

    root_->markStyleDirtyRecursive();
    root_->resolveStyles(stylesheet_);
    root_->layout(root_->bounds);

    std::vector<RenderCommand> newCommands;
    renderer_.startRecording(newCommands);
    if (root_->layoutObject) {
        root_->layoutObject->paint(renderer_);
    } else {
        root_->render(renderer_);
    }
    renderer_.stopRecording();

    activeViewTransition_.active = true;
    activeViewTransition_.oldCommands = std::move(oldCommands);
    activeViewTransition_.newCommands = std::move(newCommands);
    activeViewTransition_.startTime = std::chrono::high_resolution_clock::now();
    activeViewTransition_.duration = 0.25f;
    needsRedraw_ = true;
}

// SVG widget implementation moved to core/widgets/svg.cpp.

// [ignoring loop detection]
// Video widget implementation moved to core/widgets/video.cpp.

// ============================================================
//  CSS @keyframes animation runtime
//  (parity with Blink css_animations.cc / KeyframeEffect / Animation)
// ============================================================

// Apply a single keyframe property value to a local mutable Style, returning true if
// the property was recognized. Recognized animatable properties (per CSS Animations
// Level 1 §2 + Web Animations spec table) include opacity, color, background-color,
// border-color, transform (scale), width/height, font-size, margin/padding, etc.
bool Widget::applyKeyframePropertyOverride(const std::string& name, const std::string& value) {
    if (name.empty() || value.empty()) return false;

    if (name == "opacity") {
        computedStyle.ensureMutable().opacity = StyleSheet::parseFloat(value);
        return true;
    }
    if (name == "color") {
        computedStyle.ensureMutable().color = StyleSheet::parseColor(value);
        computedStyle.ensureMutable().hasColor = true;
        return true;
    }
    if (name == "background-color" || name == "background") {
        Color c = StyleSheet::parseColor(value);
        if (c.a > 0.0f) {
            computedStyle.ensureMutable().backgroundColor = c;
        }
        return true;
    }
    if (name == "border-color") {
        computedStyle.ensureMutable().border.color = StyleSheet::parseColor(value);
        return true;
    }
    if (name == "scale") {
        computedStyle.ensureMutable().scale = StyleSheet::parseFloat(value);
        return true;
    }
    if (name == "transform") {
        computedStyle.ensureMutable().transform = StyleSheet::parseTransformOperations(value);
        computedStyle.ensureMutable().hasTransform = true;
        // Also extract scale if present (for backward compatibility / optimization)
        for (const auto& op : computedStyle->transform) {
            if (op.type == TransformOperationType::Scale ||
                op.type == TransformOperationType::Scale3d ||
                op.type == TransformOperationType::ScaleX) {
                if (!op.args.empty()) {
                    float val = op.args[0].value;
                    if (op.args[0].unit == CSSValue::Percent) {
                        val /= 100.0f;
                    }
                    computedStyle.ensureMutable().scale = val;
                }
            }
        }
        return true;
    }
    if (name == "width") {
        computedStyle.ensureMutable().width = StyleSheet::parseCSSValue(value);
        computedStyle.ensureMutable().hasWidthVal = true;
        return true;
    }
    if (name == "height") {
        computedStyle.ensureMutable().height = StyleSheet::parseCSSValue(value);
        computedStyle.ensureMutable().hasHeightVal = true;
        return true;
    }
    if (name == "font-size") {
        computedStyle.ensureMutable().fontSize = StyleSheet::parseFontSizePixels(value, computedStyle->fontSize);
        computedStyle.ensureMutable().hasFontSize = true;
        return true;
    }
    if (name == "padding") {
        computedStyle.ensureMutable().padding = StyleSheet::parseEdgeInsets(value, computedStyle->fontSize);
        return true;
    }
    if (name == "margin") {
        computedStyle.ensureMutable().margin = StyleSheet::parseEdgeInsets(value, computedStyle->fontSize);
        return true;
    }
    if (name == "border-radius") {
        computedStyle.ensureMutable().borderRadius = StyleSheet::parseBorderRadius(value, computedStyle->fontSize);
        return true;
    }
    if (name == "filter") {
        if (value == "none") {
            computedStyle.ensureMutable().filterOperations.clear();
            computedStyle.ensureMutable().hasFilter = false;
        } else {
            auto ops = StyleSheet::parseFilterOperations(value, computedStyle->fontSize);
            computedStyle.ensureMutable().filterOperations = std::move(ops);
            computedStyle.ensureMutable().hasFilter = !computedStyle->filterOperations.empty();
        }
        return true;
    }
    if (name == "backdrop-filter") {
        if (value == "none") {
            computedStyle.ensureMutable().backdropFilterOperations.clear();
            computedStyle.ensureMutable().hasBackdropFilter = false;
            computedStyle.ensureMutable().backdropFilterBlur = 0.0f;
            computedStyle.ensureMutable().hasBackdropFilterBlur = false;
        } else {
            auto ops = StyleSheet::parseFilterOperations(value, computedStyle->fontSize);
            computedStyle.ensureMutable().backdropFilterBlur = 0.0f;
            computedStyle.ensureMutable().hasBackdropFilterBlur = false;
            for (const auto& op : ops) {
                if (op.type == FilterOperationType::Blur) {
                    computedStyle.ensureMutable().backdropFilterBlur = op.amount;
                    computedStyle.ensureMutable().hasBackdropFilterBlur = true;
                    break;
                }
            }
            computedStyle.ensureMutable().backdropFilterOperations = std::move(ops);
            computedStyle.ensureMutable().hasBackdropFilter = !computedStyle->backdropFilterOperations.empty();
        }
        return true;
    }
    if (name == "top") {
        computedStyle.ensureMutable().top = StyleSheet::parseCSSValue(value);
        return true;
    }
    if (name == "left") {
        computedStyle.ensureMutable().left = StyleSheet::parseCSSValue(value);
        return true;
    }
    if (name == "right") {
        computedStyle.ensureMutable().right = StyleSheet::parseCSSValue(value);
        return true;
    }
    if (name == "bottom") {
        computedStyle.ensureMutable().bottom = StyleSheet::parseCSSValue(value);
        return true;
    }
    if (name == "transform-origin") {
        computedStyle.ensureMutable().transformOrigin = StyleSheet::parseTransformOrigin(value);
        computedStyle.ensureMutable().hasTransformOrigin = true;
        return true;
    }
    if (name == "transform-style") {
        computedStyle.ensureMutable().transformStyle = StyleSheet::parseTransformStyle(value);
        computedStyle.ensureMutable().hasTransformStyle = true;
        return true;
    }
    if (name == "transform-box") {
        computedStyle.ensureMutable().transformBox = StyleSheet::parseTransformBox(value);
        computedStyle.ensureMutable().hasTransformBox = true;
        return true;
    }
    if (name == "perspective") {
        computedStyle.ensureMutable().perspective = StyleSheet::parsePerspective(value);
        computedStyle.ensureMutable().hasPerspective = true;
        return true;
    }
    if (name == "perspective-origin") {
        computedStyle.ensureMutable().perspectiveOrigin = StyleSheet::parsePerspectiveOrigin(value);
        computedStyle.ensureMutable().hasPerspectiveOrigin = true;
        return true;
    }
    if (name == "backface-visibility") {
        computedStyle.ensureMutable().backfaceVisibility = StyleSheet::parseBackfaceVisibility(value);
        computedStyle.ensureMutable().hasBackfaceVisibility = true;
        return true;
    }
    return false;
}

static std::string resolveKeyframeValueVars(const StyleSheet& sheet, const std::string& value) {
    if (value.find("var(") == std::string::npos) return value;
    static const std::unordered_map<std::string, std::string> emptyCustomProps;
    return sheet.resolveValue(value, emptyCustomProps);
}

void Widget::clearAnimationOverrides() {
    CompositorEngine::instance().clearKeyframeOverrides(reinterpret_cast<uintptr_t>(this));
    activeAnimations.clear();
    firstUpdate = true;
    localClock = 0.0f;
    hasLastResolveKey = false;
    markStyleDirty();
}

static std::string serializeAnimationNameList(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ',';
        out += v[i];
    }
    return out;
}

void Widget::tickAnimations(const InputState& input) {
    if (!currentSheet) return;
    if (styleDirty) {
        resolveStyles(*currentSheet);
    }
    const Style& s = *computedStyle;
    if (s.animationName.empty()) {
        if (!activeAnimations.empty()) {
            // Animation list cleared (style change removed all animations): wipe.
            clearAnimationOverrides();
        }
        return;
    }

    // Advance local clock
    float dt = std::max(0.0f, input.deltaTime);
    if (firstUpdate) { localClock = 0.0f; firstUpdate = false; }
    else              { localClock += dt; }

    // Reconcile the running animation list with the current style. New names spawn a
    // fresh ActiveAnimation; missing names have their compositor overrides cleared.
    std::string sig = serializeAnimationNameList(s.animationName);
    if (sig != lastAnimationSignature) {
        // Mark all existing as not-yet-restarted so we can detect name disappearance
        // without wiping overrides mid-frame.
        std::vector<bool> keep(s.animationName.size(), false);
        for (size_t ni = 0; ni < s.animationName.size(); ++ni) {
            const std::string& want = s.animationName[ni];
            if (want.empty() || want == "none") continue;
            for (auto& existing : activeAnimations) {
                if (existing.name == want) { keep[ni] = true; break; }
            }
        }
        // Remove animations no longer in the list
        activeAnimations.erase(
            std::remove_if(activeAnimations.begin(), activeAnimations.end(),
                [&](const ActiveAnimation& a) {
                    for (size_t ni = 0; ni < s.animationName.size(); ++ni) {
                        if (s.animationName[ni] == a.name) return false;
                    }
                    return true;
                }),
            activeAnimations.end());

        // Spawn missing ones
        for (size_t ni = 0; ni < s.animationName.size(); ++ni) {
            const std::string& want = s.animationName[ni];
            if (want.empty() || want == "none") continue;
            bool found = false;
            for (auto& existing : activeAnimations) {
                if (existing.name == want) { found = true; break; }
            }
            if (found) continue;
            ActiveAnimation aa;
            aa.name = want;
            aa.duration  = ni < s.animationDuration.size()        ? s.animationDuration[ni]        : 0.0f;
            aa.delay     = ni < s.animationDelay.size()           ? s.animationDelay[ni]           : 0.0f;
            aa.iterationCount = ni < s.animationIterationCount.size() ? s.animationIterationCount[ni] : 1.0f;
            aa.direction  = ni < s.animationDirection.size()       ? s.animationDirection[ni]       : AnimationDirection::Normal;
            aa.fillMode   = ni < s.animationFillMode.size()        ? s.animationFillMode[ni]        : AnimationFillMode::None;
            aa.playState  = ni < s.animationPlayState.size()       ? s.animationPlayState[ni]       : AnimationPlayState::Running;
            aa.timingFunction = ni < s.animationTimingFunction.size() ? s.animationTimingFunction[ni] : TimingFunction::ease();
            aa.composition = ni < s.animationComposition.size()    ? s.animationComposition[ni]    : AnimationComposition::Replace;
            aa.startTime = localClock;
            activeAnimations.push_back(aa);
        }
        lastAnimationSignature = sig;
    }

    // Tick each active animation
    auto& engine = CompositorEngine::instance();
    const uintptr_t wid = reinterpret_cast<uintptr_t>(this);
    bool anyChange = false;

    for (auto& aa : activeAnimations) {
        if (aa.finished) continue;
        if (aa.playState == AnimationPlayState::Paused) continue;

        const CSSKeyframesRule* kfRule = currentSheet->findKeyframes(aa.name);
        if (!kfRule || kfRule->keyframes.empty()) {
            aa.finished = true;
            continue;
        }
        if (aa.duration <= 0.0f) {
            // CSS spec: if duration is 0, the animation completes immediately
            aa.currentTime = 0.0f;
            aa.currentIteration = 0;
            aa.finished = true;
            continue;
        }

        float localT = localClock - aa.startTime - aa.delay;
        if (localT < 0.0f) {
            // In delay window. Apply 0% keyframe values if fill-mode is backwards/both.
            if (aa.fillMode == AnimationFillMode::Backwards || aa.fillMode == AnimationFillMode::Both) {
                for (const auto& kf : kfRule->keyframes) {
                    if (kf.keyTimes.empty()) continue;
                    float minT = *std::min_element(kf.keyTimes.begin(), kf.keyTimes.end());
                    if (minT <= 0.0f) {
                        for (const auto& prop : kf.properties) {
                            std::string val = resolveKeyframeValueVars(*currentSheet, prop.value);
                            if (applyKeyframePropertyOverride(std::string(prop.name), val)) anyChange = true;
                        }
                    }
                }
            }
            continue;
        }

        int totalIterations = (aa.iterationCount < 0.0f) ? -1 : (int)aa.iterationCount;
        int currentIteration = (int)std::floor(localT / aa.duration);
        if (totalIterations >= 0 && currentIteration >= totalIterations) {
            // Animation is over
            aa.finished = true;
            aa.currentIteration = totalIterations;
            if (aa.fillMode == AnimationFillMode::Forwards || aa.fillMode == AnimationFillMode::Both) {
                // Apply 100% (or last available) keyframe values
                const CSSKeyframeRule* lastKf = &kfRule->keyframes.back();
                for (const auto& prop : lastKf->properties) {
                    std::string val = resolveKeyframeValueVars(*currentSheet, prop.value);
                    if (applyKeyframePropertyOverride(std::string(prop.name), val)) anyChange = true;
                }
            } else {
                // fill: none — wipe overrides so the underlying value is restored on the
                // next styleDirty resolve.
                engine.clearKeyframeOverrides(wid);
                hasLastResolveKey = false;
                markStyleDirty();
            }
            continue;
        }
        aa.currentIteration = currentIteration;

        // Within an iteration: compute [0,1] progress, then apply direction.
        float progress = (localT - currentIteration * aa.duration) / aa.duration;
        if (aa.direction == AnimationDirection::Reverse) {
            progress = 1.0f - progress;
        } else if (aa.direction == AnimationDirection::Alternate) {
            if (currentIteration % 2 == 1) progress = 1.0f - progress;
        } else if (aa.direction == AnimationDirection::AlternateReverse) {
            if (currentIteration % 2 == 0) progress = 1.0f - progress;
        }
        progress = std::clamp(progress, 0.0f, 1.0f);

        // Find the surrounding keyframe pair
        const CSSKeyframeRule* kfA = nullptr;
        const CSSKeyframeRule* kfB = nullptr;
        float tA = 0.0f, tB = 1.0f;
        // Sort kfRule->keyframes by min offset (they're already sorted in parseKeyframes,
        // but we re-derive tA/tB per key to support comma-grouped selectors).
        const CSSKeyframeRule* prev = nullptr;
        for (const auto& kf : kfRule->keyframes) {
            if (kf.keyTimes.empty()) continue;
            float minT = *std::min_element(kf.keyTimes.begin(), kf.keyTimes.end());
            float maxT = *std::max_element(kf.keyTimes.begin(), kf.keyTimes.end());
            if (minT <= progress && progress <= maxT) {
                // Within this comma-grouped selector
                kfA = &kf;
                tA = minT;
                tB = maxT;
                break;
            }
            if (minT > progress) {
                kfB = &kf;
                tA = (prev ? (*std::max_element(prev->keyTimes.begin(), prev->keyTimes.end())) : 0.0f);
                tB = minT;
                kfA = prev;
                break;
            }
            prev = &kf;
        }
        if (!kfA && !kfB) {
            // progress is at the very end and not matched
            kfA = &kfRule->keyframes.back();
            kfB = kfA;
            tA = tB = 1.0f;
        } else if (kfA && !kfB) {
            kfB = kfA;
        } else if (!kfA && kfB) {
            kfA = kfB;
            tA = tB = 0.0f;
        }

        // If both points coincide, apply directly
        float segmentT = 0.0f;
        if (tB > tA) {
            segmentT = (progress - tA) / (tB - tA);
        } else {
            segmentT = 0.0f;
        }
        float easedT = StyleSheet::sampleTimingFunction(aa.timingFunction, segmentT);

        // For each property in either keyframe, interpolate.
        // Build property→value map per keyframe.
        std::unordered_map<std::string, std::string> mapA, mapB;
        if (kfA) for (const auto& p : kfA->properties) mapA[std::string(p.name)] = p.value;
        if (kfB) for (const auto& p : kfB->properties) mapB[std::string(p.name)] = p.value;
        // Union of properties
        std::vector<std::string> allProps;
        for (const auto& kv : mapA) allProps.push_back(kv.first);
        for (const auto& kv : mapB) {
            if (mapA.find(kv.first) == mapA.end()) allProps.push_back(kv.first);
        }
        for (const std::string& prop : allProps) {
            auto itA = mapA.find(prop);
            auto itB = mapB.find(prop);
            std::string valA = (itA != mapA.end()) ? itA->second : std::string();
            std::string valB = (itB != mapB.end()) ? itB->second : std::string();
            // If one side is empty, treat it as the other side (no animation effect on
            // this prop during this segment).
            if (valA.empty() || valB.empty()) {
                std::string use = valA.empty() ? valB : valA;
                if (use.empty()) continue;
                std::string val = resolveKeyframeValueVars(*currentSheet, use);
                if (applyKeyframePropertyOverride(prop, val)) anyChange = true;
                continue;
            }
            std::string resolvedA = resolveKeyframeValueVars(*currentSheet, valA);
            std::string resolvedB = resolveKeyframeValueVars(*currentSheet, valB);
            std::string syntax;
            auto defIt = currentSheet->getPropertyDefinitions().find(prop);
            if (defIt != currentSheet->getPropertyDefinitions().end()) {
                syntax = defIt->second.syntax;
            } else {
                if (prop == "opacity" || prop == "scale") {
                    syntax = "<number>";
                } else if (prop == "color" || prop == "background-color" || prop == "background" || prop == "border-color") {
                    syntax = "<color>";
                } else if (prop == "width" || prop == "height" || prop == "top" || prop == "left" || prop == "right" || prop == "bottom" ||
                           prop == "margin" || prop == "margin-top" || prop == "margin-right" || prop == "margin-bottom" || prop == "margin-left" ||
                           prop == "padding" || prop == "padding-top" || prop == "padding-right" || prop == "padding-bottom" || prop == "padding-left" ||
                           prop == "font-size") {
                    syntax = "<length-percentage>";
                } else if (prop == "transform") {
                    syntax = "<transform-list>";
                } else if (prop == "transform-origin") {
                    syntax = "<transform-origin>";
                } else if (prop == "perspective-origin") {
                    syntax = "<perspective-origin>";
                } else if (prop == "perspective") {
                    syntax = "<length>";
                }
            }
            std::string interp = StyleSheet::interpolateTypedValue(resolvedA, resolvedB, easedT, syntax);
            if (applyKeyframePropertyOverride(prop, interp)) anyChange = true;
        }
    }

    if (anyChange) {
        if (auto* app = Application::instance()) {
            app->requestRedraw();
        }
    }

    if (styleDirty) {
        resolveStyles(*currentSheet);
    }
}

}

