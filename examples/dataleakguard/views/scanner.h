// DataLeak Guard — Scanner view
#pragma once
#include "../state.h"
#include "../components.h"

namespace dlg {

inline Element ScannerView() {
    return Div({
        TopBar("File Scanner", "Sensitive-data discovery across folders, endpoints, and shares"),

        Div({
            Div({
                Icon("scanner").className("scan-orb-icon")
            }).className("scan-orb"),
            Div({
                H2("Ready for targeted scan").className("hero-title"),
                P("Current scope: C:\\Users\\Finance\\Documents, shared drives, and "
                  "removable media.").className("hero-text"),
                ProgressBar(scanProgress().get(), "#37C6A3").className("progress-large"),
                Text([]{
                    int pct = (int)(scanProgress().get() * 100);
                    return std::to_string(pct) + "% complete - 184,000 objects in queue";
                }).className("progress-caption")
            }).className("scanner-copy"),
            Div({
                Button("Start Scan").className("btn btn-primary"),
                Button("Rescan High Risk").className("btn btn-secondary")
            }).className("hero-actions")
        }).className("scanner-cockpit"),

        Div({
            Div({
                SectionHeader("Scope", "Folders and devices"),
                Pill("Finance drive", "ok"),
                Pill("Shared legal", "info"),
                Pill("USB devices", "warning")
            }).className("panel-card"),
            Div({
                SectionHeader("Patterns", "Active detectors"),
                Pill("Credit cards", "danger"),
                Pill("SSN / IDs", "warning"),
                Pill("Secrets", "info")
            }).className("panel-card"),
            Div({
                SectionHeader("Response", "Default handling"),
                ToggleRow("Quarantine matches", "Isolate high-confidence files automatically.", quarantineMode())
            }).className("panel-card")
        }).className("three-grid"),

        SectionHeader("Detected Findings", "Current scan findings grouped by action"),
        Div({
            TableRow("table-row table-head", "File", "Detector", "Action", "Confidence"),
            TableRow("table-row", "payroll_export.csv", "Employee ID", "Quarantined", "98%"),
            TableRow("table-row", "vendor_cards.xlsx", "Payment PAN", "Blocked", "96%"),
            TableRow("table-row", "contracts_2026.zip", "Legal terms", "Review", "83%"),
            TableRow("table-row", "api_backup.env", "Secret key", "Blocked", "91%")
        }).className("data-table"),

        StatusBar()
    }).className("main-scroll");
}

} // namespace dlg
