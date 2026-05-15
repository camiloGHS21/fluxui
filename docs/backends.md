# FluxUI Render Backends

FluxUI has a public backend-selection layer. The default preference is Vulkan,
matching the direction of GPU-first UI engines such as Zed's GPUI. `Auto`
tries Vulkan first, then the platform-native target (`Direct3D 12` on Windows
or `Metal` on Apple platforms), then the internal compatibility renderer.

When a requested native backend is not compiled or not selectable yet, FluxUI
uses its internal compatibility renderer and keeps the app API stable.

## Current Backend Matrix

| Backend | Status | Notes |
| --- | --- | --- |
| Auto | Stable | Tries Vulkan first, then Direct3D 12 on Windows or Metal on Apple platforms, then compatibility. |
| Vulkan | Default native target/Accelerated renderer | Uses a native Vulkan instance, surface, swapchain, render pass, command buffers, glslang-built SPIR-V shader pipelines, text atlas sampling, instanced rounded-rectangle/shadow batching, batched text draws by atlas, borders, per-frame dynamic resource pages, and presentation. |
| Direct3D 12 | Staged | Available as a public backend target on Windows with `FLUXUI_ENABLE_D3D12=ON`; command rendering is staged. Aliases: `Direct12`, `DirectX12`, `D3D12`. |
| Metal | Staged | Available as a public backend target on Apple platforms with `FLUXUI_ENABLE_METAL=ON`; command rendering is staged. |
| Compatibility | Internal fallback | Used when a requested native backend is unavailable. Apps should prefer `Auto` or a native backend. |

DirectX 13 is not exposed as a backend target. FluxUI maps the Windows native
backend to Direct3D 12, which is the documented low-level Direct3D API for this
generation of Windows graphics.

## C++

```cpp
FluxUI::Application app;
app.init("My app", 1280, 720);
```

You can still request a backend explicitly:

```cpp
app.setBackend(FluxUI::RenderBackendType::Vulkan);
app.setBackend(FluxUI::RenderBackendType::Direct3D12);
app.setBackend(FluxUI::RenderBackendType::DirectX12);
app.setBackend(FluxUI::RenderBackendType::Metal);
```

If the requested backend is unavailable or not selectable in the current build,
FluxUI logs a message and uses the compatibility renderer.

For a runtime Vulkan smoke test without keeping the app open:

```powershell
.\build\Release\DataLeakGuard.exe --probe-vulkan
```

To request Vulkan and render a short three-frame app run:

```powershell
.\build\Release\DataLeakGuard.exe --backend=vulkan --frames=3
```

Today this validates Vulkan loader/window/surface/device compatibility, then
the app renders a short run through the native Vulkan swapchain path.

## C ABI

```c
FluxUIApp* app = fluxui_app_create();
fluxui_app_init(app, "C app", 900, 600);
```

Backend capability checks:

```c
int has_vulkan = fluxui_backend_is_compiled(FLUXUI_BACKEND_VULKAN);
int has_d3d12 = fluxui_backend_is_compiled(FLUXUI_BACKEND_DIRECT3D12);
int has_directx12 = fluxui_backend_is_compiled(FLUXUI_BACKEND_DIRECTX12);
int has_metal = fluxui_backend_is_compiled(FLUXUI_BACKEND_METAL);
int can_select_vulkan = fluxui_backend_is_selectable(FLUXUI_BACKEND_VULKAN);
```

## Rust

```rust
extern crate fluxui;

use fluxui::{App, Backend};

let app = App::create()?;
app.set_backend(Backend::Direct3D12);
app.set_backend(Backend::DirectX12);
app.init("Rust app", 900, 600)?;
```

## Zig

```zig
const fluxui = @import("fluxui");

const app = try fluxui.App.create();
_ = app.setBackend(.metal);
try app.init("Zig app", 900, 600);
```

## Build Configuration

```powershell
cmake -S . -B build-vulkan -DFLUXUI_ENABLE_VULKAN=ON -DFLUXUI_DEFAULT_BACKEND=VULKAN
cmake --build build-vulkan --config Release
```

Windows Direct3D 12 preference:

```powershell
cmake -S . -B build-d3d12 -DFLUXUI_ENABLE_D3D12=ON -DFLUXUI_DEFAULT_BACKEND=D3D12
cmake --build build-d3d12 --config Release
```

These names are accepted for the same Windows backend: `D3D12`, `DIRECT12`,
`DIRECTX12`, and `DIRECT3D12`.

Apple Metal preference:

```powershell
cmake -S . -B build-metal -DFLUXUI_ENABLE_METAL=ON -DFLUXUI_DEFAULT_BACKEND=METAL
cmake --build build-metal --config Release
```

The native Vulkan renderer needs Vulkan headers, the Vulkan loader, and glslang
at build time. This repo uses vcpkg packages `vulkan-headers`,
`vulkan-loader`, and `glslang`.
Direct3D 12 is reported as compiled only on Windows builds with
`FLUXUI_ENABLE_D3D12=ON`; Metal is reported as compiled only on Apple builds
with `FLUXUI_ENABLE_METAL=ON`.

## GPUI-Like Direction

The architecture goal is a retained UI tree that emits compact GPU commands,
then lets a backend consume those commands. That keeps application code stable
while the renderer grows from the current Vulkan swapchain path into full
Vulkan, Direct3D 12, and Metal execution.

## Performance Work Landed

The Vulkan path now batches rounded rectangles and CSS box shadows through
mapped instance pages, and batches text vertices by font atlas descriptor set.
Flushes happen only when draw order, scissor state, font atlas, dynamic page, or
frame end requires it, which cuts repeated per-widget command overhead from
common dashboard screens.

Dynamic UI geometry is scheduled per swapchain frame. Each frame owns reusable
mapped pages for rectangle/shadow instances and text vertices, so a new frame
does not overwrite data that another frame may still be using on the GPU. Pages
are reset after that frame's fence is ready and expanded only when a larger
burst of UI geometry needs it.

The retained widget tree also tracks layout dirtiness. Style/layout signatures
and parent bounds are cached, so unchanged subtrees skip layout work until a
style, size, or child mutation marks them dirty. Scroll containers skip update
work for off-screen children with a small overscan window, which is the first
layer of list/table virtualization.

Widget creation through `Widget::add<T>()` uses a lazy per-parent arena-backed
allocator for widget objects and their shared control blocks. Containers can
also pre-size child storage through `reserveChildren()` for dense dashboards,
lists, and tables.

## Implementation Path

The current Vulkan path owns frame begin/end, swapchain presentation, scissor
state, rounded rectangle/shadow shader paths, instanced rect batching, border
rendering, font atlas upload, text sampling, batched text vertex submission, and
per-frame dynamic resource pages.
The next native-backend steps are:

- data-source virtualization for very large lists/tables
- persistent command/resource scheduling across backend APIs
- swapchain resize hardening and GPU timing probes
- Direct3D 12 and Metal command parity

Once those are implemented for a native backend, apps already using `Auto`,
`Vulkan`, `Direct3D12`, or `Metal` will not need source changes.
