// DataLeak Guard - Main Application
// Built on FluxUI Framework

#include "fluxui/FluxUI.h"
#include "embedded_theme.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace FluxUI;

// ============================================================
//  Application State
// ============================================================

static bool uiDirty = true;
static bool scannerRunning = false;
static float scanProgress = 0.67f;
static bool blockMode = true;
static bool quarantineMode = true;
static bool endpointIsolation = false;
static bool digestEnabled = true;
static bool reportScheduler = true;
static bool cloudSync = true;

static std::array<bool, 5> ruleEnabled = {true, true, true, false, true};

static const char* navItems[] = {
    "Dashboard", "Scanner", "Alerts", "Rules", "Reports", "Settings"
};

static const char* navRoutes[] = {
    "/dashboard", "/scanner", "/alerts", "/rules", "/reports", "/settings"
};

static const char* navIcons[] = {
    "dashboard", "scanner", "alert", "rules", "report", "settings"
};

// ============================================================
//  Retained UI Shell — persistent widget references
// ============================================================

struct RetainedShell {
    bool initialized = false;

    // Top-level containers (built once, never destroyed)
    Panel* sidebar = nullptr;
    Panel* contentArea = nullptr;
    Panel* statusBar = nullptr;

    // Nav items for highlight-only updates
    Button* navButtons[6] = {};
    Icon*   navIcons[6] = {};
    Text*   navTexts[6] = {};

    // Sidebar posture card (updates when blockMode changes)
    Text* postureTitle = nullptr;

    // Scanner page live refs (valid only while scanner page is displayed)
    ProgressBar* scanProgressBar = nullptr;
    Text*        scanCaption = nullptr;
    Text*        scanHeroTitle = nullptr;
    Icon*        scanOrbIcon = nullptr;

    void clearPageRefs() {
        scanProgressBar = nullptr;
        scanCaption = nullptr;
        scanHeroTitle = nullptr;
        scanOrbIcon = nullptr;
    }

    // Update sidebar nav highlights without rebuilding
    void updateNavHighlights(const std::string& activeRoute) {
        for (int i = 0; i < 6; i++) {
            if (!navButtons[i]) continue;
            bool active = (activeRoute == navRoutes[i]);
            navButtons[i]->className = active ? "nav-item active" : "nav-item";
            navButtons[i]->markStyleDirty();
            if (navIcons[i]) {
                navIcons[i]->className = active ? "nav-icon active-icon" : "nav-icon";
                navIcons[i]->markStyleDirty();
            }
            if (navTexts[i]) {
                navTexts[i]->className = active ? "nav-text active-text" : "nav-text";
                navTexts[i]->markStyleDirty();
            }
        }
    }

    // Update posture text from blockMode
    void syncPosture() {
        if (postureTitle) {
            postureTitle->content = blockMode ? "Containment active" : "Monitor only";
        }
    }
};

static RetainedShell shell;

// ============================================================
//  Small UI Helpers
// ============================================================

static Color toneColor(const std::string& tone) {
    if (tone == "danger") return Color::fromHex("#FF5D6C");
    if (tone == "warning") return Color::fromHex("#F3B64B");
    if (tone == "info") return Color::fromHex("#6AA9FF");
    return Color::fromHex("#37C6A3");
}

static std::string percentText(float value) {
    int pct = (int)std::clamp(value * 100.0f, 0.0f, 100.0f);
    return std::to_string(pct) + "%";
}

static Button* addButton(Widget* parent,
                         const std::string& label,
                         const std::string& icon,
                         const std::string& classes,
                         std::function<void()> onClick = {}) {
    auto* button = parent->add<Button>("", classes);
    button->reserveChildren(icon.empty() ? 1 : 2);
    std::string toneClass;
    if (classes.find("btn-primary") != std::string::npos) {
        toneClass = " primary-control";
    } else if (classes.find("btn-danger") != std::string::npos) {
        toneClass = " danger-control";
    }
    if (!icon.empty()) {
        button->add<Icon>(icon, "btn-icon" + toneClass);
    }
    button->add<Text>(label, "btn-label" + toneClass);
    if (onClick) {
        button->onClick = std::move(onClick);
    }
    return button;
}

static void addPill(Widget* parent, const std::string& text, const std::string& tone) {
    auto* pill = parent->add<Panel>("pill pill-" + tone);
    pill->reserveChildren(1);
    pill->add<Text>(text, "pill-text");
}

static ProgressBar* addProgress(Widget* parent,
                                float value,
                                const std::string& cls,
                                const Color& color) {
    auto* progress = parent->add<ProgressBar>();
    progress->className = cls;
    progress->progress = value;
    progress->barColor = color;
    return progress;
}

