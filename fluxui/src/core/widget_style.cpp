// FluxUI - Widget style invalidation: dirty-flag propagation and class/id
// change invalidation (the cheap, structural half of the style pipeline).
// Extracted from core/application.cpp.
#include "fluxui/widgets.h"
#include "widget_internal.h"
#include "fluxui/css_parser.h"
#include "fluxui/layout_object.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace FluxUI {
using namespace FluxUI::detail;

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
void Widget::dispatchInputEvent() {
    Event ev;
    ev.type = "input";
    ev.target = this;
    ev.bubbles = true;
    ev.cancelable = false;
    dispatchEvent(ev);
}
void Widget::dispatchChangeEvent() {
    Event ev;
    ev.type = "change";
    ev.target = this;
    ev.bubbles = true;
    ev.cancelable = false;
    dispatchEvent(ev);
}
void Widget::checkFormControlChanges() {
    if (isFormControl()) {
        std::string current = formControlValue();
        if (!hasValueSnapshot_) {
            // First observation: seed snapshots without firing (initial value).
            lastDispatchedValue_ = current;
            valueAtFocus_ = current;
            hasValueSnapshot_ = true;
        } else {
            // `input`: fires on every value change (HTML input event).
            if (current != lastDispatchedValue_) {
                lastDispatchedValue_ = current;
                dispatchInputEvent();
                // Instant-commit controls (checkbox/radio/select/range) also
                // fire `change` immediately on each value change.
                if (commitsValueOnInput()) {
                    valueAtFocus_ = current;
                    dispatchChangeEvent();
                }
            }
            // `change` for text-entry controls: capture the baseline when focus
            // is gained; when focus is lost, fire if the value differs.
            if (focused && !lastFrameFocused) {
                valueAtFocus_ = current;
            }
            if (!focused && lastFrameFocused && current != valueAtFocus_) {
                valueAtFocus_ = current;
                dispatchChangeEvent();
            }
        }
    }
    for (auto& child : children) {
        if (child) {
            child->checkFormControlChanges();
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

} // namespace FluxUI