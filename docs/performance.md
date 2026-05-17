# FluxUI Performance Notes

FluxUI is moving toward a Vulkan-first, GPU-command architecture similar in
spirit to Zed's GPUI. Vulkan is now the default backend preference. `Auto`
tries Vulkan first, then Direct3D 12 on Windows or Metal on Apple platforms,
then the internal compatibility renderer.

## What Was Optimized

- Uniform locations are cached once after shader creation instead of being looked
  up by name on every draw call.
- Text rendering now reuses one VAO/VBO pair instead of creating and destroying
  GPU buffers for every text widget.
- Text vertex scratch memory is kept inside the renderer and reused across draw
  calls to reduce per-frame allocations.
- Resolved CSS styles are cached per class/id/type tuple, so repeated widgets do
  not traverse the stylesheet every frame.
- Text measurement is cached and invalidated when font atlases change, reducing
  repeated UTF-8 decoding and glyph-width walks.
- Derived font-size atlases no longer duplicate the full source font bytes,
  keeping RAM lower when the UI requests multiple text sizes.
- Font atlases now default to the Latin + Latin Extended-A range
  (`FLUXUI_FONT_GLYPH_LIMIT=384`), which makes runtime font baking much faster
  for dashboard-style apps while remaining configurable for broader Unicode.
- Apps can call `Renderer::warmFontCache()` to prebuild common UI font sizes
  and upload Vulkan font textures before the first visible frame.
- After warming, `Renderer::releaseFontSources()` can drop retained font file
  bytes so base and bold fonts do not stay resident in RAM.
- Left-aligned text skips width measurement during `drawTextInRect()`, avoiding
  cache-key construction and glyph walks for the most common label path.
- Style resolution is dirty-flagged per subtree, so unchanged widget trees skip
  CSS merge work after the first resolved frame.
- Layout is dirty-flagged and cached by parent bounds, so stable subtrees skip
  flex/block layout recalculation.
- Widget children can be pooled through the lazy per-parent widget arena and
  pre-sized through `reserveChildren()` to reduce allocator churn.
- MSAA and vsync can be disabled at configure time for low-power devices or
  benchmarking.
- Vulkan runtime probing now uses dynamic loading, so the framework can verify
  loader, surface, and GPU compatibility even when the Vulkan SDK is not
  installed on the build machine.
- The default Vulkan backend now creates a native instance/surface/swapchain,
  records command buffers, builds SPIR-V shader pipelines through glslang, and
  presents frames directly. Rounded rectangles, gradients, borders, and text
  atlas sampling run on this path.
- Vulkan rectangles, borders, shadows, and text use batched dynamic pages owned
  by each swapchain frame, so the CPU can reuse memory without overwriting data
  that the GPU may still read.
- The demo app now uses the framework router instead of a manual page switch,
  and scanner progress throttles UI rebuilds to visible progress changes.
- The demo shell is retained and built before the run loop, so the first frame
  starts from an already constructed dashboard tree.

## App Runtime Additions

FluxUI now includes a lightweight event bus and retained page router:

```cpp
app.on(FluxUI::UIEventType::RouteChanged, [](FluxUI::UIEvent& event) {
    // event.previousRoute -> event.route
});

app.addRoute("/dashboard", [](FluxUI::Application& app, FluxUI::Widget* view) {
    view->text("Dashboard", "page-title");
});

app.navigate("/dashboard");
app.renderRoute(content);
```

Input events, route changes, widget clicks, and custom events share the same
listener path. The router keeps page construction centralized and lets apps
rebuild only the active view instead of maintaining ad hoc navigation switches.

## Less Verbose UI Code

Every widget now has short child builders for common controls and HTML-like
aliases:

```cpp
auto* bar = view->div("top-bar", 2);
bar->h1("Dashboard", "page-title");
bar->p("Live metrics", "page-subtitle");
bar->button("Export", "btn btn-secondary", [] {
    // handle click
});
bar->addIcon("download", "btn-icon");
view->progress(0.72f, "progress-line", FluxUI::Color::fromHex("#37C6A3"))
    ->css("margin-top: 8px; width: 100%;");
```

These helpers still use the same retained widget tree, arena allocator, dirty
style flags, and layout cache, but avoid repetitive `add<T>()` boilerplate.
`setId()`, `classes()`, `addClass()`, `removeClass()`, and `toggleClass()` make
stateful UI updates feel closer to DOM class manipulation.

## Browser-Like CSS Cascade

