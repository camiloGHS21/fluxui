// FluxUI Blink-style Decoupled Layout Object Tree Implementations
#include "fluxui/layout_object.h"
#include "fluxui/widgets.h"
#include <algorithm>
#include <glad/gl.h>
#include <iostream>

namespace FluxUI {



    LayoutObject::LayoutObject(Widget* node) : node_(node) {}
    LayoutObject::~LayoutObject() = default;

    void LayoutObject::markPaintDirty() {
        paintDirty_ = true;
        // Walk up to find nearest GPU-composited ancestor
        LayoutObject* ancestor = parent_;
        while (ancestor) {
            if (ancestor->node() && ancestor->node()->useGPUCompositing && ancestor->getPaintLayer()) {
                ancestor->getPaintLayer()->markDirty();
                break;
            }
            ancestor = ancestor->parent();
        }
    }

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

        // LayoutNG Cache check: $O(1)$ short-circuit if ConstraintSpace matches!
        if (cache_.isValid && cache_.space == constraints) {
            bounds_ = cache_.result.physicalFragment->bounds;
            applyPhysicalFragment(*cache_.result.physicalFragment);
            return;
        }

        markPaintDirty();

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

        // Cache the newly resolved immutable layout result and physical fragment
        LayoutResult result;
        result.x = bounds_.x;
        result.y = bounds_.y;
        result.width = bounds_.w;
        result.height = bounds_.h;
        result.contentHeight = node_->contentHeight;
        result.physicalFragment = createPhysicalFragment();
        result.isCached = false;