static void addSectionHeader(Widget* parent,
                             const std::string& title,
                             const std::string& subtitle = "") {
    auto* header = parent->add<Panel>("section-head");
    header->reserveChildren(1);
    auto* copy = header->add<Panel>("section-copy");
    copy->reserveChildren(subtitle.empty() ? 1 : 2);
    copy->add<Text>(title, "section-title");
    if (!subtitle.empty()) {
        copy->add<Text>(subtitle, "section-subtitle");
    }
}

static void addMetricStrip(Widget* parent,
                           const std::string& label,
                           const std::string& value,
                           const std::string& tone,
                           float progress) {
    auto* row = parent->add<Panel>("metric-strip");
    row->reserveChildren(2);
    auto* copy = row->add<Panel>("metric-copy");
    copy->reserveChildren(2);
    copy->add<Text>(label, "metric-label");
    copy->add<Text>(value, "metric-value");
    addProgress(row, progress, "progress-line", toneColor(tone));
}

static void addActivityRow(Widget* parent,
                           const std::string& icon,
                           const std::string& text,
                           const std::string& meta,
                           const std::string& severity) {
    auto* row = parent->add<Panel>("activity-row");
    row->reserveChildren(3);
    auto* iconPanel = row->add<Panel>("activity-icon alert-" + severity);
    iconPanel->reserveChildren(1);
    iconPanel->add<Icon>(icon, "activity-mark mark-" + severity);
    row->add<Text>(text, "activity-text");
    row->add<Text>(meta, "activity-time");
}

static void addToggleRow(Widget* parent,
                         const std::string& title,
                         const std::string& description,
                         bool& value,
                         bool affectsPosture = false) {
    auto* row = parent->add<Panel>("toggle-row");
    row->reserveChildren(2);
    auto* copy = row->add<Panel>("toggle-copy");
    copy->reserveChildren(2);
    copy->add<Text>(title, "toggle-title");
    copy->add<Text>(description, "toggle-desc");

    auto* toggle = row->add<Button>("", value ? "toggle toggle-on" : "toggle toggle-off");
    toggle->add<Panel>("toggle-knob");
    toggle->onClick = [&value, toggle, affectsPosture]() {
        value = !value;
        // In-place update — no full rebuild needed
        toggle->className = value ? "toggle toggle-on" : "toggle toggle-off";
        toggle->markStyleDirty();
        if (affectsPosture) shell.syncPosture();
    };
}

static void addRuleRow(Widget* parent,
                       int index,
                       const std::string& name,
                       const std::string& detail,
                       const std::string& scope,
                       const std::string& hits) {
    auto* row = parent->add<Panel>("rule-row");
    row->reserveChildren(3);
    auto* left = row->add<Panel>("rule-main");
    left->reserveChildren(2);
    left->add<Text>(name, "rule-name");
    left->add<Text>(detail, "rule-detail");
    auto* meta = row->add<Panel>("rule-meta");
    meta->reserveChildren(2);
    addPill(meta, scope, "info");
    addPill(meta, hits, "warning");
    auto* toggle = row->add<Button>("", ruleEnabled[index] ? "toggle toggle-on" : "toggle toggle-off");
    toggle->add<Panel>("toggle-knob");
    toggle->onClick = [index, toggle]() {
        ruleEnabled[index] = !ruleEnabled[index];
        // In-place update — no full rebuild
        toggle->className = ruleEnabled[index] ? "toggle toggle-on" : "toggle toggle-off";
        toggle->markStyleDirty();
    };
}

static void addTableRow(Widget* parent,
                        const std::string& cls,
                        const std::string& c1,
                        const std::string& c2,
                        const std::string& c3,
                        const std::string& c4) {
    auto* row = parent->add<Panel>(cls);
    row->reserveChildren(4);
    row->add<Text>(c1, "table-cell table-main");
    row->add<Text>(c2, "table-cell");
    row->add<Text>(c3, "table-cell");
    row->add<Text>(c4, "table-cell table-right");
}

// ============================================================
//  Shared Shell
// ============================================================

static void buildTopBar(Widget* content,
                        const std::string& title,
                        const std::string& subtitle,
                        bool showSearch = true) {
    auto* topBar = content->add<Panel>("top-bar");

    auto* titleGroup = topBar->add<Panel>("title-group");
    titleGroup->add<Text>(title, "page-title");
    titleGroup->add<Text>(subtitle, "page-subtitle");

    auto* tools = topBar->add<Panel>("top-tools");
    if (showSearch) {
        tools->add<TextInput>("Search incidents, hosts, policies...", "search-box");
    }
    addButton(tools, "Export", "download", "btn btn-secondary btn-compact",
              []() { std::cout << "[DataLeak Guard] Export queued" << std::endl; });
}

