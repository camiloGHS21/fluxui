package io.fluxui;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Deferred declarative element — mirrors the C++ {@code WidgetBuilder} node.
 *
 * <p>Build a tree with the {@link Dsl} factory methods, attach classes/handlers
 * via chaining, then mount it onto a parent {@link Widget}. Reactive {@code textFn}
 * nodes register themselves with {@link Dsl} so they refresh when state changes.
 */
public final class Node {
    enum Kind {
        ROW, COLUMN, SIDEBAR, CARD, GRID, DIV,
        TEXT, TEXT_FN, BUTTON, NAV_ITEM, INPUT, CHECKBOX, ELEMENT
    }

    private final Kind kind;
    private String tag = "";
    private String content = "";
    private String className = "";
    private String nodeId = "";
    private Runnable onClick;
    private Supplier<String> textFn;
    private boolean checked;
    private final List<Node> children = new ArrayList<>();

    Node(Kind kind) {
        this.kind = kind;
    }

    Node tag(String tag) {
        this.tag = tag;
        return this;
    }

    Node content(String content) {
        this.content = content;
        return this;
    }

    Node textFn(Supplier<String> fn) {
        this.textFn = fn;
        return this;
    }

    Node checked(boolean checked) {
        this.checked = checked;
        return this;
    }

    Node children(Node... nodes) {
        for (Node n : nodes) {
            if (n != null) children.add(n);
        }
        return this;
    }

    // ---- Chaining setters (match C++ WidgetBuilder). ----
    public Node className(String cls) {
        this.className = cls;
        return this;
    }

    public Node id(String id) {
        this.nodeId = id;
        return this;
    }

    public Node onClick(Runnable handler) {
        this.onClick = handler;
        return this;
    }

    Widget mount(Widget parent) {
        Widget w;
        switch (kind) {
            case ROW:
                w = parent.addPanel(className);
                w.css("display:flex;flex-direction:row");
                break;
            case COLUMN:
                w = parent.addPanel(className);
                w.css("display:flex;flex-direction:column");
                break;
            case SIDEBAR:
                w = parent.addElement("nav", "", className.isEmpty() ? "sidebar" : className);
                w.css("display:flex;flex-direction:column");
                break;
            case CARD:
                w = parent.addPanel(className.isEmpty() ? "card" : className);
                w.css("display:flex;flex-direction:column");
                break;
            case GRID:
                w = parent.addPanel(className);
                w.css("display:grid");
                break;
            case DIV:
                w = parent.addPanel(className);
                break;
            case TEXT:
                w = parent.addText(content, className);
                break;
            case TEXT_FN: {
                String initial = textFn != null ? textFn.get() : "";
                w = parent.addText(initial, className);
                if (textFn != null) {
                    Dsl.registerReactive(w, textFn, initial);
                }
                break;
            }
            case BUTTON:
                w = parent.addButton(content, className);
                break;
            case NAV_ITEM:
                w = parent.addButton(content, className.isEmpty() ? "nav-item" : className);
                break;
            case INPUT:
                w = parent.addTextInput(content, className);
                break;
            case CHECKBOX:
                w = parent.addCheckbox(checked, className);
                break;
            case ELEMENT:
                w = parent.addElement(tag, content, className);
                break;
            default:
                w = parent.addPanel(className);
                break;
        }

        if (!nodeId.isEmpty()) {
            w.setId(nodeId);
        }
        if (onClick != null) {
            w.setOnClick(onClick);
        }
        for (Node child : children) {
            child.mount(w);
        }
        return w;
    }
}
