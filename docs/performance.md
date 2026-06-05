# FluxUI Performance & GPU Selection

FluxUI is designed to be light on the CPU, fast to start, and to pick the right
GPU automatically — including running with **no GPU at all** (pure-CPU software
rendering) when needed.

## GPU selection (integrated-first, like a good desktop app)

On a machine with both an integrated GPU (Intel/AMD iGPU) and a discrete card
(e.g. an NVIDIA RTX), FluxUI **drives the UI on the integrated GPU by default**.
That keeps the power-hungry discrete card free and cool for games / compute,
exactly the behavior you want from a UI framework.

Selection order:

1. The GPU you ask for (see below), else
2. The **integrated** GPU (default / power-saving), else
3. The **discrete** GPU, else
4. The **CPU software** rasterizer (no GPU required).

### Choosing the GPU

C++ (set before constructing the `App`):

```cpp
fluxui::App::useDiscreteGpu();        // GPU-heavy UI -> use the RTX
// fluxui::App::useIntegratedGpu();   // force the iGPU (default)
fluxui::App app(1200, 800, "MyApp");
```

Per language:

| Language | API |
|---|---|
| C++    | `App::useIntegratedGpu()` / `App::useDiscreteGpu()` / `App::preferGpu(...)` |
| Go     | `app.SetGpuPreference(fluxui.GpuDiscrete)` (after `CreateApp`, before `Init`) |
| Python | `fluxui.DslApp(gpu=fluxui.App.GPU_DISCRETE)` |
| Java   | `app.useDiscreteGpu()` / `app.useIntegratedGpu()` (before `init`) |
| Zig    | `app.setGpuPreference(.discrete)` (before `init`) |
| Rust   | `app.set_gpu_preference(fluxui::GpuPreference::Discrete)` (before `init`) |

### Runtime override

The `FLUXUI_GPU` environment variable overrides the app's choice:

```bash
FLUXUI_GPU=integrated   # iGPU / power saving
FLUXUI_GPU=discrete     # dGPU / performance
FLUXUI_BACKEND=cpu      # force the CPU software rasterizer (no GPU)
```

`app.gpuName()` (C++) / `ActiveGpuName()` (Go) / `active_gpu_name()` (Python) /
`activeGpuName()` (Java/Zig) / `active_gpu_name()` (Rust) returns the device
actually selected.

## Power-aware frame pacing

FluxUI never spins the CPU on a static screen. The run loop only redraws when
something actually changes, and paces frames by power state:

- **AC + GPU + focused:** full rate (up to the build's `FLUXUI_TARGET_FPS`).
- **On battery:** throttled to the battery tier.
- **CPU/software backend:** capped to the battery tier.
- **Window backgrounded/minimized:** idle tier.

Profiles: `Auto` (default), `HighPerformance`, `Balanced`, `PowerSaver`
(e.g. C++ `app.powerSaver()`).

## Benchmarks

Three headless benchmarks (no window/GPU needed, stable for CI):

```bash
cmake --build build --config Release --target benchmark benchmark_tree benchmark_frame
./build/Release/benchmark.exe         # CSS selector resolution micro-bench
./build/Release/benchmark_tree.exe    # per-widget restyle (warm + cold cache)
./build/Release/benchmark_frame.exe   # full per-frame pipeline (style+layout+paint)
```

`benchmark_frame` reports, per scenario, the cost of one full frame of
style recalc + layout + paint-command generation over a realistic
dashboard tree, plus the implied FPS headroom. Example shape of the output:

```
scenario                            widgets   style/ms  layout/ms   paint/ms   total/ms      fps
small  (dashboard, ~20 cards)           131      0.21       0.00       0.21       0.42     2374
medium (dashboard, ~100 cards)          611      1.31       0.02       1.10       2.43      412
large  (dashboard, ~500 cards)         3011      8.16       0.16       7.49      15.81       63
```

Note: these are the *headless pipeline* costs. On screen, the real frame rate
is additionally bounded by GPU/software present + vsync. In steady state most
widgets hit the style cache, so per-frame style cost is far lower than the
forced full-restyle numbers above.
