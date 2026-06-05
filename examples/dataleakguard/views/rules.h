// DataLeak Guard — Rules view
#pragma once
#include "../components.h"
#include <array>

namespace dlg {

inline Element RuleRow(int idx, const std::string& name, const std::string& detail,
                       const std::string& scope, const std::string& hits) {
    static std::array<State<bool>, 5> enabled = {
        State<bool>(true), State<bool>(true), State<bool>(true),
        State<bool>(false), State<bool>(true)
    };
    auto& rule = enabled[idx];
    return Div({
        Div({ Span(name).className("rule-name"), Span(detail).className("rule-detail") }).className("rule-main"),
        Div({ Pill(scope,"info"), Pill(hits,"warning") }).className("rule-meta"),
        ToggleSwitch(rule)
    }).className("rule-row");
}

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
                RuleRow(0,"Payment card data to removable media","Block PAN-like values.","Endpoints","47 hits"),
                RuleRow(1,"Employee identifiers in shared folders","Flag national ID.","Shares","18 hits"),
                RuleRow(2,"Secret material in outbound email","Detect API keys.","Email","12 hits"),
                RuleRow(3,"Source archives to cloud sync","Monitor code archives.","Cloud","0 hits"),
                RuleRow(4,"Legal hold document export","Require approval.","Legal","6 hits")
            }).className("panel-card panel-wide"),
            Div({
                SectionHeader("Rule Packs", "Installed coverage"),
                MetricStrip("Financial identifiers","Current","ok",1.0f),
                MetricStrip("Health records","Needs review","warning",0.72f),
                MetricStrip("Source secrets","Current","info",0.88f)
            }).className("panel-card panel-side")
        }).className("dashboard-grid")
    }).className("main-scroll");
}

} // namespace dlg
