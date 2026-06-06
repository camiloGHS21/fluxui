#pragma once
// FluxUI public API - CSS property enums & grid value types.
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
namespace FluxUI {
enum class Display {
    Block,
    Flex,
    Grid,
    InlineBlock,
    Inline,
    None,
    ListItem,
    Table,
    TableRowGroup,
    TableHeaderGroup,
    TableFooterGroup,
    TableRow,
    TableCell,
    TableColumn,
    TableColumnGroup,
    TableCaption,
    Contents
};
enum class Direction { Ltr, Rtl };
enum class UnicodeBidi { Normal, Embed, BidiOverride, Isolate, IsolateOverride, Plaintext };
enum class WritingMode { HorizontalTb, VerticalRl, VerticalLr };
enum class FlexDirection { Row, Column, RowReverse, ColumnReverse };
enum class FlexWrap { NoWrap, Wrap, WrapReverse };
enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignItems { FlexStart, FlexEnd, Center, Stretch, Baseline };
enum class AlignContent { FlexStart, FlexEnd, Center, Stretch, SpaceBetween, SpaceAround, SpaceEvenly };
enum class AlignSelf { Auto, FlexStart, FlexEnd, Center, Stretch, Baseline };
// ── CSS Grid enums (Blink parity) ──────────────────────────
enum class JustifyItems { Auto, Normal, Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class JustifySelf  { Auto, Normal, Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class GridAutoFlow { Row, Column, RowDense, ColumnDense };
// ── Grid track sizing function type (Blink GridTrackSize parity) ──
enum class GridTrackSizeType {
    Fixed,        // <length> or <percentage>
    Flex,         // <number>fr
    MinMax,       // minmax(<min>, <max>)
    FitContent,   // fit-content(<length>)
    Auto,         // auto
    MinContent,   // min-content
    MaxContent,   // max-content
    Subgrid,      // subgrid keyword
};
// ── Single grid track definition (Blink GridLength/GridTrackSize parity) ──
struct GridTrackSize {
    GridTrackSizeType type = GridTrackSizeType::Auto;
    float value   = 0.0f;   // resolved px for Fixed; fr value for Flex
    float minValue= 0.0f;   // minmax: min side (px after resolve)
    float maxValue= 0.0f;   // minmax: max side (px after resolve)
    GridTrackSizeType minType = GridTrackSizeType::Auto;
    GridTrackSizeType maxType = GridTrackSizeType::Auto;
    std::string namedLine;  // optional [name] for named grid lines

    bool isFlex()     const { return type == GridTrackSizeType::Flex; }
    bool isAuto()     const { return type == GridTrackSizeType::Auto; }
    bool isFixed()    const { return type == GridTrackSizeType::Fixed; }
    bool isSubgrid()  const { return type == GridTrackSizeType::Subgrid; }

    bool operator==(const GridTrackSize& o) const {
        return type == o.type && value == o.value &&
               minValue == o.minValue && maxValue == o.maxValue &&
               minType == o.minType && maxType == o.maxType &&
               namedLine == o.namedLine;
    }
    bool operator!=(const GridTrackSize& o) const { return !(*this == o); }
};
// ── Grid placement value (Blink GridPosition parity) ──────
// Covers: auto | <integer> | <integer> span | span <integer> | [name]
struct GridPlacement {
    enum class PlacementType { Auto, Line, Span, NamedLine };
    PlacementType type = PlacementType::Auto;
    int          line  = 0;   // 1-based; negative counts from end
    int          span  = 1;   // span count
    std::string  name;        // named line reference

    bool isAuto() const { return type == PlacementType::Auto; }

