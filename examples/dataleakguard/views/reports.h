// DataLeak Guard — Reports view
#pragma once
#include "../components.h"

namespace dlg {

inline Element ReportsView() {
    static auto digestEnabled    = State<bool>(true);
    static auto reportScheduler  = State<bool>(true);

    return Div({
        TopBar("Reports", "Compliance exports, executive summaries, and forensic evidence"),
        Div({
            Div({ H2("May compliance packet is ready").className("hero-title"),
                  P("Includes incident inventory, policy changes, exception approvals.").className("hero-text"),
                  Div({ Pill("SOX","ok"), Pill("PCI","info"), Pill("Legal","warning") }).className("hero-pills")
            }).className("hero-copy"),
            Div({ Button("Generate Packet").className("btn btn-primary"),
                  Button("Schedule").className("btn btn-secondary")
            }).className("hero-actions")
        }).className("hero-panel"),
        Div({
            Div({ SectionHeader("Executive Summary"),
                  MetricStrip("Risk reduction","-31%","ok",0.69f),
                  MetricStrip("Open exceptions","14","warning",0.38f)
            }).className("panel-card"),
            Div({ SectionHeader("Audit Packet"),
                  MetricStrip("Evidence sealed","98%","info",0.98f),
                  MetricStrip("Reviewer sign-off","3 of 4","warning",0.75f)
            }).className("panel-card"),
            Div({ SectionHeader("Delivery"),
                  ToggleRow("Weekly digest","Send leadership summary each Monday.",digestEnabled),
                  ToggleRow("Auto scheduler","Generate monthly compliance packet.",reportScheduler)
            }).className("panel-card")
        }).className("three-grid"),
        SectionHeader("Recent Reports"),
        Div({
            TableRow("table-row table-head","Report","Coverage","Owner","Status"),
            TableRow("table-row","May DLP Executive Summary","Enterprise","CISO","Ready"),
            TableRow("table-row","PCI Evidence Packet","Finance","Audit","Review"),
            TableRow("table-row","Endpoint Exception Register","Workstations","IT Ops","Scheduled")
        }).className("data-table")
    }).className("main-scroll");
}

} // namespace dlg
