// FluxUI Hot-Reload Demo — edit the CSS file while the app runs and watch it
// re-style instantly. No recompile, no relaunch.
//
// How to try it:
//   1. Build + run this demo.
//   2. Open assets/styles/hotreload_demo.css in your editor.
//   3. Change a color / size / radius and save. The window updates live.
//
// The whole feature is two calls: loadStyle(path) + hotReload().
#include <fluxui/dsl.h>
#include <fstream>
using namespace fluxui;

int main() {
    // Ensure a CSS file exists next to the app so there's something to edit.
    const char* cssPath = "assets/styles/hotreload_demo.css";
    {
        std::ifstream probe(cssPath);
        if (!probe.good()) {
            std::ofstream f(cssPath);
            f << ".root { background-color:#0d1117; }\n"
                 ".card { background-color:#161b22; color:#e6edf3; padding:28px;\n"
                 "        border-radius:14px; border:1px solid #30363d; margin:40px; gap:14px;\n"
                 "        display:flex; flex-direction:column; }\n"
                 "h1 { font-size:30px; font-weight:700; color:#58a6ff; }\n"
                 "p  { font-size:16px; color:#9da7b3; }\n"
                 ".primary { background-color:#238636; color:white; padding:12px 22px;\n"
                 "           border-radius:8px; font-size:15px; }\n";
        }
    }

    App app(900, 640, "FluxUI Hot-Reload");
    app.loadStyle(cssPath);
    app.hotReload();   // ← watch cssPath and live-reload on save

    app.setRoot(
        Div({
            H1("Hot-Reload is live"),
            P("Edit assets/styles/hotreload_demo.css and save."),
            P("Colors, sizes, spacing and radii update instantly."),
            Button("A styled button").className("primary"),
        }).className("card")
    );

    return app.run();
}
