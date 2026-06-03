"""Headless test for the FluxUI Python DSL reactive layer.

Exercises State, Node tree construction and the reactive pump without opening a
window. The pump is validated against a fake widget so it does not require a GPU
context, mirroring examples/cpp/test_dsl.cpp.
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


def test_node_tree():
    root = fluxui.Row([
        fluxui.Sidebar([fluxui.NavItem("A"), fluxui.NavItem("B")]).cls("sidebar"),
        fluxui.Column([fluxui.Text("T").cls("h1"), fluxui.Button("Go")]).cls("content"),
    ]).cls("app")
    assert root._kind == "row"
    assert root._class_name == "app"
    assert len(root._children) == 2
    assert root._children[0]._kind == "sidebar"
    assert root._children[0]._class_name == "sidebar"
    assert len(root._children[0]._children) == 2
    assert root._children[1]._children[0]._content == "T"
    print("[2] declarative Node tree  PASS")


def test_reactive_pump():
    reset_bindings()
    devices = fluxui.State(128)
    w = FakeWidget()
    initial = str(devices.get())
    w.set_content(initial)
    fluxui._register_reactive(w, lambda: str(devices.get()), initial)

    assert w.content == "128"
    devices.set(129)
    # value not pushed until the pump runs
    assert w.content == "128"
    changed = fluxui.pump_reactive_bindings()
    assert changed is True
    assert w.content == "129"
    # no change => pump reports nothing changed
    assert fluxui.pump_reactive_bindings() is False
    # multiple updates accumulate to latest
    devices.set(200)
    devices.set(201)
    fluxui.pump_reactive_bindings()
    assert w.content == "201"
    print("[3] reactive pump updates on State change  PASS")


def test_onclick_drives_reactive():
    reset_bindings()
    count = fluxui.State(0)
    w = FakeWidget()
    w.set_content(str(count.get()))
    fluxui._register_reactive(w, lambda: str(count.get()), "0")

    btn = fluxui.Button("inc").on_click(lambda: count.set(count.get() + 1))
    # simulate three clicks
    for _ in range(3):
        btn._on_click()
    fluxui.pump_reactive_bindings()
    assert w.content == "3"
    print("[4] onClick -> State -> reactive Text  PASS")


def main():
    test_state_basic()
    test_node_tree()
    test_reactive_pump()
    test_onclick_drives_reactive()
    print("All Python DSL tests passed!")


if __name__ == "__main__":
    main()
