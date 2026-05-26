#include "fluxui/FluxUI.h"
#include <chrono>
#include "embedded_font_atlas.h"
#include <fstream>
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
#include <thread>
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#endif
static std::atomic<size_t> g_allocation_count{0};
static std::atomic<size_t> g_allocated_bytes{0};
static bool g_tracking_allocations = false;
static size_t g_allocation_sizes[100];
static std::atomic<size_t> g_recorded_allocs{0};
static std::atomic<int> g_current_iteration{-1};

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static void printSymbol(void* address, FILE* f) {
    static bool init = false;
    if (!init) {
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        init = true;
    }
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)address, &displacement, pSymbol)) {
        fprintf(f, "    %s + 0x%llx\n", pSymbol->Name, displacement);
    } else {
        fprintf(f, "    %p (unknown)\n", address);
    }
}

void* operator new(size_t size) {
    if (g_tracking_allocations) {
        size_t idx = g_recorded_allocs.fetch_add(1, std::memory_order_relaxed);
        if (idx < 100) {
            g_allocation_sizes[idx] = size;
        }
        g_allocation_count.fetch_add(1, std::memory_order_relaxed);
        g_allocated_bytes.fetch_add(size, std::memory_order_relaxed);

        if (idx < 50) {
            FILE* f = fopen("benchmark_allocations.log", "a");
            if (f) {
                void* stack[10];
                unsigned short frames = CaptureStackBackTrace(1, 10, stack, NULL);
                fprintf(f, "Allocation %zu (iteration %d): %zu bytes, caller stack:\n", idx, g_current_iteration.load(), size);
                for (unsigned short i = 0; i < frames; ++i) {
                    printSymbol(stack[i], f);
                }
                fclose(f);
            }
        }
    }
    void* p = malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    free(p);
}

void operator delete(void* p, size_t) noexcept {
    free(p);
}

