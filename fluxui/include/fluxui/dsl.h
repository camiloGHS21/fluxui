// FluxUI Declarative DSL — modern, HTML/Blink-faithful functional UI builder.
//
// Element names match HTML exactly (Div, Span, P, H1..H6, Nav, Section, Button,
// Input, A, Img, Ul, Li, ...), so a FluxUI tree reads like HTML and renders with
// Blink's UA stylesheet semantics. Layout is expressed in CSS (display:flex,
// grid, ...) exactly like the browser — there are no bespoke Row/Column nodes.
//
// The same pattern maps 1:1 to Rust/Go/Java/Python/Zig via the bindings.
//
// Usage:
//   #include <fluxui/dsl.h>
//   using namespace fluxui;
//
//   int main() {
//       App app(1200, 800, "CompanyGuard");
//       app.addCSS(".app{display:flex} .content{display:flex;flex-direction:column}");
//       auto devices = State<int>(128);
//       app.setRoot(
//           Div({
//               Nav({
//                   Button("Dashboard"),
//                   Button("Settings")
//               }).className("sidebar"),
//               Div({
//                   H1("CompanyGuard"),
//                   Text([&]{ return std::to_string(devices.get()); }),   // reactive!
//                   Button("Scan").onClick([&]{ devices.set(devices.get() + 1); })
//               }).className("content")
//           }).className("app")
//       );
//       return app.run();
//   }
//
// ----------------------------------------------------------------------------
//  This is the single public entry point for FluxUI applications. The DSL is
//  split into focused sub-headers under detail/ for navigability; the public
//  API is unchanged — just include <fluxui/dsl.h>.
//
//    detail/dsl/dsl_core.h       Reactive State/Ref, Element, HTML tag helpers
//    detail/dsl/dsl_app.h        App — owns Application, drives the reactive loop
//    detail/dsl/dsl_ecosystem.h  Store / Schema / Query / Skeleton helpers
// ----------------------------------------------------------------------------
#pragma once
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX   // prevent windows.h min/max macros clobbering Rule::min/max etc.
#endif
#include "fluxui/detail/dsl/dsl_core.h"
#include "fluxui/detail/dsl/dsl_app.h"
#include "fluxui/detail/dsl/dsl_ecosystem.h"
