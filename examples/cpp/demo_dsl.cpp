// FluxUI Declarative DSL Demo — the user's requested API style
#include <fluxui/dsl.h>
using namespace fluxui;

int main() {
    App app;
    app.addCSS(R"(
        .app { width: 100%; height: 100%; }
        .sidebar { width: 250px; background-color: #111115; padding: 16px; gap: 8px; }
        .sidebar button { padding: 10px; border-radius: 6px; color: #ccc; }
        .sidebar button:hover { background-color: #222; }
        .content { flex-grow: 1; padding: 24px; gap: 16px; }
        .h1 { font-size: 24px; font-weight: 700; }
        .metrics { gap: 16px; }
        .metric-card { background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }
        .primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }
        .primary:hover { background-color: #2563eb; }
    )");

    auto devices = State<int>(128);

    app.setRoot(
        Row({
            Sidebar({
                NavItem("Dashboard"),
                NavItem("Dispositivos"),
                NavItem("Backups"),
                NavItem("Seguridad")
            }).className("sidebar"),

            Column({
                Text("CompanyGuard").className("h1"),
                Row({
                    Card({
                        Text("Equipos activos"),
                        Text([&] { return std::to_string(devices.get()); })
                    }).className("metric-card"),
                    Card({
                        Text("Alertas"),
                        Text("7")
                    }).className("metric-card")
                }).className("metrics"),
                Button("Escanear ahora")
                    .className("primary")
                    .onClick([&] {
                        devices.set(devices.get() + 1);
                    })
            }).className("content")
        }).className("app")
    );

    return app.run();
}
