// FluxUI Declarative DSL Test — modern HTML/Blink-named API + reactive State.
// Verifies the functional API actually works (headless, no window).

#include "fluxui/dsl.h"
#include <iostream>
#include <string>

using namespace fluxui;

#define CHECK(cond) do{if(!(cond)){std::cerr<<"FAIL: "<<#cond<<" @ line "<<__LINE__<<std::endl;return 1;}}while(0)

// Mount a root element onto a detached Panel so we can inspect the widget tree.
static std::shared_ptr<FluxUI::Panel> mountRoot(const Element& e) {
    auto root = std::make_shared<FluxUI::Panel>();
    e.mount(root.get());
    return root;
}

int test_html_tag_names() {
    std::cout << "[1] builders produce correct HTML tag types" << std::endl;
    CHECK(mountRoot(Div())->children[0]->type == "div");
    CHECK(mountRoot(Section())->children[0]->type == "section");
    CHECK(mountRoot(Nav())->children[0]->type == "nav");
    CHECK(mountRoot(H1("x"))->children[0]->type == "h1");
    CHECK(mountRoot(P("x"))->children[0]->type == "p");
    CHECK(mountRoot(Span("x"))->children[0]->type == "span");
    CHECK(mountRoot(Button("x"))->children[0]->type == "button");
    CHECK(mountRoot(Ul())->children[0]->type == "ul");
    CHECK(mountRoot(Li())->children[0]->type == "li");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_text_content() {
    std::cout << "[2] text content flows into the widget" << std::endl;
    auto root = mountRoot(H1("Hello"));
    auto* t = static_cast<FluxUI::Text*>(root->children[0].get());
    CHECK(t->content == "Hello");
    auto rootBtn = mountRoot(Button("Click"));
    auto* b = static_cast<FluxUI::Button*>(rootBtn->children[0].get());
    CHECK(b->label == "Click");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_chaining() {
    std::cout << "[3] chaining className/id/style/attr" << std::endl;
    auto root = mountRoot(Div().className("box").id("main").style("color", "red"));
    auto* w = root->children[0].get();
    CHECK(w->className == "box");
    CHECK(w->id == "main");
    CHECK(!w->inlineProperties.empty());
    CHECK(w->inlineProperties[0].name == "color");
    CHECK(w->inlineProperties[0].value == "red");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_tree_structure() {
    std::cout << "[4] nested declarative tree + parent wiring" << std::endl;
    auto root = mountRoot(
        Div({
            Nav({
                Button("Dashboard"),
                Button("Settings")
            }).className("sidebar"),
            Div({
                H1("Title"),
                Button("Click")
            }).className("content")
        }).className("app")
    );

    auto* app = root->children[0].get();
    CHECK(app->className == "app");
    CHECK(app->type == "div");
    CHECK(app->children.size() == 2);

    auto* sidebar = app->children[0].get();
    CHECK(sidebar->type == "nav");
    CHECK(sidebar->className == "sidebar");
    CHECK(sidebar->children.size() == 2);
    CHECK(sidebar->children[0]->parent == sidebar);

    auto* content = app->children[1].get();
    CHECK(content->className == "content");
    CHECK(content->children.size() == 2);
    CHECK(content->children[0]->type == "h1");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_reactive_text() {
    std::cout << "[5] reactive Text re-evaluates on State change" << std::endl;
    auto devices = State<int>(128);
    auto root = mountRoot(Text([&]{ return std::to_string(devices.get()); }));
    auto* t = static_cast<FluxUI::Text*>(root->children[0].get());

    CHECK(t->content == "128");
    devices.set(129);
    CHECK(t->content == "128");                 // not pushed until pump runs
    CHECK(detail::pumpReactiveBindings());
    CHECK(t->content == "129");
    CHECK(!detail::pumpReactiveBindings());      // no change => nothing to do
    devices.set(200);
    devices.set(201);
    detail::pumpReactiveBindings();
    CHECK(t->content == "201");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_onclick_mutates_state() {
    std::cout << "[6] onClick mutating State drives reactive Text" << std::endl;
    auto count = State<int>(0);
    auto labelRoot = mountRoot(Text([&]{ return std::to_string(count.get()); }));
    auto* label = static_cast<FluxUI::Text*>(labelRoot->children[0].get());
    auto btnRoot = mountRoot(Button("inc").onClick([&]{ count.set(count.get() + 1); }));
    auto* btn = btnRoot->children[0].get();

    CHECK(label->content == "0");
    btn->onClick();
    btn->onClick();
    btn->onClick();
    detail::pumpReactiveBindings();
    CHECK(label->content == "3");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_anchor_href() {
    std::cout << "[7] A() sets href attribute" << std::endl;
    auto root = mountRoot(A("Home", "/index"));
    auto* a = static_cast<FluxUI::Anchor*>(root->children[0].get());
    CHECK(a->type == "a");
    CHECK(a->href == "/index");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_state_operators() {
    std::cout << "[8] State shorthand operators (++, +=, -=, =, toggle)" << std::endl;
    auto count = State<int>(0);
    ++count;            CHECK(count.get() == 1);
    count += 5;         CHECK(count.get() == 6);
    count -= 2;         CHECK(count.get() == 4);
    --count;            CHECK(count.get() == 3);
    count = 42;         CHECK(count.get() == 42);
    auto flag = State<bool>(false);
    flag.toggle();      CHECK(flag.get() == true);
    flag.toggle();      CHECK(flag.get() == false);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_typed_style_helpers() {
    std::cout << "[9] typed style helpers (color/width/padding/...)" << std::endl;
    auto root = mountRoot(
        Div().color("red").background("#111").width("250px").padding("16px")
             .gap("8px").fontSize("24px").flexDirection("column")
    );
    auto* w = root->children[0].get();
    // Each helper pushes an inline property; verify a couple resolved through.
    bool hasColor = false, hasWidth = false, hasDir = false;
    for (const auto& p : w->inlineProperties) {
        if (p.name == "color" && p.value == "red") hasColor = true;
        if (p.name == "width" && p.value == "250px") hasWidth = true;
        if (p.name == "flex-direction" && p.value == "column") hasDir = true;
    }
    CHECK(hasColor);
    CHECK(hasWidth);
    CHECK(hasDir);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_ref_capture() {
    std::cout << "[10] Ref<T> captures widget on mount (shared slot)" << std::endl;
    auto ref = Ref<FluxUI::Button>();
    // Pass the ref by value into onMount (stored in std::function) — shared slot
    // must still let the original see the captured widget.
    auto root = mountRoot(Button("Go").onMount(ref));
    CHECK((bool)ref);
    CHECK(ref->type == "button");
    CHECK(ref->label == "Go");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_store_zustand() {
    std::cout << "[11] Store<T> (Zustand-style global state)" << std::endl;
    struct CartState { int count = 0; std::string last; };
    Store<CartState> cart;
    int notified = 0;
    cart.subscribe([&]{ notified++; });
    cart.set([](CartState& s){ s.count += 1; s.last = "apple"; });
    CHECK(cart.get().count == 1);
    CHECK(cart.get().last == "apple");
    CHECK(notified == 1);
    int doubled = cart.select<int>([](const CartState& s){ return s.count * 2; });
    CHECK(doubled == 2);
    std::cout << "  PASS" << std::endl; return 0;
}

int test_schema_zod() {
    std::cout << "[12] Schema validation (Zod-style)" << std::endl;
    auto schema = Schema()
        .field("email", Rule::string().email())
        .field("age", Rule::number().min(18).max(120))
        .field("nick", Rule::string().optional().maxLength(5));

    auto ok = schema.validate({{"email", "a@b.com"}, {"age", "25"}, {"nick", "joe"}});
    CHECK(ok.ok);

    auto bad = schema.validate({{"email", "not-an-email"}, {"age", "12"}});
    CHECK(!bad.ok);
    CHECK(bad.errors.count("email") == 1);
    CHECK(bad.errors.count("age") == 1);

    auto missing = schema.validate({{"age", "20"}});
    CHECK(!missing.ok);
    CHECK(missing.errors.count("email") == 1);  // required
    std::cout << "  PASS" << std::endl; return 0;
}

int test_skeleton() {
    std::cout << "[13] Skeleton loading placeholder" << std::endl;
    auto root = mountRoot(Skeleton(4));
    auto* w = root->children[0].get();
    CHECK(w->className == "skeleton");
    CHECK(w->children.size() == 4);
    CHECK(w->children[0]->className == "skeleton-line");
    std::cout << "  PASS" << std::endl; return 0;
}

int test_route_param_match() {
    std::cout << "[14] route param matching (/user/:id)" << std::endl;
    std::map<std::string, std::string> params;
    CHECK(App::matchPattern("/user/:id", "/user/42", params));
    CHECK(params["id"] == "42");
    CHECK(App::matchPattern("/post/:cat/:slug", "/post/news/hello", params));
    CHECK(params["cat"] == "news");
    CHECK(params["slug"] == "hello");
    CHECK(!App::matchPattern("/user/:id", "/post/42", params));
    CHECK(!App::matchPattern("/user/:id", "/user/1/extra", params));
    // Query string is stripped before matching.
    CHECK(App::matchPattern("/user/:id", "/user/7?tab=info", params));
    CHECK(params["id"] == "7");
    // param/query accessors round-trip.
    App::currentParams().clear();
    App::currentParams()["id"] = "42";
    App::currentQuery().clear();
    App::currentQuery()["tab"] = "info";
    CHECK(App::param("id") == "42");
    CHECK(App::param("missing", "def") == "def");
    CHECK(App::query("tab") == "info");
    std::cout << "  PASS" << std::endl; return 0;
}

int main() {
    std::cout << "=== FluxUI DSL Test (HTML/Blink-named) ===" << std::endl;
    int rc = 0;
    rc |= test_html_tag_names();
    rc |= test_text_content();
    rc |= test_chaining();
    rc |= test_tree_structure();
    rc |= test_reactive_text();
    rc |= test_onclick_mutates_state();
    rc |= test_anchor_href();
    rc |= test_state_operators();
    rc |= test_typed_style_helpers();
    rc |= test_ref_capture();
    rc |= test_store_zustand();
    rc |= test_schema_zod();
    rc |= test_skeleton();
    rc |= test_route_param_match();
    if (rc == 0) std::cout << "All DSL tests passed!" << std::endl;
    return rc;
}
