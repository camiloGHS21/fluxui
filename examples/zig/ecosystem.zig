// FluxUI Ecosystem Demo (Zig) — the modern Next.js/Zod/Zustand-style toolkit.
//
// Shows the five ecosystem features in one app:
//   1. Routing + params  — addRoute("/user/:id"), d.param("id")
//   2. Store(T)          — Zustand-style global state shared across views
//   3. Schema + Rule     — Zod-style runtime validation
//   4. Query(T)          — fetch with idle/loading/success/err + view()
//   5. skeleton          — loading placeholders for a pending Query
//
// Zig has no closures-with-captures over the C ABI, so views are plain fns and
// state lives in module-level globals (the idiomatic Zig pattern here).
const fluxui = @import("fluxui.zig");
const d = fluxui.dsl;
const std = @import("std");

// ---- Global store (Zustand-style): a tiny cart shared by every view. --------
const CartState = struct { count: i32 = 0, last: [*:0]const u8 = "(none)" };
var cart = d.Store(CartState).init(.{});

// Buffer for the reactive "items in cart" line.
var g_count_buf: [64]u8 = undefined;

fn cartCountText(_: ?*anyopaque) callconv(.c) [*:0]const u8 {
    const s = std.fmt.bufPrintZ(&g_count_buf, "Items in cart: {d}", .{cart.get().count}) catch return "Items in cart: ?";
    return s.ptr;
}

fn addApple(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    var st = cart.get();
    st.count += 1;
    st.last = "apple";
    cart.set(st);
}
fn addBook(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    var st = cart.get();
    st.count += 1;
    st.last = "book";
    cart.set(st);
}
fn resetCart(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    cart.set(.{});
}

// ---- Schema (Zod-style) + a sample validation into a global result. ---------
var g_form_result: [128]u8 = undefined;
var g_form_msg: [*:0]const u8 = "Press a button to validate a sample payload.";

fn signupSchema() d.Schema {
    return d.Schema.init(&.{
        .{ .name = "email", .rule = d.Rule.string().email() },
        .{ .name = "age", .rule = d.Rule.number().min(18).max(120) },
        .{ .name = "nick", .rule = d.Rule.string().minLength(3).maxLength(12) },
    });
}

fn setResult(res: d.ValidationResult) void {
    if (res.ok) {
        g_form_msg = "Valid! Welcome aboard.";
    } else {
        const s = std.fmt.bufPrintZ(&g_form_result, "Invalid: {s}", .{res.first()}) catch {
            g_form_msg = "Invalid.";
            return;
        };
        g_form_msg = s.ptr;
    }
}

fn validateGood(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    setResult(signupSchema().validate(&.{
        .{ .name = "email", .value = "a@b.com" },
        .{ .name = "age", .value = "25" },
        .{ .name = "nick", .value = "joe" },
    }));
}
fn validateBad(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    setResult(signupSchema().validate(&.{
        .{ .name = "email", .value = "nope" },
        .{ .name = "age", .value = "12" },
        .{ .name = "nick", .value = "x" },
    }));
}
fn formResultText(_: ?*anyopaque) callconv(.c) [*:0]const u8 {
    return g_form_msg;
}

// ---- Query(T): a fetcher that resolves to a fixed payload. -------------------
const UsersQuery = d.Query([*:0]const u8);
var users = UsersQuery.init(fetchUsers);

fn fetchUsers() UsersQuery.QueryError![*:0]const u8 {
    return "Ada Lovelace, Alan Turing, Grace Hopper";
}

var g_users_buf: [128]u8 = undefined;
fn usersLoading() d.Node {
    return d.skeleton(3);
}
fn usersSuccess(data: [*:0]const u8) d.Node {
    const s = std.fmt.bufPrintZ(&g_users_buf, "Users: {s}", .{data}) catch return d.text("Users").class("ok");
    return d.text(s.ptr).class("ok");
}
fn usersError(err: [*:0]const u8) d.Node {
    _ = err;
    return d.text("Error fetching users").class("danger");
}

// ---- Navigation handlers (C-ABI). -------------------------------------------
fn goStore(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/store");
}
fn goForm(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/form");
}
fn goQuery(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/query");
}
fn goUser(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/user/7?tab=info");
}
fn goUser42(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/user/42?tab=settings");
}

