// DataLeak Guard — reusable UI components (all declarative)
#pragma once
#include <fluxui/dsl.h>
#include <string>

namespace dlg {

using namespace fluxui;

// --- Small reusable building blocks ---

inline Element Pill(const std::string& text, const std::string& tone) {
    return Div({ Span(text).className("pill-text") }).className("pill pill-" + tone);
}

inline Element MetricStrip(const std::string& label, const std::string& value,
                           const std::string& tone, float progress) {
    return Div({
        Div({
            Span(label).className("metric-label"),
            Span(value).className("metric-value")
        }).className("metric-copy"),
        ProgressBar(progress, tone == "danger" ? "#FF5D6C" :
                              tone == "warning" ? "#F3B64B" :
                              tone == "info"    ? "#6AA9FF" : "#37C6A3")
            .className("progress-line")
    }).className("metric-strip");
}

inline Element SectionHeader(const std::string& title, const std::string& subtitle = "") {
    auto copy = Div({ H2(title).className("section-title") }).className("section-copy");
    if (!subtitle.empty())
        copy.children_.push_back(P(subtitle).className("section-subtitle"));
    return Div({ copy }).className("section-head");
}

inline Element ActivityRow(const std::string& icon, const std::string& text,
                           const std::string& meta, const std::string& severity) {
    return Div({
        Div({ Icon(icon).className("activity-mark mark-" + severity) })
            .className("activity-icon alert-" + severity),
        Span(text).className("activity-text"),
        Span(meta).className("activity-time")
    }).className("activity-row");
}

inline Element ToggleRow(const std::string& title, const std::string& desc,
                         State<bool>& value) {
    return Div({
        Div({
            Span(title).className("toggle-title"),
            Span(desc).className("toggle-desc")
        }).className("toggle-copy"),
        Button("")
            .className(value.get() ? "toggle toggle-on" : "toggle toggle-off")
            .onMount([&value](FluxUI::Widget* w) {
                w->onClick = [&value, w]() {
                    value.set(!value.get());
                    w->className = value.get() ? "toggle toggle-on" : "toggle toggle-off";
                    w->markStyleDirty();
                };
            }),
        Div({}).className("toggle-knob")
    }).className("toggle-row");
}

inline Element RuleRow(int idx, const std::string& name, const std::string& detail,
                       const std::string& scope, const std::string& hits) {
    auto& rules = ruleEnabled();
    return Div({
        Div({
            Span(name).className("rule-name"),
            Span(detail).className("rule-detail")
        }).className("rule-main"),
        Div({ Pill(scope, "info"), Pill(hits, "warning") }).className("rule-meta"),
        Button("")
            .className(rules[idx].get() ? "toggle toggle-on" : "toggle toggle-off")
            .onMount([idx](FluxUI::Widget* w) {
                auto& r = ruleEnabled()[idx];
                w->onClick = [&r, w]() {
                    r.set(!r.get());
                    w->className = r.get() ? "toggle toggle-on" : "toggle toggle-off";
                    w->markStyleDirty();
                };
            })
    }).className("rule-row");
}

inline Element TableRow(const std::string& cls,
                        const std::string& c1, const std::string& c2,
                        const std::string& c3, const std::string& c4) {
    return Div({
        Span(c1).className("table-cell table-main"),
        Span(c2).className("table-cell"),
        Span(c3).className("table-cell"),
        Span(c4).className("table-cell table-right")
    }).className(cls);
}

inline Element TopBar(const std::string& title, const std::string& subtitle,
                      bool showSearch = true) {
    auto tools = Div({}).className("top-tools");
    if (showSearch)
        tools.children_.push_back(Input("search", "Search incidents, hosts, policies...")
                                      .className("search-box"));
    tools.children_.push_back(Button("Export").className("btn btn-secondary btn-compact"));
    return Div({
        Div({
            H1(title).className("page-title"),
            P(subtitle).className("page-subtitle")
        }).className("title-group"),
        tools
    }).className("top-bar");
}

inline Element StatusBar() {
    return Div({
        Div({
            Div({}).className("status-dot"),
            Span("Live prevention enabled").className("status-text")
        }).className("status-cluster"),
        Span("12,847 files monitored").className("status-text"),
        Span("47 blocks today").className("status-text"),
        Span("Last sync: 38 sec ago").className("status-text status-right")
    }).className("status-bar");
}

} // namespace dlg
