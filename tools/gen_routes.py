#!/usr/bin/env python3
"""FluxUI universal file-based route generator (like Next.js pages/).

Scans a views/ directory and emits a routes-registration file for the target
language. Each view file maps to a route by filename:

    views/dashboard.{ext}   -> route "/dashboard"   -> DashboardView()
    views/user_profile.{ext}-> route "/user-profile"-> UserProfileView()

Run it from a build step (CMake/Makefile/cargo build script/etc):

    python tools/gen_routes.py --lang go   --views examples/go/views   --out examples/go/routes_gen.go    --package main
    python tools/gen_routes.py --lang rust --views examples/rust/views --out examples/rust/routes_gen.rs
    python tools/gen_routes.py --lang zig  --views examples/zig/views  --out examples/zig/routes_gen.zig
    python tools/gen_routes.py --lang java --views examples/java/views --out examples/java/RoutesGen.java --package io.app
    python tools/gen_routes.py --lang cpp  --views examples/app/views   --out build/routes_gen.h          --namespace app
"""
import argparse
import glob
import os


def view_entries(views_dir, ext):
    """Return list of (route_path, PascalName, filename) for each view file."""
    entries = []
    default_route = None
    for path in sorted(glob.glob(os.path.join(views_dir, "*." + ext))):
        name = os.path.splitext(os.path.basename(path))[0]
        if name.startswith("_"):
            continue
        pascal = "".join(p.capitalize() for p in name.split("_"))
        route = "/" + name.replace("_", "-")
        entries.append((route, pascal, name, os.path.abspath(path)))
        if name in ("index", "dashboard") and default_route is None:
            default_route = route
    if default_route is None and entries:
        default_route = entries[0][0]
    return entries, (default_route or "/")


def gen_cpp(entries, default, out, namespace):
    lines = [
        "// Auto-generated file-based routes (FluxUI / Next.js style). DO NOT EDIT.",
        "#pragma once",
        "",
    ]
    for _, _, _, abspath in entries:
        lines.append('#include "%s"' % abspath.replace("\\", "/"))
    lines += [
        "",
        "namespace %s { namespace generated {" % namespace,
        "",
        "inline void registerRoutes(fluxui::App& app) {",
    ]
    for route, pascal, _, _ in entries:
        lines.append('    app.addRoute("%s", %s::%sView);' % (route, namespace, pascal))
    lines += [
        "}",
        "",
        'inline const char* defaultRoute() { return "%s"; }' % default,
        "",
        "} }",
        "",
    ]
    return "\n".join(lines)


def gen_go(entries, default, out, package):
    lines = [
        "// Auto-generated file-based routes (FluxUI / Next.js style). DO NOT EDIT.",
        "package %s" % (package or "main"),
        "",
        'import fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"',
        "",
        "// RegisterRoutes wires every view in views/ into the app router.",
        "func RegisterRoutes(app *fluxui.App) {",
    ]
    for route, pascal, _, _ in entries:
        lines.append('\tapp.AddRoute("%s", %sView)' % (route, pascal))
    lines += [
        "}",
        "",
        'func DefaultRoute() string { return "%s" }' % default,
        "",
    ]
    return "\n".join(lines)


def gen_rust(entries, default, out, _):
    lines = [
        "// Auto-generated file-based routes (FluxUI / Next.js style). DO NOT EDIT.",
        "pub fn register_routes(app: &fluxui::App) {",
    ]
    for route, pascal, name, _ in entries:
        snake = name
        lines.append('    app.add_route("%s", crate::views::%s::view);' % (route, snake))
    lines += [
        "}",
        "",
        'pub fn default_route() -> &\'static str { "%s" }' % default,
        "",
    ]
    return "\n".join(lines)


def gen_zig(entries, default, out, _):
    lines = [
        "// Auto-generated file-based routes (FluxUI / Next.js style). DO NOT EDIT.",
        'const fluxui = @import("fluxui.zig");',
    ]
    for _, _, name, _ in entries:
        lines.append('const %s = @import("views/%s.zig");' % (name, name))
    lines += [
        "",
        "pub fn registerRoutes(app: fluxui.App) void {",
    ]
    for route, _, name, _ in entries:
        lines.append('    app.addRoute("%s", %s.view);' % (route, name))
    lines += [
        "}",
        "",
        'pub fn defaultRoute() [*:0]const u8 { return "%s"; }' % default,
        "",
    ]
    return "\n".join(lines)


def gen_java(entries, default, out, package):
    cls = os.path.splitext(os.path.basename(out))[0]
    lines = [
        "// Auto-generated file-based routes (FluxUI / Next.js style). DO NOT EDIT.",
    ]
    if package:
        lines.append("package %s;" % package)
    lines += [
        "",
        "import io.fluxui.Dsl;",
        "",
        "public final class %s {" % cls,
        "    public static void registerRoutes(Dsl.Router router) {",
    ]
    for route, pascal, _, _ in entries:
        lines.append('        router.addRoute("%s", () -> Views.%sView());' % (route, pascal))
    lines += [
        "    }",
        "",
        '    public static String defaultRoute() { return "%s"; }' % default,
        "}",
        "",
    ]
    return "\n".join(lines)


GENERATORS = {
    "cpp": ("h", gen_cpp),
    "go": ("go", gen_go),
    "rust": ("rs", gen_rust),
    "zig": ("zig", gen_zig),
    "java": ("java", gen_java),
}


def main():
    ap = argparse.ArgumentParser(description="FluxUI file-based route generator")
    ap.add_argument("--lang", required=True, choices=GENERATORS.keys())
    ap.add_argument("--views", required=True, help="views/ directory to scan")
    ap.add_argument("--out", required=True, help="output file to generate")
    ap.add_argument("--namespace", default="app", help="C++ namespace")
    ap.add_argument("--package", default="", help="Go/Java package name")
    args = ap.parse_args()

    ext, gen = GENERATORS[args.lang]
    entries, default = view_entries(args.views, ext)

    if args.lang == "cpp":
        out = gen_cpp(entries, default, args.out, args.namespace)
    elif args.lang == "go":
        out = gen_go(entries, default, args.out, args.package)
    elif args.lang == "rust":
        out = gen_rust(entries, default, args.out, None)
    elif args.lang == "zig":
        out = gen_zig(entries, default, args.out, None)
    elif args.lang == "java":
        out = gen_java(entries, default, args.out, args.package)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(out)
    print("Generated %d routes -> %s (default %s)" % (len(entries), args.out, default))


if __name__ == "__main__":
    main()