        cache_.space = constraints;
        cache_.result = result;
        cache_.isValid = true;
    }

    void LayoutBox::synchronizeBoundsFromNode() {
        if (node_) {
            bounds_ = node_->bounds;
        }
        LayoutObject::synchronizeBoundsFromNode();
    }

    void LayoutBox::applyPhysicalFragment(const PhysicalFragment& fragment) {
        bounds_ = fragment.bounds;
        if (node_) {
            node_->bounds = fragment.bounds;
            node_->contentHeight = fragment.contentHeight;
            node_->layoutDirty = false;
            node_->lifecycleState = WidgetLifecycle::LayoutClean;
        }
        
        size_t childIdx = 0;
        for (auto* child : children_) {
            if (auto* childBox = dynamic_cast<LayoutBox*>(child)) {
                if (childIdx < fragment.children.size()) {
                    childBox->applyPhysicalFragment(*fragment.children[childIdx]);
                    childIdx++;
                }
            }
        }
    }

    std::shared_ptr<const PhysicalFragment> LayoutBox::createPhysicalFragment() const {
        auto fragment = std::make_shared<PhysicalFragment>();
        fragment->bounds = bounds_;
        if (node_) {
            fragment->contentHeight = node_->contentHeight;
        }
        for (const auto* child : children_) {
            if (const auto* childBox = dynamic_cast<const LayoutBox*>(child)) {
                fragment->children.push_back(childBox->createPhysicalFragment());
            }
        }
        return fragment;
    }

    void LayoutBox::paint(Renderer& renderer) {
        if (!node_ || !node_->visible) return;

        if (node_->computedStyle->contain & kContainPaint) {
            float vpW = 1920.0f;
            float vpH = 1080.0f;
            const Widget* r = node_;
            while (r->parent) r = r->parent;
            if (r) {
                vpW = r->bounds.w;
                vpH = r->bounds.h;
            }
            if (bounds_.x + bounds_.w < 0 || bounds_.x > vpW || bounds_.y + bounds_.h < 0 || bounds_.y > vpH) {
                return;
            }
        }


        if (node_->useGPUCompositing) {
            int w = std::max(1, (int)std::ceil(bounds_.w));
            int h = std::max(1, (int)std::ceil(bounds_.h));
            if (!paintLayer_) {
                paintLayer_ = std::make_unique<GPUPaintLayer>();
            }
            paintLayer_->resize(w, h);

            if (paintLayer_->isDirty() || isPaintDirty()) {
                // Render into FBO target
                renderer.pushRenderTarget(paintLayer_->fbo(), w, h);

                // Save parent translation by pushing a translation that offsets it to 0
                Vec2 parentTranslation = renderer.getTranslation();
                renderer.pushTranslation({-parentTranslation.x, -parentTranslation.y});

                // Push translation so (bounds_.x, bounds_.y) maps to (0, 0) in the FBO
                renderer.pushTranslation({-bounds_.x, -bounds_.y});

                // Clear the FBO surface
                if (glad_glClearColor != nullptr) {
                    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                }

                // Paint the actual content, bypassing the compositing check for this node
                node_->useGPUCompositing = false;
                paintInternal(renderer);
                node_->useGPUCompositing = true;

                renderer.popTranslation(); // Pop FBO translation
                renderer.popTranslation(); // Pop parent translation offset
                renderer.popRenderTarget();

                paintLayer_->markClean();
            }

            // Draw FBO texture at widget bounds
            renderer.drawTexture(paintLayer_->textureId(), bounds_, node_->computedStyle->opacity);
            return;
        }

        paintInternal(renderer);
    }

    void LayoutBox::paintInternal(Renderer& renderer) {
        bool hasScale = (node_->renderScale != 1.0f);
        if (hasScale) {
            renderer.pushScale(node_->renderScale, bounds_.center());
        }

        if (isPaintDirty()) {
            cachedCommands_.clear();
            cachedForegroundCommands_.clear();

            renderer.activeTransformNodeId = node_->transformNodeId;
            renderer.activeClipNodeId = node_->clipNodeId;
            renderer.activeEffectNodeId = node_->effectNodeId;

            // Record background and node content
            renderer.startRecording(cachedCommands_);
            
            bool oldSkip = node_->skipDOMChildrenPaint;
            node_->skipDOMChildrenPaint = true;
            node_->render(renderer);
            node_->skipDOMChildrenPaint = oldSkip;

            renderer.stopRecording();

            // Record foreground/scrollbars
            renderer.startRecording(cachedForegroundCommands_);
            bool scrollable = node_->isScrollableY();
            if (scrollable) {
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
            renderer.stopRecording();

            setPaintClean();
        }

        // Play back background
        renderer.playback(cachedCommands_);

        // Paint layout tree children, handling scroll offset, clipping, and scrollbars
        bool scrollable = node_->isScrollableY();
        bool clip = node_->isClippingOverflow();

        if (clip) {
            renderer.pushScissor(bounds_);
        }
        if (scrollable) {
            renderer.pushTranslation({0.0f, -node_->scrollY});
        }

        // Paint normal children first, then expanded native selects so their menu
        // appears above following siblings like a browser control popup.
        for (auto* child : children_) {
            if (child->node() && child->node()->computedStyle->position == Position::Fixed) {
                continue;
            }
            if (auto* select = child->node() ? dynamic_cast<Select*>(child->node()) : nullptr) {
                if (select->expanded) continue;
            }
            child->paint(renderer);
        }
        for (auto* child : children_) {
            if (child->node() && child->node()->computedStyle->position == Position::Fixed) {
                continue;
            }
            auto* select = child->node() ? dynamic_cast<Select*>(child->node()) : nullptr;
            if (!select || !select->expanded) continue;
            child->paint(renderer);
        }

        if (scrollable) {
            renderer.popTranslation();
        }

        if (clip) {
            renderer.popScissor();
        }

        // Play back foreground (scrollbars)
        renderer.playback(cachedForegroundCommands_);

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
        if (!node_) return;

        // LayoutNG Cache check: $O(1)$ short-circuit if ConstraintSpace matches!
        if (cache_.isValid && cache_.space == constraints) {
            bounds_ = cache_.result.physicalFragment->bounds;
            applyPhysicalFragment(*cache_.result.physicalFragment);
            return;
        }

        // Run W3C-compliant Flexbox Solver (Parity with Blink's NGFlexLayoutAlgorithm)
        FlexLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(node_, constraints);

        bounds_.w = res.width;
        bounds_.h = res.height;
        bounds_.x = node_->parent ? (res.x != 0.0f ? res.x : node_->bounds.x) : 0.0f;
        bounds_.y = node_->parent ? (res.y != 0.0f ? res.y : node_->bounds.y) : 0.0f;

        node_->bounds = bounds_;
        node_->contentHeight = res.contentHeight;
        node_->layoutDirty = false;
        node_->lifecycleState = WidgetLifecycle::LayoutClean;

        // Delegate child propagation, physical fragment creation, and caching to base LayoutBlock
        LayoutBlock::layout(constraints);
    }

    LayoutGrid::LayoutGrid(Widget* node) : LayoutBlock(node) {}

    void LayoutGrid::layout(const LayoutConstraints& constraints) {
        if (!node_) return;

        // LayoutNG Cache check: $O(1)$ short-circuit if ConstraintSpace matches!
        if (cache_.isValid && cache_.space == constraints) {
            bounds_ = cache_.result.physicalFragment->bounds;
            applyPhysicalFragment(*cache_.result.physicalFragment);
            return;
        }

        // Run Grid Layout Solver (Parity with Blink's NGGridLayoutAlgorithm)
        GridLayoutAlgorithm algorithm;
        LayoutResult res = algorithm.layout(node_, constraints);

        bounds_.w = res.width;
        bounds_.h = res.height;
        bounds_.x = node_->parent ? (res.x != 0.0f ? res.x : node_->bounds.x) : 0.0f;
        bounds_.y = node_->parent ? (res.y != 0.0f ? res.y : node_->bounds.y) : 0.0f;

        node_->bounds = bounds_;
        node_->contentHeight = res.contentHeight;
        node_->layoutDirty = false;
        node_->lifecycleState = WidgetLifecycle::LayoutClean;

        // Delegate child propagation, physical fragment creation, and caching to base LayoutBlock
        LayoutBlock::layout(constraints);
    }

    LayoutText::LayoutText(Widget* node, const std::string& text) 
        : LayoutBox(node), text_(text) {}

    void LayoutText::layout(const LayoutConstraints& constraints) {
        if (!node_) return;

        (void)constraints;
        bounds_ = node_->bounds;
    }

    void LayoutText::paint(Renderer& renderer) {
        LayoutBox::paint(renderer);
    }

} // namespace FluxUI
