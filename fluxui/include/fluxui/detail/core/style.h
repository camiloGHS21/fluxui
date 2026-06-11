#pragma once
// FluxUI public API - Style + ComputedStyle (depends on all detail headers).
// Auto-split from core.h; do not include directly, use <fluxui/core.h>.
#include "fluxui/config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "fluxui/detail/core/geometry.h"
#include "fluxui/detail/core/css_enums.h"
#include "fluxui/detail/core/css_value.h"
#include "fluxui/detail/core/transform.h"
#include "fluxui/detail/core/data_ref.h"
namespace FluxUI {
// ── StyleRareData — cold (rarely-set) non-inherited properties ───────────────
// Held by Style behind a DataRef so copying a Style shares this group by pointer
// and only clones it on first write (copy-on-write). Mirrors Blink's
// StyleRareNonInheritedData. Read via style.rare(), write via the non-const
// overload (which triggers COW through DataRef::access()).
struct StyleRareData {
    // ── Masking, Clipping, Blending (CSS Masking L1 / Compositing L1) ──
    std::string clipPath;                     // clip-path
    bool hasClipPath = false;
    std::string shapeOutside;                 // shape-outside
    bool hasShapeOutside = false;
    std::string maskImage;
    std::string maskMode;
    std::string maskRepeat;
    std::string maskPosition;
    std::string maskSize;
    std::string maskClip;
    std::string maskOrigin;
    std::string maskComposite;
    bool hasMask = false;
    BlendMode mixBlendMode = BlendMode::Normal;
    bool hasMixBlendMode = false;
    Isolation isolation = Isolation::Auto;
    bool hasIsolation = false;
    BlendMode backgroundBlendMode = BlendMode::Normal;
    bool hasBackgroundBlendMode = false;

    // ── Scroll API (CSS Scroll Snap L1, Overscroll Behavior L1, Scrollbars L1) ──
    std::string scrollSnapType;
    bool hasScrollSnapType = false;
    std::string scrollSnapAlign;
    bool hasScrollSnapAlign = false;
    std::string scrollSnapStop;
    EdgeInsets scrollPadding;
    EdgeInsets scrollMargin;
    bool hasScrollPadding = false;
    bool hasScrollMargin = false;
    OverscrollBehavior overscrollBehaviorX = OverscrollBehavior::Auto;
    OverscrollBehavior overscrollBehaviorY = OverscrollBehavior::Auto;
    bool hasOverscrollBehavior = false;
    Color scrollbarThumbColor = Color(0,0,0,0);
    Color scrollbarTrackColor = Color(0,0,0,0);
    bool hasScrollbarColor = false;
    ScrollbarWidth scrollbarWidth = ScrollbarWidth::Auto;
    bool hasScrollbarWidth = false;
    OverflowAnchor overflowAnchor = OverflowAnchor::Auto;
    bool hasOverflowAnchor = false;
    std::string scrollbarGutter;
    bool hasScrollbarGutter = false;
    ScrollBehavior scrollBehavior = ScrollBehavior::Auto;
    bool hasScrollBehavior = false;

    // ── Advanced Typography (CSS Fonts L4, CSS Text L4) ──
    std::string fontVariantCaps;
    std::string fontVariantNumeric;
    std::string fontVariantLigatures;
    std::string fontVariantEastAsian;
    std::string fontVariantPosition;
    std::string fontVariantAlternates;
    std::string fontFeatureSettings;
    std::string fontVariationSettings;
    std::string fontOpticalSizing;
    std::string fontPalette;
    std::string fontStretch;
    float tabSize = 8.0f;
    bool hasTabSize = false;
    std::string hyphens;
    bool hasHyphens = false;
    std::string lineBreak;
    bool hasLineBreak = false;
    std::string overflowWrap;
    bool hasOverflowWrap = false;
    std::string textJustify;
    bool hasTextJustify = false;
    float textIndent = 0.0f;
    bool hasTextIndent = false;
    std::string hangingPunctuation;
    bool hasHangingPunctuation = false;
    std::string fontSynthesis;
    std::string fontLanguageOverride;
    bool hasFontVariantCaps = false;
    bool hasFontVariantNumeric = false;
    bool hasFontVariantLigatures = false;
    bool hasFontVariantEastAsian = false;
    bool hasFontFeatureSettings = false;
    bool hasFontVariationSettings = false;
    bool hasFontOpticalSizing = false;
    bool hasFontStretch = false;