// ---- Views. -----------------------------------------------------------------
fn storeView() d.Node {
    return d.div(&.{
        d.h1("Store (Zustand-style)"),
        d.div(&.{
            d.p("A single global store, mutated and read across views."),
            d.textFn(cartCountText, null),
            d.div(&.{
                d.button("Add apple").class("primary").onClick(addApple, null),
                d.button("Add book").class("primary").onClick(addBook, null),
                d.button("Reset").onClick(resetCart, null),
            }).class("row"),
        }).class("card"),
    });
}

fn formView() d.Node {
    return d.div(&.{
        d.h1("Schema validation (Zod-style)"),
        d.div(&.{
            d.textFn(formResultText, null).class("ok"),
            d.div(&.{
                d.button("Validate good payload").class("primary").onClick(validateGood, null),
                d.button("Validate bad payload").onClick(validateBad, null),
            }).class("row"),
        }).class("card"),
    });
}

fn queryView() d.Node {
    if (users.status() == .idle) users.start();
    return d.div(&.{
        d.h1("Query (React-Query-style)"),
        d.div(&.{
            d.p("Runs a fetcher and renders per state. Skeleton on loading."),
            users.view(usersLoading, usersSuccess, usersError),
        }).class("card"),
    });
}

var g_user_title: [64]u8 = undefined;
var g_user_id: [80]u8 = undefined;
fn userView() d.Node {
    const id = d.param("id");
    const title = std.fmt.bufPrintZ(&g_user_title, "User #{s}", .{id}) catch "User";
    const idline = std.fmt.bufPrintZ(&g_user_id, "id param = {s}", .{id}) catch "id param";
    return d.div(&.{
        d.h1(title.ptr),
        d.div(&.{
            d.p("Route pattern: /user/:id — captured from the URL path."),
            d.text(idline.ptr),
            d.div(&.{
                d.button("Go to user 42").onClick(goUser42, null),
            }).class("row"),
        }).class("card"),
    });
}

// ---- App shell. -------------------------------------------------------------
fn layout(content: d.Node) d.Node {
    return d.div(&.{
        d.nav(&.{
            d.el("h2", &.{d.text("FluxUI")}),
            d.button("Store").onClick(goStore, null),
            d.button("Form (Zod)").onClick(goForm, null),
            d.button("Query").onClick(goQuery, null),
            d.button("User #7").onClick(goUser, null),
        }).class("sidebar"),
        d.div(&.{content}).id("__content__").class("content"),
    }).class("root");
}

pub fn main() !void {
    const app = try fluxui.App.create();
    defer app.deinit();

    try app.init("FluxUI Ecosystem (Zig)", 1100, 760);
    _ = app.loadDefaultFont(16.0);

    app.addStylesheet(
        ".root { display:flex; flex-direction:row; background:#0d1117; color:#e6edf3; }" ++
        ".sidebar { display:flex; flex-direction:column; width:220px; background:#161b22; padding:18px; gap:6px; }" ++
        ".sidebar button { text-align:left; padding:10px 12px; border-radius:8px; color:#c9d1d9; }" ++
        ".content { display:flex; flex-direction:column; flex-grow:1; padding:28px; gap:18px; }" ++
        "h1 { font-size:26px; font-weight:700; }" ++
        ".card { background:#161b22; border:1px solid #30363d; border-radius:12px; padding:20px; gap:10px; display:flex; flex-direction:column; }" ++
        ".row { display:flex; flex-direction:row; gap:10px; align-items:center; }" ++
        ".primary { background:#238636; color:white; padding:10px 18px; border-radius:8px; }" ++
        ".ok { color:#3fb950; } .danger { color:#f85149; }" ++
        ".skeleton { display:flex; flex-direction:column; gap:10px; }" ++
        ".skeleton-line { height:14px; border-radius:6px; background:#21262d; }",
    );

    app.addRoute("/store", storeView);
    app.addRoute("/form", formView);
    app.addRoute("/query", queryView);
    app.addRoute("/user/:id", userView);
    app.setLayout(layout);

    try app.build("/store");
    app.runReactive();
}