static void buildStatusBar(Widget* content) {
    auto* status = content->add<Panel>("status-bar");
    auto* live = status->add<Panel>("status-cluster");
    live->add<Panel>("status-dot");
    live->add<Text>("Live prevention enabled", "status-text");
    status->add<Text>("12,847 files monitored", "status-text");
    status->add<Text>("47 blocks today", "status-text");
    status->add<Text>("Last sync: 38 sec ago", "status-text status-right");
}

static void buildSidebar(Widget* root, Application& app) {
    auto* sidebar = root->add<Panel>("sidebar");
    shell.sidebar = static_cast<Panel*>(sidebar);
    sidebar->reserveChildren(3);

    auto* header = sidebar->add<Panel>("sidebar-header");
    header->reserveChildren(1);
    auto* logoRow = header->add<Panel>("logo-row");
    logoRow->reserveChildren(2);
    auto* logoMark = logoRow->add<Panel>("logo-mark");
    logoMark->reserveChildren(1);
    logoMark->add<Icon>("shield", "logo-icon");
    auto* logoCopy = logoRow->add<Panel>("logo-copy");
    logoCopy->reserveChildren(2);
    logoCopy->add<Text>("DataLeak Guard", "sidebar-logo");
    logoCopy->add<Text>("Enterprise DLP Console", "sidebar-subtitle");

    auto* nav = sidebar->add<Panel>("nav-section");
    nav->reserveChildren(7);
    nav->add<Text>("WORKSPACE", "nav-label");

    const std::string activeRoute = app.currentRoute();
    for (int i = 0; i < 6; i++) {
        std::string cls = "nav-item";
        bool active = activeRoute == navRoutes[i];
        if (active) cls += " active";
        auto* item = nav->add<Button>("", cls);
        item->reserveChildren(2);
        auto* icon = item->add<Icon>(navIcons[i], active ? "nav-icon active-icon" : "nav-icon");
        auto* text = item->add<Text>(navItems[i], active ? "nav-text active-text" : "nav-text");
        std::string route = navRoutes[i];
        item->onClick = [&app, route]() {
            app.navigate(route);
        };
        // Retain references for in-place nav highlight updates
        shell.navButtons[i] = item;
        shell.navIcons[i] = icon;
        shell.navTexts[i] = text;
    }

    auto* posture = sidebar->add<Panel>("posture-card");
    posture->reserveChildren(4);
    posture->add<Text>("POSTURE", "nav-label");
    auto* postureTitle = posture->add<Text>(blockMode ? "Containment active" : "Monitor only", "posture-title");
    shell.postureTitle = postureTitle;
    posture->add<Text>("4 high-risk events require review", "posture-copy");
    auto* posturePills = posture->add<Panel>("posture-pills");
    posturePills->reserveChildren(2);
    addPill(posturePills, "DLP", "ok");
    addPill(posturePills, "SIEM", "info");
}

// ============================================================
//  Dashboard
// ============================================================

static void buildDashboard(Application& app, Widget* content) {
    buildTopBar(content, "Command Center", "Leakage posture, response pressure, and policy health");

    auto* main = content->add<Panel>("main-scroll");

    auto* hero = main->add<Panel>("hero-panel");
    auto* heroCopy = hero->add<Panel>("hero-copy");
    heroCopy->add<Text>("Data exfiltration posture is contained", "hero-title");
    heroCopy->add<Text>("Automated block rules stopped 47 transfers while 4 incidents wait for analyst sign-off.",
                        "hero-text");
    auto* heroPills = heroCopy->add<Panel>("hero-pills");
    addPill(heroPills, "Block mode on", "ok");
    addPill(heroPills, "94% compliant", "info");
    addPill(heroPills, "4 escalations", "warning");

    auto* heroActions = hero->add<Panel>("hero-actions");
    addButton(heroActions, scannerRunning ? "Scanner Running" : "Run Full Scan",
              scannerRunning ? "pause" : "scanner", "btn btn-primary",
              [&app]() {
                  scannerRunning = true;
                  scanProgress = 0.06f;
                  app.navigate("/scanner");
              });
    auto* blockHeroBtn = addButton(heroActions, blockMode ? "Monitor Mode" : "Block Mode",
              "block", "btn btn-danger",
              nullptr);
    // In-place update for block mode hero button
    blockHeroBtn->onClick = [blockHeroBtn]() {
        blockMode = !blockMode;
        // Update button label in-place
        auto* label = static_cast<Text*>(blockHeroBtn->children.back().get());
        label->content = blockMode ? "Monitor Mode" : "Block Mode";
        shell.syncPosture();
    };

    auto* statsRow = main->add<Panel>("stats-row");
    statsRow->add<StatCard>("Files Monitored", "12,847", "+234 this week", Color::fromHex("#37C6A3"));
    statsRow->add<StatCard>("Threats Blocked", "47", "3 critical today", Color::fromHex("#FF5D6C"));
    statsRow->add<StatCard>("USB Transfers", "156", "12 flagged", Color::fromHex("#F3B64B"));
    statsRow->add<StatCard>("Compliance Score", "94%", "Above target", Color::fromHex("#6AA9FF"));

    auto* dashboardGrid = main->add<Panel>("dashboard-grid");
    auto* riskPanel = dashboardGrid->add<Panel>("panel-card panel-wide");
    addSectionHeader(riskPanel, "Risk Flow", "Where sensitive data is moving right now");
    addMetricStrip(riskPanel, "External drive transfers", "12 flagged / 156 total", "warning", 0.42f);
    addMetricStrip(riskPanel, "Email attachment scans", "5 blocked / 418 inspected", "danger", 0.24f);
    addMetricStrip(riskPanel, "Cloud upload policy", "8 quarantined / 92 reviewed", "info", 0.58f);
    addMetricStrip(riskPanel, "Privileged exports", "2 pending approval", "ok", 0.18f);

    auto* responsePanel = dashboardGrid->add<Panel>("panel-card panel-side");
    addSectionHeader(responsePanel, "Live Containment", "Automations ready for escalation");
    addToggleRow(responsePanel, "Block removable media", "Stop copy operations to untrusted USB devices.", blockMode, true);
    addToggleRow(responsePanel, "Auto quarantine", "Move high-confidence matches to isolated storage.", quarantineMode);
    addToggleRow(responsePanel, "Endpoint isolation", "Cut network access for repeat offenders.", endpointIsolation);

    addSectionHeader(main, "Recent Activity");
    auto* activity = main->add<Panel>("activity-panel");
    addActivityRow(activity, "usb", "Sensitive contract archive blocked on USB-A72", "2 min ago", "danger");
    addActivityRow(activity, "card", "Credit card pattern detected in export.csv", "15 min ago", "danger");
    addActivityRow(activity, "id", "Employee IDs found in shared Sales folder", "1 hour ago", "warning");
    addActivityRow(activity, "check", "Weekly compliance scan completed at 94%", "3 hours ago", "ok");
    addActivityRow(activity, "rules", "Rule pack updated: Financial identifiers", "5 hours ago", "info");
}

