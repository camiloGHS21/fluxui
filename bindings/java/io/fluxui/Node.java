package io.fluxui;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Deferred, HTML-named declarative element — modern Blink-faithful node.
 *
 * <p>Every node carries an HTML tag name (div, span, h1, button, nav, ...) and is
 * materialized through {@link Widget#addElement}, the single source of truth for
 * the tag &rarr; widget mapping (Blink UA parity). Reactive {@code textFn} nodes
 * register themselves with {@link Dsl} so they refresh when state changes.
 */
public final class Node {
    private final String tag;
    private String content = "";
    private String className = "";
    private String nodeId = "";
    private Runnable onClick;
    private java.util.function.Consumer<Widget> onMount;
    private Supplier<String> textFn;
    private final List<String[]> inlineStyles = new ArrayList<>();
    private final List<String[]> attrs = new ArrayList<>();
    private final List<Node> children = new ArrayList<>();

    Node(String tag) {
        this.tag = tag;
    }

    Node content(String content) {
        this.content = content;
        return this;
    }

    Node textFn(Supplier<String> fn) {
        this.textFn = fn;
        return this;
    }

    Node children(Node... nodes) {
        for (Node n : nodes) {
            if (n != null) children.add(n);
        }
        return this;
    }

    // ---- Chaining setters — mirror HTML attributes / DOM properties. ----
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

    /** Receive the materialized native Widget for advanced setup. */
    public Node onMount(java.util.function.Consumer<Widget> handler) {
        this.onMount = handler;
        return this;
    }

    public Node style(String prop, String val) {
        inlineStyles.add(new String[] {prop, val});
        return this;
    }

    public Node attr(String name, String val) {
        attrs.add(new String[] {name, val});
        return this;
    }

    public Node href(String url) {
        return attr("href", url);
    }

    public Node src(String url) {
        this.content = url;
        return this;
    }

    Widget mount(Widget parent) {
        Widget w;
        if (textFn != null) {
            String initial = textFn.get();
            w = parent.addElement("span", initial, className);
            if (w != null) {
                Dsl.registerReactive(w, textFn, initial);
            }
        } else {
            w = parent.addElement(tag, content, className);
        }

        if (w == null) {
            return null;
        }
        if (!nodeId.isEmpty()) {
            w.setId(nodeId);
        }
        if (onClick != null) {
            w.setOnClick(onClick);
        }
        for (String[] s : inlineStyles) {
            w.css(s[0] + ":" + s[1]);
        }
        if (onMount != null) {
            onMount.accept(w);
        }
        for (Node child : children) {
            child.mount(w);
        }
        return w;
    }
}
