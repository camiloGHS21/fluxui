#include "fluxui/accessibility.h"
#include "fluxui/widgets.h"
#include "fluxui/layout_object.h"
#include <sstream>

namespace FluxUI {

    AXObject::AXObject(Widget* node) : node_(node) {
        updateProperties();
    }

    AXObject::~AXObject() = default;

    LayoutObject* AXObject::layoutObject() const {
        return node_ ? node_->layoutObject.get() : nullptr;
    }

    void AXObject::updateProperties() {
        if (!node_) return;

        // 1. Role determination
        if (node_->parent == nullptr) {
            role_ = AXRole::kRootWebArea;
        } else if (node_->type == "button") {
            role_ = AXRole::kButton;
        } else if (node_->type == "checkbox") {
            role_ = AXRole::kCheckbox;
        } else if (node_->type == "radio") {
            role_ = AXRole::kRadioButton;
        } else if (node_->type == "input" || node_->type == "textarea") {
            role_ = AXRole::kTextField;
        } else if (node_->type == "text") {
            role_ = AXRole::kStaticText;
        } else if (node_->type == "select") {
            role_ = AXRole::kListBox;
        } else if (node_->type == "option") {
            role_ = AXRole::kListBoxOption;
        } else if (node_->type == "dialog") {
            role_ = AXRole::kDialog;
        } else if (node_->type == "panel") {
            role_ = AXRole::kGroup;
        } else if (node_->type == "hr") {
            role_ = AXRole::kHr;
        } else if (node_->type == "image") {
            role_ = AXRole::kImage;
        } else if (node_->type == "progress") {
            role_ = AXRole::kProgressBar;
        } else {
            role_ = AXRole::kGenericContainer;
        }

        // 2. States determination
        state_ = static_cast<uint32_t>(AXState::kEnabled); // Default enabled

        if (node_->focused) {
            state_ |= static_cast<uint32_t>(AXState::kFocused);
        }
        if (node_->hovered) {
            state_ |= static_cast<uint32_t>(AXState::kHovered);
        }
        if (node_->pressed) {
            state_ |= static_cast<uint32_t>(AXState::kPressed);
        }

        // Focusable controls
        if (role_ == AXRole::kButton || role_ == AXRole::kCheckbox ||
            role_ == AXRole::kRadioButton || role_ == AXRole::kTextField ||
            role_ == AXRole::kListBox || role_ == AXRole::kListBoxOption) {
            state_ |= static_cast<uint32_t>(AXState::kFocusable);
        }

        // 3. Name, Value, and Description
        name_ = "";
        value_ = "";
        description_ = "";

        if (role_ == AXRole::kButton) {
            auto* btn = dynamic_cast<Button*>(node_);
            if (btn) {
                name_ = btn->label;
            }
        } else if (role_ == AXRole::kStaticText) {
            auto* txt = dynamic_cast<Text*>(node_);
            if (txt) {
                name_ = txt->content;
            }
        } else if (role_ == AXRole::kTextField) {
            auto* input = dynamic_cast<TextInput*>(node_);
            if (input) {
                value_ = input->value;
                description_ = input->placeholder;
            }
        } else if (role_ == AXRole::kCheckbox) {
            auto* cb = dynamic_cast<Checkbox*>(node_);
            if (cb) {
                if (cb->checked) {
                    state_ |= static_cast<uint32_t>(AXState::kChecked);
                    value_ = "true";
                } else {
                    value_ = "false";
                }
            }
        } else if (role_ == AXRole::kRadioButton) {
            auto* rb = dynamic_cast<Radio*>(node_);
            if (rb) {
                if (rb->checked) {
                    state_ |= static_cast<uint32_t>(AXState::kChecked);
                    value_ = "true";
                } else {
                    value_ = "false";
                }
                description_ = rb->group;
            }
        } else if (role_ == AXRole::kListBoxOption) {
            auto* opt = dynamic_cast<Option*>(node_);
            if (opt) {
                name_ = opt->label;
                value_ = opt->value;
            }
        } else if (role_ == AXRole::kListBox) {
            auto* sel = dynamic_cast<Select*>(node_);
            if (sel) {
                value_ = sel->selectedValue();
                if (sel->expanded) {
                    state_ |= static_cast<uint32_t>(AXState::kExpanded);
                }
            }
        } else if (role_ == AXRole::kProgressBar) {
            auto* pb = dynamic_cast<ProgressBar*>(node_);
            if (pb) {
                value_ = std::to_string(pb->progress);
            }
        } else if (role_ == AXRole::kImage) {
            auto* img = dynamic_cast<Image*>(node_);
            if (img) {
                name_ = img->alt;
                description_ = img->source;
            }
        }

        // Fallback for custom accessibility names (ID/ClassName)
        if (name_.empty()) {
            if (!node_->id.empty()) {
                name_ = node_->id.getString();
            } else if (!node_->className.empty()) {
                name_ = node_->className.getString();
            }
        }

        // 4. Bounds computation (relative to viewport)
        bounds_ = node_->bounds;
    }

