// DataLeak Guard — Alerts view
#pragma once
#include "../components.h"

namespace dlg {

inline Element AlertsView() {
    return Div({
        TopBar("Security Alerts", "Prioritized incidents with owner, severity, and response path"),

        Div({
            StatCard("Critical", "3", "Needs action", "#FF5D6C"),
            StatCard("High", "8", "Under review", "#F3B64B"),
            StatCard("Contained", "47", "Blocked today", "#37C6A3"),
            StatCard("Mean Triage", "7m", "Down 22%", "#6AA9FF")
        }).className("stats-row"),

        Div({
            Div({
                SectionHeader("Incident Queue", "Analyst-ready alert stream"),
                ActivityRow("usb",   "Unauthorized USB copy: contracts_2026.zip", "Dana R. - 2 min", "danger"),
                ActivityRow("mail",  "Outbound email contains payroll identifiers", "SOC Pool - 11 min", "danger"),
                ActivityRow("cloud", "Cloud upload matched legal hold rule", "Miguel A. - 24 min", "warning"),
                ActivityRow("database","SQL export exceeded permitted row count", "Unassigned - 41 min", "warning")
            }).className("panel-card panel-wide"),
            Div({
                SectionHeader("Playbook", "Default response ladder"),
                MetricStrip("Evidence capture", "Complete", "ok", 1.0f),
                MetricStrip("Manager notice", "Queued", "info", 0.62f),
                MetricStrip("Legal hold", "Manual review", "warning", 0.34f)
            }).className("panel-card panel-side")
        }).className("dashboard-grid"),

        SectionHeader("Audit Timeline"),
        Div({
            ActivityRow("clock",  "SOC assigned incident DLG-4701 to Dana R.", "now", "info"),
            ActivityRow("block",  "Endpoint copy operation killed by kernel monitor", "2 min ago", "danger"),
            ActivityRow("report", "Evidence bundle sealed for DLG-4698", "16 min ago", "ok")
        }).className("activity-panel"),

        StatusBar()
    }).className("main-scroll");
}

} // namespace dlg
