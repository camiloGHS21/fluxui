# FluxUI FFI

FluxUI is written in C++, but it now exposes a C ABI in `fluxui/fluxui_c.h`.
Use that ABI for C, Rust, Zig, Java/JNI, and any language that can call C
functions.

## Build

```powershell
cmake --build build --config Release
```

This produces:

- `build/Release/fluxui.lib` for C++ static linking.
- `build/Release/fluxui_shared.dll` plus its import library for C/Rust/Zig FFI on Windows.
- `build/Release/fluxui_java.dll` for Java/JNI bindings when a JDK is available.
- `build/Release/DataLeakGuard.exe` for the demo app.

## C++

C++ code can keep using the native API:

```cpp
#include "fluxui/FluxUI.h"
```

For binary-stable plugin style integrations, C++ can also include:

```cpp
#include "fluxui/fluxui_c.h"
```

## C

C uses `fluxui/fluxui_c.h` directly. The handles are opaque:

```c
FluxUIApp* app = fluxui_app_create();
fluxui_app_init(app, "C app", 900, 600);
FluxUIWidget* root = fluxui_app_root(app);
fluxui_widget_add_text(root, "Hello from C", "title");
fluxui_app_run(app);
fluxui_app_destroy(app);
```

## Events And Routes

The C ABI mirrors the C++ event bus and router:

```c
static void on_route(FluxUIApp* app, const FluxUIEvent* event, void* user_data) {
    (void)app;
    (void)user_data;
    printf("route: %s\n", event->route);
}

static void build_dashboard(FluxUIApp* app,
                            FluxUIWidget* view,
                            const char* route,
                            void* user_data) {
    (void)app;
    (void)route;
    (void)user_data;
    fluxui_widget_add_text(view, "Dashboard", "page-title");
}

fluxui_app_on_event(app, FLUXUI_EVENT_ROUTE_CHANGED, on_route, NULL);
fluxui_app_add_route(app, "/dashboard", build_dashboard, NULL);
fluxui_app_navigate(app, "/dashboard");
```

Apps can call `fluxui_app_render_route(app, container)` during their update
callback when `fluxui_app_route_dirty(app)` is true. Input, text, mouse, widget
click, route, and custom events use the same listener API.

## Rust

Rust can import FluxUI as a normal `rustc` crate through `bindings/rust/lib.rs`:

```rust
extern crate fluxui;

use fluxui::{App, Backend};
```

Build the reusable crate and then compile an app against it:

```powershell
rustc --crate-name fluxui --crate-type rlib bindings\rust\lib.rs -L native=build\Release -o build\Release\libfluxui.rlib
rustc examples\rust\minimal.rs --extern fluxui=build\Release\libfluxui.rlib -L native=build\Release -o build\Release\fluxui_rust_minimal.exe
```

The Rust wrapper owns the app handle, exposes safe constructors, and keeps raw C handles available through `raw()` for advanced integrations.
It also exposes `Backend::Auto`, `Backend::Vulkan`, `Backend::Direct3D12`, `Backend::DirectX12`, `Backend::Metal`, and `Backend::Compatibility` through `app.set_backend(...)`.

## Zig

Zig can import FluxUI as a named module through `bindings/zig/fluxui.zig`:

```zig
const fluxui = @import("fluxui");
```

Compile the example with the module wired into Zig's import table:

```powershell
zig build-exe --dep fluxui "-Mroot=examples/zig/minimal.zig" "-Mfluxui=bindings/zig/fluxui.zig" -I fluxui\include -L build\Release -lfluxui_shared --cache-dir .zig-cache-local --global-cache-dir .zig-cache-global "-femit-bin=build/Release/fluxui_zig_minimal.exe"
```

PowerShell users should keep the `-M...=.zig` and `-femit-bin=...exe` arguments quoted so Zig receives each one as a single argument.
The Zig wrapper exposes backend selection with `app.setBackend(.auto)`, `.vulkan`, `.direct3d12`, `.metal`, or `.compatibility`.

## Java

Java uses a thin JNI layer in `bindings/java/native/fluxui_jni.cpp` and the
high-level wrapper classes in `bindings/java/io/fluxui`.

```java
import io.fluxui.*;

try (App app = App.create()) {
    app.setBackend(Backend.AUTO);
    app.init("FluxUI Java", 900, 600);
    Widget root = app.root();
    root.addText("Hello from Java", "title");
    root.addButton("Close", "button").setOnClick(app::stop);
    app.setUpdateCallback(deltaTime -> {
        // update app state from the native run loop
    });
    app.addAction("app.close", Keys.ESCAPE, Modifiers.NONE, action -> app.stop());
    app.run();
}
```

Routes are available from Java too:

```java
app.addRoute("/dashboard", (view, route) -> {
    view.addText("Dashboard", "title");
});
app.navigate("/dashboard");
app.renderRoute(app.root());
```

Compile the Java classes and example:

```powershell
$sources = @(Get-ChildItem bindings\java\io\fluxui,examples\java -Filter *.java | ForEach-Object FullName)
javac -d build\java\classes $sources
java -cp build\java\classes Smoke build\Release
java -cp build\java\classes ActionSmoke build\Release
java -cp build\java\classes AutoClose build\Release
java -cp build\java\classes Minimal build\Release
```

The optional path argument tells `FluxUI.loadFrom(...)` where to load
`fluxui_shared.dll` and `fluxui_java.dll`.

Run Rust, Zig, and Java examples from a directory where `fluxui_shared.dll`,
`SDL2.dll`, and the assets your app needs are available.

## ABI Rules

- Treat `FluxUIApp*` and `FluxUIWidget*` as opaque handles.
- Widget handles are owned by the app tree. They become invalid after clearing or rebuilding their parent children.
- Strings passed into FluxUI are copied unless the function explicitly returns `const char*`.
- The pointer from `fluxui_text_input_get_value` is owned by FluxUI and remains valid only until that widget value changes or the widget is destroyed.
- Browser-style controls can be created with typed text inputs plus `fluxui_widget_add_checkbox`, `fluxui_widget_add_radio`, `fluxui_widget_add_range`, `fluxui_widget_add_select`, `fluxui_widget_add_option`, `fluxui_widget_add_details`, `fluxui_widget_add_summary`, `fluxui_widget_add_dialog`, `fluxui_widget_add_meter`, and `fluxui_widget_add_progress_element`.
- Prefer CSS classes for production layouts. Inline style helpers are meant for small apps and FFI examples.
- Route callbacks receive a container for the active page. Widget handles from a
  previous render become invalid after the route container is rebuilt.
- Java `Widget` wrappers are lightweight handles over native widgets; keep the
  owning `App` alive for as long as callbacks or widgets can be used.
