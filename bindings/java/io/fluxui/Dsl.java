package io.fluxui;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Declarative DSL entry point — mirrors C++ dsl.h / Go dsl.go / Rust dsl module.
 *
 * <p>Build a tree with the static factory methods (row, column, text, ...), attach
 * classes/handlers via {@link Node} chaining, mount it with {@link #setRoot}, and
 * run the reactive loop with {@link #runReactive}. Reactive {@code textFn} nodes
 * are re-evaluated each frame and pushed into their widgets when the value changes.
 *
 * <pre>{@code
 * App app = App.create();
 * app.init("CompanyGuard", 1200, 800);
 * app.loadDefaultFont(16f);
 * State<Integer> devices = new State<>(128);
 * Dsl.setRoot(app,
 *     Dsl.row(
 *         Dsl.sidebar(Dsl.navItem("Dashboard"), Dsl.navItem("Settings")).className("sidebar"),
 *         Dsl.column(
 *             Dsl.text("CompanyGuard").className("h1"),
 *             Dsl.textFn(() -> String.valueOf(devices.get())),
 *             Dsl.button("Escanear").className("primary")
 *                 .onClick(() -> devices.set(devices.get() + 1))
 *         ).className("content")
 *     ).className("app"));
 * Dsl.runReactive(app);
 * }</pre>
 */
public final class Dsl {
    private Dsl() {}

    // ---- Reactive binding registry — re-evaluates textFn nodes each frame. ----
    private static final class Binding {
        final Widget widget;
        final Supplier<String> fn;
        String last;

        Binding(Widget widget, Supplier<String> fn, String last) {
            this.widget = widget;
            this.fn = fn;
            this.last = last;
        }
    }

    private static final List<Binding> bindings = new ArrayList<>();

    static void registerReactive(Widget widget, Supplier<String> fn, String initial) {
        bindings.add(new Binding(widget, fn, initial));
    }

    /**
     * Re-evaluate every reactive textFn binding, pushing changed values into the
     * underlying widgets. Returns true if anything changed. Called each frame.
     */
    public static boolean pumpReactiveBindings() {
        boolean changed = false;
        for (Binding b : bindings) {
            String v = b.fn.get();
            if (!v.equals(b.last)) {
                b.last = v;
                b.widget.setText(v);
                changed = true;
            }
        }
        return changed;
    }

    // ---- Builder functions (match C++ / Go / Rust / Python). ----
    public static Node row(Node... children) {
        return new Node(Node.Kind.ROW).children(children);
    }

    public static Node column(Node... children) {
        return new Node(Node.Kind.COLUMN).children(children);
    }

    public static Node sidebar(Node... children) {
        return new Node(Node.Kind.SIDEBAR).children(children);
    }

    public static Node card(Node... children) {
        return new Node(Node.Kind.CARD).children(children);
    }

    public static Node grid(Node... children) {
        return new Node(Node.Kind.GRID).children(children);
    }

    public static Node div(Node... children) {
        return new Node(Node.Kind.DIV).children(children);
    }

    public static Node text(String content) {
        return new Node(Node.Kind.TEXT).content(content);
    }

    /** Reactive text — re-evaluated whenever bound State changes. */
    public static Node textFn(Supplier<String> fn) {
        return new Node(Node.Kind.TEXT_FN).textFn(fn);
    }

    public static Node button(String label) {
        return new Node(Node.Kind.BUTTON).content(label);
    }

    public static Node navItem(String label) {
        return new Node(Node.Kind.NAV_ITEM).content(label);
    }

    public static Node input(String placeholder) {
        return new Node(Node.Kind.INPUT).content(placeholder);
    }

    public static Node checkbox(boolean checked) {
        return new Node(Node.Kind.CHECKBOX).checked(checked);
    }

    public static Node element(String tag, Node... children) {
        return new Node(Node.Kind.ELEMENT).tag(tag).children(children);
    }

    // ---- App helpers for the declarative flow. ----

    /** Mount a declarative Node tree as the application root. */
    public static void setRoot(App app, Node root) {
        Widget r = app.root();
        if (r == null || root == null) {
            return;
        }
        r.clearChildren();
        root.mount(r);
    }

    /** Install the reactive pump into the update loop and run the app. */
    public static void runReactive(App app) {
        app.setUpdateCallback(dt -> pumpReactiveBindings());
        app.run();
    }
}
