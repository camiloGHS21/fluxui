// FluxUI Native Table Layout Parity Test — modern HTML/Blink-named declarative DSL.
#include <fluxui/dsl.h>
#include <iostream>
#include <string>
using namespace fluxui;

int main() {
    App app(1100, 800, "FluxUI Native Table Layout Parity Test");
    app.addCSS(
        ".root { display: flex; flex-direction: column; background-color: #0b0f19; padding: 40px; gap: 24px; align-items: center; }"
        ".header-section { display: flex; flex-direction: column; align-items: center; gap: 8px; }"
        ".title { font-size: 28px; font-weight: 700; color: #f8fafc; font-family: 'Segoe UI', Outfit, sans-serif; }"
        ".subtitle { font-size: 14px; color: #64748b; text-align: center; max-width: 600px; }"
        ".table-container { display: flex; flex-direction: column; background-color: #111827; border-radius: 16px; padding: 24px; border: 1px solid #1f2937; width: 900px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); }"
        ".my-table { display: table; width: 100%; border-collapse: collapse; border-top: 1px solid #1f2937; border-left: 1px solid #1f2937; }"
        ".my-thead { display: table-header-group; background-color: #1e293b; }"
        ".my-tbody { display: table-row-group; }"
        ".my-tfoot { display: table-footer-group; background-color: #111827; border-top: 2px solid #374151; }"
        ".my-tr { display: table-row; }"
        ".my-tr:hover { background-color: #1e293b; }"
        ".my-th { display: table-cell; font-weight: 700; color: #f1f5f9; padding: 14px 16px; text-align: left; font-size: 14px; border-bottom: 2px solid #374151; border-right: 1px solid #1f2937; }"
        ".my-td { display: table-cell; color: #cbd5e1; padding: 14px 16px; font-size: 14px; vertical-align: middle; border-bottom: 1px solid #1f2937; border-right: 1px solid #1f2937; }"
        ".col-id { width: 80px; } .col-desc { width: 360px; } .col-priority { width: 100px; }"
        ".col-scale { width: 90px; } .col-price { width: 100px; } .col-total { width: 110px; }"
        ".text-right { text-align: right; } .text-center { text-align: center; }"
        ".badge { background-color: #0284c7; color: #ffffff; padding: 4px 8px; border-radius: 6px; font-size: 11px; font-weight: 600; display: inline-block; }"
        ".badge-success { background-color: #059669; } .badge-warning { background-color: #d97706; }"
        ".button-bar { display: flex; flex-direction: row; gap: 16px; margin-top: 10px; }"
        ".btn { width: 180px; height: 42px; border-radius: 8px; font-weight: 600; font-size: 14px; display: flex; justify-content: center; align-items: center; cursor: pointer; }"
        ".btn-primary { background-color: #6366f1; color: #ffffff; } .btn-primary:hover { background-color: #4f46e5; }"
        ".btn-secondary { background-color: #374151; color: #cbd5e1; } .btn-secondary:hover { background-color: #4b5563; }"
    );

    // Widgets the toggle buttons mutate, captured at mount time.
    auto spanTd = std::make_shared<FluxUI::Text*>(nullptr);
    auto r3Placeholder = std::make_shared<FluxUI::Widget*>(nullptr);
    auto totalLabel = std::make_shared<FluxUI::Text*>(nullptr);
    auto totalPrice = std::make_shared<FluxUI::Widget*>(nullptr);

    app.setRoot(
        Div({
            Div({
                Span("Native Table Layout Solver Engine").className("title"),
                Span("Premium Blink-style multi-pass layout solver displaying high fidelity "
                     "tables with nested row groups, dynamic sizing, rowspan, and colspan support.")
                    .className("subtitle")
            }).className("header-section"),

            Div({
                El("div", {
                    // Header group
                    El("div", {
                        El("div", { El("div", "Service ID").className("my-th col-id"),
                                    El("div", "Description").className("my-th col-desc"),
                                    El("div", "Priority").className("my-th col-priority"),
                                    El("div", "Usage Scale").className("my-th col-scale"),
                                    El("div", "Unit Price").className("my-th col-price text-right"),
                                    El("div", "Total Cost").className("my-th col-total text-right") })
                            .className("my-tr")
                    }).className("my-thead"),

                    // Body group
                    El("div", {
                        El("div", { El("div", "SRV-01").className("my-td"),
                                    El("div", "Compute Instances (Shared-Core CPU)").className("my-td"),
                                    Div({ Span("Standard").className("badge badge-success") }).className("my-td"),
                                    El("div", "120 Hrs").className("my-td"),
                                    El("div", "$0.024 / hr").className("my-td text-right"),
                                    El("div", "$2.88").className("my-td text-right") }).className("my-tr"),

                        El("div", {
                            El("div", "SRV-02").className("my-td"),
                            El("div", "Enterprise Storage Cluster & Automated Backups Strategy "
                                      "(High IOPS Solid State)")
                                .className("my-td").attr("rowspan", "2")
                                .onMount<FluxUI::Text>([spanTd](FluxUI::Text* t){ *spanTd = t; }),
                            Div({ Span("Critical").className("badge badge-warning") }).className("my-td"),
                            El("div", "1.2 TB").className("my-td"),
                            El("div", "$0.150 / GB").className("my-td text-right"),
                            El("div", "$180.00").className("my-td text-right")
                        }).className("my-tr"),

                        El("div", {
                            El("div", "SRV-03").className("my-td"),
                            El("div", "Enterprise Storage Cluster Only (Low IOPS Solid State)")
                                .className("my-td").visible(false)
                                .onMount([r3Placeholder](FluxUI::Widget* w){ *r3Placeholder = w; }),
                            Div({ Span("Standard").className("badge badge-success") }).className("my-td"),
                            El("div", "0.8 TB").className("my-td"),
                            El("div", "$0.100 / GB").className("my-td text-right"),
                            El("div", "$80.00").className("my-td text-right")
                        }).className("my-tr"),

                        El("div", { El("div", "SRV-04").className("my-td"),
                                    El("div", "Load Balancer & Managed DDoS Shielding").className("my-td"),
                                    Div({ Span("High").className("badge badge-warning") }).className("my-td"),
                                    El("div", "720 Hrs").className("my-td"),
                                    El("div", "$0.040 / hr").className("my-td text-right"),
                                    El("div", "$28.80").className("my-td text-right") }).className("my-tr")
                    }).className("my-tbody"),

                    // Footer group with colspan
                    El("div", {
                        El("div", {
                            El("div", "Aggregated Infrastructural Charges")
                                .className("my-td").attr("colspan", "5")
                                .style("font-weight", "700").style("color", "#f1f5f9")
                                .onMount<FluxUI::Text>([totalLabel](FluxUI::Text* t){ *totalLabel = t; }),
                            El("div", "$291.68")
                                .className("my-td text-right").attr("colspan", "1")
                                .style("font-weight", "700").style("color", "#6366f1").style("font-size", "16px")
                                .onMount([totalPrice](FluxUI::Widget* w){ *totalPrice = w; })
                        }).className("my-tr")
                    }).className("my-tfoot")
                }).className("my-table")
            }).className("table-container"),

            Div({
                Button("Toggle Row Spanning").className("btn btn-primary")
                    .onClick([spanTd, r3Placeholder] {
                        static bool spanned = true;
                        spanned = !spanned;
                        if (!*spanTd || !*r3Placeholder) return;
                        if (spanned) {
                            (*spanTd)->setAttribute("rowspan", "2");
                            (*spanTd)->content = "Enterprise Storage Cluster & Automated Backups Strategy (High IOPS Solid State)";
                            (*r3Placeholder)->visible = false;
                        } else {
                            (*spanTd)->setAttribute("rowspan", "1");
                            (*spanTd)->content = "Enterprise Storage Cluster Only (Low IOPS Solid State)";
                            (*r3Placeholder)->visible = true;
                        }
                        (*spanTd)->markLayoutDirty();
                        (*r3Placeholder)->markLayoutDirty();
                        std::cout << "Dynamic Table Layout Updated: toggled rowspan." << std::endl;
                    }),
                Button("Expand Colspan").className("btn btn-secondary")
                    .onClick([totalLabel, totalPrice] {
                        static bool expanded = false;
                        expanded = !expanded;
                        if (!*totalLabel || !*totalPrice) return;
                        if (expanded) {
                            (*totalLabel)->setAttribute("colspan", "4");
                            (*totalPrice)->setAttribute("colspan", "2");
                            (*totalLabel)->content = "Subtotal Cost (Calculated)";
                        } else {
                            (*totalLabel)->setAttribute("colspan", "5");
                            (*totalPrice)->setAttribute("colspan", "1");
                            (*totalLabel)->content = "Aggregated Infrastructural Charges";
                        }
                        (*totalLabel)->markLayoutDirty();
                        (*totalPrice)->markLayoutDirty();
                        std::cout << "Dynamic Table Layout Updated: toggled colspan." << std::endl;
                    })
            }).className("button-bar")
        }).className("root")
    );

    return app.run();
}
