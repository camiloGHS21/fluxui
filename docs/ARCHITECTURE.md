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
      application.cpp        Application + Widget + base widget subclasses
      widgets/               one TU per widget group
        video.cpp              HTMLVideoElement (Win32 audio + controls)
        svg.cpp                SVG element tree + rasterization hook
        controls.cpp           form controls: Checkbox/Radio/RangeInput/
                               ProgressBar/Meter/Progress
      widget_internal.h      shared inline detail:: helpers for the widget TUs
      compositor.cpp         animation/scroll compositor thread
      accessibility.cpp      AX object cache
      gc.cpp                 mark/sweep helpers
    style/                 CSS parsing + cascade
      css_parser.cpp         StyleSheet: tokenize, parse, selector match, cascade
      css_color.cpp          color + gradient parsing (parseColor/parseGradient)
      css_internal.h         shared inline detail:: string + color-channel helpers
    layout/                Blink-style layout
      layout.cpp             flex/block/inline solver
      layout_object.cpp      layout object tree
      grid_engine.inl        grid algorithm (included by layout.cpp)
    render/                rendering backends
      renderer.cpp           Vulkan backend, fonts, text shaping, draw API
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

`core/widget_internal.h` holds these shared helpers (paint/hit-test gates,
`normalizeTextEditingKey`, radio-group reset). Widget implementations are
extracted from `application.cpp` into `core/widgets/<group>.cpp`:
`video.cpp`, `svg.cpp`, and `controls.cpp` (Checkbox/Radio/RangeInput/
ProgressBar/Meter/Progress). The remaining text/layout-heavy widgets
(Text/Button/TextInput/TextArea/Select/Image/VirtualList) still live in
`application.cpp` because they depend on a large cluster of text-shaping
statics; they are the next candidates to move once those statics are promoted.

## Frame pipeline (per redraw)

1. **Style recalc** — `Widget::resolveStyles` matches CSS rules (cached by epoch).
2. **Layout** — `Widget::layout` / layout objects compute boxes.
3. **Pre-paint** — paint property trees (transform/clip/opacity).
4. **Paint** — `Widget::render` emits draw commands.
5. **Present** — Vulkan swapchain or CPU `SetDIBitsToDevice`.

The run loop only executes this when something is dirty, and paces frames by
power state (see `docs/PERFORMANCE.md`).
