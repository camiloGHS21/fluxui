// FluxUI Blink-style Decoupled Layout Object Tree Definitions
#pragma once
#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/layout.h"
#include <vector>
#include <memory>
#include <string>

namespace FluxUI {

    class Widget;
    class Renderer;

    // Base LayoutObject corresponding to blink::LayoutObject
    class LayoutObject {
    public:
        LayoutObject(Widget* node);
        virtual ~LayoutObject();

        virtual const char* getName() const = 0;
        
        Widget* node() const { return node_; }
        
        LayoutObject* parent() const { return parent_; }
        void setParent(LayoutObject* parent) { parent_ = parent; }

        const std::vector<LayoutObject*>& children() const { return children_; }
        void addChild(LayoutObject* child);
        void clearChildren() { children_.clear(); }

        virtual bool isBox() const { return false; }
        virtual bool isBlock() const { return false; }
        virtual bool isFlexibleBox() const { return false; }
        virtual bool isGrid() const { return false; }
        virtual bool isText() const { return false; }

        // Recursively resolves layout on this subtree
        virtual void layout(const LayoutConstraints& constraints) = 0;
        
        // Recursively renders this layout subtree
        virtual void paint(Renderer& renderer) = 0;
        
        // Recursively synchronizes layout object bounds from DOM widget bounds
        virtual void synchronizeBoundsFromNode();

    protected:
        Widget* node_ = nullptr;
        LayoutObject* parent_ = nullptr;
        std::vector<LayoutObject*> children_;
    };

    // LayoutBox corresponds to blink::LayoutBox (represents elements with sizing bounds)
    class LayoutBox : public LayoutObject {
    public:
        LayoutBox(Widget* node);
        
        bool isBox() const override { return true; }
        const char* getName() const override { return "LayoutBox"; }

        Rect bounds() const { return bounds_; }
        void setBounds(const Rect& r) { bounds_ = r; }

        void layout(const LayoutConstraints& constraints) override;
        void synchronizeBoundsFromNode() override;
        void paint(Renderer& renderer) override;

    protected:
        Rect bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    };

    // LayoutBlock corresponds to blink::LayoutBlock (block layout containers)
    class LayoutBlock : public LayoutBox {
    public:
        LayoutBlock(Widget* node);
        
        bool isBlock() const override { return true; }
        const char* getName() const override { return "LayoutBlock"; }

        void layout(const LayoutConstraints& constraints) override;
        void paint(Renderer& renderer) override;
    };

    // LayoutFlexibleBox corresponds to blink::LayoutFlexibleBox (flexbox containers)
    class LayoutFlexibleBox : public LayoutBlock {
    public:
        LayoutFlexibleBox(Widget* node);
        
        bool isFlexibleBox() const override { return true; }
        const char* getName() const override { return "LayoutFlexibleBox"; }

        void layout(const LayoutConstraints& constraints) override;
    };

    // LayoutGrid corresponds to blink::LayoutGrid (grid containers)
    class LayoutGrid : public LayoutBlock {
    public:
        LayoutGrid(Widget* node);
        
        bool isGrid() const override { return true; }
        const char* getName() const override { return "LayoutGrid"; }

        void layout(const LayoutConstraints& constraints) override;
    };

    // LayoutText corresponds to blink::LayoutText (anonymous text boxes)
    class LayoutText : public LayoutBox {
    public:
        LayoutText(Widget* node, const std::string& text);
        
        bool isText() const override { return true; }
        const char* getName() const override { return "LayoutText"; }

        const std::string& text() const { return text_; }
        void setText(const std::string& t) { text_ = t; }

        void layout(const LayoutConstraints& constraints) override;
        void paint(Renderer& renderer) override;

    private:
        std::string text_;
    };

} // namespace FluxUI
