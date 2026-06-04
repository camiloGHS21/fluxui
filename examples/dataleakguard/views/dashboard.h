// DataLeak Guard — Dashboard view (state lives here, not in a separate file)
#pragma once
#include "../components.h"
#include "scanner.h"   // for scannerRunning()/scanProgress() shared state

namespace dlg {

inline Element DashboardView() {
    // State lives inside the view — no separate state file needed.
    static auto blockMode       = State<bool>(true);
    static auto quarantineMode  = State<bool>(true);
    static auto endpointIsolation = State<bool>(false);

    return Div({
        TopBar("Command Center", "Leakage posture, response pressure, and policy health"),

        // Hero
        Div({
            Div({
                H2("Data exfiltration posture is contained").className("hero-title"),
                P("Automated block rules stopped 47 transfers while 4 incidents "
                  "wait for analyst sign-off.").className("hero-text"),
                Div({
                    Pill("Block mode on", "ok"),
                    Pill("94% compliant", "info"),
                    Pill("4 escalations", "warning")
                }).className("hero-pills")
            }).className("hero-copy"),
            Div({
                Button("Run Full Scan").className("btn btn-primary")
                    .onClick([]{
                        scanProgress().set(0.06f);
                        scannerRunning().set(true);
                        App::current().navigate("/scanner");
                    }),
                Button(blockMode.get() ? "Monitor Mode" : "Block Mode")
                    .className("btn btn-danger")
                    .onClick([&]{ blockMode.toggle(); })
            }).className("hero-actions")
        }).className("hero-panel"),

        // Stats
        Div({
            StatCard("Files Monitored", "12,847", "+234 this week", "#37C6A3"),
            StatCard("Threats Blocked", "47", "3 critical today", "#FF5D6C"),
            StatCard("USB Transfers", "156", "12 flagged", "#F3B64B"),
            StatCard("Compliance Score", "94%", "Above target", "#6AA9FF")
        }).className("stats-row"),

        // Risk + Response
        Div({
            Div({
                SectionHeader("Risk Flow", "Where sensitive data is moving right now"),
                MetricStrip("External drive transfers", "12 flagged / 156 total", "warning", 0.42f),
                MetricStrip("Email attachment scans", "5 blocked / 418 inspected", "danger", 0.24f),
                MetricStrip("Cloud upload policy", "8 quarantined / 92 reviewed", "info", 0.58f),
                MetricStrip("Privileged exports", "2 pending approval", "ok", 0.18f)
            }).className("panel-card panel-wide"),
            Div({
                SectionHeader("Live Containment", "Automations ready for escalation"),
                ToggleRow("Block removable media", "Stop copy to untrusted USB.", blockMode),
                ToggleRow("Auto quarantine", "Isolate high-confidence matches.", quarantineMode),
                ToggleRow("Endpoint isolation", "Cut network for repeat offenders.", endpointIsolation)
            }).className("panel-card panel-side")
        }).className("dashboard-grid"),

        // Activity
        SectionHeader("Recent Activity"),
        Div({
            ActivityRow("usb",  "Sensitive contract archive blocked on USB-A72", "2 min ago", "danger"),
            ActivityRow("card", "Credit card pattern detected in export.csv", "15 min ago", "danger"),
            ActivityRow("id",   "Employee IDs found in shared Sales folder", "1 hour ago", "warning"),
            ActivityRow("check","Weekly compliance scan completed at 94%", "3 hours ago", "ok"),
            ActivityRow("rules","Rule pack updated: Financial identifiers", "5 hours ago", "info")
        }).className("activity-panel")
    }).className("main-scroll");
}

} // namespace dlg
