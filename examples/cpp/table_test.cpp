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
        ".table-container { display: flex; flex-direction: column; background-color: #111827; border-radius: 16px; padding: 24px; border: 1px solid #1f2937; width: auto; min-width: 900px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); }"
        
        // CSS rules setting up the Display types for each tabular component
        ".my-table { display: table; width: auto; border-collapse: collapse; border-top: 1px solid #1f2937; border-left: 1px solid #1f2937; }"
        ".my-thead { display: table-header-group; background-color: #1e293b; }"
        ".my-tbody { display: table-row-group; }"
        ".my-tfoot { display: table-footer-group; background-color: #111827; border-top: 2px solid #374151; }"
        ".my-tr { display: table-row; }"
        ".my-tr:hover { background-color: #1e293b; }"
        ".my-th { display: table-cell; font-weight: 700; color: #f1f5f9; padding: 14px 16px; text-align: left; font-size: 14px; border-bottom: 2px solid #374151; border-right: 1px solid #1f2937; }"
        ".my-td { display: table-cell; color: #cbd5e1; padding: 14px 16px; font-size: 14px; vertical-align: middle; border-bottom: 1px solid #1f2937; border-right: 1px solid #1f2937; }"
        
        // Column specific sizing classes to ensure beautiful proportions
        ".col-id { width: 80px; }"
        ".col-desc { width: 360px; }"
        ".col-priority { width: 100px; }"
        ".col-scale { width: 90px; }"
        ".col-price { width: 100px; }"
        ".col-total { width: 110px; }"

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
    auto* table = container->element("table", "", "my-table");
    
    // Header Group
    auto* thead = table->element("thead", "", "my-thead");
    auto* thr = thead->element("tr", "", "my-tr");
    thr->element("th", "Service ID", "my-th col-id");
    thr->element("th", "Description", "my-th col-desc");
    thr->element("th", "Priority", "my-th col-priority");
    thr->element("th", "Usage Scale", "my-th col-scale");
    thr->element("th", "Unit Price", "my-th col-price text-right");
    thr->element("th", "Total Cost", "my-th col-total text-right");

    // Body Group
    auto* tbody = table->element("tbody", "", "my-tbody");
    
    // Row 1: Normal row
    auto* r1 = tbody->element("tr", "", "my-tr");
    r1->element("td", "SRV-01", "my-td");
    r1->element("td", "Compute Instances (Shared-Core CPU)", "my-td");
    auto* b1 = r1->element("td", "", "my-td");
    b1->element("span", "Standard", "badge badge-success");
    r1->element("td", "120 Hrs", "my-td");
    r1->element("td", "$0.024 / hr", "my-td text-right");
    r1->element("td", "$2.88", "my-td text-right");

    // Row 2 & 3: Rowspan demonstration!
    // Rowspan cell spans 2 rows vertically
    auto* r2 = tbody->element("tr", "", "my-tr");
    r2->element("td", "SRV-02", "my-td");
    
    // This td spans 2 rows
    auto* spanTd = r2->element("td", "Enterprise Storage Cluster & Automated Backups Strategy (High IOPS Solid State)", "my-td");
    spanTd->setAttribute("rowspan", "2");
    
    auto* b2 = r2->element("td", "", "my-td");
    b2->element("span", "Critical", "badge badge-warning");
    r2->element("td", "1.2 TB", "my-td");
    r2->element("td", "$0.150 / GB", "my-td text-right");
    r2->element("td", "$180.00", "my-td text-right");

    auto* r3 = tbody->element("tr", "", "my-tr");
    r3->element("td", "SRV-03", "my-td");
    
    // Placeholder cell for Description in Row 3 (used only when rowspan = 1)
    auto* r3_placeholder = r3->element("td", "Automated Backups Strategy (High IOPS Solid State)", "my-td");
    r3_placeholder->visible = false; // Start hidden because rowspan is initially 2
    
    // cell of index 1 (Description) is spanned from r2!
    auto* b3 = r3->element("td", "", "my-td");
    b3->element("span", "Standard", "badge badge-success");
    r3->element("td", "0.8 TB", "my-td");
    r3->element("td", "$0.100 / GB", "my-td text-right");
    r3->element("td", "$80.00", "my-td text-right");

    // Row 4: Normal row
    auto* r4 = tbody->element("tr", "", "my-tr");
    r4->element("td", "SRV-04", "my-td");
    r4->element("td", "Load Balancer & Managed DDoS Shielding", "my-td");
    auto* b4 = r4->element("td", "", "my-td");
    b4->element("span", "High", "badge badge-warning");
    r4->element("td", "720 Hrs", "my-td");
    r4->element("td", "$0.040 / hr", "my-td text-right");
    r4->element("td", "$28.80", "my-td text-right");

    // Footer Group: Colspan demonstration!
    auto* tfoot = table->element("tfoot", "", "my-tfoot");
    auto* ftr = tfoot->element("tr", "", "my-tr");
    
    // This footer cell spans 5 columns horizontally
    auto* totalLabel = ftr->element("td", "Aggregated Infrastructural Charges", "my-td");
    totalLabel->setAttribute("colspan", "5");
    totalLabel->css("font-weight: 700; color: #f1f5f9;");
    
    auto* totalPrice = ftr->element("td", "$291.68", "my-td text-right");
    totalPrice->setAttribute("colspan", "1");
    totalPrice->css("font-weight: 700; color: #6366f1; font-size: 16px;");

    // Interactivity Control Bar
    auto* btnContainer = root->element("div", "", "button-bar");
    auto* btnToggleSpan = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Toggle Row Spanning", "btn btn-primary"));

    // Helper to print widget tree
    std::function<void(FluxUI::Widget*, int)> printWidgetTree = [&](FluxUI::Widget* w, int indent) {
        if (!w) return;
        for (int i = 0; i < indent; ++i) std::cout << "  ";
        std::cout << "Widget type=" << w->type 
                  << " id=" << w->id 
                  << " class=" << w->className 
                  << " visible=" << w->visible 
                  << " colspan=" << w->colspan
                  << " rowspan=" << w->rowspan
                  << " bounds=[" << w->bounds.x << "," << w->bounds.y << "," << w->bounds.w << "," << w->bounds.h << "]";
        auto* txt = dynamic_cast<FluxUI::Text*>(w);
        if (txt) {
            std::cout << " text=\"" << txt->content << "\"";
        }
        std::cout << std::endl;
        for (auto& child : w->children) {
            printWidgetTree(child.get(), indent + 1);
        }
    };

    btnToggleSpan->onClick = [=]() {
        std::cout << "[DEBUG] onClick entered" << std::endl;
        static bool spanned = true;
        spanned = !spanned;
        std::cout << "[DEBUG] spanned = " << spanned << std::endl;
        auto* spanText = static_cast<FluxUI::Text*>(spanTd);
        std::cout << "[DEBUG] spanTd = " << spanTd << ", spanText = " << spanText << std::endl;
        if (spanned) {
            std::cout << "[DEBUG] setting rowspan to 2" << std::endl;
            spanTd->setAttribute("rowspan", "2");
            std::cout << "[DEBUG] setting spanText content" << std::endl;
            spanText->content = "Enterprise Storage Cluster & Automated Backups Strategy (High IOPS Solid State)";
            std::cout << "[DEBUG] setting r3_placeholder visible to false" << std::endl;
            if (r3_placeholder) {
                r3_placeholder->visible = false;
            } else {
                std::cout << "[DEBUG] WARNING: r3_placeholder is NULL!" << std::endl;
            }
        } else {
            std::cout << "[DEBUG] setting rowspan to 1" << std::endl;
            spanTd->setAttribute("rowspan", "1");
            std::cout << "[DEBUG] setting spanText content" << std::endl;
            spanText->content = "Enterprise Storage Cluster Only (Low IOPS Solid State)";
            std::cout << "[DEBUG] setting r3_placeholder visible to true" << std::endl;
            if (r3_placeholder) {
                r3_placeholder->visible = true;
            } else {
                std::cout << "[DEBUG] WARNING: r3_placeholder is NULL!" << std::endl;
            }
        }
        std::cout << "[DEBUG] marking spanTd layout dirty" << std::endl;
        spanTd->markLayoutDirty();
        std::cout << "[DEBUG] marking r3_placeholder layout dirty" << std::endl;
        if (r3_placeholder) {
            r3_placeholder->markLayoutDirty();
        }
        std::cout << "Dynamic Table Layout Updated: toggled rowspan." << std::endl;
    };

    auto* btnColSpan = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Expand Colspan", "btn btn-secondary"));
    btnColSpan->onClick = [=]() {
        static bool expanded = false;
        expanded = !expanded;
        auto* labelText = static_cast<FluxUI::Text*>(totalLabel);
        if (expanded) {
            totalLabel->setAttribute("colspan", "4");
            totalPrice->setAttribute("colspan", "2"); // Keep total columns = 6!
            labelText->content = "Subtotal Cost (Calculated)";
        } else {
            totalLabel->setAttribute("colspan", "5");
            totalPrice->setAttribute("colspan", "1"); // Keep total columns = 6!
            labelText->content = "Aggregated Infrastructural Charges";
        }
        totalLabel->markLayoutDirty();
        totalPrice->markLayoutDirty();
        std::cout << "Dynamic Table Layout Updated: toggled colspan." << std::endl;
    };

    bool isSimulation = (std::getenv("FLUXUI_SIMULATION") != nullptr);
    if (isSimulation) {
        auto startTime = std::chrono::steady_clock::now();
        static bool spanToggled = false;
        static bool colspanToggled = false;
        static bool initialPrinted = false;
        app.onUpdate = [=, &app](float dt) mutable {
            app.requestRedraw();
            auto now = std::chrono::steady_clock::now();
            float totalTime = std::chrono::duration<float>(now - startTime).count();
            if (totalTime >= 0.5f && !initialPrinted) {
                initialPrinted = true;
                std::cout << "================= INITIAL TREE =================" << std::endl;
                printWidgetTree(root, 0);
                std::cout << "================================================" << std::endl;
            }
            if (totalTime >= 2.0f && !spanToggled) {
                spanToggled = true;
                std::cout << "[SIMULATION] Toggling Span..." << std::endl;
                btnToggleSpan->onClick();
                app.updateStyleAndLayout();
                std::cout << "================= ROWSPAN TOGGLED TREE =================" << std::endl;
                printWidgetTree(root, 0);
                std::cout << "========================================================" << std::endl;
            }
            if (totalTime >= 4.0f && !colspanToggled) {
                colspanToggled = true;
                std::cout << "[SIMULATION] Expanding Colspan..." << std::endl;
                btnColSpan->onClick();
                app.updateStyleAndLayout();
                std::cout << "================= COLSPAN TOGGLED TREE =================" << std::endl;
                printWidgetTree(root, 0);
                std::cout << "========================================================" << std::endl;
            }
            if (totalTime >= 6.0f) {
                std::cout << "[SIMULATION] Exiting app..." << std::endl;
                app.running = false;
            }
        };
    } else {
        app.onUpdate = [&](float dt) {
            app.requestRedraw();
        };
    }

    app.run();
    app.shutdown();
    return 0;
}
