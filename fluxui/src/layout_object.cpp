// FluxUI Blink-style Decoupled Layout Object Tree Implementations
#include "fluxui/layout_object.h"
#include "fluxui/widgets.h"
#include <algorithm>

namespace FluxUI {

    LayoutObject::LayoutObject(Widget* node) : node_(node) {}
    LayoutObject::~LayoutObject() = default;

    void LayoutObject::addChild(LayoutObject* child) {
        if (!child) return;
        child->setParent(this);
        children_.push_back(child);
    }

    void LayoutObject::synchronizeBoundsFromNode() {
        for (auto* child : children_) {
            child->synchronizeBoundsFromNode();
        }
    }

    LayoutBox::LayoutBox(Widget* node) : LayoutObject(node) {}

    void LayoutBox::layout(const LayoutConstraints& constraints) {
        if (!node_) return;

        // If this is the root layout object (has no parent), we run node_->layout
        // which recursively calculates bounds for all descendants in the DOM tree.
        if (!parent_) {
            Rect parentBounds;
            parentBounds.x = bounds_.x;
            parentBounds.y = bounds_.y;
            parentBounds.w = constraints.availableWidth;
            parentBounds.h = constraints.availableHeight;

            node_->layout(parentBounds);
        }

        // Update layout object's bounds_ from node_'s bounds
        bounds_ = node_->bounds;

        // Now propagate layout/synchronization to children.
        // Note: For normal DOM children, they were already laid out by the recursive node_->layout call.
        // So we just synchronize their bounds and propagate.
        // For pseudo-elements, they are layout children but not in node_->children, so we must run node_->layout on them!
        for (auto* child : children_) {
            if (child->node()) {
                bool isPseudo = (child->node() == node_->beforePseudoNode.get() || 
                                 child->node() == node_->afterPseudoNode.get());
                if (isPseudo) {
                    LayoutConstraints pseudoConstraints;
                    pseudoConstraints.availableWidth = bounds_.w;
                    pseudoConstraints.availableHeight = bounds_.h;
                    pseudoConstraints.parentWidth = constraints.parentWidth;
                    pseudoConstraints.parentHeight = constraints.parentHeight;
                    pseudoConstraints.emBase = constraints.emBase;

                    Rect pseudoParentBounds;
                    pseudoParentBounds.x = bounds_.x;
                    pseudoParentBounds.y = bounds_.y;
                    pseudoParentBounds.w = bounds_.w;
                    pseudoParentBounds.h = bounds_.h;

                    child->node()->layout(pseudoParentBounds);
                    if (auto* childBox = dynamic_cast<LayoutBox*>(child)) {
                        childBox->setBounds(child->node()->bounds);
                    }
                    child->layout(pseudoConstraints);
                } else {
                    if (auto* childBox = dynamic_cast<LayoutBox*>(child)) {
                        childBox->setBounds(child->node()->bounds);
                    }
                    LayoutConstraints childConstraints = constraints;
                    childConstraints.availableWidth = child->node()->bounds.w;
                    childConstraints.availableHeight = child->node()->bounds.h;
                    child->layout(childConstraints);
                }
            }
        }
    }

    void LayoutBox::synchronizeBoundsFromNode() {
        if (node_) {
            bounds_ = node_->bounds;
        }
        LayoutObject::synchronizeBoundsFromNode();
    }

    void LayoutBox::paint(Renderer& renderer) {
        if (!node_ || !node_->visible) return;

        bool hasScale = (node_->renderScale != 1.0f);
        if (hasScale) {
            renderer.pushScale(node_->renderScale, bounds_.center());
        }

        // Temporarily disable DOM children painting so we only paint the element itself
        bool oldSkip = node_->skipDOMChildrenPaint;
        node_->skipDOMChildrenPaint = true;

        node_->render(renderer);

        node_->skipDOMChildrenPaint = oldSkip;

        // Paint layout tree children, handling scroll offset, clipping, and scrollbars
        bool scrollable = node_->isScrollableY();
        bool clip = node_->isClippingOverflow();

        if (clip) {
            renderer.pushScissor(bounds_);
        }
        if (scrollable) {
            renderer.pushTranslation({0.0f, -node_->scrollY});
        }

        // Paint all children recursively via the LayoutObject tree
        for (auto* child : children_) {
            if (child->node() && child->node()->computedStyle.position == Position::Fixed) {
                continue;
            }
            child->paint(renderer);
        }

        if (scrollable) {
            renderer.popTranslation();

            // Draw scrollbar
            Rect track, thumb;
            if (node_->getScrollBarRects(track, thumb)) {
                float active = (node_->scrollbarHovered || node_->scrollbarDragging) ? 1.0f : 0.0f;
                float pressed = node_->scrollbarDragging ? 1.0f : 0.0f;
                Rect visualTrack = {
                    track.x,
                    track.y,
                    track.w,
                    track.h
                };
                Rect visualThumb = thumb;
                if (!node_->scrollbarHovered && !node_->scrollbarDragging) {
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

        if (clip) {
            renderer.popScissor();
        }

        if (hasScale) {
            renderer.popScale();
        }
    }

    LayoutBlock::LayoutBlock(Widget* node) : LayoutBox(node) {}

    void LayoutBlock::layout(const LayoutConstraints& constraints) {
        LayoutBox::layout(constraints);
    }

    void LayoutBlock::paint(Renderer& renderer) {
        LayoutBox::paint(renderer);
    }

    LayoutFlexibleBox::LayoutFlexibleBox(Widget* node) : LayoutBlock(node) {}

    void LayoutFlexibleBox::layout(const LayoutConstraints& constraints) {
        LayoutBlock::layout(constraints);
    }

    LayoutGrid::LayoutGrid(Widget* node) : LayoutBlock(node) {}

    void LayoutGrid::layout(const LayoutConstraints& constraints) {
        LayoutBlock::layout(constraints);
    }

    LayoutText::LayoutText(Widget* node, const std::string& text) 
        : LayoutBox(node), text_(text) {}

    void LayoutText::layout(const LayoutConstraints& constraints) {
        if (!node_) return;

        Rect parentBounds;
        parentBounds.x = bounds_.x;
        parentBounds.y = bounds_.y;
        parentBounds.w = constraints.availableWidth;
        parentBounds.h = constraints.availableHeight;

        node_->layout(parentBounds);
        bounds_ = node_->bounds;
    }

    void LayoutText::paint(Renderer& renderer) {
        LayoutBox::paint(renderer);
    }

} // namespace FluxUI
