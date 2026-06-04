// DataLeak Guard — Blink UI Parity Sandbox view
#pragma once
#include "../components.h"

namespace dlg {

inline Element BlinkView() {
    return Div({
        TopBar("Blink UI", "Scoped native Chromium/Blink element sandbox", false),
        Div({
            H1("Blink Native Element Parity"),
            P("This sandbox uses browser-like user-agent styles inside the DataLeak Guard shell."),
            H2("Typography"),
            H1("Heading 1"), H2("Heading 2"), H3("Heading 3"),
            H4("Heading 4"), H5("Heading 5"), H6("Heading 6"),
            P("Standard paragraph with normal document flow."),
            Div({ Strong("Bold "), Em("Italic "), Small("Small "), Code("code()") }),
            Pre("class Demo {\n    void run() { std::cout << \"Hello\"; }\n};"),
            H2("Lists"),
            Ul({ Li({Span("First item")}), Li({Span("Second item")}), Li({Span("Third item")}) }),
            Ol({ Li({Span("Step one")}), Li({Span("Step two")}), Li({Span("Step three")}) }),
            H2("Form Controls"),
            Form({
                Fieldset({
                    Legend("Standard Inputs"),
                    Label("Text:"), Input("Enter value..."), Br(),
                    Label("Password:"), Input("password", "Secret"), Br(),
                    Label("Checkbox:"), Checkbox(true), Br(),
                    Label("Radio:"), Radio(true, "demo"), Radio(false, "demo"), Br(),
                    Label("Range:"), Range(0.6f, 0.0f, 1.0f, 0.01f), Br(),
                    Label("Select:"), Select({Option("Blink"), Option("FluxUI"), Option("Skia")}), Br(),
                    Button("Submit"), Button("Reset")
                })
            }),
            H2("Progress And Meters"),
            Progress(0.65f, 1.0f), Meter(0.7f, 0.0f, 1.0f),
            H2("Disclosure"),
            Details({ Summary("Click to expand"), P("Hidden content revealed.") }),
            Hr(),
            A("Chromium Blink Source", "https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink"),
            Br(),
            P("End of Blink sandbox.")
        }).className("blink-native-doc")
    }).className("main-scroll blink-page-scroll");
}

} // namespace dlg