// ============================================================
//  Scanner
// ============================================================

static void buildScanner(Widget* content) {
    buildTopBar(content, "File Scanner", "Sensitive-data discovery across folders, endpoints, and shares");

    auto* main = content->add<Panel>("main-scroll");

    auto* cockpit = main->add<Panel>("scanner-cockpit");
    auto* scanOrb = cockpit->add<Panel>("scan-orb");
    auto* orbIcon = scanOrb->add<Icon>(scannerRunning ? "pause" : "scanner", "scan-orb-icon");
    shell.scanOrbIcon = orbIcon;

    auto* scanCopy = cockpit->add<Panel>("scanner-copy");
    auto* heroTitle = scanCopy->add<Text>(scannerRunning ? "Active scan in progress" : "Ready for targeted scan", "hero-title");
    shell.scanHeroTitle = heroTitle;
    scanCopy->add<Text>("Current scope: C:\\Users\\Finance\\Documents, shared drives, and removable media.",
                        "hero-text");
    auto* progressBar = addProgress(scanCopy, scanProgress, "progress-large", Color::fromHex("#37C6A3"));
    shell.scanProgressBar = progressBar;
    auto* caption = scanCopy->add<Text>(percentText(scanProgress) + " complete - 184,000 objects in queue", "progress-caption");
    shell.scanCaption = caption;

    auto* scanActions = cockpit->add<Panel>("hero-actions");
    auto* startBtn = addButton(scanActions, scannerRunning ? "Pause Scan" : "Start Scan",
              scannerRunning ? "pause" : "play", "btn btn-primary", nullptr);
    startBtn->onClick = [startBtn]() {
        scannerRunning = !scannerRunning;
        if (scannerRunning && scanProgress >= 1.0f) scanProgress = 0.0f;
        // Update button in-place
        auto* label = static_cast<Text*>(startBtn->children.back().get());
        auto* icon = static_cast<Icon*>(startBtn->children.front().get());
        label->content = scannerRunning ? "Pause Scan" : "Start Scan";
        icon->glyph = scannerRunning ? "pause" : "play";
        // Update orb and hero title
        if (shell.scanOrbIcon) shell.scanOrbIcon->glyph = scannerRunning ? "pause" : "scanner";
        if (shell.scanHeroTitle) shell.scanHeroTitle->content = scannerRunning ? "Active scan in progress" : "Ready for targeted scan";
    };
    addButton(scanActions, "Rescan High Risk", "reload", "btn btn-secondary",
              []() {
                  scannerRunning = true;
                  scanProgress = 0.12f;
                  if (shell.scanOrbIcon) shell.scanOrbIcon->glyph = "pause";
                  if (shell.scanHeroTitle) shell.scanHeroTitle->content = "Active scan in progress";
              });

    auto* scanGrid = main->add<Panel>("three-grid");
    auto* scope = scanGrid->add<Panel>("panel-card");
    addSectionHeader(scope, "Scope", "Folders and devices");
    addPill(scope, "Finance drive", "ok");
    addPill(scope, "Shared legal", "info");
    addPill(scope, "USB devices", "warning");

    auto* patterns = scanGrid->add<Panel>("panel-card");
    addSectionHeader(patterns, "Patterns", "Active detectors");
    addPill(patterns, "Credit cards", "danger");
    addPill(patterns, "SSN / IDs", "warning");
    addPill(patterns, "Secrets", "info");

    auto* response = scanGrid->add<Panel>("panel-card");
    addSectionHeader(response, "Response", "Default handling");
    addToggleRow(response, "Quarantine matches", "Isolate high-confidence files automatically.", quarantineMode);

    addSectionHeader(main, "Detected Findings", "Current scan findings grouped by action");
    auto* table = main->add<Panel>("data-table");
    addTableRow(table, "table-row table-head", "File", "Detector", "Action", "Confidence");
    addTableRow(table, "table-row", "payroll_export.csv", "Employee ID", "Quarantined", "98%");
    addTableRow(table, "table-row", "vendor_cards.xlsx", "Payment PAN", "Blocked", "96%");
    addTableRow(table, "table-row", "contracts_2026.zip", "Legal terms", "Review", "83%");
    addTableRow(table, "table-row", "api_backup.env", "Secret key", "Blocked", "91%");
}

