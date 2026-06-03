// FluxUI Widget-Tree Restyle Benchmark
// Measures realistic per-frame style resolution over a widget tree,
// which exercises the pseudo-element fast-path and cascade allocation paths.

#include "fluxui/FluxUI.h"
#include <chrono>
#include <iostream>
#include <memory>

using namespace FluxUI;

// Build a tree of N widgets with varied classes (cache-miss heavy).
static std::shared_ptr<Panel> buildTree(int depth, int breadth, int& count) {
    auto root = std::make_shared<Panel>("container item");
    count++;
    if (depth <= 0) return root;
    for (int i = 0; i < breadth; i++) {
        auto child = buildTree(depth - 1, breadth, count);
        child->className = (i % 2 == 0) ? "row primary" : "col secondary";
        child->parent = root.get();
        root->children.push_back(child);
    }
    return root;
}

static void resolveTree(Widget* w, StyleSheet& sheet) {
    static std::vector<CSSSelectorNode> ancestors;
    ancestors.clear();
    for (Widget* p = w->parent; p; p = p->parent) {
        ancestors.push_back({p->className, p->id, p->selectorType(), p});
    }
    // Use the lazy (shared_ptr) path — same API real widgets use (avoids the
    // 6KB Style by-value copy on cache hits).
    auto getAncestors = [&]() -> const std::vector<CSSSelectorNode>& { return ancestors; };
    auto s = sheet.resolveLazy(w->className, w->id, w->selectorType(),
                               14695981039346656037ULL, 5381ULL, nullptr, getAncestors, w);
    if (s->opacity > 9999.0f) std::cout << "x"; // prevent optimization
    for (auto& c : w->children) resolveTree(c.get(), sheet);
}

int main() {
    StyleSheet sheet;
    sheet.parse(
        ".container { display: flex; padding: 8px; background-color: #1a1a1f; }"
        ".item { color: #ffffff; margin: 4px; }"
        ".row { flex-direction: row; gap: 8px; }"
        ".col { flex-direction: column; }"
        ".primary { font-weight: 700; background-color: #3b82f6; border-radius: 6px; }"
        ".secondary { opacity: 0.85; background-color: #222228; }"
        ".container .row { padding: 6px; }"
        ".primary:hover { background-color: #2563eb; }"
    );

    int count = 0;
    auto tree = buildTree(6, 4, count); // ~5461 widgets
    std::cout << "Built tree with " << count << " widgets" << std::endl;

    // Warm cache
    resolveTree(tree.get(), sheet);

    const int frames = 100;
    auto start = std::chrono::high_resolution_clock::now();
    for (int f = 0; f < frames; f++) {
        // Steady-state: cache stays warm (viewport stable), exercising the
        // realistic per-frame path where most widgets hit the style cache.
        resolveTree(tree.get(), sheet);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dur = end - start;

    std::cout << "[Warm cache] Resolved " << (count * frames) << " widget styles in "
              << dur.count() << " ms" << std::endl;
    std::cout << "[Warm cache] Per-frame tree restyle: " << (dur.count() / frames) << " ms" << std::endl;
    std::cout << "[Warm cache] Per-widget: " << (dur.count() / (count * frames) * 1000.0) << " us" << std::endl;

    // Cold path: force full re-resolution each frame (first-paint / theme-switch cost).
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int f = 0; f < frames; f++) {
        sheet.setViewportSize(1920 + (f % 2), 1080); // bump epoch → cache invalidation
        resolveTree(tree.get(), sheet);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dur2 = end2 - start2;
    std::cout << "[Cold/no-cache] Per-widget: " << (dur2.count() / (count * frames) * 1000.0) << " us" << std::endl;
    return 0;
}
