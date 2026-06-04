"""FluxUI Python file-based routing demo — like Next.js, zero codegen.

app.auto_routes("views") scans the views/ folder at runtime and registers a
route per file: views/dashboard.py -> "/dashboard", views/settings.py -> "/settings".
"""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "../../bindings/python"))

import fluxui


def main():
    app = fluxui.DslApp(1000, 700, "FluxUI Python Router Demo")
    app.add_css("""
        .app { display: flex; flex-direction: row; width: 100%; height: 100%; }
        .sidebar { display: flex; flex-direction: column; width: 220px; background-color: #111; padding: 16px; gap: 8px; }
        .content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; }
        .title { font-size: 24px; font-weight: 700; }
    """)

    # File-based routing: scan views/ and register every view automatically.
    app.auto_routes(os.path.join(os.path.dirname(__file__), "views"))

    app.set_layout(lambda content: fluxui.Div([
        fluxui.Nav([
            fluxui.Button("Dashboard").on_click(lambda: app.navigate("/dashboard")),
            fluxui.Button("Settings").on_click(lambda: app.navigate("/settings")),
        ]).cls("sidebar"),
        fluxui.Div([content] if content else []).cls("content"),
    ]).cls("app"))

    app.run_reactive()


if __name__ == "__main__":
    main()
