// DataLeak Guard — Scanner view (functional scan with live progress)
#pragma once
#include "../components.h"

namespace dlg {

// Shared scan state so the dashboard's "Run Full Scan" can start it too.
inline State<bool>&  scannerRunning() { static State<bool> s(false); return s; }
inline State<float>& scanProgress()   { static State<float> s(0.67f); return s; }

inline Element ScannerView() {
    static auto quarantineMode = State<bool>(true);
    static auto progressRef = Ref<FluxUI::ProgressBar>();

    // Drive the progress bar each frame while a scan is running.
    App::current().onTick([](float dt) {
        if (!scannerRunning().get()) return;
        float p = scanProgress().get() + dt * 0.15f;   // ~7s full scan
        if (p >= 1.0f) { p = 1.0f; scannerRunning().set(false); }
        scanProgress().set(p);
        if (progressRef) {
            progressRef->progress = p;
            progressRef->markStyleDirty();   // forces re-resolve + repaint
        }
        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
    });

    return Div({
        TopBar("File Scanner", "Sensitive-data discovery across folders, endpoints, and shares"),

        Div({
            Div({ Icon("scanner").className("scan-orb-icon") }).className("scan-orb"),
            Div({
                Text([]{ return scannerRunning().get() ? std::string("Active scan in progress")
                                                       : std::string("Ready for targeted scan"); })
                    .className("hero-title"),
                P("Scope: C:\\Users\\Finance\\Documents, shared drives, removable media.").className("hero-text"),
                ProgressBar(scanProgress().get(), "#37C6A3").className("progress-large").onMount(progressRef),
                Text([]{ return std::to_string((int)(scanProgress().get()*100)) + "% complete"; })
                    .className("progress-caption")
            }).className("scanner-copy"),
            Div({
                Button("Start Scan").className("btn btn-primary")
                    .onClick([]{
                        if (scanProgress().get() >= 1.0f) scanProgress().set(0.0f);
                        scannerRunning().set(true);
                    }),
                Button("Rescan High Risk").className("btn btn-secondary")
                    .onClick([]{ scanProgress().set(0.12f); scannerRunning().set(true); })
            }).className("hero-actions")
        }).className("scanner-cockpit"),

        Div({
            Div({ SectionHeader("Scope", "Folders and devices"),
                  Pill("Finance drive","ok"), Pill("Shared legal","info"), Pill("USB devices","warning")
            }).className("panel-card"),
            Div({ SectionHeader("Patterns", "Active detectors"),
                  Pill("Credit cards","danger"), Pill("SSN / IDs","warning"), Pill("Secrets","info")
            }).className("panel-card"),
            Div({ SectionHeader("Response", "Default handling"),
                  ToggleRow("Quarantine matches","Isolate high-confidence files.",quarantineMode)
            }).className("panel-card")
        }).className("three-grid"),

        SectionHeader("Detected Findings"),
        Div({
            TableRow("table-row table-head","File","Detector","Action","Confidence"),
            TableRow("table-row","payroll_export.csv","Employee ID","Quarantined","98%"),
            TableRow("table-row","vendor_cards.xlsx","Payment PAN","Blocked","96%"),
            TableRow("table-row","contracts_2026.zip","Legal terms","Review","83%"),
            TableRow("table-row","api_backup.env","Secret key","Blocked","91%")
        }).className("data-table")
    }).className("main-scroll");
}

} // namespace dlg
