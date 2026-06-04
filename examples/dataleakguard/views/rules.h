// DataLeak Guard — Rules view
#pragma once
#include "../state.h"
#include "../components.h"

namespace dlg {

inline Element RulesView() {
    return Div({
        TopBar("Policy Rules", "Detection logic, enforcement mode, and rule-pack coverage"),
        Div({
            Div({ H2("Rule Builder").className("hero-title"),
                  P("Draft a policy from scope, detector, confidence, and action.").className("hero-text")
            }).className("builder-copy"),
            Div({ Input("Rule name").className("builder-input"),
                  Input("Detector expression").className("builder-input"),
                  Button("Save Draft").className("btn btn-primary")
            }).className("builder-fields")
        }).className("builder-panel"),
        Div({
            Div({
                SectionHeader("Rule Library", "Production rules with live hit counts"),
                RuleRow(0,"Payment card data to removable media","Block PAN-like values when destination is external drive.","Endpoints","47 hits"),
                RuleRow(1,"Employee identifiers in shared folders","Flag national ID, employee ID, and payroll markers.","Shares","18 hits"),
                RuleRow(2,"Secret material in outbound email","Detect API keys, tokens, and private key headers.","Email","12 hits"),
                RuleRow(3,"Source archives to cloud sync","Monitor large code archives moving to personal cloud.","Cloud","0 hits"),
                RuleRow(4,"Legal hold document export","Require approval for legal-hold tagged records.","Legal","6 hits")
            }).className("panel-card panel-wide"),
            Div({
                SectionHeader("Rule Packs", "Installed coverage"),
                MetricStrip("Financial identifiers","Current","ok",1.0f),
                MetricStrip("Health records","Needs review","warning",0.72f),
                MetricStrip("Source secrets","Current","info",0.88f)
            }).className("panel-card panel-side")
        }).className("dashboard-grid"),
        StatusBar()
    }).className("main-scroll");
}

} // namespace dlg
