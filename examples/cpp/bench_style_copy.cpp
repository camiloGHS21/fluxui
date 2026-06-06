// Microbenchmark: cost of copying Style (the operation the DataRef/StyleRareData
// copy-on-write refactor optimizes — sibling style sharing, ComputedStyle::
// ensureMutable, resolved style cache all copy Style by value).
#include "fluxui/core.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace FluxUI;
using Clock = std::chrono::high_resolution_clock;

int main() {
    printf("sizeof(Style)         = %zu bytes\n", sizeof(Style));
    printf("sizeof(StyleRareData) = %zu bytes\n", sizeof(StyleRareData));

    // A representative "hot" style: only common fields set, rare group untouched
    // (the overwhelmingly common case in real UIs).
    Style src;
    src.color = Color(1, 1, 1, 1);
    src.backgroundColor = Color(0.1f, 0.1f, 0.1f, 1);
    src.fontSize = 14.0f;
    src.fontFamily = "Inter";
    src.padding = EdgeInsets{8, 12, 8, 12};
    src.width = CSSValue::px(320);

    const int N = 5'000'000;

    // Warm up
    volatile size_t sink = 0;
    {
        Style a = src;
        sink += (size_t)a.fontSize;
    }

    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        Style copy = src;          // full Style copy
        copy.fontSize += 1.0f;     // touch a hot field so it isn't optimized away
        sink += (size_t)copy.fontSize;
    }
    auto t1 = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("Copied Style %d times in %.1f ms  (%.1f ns/copy)\n",
           N, ms, ms * 1e6 / N);
    printf("sink=%zu\n", (size_t)sink);
    return 0;
}