    bool operator==(const GridPlacement& o) const {
        return type == o.type && line == o.line && span == o.span && name == o.name;
    }
    bool operator!=(const GridPlacement& o) const { return !(*this == o); }
};
// ── Parsed grid-template-areas matrix ─────────────────────
struct GridTemplateAreas {
    std::vector<std::string> areas;   // row-major named areas, "." = anonymous
    int rowCount    = 0;
    int columnCount = 0;
    bool operator==(const GridTemplateAreas& o) const {
        return areas == o.areas && rowCount == o.rowCount && columnCount == o.columnCount;
    }
};
enum class Position { Static, Relative, Absolute, Fixed, Sticky };
enum class CSSFloat { None, Left, Right };
enum class CSSClear { None, Left, Right, Both };
enum class Overflow { Visible, Hidden, Scroll, Auto, Clip };
enum class TextAlign { Left, Center, Right, Justify };
// CSS font-weight: numeric ladder 100–900 (CSS Fonts L4). Normal == 400 and
// Bold == 700 are kept as named aliases so existing FontWeight::Normal/Bold
// code keeps working. Use isBoldWeight() for bold synthesis decisions.
enum class FontWeight {
    Thin       = 100,
    ExtraLight = 200,
    Light      = 300,
    Normal     = 400,
    Medium     = 500,
    SemiBold   = 600,
    Bold       = 700,
    ExtraBold  = 800,
    Black      = 900
};
// A weight triggers bold synthesis when it is >= 600 (Blink: bold-ish threshold
// when no dedicated bold face exists).
inline bool isBoldWeight(FontWeight w) { return static_cast<int>(w) >= 600; }
inline int fontWeightValue(FontWeight w) { return static_cast<int>(w); }
// Snap an arbitrary numeric weight (e.g. parsed 100..900, or bolder/lighter
// results) to the nearest standard FontWeight bucket.
inline FontWeight fontWeightFromInt(int v) {
    if (v < 150) return FontWeight::Thin;
    if (v < 250) return FontWeight::ExtraLight;
    if (v < 350) return FontWeight::Light;
    if (v < 450) return FontWeight::Normal;
    if (v < 550) return FontWeight::Medium;
    if (v < 650) return FontWeight::SemiBold;
    if (v < 750) return FontWeight::Bold;
    if (v < 850) return FontWeight::ExtraBold;
    return FontWeight::Black;
}
enum class FontStyle { Normal, Italic, Oblique };
enum class CursorType { Default, Pointer, Text, Grab, Grabbing, NotAllowed, Crosshair, ResizeNWSE, ResizeNS };
enum class BoxSizing { ContentBox, BorderBox };
enum class Visibility { Visible, Hidden, Collapse };
enum class TextOverflow { Clip, Ellipsis };
enum class WhiteSpace { Normal, NoWrap, Pre, PreWrap, PreLine };
enum class TextDecoration { None, Underline, LineThrough, Overline };
enum class TextTransform { None, Uppercase, Lowercase, Capitalize };
enum class PointerEvents { Auto, None };
enum class WordBreak { Normal, BreakAll, KeepAll, BreakWord };
enum class ObjectFit { Fill, Contain, Cover, None, ScaleDown };
enum class ListStyleType {
    None,
    Disc,
    Circle,
    Square,
    Decimal,
    DecimalLeadingZero,
    LowerRoman,
    UpperRoman,
    LowerAlpha,
    UpperAlpha
};
enum class VerticalAlign { Baseline, Sub, Super, Middle, Top, Bottom, TextTop, TextBottom };
enum class Appearance {
    Auto,
    None,
    TextField,
    SearchField,
    PushButton,
    Button,
    Checkbox,
    Radio,
    Menulist,
    Textarea,
    SliderHorizontal,
    SquareButton
};
enum ContainmentFlags : uint8_t {
    kContainNone = 0,
    kContainSize = 1 << 0,
    kContainLayout = 1 << 1,
    kContainPaint = 1 << 2,
    kContainStyle = 1 << 3,
    kContainContent = kContainLayout | kContainPaint | kContainStyle,
    kContainStrict = kContainSize | kContainLayout | kContainPaint | kContainStyle
};
enum class AnimationDirection {
    Normal,
    Reverse,
    Alternate,
    AlternateReverse
};
enum class AnimationFillMode {
    None,
    Forwards,
    Backwards,
    Both
};
enum class AnimationPlayState {
    Running,
    Paused
};
enum class AnimationComposition {
    Replace,
    Add,
    Accumulate
};
enum class TransitionBehavior {
    Normal,
    AllowDiscrete
};
// ── Rare non-inherited enums (moved to namespace scope so they can be used by
//    StyleRareData; Style keeps `using` aliases for source-compatibility with
//    existing `Style::BlendMode::X` references). Blink groups these under
//    StyleRareNonInheritedData. ──────────────────────────────────────────────
enum class BlendMode {
    Normal, Multiply, Screen, Overlay, Darken, Lighten,
    ColorDodge, ColorBurn, HardLight, SoftLight, Difference,
    Exclusion, Hue, Saturation, Color, Luminosity
};
enum class Isolation { Auto, Isolate };
enum class OverscrollBehavior { Auto, Contain, None };
enum class ScrollbarWidth { Auto, Thin, None };
enum class OverflowAnchor { Auto, None };
enum class ScrollBehavior { Auto, Smooth };
}