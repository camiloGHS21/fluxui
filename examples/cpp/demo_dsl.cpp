// FluxUI Declarative DSL Demo — modern HTML/Blink-named API with reactive State.
#include <fluxui/dsl.h>
using namespace fluxui;

int main() {
    App app(1200, 800, "CompanyGuard");
    app.addCSS(R"(
        .app { display: flex; flex-direction: row; width: 100%; height: 100%; }
        .sidebar { display: flex; flex-direction: column; width: 250px; background-color: #111115; padding: 16px; gap: 8px; }
        .sidebar button { padding: 10px; border-radius: 6px; color: #ccc; }
        .sidebar button:hover { background-color: #222; }
        .content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; gap: 16px; }
        h1 { font-size: 24px; font-weight: 700; }
        .metrics { display: flex; flex-direction: row; gap: 16px; }
        .metric-card { display: flex; flex-direction: column; background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }
        .primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }
        .primary:hover { background-color: #2563eb; }
    )");

    // State lives right here — no separate file needed.
    static auto devices = State<int>(128);

    app.setRoot(
        Div({
            Nav({
                Button("Dashboard"),
                Button("Dispositivos"),
                Button("Backups"),
                Button("Seguridad")
            }).className("sidebar"),

            Div({
                H1("CompanyGuard"),
                Div({
                    Div({
                        Span("Equipos activos"),
                        Text([&]{ return std::to_string(devices.get()); })  // reactive
                    }).className("metric-card"),
                    Div({
                        Span("Alertas"),
                        Span("7")
                    }).className("metric-card")
                }).className("metrics"),
                Button("Escanear ahora")
                    .className("primary")
                    .onClick([&]{ ++devices; })  // ← shorthand operator
            }).className("content")
        }).className("app")
    );

    return app.run();
}