The stylesheet resolver now follows the browser cascade model for author CSS:
matching declarations are applied by `!important`, selector specificity, and
source order. That means `#id` beats `.class`, `.class` beats `button`, and a
later rule only wins when priority and specificity are tied. Inline
`widget->css("...")` declarations are applied after stylesheet resolution, like
an HTML `style=""` attribute.

FluxUI also supports the most common tree-aware selectors in the Blink style:

```css
.sidebar .nav-text { color: var(--text-muted); }
.top-bar > .title-group { min-width: 260px; }
h1.page-title { font-weight: 700; }
```

Style resolution now passes a lightweight ancestor chain into selector
matching. The matcher works right-to-left, first checking the current widget and
then walking parents for descendant (`A B`) and child (`A > B`) combinators.
Class/id changes use subtree style invalidation, matching Blink's conservative
approach: it may recalculate a little more than strictly necessary, but avoids
stale descendant styles.

Like Blink's `RuleSet`, parsed rules are now partitioned by the most useful
simple selector in the right-most compound selector. A selector such as
`.sidebar .nav-text` is stored in the class bucket for `nav-text`, while
`#primary-action` is stored in the id bucket and `button.primary` in the class
bucket for `primary`. During style resolution FluxUI gathers only universal,
matching id, matching class, and matching type buckets before running the
selector checker. This keeps cascade behavior the same while avoiding a full
stylesheet scan for every widget.

The selector checker now handles a practical subset of Selectors Level 4
functional pseudo-classes in the same spirit as Blink's `SelectorChecker`:

```css
.btn:is(.primary, .danger) { font-weight: 700; }
.nav-item:not(.active) { opacity: 0.72; }
:where(.panel-card) .section-title { letter-spacing: 0; }
```

`:is()` and `:where()` match when any selector in the argument list matches the
current compound selector; `:not()` rejects the rule when any argument matches.
Specificity follows the browser rules: `:is()` and `:not()` use the most
specific argument, while `:where()` contributes zero specificity.

Custom properties now follow the browser model more closely. `:root` variables
seed the tree, selector-scoped `--tokens` participate in cascade, descendants
inherit the computed token map, and `var(--token, fallback)` is resolved against
the current widget's computed custom properties:

```css
:root { --accent: #00ffa8; }
.danger-zone { --accent: #ff3b53; }
.danger-zone .btn { background-color: var(--accent); }
```

Inline `widget->css("--accent: ...")` also invalidates the subtree so
descendants that consume the token recompute their styles.

Basic user-agent defaults and inherited text properties are also modeled:
`h1`/`h2`/`h3` get browser-like default sizes and weight, while `color`,
`font-size`, `font-weight`, `font-family`, `line-height`, and `text-align`
inherit from the parent unless the widget's own CSS overrides them.

## Fast Build Flags

For maximum frame rate testing:

```powershell
cmake -S . -B build-fast -DFLUXUI_ENABLE_MSAA=OFF -DFLUXUI_ENABLE_VSYNC=OFF
cmake --build build-fast --config Release --target DataLeakGuard
```

For the default Vulkan-first preference:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Runtime Vulkan check:

```powershell
.\build\Release\DataLeakGuard.exe --probe-vulkan
.\build\Release\DataLeakGuard.exe --backend=vulkan --frames=3
```

## Native Backend Decision

Vulkan, Direct3D 12, and Metal can outperform immediate-style draw paths when
the renderer is built around persistent command buffers, descriptor/resource
sets, pipelines, and large batches. Switching FluxUI directly to one native API
before the backend abstraction is complete would make the framework harder to
compile and maintain while keeping many current CPU-side costs.

FluxUI now includes the backend-selection layer, with Vulkan as the default
preference and an initial native Vulkan renderer. Direct3D 12 plus Metal remain
present in the public API. See `docs/backends.md` for the C, C++, Rust, and
Zig APIs.

Recommended path:

1. Prefer Vulkan by default while using Direct3D 12 and Metal as native
   platform targets after Vulkan in the fallback order.
2. Use the backend-selection API so apps can request `Auto`, `Vulkan`,
   `Direct3D12`, or `Metal` without source churn later.
3. Continue reducing CPU work: data-source virtualization, route-level rebuild
   throttling, and larger retained batches.
4. Add GPU timing probes and resize hardening around the native Vulkan path.
5. Bring Direct3D 12 and Metal up after the Vulkan command path is mature.

That keeps FluxUI fast now and leaves room for stronger Vulkan, Direct3D 12,
and Metal paths without breaking existing C, C++, Rust, or Zig users. DirectX
13 is not a separate backend target; the Windows native target is Direct3D 12.
