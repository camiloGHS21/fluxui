"""
FluxUI - Complete Blink/Chromium Default UI Element Showcase
============================================================
ALL FluxUI widget types rendered with ZERO custom stylesheets, built with the
modern HTML/Blink-named declarative DSL. Every element uses the built-in
Blink/Chromium User Agent default styles baked into the core engine.

Reference: chromium/src/third_party/blink/renderer/core/html/resources/html.css
"""
import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), "../../bindings/python"))

import fluxui
from fluxui import (
    Div, P, H1, H2, H3, H4, H5, H6, Span, Strong, Em, Small, Code, Pre, A,
    Hr, Br, Form, Fieldset, Legend, Label, Input, PasswordInput, TextArea,
    Checkbox, Radio, Range, Select, Option, Button, Meter, Progress,
    Details, Summary, Dialog,
)


def main():
    # NO custom CSS — pure built-in Blink UA defaults.
    app = fluxui.DslApp(1024, 768, "FluxUI - All Blink Default Elements (No Custom CSS)")

    # Captured native widgets for imperative actions (dialog, reading input).
    refs = {}

    app.set_root(
        Div([
            # --- Typography / headings ---
            H1("FluxUI Blink User Agent Stylesheet Showcase"),
            H2("All Elements with Default Browser Styling"),
            H3("Section: Typography"),
            H4("Heading Level 4"),
            H5("Heading Level 5"),
            H6("Heading Level 6"),
            P("This is a standard paragraph element (<p>) with default Blink margin "
              "and font sizing. No custom CSS was loaded - this uses the built-in "
              "user-agent stylesheet."),
            P("Another paragraph to show proper block-level spacing between <p> elements."),
            Hr(),

            # --- Inline text semantics ---
            H3("Section: Inline Text Elements"),
            Div([
                Strong("Bold/Strong text"), Span("  |  "),
                Em("Italic/Emphasized text"), Span("  |  "),
                Small("Small text"), Span("  |  "),
                Span("Normal span"), Span("  |  "),
                Code("monospace code"),
            ]),
            Hr(),

            # --- Hyperlinks ---
            H3("Section: Hyperlinks"),
            Div([
                A("Chromium Blink Source",
                  "https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink"),
                Span("   "),
                A("FluxUI Documentation", "https://fluxui.dev"),
                Span("   "),
                A("Google", "https://google.com"),
            ]),
            Hr(),

            # --- Form controls ---
            H3("Section: Form Controls"),
            Form([
                Fieldset([
                    Legend("Text Inputs"),
                    Div([Label("Username: "),
                         Input("Enter username...")
                         .on_mount(lambda w: refs.__setitem__("user", w))]),
                    Div([Label("Email: "), Input("email", "name@example.com")]),
                    Div([Label("Password: "), PasswordInput("Enter password...")]),
                ]),
                Fieldset([
                    Legend("Textarea"),
                    Label("Comments:"),
                    TextArea("Type your comments here..."),
                ]),
                Fieldset([
                    Legend("Checkboxes"),
                    Div([Checkbox(True), Label(" Option A (checked by default)")]),
                    Div([Checkbox(False), Label(" Option B (unchecked)")]),
                    Div([Checkbox(False), Label(" Option C (unchecked)")]),
                ]),
                Fieldset([
                    Legend("Radio Buttons"),
                    Div([Radio(True, "color"), Label(" Red")]),
                    Div([Radio(False, "color"), Label(" Green")]),
                    Div([Radio(False, "color"), Label(" Blue")]),
                ]),
                Fieldset([
                    Legend("Range Slider"),
                    Div([Label("Volume: "), Range(50.0, 0.0, 100.0, 1.0)]),
                ]),
                Fieldset([
                    Legend("Select Dropdown"),
                    Div([
                        Label("Choose a fruit: "),
                        Select([
                            Option("Apple"), Option("Banana"),
                            Option("Cherry"), Option("Dragonfruit"),
                        ]),
                    ]),
                ]),
            ]),
            Hr(),

            # --- Buttons ---
            H3("Section: Buttons"),
            Div([
                Button("Submit").on_click(
                    lambda: (print("Submit ->", refs["user"].get_value()),
                             refs["dialog"].dialog_show_modal())),
                Button("Reset"),
                Button("Cancel").on_click(lambda: app.stop()),
                Button("Perform Action"),
            ]),
            Hr(),

            # --- Meter & progress ---
            H3("Section: Meter & Progress"),
            Div([Label("Disk usage (meter): "), Meter(65.0, 0.0, 100.0)]),
            Div([Label("Download (progress): "), Progress(42.0, 100.0)]),
            Hr(),

            # --- Details / summary ---
            H3("Section: Details & Summary"),
            Details([
                Summary("Click to expand: System Information"),
                P("Operating System: Windows 11 Pro"),
                P("Architecture: x86_64"),
                P("Renderer: Vulkan 1.3"),
            ]),
            Details([
                Summary("Click to expand: Build Configuration"),
                P("Compiler: MSVC 18.4"),
                P("Config: Release"),
                P("Backend: Vulkan + OpenGL fallback"),
            ]),
            Hr(),

            # --- Preformatted / code block ---
            H3("Section: Preformatted Text"),
            Pre("// C++ FluxUI code example\n"
                "#include <fluxui/dsl.h>\n"
                "\n"
                "int main() {\n"
                "    fluxui::App app(800, 600, \"Hello\");\n"
                "    app.setRoot(fluxui::H1(\"Hi\"));\n"
                "    return app.run();\n"
                "}"),
            Hr(),

            # --- Dialog ---
            H3("Section: Dialog"),
            Dialog([
                H3("Modal Dialog"),
                P("This is a native <dialog> element rendered with Blink default styles."),
                Button("Close Dialog").on_click(lambda: refs["dialog"].dialog_close()),
            ]).on_mount(lambda w: refs.__setitem__("dialog", w)),
            Button("Open Dialog").on_click(lambda: refs["dialog"].dialog_show_modal()),
            Hr(),

            # --- Misc ---
            H3("Section: Miscellaneous"),
            P("Line before <br>"),
            Br(),
            P("Line after <br>"),
            Hr(),

            P("End of FluxUI Blink UA Stylesheet showcase. All elements above use ZERO custom CSS."),
        ])
    )

    print("Running FluxUI Blink Default Showcase...")
    app.run_reactive()
    print("Shutting down...")
    app.shutdown()


if __name__ == "__main__":
    main()
