#include "fluxui/FluxUI.h"
#include <iostream>
#include <vector>
#include <string>

int main() {
    FluxUI::Application app;
    if (!app.init("FluxUI Native Table Layout Parity Test", 1100, 800, FluxUI::RenderBackendType::Auto)) {
        return 1;
    }

    if (!app.renderer().loadDefaultFont(16.0f)) {
        app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    }
    app.renderer().warmFontCache(std::vector<float>{14.0f, 16.0f, 20.0f, 28.0f});
    app.renderer().releaseFontSources();
    
    // Curated premium HSL-tailored dark mode styling
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #0b0f19; padding: 40px; gap: 24px; align-items: center; }"
        ".header-section { display: flex; flex-direction: column; align-items: center; gap: 8px; }"
        ".title { font-size: 28px; font-weight: 700; color: #f8fafc; font-family: 'Segoe UI', Outfit, sans-serif; }"
        ".subtitle { font-size: 14px; color: #64748b; text-align: center; max-width: 600px; }"
        ".table-container { display: flex; flex-direction: column; background-color: #111827; border-radius: 16px; padding: 24px; border: 1px solid #1f2937; width: 900px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); }"
        
        // CSS rules setting up the Display types for each tabular component
        ".my-table { display: table; width: 100%; border-collapse: collapse; border-top: 1px solid #1f2937; border-left: 1px solid #1f2937; }"
        ".my-thead { display: table-header-group; background-color: #1f2937; }"
        ".my-tbody { display: table-row-group; }"
        ".my-tfoot { display: table-footer-group; background-color: #111827; border-top: 2px solid #374151; }"
        ".my-tr { display: table-row; }"
        ".my-tr:hover { background-color: #1e293b; }"
        ".my-th { display: table-cell; font-weight: 700; color: #f1f5f9; padding: 14px 16px; text-align: left; font-size: 14px; border-bottom: 2px solid #374151; border-right: 1px solid #1f2937; }"
        ".my-td { display: table-cell; color: #cbd5e1; padding: 14px 16px; font-size: 14px; vertical-align: middle; border-bottom: 1px solid #1f2937; border-right: 1px solid #1f2937; }"
        
        // Utility colors and designs
        ".text-right { text-align: right; }"
        ".text-center { text-align: center; }"
        ".badge { background-color: #0284c7; color: #ffffff; padding: 4px 8px; border-radius: 6px; font-size: 11px; font-weight: 600; display: inline-block; }"
        ".badge-success { background-color: #059669; }"
        ".badge-warning { background-color: #d97706; }"
        
        // Buttons
        ".button-bar { display: flex; flex-direction: row; gap: 16px; margin-top: 10px; }"
        ".btn { width: 180px; height: 42px; border-radius: 8px; font-weight: 600; font-size: 14px; display: flex; justify-content: center; align-items: center; border: none; cursor: pointer; }"
        ".btn-primary { background-color: #6366f1; color: #ffffff; }"
        ".btn-primary:hover { background-color: #4f46e5; }"
        ".btn-secondary { background-color: #374151; color: #cbd5e1; }"
        ".btn-secondary:hover { background-color: #4b5563; }"
    );

    auto* root = app.root();
    
    auto* header = root->element("div", "", "header-section");
    header->add<FluxUI::Text>("Native Table Layout Solver Engine", "title");
    header->add<FluxUI::Text>("Premium Blink-style multi-pass layout solver displaying high fidelity tables with nested row groups, dynamic sizing, rowspan, and colspan support.", "subtitle");

    auto* container = root->element("div", "", "table-container");

    // Create custom W3C-structured HTML-like table
    auto* table = container->element("div", "", "my-table");
    
    // Header Group
    auto* thead = table->element("div", "", "my-thead");
    auto* thr = thead->element("div", "", "my-tr");
    thr->element("div", "Service ID", "my-th");
    thr->element("div", "Description", "my-th");
    thr->element("div", "Priority", "my-th");
    thr->element("div", "Usage Scale", "my-th");
    thr->element("div", "Unit Price", "my-th text-right");
    thr->element("div", "Total Cost", "my-th text-right");

    // Body Group
    auto* tbody = table->element("div", "", "my-tbody");
    
    // Row 1: Normal row
    auto* r1 = tbody->element("div", "", "my-tr");
    r1->element("div", "SRV-01", "my-td");
    r1->element("div", "Compute Instances (Shared-Core CPU)", "my-td");
    auto* b1 = r1->element("div", "", "my-td");
    b1->element("span", "Standard", "badge badge-success");
    r1->element("div", "120 Hrs", "my-td");
    r1->element("div", "$0.024 / hr", "my-td text-right");
    r1->element("div", "$2.88", "my-td text-right");

    // Row 2 & 3: Rowspan demonstration!
    // Rowspan cell spans 2 rows vertically
    auto* r2 = tbody->element("div", "", "my-tr");
    r2->element("div", "SRV-02", "my-td");
    
    // This td spans 2 rows
    auto* spanTd = r2->element("div", "Enterprise Storage Cluster & Automated Backups Strategy (High IOPS Solid State)", "my-td");
    spanTd->setAttribute("rowspan", "2");
    
    auto* b2 = r2->element("div", "", "my-td");
    b2->element("span", "Critical", "badge badge-warning");
    r2->element("div", "1.2 TB", "my-td");
    r2->element("div", "$0.150 / GB", "my-td text-right");
    r2->element("div", "$180.00", "my-td text-right");

    auto* r3 = tbody->element("div", "", "my-tr");
    r3->element("div", "SRV-03", "my-td");
    // cell of index 1 (Description) is spanned from r2!
    auto* b3 = r3->element("div", "", "my-td");
    b3->element("span", "Standard", "badge badge-success");
    r3->element("div", "0.8 TB", "my-td");
    r3->element("div", "$0.100 / GB", "my-td text-right");
    r3->element("div", "$80.00", "my-td text-right");

    // Row 4: Normal row
    auto* r4 = tbody->element("div", "", "my-tr");
    r4->element("div", "SRV-04", "my-td");
    r4->element("div", "Load Balancer & Managed DDoS Shielding", "my-td");
    auto* b4 = r4->element("div", "", "my-td");
    b4->element("span", "High", "badge badge-warning");
    r4->element("div", "720 Hrs", "my-td");
    r4->element("div", "$0.040 / hr", "my-td text-right");
    r4->element("div", "$28.80", "my-td text-right");

    // Footer Group: Colspan demonstration!
    auto* tfoot = table->element("div", "", "my-tfoot");
    auto* ftr = tfoot->element("div", "", "my-tr");
    
    // This footer cell spans 5 columns horizontally
    auto* totalLabel = ftr->element("div", "Aggregated Infrastructural Charges", "my-td");
    totalLabel->setAttribute("colspan", "5");
    totalLabel->css("font-weight: 700; color: #f1f5f9;");
    
    auto* totalPrice = ftr->element("div", "$291.68", "my-td text-right");
    totalPrice->css("font-weight: 700; color: #6366f1; font-size: 16px;");

    // Interactivity Control Bar
    auto* btnContainer = root->element("div", "", "button-bar");
    auto* btnToggleSpan = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Toggle Row Spanning", "btn btn-primary"));
    btnToggleSpan->onClick = [=]() {
        static bool spanned = true;
        spanned = !spanned;
        auto* spanText = static_cast<FluxUI::Text*>(spanTd);
        if (spanned) {
            spanTd->setAttribute("rowspan", "2");
            spanText->content = "Enterprise Storage Cluster & Automated Backups Strategy (High IOPS Solid State)";
        } else {
            spanTd->setAttribute("rowspan", "1");
            spanText->content = "Enterprise Storage Cluster Only (Low IOPS Solid State)";
        }
        spanTd->markLayoutDirty();
        std::cout << "Dynamic Table Layout Updated: toggled rowspan." << std::endl;
    };

    auto* btnColSpan = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Expand Colspan", "btn btn-secondary"));
    btnColSpan->onClick = [=]() {
        static bool expanded = false;
        expanded = !expanded;
        auto* labelText = static_cast<FluxUI::Text*>(totalLabel);
        if (expanded) {
            totalLabel->setAttribute("colspan", "4");
            labelText->content = "Subtotal Cost (Calculated)";
        } else {
            totalLabel->setAttribute("colspan", "5");
            labelText->content = "Aggregated Infrastructural Charges";
        }
        totalLabel->markLayoutDirty();
        std::cout << "Dynamic Table Layout Updated: toggled colspan." << std::endl;
    };

    app.run();
    app.shutdown();
    return 0;
}