// ============================================================
//  Alerts
// ============================================================

static void buildAlerts(Widget* content) {
    buildTopBar(content, "Security Alerts", "Prioritized incidents with owner, severity, and response path");

    auto* main = content->add<Panel>("main-scroll");

    auto* alertSummary = main->add<Panel>("stats-row");
    alertSummary->add<StatCard>("Critical", "3", "Needs action", Color::fromHex("#FF5D6C"));
    alertSummary->add<StatCard>("High", "8", "Under review", Color::fromHex("#F3B64B"));
    alertSummary->add<StatCard>("Contained", "47", "Blocked today", Color::fromHex("#37C6A3"));
    alertSummary->add<StatCard>("Mean Triage", "7m", "Down 22%", Color::fromHex("#6AA9FF"));

    auto* alertGrid = main->add<Panel>("dashboard-grid");
    auto* queue = alertGrid->add<Panel>("panel-card panel-wide");
    addSectionHeader(queue, "Incident Queue", "Analyst-ready alert stream");

    const char* rows[][5] = {
        {"usb", "Unauthorized USB copy: contracts_2026.zip", "Dana R.", "Critical", "2 min"},
        {"mail", "Outbound email contains payroll identifiers", "SOC Pool", "High", "11 min"},
        {"cloud", "Cloud upload matched legal hold rule", "Miguel A.", "Medium", "24 min"},
        {"database", "SQL export exceeded permitted row count", "Unassigned", "High", "41 min"}
    };

    for (auto& rowData : rows) {
        auto* row = queue->add<Panel>("incident-row");
        auto* iconPanel = row->add<Panel>("activity-icon alert-danger");
        iconPanel->add<Icon>(rowData[0], "activity-mark mark-danger");
        auto* copy = row->add<Panel>("incident-copy");
        copy->add<Text>(rowData[1], "incident-title");
        copy->add<Text>(std::string(rowData[2]) + " - " + rowData[4] + " ago", "incident-meta");
        addPill(row, rowData[3], std::string(rowData[3]) == "Critical" ? "danger" : "warning");
        addButton(row, "Triage", "forward", "btn btn-secondary btn-small",
                  []() { std::cout << "[DataLeak Guard] Alert opened" << std::endl; });
    }

    auto* playbook = alertGrid->add<Panel>("panel-card panel-side");
    addSectionHeader(playbook, "Playbook", "Default response ladder");
    addMetricStrip(playbook, "Evidence capture", "Complete", "ok", 1.0f);
    addMetricStrip(playbook, "Manager notice", "Queued", "info", 0.62f);
    addMetricStrip(playbook, "Legal hold", "Manual review", "warning", 0.34f);

    addSectionHeader(main, "Audit Timeline");
    auto* activity = main->add<Panel>("activity-panel");
    addActivityRow(activity, "clock", "SOC assigned incident DLG-4701 to Dana R.", "now", "info");
    addActivityRow(activity, "block", "Endpoint copy operation killed by kernel monitor", "2 min ago", "danger");
    addActivityRow(activity, "report", "Evidence bundle sealed for DLG-4698", "16 min ago", "ok");
}

// ============================================================
//  Rules
// ============================================================