    const char* AXObject::roleToString(AXRole role) {
        switch (role) {
            case AXRole::kButton: return "Button";
            case AXRole::kCheckbox: return "Checkbox";
            case AXRole::kRadioButton: return "RadioButton";
            case AXRole::kSlider: return "Slider";
            case AXRole::kTextField: return "TextField";
            case AXRole::kStaticText: return "StaticText";
            case AXRole::kListBox: return "ListBox";
            case AXRole::kListBoxOption: return "ListBoxOption";
            case AXRole::kDialog: return "Dialog";
            case AXRole::kGroup: return "Group";
            case AXRole::kRootWebArea: return "RootWebArea";
            case AXRole::kHr: return "HorizontalRule";
            case AXRole::kImage: return "Image";
            case AXRole::kProgressBar: return "ProgressBar";
            case AXRole::kHeading: return "Heading";
            case AXRole::kLink: return "Link";
            case AXRole::kGenericContainer: return "GenericContainer";
            default: return "None";
        }
    }

    AXObjectCache::AXObjectCache() = default;
    AXObjectCache::~AXObjectCache() = default;

    AXObject* AXObjectCache::getOrCreate(Widget* widget) {
        if (!widget) return nullptr;
        auto it = cache_.find(widget);
        if (it != cache_.end()) {
            return it->second.get();
        }
        auto obj = std::make_unique<AXObject>(widget);
        AXObject* ptr = obj.get();
        cache_[widget] = std::move(obj);
        return ptr;
    }

    void AXObjectCache::remove(Widget* widget) {
        if (!widget) return;
        cache_.erase(widget);
    }

    void AXObjectCache::invalidate(Widget* widget) {
        if (!widget) return;
        auto* axObj = getOrCreate(widget);
        if (axObj) {
            axObj->updateProperties();
        }
    }

    void AXObjectCache::update(Widget* rootWidget) {
        rootWidget_ = rootWidget;
        if (!rootWidget) {
            root_ = nullptr;
            cache_.clear();
            return;
        }

        // Rebuild AX tree hierarchy starting from root
        root_ = getOrCreate(rootWidget);
        root_->clearChildren();
        root_->setParent(nullptr);

        buildTreeRecursive(rootWidget, root_);
    }

    void AXObjectCache::buildTreeRecursive(Widget* widget, AXObject* parentAX) {
        if (!widget || !parentAX) return;

        for (auto& child : widget->children) {
            if (!child || !child->visible) continue;

            AXObject* childAX = getOrCreate(child.get());
            childAX->updateProperties();
            childAX->clearChildren();
            childAX->setParent(parentAX);

            parentAX->addChild(childAX);

            buildTreeRecursive(child.get(), childAX);
        }
    }

    std::string AXObjectCache::dumpTree(AXObject* node, int indent) const {
        if (!node) {
            node = root_;
        }
        if (!node) return "";

        std::ostringstream ss;
        for (int i = 0; i < indent; ++i) ss << "  ";

        ss << AXObject::roleToString(node->role());
        if (!node->name().empty()) {
            ss << " name=\"" << node->name() << "\"";
        }
        if (!node->value().empty()) {
            ss << " value=\"" << node->value() << "\"";
        }
        
        // Print active states
        ss << " states=[";
        bool first = true;
        auto addState = [&](AXState s, const char* name) {
            if (node->hasState(s)) {
                if (!first) ss << ", ";
                ss << name;
                first = false;
            }
        };
        addState(AXState::kEnabled, "enabled");
        addState(AXState::kFocused, "focused");
        addState(AXState::kFocusable, "focusable");
        addState(AXState::kHovered, "hovered");
        addState(AXState::kPressed, "pressed");
        addState(AXState::kSelected, "selected");
        addState(AXState::kChecked, "checked");
        addState(AXState::kExpanded, "expanded");
        ss << "]";

        ss << " bounds={" << node->bounds().x << ", " << node->bounds().y 
           << ", " << node->bounds().w << ", " << node->bounds().h << "}\n";

        for (auto* child : node->children()) {
            ss << dumpTree(child, indent + 1);
        }
        return ss.str();
    }

} // namespace FluxUI
