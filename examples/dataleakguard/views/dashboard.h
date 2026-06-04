// DataLeak Guard — Dashboard view
#pragma once
#include "../state.h"
#include "../components.h"

namespace dlg {

inline Element DashboardView() {
    return Div({
        TopBar("Command Center", "Leakage posture, response pressure, and policy health"),

        // Hero panel
        Div({
            Div({
                H2("Data exfiltration posture is contained").className("hero-title"),
                P("Automated block rules stopped 47 transfers while 4 incidents wait "
                  "for analyst sign-off.").className("hero-text"),
                Div({
                    Pill("Block mode on", "ok"),
                    Pill("94% compliant", "info"),
                    Pill("4 escalations", "warning")
                }).className("hero-pills")
            }).className("hero-copy"),
            Div({
                Button("Run Full Scan").className("btn btn-primary"),
                Button("Monitor Mode").className("btn btn-danger")
            }).className("hero-actions")
        }).className("hero-panel"),

        // Stats row
        Div({
            StatCard("Files Monitored", filesMonitored, "+234 this week", "#37C6A3"),
            StatCard("Threats Blocked", threatsBlocked, "3 critical today", "#FF5D6C"),
            StatCard("USB Transfers", usbTransfers, "12 flagged", "#F3B64B"),
            StatCard("Compliance Score", complianceScore, "Above target", "#6AA9FF")
        }).className("stats-row"),

        // Risk + Response grid
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
                ToggleRow("Block removable media", "Stop copy operations to untrusted USB devices.", blockMode()),
                ToggleRow("Auto quarantine", "Move high-confidence matches to isolated storage.", quarantineMode()),
                ToggleRow("Endpoint isolation", "Cut network access for repeat offenders.", endpointIsolation())
            }).className("panel-card panel-side")
        }).className("dashboard-grid"),

        // Recent activity
        SectionHeader("Recent Activity"),
        Div({
            ActivityRow("usb",  "Sensitive contract archive blocked on USB-A72", "2 min ago", "danger"),
            ActivityRow("card", "Credit card pattern detected in export.csv", "15 min ago", "danger"),
            ActivityRow("id",   "Employee IDs found in shared Sales folder", "1 hour ago", "warning"),
            ActivityRow("check","Weekly compliance scan completed at 94%", "3 hours ago", "ok"),
            ActivityRow("rules","Rule pack updated: Financial identifiers", "5 hours ago", "info")
        }).className("activity-panel"),

        StatusBar()
    }).className("main-scroll");
}

} // namespace dlg
