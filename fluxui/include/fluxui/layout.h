// FluxUI Blink-style Decoupled Layout Solver Definitions
#pragma once
#include <vector>
#include <string>
#include <memory>
#include "fluxui/core.h"

namespace FluxUI {

    class Widget;

    // Immutable representation of a physical fragment in LayoutNG parity
    struct PhysicalFragment {
        Rect bounds{0.0f, 0.0f, 0.0f, 0.0f};
        float contentHeight = 0.0f;
        std::vector<std::shared_ptr<const PhysicalFragment>> children;
    };

    // Computed output of a layout pass (Blink-style LayoutResult)
    struct LayoutResult {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float contentHeight = 0.0f;
        
        // LayoutNG Parity: Immutable physical fragment
        std::shared_ptr<const PhysicalFragment> physicalFragment;
        bool isCached = false;
    };

    // Decoupled Layout Constraints matching Blink's constraint spaces (ConstraintSpace)
    struct ConstraintSpace {
        float availableWidth = 0.0f;
        float availableHeight = 0.0f;
        float parentWidth = 1920.0f;
        float parentHeight = 1080.0f;
        float emBase = 16.0f;

        bool operator==(const ConstraintSpace& o) const {
            return availableWidth == o.availableWidth &&
                   availableHeight == o.availableHeight &&
                   parentWidth == o.parentWidth &&
                   parentHeight == o.parentHeight &&
                   emBase == o.emBase;
        }
        bool operator!=(const ConstraintSpace& o) const {
            return !(*this == o);
        }
    };
    using LayoutConstraints = ConstraintSpace;

    // Abstract base class for decoupled layout algorithms (Blink parity)
    class LayoutAlgorithm {
    public:
        virtual ~LayoutAlgorithm() = default;
        
        // Executes layout pass for a widget and its children, returning computed bounds
        virtual LayoutResult layout(Widget* widget, const LayoutConstraints& constraints) = 0;
    };

    // W3C-compliant Flexbox Solver (Parity with Blink's NGFlexLayoutAlgorithm)
    class FlexLayoutAlgorithm : public LayoutAlgorithm {
    public:
        LayoutResult layout(Widget* widget, const LayoutConstraints& constraints) override;
    };

    // Grid Layout Solver (Parity with Blink's NGGridLayoutAlgorithm)
    class GridLayoutAlgorithm : public LayoutAlgorithm {
    public:
        LayoutResult layout(Widget* widget, const LayoutConstraints& constraints) override;
    };

    // Table Layout Solver (Parity with Blink's NGTableLayoutAlgorithm)
    class TableLayoutAlgorithm : public LayoutAlgorithm {
    public:
        LayoutResult layout(Widget* widget, const LayoutConstraints& constraints) override;
    };

} // namespace FluxUI
