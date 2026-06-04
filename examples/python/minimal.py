"""FluxUI Python Showcase — modern HTML/Blink-named declarative DSL."""
import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), "../../bindings/python"))

import fluxui


def main():
    app = fluxui.DslApp(900, 650, "FluxUI Python Showcase Console")
    app.add_css("""
        .root { display: flex; flex-direction: column; background: radial-gradient(circle at top, #161a26 0%, #0a0b10 100%); padding: 40px; justify-content: center; align-items: center; font-family: 'Segoe UI', system-ui, sans-serif; }
        .container { display: flex; flex-direction: column; width: 720px; padding: 35px; border-radius: 16px; background: rgba(22, 26, 38, 0.8); border: 1px solid rgba(255, 255, 255, 0.08); gap: 24px; box-shadow: 0 20px 40px rgba(0, 0, 0, 0.55); backdrop-filter: blur(12px); }
        .header { display: flex; flex-direction: column; gap: 8px; border-bottom: 1px solid rgba(255, 255, 255, 0.07); padding-bottom: 20px; }
        h1 { font-size: 28px; font-weight: 800; color: #ffffff; }
        .desc { font-size: 14px; color: rgba(237, 243, 248, 0.7); line-height: 1.6; }
        fieldset { display: flex; flex-direction: column; padding: 24px; border-radius: 10px; background: rgba(10, 11, 16, 0.5); border: 1px solid rgba(255, 255, 255, 0.05); gap: 16px; }
        legend { font-size: 14px; font-weight: 700; color: #6c5ce7; padding: 0 10px; }
        .row { display: flex; flex-direction: row; align-items: center; gap: 15px; }
        label { font-size: 14px; font-weight: 600; color: #a0aec0; width: 100px; }
        input { padding: 11px 16px; border-radius: 8px; background-color: #0b0d13; border: 1px solid rgba(255, 255, 255, 0.12); color: #ffffff; font-size: 14px; width: 320px; }
        input:focus { border-color: #6c5ce7; outline: none; }
        .actions { display: flex; flex-direction: row; align-items: center; gap: 15px; margin-top: 10px; }
        .btn { display: flex; justify-content: center; align-items: center; padding: 12px 24px; border-radius: 8px; font-weight: 600; cursor: pointer; }
        .btn-primary { background: linear-gradient(135deg, #6c5ce7 0%, #a29bfe 100%); color: #ffffff; }
        .btn-danger { background: linear-gradient(135deg, #e53e3e 0%, #f56565 100%); color: #ffffff; }
        a { font-size: 14px; color: #a29bfe; font-weight: 600; }
    """)

    title = fluxui.State("FluxUI Native Style Console")

    # Hold native widget handles captured at mount for imperative actions.
    refs = {}

    app.set_root(
        fluxui.Div([
            fluxui.Div([
                fluxui.Div([
                    fluxui.TextFn(lambda: title.get()),
                ]),
                fluxui.Span("Experience beautiful, high-performance styling and controls "
                            "in native Python.").cls("desc"),
            ]).cls("header"),

            fluxui.Form([
                fluxui.Fieldset([
                    fluxui.Legend("User Information"),
                    fluxui.Div([
                        fluxui.Label("Username:"),
                        fluxui.Input("Enter username...")
                            .on_mount(lambda w: refs.__setitem__("input", w)),
                    ]).cls("row"),
                ]),
            ]).cls("form"),

            fluxui.Div([
                fluxui.Button("Submit Info").cls("btn btn-primary").on_click(
                    lambda: title.set("Hello, " + (refs["input"].get_value() or "guest") + "!")
                ),
                fluxui.A("Visit Chromium Docs",
                         "https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink"),
                fluxui.Button("Exit").cls("btn btn-danger").on_click(lambda: app.stop()),
            ]).cls("actions"),
        ]).cls("root")
    )

    print("Running FluxUI app...")
    app.run_reactive()
    print("Shutting down FluxUI app...")
    app.shutdown()


if __name__ == "__main__":
    main()
