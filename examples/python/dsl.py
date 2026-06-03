"""FluxUI Declarative DSL example in Python — mirrors the C++ demo_dsl.cpp."""
import os
import sys

# Allow running directly from the repo root.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "bindings", "python"))

import fluxui


def main():
    app = fluxui.DslApp(1200, 800, "CompanyGuard")
    app.add_css(
        """
        .app { width: 100%; height: 100%; }
        .sidebar { width: 250px; background-color: #111115; padding: 16px; gap: 8px; }
        .content { flex-grow: 1; padding: 24px; gap: 16px; }
        .h1 { font-size: 24px; font-weight: 700; }
        .metric-card { background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }
        .primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }
        """
    )

    devices = fluxui.State(128)

    app.set_root(
        fluxui.Row([
            fluxui.Sidebar([
                fluxui.NavItem("Dashboard"),
                fluxui.NavItem("Dispositivos"),
                fluxui.NavItem("Backups"),
                fluxui.NavItem("Seguridad"),
            ]).cls("sidebar"),

            fluxui.Column([
                fluxui.Text("CompanyGuard").cls("h1"),
                fluxui.Row([
                    fluxui.Card([
                        fluxui.Text("Equipos activos"),
                        fluxui.TextFn(lambda: str(devices.get())),
                    ]).cls("metric-card"),
                    fluxui.Card([
                        fluxui.Text("Alertas"),
                        fluxui.Text("7"),
                    ]).cls("metric-card"),
                ]).cls("metrics"),
                fluxui.Button("Escanear ahora")
                    .cls("primary")
                    .on_click(lambda: devices.set(devices.get() + 1)),
            ]).cls("content"),
        ]).cls("app")
    )

    app.run_reactive()


if __name__ == "__main__":
    main()
