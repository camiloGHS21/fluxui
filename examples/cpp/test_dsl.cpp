// FluxUI Declarative DSL Test — reactive State + functional tree building
// Verifies the React/SwiftUI-style API actually works (headless, no window).

#include "fluxui/dsl.h"
#include <iostream>
#include <string>

using namespace fluxui;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// Helpers to reach into the built tree.
static FluxUI::Text* asText(const WidgetBuilder& b) {
    return static_cast<FluxUI::Text*>(b.get());
}

int test_static_text() {
    std::cout << "[1] static Text content" << std::endl;
    auto t = Text("Hello");
    CHECK(asText(t)->content == "Hello");
    CHECK(asText(t)->type == "text");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_builder_chaining() {
    std::cout << "[2] builder chaining (className/id/style)" << std::endl;
    auto t = Text("X").className("title").id("main");
    CHECK(t.get()->className == "title");
    CHECK(t.get()->id == "main");
    auto b = Button("Go").className("primary").style("color", "red");
    CHECK(b.get()->className == "primary");
    CHECK(!b.get()->inlineProperties.empty());
    CHECK(b.get()->inlineProperties[0].name == "color");
    CHECK(b.get()->inlineProperties[0].value == "red");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_tree_structure() {
    std::cout << "[3] declarative tree (Row/Column/children/parent)" << std::endl;
    auto row = Row({
        Sidebar({
            NavItem("Dashboard"),
            NavItem("Settings")
        }).className("sidebar"),
        Column({
            Text("Title").className("h1"),
            Button("Click")
        }).className("content")
    }).className("app");

    auto* w = row.get();
    CHECK(w->className == "app");
    CHECK(w->style.display == FluxUI::Display::Flex);
    CHECK(w->style.flexDirection == FluxUI::FlexDirection::Row);
    CHECK(w->children.size() == 2);

    auto* sidebar = w->children[0].get();
    CHECK(sidebar->className == "sidebar");
    CHECK(sidebar->children.size() == 2);
    CHECK(sidebar->children[0]->parent == sidebar);   // parent wiring

    auto* content = w->children[1].get();
    CHECK(content->className == "content");
    CHECK(content->children.size() == 2);
    CHECK(content->style.flexDirection == FluxUI::FlexDirection::Column);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_reactive_text() {
    std::cout << "[4] reactive Text re-evaluates on State change" << std::endl;
    auto devices = State<int>(128);
    auto t = Text([&]{ return std::to_string(devices.get()); });

    // Initial evaluation happens at construction.
    CHECK(asText(t)->content == "128");

    // Mutating state alone does not push the value into the widget...
    devices.set(129);
    CHECK(asText(t)->content == "128");

    // ...until the reactive pump runs (the App does this every frame).
    bool changed = detail::pumpReactiveBindings();
    CHECK(changed);
    CHECK(asText(t)->content == "129");

    // No change => pump reports nothing changed.
    bool changed2 = detail::pumpReactiveBindings();
    CHECK(!changed2);
    CHECK(asText(t)->content == "129");

    // Multiple updates accumulate to the latest value.
    devices.set(200);
    devices.set(201);
    detail::pumpReactiveBindings();
    CHECK(asText(t)->content == "201");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_onclick_mutates_state() {
    std::cout << "[5] onClick handler mutating State drives reactive Text" << std::endl;
    auto count = State<int>(0);
    auto label = Text([&]{ return std::to_string(count.get()); });
    auto btn = Button("inc").onClick([&]{ count.set(count.get() + 1); });

    CHECK(asText(label)->content == "0");

    // Simulate three clicks.
    btn.get()->onClick();
    btn.get()->onClick();
    btn.get()->onClick();

    detail::pumpReactiveBindings();
    CHECK(asText(label)->content == "3");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_state_listeners() {
    std::cout << "[6] State onChange listeners fire" << std::endl;
    auto s = State<int>(1);
    int fired = 0;
    s.onChange([&]{ fired++; });
    s.set(2);
    s.set(3);
    CHECK(fired == 2);
    CHECK(s.get() == 3);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_binding_cleanup() {
    std::cout << "[7] expired reactive widgets are pruned" << std::endl;
    auto live = State<int>(0);
    auto liveText = Text([&]{ return std::to_string(live.get()); });
    {
        // Scoped reactive text; its widget dies at end of scope.
        auto tmp = State<int>(0);
        auto scoped = Text([&]{ return std::to_string(tmp.get()); });
        CHECK(asText(scoped)->content == "0");
    }
    // Pump should survive the dead binding and still update the live one.
    live.set(42);
    detail::pumpReactiveBindings();
    CHECK(asText(liveText)->content == "42");
    std::cout << "  PASS" << std::endl; return 0;
}

int main() {
    std::cout << "=== FluxUI DSL Test ===" << std::endl;
    int rc = 0;
    rc |= test_static_text();
    rc |= test_builder_chaining();
    rc |= test_tree_structure();
    rc |= test_reactive_text();
    rc |= test_onclick_mutates_state();
    rc |= test_state_listeners();
    rc |= test_binding_cleanup();
    if (rc == 0) std::cout << "All DSL tests passed!" << std::endl;
    return rc;
}
