"""Headless test for the FluxUI Python DSL (modern HTML/Blink-named API).

Exercises State, Element tree construction and the reactive pump without opening
a window. The pump is validated against a fake widget so it does not require a
GPU context, mirroring examples/cpp/test_dsl.cpp.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "bindings", "python"))

import fluxui


class FakeWidget:
    """Stand-in for a native Text widget; records content writes."""

    def __init__(self):
        self.content = ""

    def set_content(self, text):
        self.content = text


def reset_bindings():
    fluxui._reactive_bindings.clear()


def test_state_basic():
    s = fluxui.State(1)
    fired = []
    s.on_change(lambda: fired.append(s.get()))
    s.set(2)
    s.set(3)
    assert s.get() == 3, "state value should update"
    assert fired == [2, 3], "listeners should fire on each set"
    print("[1] State basic + listeners  PASS")


def test_element_tags():
    assert fluxui.Div()._tag == "div"
    assert fluxui.Nav()._tag == "nav"
    assert fluxui.Section()._tag == "section"
    assert fluxui.H1("x")._tag == "h1"
    assert fluxui.P("x")._tag == "p"
    assert fluxui.Span("x")._tag == "span"
    assert fluxui.Button("x")._tag == "button"
    assert fluxui.Ul()._tag == "ul"
    assert fluxui.Li()._tag == "li"
    assert fluxui.A("x", "/h")._tag == "a"
    print("[2] HTML element tag names  PASS")


def test_element_tree():
    root = fluxui.Div([
        fluxui.Nav([fluxui.Button("A"), fluxui.Button("B")]).cls("sidebar"),
        fluxui.Div([fluxui.H1("T").cls("h1"), fluxui.Button("Go")]).cls("content"),
    ]).cls("app")
    assert root._tag == "div"
    assert root._class_name == "app"
    assert len(root._children) == 2
    assert root._children[0]._tag == "nav"
    assert root._children[0]._class_name == "sidebar"
    assert len(root._children[0]._children) == 2
    assert root._children[1]._children[0]._content == "T"
    print("[3] declarative HTML tree  PASS")


def test_reactive_pump():
    reset_bindings()
    devices = fluxui.State(128)
    w = FakeWidget()
    initial = str(devices.get())
    w.set_content(initial)
    fluxui._register_reactive(w, lambda: str(devices.get()), initial)

    assert w.content == "128"
    devices.set(129)
    assert w.content == "128"  # not pushed until pump runs
    assert fluxui.pump_reactive_bindings() is True
    assert w.content == "129"
    assert fluxui.pump_reactive_bindings() is False
    devices.set(200)
    devices.set(201)
    fluxui.pump_reactive_bindings()
    assert w.content == "201"
    print("[4] reactive pump updates on State change  PASS")


def test_onclick_drives_reactive():
    reset_bindings()
    count = fluxui.State(0)
    w = FakeWidget()
    w.set_content(str(count.get()))
    fluxui._register_reactive(w, lambda: str(count.get()), "0")

    btn = fluxui.Button("inc").on_click(lambda: count.set(count.get() + 1))
    for _ in range(3):
        btn._on_click()
    fluxui.pump_reactive_bindings()
    assert w.content == "3"
    print("[5] onClick -> State -> reactive Text  PASS")


def test_store_zustand():
    cart = fluxui.Store({"count": 0, "last": ""})
    notified = []
    cart.subscribe(lambda: notified.append(1))
    cart.set(lambda s: s.update(count=s["count"] + 1, last="apple"))
    assert cart.get()["count"] == 1
    assert cart.get()["last"] == "apple"
    assert len(notified) == 1
    doubled = cart.select(lambda s: s["count"] * 2)
    assert doubled == 2
    print("[6] Store (Zustand-style global state)  PASS")


def test_schema_zod():
    schema = (fluxui.Schema()
              .field("email", fluxui.Rule.string().email())
              .field("age", fluxui.Rule.number().min(18).max(120))
              .field("nick", fluxui.Rule.string().optional().max_length(5)))

    ok = schema.validate({"email": "a@b.com", "age": "25", "nick": "joe"})
    assert ok.ok, "valid input should pass"

    bad = schema.validate({"email": "not-an-email", "age": "12"})
    assert not bad.ok
    assert "email" in bad.errors
    assert "age" in bad.errors

    missing = schema.validate({"age": "20"})
    assert not missing.ok
    assert "email" in missing.errors  # required
    print("[7] Schema validation (Zod-style)  PASS")


def test_query_states():
    q = fluxui.Query(lambda: "hello")
    assert q.status == fluxui.Query.IDLE
    q.start()
    # Worker runs on a thread; join by polling briefly.
    import time
    for _ in range(50):
        if q.status != fluxui.Query.LOADING:
            break
        time.sleep(0.01)
    assert q.status == fluxui.Query.SUCCESS
    node = q.view(lambda: fluxui.Skeleton(2),
                  lambda data: fluxui.Text(data),
                  lambda err: fluxui.Text("Error: " + err))
    assert node._content == "hello"
    print("[8] Query async fetch states + view()  PASS")


def test_skeleton():
    sk = fluxui.Skeleton(4)
    assert sk._class_name == "skeleton"
    assert len(sk._children) == 4
    assert sk._children[0]._class_name == "skeleton-line"
    print("[9] Skeleton loading placeholder  PASS")


def test_route_param_match():
    params = {}
    assert fluxui.DslApp._match_pattern("/user/:id", "/user/42", params)
    assert params["id"] == "42"
    params = {}
    assert fluxui.DslApp._match_pattern("/post/:cat/:slug", "/post/news/hello", params)
    assert params["cat"] == "news"
    assert params["slug"] == "hello"
    params = {}
    assert not fluxui.DslApp._match_pattern("/user/:id", "/post/42", params)
    params = {}
    assert fluxui.DslApp._match_pattern("/user/:id", "/user/7?tab=info", params)
    assert params["id"] == "7"
    print("[10] route param matching (/user/:id)  PASS")


def main():
    test_state_basic()
    test_element_tags()
    test_element_tree()
    test_reactive_pump()
    test_onclick_drives_reactive()
    test_store_zustand()
    test_schema_zod()
    test_query_states()
    test_skeleton()
    test_route_param_match()
    print("All Python DSL tests passed!")


if __name__ == "__main__":
    main()
