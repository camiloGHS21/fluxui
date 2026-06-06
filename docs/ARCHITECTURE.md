# FluxUI Architecture

FluxUI is a GPU-accelerated, CSS-styled UI framework. Its design mirrors a
browser engine (Blink-style): a DOM-like widget tree, a CSS cascade, a layout
solver, a paint/compositor stage, and a GPU (or CPU-software) renderer.

## Repository layout

```
fluxui/
  include/fluxui/        ← PUBLIC API (installed). Users #include <fluxui/...>.
    dsl.h                  modern declarative DSL (the public API) — umbrella
    FluxUI.h               internal umbrella header (engine + bindings only)
    core.h widgets.h ...   engine types — umbrella headers (see detail/ below)
    detail/                sub-headers, split so no file exceeds ~1000 lines.
                           Grouped by subsystem, mirroring src/. The top-level
                           umbrella headers just #include these, so the public
                           API (#include <fluxui/core.h> etc.) never changes.
      core/                  geometry.h css_enums.h css_value.h transform.h
                             style.h events.h   (the CSS / paint data model)
      widgets/               widget_base.h widget_elements.h widget_factory.h
                             application.h
      dsl/                   dsl_core.h dsl_app.h dsl_ecosystem.h
  src/                   ← engine implementation, organized by subsystem:
    core/                  application loop, widget tree, GC, a11y, compositor
      application.cpp        Application class + Widget layout/input/animation
      widget_style.cpp       Widget style invalidation (dirty-flag propagation)
      widget_cascade.cpp     Widget style resolution (CSS cascade application)
      widget_prepaint.cpp    Widget pre-paint: transform/clip/effect property tree
      widget_paint.cpp       Widget painting: list markers, background, children
      widgets/               one TU per widget group (all concrete widgets)
        video.cpp              HTMLVideoElement (Win32 audio + controls)
        svg.cpp                SVG element tree + rasterization hook
        controls.cpp           form controls: Checkbox/Radio/RangeInput/
                               ProgressBar/Meter/Progress
        text.cpp               Text, Button, TextInput
        textarea.cpp           TextArea (multi-line editor)
        media.cpp              Option, Select, Icon, Image, Canvas,
                               VirtualList, StatCard
        interactive.cpp        LazyPanel, Anchor, Details, Summary, Dialog
      widget_internal.h      shared inline detail:: helpers for the widget TUs
      compositor.cpp         animation/scroll compositor thread
      accessibility.cpp      AX object cache
      gc.cpp                 mark/sweep helpers
    style/                 CSS parsing + cascade
      css_parser.cpp         tokenize, parse rules, selectors, cascade resolve
      css_merge.cpp          property merge: maps declarations to Style fields
      css_value_parser.cpp   typed value + animation/timing parsers, @supports
      css_transform_parser.cpp transform / grid / filter value grammars
      css_interpolate.cpp    animation interpolation + @property <syntax> check
      css_media_query.cpp    @media query evaluation
      css_ua_sheet.cpp       built-in User-Agent stylesheet + UA defaults
      css_color.cpp          color + gradient parsing (parseColor/parseGradient)
      css_internal.h         shared inline detail:: string + color-channel helpers
    layout/                Blink-style layout
      layout.cpp             flex/block/inline solver
      layout_object.cpp      layout object tree
      grid_engine.inl        grid algorithm (included by layout.cpp)
    render/                rendering backends
      renderer.cpp           backend selection, GL setup, frame loop, draw API
      renderer_vulkan.cpp    Vulkan backend (device/swapchain/pipelines/draw)
      renderer_vulkan.h        Vulkan state struct + cross-TU entry points
      renderer_text.cpp      HarfBuzz/FreeType text shaping + glyph drawing
      renderer_effects.cpp   backdrop-filter blur (software + GPU Gaussian)
      renderer_svg.cpp       SVG parser + document walk (rasterizeSvgToRgba,
                             rasterizeSvgWidget)
      renderer_svg_internal.h  SVG geometry types + raster primitives
                             (inline FluxUI::detail): fills, strokes, paths
      renderer_svg.h         exported SVG entry points (shared with renderer.cpp)
      software_raster.cpp    CPU software rasterizer (no-GPU fallback)
      software_internal.h    shared inline detail:: pixel helpers
      image_resource.cpp     image decode/cache
      vulkan_shaders.h        embedded GLSL (included by renderer.cpp)
      vulkan_pipeline_cache.h pipeline cache (included by renderer.cpp)
    platform/              per-OS window/input/power
      platform_win32.cpp  platform_macos.mm  platform_linux.cpp  platform_android.cpp
    ffi/                   C ABI for language bindings
      fluxui_c.cpp
bindings/                ← Go, Rust, Python, Java, Zig wrappers over the C ABI
examples/                ← demos, tests, benchmarks
docs/                    ← PERFORMANCE.md, ARCHITECTURE.md
```

