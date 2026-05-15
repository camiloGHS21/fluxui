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

## App Runtime Additions

FluxUI now includes a lightweight event bus and retained page router:

```cpp
app.on(FluxUI::UIEventType::RouteChanged, [](FluxUI::UIEvent& event) {
    // event.previousRoute -> event.route
});

app.addRoute("/dashboard", [](FluxUI::Application& app, FluxUI::Widget* view) {
    view->add<FluxUI::Text>("Dashboard", "page-title");
});

app.navigate("/dashboard");
app.renderRoute(content);
```

Input events, route changes, widget clicks, and custom events share the same
listener path. The router keeps page construction centralized and lets apps
rebuild only the active view instead of maintaining ad hoc navigation switches.

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
