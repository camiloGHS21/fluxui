// FluxUI GPU mode demo — shows the three GPU strategies and which device wins.
//
//   default            -> integrated GPU (Intel/AMD iGPU); leaves a discrete
//                         RTX free. Falls back to CPU software if no GPU.
//   App::useDiscreteGpu() -> force the discrete GPU
//   App::gameMode()       -> discrete GPU + uncapped max-FPS pacing (games)
//
// Pick the mode with an arg:  demo_gpu_modes [default|discrete|game]
// Or override anything at runtime:  FLUXUI_GPU=game ./demo_gpu_modes
#include <fluxui/dsl.h>
#include <string>
#include <iostream>
using namespace fluxui;

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "default";

    // GPU selection must happen BEFORE the App is constructed.
    if (mode == "discrete")      App::useDiscreteGpu();
    else if (mode == "game")     App::gameMode();      // discrete + max FPS
    else                          App::useIntegratedGpu();  // (also the default)

    App app(1100, 700, "FluxUI GPU Modes");
    std::cout << "Requested mode: " << mode << "\n";
    std::cout << "Active GPU:     " << app.gpuName() << "\n";

    app.addCSS(R"(
        .root { display:flex; align-items:center; justify-content:center;
                background:#0d1117; color:#e6edf3; }
        .card { display:flex; flex-direction:column; gap:12px; padding:32px;
                background:#161b22; border:1px solid #30363d; border-radius:14px; }
        h1 { font-size:26px; font-weight:700; color:#58a6ff; }
        .mono { font-family:monospace; color:#9da7b3; }
    )");

    static auto gpuName = State<std::string>(app.gpuName());
    app.setRoot(
        Div({
            H1("GPU Mode: " + mode),
            Text([]{ return "Active device: " + gpuName.get(); }).className("mono"),
            P("Default uses the integrated GPU so a discrete RTX stays free for "
              "games. Use gameMode() for the discrete GPU at max FPS.").className("mono"),
        }).className("card")
    );
    return app.run();
}
