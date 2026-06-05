// FluxUI Full-Frame Benchmark
// ----------------------------
// Measures the realistic per-frame cost of the whole document pipeline the way
// a real app pays it each frame: style recalc -> layout -> paint-command
// generation, over a representative widget tree. This mirrors how GPU UI
// frameworks (e.g. Zed's GPUI) benchmark the frame loop rather than micro
// pieces — what the user feels is the end-to-end frame time.
//
// It runs entirely headless (no window / GPU) by recording paint into a
// throwaway command buffer, so results are stable across machines and CI.
//
// Reported metrics per scenario:
//   - widgets:           number of widgets in the tree
//   - style ms/frame:    cost of resolveStyles() across the tree
//   - layout ms/frame:   cost of the layout pass
//   - paint ms/frame:    cost of generating paint commands
//   - total ms/frame:    full pipeline
//   - FPS headroom:      1000 / total (how many such frames fit in 1s)

#include "fluxui/FluxUI.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

using namespace FluxUI;
using Clock = std::chrono::high_resolution_clock;

static double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// Build a dashboard-like tree: a sidebar + a scrollable content area with N
// "cards", each containing a title, a few text rows and a progress bar — a
// realistic mix of containers, text and leaf widgets.
static std::shared_ptr<Panel> buildDashboard(int cards, int& count) {
    auto root = std::make_shared<Panel>("root");
    count = 1;

    auto sidebar = std::make_shared<Panel>("sidebar");
    sidebar->parent = root.get();
    count++;
    for (int i = 0; i < 8; ++i) {
        auto btn = std::make_shared<Button>("Nav " + std::to_string(i), "nav-item");
        btn->parent = sidebar.get();
        sidebar->children.push_back(btn);
        count++;
    }
    root->children.push_back(sidebar);

    auto content = std::make_shared<Panel>("content");
    content->parent = root.get();
    content->style.overflow = Overflow::Auto;
    count++;
    for (int c = 0; c < cards; ++c) {
        auto card = std::make_shared<Panel>((c % 2 == 0) ? "card primary" : "card secondary");
        card->parent = content.get();
        count++;

        auto title = std::make_shared<Text>("Metric " + std::to_string(c), "card-title");
        title->parent = card.get();
        card->children.push_back(title);
        count++;

        for (int r = 0; r < 3; ++r) {
            auto row = std::make_shared<Text>("row value " + std::to_string(r), "row");
            row->parent = card.get();
            card->children.push_back(row);
            count++;
        }

        auto bar = std::make_shared<ProgressBar>();
        bar->progress = (float)((c % 10) / 10.0f);
        bar->className = "progress";
        bar->parent = card.get();
        card->children.push_back(bar);
        count++;

        content->children.push_back(card);
    }
    root->children.push_back(content);
    return root;
}

static void resolveTree(Widget* w, StyleSheet& sheet) {
    w->resolveStyles(sheet);
}

static int countAndLayout(Widget* w, StyleSheet& sheet, int vw, int vh) {
    w->layout({0, 0, (float)vw, (float)vh});
    return 0;
}

static void paintTree(Widget* w, Renderer& r) {
    w->render(r);
}

struct Result {
    int widgets = 0;
    double styleMs = 0, layoutMs = 0, paintMs = 0;
};

static Result runScenario(int cards, int frames, int vw, int vh) {
    StyleSheet sheet;
    sheet.parse(
        ".root { display:flex; flex-direction:row; width:100%; height:100%; background-color:#0d1117; color:#e6edf3; }"
        ".sidebar { display:flex; flex-direction:column; width:220px; background-color:#161b22; padding:16px; gap:6px; }"
        ".nav-item { padding:10px 12px; border-radius:8px; color:#c9d1d9; }"
        ".nav-item:hover { background-color:#21262d; }"
        ".content { display:flex; flex-direction:column; flex-grow:1; padding:24px; gap:14px; }"
        ".card { display:flex; flex-direction:column; background-color:#161b22; border:1px solid #30363d; border-radius:12px; padding:18px; gap:8px; }"
        ".primary { border-color:#1f6feb; }"
        ".secondary { opacity:0.95; }"
        ".card-title { font-size:18px; font-weight:700; }"
        ".row { font-size:14px; color:#9da7b3; }"
        ".progress { height:8px; border-radius:4px; background-color:#21262d; }"
    );
    sheet.setViewportSize((float)vw, (float)vh);

    int count = 0;
    auto tree = buildDashboard(cards, count);

    // Warm the caches the way a real first frame would.
    resolveTree(tree.get(), sheet);
    countAndLayout(tree.get(), sheet, vw, vh);

    Result res;
    res.widgets = count;

    // Per-frame style recalc. Force a full subtree restyle each iteration
    // (markStyleDirtyRecursive) so we measure the real cascade cost, not the
    // dirty-skip fast path. This is the cost paid on a theme change / first paint.
    auto t0 = Clock::now();
    for (int f = 0; f < frames; ++f) {
        tree->markStyleDirtyRecursive();
        resolveTree(tree.get(), sheet);
    }
    res.styleMs = msSince(t0) / frames;

    // Layout pass (forced dirty each frame to measure the real layout cost).
    t0 = Clock::now();
    for (int f = 0; f < frames; ++f) {
        tree->markLayoutDirty();
        countAndLayout(tree.get(), sheet, vw, vh);
    }
    res.layoutMs = msSince(t0) / frames;

    // Paint-command generation (record into a throwaway buffer; no GPU).
    Renderer r;
    std::vector<RenderCommand> cmds;
    t0 = Clock::now();
    for (int f = 0; f < frames; ++f) {
        cmds.clear();
        r.startRecording(cmds);
        paintTree(tree.get(), r);
        r.stopRecording();
    }
    res.paintMs = msSince(t0) / frames;

    return res;
}

int main() {
    std::cout << "=== FluxUI Full-Frame Benchmark (headless) ===\n";
    std::cout << std::fixed << std::setprecision(3);

    struct Scenario { const char* name; int cards; int frames; };
    Scenario scenarios[] = {
        {"small  (dashboard, ~20 cards)",   20,  400},
        {"medium (dashboard, ~100 cards)", 100,  200},
        {"large  (dashboard, ~500 cards)", 500,  100},
    };

    const int vw = 1920, vh = 1080;
    std::cout << "viewport " << vw << "x" << vh << "\n\n";
    std::cout << std::left << std::setw(34) << "scenario"
              << std::right << std::setw(9) << "widgets"
              << std::setw(11) << "style/ms"
              << std::setw(11) << "layout/ms"
              << std::setw(11) << "paint/ms"
              << std::setw(11) << "total/ms"
              << std::setw(10) << "fps\n";
    std::cout << std::string(97, '-') << "\n";

    for (const auto& s : scenarios) {
        Result r = runScenario(s.cards, s.frames, vw, vh);
        double total = r.styleMs + r.layoutMs + r.paintMs;
        double fps = total > 0.0 ? 1000.0 / total : 0.0;
        std::cout << std::left << std::setw(34) << s.name
                  << std::right << std::setw(9) << r.widgets
                  << std::setw(11) << r.styleMs
                  << std::setw(11) << r.layoutMs
                  << std::setw(11) << r.paintMs
                  << std::setw(11) << total
                  << std::setw(10) << std::setprecision(0) << fps
                  << std::setprecision(3) << "\n";
    }
    std::cout << "\nNote: headless pipeline cost (no GPU present). Real on-screen\n"
                 "frame rate is additionally bounded by GPU/software present + vsync.\n";
    return 0;
}
