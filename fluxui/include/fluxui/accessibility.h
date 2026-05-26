#pragma once
#include "fluxui/core.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace FluxUI {

    class Widget;
    class LayoutObject;

    // AXRole represents roles of nodes in the accessibility tree (matching blink::AXRole)
    enum class AXRole {
        kNone = 0,
        kButton,
        kCheckbox,
        kRadioButton,
        kSlider,
        kTextField,
        kStaticText,
        kListBox,
        kListBoxOption,
        kDialog,
        kGroup,
        kRootWebArea,
        kHr,
        kImage,
        kProgressBar,
        kHeading,
        kLink,
        kGenericContainer
    };

    // AXState represents accessibility states (matching blink::AXState / ax::mojom::State)
    enum class AXState : uint32_t {
        kNone = 0,
        kEnabled = 1 << 0,
        kFocused = 1 << 1,
        kFocusable = 1 << 2,
        kHovered = 1 << 3,
        kPressed = 1 << 4,
        kSelected = 1 << 5,
        kChecked = 1 << 6,
        kExpanded = 1 << 7
    };

    inline AXState operator|(AXState a, AXState b) {
        return static_cast<AXState>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline AXState operator&(AXState a, AXState b) {
        return static_cast<AXState>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    // AXObject represents a node in the Accessibility Tree
    class AXObject {
    public:
        AXObject(Widget* node);
        virtual ~AXObject();

        Widget* node() const { return node_; }
        LayoutObject* layoutObject() const;

        AXRole role() const { return role_; }
        std::string name() const { return name_; }
        std::string description() const { return description_; }
        std::string value() const { return value_; }
        Rect bounds() const { return bounds_; }
        uint32_t state() const { return state_; }

        bool hasState(AXState s) const { return (state_ & static_cast<uint32_t>(s)) != 0; }

        AXObject* parent() const { return parent_; }
        void setParent(AXObject* parent) { parent_ = parent; }

        const std::vector<AXObject*>& children() const { return children_; }
        void addChild(AXObject* child) { children_.push_back(child); }
        void clearChildren() { children_.clear(); }

        // Updates the attributes, name, state and bounds for the node
        void updateProperties();

        static const char* roleToString(AXRole role);

    private:
        Widget* node_ = nullptr;
        AXObject* parent_ = nullptr;
        std::vector<AXObject*> children_;

        AXRole role_ = AXRole::kNone;
        std::string name_;
        std::string description_;
        std::string value_;
        Rect bounds_{0.0f, 0.0f, 0.0f, 0.0f};
        uint32_t state_ = 0;
    };

    // AXObjectCache manages the lifecycle of AXObjects (matching blink::AXObjectCache)
    class AXObjectCache {
    public:
        AXObjectCache();
        ~AXObjectCache();

        AXObject* getOrCreate(Widget* widget);
        void remove(Widget* widget);
        void invalidate(Widget* widget);

        AXObject* root() const { return root_; }

        // Performs a complete layout tree traversal or synchronization to update bounds and hierarchy
        void update(Widget* rootWidget);

        // Debug printing or serialization
        std::string dumpTree(AXObject* node = nullptr, int indent = 0) const;

    private:
        Widget* rootWidget_ = nullptr;
        AXObject* root_ = nullptr;
        std::unordered_map<Widget*, std::unique_ptr<AXObject>> cache_;
        
        void buildTreeRecursive(Widget* widget, AXObject* parentAX);
    };

} // namespace FluxUI