static void buildRules(Widget* content) {
    buildTopBar(content, "Policy Rules", "Detection logic, enforcement mode, and rule-pack coverage");

    auto* main = content->add<Panel>("main-scroll");

    auto* builder = main->add<Panel>("builder-panel");
    auto* copy = builder->add<Panel>("builder-copy");
    copy->add<Text>("Rule Builder", "hero-title");
    copy->add<Text>("Draft a policy from scope, detector, confidence, and action. Save it as monitor-only before enforcement.",
                   "hero-text");
    auto* fields = builder->add<Panel>("builder-fields");
    fields->add<TextInput>("Rule name", "builder-input");
    fields->add<TextInput>("Detector expression", "builder-input");
    addButton(fields, "Save Draft", "plus", "btn btn-primary",
              []() { std::cout << "[DataLeak Guard] Draft rule saved" << std::endl; });

    auto* grid = main->add<Panel>("dashboard-grid");
    auto* library = grid->add<Panel>("panel-card panel-wide");
    addSectionHeader(library, "Rule Library", "Production rules with live hit counts");
    addRuleRow(library, 0, "Payment card data to removable media",
               "Block PAN-like values when destination is external drive.", "Endpoints", "47 hits");
    addRuleRow(library, 1, "Employee identifiers in shared folders",
               "Flag national ID, employee ID, and payroll markers.", "Shares", "18 hits");
    addRuleRow(library, 2, "Secret material in outbound email",
               "Detect API keys, tokens, and private key headers.", "Email", "12 hits");
    addRuleRow(library, 3, "Source archives to cloud sync",
               "Monitor large code archives moving to personal cloud.", "Cloud", "0 hits");
    addRuleRow(library, 4, "Legal hold document export",
               "Require approval for legal-hold tagged records.", "Legal", "6 hits");

    auto* packs = grid->add<Panel>("panel-card panel-side");
    addSectionHeader(packs, "Rule Packs", "Installed coverage");
    addMetricStrip(packs, "Financial identifiers", "Current", "ok", 1.0f);
    addMetricStrip(packs, "Health records", "Needs review", "warning", 0.72f);
    addMetricStrip(packs, "Source secrets", "Current", "info", 0.88f);
}

// ============================================================
//  Reports
// ============================================================

static void buildReports(Widget* content) {
    buildTopBar(content, "Reports", "Compliance exports, executive summaries, and forensic evidence");

    auto* main = content->add<Panel>("main-scroll");

    auto* reportHero = main->add<Panel>("hero-panel");
    auto* copy = reportHero->add<Panel>("hero-copy");
    copy->add<Text>("May compliance packet is ready", "hero-title");
    copy->add<Text>("Includes incident inventory, policy changes, exception approvals, and immutable audit trail.",
                   "hero-text");
    auto* pills = copy->add<Panel>("hero-pills");
    addPill(pills, "SOX", "ok");
    addPill(pills, "PCI", "info");
    addPill(pills, "Legal", "warning");
    auto* actions = reportHero->add<Panel>("hero-actions");
    addButton(actions, "Generate Packet", "report", "btn btn-primary",
              []() { std::cout << "[DataLeak Guard] Compliance packet generated" << std::endl; });
    auto* schedBtn = addButton(actions, "Schedule", "clock", "btn btn-secondary", nullptr);
    schedBtn->onClick = [schedBtn]() {
        reportScheduler = !reportScheduler;
        // No rebuild needed — toggle state only affects delivery toggles below
    };

    auto* reportGrid = main->add<Panel>("three-grid");
    auto* exec = reportGrid->add<Panel>("panel-card");
    addSectionHeader(exec, "Executive Summary");
    addMetricStrip(exec, "Risk reduction", "-31%", "ok", 0.69f);
    addMetricStrip(exec, "Open exceptions", "14", "warning", 0.38f);

    auto* audit = reportGrid->add<Panel>("panel-card");
    addSectionHeader(audit, "Audit Packet");
    addMetricStrip(audit, "Evidence sealed", "98%", "info", 0.98f);
    addMetricStrip(audit, "Reviewer sign-off", "3 of 4", "warning", 0.75f);

    auto* delivery = reportGrid->add<Panel>("panel-card");
    addSectionHeader(delivery, "Delivery");
    addToggleRow(delivery, "Weekly digest", "Send leadership summary each Monday.", digestEnabled);
    addToggleRow(delivery, "Auto scheduler", "Generate monthly compliance packet.", reportScheduler);

    addSectionHeader(main, "Recent Reports");
    auto* table = main->add<Panel>("data-table");
    addTableRow(table, "table-row table-head", "Report", "Coverage", "Owner", "Status");
    addTableRow(table, "table-row", "May DLP Executive Summary", "Enterprise", "CISO", "Ready");
    addTableRow(table, "table-row", "PCI Evidence Packet", "Finance", "Audit", "Review");
    addTableRow(table, "table-row", "Endpoint Exception Register", "Workstations", "IT Ops", "Scheduled");
}

