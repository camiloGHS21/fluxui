// DataLeak Guard — Settings view
#pragma once
#include "../components.h"

namespace dlg {

inline Element SettingsView() {
    static auto blockMode          = State<bool>(true);
    static auto quarantineMode     = State<bool>(true);
    static auto endpointIsolation  = State<bool>(false);
    static auto digestEnabled      = State<bool>(true);

    return Div({
        TopBar("Settings", "Console preferences, integrations, and enforcement defaults", false),
        Div({
            Div({
                SectionHeader("Enforcement Defaults", "Global behavior for newly discovered risks"),
                ToggleRow("Block mode","Apply blocking actions when confidence exceeds threshold.",blockMode),
                ToggleRow("Quarantine mode","Move restricted files to isolated encrypted storage.",quarantineMode),
                ToggleRow("Endpoint isolation","Disconnect endpoints with repeated critical violations.",endpointIsolation),
                ToggleRow("Leadership digest","Send summarized posture notes to executives.",digestEnabled)
            }).className("panel-card panel-wide"),
            Div({
                SectionHeader("Integrations", "Connected systems"),
                MetricStrip("SIEM stream","Healthy","ok",1.0f),
                MetricStrip("Cloud sync","Enabled","info",0.88f),
                MetricStrip("Directory sync","Healthy","ok",0.96f)
            }).className("panel-card panel-side")
        }).className("dashboard-grid"),
        SectionHeader("Retention And Privacy"),
        Div({
            TableRow("table-row table-head","Setting","Value","Scope","Owner"),
            TableRow("table-row","Evidence retention","365 days","All incidents","Legal"),
            TableRow("table-row","Low-risk telemetry","30 days","Endpoints","IT Ops"),
            TableRow("table-row","PII masking","Enabled","Reports","Compliance")
        }).className("data-table")
    }).className("main-scroll");
}

} // namespace dlg