    // ── UI Misc Properties (CSS UI L4, CSS Color Adjust L1) ──
    Color accentColor = Color(0, 0, 0, 0);
    bool hasAccentColor = false;
    Color caretColor = Color(0, 0, 0, 0);
    bool hasCaretColor = false;
    std::string colorScheme;
    bool hasColorScheme = false;
    bool inert = false;
    std::string fieldSizing;
    bool hasFieldSizing = false;
    std::string imageRendering;
    bool hasImageRendering = false;
    std::string imageOrientation;
    bool hasImageOrientation = false;
    std::string objectViewBox;
    bool hasObjectViewBox = false;
    std::string touchAction;
    bool hasTouchAction = false;
    std::string userSelect;
    bool hasUserSelect = false;
    std::string willChange;
    bool hasWillChange = false;
    std::string containIntrinsicSize;
    bool hasContainIntrinsicSize = false;
    std::string contentVisibility;
    bool hasContentVisibility = false;

    // ── Animations (CSS Animations L1) ──
    std::vector<std::string> animationName;
    std::vector<float> animationDuration;
    std::vector<float> animationDelay;
    std::vector<float> animationIterationCount;
    std::vector<AnimationDirection> animationDirection;
    std::vector<AnimationFillMode> animationFillMode;
    std::vector<AnimationPlayState> animationPlayState;
    std::vector<TimingFunction> animationTimingFunction;
    std::vector<AnimationComposition> animationComposition;

    // ── Scroll-driven Animations (CSS Scroll-driven Animations L1) ──
    std::vector<std::string> animationTimeline;
    std::string animationRangeStart;
    std::string animationRangeEnd;
    std::vector<std::string> scrollTimelineName;
    std::vector<std::string> scrollTimelineAxis;
    std::vector<std::string> viewTimelineName;
    std::vector<std::string> viewTimelineAxis;
    std::string viewTimelineInset;
    std::vector<std::string> timelineScope;
    bool hasAnimationTimeline = false;
    bool hasScrollTimeline    = false;
    bool hasViewTimeline      = false;
    bool hasTimelineScope     = false;