// ============================================================
//  Settings
// ============================================================

static void buildSettings(Widget* content) {
    buildTopBar(content, "Settings", "Console preferences, integrations, and enforcement defaults", false);

    auto* main = content->add<Panel>("main-scroll");

    auto* settingsGrid = main->add<Panel>("dashboard-grid");
    auto* enforcement = settingsGrid->add<Panel>("panel-card panel-wide");
    addSectionHeader(enforcement, "Enforcement Defaults", "Global behavior for newly discovered risks");
    addToggleRow(enforcement, "Block mode", "Apply blocking actions when confidence exceeds policy threshold.", blockMode, true);
    addToggleRow(enforcement, "Quarantine mode", "Move restricted files to isolated encrypted storage.", quarantineMode);
    addToggleRow(enforcement, "Endpoint isolation", "Disconnect endpoints with repeated critical violations.", endpointIsolation);
    addToggleRow(enforcement, "Leadership digest", "Send summarized posture notes to executives.", digestEnabled);

    auto* integrations = settingsGrid->add<Panel>("panel-card panel-side");
    addSectionHeader(integrations, "Integrations", "Connected systems");
    addMetricStrip(integrations, "SIEM stream", "Healthy", "ok", 1.0f);
    addMetricStrip(integrations, "Cloud sync", cloudSync ? "Enabled" : "Paused", cloudSync ? "info" : "warning", cloudSync ? 0.88f : 0.42f);
    addMetricStrip(integrations, "Directory sync", "Healthy", "ok", 0.96f);
    auto* cloudBtn = addButton(integrations, cloudSync ? "Pause Cloud Sync" : "Enable Cloud Sync", "cloud", "btn btn-secondary btn-full",
              nullptr);
    cloudBtn->onClick = [cloudBtn]() {
        cloudSync = !cloudSync;
        // Update button label in-place
        auto* label = static_cast<Text*>(cloudBtn->children.back().get());
        label->content = cloudSync ? "Pause Cloud Sync" : "Enable Cloud Sync";
    };

    addSectionHeader(main, "Retention And Privacy");
    auto* privacy = main->add<Panel>("data-table");
    addTableRow(privacy, "table-row table-head", "Setting", "Value", "Scope", "Owner");
    addTableRow(privacy, "table-row", "Evidence retention", "365 days", "All incidents", "Legal");
    addTableRow(privacy, "table-row", "Low-risk telemetry", "30 days", "Endpoints", "IT Ops");
    addTableRow(privacy, "table-row", "PII masking", "Enabled", "Reports", "Compliance");
}

// ============================================================
//  Main
// ============================================================

static bool parseBackendArg(const std::string& value, RenderBackendType& backend) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });

    if (normalized == "auto") {
        backend = RenderBackendType::Auto;
    } else if (normalized == "vulkan") {
        backend = RenderBackendType::Vulkan;
    } else if (normalized == "d3d12" || normalized == "direct12" ||
               normalized == "directx12" || normalized == "direct3d12") {
        backend = RenderBackendType::Direct3D12;
    } else if (normalized == "metal") {
        backend = RenderBackendType::Metal;
    } else if (normalized == "compatibility") {
        backend = RenderBackendType::Compatibility;
    } else {
        return false;
    }
    return true;
}