using namespace FluxUI;
static bool uiDirty = true;
static bool scannerRunning = false;
static float scanProgress = 0.67f;
static std::string dbFilesMonitored = "12,847";
static std::string dbThreatsBlocked = "47";
static std::string dbUsbTransfers = "156";
static std::string dbComplianceScore = "94%";
static bool blockMode = true;
static bool quarantineMode = true;
static bool endpointIsolation = false;
static bool digestEnabled = true;
static bool reportScheduler = true;
static bool cloudSync = true;
static std::array<bool, 5> ruleEnabled = {true, true, true, false, true};
static const char* navItems[] = {
    "Dashboard", "Scanner", "Alerts", "Rules", "Reports", "Settings", "Blink UI"
};
static const char* navRoutes[] = {
    "/dashboard", "/scanner", "/alerts", "/rules", "/reports", "/settings", "/blink"
};
static const char* navIcons[] = {
    "dashboard", "scanner", "alert", "rules", "report", "settings", "rules"
};
struct RetainedShell {
    bool initialized = false;
    Panel* sidebar = nullptr;
    Panel* contentArea = nullptr;
    Panel* statusBar = nullptr;
    Button* navButtons[7] = {};
    Icon*   navIcons[7] = {};
    Text*   navTexts[7] = {};
    Text* postureTitle = nullptr;
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
    void updateNavHighlights(const std::string& activeRoute) {
        for (int i = 0; i < 7; i++) {
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
    void syncPosture() {
        if (postureTitle) {
            postureTitle->content = blockMode ? "Containment active" : "Monitor only";
        }
    }
};
static RetainedShell shell;
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
    auto* button = parent->button("", classes, std::move(onClick));
    button->reserveChildren(icon.empty() ? 1 : 2);
    std::string toneClass;
    if (classes.find("btn-primary") != std::string::npos) {
        toneClass = " primary-control";
    } else if (classes.find("btn-danger") != std::string::npos) {
        toneClass = " danger-control";
    }
    if (!icon.empty()) {
        button->addIcon(icon, "btn-icon" + toneClass);
    }
    button->text(label, "btn-label" + toneClass);
    return button;
}
static void addPill(Widget* parent, const std::string& text, const std::string& tone) {
    parent->panel("pill pill-" + tone, 1)->text(text, "pill-text");
}
static ProgressBar* addProgress(Widget* parent,
                                float value,
                                const std::string& cls,
                                const Color& color) {
    return parent->progress(value, cls, color);
}
static void addSectionHeader(Widget* parent,
                             const std::string& title,
                             const std::string& subtitle = "") {
    auto* header = parent->panel("section-head", 1);
    auto* copy = header->panel("section-copy", subtitle.empty() ? 1 : 2);
    copy->h2(title, "section-title");
    if (!subtitle.empty()) {
        copy->p(subtitle, "section-subtitle");
    }
}
static void addMetricStrip(Widget* parent,
                           const std::string& label,
                           const std::string& value,
                           const std::string& tone,
                           float progress) {
    auto* row = parent->panel("metric-strip", 2);
    auto* copy = row->panel("metric-copy", 2);
    copy->text(label, "metric-label");
    copy->text(value, "metric-value");
    addProgress(row, progress, "progress-line", toneColor(tone));
}
static void addActivityRow(Widget* parent,
                           const std::string& icon,
                           const std::string& text,
                           const std::string& meta,
                           const std::string& severity) {
    auto* row = parent->panel("activity-row", 3);
    row->panel("activity-icon alert-" + severity, 1)->addIcon(icon, "activity-mark mark-" + severity);
    row->text(text, "activity-text");
    row->text(meta, "activity-time");
}
static void addToggleRow(Widget* parent,
                         const std::string& title,
                         const std::string& description,
                         bool& value,
                         bool affectsPosture = false) {
    auto* row = parent->panel("toggle-row", 2);
    auto* copy = row->panel("toggle-copy", 2);
    copy->text(title, "toggle-title");
    copy->text(description, "toggle-desc");
    auto* toggle = row->button("", value ? "toggle toggle-on" : "toggle toggle-off");
    toggle->panel("toggle-knob");
    toggle->onClick = [&value, toggle, affectsPosture]() {
        value = !value;
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
    auto* row = parent->panel("rule-row", 3);
    auto* left = row->panel("rule-main", 2);
    left->text(name, "rule-name");
    left->text(detail, "rule-detail");
    auto* meta = row->panel("rule-meta", 2);
    addPill(meta, scope, "info");
    addPill(meta, hits, "warning");
    auto* toggle = row->button("", ruleEnabled[index] ? "toggle toggle-on" : "toggle toggle-off");
    toggle->panel("toggle-knob");
    toggle->onClick = [index, toggle]() {
        ruleEnabled[index] = !ruleEnabled[index];
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
    auto* row = parent->panel(cls, 4);
    row->text(c1, "table-cell table-main");
    row->text(c2, "table-cell");
    row->text(c3, "table-cell");
    row->text(c4, "table-cell table-right");
}
static void buildTopBar(Widget* content,
                        const std::string& title,
                        const std::string& subtitle,
                        bool showSearch = true) {
    auto* topBar = content->panel("top-bar");
    auto* titleGroup = topBar->panel("title-group");
    titleGroup->h1(title, "page-title");
    titleGroup->p(subtitle, "page-subtitle");
    auto* tools = topBar->panel("top-tools");
    if (showSearch) {
        tools->input("search", "Search incidents, hosts, policies...", "search-box");
    }
    addButton(tools, "Export", "download", "btn btn-secondary btn-compact",
              []() { std::cout << "[DataLeak Guard] Export queued" << std::endl; });
}
static void buildStatusBar(Widget* content) {
    auto* status = content->panel("status-bar");
    auto* live = status->panel("status-cluster");
    live->panel("status-dot");
    live->span("Live prevention enabled", "status-text");
    status->span("12,847 files monitored", "status-text");
    status->span("47 blocks today", "status-text");
    status->span("Last sync: 38 sec ago", "status-text status-right");
}
static void buildSidebar(Widget* root, Application& app) {
    auto* sidebar = root->panel("sidebar", 3);
    shell.sidebar = sidebar;
    auto* header = sidebar->panel("sidebar-header", 1);
    auto* logoRow = header->panel("logo-row", 2);
    logoRow->panel("logo-mark", 1)->addIcon("shield", "logo-icon");
    auto* logoCopy = logoRow->panel("logo-copy", 2);
    logoCopy->text("DataLeak Guard", "sidebar-logo");
    logoCopy->text("Enterprise DLP Console", "sidebar-subtitle");
    auto* nav = sidebar->panel("nav-section", 8);
    nav->text("WORKSPACE", "nav-label");
    const std::string activeRoute = app.currentRoute();
    for (int i = 0; i < 7; i++) {
        std::string cls = "nav-item";
        bool active = activeRoute == navRoutes[i];
        if (active) cls += " active";
        auto* item = nav->button("", cls);
        item->reserveChildren(2);
        auto* icon = item->addIcon(navIcons[i], active ? "nav-icon active-icon" : "nav-icon");
        auto* text = item->text(navItems[i], active ? "nav-text active-text" : "nav-text");
        std::string route = navRoutes[i];
        item->onClick = [&app, route]() {
            app.navigate(route);
        };
        shell.navButtons[i] = item;
        shell.navIcons[i] = icon;
        shell.navTexts[i] = text;
    }
    auto* posture = sidebar->panel("posture-card", 4);
    posture->text("POSTURE", "nav-label");
    auto* postureTitle = posture->text(blockMode ? "Containment active" : "Monitor only", "posture-title");
    shell.postureTitle = postureTitle;
    posture->text("4 high-risk events require review", "posture-copy");
    auto* posturePills = posture->panel("posture-pills", 2);
    addPill(posturePills, "DLP", "ok");
    addPill(posturePills, "SIEM", "info");
}
static void buildRetainedShell(Application& app) {
    auto* root = app.root();
    if (!root) return;
    root->clearChildren();
    root->reserveChildren(2);
    buildSidebar(root, app);
    shell.contentArea = root->panel("content", 3);
    app.renderRoute(shell.contentArea);
    buildStatusBar(shell.contentArea);
    shell.initialized = true;
    uiDirty = false;
}
static void rebuildActiveRoute(Application& app) {
    if (!shell.contentArea) return;
    shell.clearPageRefs();
    shell.updateNavHighlights(app.currentRoute());
    shell.contentArea->clearChildren();
    shell.contentArea->reserveChildren(3);
    app.renderRoute(shell.contentArea);
    buildStatusBar(shell.contentArea);
    uiDirty = false;
}
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
    blockHeroBtn->onClick = [blockHeroBtn]() {
        blockMode = !blockMode;
        auto* label = static_cast<Text*>(blockHeroBtn->children.back().get());
        label->content = blockMode ? "Monitor Mode" : "Block Mode";
        shell.syncPosture();
    };
    auto* statsRow = main->panel("stats-row");
    statsRow->add<StatCard>("Files Monitored", dbFilesMonitored, "+234 this week", Color::fromHex("#37C6A3"));
    statsRow->add<StatCard>("Threats Blocked", dbThreatsBlocked, "3 critical today", Color::fromHex("#FF5D6C"));
    statsRow->add<StatCard>("USB Transfers", dbUsbTransfers, "12 flagged", Color::fromHex("#F3B64B"));
    statsRow->add<StatCard>("Compliance Score", dbComplianceScore, "Above target", Color::fromHex("#6AA9FF"));

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
    auto* activity = main->panel("activity-panel");
    addActivityRow(activity, "usb", "Sensitive contract archive blocked on USB-A72", "2 min ago", "danger");
    addActivityRow(activity, "card", "Credit card pattern detected in export.csv", "15 min ago", "danger");
    addActivityRow(activity, "id", "Employee IDs found in shared Sales folder", "1 hour ago", "warning");
    addActivityRow(activity, "check", "Weekly compliance scan completed at 94%", "3 hours ago", "ok");
    addActivityRow(activity, "rules", "Rule pack updated: Financial identifiers", "5 hours ago", "info");
}
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
        auto* label = static_cast<Text*>(startBtn->children.back().get());
        auto* icon = static_cast<Icon*>(startBtn->children.front().get());
        label->content = scannerRunning ? "Pause Scan" : "Start Scan";
        icon->glyph = scannerRunning ? "pause" : "play";
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
        auto* row = queue->panel("incident-row", 4);
        row->panel("activity-icon alert-danger", 1)->addIcon(rowData[0], "activity-mark mark-danger");
        auto* copy = row->panel("incident-copy", 2);
        copy->text(rowData[1], "incident-title");
        copy->text(std::string(rowData[2]) + " - " + rowData[4] + " ago", "incident-meta");
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
static void buildBlinkParity(Application& app, Widget* content) {
    buildTopBar(content, "Blink UI", "Testing native Chromium Blink user-agent element behaviors", false);
    auto* main = content->add<Panel>("main-scroll");
    
    auto* grid = main->panel("blink-showcase-grid", 2);
    
    auto* col1 = grid->panel("blink-showcase-column", 2);
    
    // Disclosure Widgets
    auto* cardDetails = col1->panel("blink-section-card", 3);
    cardDetails->text("Disclosure Widgets (<details> & <summary>)", "blink-section-title");
    
    auto* details1 = cardDetails->add<Details>(false);
    details1->summary("Technical Specifications");
    details1->text("This widget operates in high fidelity parity with Chrome's native Blink engine disclosure element. Expanding this item triggers a dynamic layout pass where child nodes are set to visible/invisible.");
    
    auto* details2 = cardDetails->add<Details>(true);
    details2->summary("Enforcement Engine Architecture");
    details2->text("The engine intercepts file modification and memory allocation streams on target endpoints. Real-time classification models verify standard compliance parameters.");
    
    // Hyperlinks
    auto* cardLinks = col1->panel("blink-section-card", 6);
    cardLinks->text("Hyperlinks (<a> Anchor)", "blink-section-title");
    cardLinks->a("Go to Chromium Blink Engine Source", "https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/");
    cardLinks->br();
    cardLinks->a("Go to FluxUI Documentation Portal", "https://github.com/camiloGHS21/FluxUI");
    cardLinks->br();
    cardLinks->a("Go to Google DeepMind Official Site", "https://deepmind.google");

    // Progress Bars
    auto* cardProgress = col1->panel("blink-section-card", 5);
    cardProgress->text("Progress Bars (<progress>)", "blink-section-title");
    
    cardProgress->text("Determinate (65%):", "page-subtitle");
    cardProgress->progressElement(0.65f, 1.0f, "progress");

    cardProgress->text("Indeterminate (Animated):", "page-subtitle");
    cardProgress->progressElement(-1.0f, 1.0f, "progress");

    auto* col2 = grid->panel("blink-showcase-column", 5);
    
    // Progress Meters
    auto* cardMeters = col2->panel("blink-section-card", 7);
    cardMeters->text("Progress Meters (<meter>)", "blink-section-title");
    
    cardMeters->text("Optimal Range (Green):", "page-subtitle");
    auto* m1 = cardMeters->meter(0.6f, 0.0f, 1.0f, "meter");
    m1->low = 0.1f;
    m1->high = 0.9f;
    m1->optimum = 0.5f;

    cardMeters->text("Sub-optimal Warning (Yellow):", "page-subtitle");
    auto* m2 = cardMeters->meter(0.7f, 0.0f, 1.0f, "meter");
    m2->low = 0.3f;
    m2->high = 0.9f;
    m2->optimum = 0.1f;

    cardMeters->text("Out-of-bounds Alert (Red):", "page-subtitle");
    auto* m3 = cardMeters->meter(0.15f, 0.0f, 1.0f, "meter");
    m3->low = 0.3f;
    m3->high = 0.8f;
    m3->optimum = 0.5f;

    // Horizontal Rules & Breaks
    auto* cardSeparators = col2->panel("blink-section-card", 5);
    cardSeparators->text("Horizontal Rules & Breaks (<hr> & <br>)", "blink-section-title");
    cardSeparators->text("Line 1 above break", "page-subtitle");
    cardSeparators->br();
    cardSeparators->text("Line 2 below break", "page-subtitle");
    cardSeparators->hr();
    cardSeparators->text("Content below horizontal divider rule", "page-subtitle");

    // Dialog Box Trigger Card
    auto* cardDialog = col2->panel("blink-section-card", 3);
    cardDialog->text("Overlay Dialogs (<dialog>)", "blink-section-title");
    cardDialog->text("Click the button below to display a modal dialog overlay styled with custom positions, borders, and shadows.", "page-subtitle");
    
    // Dialog modal widget
    auto* dl = main->dialog("dialog");
    dl->text("System Access Requested", "dialog-title");
    dl->text("An external process is attempting to load system assets for rendering evaluation. Confirm authorization to proceed.", "dialog-text");
    
    auto* dialogButtons = dl->panel("posture-pills", 2);
    addButton(dialogButtons, "Authorize", "shield", "btn btn-primary btn-small", [dl]() {
        std::cout << "[Blink UI] Dialog Authorized" << std::endl;
        dl->close();
    });
    addButton(dialogButtons, "Decline", "alert", "btn btn-danger btn-small", [dl]() {
        std::cout << "[Blink UI] Dialog Declined" << std::endl;
        dl->close();
    });

    addButton(cardDialog, "Show Dialog", "alert", "btn btn-primary", [dl]() {
        std::cout << "[Blink UI] Showing Modal Dialog" << std::endl;
        dl->showModal();
    });

    // Backdrop Filters Glassmorphism Card
    auto* cardGlass = col2->panel("blink-section-card", 2);
    cardGlass->text("Backdrop Filters (Glassmorphism)", "blink-section-title");
    auto* glassContainer = cardGlass->panel("glass-container", 1);
    auto* glassOverlay = glassContainer->panel("glass-overlay", 2);
    glassOverlay->text("Frosted Glass Effect", "glass-title");
    glassOverlay->text("Blink visual filter rendering in high-fidelity on C++.", "glass-text");

    // Resize Observer Card (Blink Parity)
    auto* cardResize = col2->panel("blink-section-card", 4);
    cardResize->text("Resize Observer (<ResizeObserver>)", "blink-section-title");
    cardResize->text("Observed Container Query box. Toggle size to trigger observer updates.", "page-subtitle");

    auto* observedBox = cardResize->panel("posture-card", 2);
    observedBox->css("width: 260px; height: 110px; display: flex; flex-direction: column; gap: 8px; justify-content: center; align-items: center; border: 1px solid rgba(255,255,255,0.15); background-color: rgba(237,243,248,0.03); border-radius: 6px;");

    auto* statusText = observedBox->text("Narrow Layout", "pill-text");
    statusText->classes("reactive-badge pill-warning pill-text");
    auto* sizeText = observedBox->text("Size: 260px x 110px", "page-subtitle");

    // Static/persisted ResizeObserver so it stays alive throughout the app lifecycle
    static ResizeObserver* ro = nullptr;
    if (ro) {
        delete ro;
    }
    ro = new ResizeObserver([statusText, sizeText](const std::vector<ResizeObserverEntry>& entries, ResizeObserver&) {
        for (const auto& entry : entries) {
            float w = entry.contentRect.w;
            float h = entry.contentRect.h;
            
            char buf[128];
            sprintf(buf, "Size: %.0fpx x %.0fpx", w, h);
            sizeText->content = buf;
            sizeText->markStyleDirty();

            if (w > 350.0f) {
                statusText->content = "Wide Layout Badge (Reactive)";
                statusText->classes("reactive-badge pill-ok pill-text");
            } else {
                statusText->content = "Narrow Layout Badge (Reactive)";
                statusText->classes("reactive-badge pill-warning pill-text");
            }
            statusText->markStyleDirty();
        }
    });

    ro->observe(observedBox);


    addButton(cardResize, "Toggle Size (Resize)", "expand", "btn btn-secondary btn-full", [observedBox, isExpanded = false]() mutable {
        isExpanded = !isExpanded;
        if (isExpanded) {
            observedBox->css("width: 400px; flex-shrink: 0; height: 110px; display: flex; flex-direction: column; gap: 8px; justify-content: center; align-items: center; border: 1px solid rgba(255,255,255,0.15); background-color: rgba(200,50,50,0.2); border-radius: 6px;");
        } else {
            observedBox->css("width: 260px; flex-shrink: 0; height: 110px; display: flex; flex-direction: column; gap: 8px; justify-content: center; align-items: center; border: 1px solid rgba(255,255,255,0.15); background-color: rgba(50,200,50,0.2); border-radius: 6px;");
        }
    });
}
static void runKeymapBenchmark(Application& app) {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "         FLUXUI KEYMAP BUBBLING BENCHMARK" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Register simulated actions
    for (int i = 0; i < 100; ++i) {
        std::string actionName = "action:simulated_" + std::to_string(i);
        app.registerAction(actionName, [](Application&, const std::string&) {});
    }

    // Add keymap bindings
    std::stringstream ss;
    ss << "[";
    for (int i = 0; i < 100; ++i) {
        if (i > 0) ss << ",";
        ss << "{\n";
        ss << "  \"context\": \".context-level-" << i << "\",\n";
        ss << "  \"bindings\": {\n";
        ss << "    \"ctrl-shift-alt-" << (char)('a' + (i % 26)) << "\": \"action:simulated_" << i << "\"\n";
        ss << "  }\n";
        ss << "}";
    }
    ss << "]";
    app.addKeymap(ss.str());

    // 2. Build nested widget hierarchy (100 levels)
    Widget* current = app.root();
    for (int i = 0; i < 100; ++i) {
        auto* next = current->add<Panel>("panel-level-" + std::to_string(i));
        next->className = "context-level-" + std::to_string(i);
        current = next;
    }
    current->focused = true;

    std::cout << "Widget Tree Depth: 100" << std::endl;
    std::cout << "Registered Keymap Entry Contexts: 100" << std::endl;
    std::cout << "Simulating 100,000 keystroke dispatches..." << std::endl;

    // Test a keystroke that matches context-level-99:
    // "ctrl-shift-alt-v" (for 99 % 26 = 21 -> 'a'+21 = 'v')
    int testKeyCode = 'V';
    int testModifiers = MOD_CTRL | MOD_SHIFT | MOD_ALT;

    auto t_start = std::chrono::high_resolution_clock::now();
    constexpr int iterations = 100000;
    int successCount = 0;
    for (int i = 0; i < iterations; ++i) {
        if (app.dispatchKeyAction(testKeyCode, testModifiers)) {
            successCount++;
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    double avgUs = (durationMs * 1000.0) / iterations;

    std::cout << "Benchmark complete." << std::endl;
    std::cout << "Total time: " << durationMs << " ms for " << iterations << " iterations" << std::endl;
    std::cout << "Average latency per key dispatch: " << avgUs << " microseconds" << std::endl;
    std::cout << "Bubbling lookup resolved: " << (successCount > 0 ? "YES" : "NO") << std::endl;
    if (avgUs < 1000.0) {
        std::cout << "STATUS: PASS (Latency is sub-millisecond: " << avgUs << " us < 1000 us)" << std::endl;
    } else {
        std::cout << "STATUS: FAIL (Latency is not sub-millisecond)" << std::endl;
    }
    std::cout << "==================================================\n" << std::endl;
}

static void runStyleResolutionBenchmark(Application& app) {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "      FLUXUI STYLE RESOLUTION BENCHMARK" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Create a stylesheet with complex nested rules
    StyleSheet sheet;
    sheet.parse(R"(
        * { box-sizing: border-box; display: block; }
        .container { padding: 20px; background-color: #f0f0f0; }
        .sidebar { width: 250px; background-color: #333; color: white; }
        .content { flex: 1; padding: 10px; }
        .button { display: flex; padding: 10px 20px; border-radius: 4px; background-color: #007acc; color: #ffffff; }
        .button:hover { background-color: #005999; }
        .button:active { background-color: #004080; }
        #main-title { font-size: 24px; font-weight: bold; color: #222; }
        div { margin: 5px; }
        panel { display: flex; flex-direction: row; }
        .text-input { border: 1px solid #ccc; padding: 8px; }
        .text-input:focus { border-color: #007acc; }
    )");

    // 2. Build a tree of widgets (e.g. 150 widgets)
    std::vector<Widget*> allWidgets;
    Widget* root = app.root();
    root->clearChildren(true);
    root->type = "div";
    root->className = "container";
    root->id = "app-root";
    allWidgets.push_back(root);

    // Create a branch of nested panels, text and buttons
    Widget* current = root;
    for (int i = 0; i < 50; ++i) {
        Panel* panel = current->add<Panel>("sidebar");
        panel->id = "panel_" + std::to_string(i);
        allWidgets.push_back(panel);
        
        Text* txt = panel->add<Text>("Item " + std::to_string(i), "content");
        txt->id = "text_" + std::to_string(i);
        allWidgets.push_back(txt);

        Button* btn = panel->add<Button>("Click Me", "button");
        btn->id = "btn_" + std::to_string(i);
        allWidgets.push_back(btn);

        current = panel; // nest further
    }

    std::ofstream log("style_benchmark.log");
    auto log_print = [&](const std::string& msg) {
        std::cout << msg << std::endl;
        log << msg << std::endl;
    };

    log_print("Total Widgets in tree: " + std::to_string(allWidgets.size()));
    log_print("Resolving styles 1,000 times (simulating 1,000 full-tree recalculations)...");

    auto t_start = std::chrono::high_resolution_clock::now();
    constexpr int iterations = 1000;
    log_print("  Warming up style cache (Iteration 0)...");
    g_tracking_allocations = false;
    log_print("  Calling root->markStyleDirtyRecursive()...");
    root->markStyleDirtyRecursive();
    log_print("  Calling root->resolveStyles(sheet)...");
    root->resolveStyles(sheet);
    log_print("  Finished root->resolveStyles(sheet).");


    g_allocation_count = 0;
    g_allocated_bytes = 0;
    g_recorded_allocs = 0;
    log_print("  Iteration 0...");
    g_tracking_allocations = true;
    try {
        for (int it = 0; it < iterations; ++it) {
            g_current_iteration = it;
            if (it > 0 && it % 1000 == 0) {
                g_tracking_allocations = false;
                log_print("  Iteration " + std::to_string(it) + "...");
                g_tracking_allocations = true;
            }
            // Force complete style dirtying
            root->markStyleDirtyRecursive();
            
            // Resolve styles recursively
            root->resolveStyles(sheet);
        }
        g_tracking_allocations = false;
        log_print("  Finished loop successfully!");
        log_print("Total heap allocations during benchmark loop: " + std::to_string(g_allocation_count.load()) + " (" + std::to_string(g_allocated_bytes.load()) + " bytes)");

        size_t limit = std::min((size_t)g_recorded_allocs.load(), (size_t)20);
        log_print("First " + std::to_string(limit) + " allocation sizes:");
        for (size_t i = 0; i < limit; ++i) {
            log_print("  [" + std::to_string(i) + "]: " + std::to_string(g_allocation_sizes[i]) + " bytes");
        }
    } catch (const std::exception& e) {
        g_tracking_allocations = false;
        log_print("  CRASHED WITH EXCEPTION: " + std::string(e.what()));
        return;
    } catch (...) {
        g_tracking_allocations = false;
        log_print("  CRASHED WITH UNKNOWN EXCEPTION!");
        return;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    double avgUsPerFrame = (durationMs * 1000.0) / iterations;
    double avgUsPerWidget = (durationMs * 1000.0) / (iterations * allWidgets.size());

    log_print("Style Resolution Benchmark complete.");
    log_print("Total time: " + std::to_string(durationMs) + " ms for " + std::to_string(iterations) + " iterations");
    log_print("Average latency per full tree style resolution: " + std::to_string(avgUsPerFrame) + " microseconds");
    log_print("Average latency per single widget style resolution: " + std::to_string(avgUsPerWidget) + " microseconds");
    
    // Quick sanity checks
    bool correctness = true;
    for (Widget* w : allWidgets) {
        if (w->type == "button") {
            if (!w->computedStyle->hasColor ||
                w->computedStyle->color.r != 1.0f ||
                w->computedStyle->color.g != 1.0f ||
                w->computedStyle->color.b != 1.0f ||
                w->computedStyle->color.a != 1.0f) {
                std::cout << "Button id=" << w->id << " failed color verification. hasColor=" << w->computedStyle->hasColor
                          << " r=" << w->computedStyle->color.r
                          << " g=" << w->computedStyle->color.g
                          << " b=" << w->computedStyle->color.b
                          << " a=" << w->computedStyle->color.a << std::endl;
                correctness = false;
            }
        }
    }
    log_print("Correctness verification: " + std::string(correctness ? "PASS" : "FAIL"));
    
    if (avgUsPerWidget < 1.0) {
        log_print("STATUS: PASS (Sub-microsecond style resolution: " + std::to_string(avgUsPerWidget) + " us < 1.0 us)");
    } else {
        log_print("STATUS: WARN (Style resolution latency is " + std::to_string(avgUsPerWidget) + " us)");
    }
    log_print("==================================================\n");
}

static void validateStyleInvalidationSets(Application& app) {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "    VALIDATING STYLE INVALIDATION SETS (BLINK PARITY)" << std::endl;
    std::cout << "==================================================" << std::endl;

    StyleSheet oldSheet = app.stylesheet();
    
    StyleSheet sheet;
    sheet.parse(R"(
        .container .descendant { color: red; }
        .container > .child { color: green; }
        .active + .adjacent { color: blue; }
        .active ~ .sibling { color: yellow; }
        .simple-btn { color: white; }
    )");

    app.stylesheet() = sheet;

    Widget* root = app.root();
    root->clearChildren(true);
    root->type = "div";
    root->className = "container";
    root->id = "app-root";

    // Create children and descendants
    Widget* child1 = root->add<Widget>();
    child1->id = "child-1";
    child1->className = "child";
    
    Widget* child2 = root->add<Widget>();
    child2->id = "child-2";
    child2->className = "not-a-child";

    Widget* desc1 = child2->add<Widget>();
    desc1->id = "desc-1";
    desc1->className = "descendant";

    Widget* desc2 = child2->add<Widget>();
    desc2->id = "desc-2";
    desc2->className = "not-a-descendant";

    // Sibling chain
    Widget* active = root->add<Widget>();
    active->id = "active-node";
    active->className = "active";

    Widget* adj = root->add<Widget>();
    adj->id = "adj-node";
    adj->className = "adjacent";

    Widget* sib = root->add<Widget>();
    sib->id = "sib-node";
    sib->className = "sibling";

    // Simple button
    Widget* btn = root->add<Widget>();
    btn->id = "simple-btn-node";
    btn->className = "simple-btn";

    // Initial style resolution to clean dirty flags
    root->resolveStyles(sheet);

    auto resetAllFlags = [&]() {
        std::function<void(Widget*)> reset = [&](Widget* w) {
            w->styleDirty = false;
            w->subtreeStyleDirty = false;
            w->layoutDirty = false;
            w->lifecycleState = WidgetLifecycle::StyleClean;
            for (auto& child : w->children) {
                reset(child.get());
            }
        };
        reset(root);
    };

    // Test Case 1: Modifying simple-btn (which only has simple rules)
    resetAllFlags();
    std::cout << "[Test 1] Modifying simple button class name..." << std::endl;
    btn->invalidateStyleOnClassListChange("simple-btn", "simple-btn active-state");
    
    bool t1_ok = btn->styleDirty;
    bool t1_clean = !child1->styleDirty && !child2->styleDirty && !desc1->styleDirty && !desc2->styleDirty && !active->styleDirty && !adj->styleDirty && !sib->styleDirty;
    std::cout << "  - Button marked dirty: " << (t1_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Subtree/Siblings remained clean: " << (t1_clean ? "PASS" : "FAIL") << std::endl;

    // Test Case 2: Modifying container (triggers descendant and child invalidation)
    resetAllFlags();
    std::cout << "[Test 2] Modifying container class list (triggers child and descendant invalidation)..." << std::endl;
    root->invalidateStyleOnClassListChange("container", "modified-container");

    bool t2_root_ok = root->styleDirty;
    bool t2_child1_ok = child1->styleDirty;
    bool t2_child2_ok = !child2->styleDirty;
    bool t2_desc1_ok = desc1->styleDirty;
    bool t2_desc2_ok = !desc2->styleDirty;

    std::cout << "  - Root marked dirty: " << (t2_root_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Direct child (.child) marked dirty: " << (t2_child1_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Direct child (non-matching) remained clean: " << (t2_child2_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Deep descendant (.descendant) marked dirty: " << (t2_desc1_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Deep descendant (non-matching) remained clean: " << (t2_desc2_ok ? "PASS" : "FAIL") << std::endl;

    // Test Case 3: Sibling invalidation (modifying active triggers adjacent and sibling invalidations)
    resetAllFlags();
    std::cout << "[Test 3] Modifying active node class list (triggers sibling and adjacent sibling invalidation)..." << std::endl;
    active->invalidateStyleOnClassListChange("active", "modified-active");

    bool t3_active_ok = active->styleDirty;
    bool t3_adj_ok = adj->styleDirty;
    bool t3_sib_ok = sib->styleDirty;
    bool t3_desc_clean = !child1->styleDirty && !child2->styleDirty && !desc1->styleDirty && !desc2->styleDirty;

    std::cout << "  - Active node marked dirty: " << (t3_active_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Adjacent sibling (.adjacent) marked dirty: " << (t3_adj_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - General sibling (.sibling) marked dirty: " << (t3_sib_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "  - Non-sibling nodes remained clean: " << (t3_desc_clean ? "PASS" : "FAIL") << std::endl;

    bool overall_pass = t1_ok && t1_clean && t2_root_ok && t2_child1_ok && t2_child2_ok && t2_desc1_ok && t2_desc2_ok && t3_active_ok && t3_adj_ok && t3_sib_ok && t3_desc_clean;
    std::cout << "STYLE INVALIDATION SET VERIFICATION: " << (overall_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "==================================================" << std::endl;
    
    app.stylesheet() = oldSheet;
}

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
    } else if (normalized == "skia") {
        backend = RenderBackendType::Skia;

    } else if (normalized == "compatibility" || normalized == "software" ||
               normalized == "cpu") {
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
#ifdef _WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
    auto t_start = std::chrono::high_resolution_clock::now();
    RenderBackendType requestedBackend = Renderer::defaultBackend();
    bool backendSpecified = false;
    bool probeVulkan = false;
    bool runBenchmark = false;
    int frameLimit = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--probe-vulkan") {
            probeVulkan = true;
        } else if (arg == "--benchmark") {
            runBenchmark = true;
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
    if (runBenchmark) {
        runKeymapBenchmark(app);
        runStyleResolutionBenchmark(app);
        validateStyleInvalidationSets(app);
        app.shutdown();
        return 0;
    }
    auto t_init = std::chrono::high_resolution_clock::now();
#if !FLUXUI_SILENT_STARTUP
    std::cout << "[TIMER] app.init took: " << std::chrono::duration<float, std::milli>(t_init - t_start).count() << " ms" << std::endl;
#endif
    constexpr float baseFontAtlasSize = 13.0f;
    bool fontsLoaded = false;
    if (DataLeakGuardEmbeddedFonts::loadEmbeddedUiFontAtlases(app.renderer())) {
#if !FLUXUI_SILENT_STARTUP
        std::cout << "Loaded precompiled UI font atlases" << std::endl;
#endif
        fontsLoaded = true;
    } else if (app.renderer().loadDefaultFont(baseFontAtlasSize)) {
#if !FLUXUI_SILENT_STARTUP
        std::cout << "Loaded default UI font" << std::endl;
#endif
        fontsLoaded = true;
    } else {
        std::cerr << "Warning: No font loaded, text will not render" << std::endl;
    }
    auto t_fonts = std::chrono::high_resolution_clock::now();
#if !FLUXUI_SILENT_STARTUP
    std::cout << "[TIMER] font loading took: " << std::chrono::duration<float, std::milli>(t_fonts - t_init).count() << " ms" << std::endl;
#endif
    if (fontsLoaded) {
        app.renderer().warmFontCache(std::vector<float>{
            11.0f, 12.0f, 13.0f, 14.0f, 16.0f, 20.0f, 28.0f, 29.0f, 32.0f
        });
        app.renderer().releaseFontSources();
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
    app.addRoute("/blink", [](Application& appRef, Widget* content) { buildBlinkParity(appRef, content); });
    app.setNotFoundRoute([](Application&, Widget* content) {
        buildTopBar(content, "Page Not Found", "The requested console view is not registered", false);
        auto* main = content->add<Panel>("main-scroll");
        auto* card = main->add<Panel>("panel-card");
        addSectionHeader(card, "Route missing", "Register the page before navigating to it.");
    });
    app.navigate("/dashboard");
    buildRetainedShell(app);

    // Register actions for the JSON-driven keymap
    app.registerAction("nav:dashboard", [](Application& a, const std::string&) {
        a.navigate("/dashboard");
    });
    app.registerAction("nav:scanner", [](Application& a, const std::string&) {
        a.navigate("/scanner");
    });
    app.registerAction("nav:alerts", [](Application& a, const std::string&) {
        a.navigate("/alerts");
    });
    app.registerAction("nav:rules", [](Application& a, const std::string&) {
        a.navigate("/rules");
    });
    app.registerAction("nav:reports", [](Application& a, const std::string&) {
        a.navigate("/reports");
    });
    app.registerAction("nav:settings", [](Application& a, const std::string&) {
        a.navigate("/settings");
    });
    app.registerAction("nav:blink", [](Application& a, const std::string&) {
        a.navigate("/blink");
    });
    app.registerAction("policy:toggle-block", [](Application& a, const std::string&) {
        blockMode = !blockMode;
        shell.syncPosture();
        a.requestRedraw();
        std::cout << "[DataLeak Guard] Policy mode toggled. Block mode: " << (blockMode ? "ACTIVE" : "DISABLED") << std::endl;
    });
    app.registerAction("input:blur-search", [](Application& a, const std::string&) {
        Widget* w = a.focusedWidget();
        if (w) {
            w->focused = false;
            a.requestRedraw();
            std::cout << "[DataLeak Guard] Blurred focused element" << std::endl;
        }
    });
    app.registerAction("input:clear-search", [](Application& a, const std::string&) {
        Widget* w = a.focusedWidget();
        if (w && w->type == "input") {
            auto* input = static_cast<TextInput*>(w);
            input->value.clear();
            a.requestRedraw();
            std::cout << "[DataLeak Guard] Cleared search input" << std::endl;
        }
    });

    // Load keymap.json
    if (app.loadKeymap("keymap.json")) {
#if !FLUXUI_SILENT_STARTUP
        std::cout << "Loaded keymap.json successfully" << std::endl;
#endif
    } else {
        std::cerr << "Warning: Failed to load keymap.json" << std::endl;
    }

    auto t_routes = std::chrono::high_resolution_clock::now();
#if !FLUXUI_SILENT_STARTUP
    std::cout << "[TIMER] routes and shell took: " << std::chrono::duration<float, std::milli>(t_routes - t_fonts).count() << " ms" << std::endl;
    app.on(UIEventType::RouteChanged, [](UIEvent& event) {
        std::cout << "[DataLeak Guard] Route: "
                  << (event.previousRoute.empty() ? "<start>" : event.previousRoute)
                  << " -> " << event.route << std::endl;
    });
#else
    (void)t_routes;
#endif
    int renderedFrames = 0;
    app.onUpdate = [t_start, &renderedFrames, &frameLimit, &app](float dt) {
#if !FLUXUI_SILENT_STARTUP
        static bool firstFrameTimer = true;
        if (firstFrameTimer) {
            firstFrameTimer = false;
            auto t_first_frame = std::chrono::high_resolution_clock::now();
            std::cout << "[TIMER] time to first frame took: " << std::chrono::duration<float, std::milli>(t_first_frame - t_start).count() << " ms" << std::endl;
        }
#else
        (void)t_start;
#endif
        if (scannerRunning) {
            bool wasRunning = scannerRunning;
            scanProgress = std::min(1.0f, scanProgress + dt * 0.055f);
            if (scanProgress >= 1.0f) {
                scannerRunning = false;
            }
            if (shell.scanProgressBar) {
                shell.scanProgressBar->progress = scanProgress;
            }
            if (shell.scanCaption) {
                shell.scanCaption->content = percentText(scanProgress) + " complete - 184,000 objects in queue";
            }
            if (wasRunning && !scannerRunning) {
                if (shell.scanOrbIcon) shell.scanOrbIcon->glyph = "scanner";
                if (shell.scanHeroTitle) shell.scanHeroTitle->content = "Scan complete";
            }
            app.requestRedraw();
        }
        if (!shell.initialized) {
            buildRetainedShell(app);
        } else if (app.routeDirty()) {
            rebuildActiveRoute(app);
        }
        if (frameLimit > 0 && ++renderedFrames >= frameLimit) {
            app.running = false;
        }
    };
#if !FLUXUI_SILENT_STARTUP
    std::cout << "========================================" << std::endl;
    std::cout << "  DataLeak Guard v1.0" << std::endl;
    std::cout << "  Built with FluxUI Framework" << std::endl;
    std::cout << "  GPU-Accelerated | CSS-Styled" << std::endl;
    std::cout << "  Active backend: " << app.renderer().activeBackendName() << std::endl;
    std::cout << "========================================" << std::endl;
#endif
    app.run();
    app.shutdown();
    return 0;
}
