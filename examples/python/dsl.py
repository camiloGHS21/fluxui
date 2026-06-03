"""FluxUI Declarative DSL example in Python — modern HTML/Blink-named API."""
import os
import sys

# Allow running directly from the repo root.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "bindings", "python"))

import fluxui


def main():
    app = fluxui.DslApp(1200, 800, "CompanyGuard")
    app.add_css(
        """
        .app { display: flex; flex-direction: row; width: 100%; height: 100%; }
        .sidebar { display: flex; flex-direction: column; width: 250px; background-color: #111115; padding: 16px; gap: 8px; }
        .content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; gap: 16px; }
        h1 { font-size: 24px; font-weight: 700; }
        .metrics { display: flex; flex-direction: row; gap: 16px; }
        .metric-card { display: flex; flex-direction: column; background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }
        .primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }
        """
    )

    devices = fluxui.State(128)

    app.set_root(
        fluxui.Div([
            fluxui.Nav([
                fluxui.Button("Dashboard"),
                fluxui.Button("Dispositivos"),
                fluxui.Button("Backups"),
                fluxui.Button("Seguridad"),
            ]).cls("sidebar"),

            fluxui.Div([
                fluxui.H1("CompanyGuard"),
                fluxui.Div([
                    fluxui.Div([
                        fluxui.Span("Equipos activos"),
                        fluxui.TextFn(lambda: str(devices.get())),
                    ]).cls("metric-card"),
                    fluxui.Div([
                        fluxui.Span("Alertas"),
                        fluxui.Span("7"),
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