## Layering (who depends on whom)

```
platform  →  render  →  layout  →  style  →  core  →  ffi  →  bindings
                                                  ↑
                                       dsl.h (public, header-only)
```

- **core** orchestrates a frame: style recalc → layout → pre-paint → paint.
- **style** is standalone (no widget/render deps beyond the public types).
- **render** is the only subsystem that talks to Vulkan/GDI.
- **platform** is the only subsystem that talks to the OS window/input APIs.
- **ffi** is a thin C shim; the bindings call only the C ABI.

## Public vs internal headers

- `fluxui/include/fluxui/*.h` is the **public, stable API**. The only header a
  user app includes is `<fluxui/dsl.h>`. The top-level headers (`core.h`,
  `widgets.h`, `dsl.h`) are **umbrella headers**: they only `#include` the
  focused sub-headers under `include/fluxui/detail/{core,widgets,dsl}/`. This
  keeps every file under ~1000 lines without changing a single user include.
- `fluxui/src/**/**.h` (e.g. `core/widget_internal.h`, `render/vulkan_*.h`) are
  **private** to the engine and are not installed.

## Splitting large translation units

Three subsystems (`core/application.cpp`, `render/renderer.cpp`,
`style/css_parser.cpp`) historically held an entire subsystem in one file. They
are being split incrementally into cohesive TUs. The rule that keeps this safe:

> A file-local `static` helper shared by functions that move to a new TU must
> first be promoted to an `inline` function in a private internal header (in a
> `FluxUI::detail` namespace), so every TU sees one definition.

`core/widget_internal.h` holds these shared helpers: the paint/hit-test gates,
text shaping/measurement, UTF-8 navigation, overflow/border resolution, text
transform/decoration, and the layout-signature hash — everything the widget
TUs and the `Widget` base class share. `application.cpp` and every widget TU do
`using namespace FluxUI::detail;` so the original call sites compile unchanged.

ALL concrete widget implementations now live under `core/widgets/`; the
`Widget` base class itself is split by pipeline stage across `core/`:
`widget_style.cpp` (style invalidation), `widget_cascade.cpp` (cascade
resolution), `widget_prepaint.cpp` (property-tree build), and `widget_paint.cpp`
(rasterization). What remains in `application.cpp` is the `Application` class
plus the Widget layout / input / animation methods (~3160 lines, down from the
original ~8200). Every TU stays well under 1000 lines.

## Frame pipeline (per redraw)

1. **Style recalc** — `Widget::resolveStyles` matches CSS rules (cached by epoch).
2. **Layout** — `Widget::layout` / layout objects compute boxes.
3. **Pre-paint** — paint property trees (transform/clip/opacity).
4. **Paint** — `Widget::render` emits draw commands.
5. **Present** — Vulkan swapchain or CPU `SetDIBitsToDevice`.

The run loop only executes this when something is dirty, and paces frames by
power state (see `docs/PERFORMANCE.md`).