    // ── Transitions (CSS Transitions L1) ──
    std::vector<std::string> transitionProperty;
    std::vector<float> transitionDurations;
    std::vector<float> transitionDelays;
    std::vector<TimingFunction> transitionTimingFunctions;
    std::vector<TransitionBehavior> transitionBehavior;
};
struct Style {
    Display display = Display::Block;
    Position position = Position::Static;
    FlexDirection flexDirection = FlexDirection::Column;
    FlexWrap flexWrap = FlexWrap::NoWrap;
    JustifyContent justifyContent = JustifyContent::FlexStart;
    AlignItems alignItems = AlignItems::Stretch;
    AlignContent alignContent = AlignContent::Stretch;
    AlignSelf alignSelf = AlignSelf::Auto;
    float flexGrow = 0;
    float flexShrink = 1;
    CSSValue flexBasis;
    int order = 0;
    float gap = 0;
    float rowGap = 0;
    float columnGap = 0;
    int columnCount = 0;
    float columnWidth = 0.0f;
    Overflow overflow = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;
    Overflow overflowY = Overflow::Visible;
    BoxSizing boxSizing = BoxSizing::ContentBox;
    bool hasBoxSizing = false;
    Visibility visibility = Visibility::Visible;
    PointerEvents pointerEvents = PointerEvents::Auto;
    int zIndex = 0;
    bool hasZIndex = false;
    CSSFloat cssFloat = CSSFloat::None;
    CSSClear cssClear = CSSClear::None;
    // ── CSS Grid Layout (Blink NGGridLayoutAlgorithm parity) ──────────────
    // Parsed track lists — replaces the old raw std::string fields.
    std::vector<GridTrackSize> gridTemplateColumnTracks; // grid-template-columns
    std::vector<GridTrackSize> gridTemplateRowTracks;    // grid-template-rows
    // Raw strings kept for subgrid / complex values that need further resolution
    std::string gridTemplateColumns;
    std::string gridTemplateRows;
    // Item placement (grid-column / grid-row shorthand → start/end)
    GridPlacement gridColumnStart;
    GridPlacement gridColumnEnd;
    GridPlacement gridRowStart;
    GridPlacement gridRowEnd;
    std::string gridColumn;  // raw shorthand (kept for cascade)
    std::string gridRow;
    // grid-area shorthand → fills all four GridPlacement fields above
    std::string gridArea;
    // Template areas
    GridTemplateAreas gridTemplateAreas;
    bool hasGridTemplateAreas = false;
    // Implicit track sizing (grid-auto-rows / grid-auto-columns)
    std::vector<GridTrackSize> gridAutoRowTracks;
    std::vector<GridTrackSize> gridAutoColumnTracks;
    // grid-auto-flow
    GridAutoFlow gridAutoFlow = GridAutoFlow::Row;
    // Per-item alignment
    JustifyItems justifyItems = JustifyItems::Normal;
    JustifySelf  justifySelf  = JustifySelf::Auto;
    bool hasJustifyItems = false;
    bool hasJustifySelf  = false;
    std::string content;
    float aspectRatio = 0;
    ObjectFit objectFit = ObjectFit::Fill;
    bool hasObjectFit = false;
    Appearance appearance = Appearance::Auto;
    bool hasAppearance = false;
    Vec2 objectPosition = {0.5f, 0.5f};
    Vec2 objectPositionOffset = {0.0f, 0.0f};
    bool hasObjectPosition = false;
    ContainmentFlags contain = kContainNone;
    CSSValue width, height;
    CSSValue minWidth, minHeight;
    CSSValue maxWidth, maxHeight;
    EdgeInsets padding;
    EdgeInsets margin;
    Direction direction = Direction::Ltr;
    bool hasDirection = false;
    UnicodeBidi unicodeBidi = UnicodeBidi::Normal;
    bool hasUnicodeBidi = false;
    WritingMode writingMode = WritingMode::HorizontalTb;
    bool hasWritingMode = false;
    float marginInlineStart = 0.0f, marginInlineEnd = 0.0f;
    float marginBlockStart = 0.0f, marginBlockEnd = 0.0f;
    bool hasMarginInlineStart = false, hasMarginInlineEnd = false;
    bool hasMarginBlockStart = false, hasMarginBlockEnd = false;
    float paddingInlineStart = 0.0f, paddingInlineEnd = 0.0f;
    float paddingBlockStart = 0.0f, paddingBlockEnd = 0.0f;
    bool hasPaddingInlineStart = false, hasPaddingInlineEnd = false;
    bool hasPaddingBlockStart = false, hasPaddingBlockEnd = false;
    CSSValue insetInlineStart, insetInlineEnd;
    CSSValue insetBlockStart, insetBlockEnd;
    bool hasInsetInlineStart = false, hasInsetInlineEnd = false;
    bool hasInsetBlockStart = false, hasInsetBlockEnd = false;
    Border borderInlineStart, borderInlineEnd;
    Border borderBlockStart, borderBlockEnd;
    bool hasBorderInlineStart = false, hasBorderInlineEnd = false;
    bool hasBorderBlockStart = false, hasBorderBlockEnd = false;
    CSSValue inlineSize, blockSize;
    CSSValue minInlineSize, minBlockSize;
    CSSValue maxInlineSize, maxBlockSize;
    bool hasInlineSize = false, hasBlockSize = false;
    bool hasMinInlineSize = false, hasMinBlockSize = false;
    bool hasMaxInlineSize = false, hasMaxBlockSize = false;
    float marginT = 0.0f, marginR = 0.0f, marginB = 0.0f, marginL = 0.0f;
    float paddingT = 0.0f, paddingR = 0.0f, paddingB = 0.0f, paddingL = 0.0f;
    bool hasMarginT = false, hasMarginR = false, hasMarginB = false, hasMarginL = false;
    bool hasPaddingT = false, hasPaddingR = false, hasPaddingB = false, hasPaddingL = false;
    CSSValue topVal, rightVal, bottomVal, leftVal;
    bool hasTopVal = false, hasRightVal = false, hasBottomVal = false, hasLeftVal = false;
    Border borderT, borderR, borderB, borderL;
    bool hasBorderT = false, hasBorderR = false, hasBorderB = false, hasBorderL = false;
    bool hasWidthVal = false, hasHeightVal = false;
    bool hasMinWidthVal = false, hasMinHeightVal = false;
    bool hasMaxWidthVal = false, hasMaxHeightVal = false;
    uint32_t propertyOrder = 0;
    uint32_t orderMarginTop = 0, orderMarginRight = 0, orderMarginBottom = 0, orderMarginLeft = 0;
    uint32_t orderMarginBlockStart = 0, orderMarginBlockEnd = 0, orderMarginInlineStart = 0, orderMarginInlineEnd = 0;
    uint32_t orderPaddingTop = 0, orderPaddingRight = 0, orderPaddingBottom = 0, orderPaddingLeft = 0;
    uint32_t orderPaddingBlockStart = 0, orderPaddingBlockEnd = 0, orderPaddingInlineStart = 0, orderPaddingInlineEnd = 0;
    uint32_t orderBorderTop = 0, orderBorderRight = 0, orderBorderBottom = 0, orderBorderLeft = 0;
    uint32_t orderBorderBlockStart = 0, orderBorderBlockEnd = 0, orderBorderInlineStart = 0, orderBorderInlineEnd = 0;
    uint32_t orderTop = 0, orderRight = 0, orderBottom = 0, orderLeft = 0;
    uint32_t orderInsetBlockStart = 0, orderInsetBlockEnd = 0, orderInsetInlineStart = 0, orderInsetInlineEnd = 0;
    uint32_t orderWidth = 0, orderHeight = 0;
    uint32_t orderMinWidth = 0, orderMinHeight = 0;
    uint32_t orderMaxWidth = 0, orderMaxHeight = 0;
    uint32_t orderInlineSize = 0, orderBlockSize = 0;
    uint32_t orderMinInlineSize = 0, orderMinBlockSize = 0;
    uint32_t orderMaxInlineSize = 0, orderMaxBlockSize = 0;
    enum PhysicalSide { PhysTop, PhysRight, PhysBottom, PhysLeft };
    enum LogicalSide { LogBlockStart, LogBlockEnd, LogInlineStart, LogInlineEnd };
    PhysicalSide mapLogicalSide(LogicalSide log) const {
        if (writingMode == WritingMode::HorizontalTb) {
            switch(log) {
                case LogBlockStart: return PhysTop;
                case LogBlockEnd: return PhysBottom;
                case LogInlineStart: return (direction == Direction::Ltr) ? PhysLeft : PhysRight;
                case LogInlineEnd: return (direction == Direction::Ltr) ? PhysRight : PhysLeft;
            }
        } else if (writingMode == WritingMode::VerticalRl) {
            switch(log) {
                case LogBlockStart: return PhysRight;
                case LogBlockEnd: return PhysLeft;
                case LogInlineStart: return (direction == Direction::Ltr) ? PhysTop : PhysBottom;
                case LogInlineEnd: return (direction == Direction::Ltr) ? PhysBottom : PhysTop;
            }
        } else {
            switch(log) {
                case LogBlockStart: return PhysLeft;
                case LogBlockEnd: return PhysRight;
                case LogInlineStart: return (direction == Direction::Ltr) ? PhysTop : PhysBottom;
                case LogInlineEnd: return (direction == Direction::Ltr) ? PhysBottom : PhysTop;
            }
        }
        return PhysTop;
    }
    LogicalSide getLogicalSideMapping(PhysicalSide phys) const {
        for (int l = 0; l < 4; ++l) {
            if (mapLogicalSide((LogicalSide)l) == phys) {
                return (LogicalSide)l;
            }
        }
        return LogBlockStart;
    }
    void resolveLogicalProperties() {
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            float physVal = 0.0f;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = marginT; hasPhys = hasMarginT; physOrder = orderMarginTop; break;
                case PhysRight: physVal = marginR; hasPhys = hasMarginR; physOrder = orderMarginRight; break;
                case PhysBottom: physVal = marginB; hasPhys = hasMarginB; physOrder = orderMarginBottom; break;
                case PhysLeft: physVal = marginL; hasPhys = hasMarginL; physOrder = orderMarginLeft; break;
            }
            float logVal = 0.0f;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = marginBlockStart; hasLog = hasMarginBlockStart; logOrder = orderMarginBlockStart; break;
                case LogBlockEnd: logVal = marginBlockEnd; hasLog = hasMarginBlockEnd; logOrder = orderMarginBlockEnd; break;
                case LogInlineStart: logVal = marginInlineStart; hasLog = hasMarginInlineStart; logOrder = orderMarginInlineStart; break;
                case LogInlineEnd: logVal = marginInlineEnd; hasLog = hasMarginInlineEnd; logOrder = orderMarginInlineEnd; break;
            }
            float finalVal = 0.0f;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                finalVal = logVal;
            } else if (hasPhys) {
                finalVal = physVal;
            } else {
                switch(phys) {
                    case PhysTop: finalVal = margin.top; break;
                    case PhysRight: finalVal = margin.right; break;
                    case PhysBottom: finalVal = margin.bottom; break;
                    case PhysLeft: finalVal = margin.left; break;
                }
            }
            switch(phys) {
                case PhysTop: margin.top = finalVal; break;
                case PhysRight: margin.right = finalVal; break;
                case PhysBottom: margin.bottom = finalVal; break;
                case PhysLeft: margin.left = finalVal; break;
            }
        }
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            float physVal = 0.0f;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = paddingT; hasPhys = hasPaddingT; physOrder = orderPaddingTop; break;
                case PhysRight: physVal = paddingR; hasPhys = hasPaddingR; physOrder = orderPaddingRight; break;
                case PhysBottom: physVal = paddingB; hasPhys = hasPaddingB; physOrder = orderPaddingBottom; break;
                case PhysLeft: physVal = paddingL; hasPhys = hasPaddingL; physOrder = orderPaddingLeft; break;
            }
            float logVal = 0.0f;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = paddingBlockStart; hasLog = hasPaddingBlockStart; logOrder = orderPaddingBlockStart; break;
                case LogBlockEnd: logVal = paddingBlockEnd; hasLog = hasPaddingBlockEnd; logOrder = orderPaddingBlockEnd; break;
                case LogInlineStart: logVal = paddingInlineStart; hasLog = hasPaddingInlineStart; logOrder = orderPaddingInlineStart; break;
                case LogInlineEnd: logVal = paddingInlineEnd; hasLog = hasPaddingInlineEnd; logOrder = orderPaddingInlineEnd; break;
            }
            float finalVal = 0.0f;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                finalVal = logVal;
            } else if (hasPhys) {
                finalVal = physVal;
            } else {
                switch(phys) {
                    case PhysTop: finalVal = padding.top; break;
                    case PhysRight: finalVal = padding.right; break;
                    case PhysBottom: finalVal = padding.bottom; break;
                    case PhysLeft: finalVal = padding.left; break;
                }
            }
            switch(phys) {
                case PhysTop: padding.top = finalVal; break;
                case PhysRight: padding.right = finalVal; break;
                case PhysBottom: padding.bottom = finalVal; break;
                case PhysLeft: padding.left = finalVal; break;
            }
        }
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            CSSValue physVal;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = topVal; hasPhys = hasTopVal; physOrder = orderTop; break;
                case PhysRight: physVal = rightVal; hasPhys = hasRightVal; physOrder = orderRight; break;
                case PhysBottom: physVal = bottomVal; hasPhys = hasBottomVal; physOrder = orderBottom; break;
                case PhysLeft: physVal = leftVal; hasPhys = hasLeftVal; physOrder = orderLeft; break;
            }
            CSSValue logVal;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = insetBlockStart; hasLog = hasInsetBlockStart; logOrder = orderInsetBlockStart; break;
                case LogBlockEnd: logVal = insetBlockEnd; hasLog = hasInsetBlockEnd; logOrder = orderInsetBlockEnd; break;
                case LogInlineStart: logVal = insetInlineStart; hasLog = hasInsetInlineStart; logOrder = orderInsetInlineStart; break;
                case LogInlineEnd: logVal = insetInlineEnd; hasLog = hasInsetInlineEnd; logOrder = orderInsetInlineEnd; break;
            }
            CSSValue finalVal;
            bool finalSet = false;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
                finalSet = true;
            } else if (hasLog) {
                finalVal = logVal;
                finalSet = true;
            } else if (hasPhys) {
                finalVal = physVal;
                finalSet = true;
            }
            if (finalSet) {
                switch(phys) {
                    case PhysTop: top = finalVal; break;
                    case PhysRight: right = finalVal; break;
                    case PhysBottom: bottom = finalVal; break;
                    case PhysLeft: left = finalVal; break;
                }
            }
        }
        for (int p = 0; p < 4; ++p) {
            PhysicalSide phys = (PhysicalSide)p;
            LogicalSide log = getLogicalSideMapping(phys);
            Border physVal;
            bool hasPhys = false;
            uint32_t physOrder = 0;
            switch(phys) {
                case PhysTop: physVal = borderT; hasPhys = hasBorderT; physOrder = orderBorderTop; break;
                case PhysRight: physVal = borderR; hasPhys = hasBorderR; physOrder = orderBorderRight; break;
                case PhysBottom: physVal = borderB; hasPhys = hasBorderB; physOrder = orderBorderBottom; break;
                case PhysLeft: physVal = borderL; hasPhys = hasBorderL; physOrder = orderBorderLeft; break;
            }
            Border logVal;
            bool hasLog = false;
            uint32_t logOrder = 0;
            switch(log) {
                case LogBlockStart: logVal = borderBlockStart; hasLog = hasBorderBlockStart; logOrder = orderBorderBlockStart; break;
                case LogBlockEnd: logVal = borderBlockEnd; hasLog = hasBorderBlockEnd; logOrder = orderBorderBlockEnd; break;
                case LogInlineStart: logVal = borderInlineStart; hasLog = hasBorderInlineStart; logOrder = orderBorderInlineStart; break;
                case LogInlineEnd: logVal = borderInlineEnd; hasLog = hasBorderInlineEnd; logOrder = orderBorderInlineEnd; break;
            }
            Border finalVal;
            bool finalSet = false;
            if (hasPhys && hasLog) {
                finalVal = (logOrder > physOrder) ? logVal : physVal;
                finalSet = true;
            } else if (hasLog) {
                finalVal = logVal;
                finalSet = true;
            } else if (hasPhys) {
                finalVal = physVal;
                finalSet = true;
            }
            if (finalSet) {
                switch(phys) {
                    case PhysTop: borderTop = finalVal; hasBorderTop = true; break;
                    case PhysRight: borderRight = finalVal; hasBorderRight = true; break;
                    case PhysBottom: borderBottom = finalVal; hasBorderBottom = true; break;
                    case PhysLeft: borderLeft = finalVal; hasBorderLeft = true; break;
                }
            }
        }
        {
            bool isWidthLogical = (writingMode != WritingMode::HorizontalTb);
            CSSValue physVal = width;
            bool hasPhys = hasWidthVal;
            uint32_t physOrder = orderWidth;
            CSSValue logVal = isWidthLogical ? blockSize : inlineSize;
            bool hasLog = isWidthLogical ? hasBlockSize : hasInlineSize;
            uint32_t logOrder = isWidthLogical ? orderBlockSize : orderInlineSize;
            if (hasPhys && hasLog) {
                width = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                width = logVal;
            } else if (hasPhys) {
                width = physVal;
            }
        }
        {
            bool isHeightLogical = (writingMode == WritingMode::HorizontalTb);
            CSSValue physVal = height;
            bool hasPhys = hasHeightVal;
            uint32_t physOrder = orderHeight;
            CSSValue logVal = isHeightLogical ? blockSize : inlineSize;
            bool hasLog = isHeightLogical ? hasBlockSize : hasInlineSize;
            uint32_t logOrder = isHeightLogical ? orderBlockSize : orderInlineSize;
            if (hasPhys && hasLog) {
                height = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                height = logVal;
            } else if (hasPhys) {
                height = physVal;
            }
        }
        {
            bool isWidthLogical = (writingMode != WritingMode::HorizontalTb);
            CSSValue physVal = minWidth;
            bool hasPhys = hasMinWidthVal;
            uint32_t physOrder = orderMinWidth;
            CSSValue logVal = isWidthLogical ? minBlockSize : minInlineSize;
            bool hasLog = isWidthLogical ? hasMinBlockSize : hasMinInlineSize;
            uint32_t logOrder = isWidthLogical ? orderMinBlockSize : orderMinInlineSize;
            if (hasPhys && hasLog) {
                minWidth = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                minWidth = logVal;
            } else if (hasPhys) {
                minWidth = physVal;
            }
        }
        {
            bool isHeightLogical = (writingMode == WritingMode::HorizontalTb);
            CSSValue physVal = minHeight;
            bool hasPhys = hasMinHeightVal;
            uint32_t physOrder = orderMinHeight;
            CSSValue logVal = isHeightLogical ? minBlockSize : minInlineSize;
            bool hasLog = isHeightLogical ? hasMinBlockSize : hasMinInlineSize;
            uint32_t logOrder = isHeightLogical ? orderMinBlockSize : orderMinInlineSize;
            if (hasPhys && hasLog) {
                minHeight = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                minHeight = logVal;
            } else if (hasPhys) {
                minHeight = physVal;
            }
        }
        {
            bool isWidthLogical = (writingMode != WritingMode::HorizontalTb);
            CSSValue physVal = maxWidth;
            bool hasPhys = hasMaxWidthVal;
            uint32_t physOrder = orderMaxWidth;
            CSSValue logVal = isWidthLogical ? maxBlockSize : maxInlineSize;
            bool hasLog = isWidthLogical ? hasMaxBlockSize : hasMaxInlineSize;
            uint32_t logOrder = isWidthLogical ? orderMaxBlockSize : orderMaxInlineSize;
            if (hasPhys && hasLog) {
                maxWidth = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                maxWidth = logVal;
            } else if (hasPhys) {
                maxWidth = physVal;
            }
        }
        {
            bool isHeightLogical = (writingMode == WritingMode::HorizontalTb);
            CSSValue physVal = maxHeight;
            bool hasPhys = hasMaxHeightVal;
            uint32_t physOrder = orderMaxHeight;
            CSSValue logVal = isHeightLogical ? maxBlockSize : maxInlineSize;
            bool hasLog = isHeightLogical ? hasMaxBlockSize : hasMaxInlineSize;
            uint32_t logOrder = isHeightLogical ? orderMaxBlockSize : orderMaxInlineSize;
            if (hasPhys && hasLog) {
                maxHeight = (logOrder > physOrder) ? logVal : physVal;
            } else if (hasLog) {
                maxHeight = logVal;
            } else if (hasPhys) {
                maxHeight = physVal;
            }
        }
    }
    CSSValue top, right, bottom, left;
    Color color = Color(1, 1, 1, 1);
    Color backgroundColor = Color(0, 0, 0, 0);
    Gradient backgroundGradient;
    Border border;
    Border borderTop, borderRight, borderBottom, borderLeft;
    bool hasBorderTop = false, hasBorderRight = false;
    bool hasBorderBottom = false, hasBorderLeft = false;
    Border outline;
    BorderRadius borderRadius;
    BoxShadow boxShadow;
    float opacity = 1.0f;
    float outlineOffset = 0;
    // Legacy scalar kept for the renderer drawBackdropFilterBlur() path.
    // Always synced from backdropFilterOperations when parsed.
    float backdropFilterBlur = 0.0f;
    bool hasBackdropFilterBlur = false;
    // Full filter operation lists (Blink FilterOperations parity).
    // Both properties accept the same function set; backdrop-filter also
    // accepts blur() as the primary GPU-composited path.
    std::vector<FilterOperation> filterOperations;           // filter:
    std::vector<FilterOperation> backdropFilterOperations;  // backdrop-filter:
    bool hasFilter = false;
    bool hasBackdropFilter = false;

    // ── Masking / Scroll / Blending enum aliases ──────────────────────────────
    // The enums themselves now live at namespace scope (css_enums.h) so the cold
    // StyleRareData group can use them, but these `using` aliases keep all existing
    // `Style::BlendMode::X` / `Style::Isolation::X` references compiling unchanged.
    using BlendMode = FluxUI::BlendMode;
    using Isolation = FluxUI::Isolation;
    using OverscrollBehavior = FluxUI::OverscrollBehavior;
    using ScrollbarWidth = FluxUI::ScrollbarWidth;
    using OverflowAnchor = FluxUI::OverflowAnchor;
    using ScrollBehavior = FluxUI::ScrollBehavior;

    float fontSize = 14.0f;
    FontWeight fontWeight = FontWeight::Normal;
    FontStyle fontStyle = FontStyle::Normal;
    TextAlign textAlign = TextAlign::Left;
    float lineHeight = 1.4f;
    std::string fontFamily;
    TextOverflow textOverflow = TextOverflow::Clip;
    WhiteSpace whiteSpace = WhiteSpace::Normal;
    TextDecoration textDecoration = TextDecoration::None;
    TextTransform textTransform = TextTransform::None;
    WordBreak wordBreak = WordBreak::Normal;
    VerticalAlign verticalAlign = VerticalAlign::Baseline;
    ListStyleType listStyleType = ListStyleType::Disc;
    float letterSpacing = 0;
    float wordSpacing = 0;
    Color textDecorationColor;
    bool hasTextDecorationColor = false;
    // text-shadow: comma-separated list of shadow layers (inherited).
    std::vector<TextShadow> textShadows;
    bool hasTextShadow = false;
    bool hasColor = false;
    bool hasFontSize = false;
    bool hasFontWeight = false;
    bool hasFontStyle = false;
    bool hasTextAlign = false;
    bool hasLineHeight = false;
    bool hasFontFamily = false;
    bool hasLetterSpacing = false;
    bool hasWordSpacing = false;
    bool hasTextOverflow = false;
    bool hasWhiteSpace = false;
    bool hasTextDecoration = false;
    bool hasTextTransform = false;
    bool hasWordBreak = false;
    bool hasVerticalAlign = false;
    bool hasListStyleType = false;

    // ── Advanced Typography / Masking / Scroll / UI-misc / Animation / Transition ──
    // All cold (rarely-set) non-inherited properties now live in StyleRareData,
    // shared copy-on-write via DataRef (Blink StyleRareNonInheritedData parity).
    // Access through rare() — const for reads (no copy), non-const for writes (COW).

    FastCustomProperties customProperties;
    FastCustomProperties hoverCustomProperties;
    FastCustomProperties focusCustomProperties;
    FastCustomProperties activeCustomProperties;
    std::string unresolvedBackgroundColor;
    std::string unresolvedColor;
    std::string unresolvedBorderColor;
    std::string unresolvedBackgroundGradient;
    CursorType cursor = CursorType::Default;
    float transitionDuration = 0.15f;
    float scale = 1.0f;
    float springStiffness = 180.0f;
    float springDamping = 18.0f;
    Color hoverBackgroundColor;
    Color hoverColor;
    Gradient hoverBackgroundGradient;
    float hoverOpacity = -1;
    bool hasHoverBg = false;
    bool hasHoverColor = false;
    bool hasHoverBorder = false;
    bool hasHoverGradient = false;
    Color hoverBorderColor;
    float hoverScale = -1;
    Color focusBackgroundColor;
    Color focusColor;
    Color focusBorderColor;
    Gradient focusBackgroundGradient;
    Border focusOutline;
    bool hasFocusBg = false;
    bool hasFocusColor = false;
    bool hasFocusBorder = false;
    bool hasFocusOutline = false;
    bool hasFocusGradient = false;
    float focusOpacity = -1;
    float focusScale = -1;
    Color activeBackgroundColor;
    Color activeColor;
    Color activeBorderColor;
    Gradient activeBackgroundGradient;
    Border activeOutline;
    bool hasActiveBg = false;
    bool hasActiveColor = false;
    bool hasActiveBorder = false;
    bool hasActiveOutline = false;
    bool hasActiveGradient = false;
    float activeOpacity = -1;
    float activeScale = -1;
    std::vector<TransformOperation> transform;
    TransformOrigin transformOrigin;
    TransformStyle transformStyle = TransformStyle::Flat;
    TransformBox transformBox = TransformBox::BorderBox;
    CSSValue perspective = CSSValue{0.0f, CSSValue::None};
    PerspectiveOrigin perspectiveOrigin;
    BackfaceVisibility backfaceVisibility = BackfaceVisibility::Visible;
    bool hasTransform = false;
    bool hasTransformOrigin = false;
    bool hasTransformStyle = false;
    bool hasTransformBox = false;
    bool hasPerspective = false;
    bool hasPerspectiveOrigin = false;
    bool hasBackfaceVisibility = false;

    // ── Cold (rarely-set) non-inherited property group (copy-on-write) ──────────
    // Shared by pointer across Style copies; cloned lazily on first write.
    DataRef<StyleRareData> rareData;
    // Read access (no copy). Use on const Style / when only reading.
    const StyleRareData& rare() const { return *rareData; }
    // Write access (copy-on-write via DataRef::access()).
    StyleRareData& rare() { return *rareData.access(); }

    uint64_t inheritedHash = 0;
};
class ComputedStyle {
public:
    ComputedStyle() : m_style(std::make_shared<Style>()) {}
    ComputedStyle(const Style& style) : m_style(std::make_shared<Style>(style)) {}
    ComputedStyle(std::shared_ptr<const Style> style) : m_style(std::move(style)) {}
    ComputedStyle(const ComputedStyle& other) = default;
    ComputedStyle& operator=(const ComputedStyle& other) = default;
    ComputedStyle(ComputedStyle&& other) noexcept = default;
    ComputedStyle& operator=(ComputedStyle&& other) noexcept = default;
    const Style* operator->() const { return m_style.get(); }
    Style* operator->() { return &ensureMutable(); }
    const Style& operator*() const { return *m_style; }
    Style& operator*() { return ensureMutable(); }
    operator const Style&() const { return *m_style; }
    explicit operator bool() const { return m_style != nullptr; }
    const Style* get() const { return m_style.get(); }
    Style& ensureMutable() {
        if (!m_style) {
            m_style = std::make_shared<Style>();
        } else if (m_style.use_count() > 1) {
            m_style = std::make_shared<Style>(*m_style);
        }
        return const_cast<Style&>(*m_style);
    }
    ComputedStyle& operator=(std::shared_ptr<const Style> other) {
        m_style = std::move(other);
        return *this;
    }
    ComputedStyle& operator=(const Style& other) {
        m_style = std::make_shared<Style>(other);
        return *this;
    }
    ComputedStyle& operator=(std::nullptr_t) {
        m_style = nullptr;
        return *this;
    }
    bool operator==(const ComputedStyle& other) const {
        return m_style == other.m_style;
    }
    bool operator!=(const ComputedStyle& other) const {
        return m_style != other.m_style;
    }
    bool operator==(std::nullptr_t) const {
        return m_style == nullptr;
    }
    bool operator!=(std::nullptr_t) const {
        return m_style != nullptr;
    }
private:
    std::shared_ptr<const Style> m_style;
};
}