static int runVulkanProbe() {
    auto result = Renderer::probeVulkanRuntime("DataLeak Guard Vulkan Probe");

    std::cout << "========================================" << std::endl;
    std::cout << "  Vulkan probe" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Loader:   " << (result.loaderAvailable ? "ok" : "missing") << std::endl;
    std::cout << "Window:   " << (result.windowCreated ? "ok" : "failed") << std::endl;
    std::cout << "Instance: " << (result.instanceCreated ? "ok" : "failed") << std::endl;
    std::cout << "Surface:  " << (result.surfaceCreated ? "ok" : "failed") << std::endl;
    std::cout << "GPUs:     " << result.physicalDeviceCount << std::endl;
    for (const auto& device : result.deviceNames) {
        std::cout << "  - " << device << std::endl;
    }
    if (!result.error.empty()) {
        std::cerr << "Error: " << result.error << std::endl;
    }

    return result.loaderAvailable && result.instanceCreated &&
           result.surfaceCreated && result.physicalDeviceCount > 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    RenderBackendType requestedBackend = Renderer::defaultBackend();
    bool backendSpecified = false;
    bool probeVulkan = false;
    int frameLimit = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--probe-vulkan") {
            probeVulkan = true;
        } else if (arg.rfind("--backend=", 0) == 0) {
            if (!parseBackendArg(arg.substr(10), requestedBackend)) {
                std::cerr << "Unknown backend: " << arg.substr(10) << std::endl;
                return 2;
            }
            backendSpecified = true;
        } else if (arg.rfind("--frames=", 0) == 0) {
            frameLimit = std::max(0, std::stoi(arg.substr(9)));
        }
    }

    if (probeVulkan) {
        return runVulkanProbe();
    }

    Application app;
    if (backendSpecified) {
        app.setBackend(requestedBackend);
    }

    if (!app.init("DataLeak Guard - Enterprise DLP", 1400, 900)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    bool fontLoaded = false;
    constexpr float baseFontAtlasSize = 32.0f;
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/SFPro.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        nullptr
    };
    for (int i = 0; fontPaths[i]; i++) {
        if (app.renderer().loadFont(fontPaths[i], baseFontAtlasSize)) {
            fontLoaded = true;
            std::cout << "Loaded font: " << fontPaths[i] << std::endl;
            break;
        }
    }
    if (!fontLoaded && app.renderer().loadFontFromMemory(nullptr, 0, baseFontAtlasSize, "default")) {
        fontLoaded = true;
        std::cout << "Using optimized pre-baked font atlas" << std::endl;
    }
    if (!fontLoaded) {
        std::cerr << "Warning: No font loaded, text will not render" << std::endl;
    }

    app.addStylesheet(dataleakguardEmbeddedThemeCss());

    app.addRoute("/dashboard", [](Application& appRef, Widget* content) {
        buildDashboard(appRef, content);
    });
    app.addRoute("/scanner", [](Application&, Widget* content) { buildScanner(content); });
    app.addRoute("/alerts", [](Application&, Widget* content) { buildAlerts(content); });
    app.addRoute("/rules", [](Application&, Widget* content) { buildRules(content); });
    app.addRoute("/reports", [](Application&, Widget* content) { buildReports(content); });
    app.addRoute("/settings", [](Application&, Widget* content) { buildSettings(content); });
    app.setNotFoundRoute([](Application&, Widget* content) {
        buildTopBar(content, "Page Not Found", "The requested console view is not registered", false);
        auto* main = content->add<Panel>("main-scroll");
        auto* card = main->add<Panel>("panel-card");
        addSectionHeader(card, "Route missing", "Register the page before navigating to it.");
    });
    app.navigate("/dashboard");

    app.on(UIEventType::RouteChanged, [](UIEvent& event) {
        std::cout << "[DataLeak Guard] Route: "
                  << (event.previousRoute.empty() ? "<start>" : event.previousRoute)
                  << " -> " << event.route << std::endl;
    });

    int renderedFrames = 0;

    app.onUpdate = [&](float dt) {
        // ── Scan progress: update live references directly ──
        if (scannerRunning) {
            bool wasRunning = scannerRunning;
            scanProgress = std::min(1.0f, scanProgress + dt * 0.055f);
            if (scanProgress >= 1.0f) {
                scannerRunning = false;
            }

            // Direct widget updates — NO full rebuild!
            if (shell.scanProgressBar) {
                shell.scanProgressBar->progress = scanProgress;
            }
            if (shell.scanCaption) {
                shell.scanCaption->content = percentText(scanProgress) + " complete - 184,000 objects in queue";
            }

            // Scan completed — update button/icon states
            if (wasRunning && !scannerRunning) {
                if (shell.scanOrbIcon) shell.scanOrbIcon->glyph = "scanner";
                if (shell.scanHeroTitle) shell.scanHeroTitle->content = "Scan complete";
            }
            app.requestRedraw();
        }

        // ── Build shell once, then only rebuild on route change ──
        if (!shell.initialized) {
            // First frame: build entire shell
            auto* root = app.root();
            root->clearChildren();
            root->reserveChildren(2);

            buildSidebar(root, app);

            shell.contentArea = root->add<Panel>("content");
            shell.contentArea->reserveChildren(3);
            app.renderRoute(shell.contentArea);
            buildStatusBar(shell.contentArea);

            shell.initialized = true;
            uiDirty = false;
        } else if (app.routeDirty()) {
            // Route change: update nav highlights + rebuild content only
            shell.clearPageRefs();
            shell.updateNavHighlights(app.currentRoute());

            // Only clear and rebuild the content area — sidebar is retained
            shell.contentArea->clearChildren();
            shell.contentArea->reserveChildren(3);
            app.renderRoute(shell.contentArea);
            buildStatusBar(shell.contentArea);

            uiDirty = false;
        }

        if (frameLimit > 0 && ++renderedFrames >= frameLimit) {
            app.running = false;
        }
    };

    std::cout << "========================================" << std::endl;
    std::cout << "  DataLeak Guard v1.0" << std::endl;
    std::cout << "  Built with FluxUI Framework" << std::endl;
    std::cout << "  GPU-Accelerated | CSS-Styled" << std::endl;
    std::cout << "  Active backend: " << app.renderer().activeBackendName() << std::endl;
    std::cout << "========================================" << std::endl;

    app.run();
    app.shutdown();

    return 0;
}
