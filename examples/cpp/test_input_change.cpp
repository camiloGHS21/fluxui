// FluxUI `input` / `change` event test.
// Mirrors HTML form events (HTML §4.10.5.5 / Blink DispatchInputEvent +
// DispatchFormControlChangeEvent):
//   input  — fires on every value change of a form control
//   change — text controls: on blur if value changed since focus
//            checkbox/radio/select/range: immediately on each change
//
// The engine detects value changes once per frame in checkFormControlChanges(),
// so the test drives it manually to simulate frames.

#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include <iostream>

using namespace FluxUI;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// Simulate one frame of change detection (order matches the engine:
// form-control changes are read against the previous frame's focus state).
static void frame(Widget* root) {
    root->checkFormControlChanges();
    root->checkFocusChanges();
}

// ── [1] input fires on every text value change ──
int test_text_input() {
    std::cout << "[1] input fires on each text change" << std::endl;
    auto root = std::make_shared<Panel>("root");
    auto inp = std::make_shared<TextInput>();
    inp->parent = root.get();
    root->children.push_back(inp);

    int inputCount = 0;
    inp->addEventListener("input", [&](Event&){ inputCount++; });

    frame(root.get());                 // seed snapshot (no fire)
    CHECK(inputCount == 0);

    inp->value = "h";  frame(root.get());
    inp->value = "he"; frame(root.get());
    inp->value = "hel";frame(root.get());
    CHECK(inputCount == 3);

    // No change → no event.
    frame(root.get());
    CHECK(inputCount == 3);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [2] change fires on blur when text changed since focus ──
int test_text_change_on_blur() {
    std::cout << "[2] change fires on blur (text)" << std::endl;
    auto root = std::make_shared<Panel>("root");
    auto inp = std::make_shared<TextInput>();
    inp->parent = root.get();
    root->children.push_back(inp);

    int changeCount = 0;
    inp->addEventListener("change", [&](Event&){ changeCount++; });

    frame(root.get());                 // seed
    inp->focused = true; frame(root.get());   // gain focus → baseline captured
    inp->value = "typed"; frame(root.get());  // input only, no change yet
    CHECK(changeCount == 0);

    inp->focused = false; frame(root.get());  // blur with changed value → change
    CHECK(changeCount == 1);

    // Blur again without change → no extra change.
    inp->focused = true;  frame(root.get());
    inp->focused = false; frame(root.get());
    CHECK(changeCount == 1);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [3] checkbox fires input AND change immediately ──
int test_checkbox_instant() {
    std::cout << "[3] checkbox fires input + change instantly" << std::endl;
    auto root = std::make_shared<Panel>("root");
    auto cb = std::make_shared<Checkbox>();
    cb->parent = root.get();
    root->children.push_back(cb);

    int inputCount = 0, changeCount = 0;
    cb->addEventListener("input",  [&](Event&){ inputCount++; });
    cb->addEventListener("change", [&](Event&){ changeCount++; });

    frame(root.get());                 // seed (unchecked)
    cb->checked = true;  frame(root.get());
    CHECK(inputCount == 1);
    CHECK(changeCount == 1);

    cb->checked = false; frame(root.get());
    CHECK(inputCount == 2);
    CHECK(changeCount == 2);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [4] range + select instant commit ──
int test_range_select() {
    std::cout << "[4] range + select fire change instantly" << std::endl;
    auto root = std::make_shared<Panel>("root");
    auto rng = std::make_shared<RangeInput>();
    rng->parent = root.get();
    root->children.push_back(rng);
    int rngChange = 0;
    rng->addEventListener("change", [&](Event&){ rngChange++; });

    frame(root.get());
    rng->value = 0.75f; frame(root.get());
    CHECK(rngChange == 1);

    auto sel = std::make_shared<Select>();
    auto o1 = std::make_shared<Option>("One", "1");  o1->parent = sel.get();
    auto o2 = std::make_shared<Option>("Two", "2");  o2->parent = sel.get();
    sel->children = {o1, o2};
    sel->parent = root.get();
    root->children.push_back(sel);
    int selChange = 0;
    sel->addEventListener("change", [&](Event&){ selChange++; });

    frame(root.get());                 // seed (index 0 → "1")
    sel->selectedIndex = 1;            // → "2"
    frame(root.get());
    CHECK(selChange == 1);
    std::cout << "  PASS" << std::endl;
    return 0;
}

// ── [5] input bubbles to ancestors ──
int test_bubbling() {
    std::cout << "[5] input/change bubble to ancestors" << std::endl;
    auto root = std::make_shared<Panel>("root");
    auto form = std::make_shared<Panel>("form");
    form->parent = root.get();
    root->children.push_back(form);
    auto inp = std::make_shared<TextInput>();
    inp->parent = form.get();
    form->children.push_back(inp);

    int rootInput = 0;
    Widget* gotTarget = nullptr;
    root->addEventListener("input", [&](Event& e){ rootInput++; gotTarget = e.target; });

    frame(root.get());
    inp->value = "x"; frame(root.get());
    CHECK(rootInput == 1);
    CHECK(gotTarget == inp.get());
    std::cout << "  PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== FluxUI input/change Event Tests ===" << std::endl;
    int rc = 0;
    rc |= test_text_input();
    rc |= test_text_change_on_blur();
    rc |= test_checkbox_instant();
    rc |= test_range_select();
    rc |= test_bubbling();
    if (rc == 0) std::cout << "\nAll input/change tests passed!" << std::endl;
    return rc;
}
